import AppKit
import XCTest
@testable import AsukaHost

// Rich text's typesetting on AppKit, driven as the core drives it (ops):
// what it costs is TextKit's, which a test reads from its text container.

@MainActor
final class RichTextTests: XCTestCase {
    override func setUp() async throws {
        await MainActor.run {
            _ = NSApplication.shared
            let rt = AsukaRuntime.shared
            if rt.main == nil { rt.makeMainWindow() }
        }
    }

    /// ops.AppendRich's encoding of one paragraph in one plain style.
    private func plain(_ text: String) -> [UInt8] {
        var w = AsukaByteWriter()
        w.u32(1)
        w.u8(0)
        w.f32(0)
        w.u8(0)
        w.u8(0)
        w.u64(0)
        w.u64(0)
        w.string("")
        w.u32(1)
        w.u32(1)
        w.u32(0)
        w.string(text)
        return w.bytes
    }

    // Measured before it's placed, a text is typeset at the width it's
    // measured at, and not again at the width it shows at, which it has
    // none of yet (a column a point wide costs as much as the text is
    // long); placed, it's typeset at its frame's width.
    func testMeasuredBeforePlaced() throws {
        let rt = AsukaRuntime.shared
        let id: UInt32 = 6001
        rt.create(id, kind: AsukaKind.richText.rawValue)
        plain(String(repeating: "word ", count: 400)).withUnsafeBytes {
            rt.setProp(id, prop: AsukaProp.RichText.content, value: .bytes($0))
        }
        let n = try XCTUnwrap(rt.nodes[id])
        let r = try XCTUnwrap(n.rich)
        let s = rt.measure(id, .nan, .nan, 300, .infinity)
        XCTAssertLessThanOrEqual(s.width, 300)
        XCTAssertGreaterThan(s.height, 100)
        XCTAssertEqual(r.view.textContainer?.containerSize.width, 300, "typeset again at a width it doesn't have")
        n.frame = CGRect(x: 0, y: 0, width: 500, height: s.height)
        rt.placeInlines(n)
        XCTAssertEqual(r.view.textContainer?.containerSize.width, 500)
    }
}
