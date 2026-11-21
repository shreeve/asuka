import AppKit
import XCTest
@testable import AsukaHost

// The app's accent in light and dark appearances: the Accent command's
// color and its dark one (asuka.toml's accent and accentDark), and the
// same two from the app's settings (asuka-app.json).

@MainActor
final class AccentTests: XCTestCase {
    private var rt: AsukaRuntime { AsukaRuntime.shared }

    override func setUp() async throws {
        await MainActor.run {
            _ = NSApplication.shared
            let rt = AsukaRuntime.shared
            if rt.main == nil { rt.makeMainWindow() }
            rt.started = true
        }
    }

    override func tearDown() async throws {
        await MainActor.run { AsukaRuntime.shared.setAccent(0) }
    }

    /// c's sRGB components as 0-255, drawn in the appearance named.
    private func rgba(_ c: NSColor, in name: NSAppearance.Name) -> [Int] {
        var out: [Int] = []
        NSAppearance(named: name)!.performAsCurrentDrawingAppearance {
            let s = c.usingColorSpace(.sRGB)!
            out = [s.redComponent, s.greenComponent, s.blueComponent, s.alphaComponent].map { Int(($0 * 255).rounded()) }
        }
        return out
    }

    /// ops.Accent's arguments: u64 Color, u8 flags, u64 Dark.
    private func args(_ color: UInt64, _ dark: UInt64) -> [UInt8] {
        var b: [UInt8] = []
        for i in 0..<8 { b.append(UInt8(truncatingIfNeeded: color >> (8 * i))) }
        b.append(0)
        for i in 0..<8 { b.append(UInt8(truncatingIfNeeded: dark >> (8 * i))) }
        return b
    }

    /// With a dark accent, the accent (and .accent) is the light one in
    /// Aqua and the dark one in Dark Aqua; without one, the light one in
    /// both.
    func testDarkAccent() {
        let root: UInt32 = 910
        rt.create(root, kind: AsukaKind.view.rawValue)
        defer { rt.destroy(root) }
        let blue: UInt64 = 2 << 32 | 0x2563ebff, sky: UInt64 = 2 << 32 | 0x93c5fdff
        args(blue, sky).withUnsafeBytes { rt.command(root, command: AsukaCommand.accent.rawValue, args: $0) }
        let semantic = asukaColor(1 << 32 | UInt64(AsukaSemanticColor.accent.rawValue))!
        for c in [accentNSColor, semantic] {
            XCTAssertEqual(rgba(c, in: .aqua), [0x25, 0x63, 0xeb, 255])
            XCTAssertEqual(rgba(c, in: .darkAqua), [0x93, 0xc5, 0xfd, 255])
        }
        args(blue, 0).withUnsafeBytes { rt.command(root, command: AsukaCommand.accent.rawValue, args: $0) }
        XCTAssertEqual(rgba(accentNSColor, in: .aqua), [0x25, 0x63, 0xeb, 255])
        XCTAssertEqual(rgba(accentNSColor, in: .darkAqua), [0x25, 0x63, 0xeb, 255])
    }

    /// asuka-app.json's accentDark, as the host reads it before the app's
    /// init sends the Accent command.
    func testConfig() throws {
        let json = ##"{"accent": "#2563eb", "accentDark": "#93c5fd"}"##
        let c = try JSONDecoder().decode(AsukaConfig.self, from: Data(json.utf8))
        XCTAssertEqual(c.accentColor, 2 << 32 | 0x2563ebff)
        XCTAssertEqual(c.accentDarkColor, 2 << 32 | 0x93c5fdff)
        XCTAssertNil(try JSONDecoder().decode(AsukaConfig.self, from: Data(##"{"accent": "#2563eb"}"##.utf8)).accentDarkColor)
    }
}
