// The automation's steps for the visual part (the host's tests drive them
// through ASUKA_COMMANDS: Automation.swift calls visualStep for a step it
// doesn't know), and what they read back in the tree's JSON:
//
//   - mouse <kind> <id> <x> <y> [t], ...: mouse events (down, up, dragged,
//     rightdown, rightup) at x, y in node id's view, sent to its window as
//     the window server would; t: seconds after the step began (the
//     event's timestamp)
//   - wheel <id> <phase> <dx> <dy> <t>, ...: a trackpad's scroll events
//     (began, changed, ended, cancelled; continuous deltas) sent to node
//     id's view; t: seconds after the step began
//   - hover enter|exit <id>: the tracking area's entered or exited
//   - hoverprobe <x> <y> | off: the pointer at x, y in the root's view, as
//     the hover checks see it, or the real one
//   - appactive 0|1: the app resigning or becoming active
//   - hide <id> 0|1: hides or shows a view
//   - a11ypress <id>: VoiceOver's press
//   - readd <id>: takes a view out of its superview and puts it back
//   - popupselect <id> <index>: chooses a pop-up button's item
//   - pasteboard <name> text <s> | file <path> | png | local <id> <text>:
//     a pasteboard of its own with an item, for drags named name
//   - dragging <name> <seq> <method> <id> <x> <y>: an NSDraggingInfo of the
//     pasteboard, at x, y in node id's view, sent to its view (entered,
//     updated, exited, prepare, perform, ended); what it answered goes in
//     the tree's "visual".dragResults
//   - fakedrag on|off: drags record their pasteboards' types
//     ("visual".dragLog) instead of beginning a session
//   - dragmask outside|inside|valuesoutside, dragbegan, enddrag <op>: the
//     drag source's answers and its end
//   - nativeemit <id> <name> <float>: the test native view's event
//   - module <module> <method> <args>: calls one of the host's modules
//     ("web eval": u32 the node, then the script; others: the text); its
//     answer goes in "visual".moduleResults
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

/// A test's NSDraggingInfo: a pasteboard, a place, a sequence number.
final class AsukaTestDraggingInfo: NSObject, NSDraggingInfo {
    let pasteboard: NSPasteboard
    var location = CGPoint.zero
    let seq: Int
    weak var window: NSWindow?

    init(_ pb: NSPasteboard, _ seq: Int) {
        pasteboard = pb
        self.seq = seq
    }

    var draggingDestinationWindow: NSWindow? { window }
    var draggingSourceOperationMask: NSDragOperation { .every }
    var draggingLocation: NSPoint { location }
    var draggedImageLocation: NSPoint { location }
    var draggedImage: NSImage? { nil }
    var draggingPasteboard: NSPasteboard { pasteboard }
    var draggingSource: Any? { nil }
    var draggingSequenceNumber: Int { seq }
    func slideDraggedImage(to screenPoint: NSPoint) {}
    var draggingFormation: NSDraggingFormation = .default
    var animatesToDestination = false
    var numberOfValidItemsForDrop = 1
    func enumerateDraggingItems(options enumOpts: NSDraggingItemEnumerationOptions = [], for view: NSView?, classes classArray: [AnyClass],
                                searchOptions: [NSPasteboard.ReadingOptionKey: Any] = [:], using block: (NSDraggingItem, Int, UnsafeMutablePointer<ObjCBool>) -> Void) {}
    var springLoadingHighlight: NSSpringLoadingHighlight { .none }
    func resetSpringLoading() {}
}

/// The automation's native view: an NSSlider whose "value" prop it shows
/// (a typed view's float, or a float), and which emits "changed".
@MainActor final class AsukaTestNative: AsukaNativeView {
    let slider = NSSlider(frame: .zero)
    let ctx: AsukaNativeContext
    var set: [String] = []
    var destroyed = false
    static var made: [AsukaTestNative] = []

    init(_ ctx: AsukaNativeContext) {
        self.ctx = ctx
        slider.minValue = 0
        slider.maxValue = 1
        Self.made.append(self)
    }

    var view: NSView { slider }

    func set(_ name: String, _ value: AsukaNativeValue) {
        set.append(name)
        guard name.lowercased() == "value" else { return }
        switch value {
        case .bytes(let d) where d.count >= 8: slider.doubleValue = Double(bitPattern: [UInt8](d).u64(0))
        default: slider.doubleValue = value.float
        }
    }

    func sizeThatFits(_ size: CGSize) -> CGSize { CGSize(width: 160, height: 24) }
    func destroy() { destroyed = true }
}

/// What the visual steps keep.
@MainActor final class AsukaVisualTest {
    static let shared = AsukaVisualTest()
    var pasteboards: [String: NSPasteboard] = [:]
    var infos: [String: AsukaTestDraggingInfo] = [:]
    var dragResults: [String] = []
    var moduleResults: [String] = []
    var probe: CGPoint?
}

@MainActor extension AsukaRuntime {
    /// The automation's own native view kinds: a native component named
    /// MacTestSlider (in any package) is the test's slider.
    func nativeTestFactory(_ type: String) -> ((AsukaNativeContext) -> AsukaNativeView)? {
        guard AsukaAutomation.shared.active, type == "MacTestSlider" || type.hasSuffix("#MacTestSlider") else { return nil }
        return { AsukaTestNative($0) }
    }

    func visualStateJSON() -> [String: Any] {
        let t = AsukaVisualTest.shared
        var j: [String: Any] = ["mouseActive": visual.mouse.active, "dragLog": visual.dragLog, "dragResults": t.dragResults,
                                "moduleResults": t.moduleResults, "shaderClock": visual.shaderClock != nil,
                                "nativesMade": AsukaTestNative.made.count,
                                "ringsHolding": AsukaRing.all.allObjects.compactMap { $0 as? AsukaRing }.filter { $0.holds }.count,
                                "rings": AsukaRing.all.allObjects.count, "nativesDestroyed": AsukaTestNative.made.filter { $0.destroyed }.count]
        if let x = visual.mouse.target { j["mouseTarget"] = x.id }
        if let x = visual.over.target { j["overTarget"] = x.id }
        if let d = visual.drag { j["drag"] = ["id": d.id, "began": d.began, "items": d.items.count] }
        return j
    }

    /// Runs a step of the visual part's: nil if it isn't one (else its
    /// error, "" for none).
    func visualStep(_ verb: String, _ arg: String) -> String? {
        let t = AsukaVisualTest.shared
        let a = arg.split(separator: " ", omittingEmptySubsequences: true).map(String.init)
        func node(_ s: String) -> AsukaNode? { UInt32(s).flatMap { nodes[$0] } }
        func point(_ n: AsukaNode, _ x: String, _ y: String) -> CGPoint? {
            guard let v = n.view, let x = Double(x), let y = Double(y) else { return nil }
            return v.convert(CGPoint(x: x, y: y), to: nil)
        }
        switch verb {
        case "mouse":
            let base = ProcessInfo.processInfo.systemUptime
            for ev in arg.split(separator: ",") {
                let p = ev.split(separator: " ").map(String.init)
                guard p.count >= 4, let n = node(p[1]), let w = n.view?.window, let at = point(n, p[2], p[3]) else { return "want mouse <kind> <id> <x> <y> [t]" }
                let types: [String: NSEvent.EventType] = ["down": .leftMouseDown, "up": .leftMouseUp, "dragged": .leftMouseDragged,
                                                          "rightdown": .rightMouseDown, "rightup": .rightMouseUp]
                guard let type = types[p[0]] else { return "no mouse event \(p[0])" }
                if !w.isVisible { w.orderFront(nil) } // a window off screen takes no clicks
                let ts = base + (p.count > 4 ? Double(p[4]) ?? 0 : 0)
                guard let e = NSEvent.mouseEvent(with: type, location: at, modifierFlags: [], timestamp: ts, windowNumber: w.windowNumber,
                                                 context: nil, eventNumber: 0, clickCount: 1, pressure: 1) else { return "no event" }
                // What the pointer's monitor sees of a press (Focus.swift):
                // the window's sendEvent skips the application's monitors.
                if type != .leftMouseDragged && pointerEvent(e) { continue }
                focusState.testEvent = e
                w.sendEvent(e)
                focusState.testEvent = nil
            }
        case "wheel":
            // A trackpad's scroll over a view, as the window server sends
            // it: continuous (precise) deltas in a phase, at t seconds
            // after the step began.
            let base = ProcessInfo.processInfo.systemUptime
            for ev in arg.split(separator: ",") {
                let p = ev.split(separator: " ").map(String.init)
                let phases: [String: Int64] = ["began": 1, "changed": 2, "ended": 4, "cancelled": 8]
                guard p.count == 5, let n = node(p[0]), let v = n.view, let phase = phases[p[1]], let dx = Int32(p[2]), let dy = Int32(p[3]),
                      let t = Double(p[4]) else { return "want wheel <id> <phase> <dx> <dy> <t>, ..." }
                guard let cg = CGEvent(scrollWheelEvent2Source: nil, units: .pixel, wheelCount: 2, wheel1: dy, wheel2: dx, wheel3: 0) else { return "no event" }
                cg.setIntegerValueField(.scrollWheelEventIsContinuous, value: 1)
                cg.setIntegerValueField(.scrollWheelEventScrollPhase, value: phase)
                cg.timestamp = CGEventTimestamp((base + t) * 1e9)
                guard let e = NSEvent(cgEvent: cg) else { return "no event" }
                v.scrollWheel(with: e)
            }
        case "hover":
            guard a.count == 2, let n = node(a[1]), let v = n.view else { return "want hover enter|exit <id>" }
            if a[0] == "enter" { mouseEntered(v) } else { mouseExited(v) }
        case "hoverprobe":
            if a.first == "off" {
                visual.hoverProbe = nil
                return ""
            }
            guard a.count == 2, let x = Double(a[0]), let y = Double(a[1]), let r = root?.view else { return "want hoverprobe <x> <y>" }
            let p = r.convert(CGPoint(x: x, y: y), to: nil)
            let w = r.window
            visual.hoverProbe = { win in win === w ? p : nil }
        case "appactive":
            appActive(a.first == "1")
        case "hide":
            guard a.count == 2, let v = node(a[0])?.view else { return "want hide <id> 0|1" }
            v.isHidden = a[1] == "1"
        case "readd":
            // Out of its superview and in again (AppKit sets a layer's shadow
            // from the view's as it moves into a window).
            guard let v = a.first.flatMap(node)?.view, let sup = v.superview else { return "no view" }
            v.removeFromSuperview()
            sup.addSubview(v)
        case "popupselect":
            // The user chooses a pop-up's item, as its menu does: the item,
            // then the action.
            guard a.count == 2, let p = node(a[0])?.view as? NSPopUpButton, let i = Int(a[1]) else { return "want popupselect <id> <index>" }
            p.selectItem(at: i)
            _ = p.sendAction(p.action, to: p.target)
        case "a11ypress":
            guard let v = a.first.flatMap(node)?.view else { return "no view" }
            _ = v.accessibilityPerformPress()
        case "pasteboard":
            guard a.count >= 2 else { return "want pasteboard <name> <kind> ..." }
            let pb = NSPasteboard(name: NSPasteboard.Name("asuka-test-\(getpid())-\(a[0])"))
            pb.clearContents()
            let item = NSPasteboardItem()
            let rest = a.count > 2 ? a[2...].joined(separator: " ") : ""
            switch a[1] {
            case "text": item.setString(rest, forType: .string)
            case "file": item.setString(URL(fileURLWithPath: rest).absoluteString, forType: .fileURL)
            case "png":
                guard let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 2, pixelsHigh: 2, bitsPerSample: 8, samplesPerPixel: 4,
                                                 hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0),
                      let png = rep.representation(using: .png, properties: [:]) else { return "no png" }
                item.setData(png, forType: .png)
                t.moduleResults.append("png \(png.count)")
            case "local":
                guard a.count >= 4 else { return "want pasteboard <name> local <id> <text>" }
                item.setString("\(getpid()):\(a[2])", forType: asukaDragType)
                item.setString(a[3...].joined(separator: " "), forType: .string)
            default:
                return "no pasteboard kind \(a[1])"
            }
            pb.writeObjects([item])
            t.pasteboards[a[0]] = pb
        case "dragging":
            guard a.count == 6, let pb = t.pasteboards[a[0]], let seq = Int(a[1]), let n = node(a[3]), let v = n.view,
                  let at = point(n, a[4], a[5]) else { return "want dragging <pasteboard> <seq> <method> <id> <x> <y>" }
            let key = a[0] + "-" + a[1]
            let info = t.infos[key] ?? AsukaTestDraggingInfo(pb, seq)
            t.infos[key] = info
            info.location = at
            info.window = v.window
            var r = ""
            switch a[2] {
            case "entered": r = "\(v.draggingEntered(info).rawValue)"
            case "updated": r = "\(v.draggingUpdated(info).rawValue)"
            case "exited": v.draggingExited(info)
            case "prepare": r = "\(v.prepareForDragOperation(info))"
            case "perform": r = "\(v.performDragOperation(info))"
            case "ended": v.draggingEnded(info)
            default: return "no dragging method \(a[2])"
            }
            t.dragResults.append("\(a[2])=\(r)")
        case "fakedrag":
            visual.fakeSessions = a.first == "on"
            visual.dragLog = []
        case "dragmask":
            switch a.first {
            case "inside": t.dragResults.append("mask=\(dragMask(.withinApplication).rawValue)")
            case "valuesoutside":
                guard let d = visual.drag else { return "no drag" }
                let items = d.items
                d.items = [AsukaItem(mime: "application/x-n", value: true)]
                t.dragResults.append("mask=\(dragMask(.outsideApplication).rawValue)")
                d.items = items
            default: t.dragResults.append("mask=\(dragMask(.outsideApplication).rawValue)")
            }
        case "dragbegan":
            dragBegan()
        case "enddrag":
            endDrag(NSDragOperation(rawValue: UInt(a.first ?? "0") ?? 0))
        case "nativeemit":
            guard a.count == 3, let n = node(a[0]), let nv = n.native?.native as? AsukaTestNative, let f = Double(a[2]) else { return "want nativeemit <id> <name> <float>" }
            // A typed view's event: its parameters encoded, as "v".
            var w = AsukaWriter()
            w.f64(f)
            nv.ctx.emit(a[1], ["v": .bytes(Data(w.bytes)), "value": .float(f)])
        case "module":
            guard a.count >= 2 else { return "want module <module> <method> <args>" }
            let rest = a.count > 2 ? a[2...].joined(separator: " ") : ""
            var args = [UInt8]()
            if a[0] == "web" && a[1] == "eval" {
                let p = rest.split(separator: " ", maxSplits: 1).map(String.init)
                let id = UInt32(p.first ?? "") ?? 0
                withUnsafeBytes(of: id.littleEndian) { args.append(contentsOf: $0) }
                args += Array((p.count > 1 ? p[1] : "").utf8)
            } else {
                args = Array(rest.utf8)
            }
            let i = t.moduleResults.count
            t.moduleResults.append("")
            callModule(a[0], a[1], Data(args)) { r in
                DispatchQueue.main.async {
                    MainActor.assumeIsolated {
                        switch r {
                        case .success(let d): AsukaVisualTest.shared.moduleResults[i] = "ok " + String(decoding: d, as: UTF8.self)
                        case .failure(let e): AsukaVisualTest.shared.moduleResults[i] = "error " + asukaErrorText(e)
                        }
                    }
                }
            }
        default:
            return nil
        }
        return ""
    }
}
