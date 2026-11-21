import AppKit
import XCTest
@testable import AsukaHost

// Interaction states the automation can't reach: under it, the steps stand
// in for the pointer and the app's activation (platform/macos/mactest).

@MainActor
final class InteractionTests: XCTestCase {
    private var rt: AsukaRuntime { AsukaRuntime.shared }

    override func setUp() async throws {
        await MainActor.run {
            _ = NSApplication.shared
            let rt = AsukaRuntime.shared
            if rt.main == nil { rt.makeMainWindow() }
            rt.started = true
        }
    }

    // A tracking area in the active app gets no exited while another app is
    // active, so the host follows the activation itself, as launch sets it
    // up: leaving the app, a hovered view isn't; back, the view under the
    // pointer is.
    func testHoverFollowsTheAppsActivation() throws {
        rt.observe() // what launch observes
        let id: UInt32 = 5001
        rt.create(id, kind: AsukaKind.view.rawValue)
        rt.listen(id, event: AsukaEventKind.hover.rawValue)
        let n = try XCTUnwrap(rt.nodes[id])
        let v = try XCTUnwrap(n.view)
        v.frame = CGRect(x: 0, y: 0, width: 50, height: 50)
        rt.main.window.contentView?.addSubview(v)
        defer { v.removeFromSuperview() }
        rt.mouseEntered(v)
        XCTAssertEqual(n.interaction?.hovered, true)

        NotificationCenter.default.post(name: NSApplication.didResignActiveNotification, object: NSApp)
        XCTAssertEqual(n.interaction?.hovered, false, "still hovered with another app active")

        let w = try XCTUnwrap(rt.main.window)
        let p = v.convert(CGPoint(x: 25, y: 25), to: nil)
        rt.visual.hoverProbe = { $0 === w ? p : nil }
        defer { rt.visual.hoverProbe = nil }
        NotificationCenter.default.post(name: NSApplication.didBecomeActiveNotification, object: NSApp)
        XCTAssertEqual(n.interaction?.hovered, true, "not hovered under the pointer once the app is back")
    }
}
