// Windows: a Window's (openWindow), the main window's options
// (WindowOptions), and controlling them (ui.WindowHandle, WindowProps):
// their options as they open, WindowAction commands, and their state
// reported as it changes.
//
// The options are NSWindow's: its style mask (no resize, close or minimize
// button; a full-size content view under a transparent, title-less title
// bar for TitleBarHidden, the traffic lights kept, the environment's
// SafeTop the title bar's height, from contentLayoutRect), the zoom button
// turned off for DisableMaximize, the content's least and most size, the
// floating level for AlwaysOnTop, left out of the Window menu for
// SkipTaskbar (a Mac has no taskbar), a background color or a clear,
// non-opaque window for Transparent, zoomed or in full screen as it shows.
// A child is a child window of its parent's (ordered above it, moving with
// it), a modal one a sheet of it. Positions are the frame's top left from
// the main display's top left, y down: AppKit's are from its bottom left,
// y up.
//
// Where a window was is NSWindow's frame autosave: the main window's under
// "asuka <app>" (as before keys) when its key is "main", the others' under
// "asuka <app> <key>"; restored as the window opens, over its size and
// place. A Mac has no maximized state to keep: a zoomed window's frame is
// what's restored. Nothing is remembered under automation, which would
// leave its windows' places in the user's defaults.
//
// The state is reported (WindowState) while the core listens: at once,
// then as the window shows or hides (the app's actions), becomes or stops
// being key, the app comes to the front or leaves it, the window moves,
// resizes, is minimized, zoomed or goes in or out of full screen.
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

/// A window's state, as WindowState reports it (ops.WindowState): its
/// frame's top left from the main display's top left, its content's size,
/// and its flags.
struct AsukaWindowState: Equatable {
    var x: Float = 0, y: Float = 0, width: Float = 0, height: Float = 0
    var flags: UInt8 = 0

    static let visible: UInt8 = 1 << 0
    static let focused: UInt8 = 1 << 1
    static let minimized: UInt8 = 1 << 2
    static let maximized: UInt8 = 1 << 3
    static let fullscreen: UInt8 = 1 << 4
    static let onTop: UInt8 = 1 << 5
    static let clickThrough: UInt8 = 1 << 6

    /// ops.AppendWindowState: f32 x, y, width, height; u8 flags.
    var bytes: [UInt8] {
        var w = AsukaByteWriter()
        for v in [x, y, width, height] { w.f32(v) }
        w.u8(flags)
        return w.bytes
    }

    var json: [String: Any] {
        ["x": Double(x), "y": Double(y), "width": Double(width), "height": Double(height), "flags": Int(flags),
         "visible": flags & Self.visible != 0, "focused": flags & Self.focused != 0, "minimized": flags & Self.minimized != 0,
         "maximized": flags & Self.maximized != 0, "fullscreen": flags & Self.fullscreen != 0, "onTop": flags & Self.onTop != 0,
         "clickThrough": flags & Self.clickThrough != 0]
    }
}

/// What a flyout window (WindowFlyout) hangs from, as its Window's Flyout
/// prop carries it (ops.Flyout): a tray icon, or a rectangle of its
/// parent's content.
struct AsukaFlyout: Equatable {
    /// The tray icon it hangs from; 0: the rectangle instead.
    var tray: UInt32 = 0
    /// The rectangle, in points from the top left of its parent's content;
    /// one of no size is a point.
    var x: Float = 0, y: Float = 0, width: Float = 0, height: Float = 0
    var placement: Int32 = 0
    var gap: Float = 0

    init() {}

    /// ops.DecodeFlyout: u32 tray, f32 x, y, width, height, u8 placement,
    /// f32 gap; cut short, none.
    init?(_ b: [UInt8]) {
        guard b.count >= 25 else { return nil }
        tray = b.u32(0)
        x = b.f32(4); y = b.f32(8); width = b.f32(12); height = b.f32(16)
        placement = Int32(b[20])
        gap = b.f32(21)
    }
}

/// A window's options' and state's (winctl, flyouts).
@MainActor final class AsukaWinShell {
    /// It lets the mouse through (but over its views that take clicks:
    /// ClickRegion.swift).
    var through = false
    /// As last reported.
    var state = AsukaWindowState()
    var stateSent = false
    /// Has shown: what it opens as (zoomed, full screen) is done.
    var shown = false
    /// The window it's a child window of.
    weak var child: NSWindow?
    /// The window it's a sheet of (Modal).
    weak var sheetOf: NSWindow?
    /// A flyout's anchor, and where on the screen it was last placed
    /// against (y down).
    var flyout = AsukaFlyout()
    var placedAgainst: AsukaFlyoutRect?
    /// A panel the app placed (not centered on the pointer's display as
    /// it shows).
    var placed = false
    /// A window sized to its content that shows once it's sized (fitted).
    var showOnFit = false, fitted = false
}

extension AsukaWindowConfig {
    /// A flyout's or a panel's.
    var floating: Bool { flags.contains(.panel) || flags.contains(.flyout) }
}

/// The height of the main display (the one with the menu bar, AppKit's
/// origin), which positions are flipped by.
@MainActor func asukaMainScreenHeight() -> CGFloat { NSScreen.screens.first?.frame.height ?? 0 }

@MainActor extension AsukaRuntime {
    /// Where AppKit keeps a window's frame for key.
    func autosaveName(_ key: String) -> String {
        if key.isEmpty || key == "main" { return "asuka " + appName }
        return "asuka " + appName + " " + key
    }

    /// Whether the app's own hiding (WindowAction Hide: a tray app's
    /// window put away) took its windows off the screen, rather than the
    /// user closing them. AppKit asks applicationShouldTerminateAfterLast-
    /// WindowClosed as the last visible window is ordered out, hidden or
    /// closed alike, and a hide must not quit the app: the delegate
    /// (Shell.swift) says no while this holds.
    var windowsHiddenByApp: Bool { navTarget.hidByApp }

    /// Whether windows' places are kept: not under automation (a test's or
    /// a snapshot's), which would leave theirs in the user's defaults.
    var remembersWindows: Bool { !AsukaAutomation.shared.active }

    /// The style a window's options give it.
    func styleMask(_ c: AsukaWindowConfig) -> NSWindow.StyleMask {
        if c.floating { return floatingStyle(c) }
        var m: NSWindow.StyleMask = [.titled, .closable, .miniaturizable, .resizable]
        if c.flags.contains(.fixedSize) { m.remove(.resizable) }
        if c.flags.contains(.noClose) { m.remove(.closable) }
        if c.flags.contains(.noMinimize) { m.remove(.miniaturizable) }
        if c.titleBar == .hidden { m.insert(.fullSizeContentView) }
        return m
    }

    /// Applies a window's options to x as it opens: its style, its least
    /// and most size, its size and place (unless its frame was kept under
    /// its key, which wins), level, background. It reports whether a kept
    /// frame was restored.
    @discardableResult
    func configure(_ x: AsukaWin, _ c: AsukaWindowConfig) -> Bool {
        let w = x.window
        x.cfg = c
        x.configured = true
        var mask = styleMask(c)
        if w.styleMask.contains(.fullScreen) { mask.insert(.fullScreen) }
        // A flyout's or a panel's is the style it was made with: set again,
        // even as it was, the window server forgets it never activates the
        // app.
        if !c.floating {
            w.styleMask = mask
        } else {
            floatingChrome(x)
            x.shell.placed = c.flags.contains(.positioned) || c.flags.contains(.centered)
        }
        if c.flags.contains(.clickThrough) { setClickThrough(x, true) }
        if c.titleBar == .hidden && !c.floating {
            w.titlebarAppearsTransparent = true
            w.titleVisibility = .hidden
        }
        w.standardWindowButton(.zoomButton)?.isEnabled = !c.flags.contains(.noMaximize) && !c.flags.contains(.fixedSize)
        if c.maxWidth > 0 || c.maxHeight > 0 {
            let mw = c.maxWidth > 0 ? CGFloat(c.maxWidth) : CGFloat(Float.greatestFiniteMagnitude)
            let mh = c.maxHeight > 0 ? CGFloat(c.maxHeight) : CGFloat(Float.greatestFiniteMagnitude)
            w.contentMaxSize = CGSize(width: mw, height: mh)
        }
        if c.flags.contains(.alwaysOnTop) { w.level = .floating }
        if c.flags.contains(.skipTaskbar) { w.isExcludedFromWindowsMenu = true }
        if c.flags.contains(.transparent) {
            w.isOpaque = false
            w.backgroundColor = .clear
        } else if c.background != 0, let col = asukaColor(c.background) {
            w.backgroundColor = col
        }
        // Its size, then its place: where it was left, or the options'.
        if c.width > 0 || c.height > 0 {
            let cur = x.content.bounds.size
            w.setContentSize(CGSize(width: c.width > 0 ? CGFloat(c.width) : cur.width, height: c.height > 0 ? CGFloat(c.height) : cur.height))
        }
        if x.isMain || c.flags.contains(.centered) { w.center() }
        if c.flags.contains(.positioned) { setTopLeft(w, CGFloat(c.x), CGFloat(c.y)) }
        var key = c.stateKey
        if x.isMain && key.isEmpty && !c.floating { key = "main" } // a panel goes where the pointer is, unless kept
        var restored = false
        if !key.isEmpty && remembersWindows {
            let name = autosaveName(key)
            restored = w.setFrameUsingName(name)
            w.setFrameAutosaveName(name)
            x.shell.placed = x.shell.placed || restored
        }
        layoutChanged(x)
        return restored
    }

    /// Puts w's frame's top left at x, y points from the main display's top
    /// left.
    func setTopLeft(_ w: NSWindow, _ x: CGFloat, _ y: CGFloat) {
        w.setFrameTopLeftPoint(CGPoint(x: x, y: asukaMainScreenHeight() - y))
    }

    /// Opens a Window's window with its options: its content's size (the
    /// main window's default if the app gives none), its least and most, its
    /// title; where the user left it if it has a key, else where its
    /// options say, else down and right of the window in use, as new
    /// windows cascade. A child goes above its parent, a modal one is a
    /// sheet of it. It shows unless it opens hidden.
    func openWindow(_ n: AsukaNode) {
        guard let nav = n.nav, n.window == nil, let view = n.view else { return }
        var c = nav.cfg
        let size = CGSize(width: c.width > 0 ? CGFloat(c.width) : asukaDefaultWindowSize.width,
                          height: c.height > 0 ? CGFloat(c.height) : asukaDefaultWindowSize.height)
        c.width = Float(size.width)
        c.height = Float(size.height)
        let r = CGRect(origin: .zero, size: size)
        let w: NSWindow = c.floating ? newPanelWindow(size, c)
            : NSWindow(contentRect: r, styleMask: styleMask(c), backing: .buffered, defer: false)
        w.isReleasedWhenClosed = false
        let content = AsukaContentView(frame: r)
        w.contentView = content
        if c.minWidth > 0 || c.minHeight > 0 {
            w.contentMinSize = CGSize(width: max(CGFloat(c.minWidth), 1), height: max(CGFloat(c.minHeight), 1))
        }
        view.frame = r
        content.addSubview(view)
        let x = AsukaWin(window: w, content: content, base: n.title.isEmpty ? appName : n.title)
        x.root = n
        x.shell.flyout = nav.flyout
        wins[ObjectIdentifier(w)] = x
        n.window = x
        nav.window = w
        n.presented = true
        if c.flags.contains(.child) {
            let parent = parentWindow(c.parent)
            // Modal to a flyout or a panel: its child window, over it, not
            // a sheet hanging from it.
            if c.flags.contains(.modal) && overFloating(c) == nil { x.shell.sheetOf = parent } else { x.shell.child = parent }
        }
        // Cascade from the window in use, unless it's placed.
        let from = keyWin.window.frame
        w.cascadeTopLeft(from: CGPoint(x: from.minX + 24, y: from.maxY - 24))
        configure(x, c)
        applyDocument(n)
        reportScreen(n, content.bounds.size)
        updateWindowChrome()
        if c.flags.contains(.hidden) { return }
        if c.flags.contains(.fitContent) && !x.shell.fitted {
            // Shown once sized to its content (WindowAction Fit, in the
            // frames' batch of this flush).
            x.shell.showOnFit = true
            return
        }
        // An app in the background (an accessory) isn't active: its window
        // comes over the app in use, as the user asked for it.
        showWin(x, AsukaShell.shared.inBackground)
    }

    /// Closes a Window's window, the core having destroyed it.
    func closeWindowOf(_ n: AsukaNode) {
        guard let x = n.window else { return }
        let w = x.window
        wins.removeValue(forKey: ObjectIdentifier(w))
        w.delegate = nil
        if let p = w.sheetParent { p.endSheet(w) }
        let p = w.parent
        p?.removeChildWindow(w)
        let key = w.isKeyWindow
        w.orderOut(nil)
        w.close()
        n.window = nil
        n.nav?.window = nil
        n.nav?.begin = nil
        if key, let p { regainKey(p) }
        afterCore { [weak self] in self?.windowsChanged() }
    }

    /// Shows the document a Window shows: the dot in its close button while
    /// it's edited, and its file's icon in the title bar, which drags the
    /// file and whose Command-click lists its folders.
    func applyDocument(_ n: AsukaNode) {
        guard let nav = n.nav, let w = n.window?.window else { return }
        w.isDocumentEdited = nav.edited
        let u: URL? = nav.file.isEmpty ? nil : URL(fileURLWithPath: nav.file)
        if u != w.representedURL { w.representedURL = u }
    }

    func setWindowProp(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        guard let nav = n.nav else { return }
        switch p {
        case AsukaProp.Window.title:
            n.title = v.string
            if let w = n.window {
                w.base = v.string.isEmpty ? appName : v.string
                updateWindowChrome()
            }
        case AsukaProp.Window.config:
            nav.cfg = AsukaWindowConfig(v.data)
        case AsukaProp.Window.flyout:
            nav.flyout = AsukaFlyout(v.data) ?? AsukaFlyout()
            if let x = n.window {
                x.shell.flyout = nav.flyout
                if x.window.isVisible { placeFlyout(x) }
            }
        case AsukaProp.Window.edited:
            nav.edited = v.int != 0 || v.bool
            applyDocument(n)
        case AsukaProp.Window.file:
            nav.file = v.string
            applyDocument(n)
        default:
            break
        }
    }

    /// Applies WindowOptions to the main window: its title, its least size,
    /// and its other options (configure: its size and place unless the
    /// user's are remembered).
    /// The app without a main window (WindowNone) never shows it; one
    /// reloaded with a main window again shows it.
    func windowOptions(_ b: [UInt8]) {
        let c = AsukaWindowConfig(b)
        let wasNone = main.cfg.flags.contains(.none)
        if !c.title.isEmpty { main.base = c.title }
        if c.flags.contains(.panel) { makeMainPanel(c) }
        if c.minWidth > 0 || c.minHeight > 0 {
            main.window.contentMinSize = CGSize(width: max(CGFloat(c.minWidth), 1), height: max(CGFloat(c.minHeight), 1))
        }
        configure(main, c)
        updateWindowChrome()
        environmentChanged()
        guard windowsStarted else { return }
        if c.flags.contains(.none), main.window.isVisible {
            navTarget.hidByApp = true
            main.window.orderOut(nil)
        } else if wasNone, !c.flags.contains(.none), !c.flags.contains(.hidden) {
            showWin(main, true)
        }
    }

    /// The WindowTitle command: the main window's title when no screen
    /// names it.
    func setWindowTitle(_ s: String) {
        main.base = s.isEmpty ? appName : s
        updateWindowChrome()
    }

    /// Shows x: a modal one as a sheet of its parent, the others in front,
    /// key; then what it was to open as (zoomed, in full screen). Under
    /// automation it takes no focus: the app isn't activated, and a
    /// snapshot's window is only ordered in front.
    func showWin(_ x: AsukaWin, _ activate: Bool) {
        let w = x.window
        navTarget.hidByApp = false
        if x.cfg.floating {
            showFloating(x)
            x.shell.shown = true
            windowsChanged()
            return
        }
        var activate = activate
        if let over = win(of: x.shell.child), over.cfg.floating, x.cfg.flags.contains(.modal) {
            placeOver(x, over)
            activate = true // a dialog the flyout asked for, in front
        }
        if let s = x.shell.sheetOf {
            if w.sheetParent == nil { s.beginSheet(w, completionHandler: nil) }
        } else if ProcessInfo.processInfo.environment["ASUKA_SNAPSHOT"] != nil {
            w.orderFront(nil)
        } else {
            if activate && !AsukaAutomation.shared.active { NSApp.activate(ignoringOtherApps: true) }
            w.makeKeyAndOrderFront(nil)
        }
        if let c = x.shell.child, w.parent == nil { c.addChildWindow(w, ordered: .above) }
        if !x.shell.shown {
            x.shell.shown = true
            if x.cfg.flags.contains(.maximized) && !w.isZoomed { w.zoom(nil) }
            if x.cfg.flags.contains(.fullscreen) && !w.styleMask.contains(.fullScreen) { w.toggleFullScreen(nil) }
        }
        windowsChanged()
    }

    /// The NSWindow a child's Config.Parent names: a Window node's, else
    /// the main window (the root's, whose SetRoot may come after the
    /// child's props).
    func parentWindow(_ id: UInt32) -> NSWindow {
        if let p = nodes[id], p.kind == .window, let w = p.window?.window { return w }
        return main.window
    }

    /// The node a window's events go to: its Window node, or the root for
    /// the main window.
    func winNode(_ x: AsukaWin) -> AsukaNode? { x.isMain ? root : x.root }

    /// The window a WindowAction's node controls.
    func nodeWin(_ n: AsukaNode) -> AsukaWin? {
        if n.kind == .window { return n.window }
        if n === root || n.kind == .view && root == nil { return main }
        return nil
    }

    /// Does what the app asked of a window.
    func windowAction(_ n: AsukaNode, _ b: [UInt8]) {
        guard let first = b.first, let a = AsukaWindowAction(rawValue: Int32(first)) else { return }
        var px: Float = 0, py: Float = 0
        if a == .size || a == .position || a == .fit {
            guard b.count >= 9 else { return }
            px = b.f32(1)
            py = b.f32(5)
        }
        if a == .fit && fitsContent(n) {
            fitSheet(n, px, py)
            return
        }
        guard let x = nodeWin(n) else { return }
        let w = x.window
        switch a {
        case .show:
            showWin(x, false)
        case .hide:
            navTarget.hidByApp = true
            if let p = w.sheetParent { p.endSheet(w) }
            if x.cfg.floating { hideFloating(x) }
            w.orderOut(nil)
        case .focus:
            if x.cfg.floating {
                // A flyout or a panel takes the keyboard, the app the user
                // is in staying in front.
                showWin(x, false)
                break
            }
            if NSApp.isHidden { NSApp.unhide(nil) }
            if w.isMiniaturized { w.deminiaturize(nil) }
            showWin(x, true)
        case .minimize:
            w.miniaturize(nil)
        case .maximize:
            if !w.isZoomed { w.zoom(nil) }
        case .restore:
            if w.isMiniaturized { w.deminiaturize(nil) } else if w.isZoomed { w.zoom(nil) }
        case .fullscreen, .exitFullscreen:
            if w.styleMask.contains(.fullScreen) != (a == .fullscreen) { w.toggleFullScreen(nil) }
        case .center:
            w.center()
            x.shell.placed = true
        case .size:
            w.setContentSize(CGSize(width: CGFloat(px), height: CGFloat(py)))
            if x.isFlyout && w.isVisible { placeFlyout(x) }
        case .fit:
            fit(x, px, py)
        case .clickThrough, .notClickThrough:
            setClickThrough(x, a == .clickThrough)
        case .position:
            setTopLeft(w, CGFloat(px), CGFloat(py))
            x.shell.placed = true
        case .alwaysOnTop:
            w.level = .floating
        case .notAlwaysOnTop:
            w.level = .normal
        case .close:
            if x.isMain {
                // As the user's closing does, past the core's question: the
                // app quits once its last window has closed.
                navTarget.hidByApp = false
                afterCore { w.close() }
            }
        }
        windowsChanged()
    }

    /// x's state, as WindowState reports it.
    func windowState(_ x: AsukaWin) -> AsukaWindowState {
        let w = x.window
        let f = w.frame
        let b = x.content.bounds.size
        var s = AsukaWindowState(x: Float(f.minX), y: Float(asukaMainScreenHeight() - f.minY - f.height),
                                 width: Float(b.width), height: Float(b.height))
        let mini = w.isMiniaturized
        if w.isVisible || mini { s.flags |= AsukaWindowState.visible }
        if w.isKeyWindow && NSApp.isActive { s.flags |= AsukaWindowState.focused }
        if mini { s.flags |= AsukaWindowState.minimized }
        if w.styleMask.contains(.fullScreen) {
            s.flags |= AsukaWindowState.fullscreen
        } else if w.isZoomed && w.styleMask.contains(.resizable) {
            s.flags |= AsukaWindowState.maximized
        }
        if w.level.rawValue > 0 { s.flags |= AsukaWindowState.onTop }
        if x.shell.through { s.flags |= AsukaWindowState.clickThrough }
        return s
    }

    /// Reports the state of each window the core watches, if it changed,
    /// once the call into the core in progress has returned.
    func windowsChanged() {
        if navTarget.winQueued { return }
        navTarget.winQueued = true
        afterCore { [weak self] in
            guard let self else { return }
            self.navTarget.winQueued = false
            for x in self.allWins {
                guard let n = self.winNode(x), n.listening(.windowState) else { continue }
                let s = self.windowState(x)
                if x.shell.stateSent && s == x.shell.state { continue }
                x.shell.state = s
                x.shell.stateSent = true
                self.send(AsukaEvents.windowState(n.id, s.bytes))
            }
        }
    }

    /// Starts reporting a window's state: it's sent at once.
    func listenWindow(_ n: AsukaNode) {
        guard let x = nodeWin(n) else { return }
        x.shell.stateSent = false
        windowsChanged()
    }

    /// The room a hidden title bar's strip (and a toolbar in it) takes over
    /// w's content: what the content lays out under.
    func safeTop(_ w: AsukaWin) -> Float {
        guard w.cfg.titleBar == .hidden, !w.window.styleMask.contains(.fullScreen) else { return 0 }
        let b = w.content.bounds.height
        let l = w.window.contentLayoutRect.height
        return Float(max(b - l, 0))
    }

    /// Reports what a window's options changed in its content's environment
    /// (a hidden title bar's room).
    func layoutChanged(_ x: AsukaWin) {
        if x.isMain {
            environmentChanged()
        } else if let r = x.root, let nav = r.nav {
            nav.reported = false
            reportScreen(r, x.content.bounds.size)
        }
    }

    /// A Window's content changed size (or the appearance): what the host
    /// sizes in it laid out again, its environment reported.
    func windowContentResized(_ w: AsukaWin) {
        if let r = w.root, r.kind == .window, let v = r.view {
            let b = w.content.bounds
            if v.frame != b { v.frame = b }
            reportScreen(r, b.size)
        }
        layoutNav()
    }
}

// The windows' delegate calls (AsukaWin is each one's delegate): the
// user's closing, and what changes their state.
extension AsukaWin {
    /// The user closing a window: the core asks the app first if it
    /// listens, and closes a Window itself (Dismissed).
    func windowShouldClose(_ sender: NSWindow) -> Bool {
        let rt = AsukaRuntime.shared
        if isMain {
            // The core asks the app first, and closes it if it may.
            if let r = rt.root, r.listening(.closeRequest) {
                rt.emit(r, .closeRequest) { AsukaEvents.closeRequest($0) }
                return false
            }
            rt.navTarget.hidByApp = false // closed: the app quits after it, unless it keeps running
            return true
        }
        // A Window's: the core closes it, asking the app first if it wants
        // to be.
        if let r = root {
            if r.listening(.closeRequest) {
                rt.emit(r, .closeRequest) { AsukaEvents.closeRequest($0) }
            } else {
                rt.emit(r, .dismissed) { AsukaEvents.dismissed($0) }
            }
            return false
        }
        rt.cancel(content)
        return false
    }

    func windowDidMove(_ notification: Notification) { AsukaRuntime.shared.windowsChanged() }
    func windowDidBecomeKey(_ notification: Notification) {
        let rt = AsukaRuntime.shared
        // A movable sheet over it stays in front and keeps the keys
        // (Navigation.swift).
        if let s = rt.blockingSheet(window), s.isVisible { s.makeKeyAndOrderFront(nil) }
        rt.windowsChanged()
    }

    func windowDidResignKey(_ notification: Notification) {
        let rt = AsukaRuntime.shared
        rt.windowsChanged()
        rt.resignedKey(window)
    }

    func windowDidEnterFullScreen(_ notification: Notification) {
        AsukaRuntime.shared.environmentChanged()
        AsukaRuntime.shared.windowContentResized(self)
        AsukaRuntime.shared.windowsChanged()
    }

    func windowDidExitFullScreen(_ notification: Notification) {
        AsukaRuntime.shared.environmentChanged()
        AsukaRuntime.shared.windowContentResized(self)
        AsukaRuntime.shared.windowsChanged()
    }

    func windowDidMiniaturize(_ notification: Notification) {
        AsukaRuntime.shared.lifecycle(.background)
        AsukaRuntime.shared.windowsChanged()
    }

    func windowDidDeminiaturize(_ notification: Notification) {
        AsukaRuntime.shared.lifecycle(.foreground)
        AsukaRuntime.shared.windowsChanged()
    }

    /// Escape where nothing in the window took it: a flyout or a panel
    /// closes.
    @objc func cancelOperation(_ sender: Any?) { AsukaRuntime.shared.cancel(content) }

    var isFlyout: Bool { cfg.flags.contains(.flyout) }
    var isPanel: Bool { cfg.flags.contains(.panel) }
}
