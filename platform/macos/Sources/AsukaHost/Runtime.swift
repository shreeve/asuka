// The Mac host: AppKit views made and driven from the core's batches, in
// the app's process, the core linked in (asuka.h). Everything runs on the
// main thread; the core's post callback is the only one called from other
// threads.
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif
#if canImport(CAsuka)
@_implementationOnly import CAsuka
#endif

/// The Mac host: applies the core's ops to AppKit views and reports
/// events back.
@MainActor public final class AsukaRuntime: NSObject, AsukaOpHandler {
    public static let shared = AsukaRuntime()

    var nodes: [UInt32: AsukaNode] = [:]
    /// Nodes by their views (and by a multiline field's text view).
    var byView: [ObjectIdentifier: AsukaNode] = [:]
    /// Nodes by the menus they made (their Menu prop's).
    var byMenu: [ObjectIdentifier: AsukaNode] = [:]
    var root: AsukaNode?
    /// The main window, holding the root.
    var main: AsukaWin!
    /// The other windows (Window's, panels', sheets' that are windows), by
    /// their NSWindow.
    var wins: [ObjectIdentifier: AsukaWin] = [:]
    var env = AsukaEnvironment()
    var started = false
    /// startWindows ran: the main window shows as WindowOptions say.
    var windowsStarted = false
    var config = AsukaConfig.load()

    // Calls into the core.
    var depth = 0
    var pendingEvents: [[UInt8]] = []
    /// Work to do once the core has returned (list reloads, AppKit work
    /// that would call back into it).
    var later: [() -> Void] = []

    /// Between AnimateBegin and AnimateEnd: an animated batch.
    var animating = false
    var animation = AsukaAnimation()
    /// The app's accent (the Accent command's); 0 for the system's.
    var accent: UInt64 = 0
    /// The app's accent in a dark appearance (the Accent command's); 0 for
    /// accent's.
    var accentDark: UInt64 = 0
    var dark = false
    /// The frame clock, while the core wants Frame events.
    var frameClock: AsukaFrameClock?
    /// Native modules' methods, by module and method (ui.Call, platform/host).
    var modules: [String: [String: AsukaModuleMethod]] = [:]
    var modulesRegistered = false
    /// Native views' factories, by their type (ui.Native).
    var nativeFactories: [String: (AsukaNativeContext) -> AsukaNativeView] = [:]
    var nextCell: UInt32 = 0

    // Languages (Locale.swift): the language the app shows, the way it
    // reads, the way AppKit reads for good, and the environment's.
    var locale = ""
    var rtl = false
    var appRTL = false
    var languages = ""
    var firstWeekday: UInt8 = 0
    var hourCycle: UInt8 = 0

    /// The app's name: asuka-app.json's, its bundle's, or the process's.
    lazy var appName: String = {
        if !config.name.isEmpty { return config.name }
        let info = Bundle.main.infoDictionary ?? [:]
        return info["CFBundleDisplayName"] as? String ?? info["CFBundleName"] as? String ?? ProcessInfo.processInfo.processName
    }()

    /// Where the app's assets are: the bundle's Resources/assets, else
    /// $ASUKA_ASSETS (asuka dev), else assets in the working directory.
    lazy var assets: URL = AsukaNative.assets

    // MARK: Starting

    /// Runs the app until it quits: the application, its menus and main
    /// window, the core with the app (compiled in, or asuka dev's
    /// bytecode), then AppKit's event loop. From main, on the main thread.
    public static func main() {
        let rt = AsukaRuntime.shared
        asukaLatchDirection(rt.config) // before the application reads it
        let app = NSApplication.shared
        rt.launch(app)
        app.run()
    }

    func launch(_ app: NSApplication) {
        AsukaCrash.start() // before the app's init: its crashes are kept
        AsukaShell.shared.waitForPreviousRun() // under asuka dev: this run starts behind the one before
        refreshLocale()
        appRTL = app.userInterfaceLayoutDirection == .rightToLeft
        rtl = appRTL
        app.setActivationPolicy(AsukaShell.shared.activationPolicy)
        NSWindow.allowsAutomaticWindowTabbing = false
        app.delegate = AsukaAppDelegate.shared
        if let c = config.accentColor { accent = c }
        if let c = config.accentDarkColor { accentDark = c }
        AsukaShell.shared.makeMenuBar()
        makeMainWindow()
        env = environment(main)
        observe()
        AsukaKeyWatcher.shared.watch()
        AsukaShell.shared.restoreState()
        AsukaShell.shared.beforeCore()
        enter { boot() }
        startWindows()
        AsukaAutomation.shared.start()
    }

    /// Starts the core and the app in it.
    private func boot() {
        started = true
        // The app's own files (std/fs's appDir): ASUKA_DATA (asuka dev's
        // work directory), else Application Support/<its ID>.
        appDir.path.withCString { asuka_set_app_dir($0) }
        AsukaNative.done = { call, b in b.withUnsafeBufferPointer { asuka_native_done(call, $0.baseAddress, $0.count) } }
        AsukaNative.invoke = { id, b in
            var out: UnsafeMutablePointer<UInt8>? = nil
            let n = b.withUnsafeBufferPointer { asuka_native_invoke(id, $0.baseAddress, $0.count, &out) }
            guard let out else { return nil }
            defer { asuka_free(out) }
            return Array(UnsafeBufferPointer(start: out, count: n))
        }
        // A Ref a package's method was passed: the NSView of its id (a
        // WebView's WKWebView).
        AsukaNative.views = { [weak self] id in id <= UInt64(UInt32.max) ? self?.nodes[UInt32(id)]?.view : nil }
        var cb = asuka_host_callbacks(
            apply: { buf, len in
                MainActor.assumeIsolated {
                    AsukaRuntime.shared.apply(UnsafeRawBufferPointer(start: buf, count: len))
                }
            },
            measure: { id, w, h, aw, ah, ow, oh in
                MainActor.assumeIsolated {
                    let s = AsukaRuntime.shared.measure(id, w, h, aw, ah)
                    ow?.pointee = s.width.isNaN ? 0 : Float(s.width)
                    oh?.pointee = s.height.isNaN ? 0 : Float(s.height)
                }
            },
            post: {
                DispatchQueue.main.async {
                    MainActor.assumeIsolated { AsukaRuntime.shared.enter { asuka_run_posted() } }
                }
            },
            native_new: { cls, len, iface, out, cap, big in
                let name = String(decoding: UnsafeBufferPointer(start: cls, count: len), as: UTF8.self)
                return AsukaNative.write(AsukaNative.new(name, iface), out, cap, big)
            },
            native_call: { h, method, p, n, out, cap, big in
                AsukaNative.write(AsukaNative.call(h, method, Array(UnsafeBufferPointer(start: p, count: n))), out, cap, big)
            },
            native_start: { h, method, p, n, call in
                AsukaNative.start(h, method, Array(UnsafeBufferPointer(start: p, count: n)), call)
            },
            native_cancel: { AsukaNative.cancel($0) },
            native_release: { AsukaNative.release($0) },
            native_free: { free($0) },
            measure_v: nil
        )
        asuka_start(&cb)
        // The environment before the app: its first layout is at the
        // window's size.
        send(AsukaEvents.environment(0, env))
        startApp()
    }

    /// The app's directory (std/fs's appDir).
    lazy var appDir: URL = {
        if let d = ProcessInfo.processInfo.environment["ASUKA_DATA"], !d.isEmpty { return URL(fileURLWithPath: d, isDirectory: true) }
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSHomeDirectory() + "/Library/Application Support", isDirectory: true)
        return base.appendingPathComponent(appID, isDirectory: true)
    }()

    /// The app's ID: its bundle's, asuka-app.json's, else its name.
    lazy var appID: String = Bundle.main.bundleIdentifier ?? (config.id.isEmpty ? appName : config.id)

    /// Runs the app: under asuka dev (ASUKA_DEV=host:port, which the CLI
    /// starts the player with), the bytecode the CLI sends; otherwise the
    /// app compiled into this build.
    private func startApp() {
        AsukaRoutes.shared.start()
        var err: UnsafeMutablePointer<CChar>?
        var rc: Int32
        let env = ProcessInfo.processInfo.environment
        if let dev = env["ASUKA_DEV"], let colon = dev.lastIndex(of: ":"), let port = Int32(dev[dev.index(after: colon)...]) {
            let hosts = String(dev[..<colon])
            let v = ProcessInfo.processInfo.operatingSystemVersion
            let hello = "{\"platform\":\"darwin\",\"device\":\"desktop\",\"os\":\"\(v.majorVersion).\(v.minorVersion)\"}"
            rc = asuka_dev_connect(hosts, port, hello, &err)
        } else if asuka_has_app() != 0 {
            rc = asuka_run(&err)
        } else {
            print("asuka: this player has no app: run it under asuka dev (ASUKA_DEV)")
            rc = 0
        }
        if rc != 0 {
            let msg = err.map { String(cString: $0) } ?? "asuka: the core failed"
            asuka_free_string(err)
            FileHandle.standardError.write(Data((msg + "\n").utf8))
            Darwin.exit(1)
        }
    }

    // MARK: Calling into the core

    /// Every call into the core goes through here: events AppKit raises
    /// meanwhile (a delegate called while a batch is applied) wait until
    /// it returns, as does work AppKit would do re-entrantly.
    func enter(_ f: () -> Void) {
        depth += 1
        f()
        depth -= 1
        while depth == 0 && (!later.isEmpty || !pendingEvents.isEmpty) {
            if !later.isEmpty {
                let fs = later
                later = []
                for f in fs { f() }
                continue
            }
            let e = pendingEvents.removeFirst()
            send(e)
        }
    }

    /// Runs f once the call into the core in progress has returned (now,
    /// if none is).
    func afterCore(_ f: @escaping () -> Void) {
        if depth == 0 { f() } else { later.append(f) }
    }

    /// Sends an event to the core, or queues it while a call into the
    /// core is in progress.
    func send(_ bytes: [UInt8]) {
        guard started else { return }
        if depth > 0 {
            pendingEvents.append(bytes)
            return
        }
        enter { bytes.withUnsafeBufferPointer { asuka_event($0.baseAddress, $0.count) } }
    }

    /// Sends the event if the core listens for it on the node.
    func emit(_ id: UInt32, _ e: AsukaEventKind, _ make: () -> [UInt8]) {
        guard let n = nodes[id], n.listening(e) else { return }
        send(make())
    }

    func emit(_ n: AsukaNode?, _ e: AsukaEventKind, _ make: (UInt32) -> [UInt8]) {
        guard let n, n.listening(e), nodes[n.id] === n || n.cell != nil else { return }
        send(make(n.id))
    }

    /// Whether the node is still the core's.
    func alive(_ n: AsukaNode) -> Bool { nodes[n.id] === n }

    // MARK: Applying ops

    var batchHasFrames = false

    func apply(_ buf: UnsafeRawBufferPointer) {
        depth += 1
        batchHasFrames = false
        asukaDecode(buf, self)
        batchApplied(batchHasFrames)
        depth -= 1
    }

    public func create(_ id: UInt32, kind raw: UInt16) {
        guard let kind = AsukaKind(rawValue: raw) else {
            fatalError("asuka: the macOS host can't show kind \(raw)")
        }
        let n = AsukaNode(id: id, kind: kind, view: nil)
        if createNav(n) {
            add(n)
            return
        }
        n.view = makeView(n, kind)
        if let v = n.view {
            v.autoresizingMask = []
            v.translatesAutoresizingMaskIntoConstraints = true
        }
        add(n)
    }

    func add(_ n: AsukaNode) {
        nodes[n.id] = n
        if let v = n.view { byView[ObjectIdentifier(v)] = n }
        applyDirection(n)
    }

    /// The node a view is the view of.
    func node(of v: NSView?) -> AsukaNode? {
        guard let v else { return nil }
        return byView[ObjectIdentifier(v)]
    }

    /// The node of the view or the nearest view holding it that has one.
    func nodeAt(_ v: NSView?) -> AsukaNode? {
        var v = v
        while let x = v {
            if let n = byView[ObjectIdentifier(x)] { return n }
            v = x.superview
        }
        return nil
    }

    /// Swaps n's view for v, keeping its place, frame and the props that
    /// matter (a field made secure or multiline, a toggle made a check box).
    func replaceView(_ n: AsukaNode, _ v: NSView) {
        guard let old = n.view else { return }
        v.frame = n.frame
        v.autoresizingMask = []
        old.superview?.replaceSubview(old, with: v)
        byView.removeValue(forKey: ObjectIdentifier(old))
        n.view = v
        byView[ObjectIdentifier(v)] = n
        applyFont(n)
        applyColor(n)
        applyDirection(n)
        // The app's background, border, corners and opacity go with it
        // too: they may have come before what made the new view.
        v.isHidden = old.isHidden
        v.alphaValue = old.alphaValue
        if n.borderWidth != 0 { layer(n)?.borderWidth = n.borderWidth }
        if n.cornerRadius != 0 { applyCorner(n) }
        withAppearance(n) {
            if n.background != 0 { layer(n)?.backgroundColor = asukaColor(n.background)?.cgColor }
            if n.borderColor != 0 { layer(n)?.borderColor = asukaColor(n.borderColor)?.cgColor }
        }
        // What VoiceOver reads of it goes with it: its Label may have come
        // before what made the new view (a checkbox, a multiline field).
        if !n.a11yLabel.isEmpty { (n.textView ?? v).setAccessibilityLabel(n.a11yLabel) }
    }

    public func destroy(_ id: UInt32) {
        guard let n = nodes.removeValue(forKey: id) else { return }
        destroyNative(n)
        if destroyNav(n) { return }
        destroyParts(n)
        for c in n.children { c.parent = nil }
        // Going out with a transition or to its match, or inside a view
        // that is: shown until it's gone.
        if matchOut(n) || exit(n) || inExiting(n.view) {
            if let p = n.parent, let i = p.children.firstIndex(where: { $0 === n }) { p.children.remove(at: i) }
            n.parent = nil
            forget(n)
            return
        }
        if let p = n.parent { remove(p, n) }
        release(n)
    }

    /// Lets go of n's AppKit objects but its view, which stays on screen
    /// (going out with a transition): the host no longer knows it.
    func forget(_ n: AsukaNode) {
        if let v = n.view { byView.removeValue(forKey: ObjectIdentifier(v)) }
        if let t = n.textView { byView.removeValue(forKey: ObjectIdentifier(t)) }
        if let m = n.menu { byMenu.removeValue(forKey: ObjectIdentifier(m)) }
    }

    /// Lets go of n's AppKit objects.
    func release(_ n: AsukaNode) {
        releaseParts(n)
        if n.observesScroll, let sv = n.view as? NSScrollView {
            NotificationCenter.default.removeObserver(self, name: NSView.boundsDidChangeNotification, object: sv.contentView)
        }
        n.view?.removeFromSuperview()
        forget(n)
        n.menu = nil
        n.clipView = nil
    }

    public func insert(_ parent: UInt32, child: UInt32, index: Int) {
        guard let p = nodes[parent], let c = nodes[child] else { return }
        insert(p, c, index)
    }

    func insert(_ p: AsukaNode, _ c: AsukaNode, _ index: Int) {
        if insertNav(p, c, index) || insertSplit(p, c, index) { return }
        let i = min(index, p.children.count)
        p.children.insert(c, at: i)
        c.parent = p
        if animating && !c.transition.isEmpty && !c.placed { c.entering = true }
        matchIn(c)
        if p.kind == .scroll {
            (p.view as? NSScrollView)?.documentView = c.view
            return
        }
        if p.rich != nil, let v = c.view {
            p.view?.addSubview(v) // an inline component (RichText.swift places it)
            return
        }
        guard let cv = p.container, let v = c.view else { return }
        // Above the views of the children before it, below those after.
        if i + 1 < p.children.count, let next = p.children[i + 1].view, next.superview === cv {
            cv.addSubview(v, positioned: .below, relativeTo: next)
            return
        }
        cv.addSubview(v)
    }

    public func remove(_ parent: UInt32, child: UInt32) {
        guard let p = nodes[parent], let c = nodes[child] else { return }
        remove(p, c)
    }

    func remove(_ p: AsukaNode, _ c: AsukaNode) {
        if let i = p.children.firstIndex(where: { $0 === c }) { p.children.remove(at: i) }
        c.parent = nil
        if removeNav(p, c) { return }
        if p.kind == .scroll {
            if let sv = p.view as? NSScrollView, sv.documentView === c.view { sv.documentView = nil }
            return
        }
        c.view?.removeFromSuperview()
    }

    public func setRoot(_ id: UInt32) {
        guard let n = nodes[id] else { return }
        root = n
        main.root = n
        if let v = n.view {
            v.autoresizingMask = []
            if v.superview !== main.content { main.content.addSubview(v) }
        }
        updateWindowChrome()
        // The window may have changed since the core read the environment
        // (WindowOptions come in the same batch, before the root is set).
        env = environment(main)
        send(AsukaEvents.environment(n.id, env))
    }

    public func setFrame(_ id: UInt32, x: Float, y: Float, width: Float, height: Float) {
        guard let n = nodes[id] else { return }
        batchHasFrames = true
        if n.parent?.split != nil { return } // a SplitView's detail: its content item sizes it
        let r = CGRect(x: CGFloat(x), y: CGFloat(y), width: CGFloat(width), height: CGFloat(height))
        if animating && !n.placed && !n.entering {
            // A view's first frame doesn't animate: it wasn't on screen. The
            // batch's implicit animations would grow it, its layers and its
            // subviews out of their zero frames.
            withoutAnimation { place(n, r) }
            return
        }
        place(n, r)
    }

    /// Sets n's frame, and what follows its size: its transform, its
    /// layers and the views it lays out itself.
    func place(_ n: AsukaNode, _ r: CGRect) {
        if let c = n.cell {
            c.setHeight(r.height)
            return
        }
        guard let v = n.view, !(n.nav?.sizedByHost(n) ?? false), n.kind != .overlay else { return } // an overlay's size is its root's
        if let p = n.parent, p.rich != nil {
            // An inline component: its size is the core's, its place the text's.
            n.frame.size = r.size
            v.setFrameSize(r.size)
            n.placed = true
            placeInlines(p)
            return
        }
        n.frame = r
        let keep = horizontalScroll(n)
        let gap = keep.map { startGap($0) } ?? 0
        if n.entering {
            // Comes in from its transition's state, to its first frame.
            n.entering = false
            enterWithTransition(n, r)
        } else if animating && keep == nil {
            v.animator().frame = r
        } else {
            v.frame = r
        }
        if let k = keep { scrollFromStart(k, gap) }
        n.placed = true
        placed(n)
        if !n.transform.isEmpty { setTransform(n) }
        if let g = n.gradientLayer { layoutGradient(n, g) }
        n.effect?.frame = CGRect(origin: .zero, size: r.size)
        n.clipView?.frame = CGRect(origin: .zero, size: r.size)
        if n.cornerRadius == 0 { updateShadowPath(n) } // applyCorner does when it's rounded
        else { applyCorner(n) }
        if n.kind == .stack || n.kind == .tabs { layoutScreens(n) }
    }

    public func listen(_ id: UInt32, event: UInt16) {
        guard let n = nodes[id] else { return }
        n.listens.insert(event)
        guard let e = AsukaEventKind(rawValue: event) else { return }
        listen(n, e)
    }

    public func unlisten(_ id: UInt32, event: UInt16) {
        nodes[id]?.listens.remove(event)
    }

    public func setProp(_ id: UInt32, prop: UInt16, value: AsukaValue) {
        guard let n = nodes[id] else { return }
        if prop < 64 {
            setCommon(n, prop, value)
        } else {
            setOwn(n, prop, value)
        }
    }

    // MARK: Cells

    /// A node for a list cell the host made: its id the host's own
    /// (0x80000000 and up), which the core binds.
    func makeCell(_ cell: AsukaListCell) -> AsukaNode {
        nextCell += 1
        let n = AsukaNode(id: 0x8000_0000 | nextCell, kind: nil, view: cell.content)
        n.cell = cell
        nodes[n.id] = n
        byView[ObjectIdentifier(cell.content)] = n
        applyDirection(n)
        return n
    }
}

/// What AnimateBegin asked for.
struct AsukaAnimation {
    var duration: Double = 0
    var curve: AsukaAnimationCurve = .easeInOut
}

/// Runs f with implicit animations off. An animated batch turns them on,
/// and a view made in it would grow out of its zero frame into its first.
@MainActor func withoutAnimation(_ f: () -> Void) {
    NSAnimationContext.beginGrouping()
    let c = NSAnimationContext.current
    c.duration = 0
    c.allowsImplicitAnimation = false
    f()
    NSAnimationContext.endGrouping()
}

/// The app's settings from its asuka.toml (internal/appconfig's File,
/// asuka-app.json): its Resources' (or ASUKA_APP_CONFIG's).
struct AsukaConfig: Decodable {
    var name = ""
    var id = ""
    var version = ""
    var build = ""
    var accent = ""
    var accentDark = ""
    var scheme = ""
    var fileTypes = ""
    var lang = ""
    var direction = ""
    var background = false
    var keepRunning = false
    var multipleInstances = false
    var updateFeed = ""
    var updateKey = ""
    var updateTarget = ""

    enum CodingKeys: String, CodingKey {
        case name, id, version, build, accent, accentDark, scheme, fileTypes, lang, direction, background, keepRunning, multipleInstances, updateFeed, updateKey, updateTarget
    }

    init() {}

    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: CodingKeys.self)
        func s(_ k: CodingKeys) -> String { (try? c.decodeIfPresent(String.self, forKey: k)) ?? "" }
        func b(_ k: CodingKeys) -> Bool { (try? c.decodeIfPresent(Bool.self, forKey: k)) ?? false }
        name = s(.name); id = s(.id); version = s(.version); build = s(.build); accent = s(.accent); accentDark = s(.accentDark)
        scheme = s(.scheme); fileTypes = s(.fileTypes); lang = s(.lang); direction = s(.direction)
        background = b(.background); keepRunning = b(.keepRunning); multipleInstances = b(.multipleInstances)
        updateFeed = s(.updateFeed); updateKey = s(.updateKey); updateTarget = s(.updateTarget)
    }

    static let file = "asuka-app.json"

    static func load() -> AsukaConfig {
        var url: URL?
        if let p = ProcessInfo.processInfo.environment["ASUKA_APP_CONFIG"], !p.isEmpty {
            url = URL(fileURLWithPath: p)
        } else if let r = Bundle.main.resourceURL?.appendingPathComponent(file), FileManager.default.fileExists(atPath: r.path) {
            url = r
        }
        guard let u = url, let data = try? Data(contentsOf: u) else { return AsukaConfig() }
        return (try? JSONDecoder().decode(AsukaConfig.self, from: data)) ?? AsukaConfig()
    }

    /// The accent, "#rrggbb" or "#rrggbbaa", as an ops color.
    var accentColor: UInt64? { Self.hexColor(accent) }

    /// The accent in a dark appearance, likewise.
    var accentDarkColor: UInt64? { Self.hexColor(accentDark) }

    /// A "#rrggbb" or "#rrggbbaa" color as an ops color.
    static func hexColor(_ s: String) -> UInt64? {
        var hex = s
        guard hex.hasPrefix("#") else { return nil }
        hex.removeFirst()
        if hex.count == 6 { hex += "ff" }
        guard hex.count == 8, let v = UInt32(hex, radix: 16) else { return nil }
        return 2 << 32 | UInt64(v)
    }
}
