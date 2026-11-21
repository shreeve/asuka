import AppKit
import XCTest
@testable import AsukaHost

// Navigation's alerts and snackbars, driven as the core drives them (ops)
// on AppKit, the events the host sends kept: the language this branch
// compiles has no alert() or snackbar() yet, so no Asuka app reaches them
// through the player (platform/macos/mactest).

/// The events the host sent the core (the stand-in's asuka_event, which
/// this one overrides).
nonisolated(unsafe) var navSentEvents: [[UInt8]] = []

@_cdecl("asuka_event")
func navTestEvent(_ buf: UnsafePointer<UInt8>?, _ len: Int) {
    guard let buf else { return }
    navSentEvents.append(Array(UnsafeBufferPointer(start: buf, count: len)))
}

/// Builds wire values as ops/*.go append them.
private struct Wire {
    var bytes: [UInt8] = []
    mutating func u8(_ v: UInt8) { bytes.append(v) }
    mutating func u32(_ v: Int) { for i in 0..<4 { bytes.append(UInt8(truncatingIfNeeded: v >> (8 * i))) } }
    mutating func string(_ s: String) {
        let b = Array(s.utf8)
        u32(b.count)
        bytes += b
    }

    /// ops.AppendActions: u32 count, per item u8 role, title, icon, u8
    /// flags (no submenus here).
    static func actions(_ items: [(role: AsukaAlertButtonRole, title: String, disabled: Bool)]) -> [UInt8] {
        var w = Wire()
        w.u32(items.count)
        for it in items {
            w.u8(UInt8(it.role.rawValue))
            w.string(it.title)
            w.string("")
            w.u8(it.disabled ? AsukaActionItem.disabledFlag : 0)
        }
        return w.bytes
    }

    /// ops.AppendAlertFields.
    static func fields(_ fs: [AsukaAlertField]) -> [UInt8] {
        var w = Wire()
        w.u32(fs.count)
        for f in fs {
            w.string(f.placeholder)
            w.string(f.text)
            w.u8(f.flags)
            w.u8(UInt8(f.keyboard))
            w.string(f.contentType)
        }
        return w.bytes
    }
}

@MainActor
final class NavTests: XCTestCase {
    private var rt: AsukaRuntime { AsukaRuntime.shared }
    private var next: UInt32 = 1000

    override func setUp() async throws {
        await MainActor.run {
            _ = NSApplication.shared
            let rt = AsukaRuntime.shared
            if rt.main == nil { rt.makeMainWindow() }
            rt.started = true
            rt.main.window.orderFront(nil)
            navSentEvents = []
        }
    }

    private func make(_ kind: AsukaKind) -> UInt32 {
        next += 1
        rt.create(next, kind: kind.rawValue)
        return next
    }

    private func set(_ id: UInt32, _ p: UInt16, _ v: AsukaValue) { rt.setProp(id, prop: p, value: v) }

    /// Sets a bytes prop (the value borrows them, as the decoder's does).
    private func set(_ id: UInt32, _ p: UInt16, _ b: [UInt8]) { b.withUnsafeBytes { set(id, p, .bytes($0)) } }

    private func command(_ id: UInt32, _ c: AsukaCommand) {
        let none: [UInt8] = []
        none.withUnsafeBytes { rt.command(id, command: c.rawValue, args: $0) }
    }

    private func listen(_ id: UInt32, _ e: AsukaEventKind...) { for k in e { rt.listen(id, event: k.rawValue) } }

    /// Turns the run loop until cond holds (a second at most).
    private func spin(_ cond: () -> Bool) -> Bool {
        for _ in 0..<100 {
            if cond() { return true }
            RunLoop.main.run(until: Date().addingTimeInterval(0.01))
        }
        return cond()
    }

    // An alert's fields are its accessory view: an NSTextField and an
    // NSSecureTextField with their starting text, the first being edited
    // as it shows. Typing reports FieldChanged, and the default button
    // follows its Disabled. Return goes on to the next field, and in the
    // last presses the default button once it's enabled, and does nothing
    // before (TestAlertFieldsDarwin).
    func testAlertFields() throws {
        let id = make(.alert)
        listen(id, .fieldChanged, .action, .dismissed)
        set(id, AsukaProp.Alert.title, .string("New folder"))
        set(id, AsukaProp.Alert.style, .enumValue(AsukaAlertStyle.alert.rawValue))
        set(id, AsukaProp.Alert.fields, Wire.fields([
            AsukaAlertField(placeholder: "Name", text: "Notes"),
            AsukaAlertField(placeholder: "Password", flags: AsukaAlertField.secureFlag, contentType: "password"),
        ]))
        func buttons(createDisabled: Bool) -> [UInt8] {
            Wire.actions([(.destructive, "Delete", false), (.cancel, "Cancel", false), (.default, "Create", createDisabled)])
        }
        set(id, AsukaProp.Alert.buttons, buttons(createDisabled: true))
        command(id, .present)
        let n = try XCTUnwrap(rt.nodes[id])
        let nav = try XCTUnwrap(n.nav)
        let a = try XCTUnwrap(nav.alert, "no alert")
        let w = a.window
        let inputs = nav.inputs
        XCTAssertEqual(inputs.count, 2)
        XCTAssertNotNil(a.accessoryView)
        XCTAssertFalse(inputs[0] is NSSecureTextField)
        XCTAssertTrue(inputs[1] is NSSecureTextField, "the password isn't the secure field")
        XCTAssertEqual(inputs[0].stringValue, "Notes")
        XCTAssertEqual(inputs[1].placeholderString, "Password")
        XCTAssertEqual(rt.editingAlertField(n), inputs[0], "the first field isn't being edited")
        // The buttons: the default first (rightmost), cancel last; return
        // is Create's, not the first's.
        XCTAssertEqual(a.buttons.map(\.title), ["Delete", "Create", "Cancel"])
        XCTAssertEqual(a.buttons[1].keyEquivalent, "\r")
        XCTAssertNotEqual(a.buttons[0].keyEquivalent, "\r")
        XCTAssertEqual(a.buttons[2].keyEquivalent, "\u{1b}")
        XCTAssertTrue(a.buttons[0].hasDestructiveAction)
        XCTAssertFalse(a.buttons[1].isEnabled, "Create enabled with no password")

        // Typing in the first field reports it; return goes on to the next.
        inputs[0].stringValue = "a"
        XCTAssertTrue(rt.alertFieldChanged(inputs[0]))
        XCTAssertEqual(navSentEvents.last, AsukaEvents.fieldChanged(id, AsukaAlertFields.changed(0, "a")))
        XCTAssertEqual(rt.alertFieldCommand(inputs[0], #selector(NSResponder.insertNewline(_:))), true)
        XCTAssertEqual(rt.editingAlertField(n), inputs[1], "return didn't go on to the password")
        // Return in the last while Create is disabled: nothing.
        navSentEvents = []
        XCTAssertEqual(rt.alertFieldCommand(inputs[1], #selector(NSResponder.insertNewline(_:))), true)
        _ = spin { false }
        XCTAssertTrue(navSentEvents.isEmpty, "return with Create disabled did something")
        XCTAssertNotNil(nav.alert)
        inputs[1].stringValue = "a"
        _ = rt.alertFieldChanged(inputs[1])
        XCTAssertEqual(navSentEvents.last, AsukaEvents.fieldChanged(id, AsukaAlertFields.changed(1, "a")))
        // The core enables Create.
        set(id, AsukaProp.Alert.buttons, buttons(createDisabled: false))
        XCTAssertTrue(a.buttons[1].isEnabled, "Create didn't follow")
        navSentEvents = []
        XCTAssertEqual(rt.alertFieldCommand(inputs[1], #selector(NSResponder.insertNewline(_:))), true)
        XCTAssertTrue(spin { navSentEvents.contains(AsukaEvents.action(id, 2)) }, "Create wasn't reported: \(navSentEvents)")
        XCTAssertNil(nav.alert)
        XCTAssertTrue(rt.navTarget.alertFields.isEmpty, "\(rt.navTarget.alertFields.count) fields left")
        XCTAssertFalse(w.isVisible)
        rt.destroy(id)
    }

    // An action sheet's style has no fields; Escape (its cancel button)
    // reports the cancel button's number.
    func testAlertCancel() throws {
        let id = make(.alert)
        listen(id, .action, .dismissed)
        set(id, AsukaProp.Alert.title, .string("Delete?"))
        set(id, AsukaProp.Alert.message, .string("You can't undo this."))
        set(id, AsukaProp.Alert.buttons, Wire.actions([(.cancel, "Cancel", false), (.destructive, "Delete", false)]))
        command(id, .present)
        let a = try XCTUnwrap(rt.nodes[id]?.nav?.alert)
        XCTAssertEqual(a.messageText, "Delete?")
        XCTAssertEqual(a.informativeText, "You can't undo this.")
        XCTAssertEqual(a.buttons.map(\.title), ["Delete", "Cancel"])
        XCTAssertTrue(rt.tapAlertButton("Cancel"))
        XCTAssertTrue(spin { navSentEvents.contains(AsukaEvents.action(id, 0)) }, "Cancel wasn't reported: \(navSentEvents)")
        rt.destroy(id)
    }

    /// Shows a snackbar with an Undo action, its events listened for.
    private func showSnack() throws -> AsukaNode {
        let id = make(.snackbar)
        listen(id, .action, .dismissed)
        set(id, AsukaProp.Snackbar.message, .string("Note deleted"))
        set(id, AsukaProp.Snackbar.action, .string("Undo"))
        command(id, .present)
        let n = try XCTUnwrap(rt.nodes[id])
        XCTAssertNotNil(n.snack?.box)
        return n
    }

    // A snackbar is an overlay at the bottom of the window: its message,
    // its action's button and a close button, in the system's material.
    // Its button reports the action; closing it, or its time running out,
    // dismisses it; its time stops under the mouse (TestSnackbar).
    func testSnackbar() throws {
        var n = try showSnack()
        var s = try XCTUnwrap(n.snack)
        let box = try XCTUnwrap(s.box)
        XCTAssertTrue(box.superview === rt.main.content, "not on the window's content")
        XCTAssertEqual(s.label?.stringValue, "Note deleted")
        XCTAssertEqual(s.button?.title, "Undo")
        XCTAssertNotNil(s.effect)
        // Its message is measured in the system's font, on one line.
        let lf = try XCTUnwrap(s.label).frame
        XCTAssertTrue(lf.width >= 60 && lf.height >= 12 && lf.height <= 24, "the message's frame \(lf)")
        // At the bottom, centered, inside the window.
        let cb = rt.main.content.bounds
        let f = box.frame
        XCTAssertTrue(f.maxY <= cb.height - 20 + 0.5 && f.minY >= cb.height / 2, "frame \(f) in \(cb)")
        XCTAssertEqual(f.midX, cb.width / 2, accuracy: 1)
        // Its action's title follows the core while it shows.
        set(n.id, AsukaProp.Snackbar.action, .string("Rückgängig"))
        XCTAssertEqual(s.button?.title, "Rückgängig")
        // Its time stops under the mouse, and goes on after.
        XCTAssertNotNil(s.timer)
        rt.snackHover(box, true)
        XCTAssertNil(s.timer, "the timer runs under the mouse")
        rt.snackHover(box, false)
        XCTAssertNotNil(s.timer, "the timer didn't resume")

        // Its button: the action, and it goes.
        navSentEvents = []
        rt.snackPressed(n, s.button)
        XCTAssertEqual(navSentEvents, [AsukaEvents.action(n.id, 0)])
        XCTAssertTrue(s.done)
        XCTAssertNil(s.box)
        XCTAssertNil(s.timer)
        rt.destroy(n.id)

        // The close button dismisses it.
        n = try showSnack()
        s = try XCTUnwrap(n.snack)
        navSentEvents = []
        rt.snackPressed(n, s.close)
        XCTAssertEqual(navSentEvents, [AsukaEvents.dismissed(n.id)])
        XCTAssertNil(rt.node(of: s.close!), "the closed one's buttons still route")
        rt.destroy(n.id)

        // Its time running out dismisses it, once.
        n = try showSnack()
        navSentEvents = []
        rt.snackReport(n, .dismissed)
        rt.snackReport(n, .dismissed)
        XCTAssertEqual(navSentEvents, [AsukaEvents.dismissed(n.id)])
        rt.destroy(n.id)

        // Destroyed by the core while it shows (a new one replaced it):
        // it goes out, its timer stopped, reporting nothing.
        n = try showSnack()
        s = try XCTUnwrap(n.snack)
        navSentEvents = []
        rt.destroy(n.id)
        XCTAssertTrue(s.done)
        XCTAssertNil(s.timer)
        XCTAssertNil(s.box)
        XCTAssertTrue(navSentEvents.isEmpty)
    }

    // A movable sheet's blocker over the window it's over: a press there
    // dismisses the sheet as Escape does (its cancelOperation) when the
    // sheet's DismissOutside said so, and otherwise only brings it forward.
    @MainActor func testModalBlockerDismissOutside() throws {
        final class Sheet: NSWindow {
            var cancelled = 0
            override func cancelOperation(_ sender: Any?) { cancelled += 1 }
        }
        let sheet = Sheet(contentRect: CGRect(x: 0, y: 0, width: 200, height: 100), styleMask: [.titled], backing: .buffered, defer: true)
        let b = AsukaModalBlocker(frame: CGRect(x: 0, y: 0, width: 400, height: 300))
        b.sheet = sheet
        let press = try XCTUnwrap(NSEvent.mouseEvent(with: .leftMouseDown, location: CGPoint(x: 10, y: 10), modifierFlags: [], timestamp: 0,
                                                     windowNumber: 0, context: nil, eventNumber: 0, clickCount: 1, pressure: 1))
        b.mouseDown(with: press)
        XCTAssertEqual(sheet.cancelled, 0, "a press dismissed a sheet without DismissOutside")
        b.dismisses = true
        b.mouseDown(with: press)
        XCTAssertEqual(sheet.cancelled, 1, "a press outside didn't dismiss a sheet with DismissOutside")
    }

    // Placing a flyout (the desktop hosts' cases): below its anchor and
    // centered, flipped above where there's no room below, lined up with
    // its start, slid and shrunk into the work area.
    func testFlyoutPlacement() {
        let work = AsukaFlyoutRect(x: 0, y: 25, w: 1440, h: 875)
        let icon = AsukaFlyoutRect(x: 1000, y: 0, w: 24, h: 24)
        XCTAssertEqual(AsukaFlyoutPlacement.place(icon, 300, 200, AsukaPlacement.bottom.rawValue, 0, work),
                       AsukaFlyoutRect(x: 862, y: 25, w: 300, h: 200))
        // Near the right edge: slid back in.
        let edge = AsukaFlyoutRect(x: 1420, y: 0, w: 20, h: 24)
        XCTAssertEqual(AsukaFlyoutPlacement.place(edge, 300, 200, AsukaPlacement.bottom.rawValue, 0, work).x, 1140)
        // A taskbar icon at the bottom: flipped above it.
        let low = AsukaFlyoutRect(x: 700, y: 880, w: 24, h: 20)
        XCTAssertEqual(AsukaFlyoutPlacement.place(low, 300, 200, AsukaPlacement.bottom.rawValue, 4, work),
                       AsukaFlyoutRect(x: 562, y: 676, w: 300, h: 200))
        // Start: lined up with the anchor's start, gap away.
        let rect = AsukaFlyoutRect(x: 210, y: 254, w: 100, h: 30)
        XCTAssertEqual(AsukaFlyoutPlacement.place(rect, 180, 90, AsukaPlacement.bottomStart.rawValue, 4, work),
                       AsukaFlyoutRect(x: 210, y: 288, w: 180, h: 90))
        // Too tall: shrunk to the work area.
        XCTAssertEqual(AsukaFlyoutPlacement.place(icon, 300, 2000, AsukaPlacement.bottom.rawValue, 0, work).h, 875)
        // Centered a third of the way down.
        XCTAssertEqual(AsukaFlyoutPlacement.center(600, 300, work), AsukaFlyoutRect(x: 420, y: 25 + 575.0 / 3, w: 600, h: 300))
    }
}
