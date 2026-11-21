// The app's windows: the main one, holding the root (its content a flipped
// view), and a Window's; what each reports as its content's environment.
// Their options, state and kinds (panels, flyouts) are Windows.swift's,
// their toolbars Navigation.swift's.
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

/// A window's content size, in points, when the app gives none.
let asukaDefaultWindowSize = CGSize(width: 800, height: 600)

/// A WindowConfig (ops/window.go): a window's options.
struct AsukaWindowConfig: Equatable {
    var width: Float = 0, height: Float = 0 // 0: the host's (800 by 600)
    var minWidth: Float = 0, minHeight: Float = 0 // 0: the host's least
    var maxWidth: Float = 0, maxHeight: Float = 0 // 0: no most
    var x: Float = 0, y: Float = 0 // with .positioned
    var flags: Flags = []
    var titleBar: AsukaTitleBar = .default
    var background: UInt64 = 0 // 0: the platform's window background
    var parent: UInt32 = 0 // a Window's: the Window node or root it belongs to
    /// Where the host remembers the window's place across launches;
    /// empty: not remembered. The main window's is "main" when the app
    /// gives none.
    var stateKey = ""
    var title = "" // the main window's (a Window's is its Title prop)

    struct Flags: OptionSet, Equatable {
        let rawValue: UInt32
        static let fixedSize = Flags(rawValue: 1 << 0)
        static let noMinimize = Flags(rawValue: 1 << 1)
        static let noMaximize = Flags(rawValue: 1 << 2)
        static let noClose = Flags(rawValue: 1 << 3)
        static let alwaysOnTop = Flags(rawValue: 1 << 4)
        static let hidden = Flags(rawValue: 1 << 5)
        static let skipTaskbar = Flags(rawValue: 1 << 6)
        static let centered = Flags(rawValue: 1 << 7)
        static let positioned = Flags(rawValue: 1 << 8)
        static let transparent = Flags(rawValue: 1 << 9)
        static let maximized = Flags(rawValue: 1 << 10)
        static let fullscreen = Flags(rawValue: 1 << 11)
        static let child = Flags(rawValue: 1 << 12)
        static let modal = Flags(rawValue: 1 << 13)
        static let panel = Flags(rawValue: 1 << 14)
        static let flyout = Flags(rawValue: 1 << 15)
        static let fitContent = Flags(rawValue: 1 << 16)
        static let clickThrough = Flags(rawValue: 1 << 17)
        static let menuBar = Flags(rawValue: 1 << 18)
        /// The root's: the app has no main window, which never shows.
        static let none = Flags(rawValue: 1 << 19)
    }

    /// ops.DecodeWindowConfig: f32 width, height, min width, min height,
    /// max width, max height, x, y; u32 flags; u8 title bar; u64
    /// background; u32 parent; a u32-length state key; the rest the
    /// title. Short (an older form): the first four, then the title.
    init(_ b: [UInt8]) {
        if b.count < 32 {
            guard b.count >= 16 else { return }
            width = b.f32(0); height = b.f32(4); minWidth = b.f32(8); minHeight = b.f32(12)
            title = String(decoding: b[16...], as: UTF8.self)
            return
        }
        width = b.f32(0); height = b.f32(4); minWidth = b.f32(8); minHeight = b.f32(12)
        maxWidth = b.f32(16); maxHeight = b.f32(20); x = b.f32(24); y = b.f32(28)
        guard b.count >= 32 + 4 + 1 + 8 + 4 + 4 else { return }
        flags = Flags(rawValue: b.u32(32))
        titleBar = AsukaTitleBar(rawValue: Int32(b[36])) ?? .default
        background = b.u64(37)
        parent = b.u32(45)
        let k = Int(b.u32(49))
        guard 53 + k <= b.count else { return }
        stateKey = String(decoding: b[53..<53 + k], as: UTF8.self)
        title = String(decoding: b[(53 + k)...], as: UTF8.self)
    }

    init() {}
}

/// One of the app's windows: the main one, or a Window's (and a panel's or
/// a flyout's). It's its NSWindow's delegate.
@MainActor final class AsukaWin: NSObject, NSWindowDelegate {
    let window: NSWindow
    let content: AsukaContentView
    /// What it shows: the root, or the Window node.
    var root: AsukaNode?
    /// Its title when no screen names it.
    var base: String
    /// Its options, as last applied.
    var cfg = AsukaWindowConfig()
    /// Its options are applied.
    var configured = false
    /// Its title bar's and toolbar's (Navigation.swift).
    var chrome = AsukaChrome()
    /// Its state, kind and place (Windows.swift).
    var shell = AsukaWinShell()

    init(window: NSWindow, content: AsukaContentView, base: String) {
        self.window = window
        self.content = content
        self.base = base
        super.init()
        window.delegate = self
        // A toolbar coming or going resizes the content, not the window.
        content.postsFrameChangedNotifications = true
        NotificationCenter.default.addObserver(self, selector: #selector(contentResized(_:)), name: NSView.frameDidChangeNotification, object: content)
    }

    var isMain: Bool { self === AsukaRuntime.shared.main }

    @objc func contentResized(_ note: Notification) { AsukaRuntime.shared.windowResized(self) }

    func windowDidResize(_ notification: Notification) {
        AsukaRuntime.shared.windowResized(self)
        AsukaRuntime.shared.windowsChanged()
    }

    func windowDidChangeBackingProperties(_ notification: Notification) {
        AsukaRuntime.shared.windowResized(self)
    }
}

@MainActor extension AsukaRuntime {
    /// Makes the app's window, its content a flipped container the root
    /// goes in: 800 by 600 points if the app gives no size, as desktop
    /// frameworks' windows are (Electron's), centered.
    func makeMainWindow() {
        let r = CGRect(origin: .zero, size: asukaDefaultWindowSize)
        let w = NSWindow(contentRect: r, styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        w.isReleasedWhenClosed = false
        w.title = appName
        let content = AsukaContentView(frame: r)
        w.contentView = content
        w.contentMinSize = CGSize(width: 320, height: 320)
        w.center()
        // Where the user left it comes back as its options are applied
        // (Windows.swift's configure, under its key).
        main = AsukaWin(window: w, content: content, base: appName)
    }

    /// The window w is (nil: not one of the app's).
    func win(of w: NSWindow?) -> AsukaWin? {
        guard let w else { return nil }
        if main?.window === w { return main }
        return wins[ObjectIdentifier(w)]
    }

    /// The window the user is working in: the key window if it's one of
    /// the app's, else the main one.
    var keyWin: AsukaWin {
        if let w = win(of: NSApp.keyWindow) ?? win(of: NSApp.mainWindow) { return w }
        // The app isn't active: its front window that shows (a document
        // app's main window stays hidden).
        if !main.window.isVisible {
            for nw in NSApp.orderedWindows { if let w = win(of: nw), nw.isVisible { return w } }
        }
        return main
    }

    var allWins: [AsukaWin] { [main] + Array(wins.values) }

    /// The window a node's view is in, or that it belongs to.
    func winOf(_ n: AsukaNode) -> AsukaWin? {
        if let w = n.window { return w }
        if let v = n.view, let w = win(of: v.window) { return w }
        var p = n.parent
        while let x = p {
            if let w = x.window { return w }
            if let v = x.view, let w = win(of: v.window) { return w }
            p = x.parent
        }
        return main
    }

    /// The window's environment: its content's size, its display's scale,
    /// dark mode, Reduce Motion, the locale, languages and formats, and the
    /// time zone. A Mac has no safe area or keyboard over the window, and
    /// no text size setting; a hidden title bar's strip is its SafeTop.
    func environment(_ w: AsukaWin) -> AsukaEnvironment {
        var e = AsukaEnvironment()
        let b = w.content.bounds
        e.width = Float(b.width)
        e.height = Float(b.height)
        e.safeTop = safeTop(w)
        let scale = w.window.backingScaleFactor
        e.scale = Float(scale > 0 ? scale : 2)
        e.fontScale = 1
        e.dark = isDark(w.window)
        let ws = NSWorkspace.shared
        e.reduceMotion = ws.accessibilityDisplayShouldReduceMotion
        e.screenReader = ws.isVoiceOverEnabled
        e.platform = 3 // ops.PlatformMacOS
        e.locale = Locale.current.identifier
        e.timeZone = TimeZone.current.identifier
        e.languages = languages
        e.firstWeekday = firstWeekday
        e.hourCycle = hourCycle
        return e
    }

    func isDark(_ w: NSWindow?) -> Bool {
        let a = w?.effectiveAppearance ?? NSApp.effectiveAppearance
        return a.bestMatch(from: [.aqua, .darkAqua]) == .darkAqua
    }

    /// A window's content changed size (or scale): the main window's
    /// reports its environment to the root, a Window's to its screen.
    func windowResized(_ w: AsukaWin) {
        if w.isMain {
            environmentChanged()
        } else {
            windowContentResized(w)
        }
    }

    /// Reports a new environment to the root, and lays out what the host
    /// sizes (screens, sheets).
    func environmentChanged() {
        guard let main else { return }
        let e = environment(main)
        if e == env {
            layoutNav()
            return
        }
        env = e
        if let r = root { send(AsukaEvents.environment(r.id, e)) }
        layoutNav()
    }

    /// Follows what the environment reports: dark mode, Reduce Motion, the
    /// locale and time zone (the window's size and scale come to its
    /// delegate), and the app's activation (Interaction.swift's appActive).
    func observe() {
        visual.observeApp()
        appearanceObservation = NSApp.observe(\.effectiveAppearance, options: []) { _, _ in
            DispatchQueue.main.async { MainActor.assumeIsolated { AsukaRuntime.shared.appearanceChanged() } }
        }
        let c = NotificationCenter.default
        for name in [NSLocale.currentLocaleDidChangeNotification, NSNotification.Name.NSSystemTimeZoneDidChange, NSColor.systemColorsDidChangeNotification] {
            c.addObserver(self, selector: #selector(systemChanged(_:)), name: name, object: nil)
        }
        NSWorkspace.shared.notificationCenter.addObserver(self, selector: #selector(systemChanged(_:)),
            name: NSWorkspace.accessibilityDisplayOptionsDidChangeNotification, object: nil)
    }

    @objc func systemChanged(_ note: Notification) {
        refreshLocale()
        environmentChanged()
        if note.name == NSColor.systemColorsDidChangeNotification { appearanceChanged() }
    }

    /// Redoes the colors that don't follow the appearance themselves
    /// (layers' CGColors), and reports dark mode.
    func appearanceChanged() {
        for n in nodes.values { withAppearance(n) { applyLayerColors(n) } }
        dark = isDark(main?.window)
        environmentChanged()
        for w in wins.values { windowContentResized(w) }
    }

    func lifecycle(_ s: AsukaLifecycle) {
        if s == .background { AsukaShell.shared.flushRoutes() }
        guard let r = root else { return }
        send(AsukaEvents.lifecycle(r.id, Int64(s.rawValue)))
    }
}

private var appearanceObservation: NSKeyValueObservation?
