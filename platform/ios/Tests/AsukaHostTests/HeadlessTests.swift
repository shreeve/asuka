#if canImport(UIKit)
import XCTest
import UIKit
import CAsukaStub
@testable import AsukaHost
@testable import AsukaOps

// RFC 0010's host contract on the iOS host: the Overlay's placement over
// its root and its z-order, its hit testing, outside presses (modal and
// not), AnchorFrame (after a scroll too), FocusChange, Tab in a
// FocusTrap, NoFocusRing, KeyText, PressPoint, and each accessibility
// prop on the view's UIAccessibility. The ops go to the runtime as the
// decoder hands them over; the core is a stub that keeps the events the
// host sends (Tests/CAsukaStub).

/// An event the host sent: its view, kind and payload.
struct SentEvent {
    var id: UInt32
    var kind: UInt16
    var payload: [UInt8]

    var int: Int64 {
        var r = Reader(payload)
        return Int64(bitPattern: r.u64())
    }
    var point: CGPoint {
        var r = Reader(payload)
        return CGPoint(x: r.f64(), y: r.f64())
    }
    var string: String {
        var r = Reader(payload)
        let n = Int(r.u32())
        return String(decoding: payload[4..<4 + n], as: UTF8.self)
    }
    /// An AnchorFrame's frame and visibility.
    var anchor: (CGRect, Bool) {
        var r = Reader(payload)
        _ = r.u32() // the bytes' length
        let x = r.f32(), y = r.f32(), w = r.f32(), h = r.f32()
        return (CGRect(x: CGFloat(x), y: CGFloat(y), width: CGFloat(w), height: CGFloat(h)), r.u8() != 0)
    }
    /// An Environment's width and height.
    var size: CGSize {
        var r = Reader(payload)
        return CGSize(width: CGFloat(r.f32()), height: CGFloat(r.f32()))
    }
}

struct Reader {
    let b: [UInt8]
    var i = 0
    init(_ b: [UInt8]) { self.b = b }
    mutating func u8() -> UInt8 { defer { i += 1 }; return b[i] }
    mutating func u16() -> UInt16 { UInt16(u8()) | UInt16(u8()) << 8 }
    mutating func u32() -> UInt32 { UInt32(u16()) | UInt32(u16()) << 16 }
    mutating func u64() -> UInt64 { UInt64(u32()) | UInt64(u32()) << 32 }
    mutating func f32() -> Float { Float(bitPattern: u32()) }
    mutating func f64() -> Double { Double(bitPattern: u64()) }
}

@MainActor final class HeadlessTests: XCTestCase {
    var rt: AsukaRuntime { .shared }
    static var window: UIWindow?
    static var vc: AsukaViewController?
    var made: [UInt32] = []
    static var nextID: UInt32 = 1000

    override func setUp() async throws {
        if Self.window == nil {
            let w = UIWindow(frame: CGRect(x: 0, y: 0, width: 390, height: 844))
            let vc = AsukaViewController()
            w.rootViewController = vc
            w.makeKeyAndVisible()
            Self.window = w
            Self.vc = vc
            vc.view.layoutIfNeeded()
        }
        asuka_stub_reset()
    }

    override func tearDown() async throws {
        for id in made.reversed() { rt.destroy(id) }
        made = []
        rt.focusedID = 0
        rt.keyboardModality = false
    }

    var window: UIWindow { Self.window! }
    var vc: AsukaViewController { Self.vc! }

    // MARK: Helpers

    func make(_ kind: AsukaKind) -> UInt32 {
        Self.nextID += 1
        let id = Self.nextID
        rt.create(id, kind: kind.rawValue)
        made.append(id)
        return id
    }

    func view(_ id: UInt32) -> UIView { rt.nodes[id]!.view }

    func frame(_ id: UInt32, _ r: CGRect) {
        rt.setFrame(id, x: Float(r.minX), y: Float(r.minY), width: Float(r.width), height: Float(r.height))
    }

    func set(_ id: UInt32, _ prop: UInt16, _ v: AsukaValue) { rt.setProp(id, prop: prop, value: v) }

    func bytes(_ b: [UInt8], _ f: (UnsafeRawBufferPointer) -> Void) { b.withUnsafeBytes { f($0) } }

    func command(_ id: UInt32, _ c: AsukaCommand, _ args: [UInt8] = []) {
        bytes(args) { rt.command(id, command: c.rawValue, args: $0) }
    }

    /// The events sent since the last call, and forgets them.
    func sent() -> [SentEvent] {
        var p: UnsafePointer<UInt8>?
        let n = asuka_stub_events(&p)
        var out: [SentEvent] = []
        if let p, n > 0 {
            let all = Array(UnsafeBufferPointer(start: p, count: n))
            var r = Reader(all)
            while r.i < all.count {
                let len = Int(r.u32())
                var e = Reader(Array(all[r.i..<r.i + len]))
                let id = e.u32(), kind = e.u16()
                out.append(SentEvent(id: id, kind: kind, payload: Array(all[r.i + 6..<r.i + len])))
                r.i += len
            }
        }
        asuka_stub_reset()
        return out
    }

    func sent(_ kind: AsukaEventKind) -> [SentEvent] { sent().filter { $0.kind == kind.rawValue } }

    /// Runs the main queue until cond holds (work the host put off: an
    /// anchor's report after a scroll).
    func waitUntil(_ cond: () -> Bool) {
        for _ in 0..<1000 where !cond() {
            RunLoop.main.run(until: Date().addingTimeInterval(0.01))
        }
        XCTAssertTrue(cond())
    }

    /// A root view filling the root controller, with the overlays' root.
    func root() -> UInt32 {
        let r = make(.view)
        rt.setRoot(r)
        frame(r, vc.view.bounds)
        return r
    }

    func overlay(over root: UInt32, modal: Bool = false) -> UInt32 {
        let o = make(.overlay)
        set(o, AsukaProp.Overlay.root, .int(Int64(root)))
        set(o, AsukaProp.Overlay.modal, .bool(modal))
        rt.listen(o, event: AsukaEventKind.outsidePress.rawValue)
        rt.listen(o, event: AsukaEventKind.anchorFrame.rawValue)
        command(o, .present)
        return o
    }

    // MARK: Overlay

    func testOverlayOverItsRoot() {
        let r = root()
        let a = overlay(over: r)
        let b = overlay(over: r)
        let host = vc.view!
        XCTAssertTrue(view(a).superview === host)
        XCTAssertEqual(view(a).frame, host.bounds)
        // The last presented on top; presenting one again brings it up.
        XCTAssertTrue(host.subviews.last === view(b))
        command(a, .present)
        XCTAssertTrue(host.subviews.last === view(a))
        // A root mounted again stays under them.
        rt.setRoot(r)
        XCTAssertTrue(host.subviews.last === view(a))
        XCTAssertTrue(host.subviews[host.subviews.count - 2] === view(b))
        // Its children are in the root's coordinates.
        let c = make(.view)
        rt.insert(a, child: c, index: 0)
        frame(c, CGRect(x: 20, y: 30, width: 100, height: 40))
        XCTAssertEqual(view(c).convert(view(c).bounds, to: host), CGRect(x: 20, y: 30, width: 100, height: 40))
    }

    func testOverlayEnvironment() {
        let r = root()
        let o = make(.overlay)
        _ = sent()
        set(o, AsukaProp.Overlay.root, .int(Int64(r)))
        let env = sent(.environment).filter { $0.id == o }
        XCTAssertEqual(env.count, 1)
        XCTAssertEqual(env.first?.size, vc.view.bounds.size)
        // Unchanged: not again.
        view(o).setNeedsLayout()
        view(o).layoutIfNeeded()
        XCTAssertEqual(sent(.environment).filter { $0.id == o }.count, 0)
    }

    func testOverlayOverASheet() {
        let r = root()
        let s = make(.sheet)
        let content = make(.view)
        rt.insert(s, child: content, index: 0)
        let o = overlay(over: s)
        let sheetView = rt.nodes[s]!.container
        XCTAssertTrue(view(o).superview === sheetView)
        // The sheet's children inserted after it stay under it.
        let more = make(.view)
        rt.insert(s, child: more, index: 1)
        XCTAssertTrue(sheetView.subviews.last === view(o))
        _ = r
    }

    func testOverlayHitTesting() {
        let r = root()
        let o = overlay(over: r)
        let c = make(.view)
        rt.insert(o, child: c, index: 0)
        frame(c, CGRect(x: 10, y: 10, width: 50, height: 50))
        let ov = view(o)
        XCTAssertTrue(ov.hitTest(CGPoint(x: 20, y: 20), with: nil) === view(c))
        XCTAssertNil(ov.hitTest(CGPoint(x: 200, y: 200), with: nil)) // passes through
        set(o, AsukaProp.Overlay.modal, .bool(true))
        XCTAssertTrue(ov.hitTest(CGPoint(x: 200, y: 200), with: nil) === ov) // the layer takes it
        XCTAssertTrue(ov.accessibilityViewIsModal)
        set(o, AsukaProp.Overlay.modal, .bool(false))
        XCTAssertFalse(ov.accessibilityViewIsModal)
    }

    func testOutsidePress() {
        let r = root()
        let under = make(.view)
        rt.insert(r, child: under, index: 0)
        frame(under, CGRect(x: 0, y: 0, width: 390, height: 844))
        let o = overlay(over: r)
        let c = make(.view)
        rt.insert(o, child: c, index: 0)
        frame(c, CGRect(x: 10, y: 10, width: 50, height: 50))
        _ = sent()
        func press(_ p: CGPoint) {
            let at = vc.view.convert(p, to: window)
            rt.outsidePress(in: window, at: at, on: window.hitTest(at, with: nil))
        }
        press(CGPoint(x: 20, y: 20)) // on its child
        XCTAssertEqual(sent(.outsidePress).count, 0)
        press(CGPoint(x: 200, y: 300)) // on what's under it
        var e = sent(.outsidePress)
        XCTAssertEqual(e.count, 1)
        XCTAssertEqual(e.first?.id, o)
        XCTAssertEqual(e.first?.point, CGPoint(x: 200, y: 300))
        // Modal: the layer takes it, and it's reported.
        set(o, AsukaProp.Overlay.modal, .bool(true))
        let at = vc.view.convert(CGPoint(x: 200, y: 300), to: window)
        XCTAssertTrue(window.hitTest(at, with: nil) === view(o))
        press(CGPoint(x: 200, y: 300))
        e = sent(.outsidePress)
        XCTAssertEqual(e.count, 1)
        // Not listening: not reported.
        rt.unlisten(o, event: AsukaEventKind.outsidePress.rawValue)
        press(CGPoint(x: 200, y: 300))
        XCTAssertEqual(sent(.outsidePress).count, 0)
    }

    func testAnchorFrame() {
        let r = root()
        let s = make(.scroll)
        rt.insert(r, child: s, index: 0)
        frame(s, CGRect(x: 0, y: 100, width: 390, height: 400))
        let content = make(.view)
        rt.insert(s, child: content, index: 0)
        frame(content, CGRect(x: 0, y: 0, width: 390, height: 2000))
        let anchor = make(.view)
        rt.insert(content, child: anchor, index: 0)
        frame(anchor, CGRect(x: 30, y: 200, width: 80, height: 44))
        let o = overlay(over: r)
        _ = sent()
        set(o, AsukaProp.Overlay.anchor, .int(Int64(anchor)))
        rt.reportAnchors()
        var e = sent(.anchorFrame)
        XCTAssertEqual(e.count, 1)
        XCTAssertEqual(e.first?.anchor.0, CGRect(x: 30, y: 300, width: 80, height: 44))
        XCTAssertEqual(e.first?.anchor.1, true)
        // Unchanged: not again.
        rt.reportAnchors()
        XCTAssertEqual(sent(.anchorFrame).count, 0)
        // Scrolled: reported again, where it is now.
        (view(s) as! UIScrollView).contentOffset = CGPoint(x: 0, y: 150)
        var got: [SentEvent] = []
        waitUntil { got += sent(.anchorFrame); return !got.isEmpty }
        XCTAssertEqual(got.last?.anchor.0, CGRect(x: 30, y: 150, width: 80, height: 44))
        XCTAssertEqual(got.last?.anchor.1, true)
        // Scrolled out of the scroll view's visible part: not visible.
        (view(s) as! UIScrollView).contentOffset = CGPoint(x: 0, y: 600)
        got = []
        waitUntil { got += sent(.anchorFrame); return !got.isEmpty }
        XCTAssertEqual(got.last?.anchor.1, false)
        // Back, then hidden: not visible.
        (view(s) as! UIScrollView).contentOffset = .zero
        got = []
        waitUntil { got += sent(.anchorFrame); return !got.isEmpty }
        XCTAssertEqual(got.last?.anchor.1, true)
        set(anchor, AsukaProp.hidden, .bool(true))
        rt.reportAnchors()
        e = sent(.anchorFrame)
        XCTAssertEqual(e.last?.anchor.1, false)
    }

    // MARK: Focus

    func testFocusable() {
        let r = root()
        let v = make(.view)
        rt.insert(r, child: v, index: 0)
        let c = view(v) as! AsukaContainerView
        XCTAssertFalse(c.canBecomeFocused)
        set(v, AsukaProp.focusable, .bool(true))
        XCTAssertTrue(c.canBecomeFocused)
        set(v, AsukaProp.disabled, .bool(true))
        XCTAssertFalse(c.canBecomeFocused)
        set(v, AsukaProp.disabled, .bool(false))
        set(v, AsukaProp.focusable, .bool(false))
        rt.listen(v, event: AsukaEventKind.press.rawValue) // pressable: a tab stop too
        XCTAssertTrue(c.canBecomeFocused)
    }

    func testFocusChange() {
        let r = root()
        let a = make(.view), b = make(.view)
        rt.insert(r, child: a, index: 0)
        rt.insert(r, child: b, index: 1)
        set(a, AsukaProp.focusable, .bool(true))
        set(b, AsukaProp.focusable, .bool(true))
        _ = sent()
        // Focus with no key before it: 1, sent without being listened for.
        command(a, .focus)
        var e = sent(.focusChange)
        XCTAssertEqual(e.map { $0.id }, [a])
        XCTAssertEqual(e.map { $0.int }, [1])
        // Right after a key: visible, 2; the one that had it loses it.
        rt.noteKey()
        command(b, .focus)
        e = sent(.focusChange)
        XCTAssertEqual(e.map { $0.id }, [a, b])
        XCTAssertEqual(e.map { $0.int }, [0, 2])
        // Again: nothing.
        command(b, .focus)
        XCTAssertEqual(sent(.focusChange).count, 0)
        command(b, .blur)
        e = sent(.focusChange)
        XCTAssertEqual(e.map { $0.id }, [b])
        XCTAssertEqual(e.map { $0.int }, [0])
        // A touch after the key: 1 again.
        rt.notePointer()
        command(a, .focus)
        XCTAssertEqual(sent(.focusChange).map { $0.int }, [1])
    }

    func testFocusTrapTab() {
        let r = root()
        let outside = make(.view)
        rt.insert(r, child: outside, index: 0)
        set(outside, AsukaProp.focusable, .bool(true))
        let trap = make(.view)
        rt.insert(r, child: trap, index: 1)
        set(trap, AsukaProp.focusTrap, .bool(true))
        XCTAssertNotNil(view(trap).focusGroupIdentifier)
        var items: [UInt32] = []
        for i in 0..<3 {
            let v = make(.view)
            rt.insert(trap, child: v, index: i)
            set(v, AsukaProp.focusable, .bool(true))
            items.append(v)
        }
        let hidden = make(.view) // not a stop
        rt.insert(trap, child: hidden, index: 3)
        set(hidden, AsukaProp.focusable, .bool(true))
        set(hidden, AsukaProp.hidden, .bool(true))
        command(outside, .focus)
        XCTAssertTrue(rt.trapCommands().isEmpty) // not in a trap
        command(items[0], .focus)
        XCTAssertEqual(rt.trapCommands().count, 2)
        XCTAssertTrue(rt.keyCommands().contains { $0.input == "\t" && $0.modifierFlags == .shift })
        _ = sent()
        rt.tabInTrap(backward: false)
        XCTAssertEqual(rt.focusedID, items[1])
        rt.tabInTrap(backward: false)
        rt.tabInTrap(backward: false) // round to the first, never out
        XCTAssertEqual(rt.focusedID, items[0])
        rt.tabInTrap(backward: true)
        XCTAssertEqual(rt.focusedID, items[2])
        let e = sent(.focusChange).filter { $0.int != 0 }
        XCTAssertEqual(e.map { $0.int }, [2, 2, 2, 2]) // from the keyboard
        // The focus system's moves don't leave it either.
        XCTAssertTrue((view(trap) as! AsukaContainerView).focusTrap)
    }

    func testNoFocusRing() {
        let r = root()
        let v = make(.view)
        rt.insert(r, child: v, index: 0)
        set(v, AsukaProp.focusable, .bool(true))
        XCTAssertNotNil(view(v).focusEffect) // the system's
        set(v, AsukaProp.noFocusRing, .bool(true))
        XCTAssertNil(view(v).focusEffect)
        set(v, AsukaProp.noFocusRing, .bool(false))
        XCTAssertNotNil(view(v).focusEffect) // its own again (a copy)
    }

    func testKeysToTheFocusedView() {
        let r = root()
        let v = make(.view)
        rt.insert(r, child: v, index: 0)
        set(v, AsukaProp.focusable, .bool(true))
        rt.listen(v, event: AsukaEventKind.keyText.rawValue)
        rt.listen(v, event: AsukaEventKind.press.rawValue)
        command(v, .focus)
        _ = sent()
        XCTAssertTrue(rt.focusedKey(.keyboardA, "a", []))
        XCTAssertTrue(rt.focusedKey(.keyboardB, "B", .shift))
        XCTAssertFalse(rt.focusedKey(.keyboardC, "c", .command)) // a shortcut, not text
        XCTAssertTrue(rt.focusedKey(.keyboardReturnOrEnter, "\r", [])) // presses it
        XCTAssertTrue(rt.focusedKey(.keyboardSpacebar, " ", []))
        XCTAssertFalse(rt.focusedKey(.keyboardTab, "\t", []))
        let e = sent().filter { $0.kind == AsukaEventKind.keyText.rawValue || $0.kind == AsukaEventKind.press.rawValue }
        XCTAssertEqual(e.map { $0.kind }, [AsukaEventKind.keyText.rawValue, AsukaEventKind.keyText.rawValue,
                                           AsukaEventKind.press.rawValue, AsukaEventKind.press.rawValue])
        XCTAssertEqual(e.prefix(2).map { $0.string }, ["a", "B"])
        // Not pressing: Space is text.
        rt.unlisten(v, event: AsukaEventKind.press.rawValue)
        XCTAssertTrue(rt.focusedKey(.keyboardSpacebar, " ", []))
        XCTAssertEqual(sent(.keyText).map { $0.string }, [" "])
        // The Keys of the views around the focused one come first.
        let k = AsukaKeysTestEncoding.encode([AsukaKey("down")])
        bytes(k) { set(r, AsukaProp.keys, .bytes($0)) }
        XCTAssertTrue(rt.keyCommands().contains { $0.input == UIKeyCommand.inputDownArrow })
    }

    func testPressPoint() {
        let r = root()
        let v = make(.view)
        rt.insert(r, child: v, index: 0)
        rt.listen(v, event: AsukaEventKind.press.rawValue)
        rt.listen(v, event: AsukaEventKind.pressPoint.rawValue)
        let g = view(v).gestureRecognizers!.compactMap { $0 as? AsukaPressRecognizer }.first!
        _ = sent()
        g.start = CGPoint(x: 5, y: 7)
        rt.pressRecognized(g)
        let e = sent().filter { $0.kind == AsukaEventKind.pressPoint.rawValue || $0.kind == AsukaEventKind.press.rawValue }
        XCTAssertEqual(e.map { $0.kind }, [AsukaEventKind.pressPoint.rawValue, AsukaEventKind.press.rawValue]) // the point first
        XCTAssertEqual(e.first?.point, CGPoint(x: 5, y: 7))
        rt.pressRecognized(g) // once a press
        XCTAssertEqual(sent(.pressPoint).count, 0)
    }

    // MARK: Accessibility

    func role(_ id: UInt32, _ r: AsukaRole) { set(id, AsukaProp.a11yRole, .enumValue(r.rawValue)) }

    func testCheckable() {
        let r = root()
        let v = make(.view)
        rt.insert(r, child: v, index: 0)
        role(v, .checkbox)
        set(v, AsukaProp.a11yChecked, .enumValue(AsukaA11yChecked.checked.rawValue))
        let view = view(v)
        XCTAssertTrue(view.isAccessibilityElement)
        XCTAssertTrue(view.accessibilityTraits.contains(.button))
        XCTAssertTrue(view.accessibilityTraits.contains(.toggleButton))
        XCTAssertEqual(view.accessibilityValue, "1")
        set(v, AsukaProp.a11yChecked, .enumValue(AsukaA11yChecked.none.rawValue)) // none reads as unchecked
        XCTAssertEqual(view.accessibilityValue, "0")
        // A radio: selected while checked.
        role(v, .radio)
        XCTAssertFalse(view.accessibilityTraits.contains(.toggleButton)) // the checkbox's traits went
        XCTAssertFalse(view.accessibilityTraits.contains(.selected))
        set(v, AsukaProp.a11yChecked, .enumValue(AsukaA11yChecked.checked.rawValue))
        XCTAssertTrue(view.accessibilityTraits.contains(.selected))
        XCTAssertNil(view.accessibilityValue)
    }

    func testSelectedExpandedValueRange() {
        let r = root()
        let v = make(.view)
        rt.insert(r, child: v, index: 0)
        let view = view(v)
        role(v, .tab)
        set(v, AsukaProp.a11ySelected, .bool(true))
        XCTAssertTrue(view.accessibilityTraits.contains(.selected))
        set(v, AsukaProp.a11ySelected, .bool(false))
        XCTAssertFalse(view.accessibilityTraits.contains(.selected))
        role(v, .comboBox)
        set(v, AsukaProp.a11yExpanded, .enumValue(AsukaA11yExpanded.expanded.rawValue))
        if #available(iOS 18.0, *) { XCTAssertEqual(view.accessibilityExpandedStatus, .expanded) }
        set(v, AsukaProp.a11yExpanded, .enumValue(AsukaA11yExpanded.collapsed.rawValue))
        if #available(iOS 18.0, *) { XCTAssertEqual(view.accessibilityExpandedStatus, .collapsed) }
        role(v, .progressBar)
        var w = AsukaWriter()
        w.f64(0); w.f64(10); w.f64(4)
        bytes(w.bytes) { set(v, AsukaProp.a11yRange, .bytes($0)) }
        XCTAssertEqual(view.accessibilityValue, NumberFormatter.localizedString(from: 0.4, number: .percent))
        role(v, .slider)
        XCTAssertTrue(view.accessibilityTraits.contains(.adjustable))
        XCTAssertEqual(view.accessibilityValue, "4")
        set(v, AsukaProp.a11yValue, .string("four of ten"))
        XCTAssertEqual(view.accessibilityValue, "four of ten")
        set(v, AsukaProp.a11yValue, .string(""))
        bytes([]) { set(v, AsukaProp.a11yRange, .bytes($0)) }
        XCTAssertNil(view.accessibilityValue)
    }

    func testContainersAndModal() {
        let r = root()
        let list = make(.view)
        rt.insert(r, child: list, index: 0)
        role(list, .listbox)
        set(list, AsukaProp.a11yLabel, .string("Fruit"))
        XCTAssertFalse(view(list).isAccessibilityElement) // its options are the elements
        XCTAssertEqual(view(list).accessibilityContainerType, .list)
        XCTAssertEqual(view(list).accessibilityLabel, "Fruit")
        let tabs = make(.view)
        rt.insert(r, child: tabs, index: 1)
        role(tabs, .tabList)
        XCTAssertTrue(view(tabs).accessibilityTraits.contains(.tabBar))
        // A dialog in an overlay: VoiceOver stays in it and the overlay.
        let o = overlay(over: r)
        let d = make(.view)
        rt.insert(o, child: d, index: 0)
        role(d, .dialog)
        set(d, AsukaProp.a11yModal, .bool(true))
        XCTAssertEqual(view(d).accessibilityContainerType, .semanticGroup)
        XCTAssertTrue(view(d).accessibilityViewIsModal)
        XCTAssertTrue(view(o).accessibilityViewIsModal)
        set(d, AsukaProp.a11yModal, .bool(false))
        XCTAssertFalse(view(o).accessibilityViewIsModal)
    }

    func testRelations() {
        let r = root()
        let field = make(.view)
        rt.insert(r, child: field, index: 0)
        set(field, AsukaProp.a11yLabel, .string("own"))
        // Named before the view it names is made: its text once it is.
        let labelID = Self.nextID + 1
        set(field, AsukaProp.a11yLabelledBy, .int(Int64(labelID)))
        let label = make(.text)
        XCTAssertEqual(label, labelID)
        rt.insert(r, child: label, index: 1)
        set(label, AsukaProp.Text.text, .string("Email"))
        let hint = make(.text)
        rt.insert(r, child: hint, index: 2)
        set(hint, AsukaProp.Text.text, .string("We never share it"))
        set(field, AsukaProp.a11yDescribedBy, .int(Int64(hint)))
        rt.refreshRelations() // as after a batch
        XCTAssertEqual(view(field).accessibilityLabel, "Email")
        XCTAssertEqual(view(field).accessibilityHint, "We never share it")
        XCTAssertTrue(view(field).isAccessibilityElement)
        // Its text changed: followed.
        set(label, AsukaProp.Text.text, .string("Work email"))
        rt.refreshRelations()
        XCTAssertEqual(view(field).accessibilityLabel, "Work email")
        // None again: its own.
        set(field, AsukaProp.a11yLabelledBy, .int(0))
        XCTAssertEqual(view(field).accessibilityLabel, "own")
    }

    // MARK: Navigation

    /// A tab's page is built when the tab is first chosen, so its Stack
    /// comes into a page that's already in the window: it's the page's
    /// child controller, as one there from the start is once it appears,
    /// or UIKit throws (UIViewControllerHierarchyInconsistency).
    func testStackIntoAShownTab() {
        let r = root()
        let tabs = make(.tabs)
        rt.insert(r, child: tabs, index: 0)
        frame(tabs, vc.view.bounds)
        let first = make(.screen), second = make(.screen)
        rt.insert(tabs, child: first, index: 0)
        rt.insert(tabs, child: second, index: 1)
        set(tabs, AsukaProp.Tabs.selected, .int(1))
        window.layoutIfNeeded()
        waitUntil { view(second).window != nil }
        let stack = make(.stack)
        rt.insert(second, child: stack, index: 0)
        XCTAssertTrue(rt.nodes[stack]!.vc!.parent === rt.nodes[second]!.vc)
        XCTAssertTrue(view(stack).isDescendant(of: view(second)))
    }

    // MARK: Controls

    /// A Picker draws its choice in its font, kept as its items and
    /// choice change; textAlign and lineLimit, which a button's title
    /// doesn't take, change nothing.
    func testPickerFont() {
        let r = root()
        let p = make(.picker)
        rt.insert(r, child: p, index: 0)
        set(p, AsukaProp.fontSize, .float(13))
        set(p, AsukaProp.fontWeight, .enumValue(AsukaFontWeight.semibold.rawValue))
        var w = AsukaWriter()
        w.u32(2); w.string("One"); w.string("Two")
        bytes(w.bytes) { set(p, AsukaProp.Picker.items, .bytes($0)) }
        set(p, AsukaProp.Picker.selected, .int(1))
        set(p, AsukaProp.textAlign, .enumValue(AsukaTextAlign.center.rawValue))
        set(p, AsukaProp.lineLimit, .int(1))
        frame(p, CGRect(x: 0, y: 0, width: 200, height: 44))
        let b = view(p) as! AsukaPickerButton
        b.layoutIfNeeded()
        XCTAssertEqual(b.titleLabel?.text, "Two")
        XCTAssertEqual(b.titleLabel?.font.pointSize, 13)
        XCTAssertEqual(b.titleLabel?.font.fontName, UIFont.systemFont(ofSize: 13, weight: .semibold).fontName)
    }

    // MARK: asuka dev's error overlay

    /// A DevError command's arguments, as ops.AppendDevError writes them.
    func devError(_ flags: UInt8, _ title: String, _ message: String, _ source: String = "", _ details: String = "") -> [UInt8] {
        var w = AsukaWriter()
        w.u8(flags)
        for s in [title, message, source, details] { w.string(s) }
        return w.bytes
    }

    /// The overlay is a window of its own over the app's, alerts' too; it
    /// shows the error's parts, offers Reload only when the core does,
    /// sends what's pressed to the root and stays until DevErrorClear.
    func testDevErrorOverlay() {
        let r = root()
        _ = sent()
        command(r, .devError, devError(1, "Uncaught error", "boom at 1", "> 12 |   throw Error(\"boom at {n}\")\n     |   ^^^^^", "explode (app.asuka:12)"))
        guard let o = rt.devError else { return XCTFail("no overlay") }
        XCTAssertFalse(o.window.isHidden)
        XCTAssertTrue(o.window !== window)
        XCTAssertGreaterThan(o.window.windowLevel, .alert)
        XCTAssertTrue(o.window.isKeyWindow)
        XCTAssertTrue(window.accessibilityElementsHidden) // VoiceOver reads the overlay only
        let c = o.controller
        XCTAssertEqual(c.texts, ["Uncaught error", "boom at 1", "> 12 |   throw Error(\"boom at {n}\")\n     |   ^^^^^", "explode (app.asuka:12)", "Dismiss", "Reload"])
        XCTAssertEqual(c.titleView.textColor, .systemRed)
        XCTAssertFalse(c.sourceBox.isHidden)
        XCTAssertFalse(c.reloadButton.isHidden)
        // The source's lines aren't wrapped: its box scrolls sideways.
        o.window.layoutIfNeeded()
        XCTAssertGreaterThanOrEqual(c.sourceBox.contentSize.width, c.sourceBox.textView.frame.maxX)
        XCTAssertTrue(c.press("Reload"))
        XCTAssertTrue(c.press("Dismiss"))
        let acts = sent(.devErrorAction)
        XCTAssertEqual(acts.map(\.id), [r, r])
        XCTAssertEqual(acts.map(\.int), [Int64(AsukaDevErrorAction.reload.rawValue), Int64(AsukaDevErrorAction.dismiss.rawValue)])
        // Pressed, it stays: the core says when it goes.
        XCTAssertFalse(o.window.isHidden)
        // Sent again: the same window shows the new error, a build's in
        // orange, without Reload, nor a source or details when empty.
        command(r, .devError, devError(2, "Build failed", "app.asuka:6:32: += doesn't take an int and a string"))
        XCTAssertTrue(rt.devError === o)
        XCTAssertEqual(c.titleView.textColor, .systemOrange)
        XCTAssertTrue(c.reloadButton.isHidden)
        XCTAssertTrue(c.sourceBox.isHidden)
        XCTAssertTrue(c.detailsView.isHidden)
        XCTAssertFalse(c.press("Reload"))
        XCTAssertEqual(c.texts, ["Build failed", "app.asuka:6:32: += doesn't take an int and a string", "Dismiss"])
        command(r, .devErrorClear)
        XCTAssertNil(rt.devError)
        XCTAssertTrue(o.window.isHidden)
        XCTAssertTrue(window.isKeyWindow)
        XCTAssertFalse(window.accessibilityElementsHidden)
        // Cut short: nothing shows.
        command(r, .devError, Array(devError(0, "Uncaught error", "x").dropLast(1)))
        XCTAssertNil(rt.devError)
        // Its root destroyed without a DevErrorClear: it goes with it.
        command(r, .devError, devError(0, "Uncaught error", "x"))
        XCTAssertNotNil(rt.devError)
        rt.destroy(r)
        XCTAssertNil(rt.devError)
    }
}

/// The Keys prop as ops.AppendKeys writes it.
enum AsukaKeysTestEncoding {
    static func encode(_ keys: [AsukaKey]) -> [UInt8] {
        var w = AsukaWriter()
        w.u32(UInt32(keys.count))
        for k in keys {
            w.string(k.name)
            w.u8(k.mods)
        }
        return w.bytes
    }
}
#endif
