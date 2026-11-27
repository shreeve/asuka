// An Overlay on a Mac (RFC 0010): a layer over a layout root, its
// children placed by the core in that root's coordinates.
//
//   - Its view goes over everything in its root's window: a subview of the
//     view holding the root's view (the main window's content, a Window's,
//     a sheet's window's content), with the root's view's frame; in a
//     Popover, over the popover's own view, at its bounds. Overlays stay
//     the top subviews there, the last presented on top, whatever comes in
//     after them.
//   - It's sized by the host, as a sheet is: it reports its Environment
//     (its root's, at its size) once it's in its place and as that changes.
//   - It takes no presses of its own (hit testing passes through it) unless
//     Modal: then a press on none of its views is its own view's, and what
//     is behind it gets no press, hover or scroll.
//   - A press that begins in its root on none of its views is reported as
//     OutsidePress, before the press reaches its target: the pointer's
//     local event monitor (Focus.swift's).
//   - Its Anchor's frame in the root's coordinates, and whether any of it
//     shows (in the window, not hidden, not clipped by a clip view or a
//     clipping view around it), is reported as AnchorFrame once it's
//     known and each time it changes: after each batch, as a scroll view
//     around it scrolls (once per turn of the run loop) and as the window
//     resizes.
//   - Modal, screen readers see only it (A11y.swift).
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

/// An Overlay's state.
@MainActor final class AsukaOverlay {
    var root: UInt32 = 0
    var anchor: UInt32 = 0
    var modal = false
    /// The order it was last presented in: the last is on top.
    var seq = 0
    /// The environment last reported.
    var env = AsukaEnvironment()
    var envSent = false
    /// The anchor's frame and visibility last reported (x, y, w, h, 0|1).
    var anchorSent: [Float] = []
}

/// The overlays, in the order they were presented, and what follows them.
@MainActor final class AsukaOverlays {
    static let shared = AsukaOverlays()
    var list: [AsukaNode] = []
    var seq = 0
    var scrollObserver: Any?
    var anchorsQueued = false
}

/// An Overlay's view: presses go through it to what's under it, but for
/// its children's, unless it's Modal.
final class AsukaOverlayView: AsukaView {
    override func hitTest(_ point: NSPoint) -> NSView? {
        let h = super.hitTest(point)
        if h === self, !(asukaNode?.overlay?.modal ?? false) { return nil }
        return h
    }
}

/// Orders a view's subviews by rank (sortSubviews' comparator).
private func asukaRankOrder(_ a: NSView, _ b: NSView, _ ctx: UnsafeMutableRawPointer?) -> ComparisonResult {
    guard let ranks = ctx?.assumingMemoryBound(to: [ObjectIdentifier: Int].self).pointee else { return .orderedSame }
    let x = ranks[ObjectIdentifier(a)] ?? 0, y = ranks[ObjectIdentifier(b)] ?? 0
    return x < y ? .orderedAscending : x > y ? .orderedDescending : .orderedSame
}

@MainActor extension AsukaRuntime {
    var overlays: AsukaOverlays { AsukaOverlays.shared }

    func makeOverlay(_ n: AsukaNode) {
        n.view = AsukaOverlayView(frame: .zero)
        n.view?.autoresizingMask = []
        n.overlay = AsukaOverlay()
        overlays.list.append(n)
        startPointerMonitor()
        if overlays.scrollObserver == nil {
            overlays.scrollObserver = NotificationCenter.default.addObserver(forName: NSView.boundsDidChangeNotification, object: nil, queue: nil) { note in
                guard note.object is NSClipView else { return }
                MainActor.assumeIsolated { AsukaRuntime.shared.queueAnchors() }
            }
        }
    }

    func setOverlayProp(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        guard let o = n.overlay else { return }
        switch p {
        case AsukaProp.Overlay.root:
            o.root = UInt32(truncatingIfNeeded: v.int)
        case AsukaProp.Overlay.anchor:
            o.anchor = UInt32(truncatingIfNeeded: v.int)
            o.anchorSent = [] // reported again
        case AsukaProp.Overlay.modal:
            o.modal = v.bool
            n.view?.setAccessibilityModal(v.bool)
            a11yModalChanged(n.view?.window)
        default:
            break
        }
    }

    /// Present: over the root's other overlays.
    func presentOverlay(_ n: AsukaNode) {
        guard let o = n.overlay else { return }
        overlays.seq += 1
        o.seq = overlays.seq
        n.presented = true
        overlays.list.sort { ($0.overlay?.seq ?? 0) < ($1.overlay?.seq ?? 0) }
    }

    func overlayDestroyed(_ n: AsukaNode) {
        guard n.overlay != nil else { return }
        overlays.list.removeAll { $0 === n }
        if n.overlay?.modal == true { afterCore { [weak self] in self?.a11yModalChanged(self?.main?.window) } }
    }

    /// The overlays over the layout root top, in the order presented.
    func overlaysOver(_ top: AsukaNode) -> [AsukaNode] {
        overlays.list.filter { $0.overlay?.root == top.id && alive($0) }
    }

    /// The Modal overlays shown in w, in the order presented.
    func modalOverlays(_ w: NSWindow) -> [AsukaNode] {
        overlays.list.filter { n in
            guard n.overlay?.modal == true, alive(n), let v = n.view, v.window === w, !v.isHidden else { return false }
            return true
        }
    }

    /// Whether a modal overlay over v keeps the pointer from it (v isn't in
    /// that overlay).
    func behindModal(_ v: NSView) -> Bool {
        guard let w = v.window else { return false }
        if blockingSheet(w) != nil { return true } // a movable sheet over its window (Navigation.swift)
        for o in modalOverlays(w) {
            guard let ov = o.view else { continue }
            if v !== ov && !v.isDescendant(of: ov) { return true }
        }
        return false
    }

    /// Where an overlay's view goes, and its frame there: over its root's
    /// view (a Popover's: in it).
    func overlayPlace(_ o: AsukaOverlay) -> (NSView, CGRect)? {
        guard let r = nodes[o.root], let rv = r.view else { return nil }
        if r.kind == .popover { return (rv, rv.bounds) }
        guard let host = rv.superview else { return nil }
        return (host, rv.frame)
    }

    /// Puts each overlay in its place, sized to its root, on top; reports
    /// their environments; then where their anchors are.
    func syncOverlays() {
        guard !overlays.list.isEmpty else { return }
        var hosts: [ObjectIdentifier: NSView] = [:]
        for n in overlays.list {
            guard let o = n.overlay, let v = n.view, alive(n), let (host, frame) = overlayPlace(o) else { continue }
            if v.superview !== host { host.addSubview(v) }
            if v.frame != frame {
                v.frame = frame
                n.frame = frame
            }
            hosts[ObjectIdentifier(host)] = host
            reportOverlayEnv(n)
        }
        for host in hosts.values { raiseOverlays(host) }
        raiseBlockers()
        reportAnchors()
    }

    /// Keeps host's overlays its top subviews, in the order presented
    /// (sorted in place: a view moved isn't taken out of its window, its
    /// focus kept).
    func raiseOverlays(_ host: NSView) {
        let mine = overlays.list.filter { $0.view?.superview === host }.compactMap { $0.view }
        let subs = host.subviews
        guard !mine.isEmpty, subs.count >= mine.count else { return }
        if Array(subs.suffix(mine.count)).elementsEqual(mine, by: ===) { return }
        var ranks: [ObjectIdentifier: Int] = [:]
        for (i, s) in subs.enumerated() { ranks[ObjectIdentifier(s)] = i }
        for (i, v) in mine.enumerated() { ranks[ObjectIdentifier(v)] = subs.count + i }
        withUnsafeMutablePointer(to: &ranks) { p in
            host.sortSubviews(asukaRankOrder, context: UnsafeMutableRawPointer(p))
        }
    }

    /// Reports n's environment: its root's, at its size.
    func reportOverlayEnv(_ n: AsukaNode) {
        guard let o = n.overlay, let r = nodes[o.root], n.frame.width > 0 else { return }
        var e = env
        if let nav = r.nav, nav.reported { e = nav.env }
        e.width = Float(n.frame.width)
        e.height = Float(n.frame.height)
        if let w = n.view?.window, w.backingScaleFactor > 0 { e.scale = Float(w.backingScaleFactor) }
        if o.envSent && e == o.env { return }
        o.env = e
        o.envSent = true
        send(AsukaEvents.environment(n.id, e))
    }

    // MARK: Anchors

    /// Reports the anchors, once the run loop turns (a scroll's bounds
    /// change many times in one).
    func queueAnchors() {
        guard !overlays.anchorsQueued, !overlays.list.isEmpty else { return }
        overlays.anchorsQueued = true
        DispatchQueue.main.async {
            MainActor.assumeIsolated {
                let rt = AsukaRuntime.shared
                rt.overlays.anchorsQueued = false
                rt.reportAnchors()
            }
        }
    }

    /// Sends AnchorFrame for each overlay whose anchor moved, showed or
    /// hid since it was last sent.
    func reportAnchors() {
        for n in overlays.list {
            guard let o = n.overlay, o.anchor != 0, alive(n), n.listening(.anchorFrame),
                  let r = nodes[o.root], let rv = r.view, let a = nodes[o.anchor], let av = a.view else { continue }
            let same = av.window != nil && av.window === rv.window
            let f = same ? av.convert(av.bounds, to: rv) : CGRect(origin: .zero, size: a.frame.size)
            let vis = same && anchorShows(av)
            let vals: [Float] = [Float(f.minX), Float(f.minY), Float(f.width), Float(f.height), vis ? 1 : 0]
            if vals == o.anchorSent { continue }
            o.anchorSent = vals
            var b: [UInt8] = []
            for x in vals.prefix(4) { withUnsafeBytes(of: x.bitPattern.littleEndian) { b.append(contentsOf: $0) } }
            b.append(vis ? 1 : 0)
            send(AsukaEvents.anchorFrame(n.id, b))
        }
    }

    /// Whether any of v shows: it isn't hidden, and no clip view or view
    /// that clips around it, nor its window's content, cuts all of it off.
    /// Its clip views say when they scroll.
    func anchorShows(_ v: NSView) -> Bool {
        guard let w = v.window, !v.isHiddenOrHasHiddenAncestor else { return false }
        var r = v.bounds
        var at = v
        while let s = at.superview {
            r = at.convert(r, to: s)
            var clips = s is NSClipView || s.layer?.masksToBounds == true || s === w.contentView
            if #available(macOS 14.0, *), s.clipsToBounds { clips = true }
            if let c = s as? NSClipView, !c.postsBoundsChangedNotifications { c.postsBoundsChangedNotifications = true }
            if clips {
                r = r.intersection(s.bounds)
                if r.isNull { return false }
            }
            if s === w.contentView { break }
            at = s
        }
        return true
    }

    // MARK: Presses

    /// A press begins in ev's window: each overlay over a root there that
    /// it isn't on hears of it (OutsidePress), top first. True when a Modal
    /// one took it: it goes no further, though the overlay closes as it
    /// hears of it (what's behind it gets nothing).
    func overlaysPointerDown(_ ev: NSEvent) -> Bool {
        guard let w = ev.window, !overlays.list.isEmpty else { return false }
        var outside: [(UInt32, CGPoint)] = []
        var took = false
        var on = false // on an overlay's views above
        for n in overlays.list.reversed() {
            guard let o = n.overlay, alive(n), n.presented, let ov = n.view, ov.window === w, !ov.isHidden,
                  let r = nodes[o.root], let rv = r.view else { continue }
            let p = rv.convert(ev.locationInWindow, from: nil)
            guard rv.bounds.contains(p) else { continue }
            if onSubview(ov, ov.convert(ev.locationInWindow, from: nil)) {
                on = true
                continue
            }
            if n.listening(.outsidePress) { outside.append((n.id, p)) }
            if o.modal {
                took = !on
                break // what's under it gets nothing
            }
        }
        for (id, p) in outside { send(AsukaEvents.outsidePress(id, x: Double(p.x), y: Double(p.y))) }
        return took
    }

    /// Whether p (in v's coordinates) is on a view under v that shows.
    func onSubview(_ v: NSView, _ p: CGPoint) -> Bool {
        for s in v.subviews where !s.isHidden {
            let q = s.convert(p, from: v)
            if s.bounds.contains(q) || onSubview(s, q) { return true }
        }
        return false
    }

    /// The host's going back in w (its back button, ⌘[): the topmost
    /// overlay there that takes it (InterceptBack) gets Back first.
    func overlayBack(_ w: AsukaWin) -> Bool {
        for n in overlays.list.reversed() where alive(n) && n.interceptBack && n.view?.window === w.window {
            emit(n, .back) { AsukaEvents.back($0) }
            return true
        }
        return false
    }

    func overlayJSON(_ n: AsukaNode, _ d: inout [String: Any]) {
        guard let o = n.overlay else { return }
        var j: [String: Any] = ["root": o.root, "anchor": o.anchor, "modal": o.modal, "seq": o.seq, "presented": n.presented]
        if let v = n.view, let s = v.superview {
            j["hostClass"] = String(describing: type(of: s))
            if let hn = node(of: s) { j["host"] = hn.id }
            j["index"] = s.subviews.firstIndex { $0 === v } ?? -1
            j["siblings"] = s.subviews.count
        }
        if o.envSent { j["envSize"] = [Double(o.env.width), Double(o.env.height)] }
        if o.anchorSent.count == 5 { j["anchorFrame"] = o.anchorSent.map { Double($0) } }
        d["overlay"] = j
    }
}
