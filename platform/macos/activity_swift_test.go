//go:build darwin && !ios

package macos

import (
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

// platform/activity's Swift (sdk/activity/swift/Activity.swift) on this
// Mac, compiled with its generated protocol and a driver of the test's
// that sends events through NSApplication as the window server's come:
// a key, a mouse's press, a scroll and a text view's change are input, a
// pointer moving isn't; watch hears inputs at most once a second, until
// its task is cancelled.
func TestActivitySwift(t *testing.T) {
	if _, err := exec.LookPath("swiftc"); err != nil {
		t.Skip("no swiftc")
	}
	glue, err := os.ReadFile("../../sdk/activity/swift/Activity.asuka.swift")
	if err != nil {
		t.Fatal(err)
	}
	proto := regexp.MustCompile(`(?s)@MainActor public protocol Watcher[:\s][^{]*\{.*?\n\}\n`).Find(glue)
	if proto == nil {
		t.Fatal("no Watcher in the glue")
	}
	driver := `import AppKit

let app = NSApplication.shared

@MainActor func pump(_ seconds: Double = 0.05) {
    let end = Date(timeIntervalSinceNow: seconds)
    while let e = app.nextEvent(matching: .any, until: end, inMode: .default, dequeue: true) { app.sendEvent(e) }
}

@MainActor func send(_ e: NSEvent?) {
    app.postEvent(e!, atStart: false)
    pump()
}

func key() -> NSEvent? {
    NSEvent.keyEvent(with: .keyDown, location: .zero, modifierFlags: [], timestamp: 0, windowNumber: 0, context: nil,
                     characters: "a", charactersIgnoringModifiers: "a", isARepeat: false, keyCode: 0)
}

func mouse(_ type: NSEvent.EventType) -> NSEvent? {
    NSEvent.mouseEvent(with: type, location: .zero, modifierFlags: [], timestamp: 0, windowNumber: 0, context: nil,
                       eventNumber: 0, clickCount: 1, pressure: 1)
}

func scroll() -> NSEvent? {
    NSEvent(cgEvent: CGEvent(scrollWheelEvent2Source: nil, units: .pixel, wheelCount: 1, wheel1: -5, wheel2: 0, wheel3: 0)!)
}

MainActor.assumeIsolated {
    let w = try! makeWatcher()
    @MainActor func fresh() -> Bool { try! w.idle() < 0.1 }
    Thread.sleep(forTimeInterval: 0.3)
    print("idle at start \(try! w.idle() >= 0.3)")
    send(mouse(.mouseMoved))
    print("moved \(fresh())")
    send(key())
    print("key \(fresh())")
    Thread.sleep(forTimeInterval: 0.2)
    send(mouse(.leftMouseDown))
    print("press \(fresh())")
    Thread.sleep(forTimeInterval: 0.2)
    send(mouse(.rightMouseDown))
    print("right press \(fresh())")
    Thread.sleep(forTimeInterval: 0.2)
    send(scroll())
    print("scroll \(fresh())")
    Thread.sleep(forTimeInterval: 0.2)
    NotificationCenter.default.post(name: NSText.didChangeNotification, object: nil)
    print("edit \(fresh())")

    var heard = 0
    let watching = Task { @MainActor in try await w.watch(input: { heard += 1 }) }
    pump()
    send(key())
    send(key())
    print("heard once \(heard)")
    Thread.sleep(forTimeInterval: 1.05)
    send(mouse(.leftMouseDown))
    print("heard again \(heard)")
    watching.cancel()
    pump()
    Thread.sleep(forTimeInterval: 1.05)
    send(key())
    print("heard after cancel \(heard)")
}
`
	dir := t.TempDir()
	stub := filepath.Join(dir, "stub")
	os.MkdirAll(stub, 0o755)
	asuka := filepath.Join(stub, "Asuka.swift")
	os.WriteFile(asuka, []byte("public enum AsukaNativeError: Error { case unsupported(String) }\n"), 0o644)
	if b, err := exec.Command("swiftc", "-parse-as-library", "-emit-module", "-module-name", "Asuka", "-emit-module-path", filepath.Join(stub, "Asuka.swiftmodule"), "-emit-object", "-o", filepath.Join(stub, "Asuka.o"), asuka).CombinedOutput(); err != nil {
		t.Fatalf("swiftc (Asuka): %v\n%s", err, b)
	}
	shim := filepath.Join(dir, "shim.swift")
	main := filepath.Join(dir, "main.swift")
	os.WriteFile(shim, append([]byte("import Foundation\n"), proto...), 0o644)
	os.WriteFile(main, []byte(driver), 0o644)
	impl, _ := filepath.Abs("../../sdk/activity/swift/Activity.swift")
	exe := filepath.Join(dir, "probe")
	if b, err := exec.Command("swiftc", "-swift-version", "5", "-I", stub, shim, main, impl, filepath.Join(stub, "Asuka.o"), "-o", exe).CombinedOutput(); err != nil {
		t.Fatalf("swiftc: %v\n%s", err, b)
	}
	b, err := exec.Command(exe).CombinedOutput()
	got := string(b)
	if err != nil {
		t.Fatalf("%v\n%s", err, got)
	}
	for _, want := range []string{
		"idle at start true", "moved false", "key true", "press true", "right press true", "scroll true", "edit true",
		"heard once 1", "heard again 2", "heard after cancel 2",
	} {
		if !strings.Contains(got, want+"\n") {
			t.Errorf("want %q in:\n%s", want, got)
		}
	}
}
