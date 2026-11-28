// platform/activity on iOS and the Mac. iOS: a gesture recognizer on
// each of the app's windows that sees a touch begin (a finger, a pencil,
// a trackpad's or mouse's click) or a key's press and fails at once, so
// it never takes, delays nor cancels them; a pan recognizer that takes no
// touches, for a trackpad's or mouse's scrolling; and the text fields'
// and text views' changes, the user's own (a software keyboard's,
// dictation's). The Mac: a local event monitor, which sees the app's
// events as they're sent and passes each on unchanged (a mouse's buttons,
// keys and modifiers, scrolling, a trackpad's gestures), and the text
// views' changes (the user's edits that come from no key: dictation, the
// character viewer). Times are CLOCK_MONOTONIC's, which goes on while the
// device sleeps.
import Foundation
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// The app's input, watched once for the process: every Watcher (asuka
/// dev's reloads make new ones) reads the same.
@MainActor final class InputWatch: NSObject {
    static let shared = InputWatch()

    /// When the last input was (nanoseconds, CLOCK_MONOTONIC).
    private var last = InputWatch.now()
    /// When the listeners last heard of one.
    private var told: UInt64 = 0
    private var listeners: [Int: () -> Void] = [:]
    private var nextID = 0
    #if os(macOS)
    private var monitor: Any?
    #endif

    nonisolated static func now() -> UInt64 { clock_gettime_nsec_np(CLOCK_MONOTONIC) }

    private override init() {
        super.init()
        let nc = NotificationCenter.default
        #if canImport(UIKit)
        nc.addObserver(self, selector: #selector(shown(_:)), name: UIWindow.didBecomeVisibleNotification, object: nil)
        nc.addObserver(self, selector: #selector(edited), name: UITextField.textDidChangeNotification, object: nil)
        nc.addObserver(self, selector: #selector(edited), name: UITextView.textDidChangeNotification, object: nil)
        for s in UIApplication.shared.connectedScenes {
            for w in (s as? UIWindowScene)?.windows ?? [] { observe(w) }
        }
        #else
        nc.addObserver(self, selector: #selector(edited), name: NSText.didChangeNotification, object: nil)
        let kinds: NSEvent.EventTypeMask = [.leftMouseDown, .rightMouseDown, .otherMouseDown, .keyDown, .flagsChanged,
                                            .scrollWheel, .magnify, .rotate, .swipe, .smartMagnify]
        monitor = NSEvent.addLocalMonitorForEvents(matching: kinds) { e in
            MainActor.assumeIsolated { InputWatch.shared.input() }
            return e
        }
        #endif
    }

    /// The user gave input: now.
    func input() {
        let t = Self.now()
        last = t
        if listeners.isEmpty || t &- told < 1_000_000_000 { return }
        told = t
        for l in Array(listeners.values) { l() }
    }

    func idle() -> TimeInterval { Double(Self.now() &- last) / 1e9 }

    func listen(_ f: @escaping () -> Void) -> Int {
        nextID += 1
        listeners[nextID] = f
        return nextID
    }

    func unlisten(_ id: Int) { listeners[id] = nil }

    @objc func edited() { input() }

    #if canImport(UIKit)
    @objc func shown(_ n: Notification) {
        if let w = n.object as? UIWindow { observe(w) }
    }

    /// Watches a window's touches, presses and scrolls (once per window).
    func observe(_ w: UIWindow) {
        if w.gestureRecognizers?.contains(where: { $0 is ActivityTouches }) == true { return }
        w.addGestureRecognizer(ActivityTouches())
        w.addGestureRecognizer(ActivityScrolls(watch: self))
    }

    @objc func scrolled(_ g: UIPanGestureRecognizer) {
        if g.state == .began { input() }
    }
    #endif
}

#if canImport(UIKit)
/// Sees each touch and press begin in its window, and fails: it never
/// recognizes, so the window's views get every touch as before.
final class ActivityTouches: UIGestureRecognizer, UIGestureRecognizerDelegate {
    init() {
        super.init(target: nil, action: nil)
        cancelsTouchesInView = false
        delaysTouchesBegan = false
        delaysTouchesEnded = false
        delegate = self
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent) { began() }
    override func pressesBegan(_ presses: Set<UIPress>, with event: UIPressesEvent) { began() }

    private func began() {
        MainActor.assumeIsolated { InputWatch.shared.input() }
        state = .failed
    }

    func gestureRecognizer(_ g: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith other: UIGestureRecognizer) -> Bool { true }
}

/// A pan over a trackpad's or mouse's scrolling only (no touches), which
/// recognizes alongside the scroll views' own. It's given the watch it
/// reports to: made while InputWatch.shared is being made (its init
/// observes the windows already shown), it can't reach for shared, whose
/// one-time initialization would wait on itself and trap.
final class ActivityScrolls: UIPanGestureRecognizer, UIGestureRecognizerDelegate {
    init(watch: InputWatch) {
        super.init(target: watch, action: #selector(InputWatch.scrolled(_:)))
        allowedScrollTypesMask = .all
        allowedTouchTypes = []
        cancelsTouchesInView = false
        delaysTouchesBegan = false
        delaysTouchesEnded = false
        delegate = self
    }

    func gestureRecognizer(_ g: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith other: UIGestureRecognizer) -> Bool { true }
}
#endif

@MainActor final class AppWatcher: Watcher {
    init() { _ = InputWatch.shared }

    func idle() throws -> TimeInterval { InputWatch.shared.idle() }

    func watch(input: @escaping () -> Void) async throws {
        let id = InputWatch.shared.listen(input)
        defer { InputWatch.shared.unlisten(id) }
        while !Task.isCancelled {
            try? await Task.sleep(nanoseconds: 3_600_000_000_000)
        }
    }
}

@MainActor func makeWatcher() throws -> any Watcher { AppWatcher() }
