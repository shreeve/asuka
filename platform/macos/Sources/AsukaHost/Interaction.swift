// Interaction states (DESIGN.md, "Interaction states, drag and drop").
//
// Presses are the views' own mouse tracking (AsukaView's mouseDown,
// mouseDragged, mouseUp), not a click recognizer: Pressed and Press then
// come from the same events and agree. The recognizers the views have
// (long press, pan, ...) don't delay the mouse's events (their
// delaysPrimaryMouseButtonEvents is off), so the view sees the press as it
// happens; one that begins cancels it. Only the innermost pressable view
// under the mouse is pressed: a control (a button, a field) takes its
// clicks itself and never passes them on.
//
// Hover is an NSTrackingArea per view, checked again as views appear, move
// or show under a still pointer. Its areas work in the active app alone, so
// leaving the app clears every hover and coming back checks them again
// (appActive). Gestures are Gestures.swift's, drops and
// drags DragDrop.swift's.
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

/// A node's interaction states: its hover area, its drop types and drag
/// items, its pan recognizer.
@MainActor final class AsukaInteraction {
    var hoverArea: NSTrackingArea?
    var hovered = false
    /// DropAccept's MIME patterns.
    var dropAccept: [String] = []
    /// DragItems: it's draggable.
    var dragItems: [AsukaItem] = []
    /// Its NSPanGestureRecognizer (Pan, Swipe).
    var panGesture: NSPanGestureRecognizer?
    /// Its other recognizers, by event (one of each).
    var gestures: Set<UInt16> = []
}

/// The primary mouse button's press, from its down to its up.
struct AsukaMouseState {
    var active = false
    /// The innermost pressable view, nil once cancelled.
    var target: AsukaNode?
    /// The mouse is on target (it's pressed).
    var inside = false
    /// The innermost draggable view.
    var source: AsukaNode?
    /// Where it went down, in window coordinates.
    var down = CGPoint.zero
}

/// How far the mouse moves with the button down, in points, before a
/// draggable view's drag begins (AppKit's own views begin theirs at about
/// this distance).
let asukaDragDistance: CGFloat = 3

@MainActor extension AsukaRuntime {
    /// n's interaction states, made as they're needed.
    func ix(_ n: AsukaNode) -> AsukaInteraction {
        if let x = n.interaction { return x }
        let x = AsukaInteraction()
        n.interaction = x
        return x
    }

    /// Whether a click on n is n's: it presses, long presses, reports its
    /// pressed state or opens a menu.
    func pressable(_ n: AsukaNode) -> Bool {
        n.listening(.press) || n.listening(.longPress) || n.listening(.pressed)
            || n.menu != nil && n.menuTrigger == AsukaMenuTrigger.tap.rawValue
    }

    /// Whether n is still one of the core's views.
    func live(_ n: AsukaNode?) -> Bool {
        guard let n else { return false }
        return nodes[n.id] === n && n.view != nil
    }

    // MARK: Listening

    /// Adds what an event needs on a view: gesture recognizers, tracking
    /// areas, the press role.
    func listenGesture(_ n: AsukaNode, _ e: AsukaEventKind) {
        guard n.kind == .view, let v = n.view else { return }
        let x = ix(n)
        func add(_ g: NSGestureRecognizer) {
            // The view sees the mouse as it goes, for its press (Pressed); a
            // gesture that begins cancels the press.
            g.delaysPrimaryMouseButtonEvents = false
            v.addGestureRecognizer(g)
        }
        switch e {
        case .press:
            // Pressed and released on the view (mouseDown, mouseUp); a button
            // to VoiceOver, unless its role says otherwise (A11y.swift).
            v.setAccessibilityElement(true)
            if (n.a11y?.role ?? .none) == .none { v.setAccessibilityRole(.button) }
        case .longPress:
            guard x.gestures.insert(e.rawValue).inserted else { return }
            let g = NSPressGestureRecognizer(target: visual, action: #selector(AsukaVisual.longPressed(_:)))
            g.minimumPressDuration = 0.5
            add(g)
            // A secondary click is a Mac's long press.
            let r = NSClickGestureRecognizer(target: visual, action: #selector(AsukaVisual.longPressed(_:)))
            r.buttonMask = 2
            add(r)
        case .pan, .swipe:
            // A swipe is a quick pan (a drag with the mouse) or a trackpad's
            // flick (Gestures.swift).
            guard x.panGesture == nil else { return }
            let g = NSPanGestureRecognizer(target: visual, action: #selector(AsukaVisual.panned(_:)))
            g.delegate = visual
            add(g)
            x.panGesture = g
        case .hover:
            listenHover(n)
        case .pinch:
            guard x.gestures.insert(e.rawValue).inserted else { return }
            add(NSMagnificationGestureRecognizer(target: visual, action: #selector(AsukaVisual.magnified(_:))))
        case .rotate:
            guard x.gestures.insert(e.rawValue).inserted else { return }
            add(NSRotationGestureRecognizer(target: visual, action: #selector(AsukaVisual.rotated(_:))))
        default:
            break
        }
    }

    // MARK: Presses

    /// Starts tracking a press on v (the view the click hit, or one it
    /// passed the click to); false: no view there takes it.
    func mouseDown(_ v: NSView, _ e: NSEvent) -> Bool {
        cancelPress()
        visual.mouse = AsukaMouseState()
        if e.modifierFlags.contains(.control) { return false } // a secondary click: the context menu
        pressFocus(v) // the Focusable view it's on takes the focus (Focus.swift)
        if visual.listSelectClick?(v) == true { return true } // a row of a selecting list: selected or deselected
        var target: AsukaNode?, source: AsukaNode?
        var at: NSView? = v
        while let x = at, target == nil || source == nil {
            at = x.superview
            guard let n = node(of: x) else { continue }
            if target == nil && pressable(n) { target = n }
            if source == nil, let d = n.interaction?.dragItems, !d.isEmpty { source = n }
        }
        guard target != nil || source != nil else { return false }
        visual.mouse = AsukaMouseState(active: true, target: target, inside: false, source: source, down: e.locationInWindow)
        if let t = target {
            visual.mouse.inside = true
            if t.listening(.pressPoint), let tv = t.view {
                // Where it began, from its top left.
                let p = tv.convert(e.locationInWindow, from: nil)
                emit(t, .pressPoint) { AsukaEvents.pressPoint($0, x: Double(p.x), y: Double(p.y)) }
            }
            if live(t) { emit(t, .pressed) { AsukaEvents.pressed($0, true) } }
        }
        return true
    }

    func mouseDragged(_ e: NSEvent) -> Bool {
        guard visual.mouse.active else { return false }
        let loc = e.locationInWindow
        let m = visual.mouse
        if let s = m.source, hypot(loc.x - m.down.x, loc.y - m.down.y) >= asukaDragDistance {
            cancelPress()
            visual.mouse = AsukaMouseState()
            if live(s) { beginDrag(s, e) }
            return true
        }
        if let t = m.target {
            guard live(t) else {
                visual.mouse.target = nil
                return true
            }
            let inside = mouseIn(t, loc)
            if inside != m.inside {
                visual.mouse.inside = inside
                emit(t, .pressed) { AsukaEvents.pressed($0, inside) }
            }
        }
        return true
    }

    func mouseUp(_ e: NSEvent) -> Bool {
        let m = visual.mouse
        guard m.active else { return false }
        visual.mouse = AsukaMouseState()
        guard let t = m.target, m.inside, live(t) else { return true }
        if mouseIn(t, e.locationInWindow) {
            if t.menu != nil && t.menuTrigger == AsukaMenuTrigger.tap.rawValue && !t.listening(.press) {
                emit(t, .pressed) { AsukaEvents.pressed($0, false) }
                popUp(t)
                return true
            }
            emit(t, .press) { AsukaEvents.press($0) }
        }
        if live(t) { emit(t, .pressed) { AsukaEvents.pressed($0, false) } }
        return true
    }

    /// Whether a point in the window is on n's view.
    func mouseIn(_ n: AsukaNode, _ loc: CGPoint) -> Bool {
        guard let v = n.view else { return false }
        let p = v.convert(loc, from: nil)
        let b = v.bounds
        return p.x >= b.minX && p.y >= b.minY && p.x < b.maxX && p.y < b.maxY
    }

    /// Ends the press under way without a Press: a gesture, a drag or the
    /// app going away took the mouse. A drag may still begin.
    func cancelPress() {
        guard let t = visual.mouse.target else { return }
        visual.mouse.target = nil
        if visual.mouse.inside && live(t) { emit(t, .pressed) { AsukaEvents.pressed($0, false) } }
        visual.mouse.inside = false
    }

    /// n left its window or was hidden: no longer pressed or hovered.
    func viewLeft(_ n: AsukaNode) {
        if visual.mouse.target === n { cancelPress() }
        setHover(n, false)
    }

    // MARK: Hover

    /// Gives n's view a tracking area: entered and exited, over its visible
    /// rect, in the active app, during drags too.
    func listenHover(_ n: AsukaNode) {
        let x = ix(n)
        guard x.hoverArea == nil, let v = n.view else { return }
        let a = NSTrackingArea(rect: .zero, options: [.mouseEnteredAndExited, .activeInActiveApp, .inVisibleRect, .enabledDuringMouseDrag],
                               owner: v, userInfo: nil)
        v.addTrackingArea(a)
        x.hoverArea = a
        checkHover(n)
    }

    func mouseEntered(_ v: NSView) {
        if let n = node(of: v), n.interaction?.hoverArea != nil { setHover(n, true) }
    }

    func mouseExited(_ v: NSView) {
        if let n = node(of: v), n.interaction?.hoverArea != nil { setHover(n, false) }
    }

    /// Reports n hovered, or not, if that changed.
    func setHover(_ n: AsukaNode, _ on: Bool) {
        guard let x = n.interaction, x.hovered != on, n.listening(.hover) || x.hovered else { return }
        if on, let v = n.view, behindModal(v) { return } // a Modal overlay takes the pointer (Overlay.swift)
        x.hovered = on
        if live(n) { emit(n, .hover) { AsukaEvents.hover($0, on) } }
    }

    /// Checks, once the core has returned, whether the pointer is over n:
    /// a view that appears, moves or shows under a still pointer gets no
    /// entered event.
    func checkHover(_ n: AsukaNode) {
        guard let x = n.interaction, !x.hovered, x.hoverArea != nil else { return }
        visual.hoverCheck.append(n)
        queueHoverCheck()
    }

    func queueHoverCheck() {
        guard !visual.hoverQueued, visual.hoverHeld == 0, !visual.hoverCheck.isEmpty else { return }
        visual.hoverQueued = true
        afterCore { self.runHoverCheck() }
    }

    /// Holds the hover checks until the views are placed (hoverHeld), then
    /// runs them.
    func holdHover() { visual.hoverHeld += 1 }

    func releaseHover() {
        visual.hoverHeld -= 1
        queueHoverCheck()
    }

    func runHoverCheck() {
        visual.hoverQueued = false
        guard visual.hoverHeld == 0 else { return } // releaseHover runs them
        let q = visual.hoverCheck
        visual.hoverCheck = []
        for n in q {
            guard let x = n.interaction, !x.hovered, live(n), n.listening(.hover), let v = n.view, !v.isHiddenOrHasHiddenAncestor,
                  let w = v.window, let p = hoverProbe(w) else { continue }
            // A List's row's visibleRect is the list's whole shown area, not
            // clipped to the row: the bounds clip it.
            if v.visibleRect.intersection(v.bounds).contains(v.convert(p, from: nil)) { setHover(n, true) }
        }
    }

    /// Where the pointer is in w, in its coordinates, if it's over w in the
    /// active app (under the automation, its hoverprobe's: none without).
    func hoverProbe(_ w: NSWindow) -> CGPoint? {
        if let probe = visual.hoverProbe { return probe(w) }
        guard !AsukaAutomation.shared.active, NSApp.isActive, w.isVisible else { return nil }
        let p = NSEvent.mouseLocation
        guard NSWindow.windowNumber(at: p, belowWindowWithWindowNumber: 0) == w.windowNumber else { return nil }
        return w.convertPoint(fromScreen: p)
    }

    /// Follows the app's activation: leaving it, nothing is pressed or
    /// hovered; back, the views under the pointer are.
    func appActive(_ active: Bool) {
        if !active {
            cancelPress()
            visual.mouse = AsukaMouseState()
        }
        for n in nodes.values where n.interaction?.hoverArea != nil {
            if active { checkHover(n) } else { setHover(n, false) }
        }
    }

    /// Forgets n as it goes: the core reports it leaving its states.
    func dropInteraction(_ n: AsukaNode) {
        if visual.mouse.target === n {
            visual.mouse.target = nil
            visual.mouse.inside = false
        }
        if visual.mouse.source === n { visual.mouse.source = nil }
        if visual.over.target === n { visual.over.target = nil }
        visual.hoverCheck.removeAll { $0 === n }
        n.interaction?.hovered = false
        n.interaction?.hoverArea = nil
    }

    func interactionJSON(_ n: AsukaNode, _ d: inout [String: Any]) {
        guard let x = n.interaction, let v = n.view else { return }
        var j: [String: Any] = ["hovered": x.hovered, "dropAccept": x.dropAccept, "dragItems": x.dragItems.count,
                                "pan": x.panGesture != nil, "registeredTypes": v.registeredDraggedTypes.count,
                                "recognizers": v.gestureRecognizers.map { String(describing: type(of: $0)) }]
        if let a = x.hoverArea {
            j["hoverOptions"] = a.options.rawValue
            j["trackingAreas"] = v.trackingAreas.count
        }
        d["interaction"] = j
    }
}

extension AsukaVisual {
    /// Follows the app's activation (once, as the host starts).
    @MainActor func observeApp() {
        guard !appObserved else { return }
        appObserved = true
        let c = NotificationCenter.default
        c.addObserver(self, selector: #selector(appResigned(_:)), name: NSApplication.didResignActiveNotification, object: nil)
        c.addObserver(self, selector: #selector(appBecameActive(_:)), name: NSApplication.didBecomeActiveNotification, object: nil)
    }

    @objc func appResigned(_ note: Notification) { AsukaRuntime.shared.appActive(false) }
    @objc func appBecameActive(_ note: Notification) { AsukaRuntime.shared.appActive(true) }
}
