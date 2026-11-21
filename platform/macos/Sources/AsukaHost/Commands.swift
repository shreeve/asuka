// The core's commands to views and to the app: focus, animation, the frame
// clock, Call (the host's modules), the pasteboard, sharing, announcements,
// asuka dev's error overlay (DevError.swift).
// Each part's own commands go to its file.
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif
#if canImport(CAsuka)
@_implementationOnly import CAsuka
#endif

@MainActor extension AsukaRuntime {
    public func command(_ id: UInt32, command: UInt16, args: UnsafeRawBufferPointer) {
        guard let n = nodes[id], let c = AsukaCommand(rawValue: command) else { return }
        switch c {
        case .focus:
            focusCommand(n) // Focus.swift
        case .blur:
            blurCommand(n)
        case .listUpdate:
            listUpdate(n, args)
        case .insertText:
            if n.kind == .textField { insertText(n, String(decoding: args, as: UTF8.self)) }
        case .format:
            formatField(n, Array(args))
        case .setLink:
            setFieldLink(n, String(decoding: args, as: UTF8.self))
        case .present:
            if n.kind == .overlay { presentOverlay(n) } else { present(n) }
        case .haptic:
            // The trackpad's feedback: alignment for a selection, a level
            // change for the rest.
            let selection = args.count > 0 && AsukaHaptic(rawValue: Int32(args[0])) == .selection
            NSHapticFeedbackManager.defaultPerformer.perform(selection ? .alignment : .levelChange, performanceTime: .default)
        case .animateBegin:
            beginAnimation(Array(args))
        case .animateEnd:
            endAnimation()
        case .openURL:
            if let u = URL(string: String(decoding: args, as: UTF8.self)) { NSWorkspace.shared.open(u) }
        case .call:
            call(n, Array(args))
        case .copy:
            let pb = NSPasteboard.general
            pb.clearContents()
            pb.setString(String(decoding: args, as: UTF8.self), forType: .string)
        case .play, .pause, .seek:
            videoCommand(n, c, Array(args))
        case .eval, .webNavigate:
            webCommand(n, c, Array(args))
        case .frames:
            setFrames(args.count > 0 && args[0] != 0)
        case .announce:
            announce(String(decoding: args, as: UTF8.self))
        case .devError:
            devError(n, args) // DevError.swift: asuka dev's overlay
        case .devErrorClear:
            devErrorClear()
        case .replace:
            n.nav?.instant = true
        case .a11yFocus:
            a11yFocus(n)
        case .share:
            share(String(decoding: args, as: UTF8.self))
        case .scrollTo:
            guard args.count >= 5 else { return }
            let b = Array(args)
            listScrollTo(n, Int(b.u32(0)), b[4] != 0)
        case .scrollToOffset:
            guard args.count >= 9 else { return }
            let b = Array(args)
            scrollToOffset(n, CGFloat(b.f32(0)), CGFloat(b.f32(4)), b[8] != 0)
        case .accent:
            guard args.count >= 8 else { return }
            let b = Array(args)
            setAccent(b.u64(0), dark: b.count >= 17 ? b.u64(9) : 0) // the dark one after Android's flags
        case .menuBar:
            AsukaShell.shared.setMenuBar(Array(args))
        case .windowOptions:
            windowOptions(Array(args))
        case .windowAction:
            windowAction(n, Array(args))
        case .windowTitle:
            setWindowTitle(String(decoding: args, as: UTF8.self))
        case .location:
            AsukaRoutes.shared.location(Array(args), from: n)
        case .locale:
            setLocale(args)
        case .saveLocale:
            saveLocale(String(decoding: args, as: UTF8.self))
        case .colorScheme:
            guard args.count > 0 else { return }
            setColorScheme(AsukaColorScheme(rawValue: Int32(args[0])) ?? .system)
        case .quit:
            AsukaShell.shared.quit(Array(args))
        case .shareClose, .backExit:
            break // a share extension's; a phone's back out of the app
        }
    }

    /// Makes c the app's accent, and dark its accent in a dark appearance
    /// (0: c in both): Accent colors, filled buttons, links. AppKit's own
    /// controls keep the system's accent, which the user picks.
    func setAccent(_ c: UInt64, dark: UInt64 = 0) {
        accent = c
        accentDark = dark
        for n in nodes.values {
            if n.kind == .button { buttonVariant(n) } else if n.rich != nil { richInvalidate(n) }
        }
    }

    func setColorScheme(_ s: AsukaColorScheme) {
        switch s {
        case .light: NSApp.appearance = NSAppearance(named: .aqua)
        case .dark: NSApp.appearance = NSAppearance(named: .darkAqua)
        default: NSApp.appearance = nil
        }
    }

    /// Scrolls a Scroll or a List to x, y from its content's start, as far
    /// as it scrolls.
    func scrollToOffset(_ n: AsukaNode, _ x: CGFloat, _ y: CGFloat, _ animated: Bool) {
        guard n.kind == .scroll || n.kind == .list, let sv = n.view as? NSScrollView, let doc = sv.documentView else { return }
        listFlush(n)
        let clip = sv.contentView
        let ds = doc.frame.size, cs = clip.bounds.size
        let to = CGPoint(x: min(max(x, 0), max(ds.width - cs.width, 0)), y: min(max(y, 0), max(ds.height - cs.height, 0)))
        if animated { clip.animator().setBoundsOrigin(to) } else { clip.scroll(to: to) }
        sv.reflectScrolledClipView(clip)
    }

    /// Reports a Scroll's or a List's offset (Scrolled), as it scrolls.
    @objc func boundsChanged(_ note: Notification) {
        guard let clip = note.object as? NSClipView else { return }
        listScrolled(clip)
        guard let n = node(of: clip.superview) else { return }
        let b = clip.bounds
        emit(n, .scrolled) { AsukaEvents.scrolled($0, x: Double(b.origin.x), y: Double(b.origin.y)) }
    }

    /// What an event needs on the AppKit side: scroll observation, the
    /// window's state, and each part's (gestures: Interaction.swift).
    func listen(_ n: AsukaNode, _ e: AsukaEventKind) {
        if e == .windowState {
            listenWindow(n)
            return
        }
        if e == .scrolled, n.kind == .scroll || n.kind == .list, !n.observesScroll, let sv = n.view as? NSScrollView {
            n.observesScroll = true
            sv.contentView.postsBoundsChangedNotifications = true
            NotificationCenter.default.addObserver(self, selector: #selector(boundsChanged(_:)), name: NSView.boundsDidChangeNotification, object: sv.contentView)
            return
        }
        listenGesture(n, e)
    }

    // MARK: Animation

    func beginAnimation(_ b: [UInt8]) {
        guard b.count >= 5 else { return }
        var a = AsukaAnimation(duration: Double(b.f32(0)), curve: AsukaAnimationCurve(rawValue: Int32(b[4])) ?? .easeInOut)
        if env.reduceMotion { a.duration = 0 }
        animation = a
        animating = a.duration > 0
        guard animating else { return }
        NSAnimationContext.beginGrouping()
        let ctx = NSAnimationContext.current
        ctx.duration = a.duration
        ctx.allowsImplicitAnimation = true
        var name = CAMediaTimingFunctionName.easeInEaseOut
        switch a.curve {
        case .linear: name = .linear
        case .easeIn: name = .easeIn
        case .easeOut, .spring: name = .easeOut
        default: break
        }
        var tf = CAMediaTimingFunction(name: name)
        if a.curve == .spring, b.count >= 22, b.f32(14) < 0.7 {
            // A bouncy spring: AppKit's implicit animations have no springs,
            // so a curve that overshoots its end and comes back, about as
            // far as the damping says. The delay isn't AppKit's either: it
            // starts at once.
            let over = 1 + 0.9 * (0.7 - b.f32(14))
            tf = CAMediaTimingFunction(controlPoints: 0.34, over, 0.64, 1)
        }
        ctx.timingFunction = tf
    }

    func endAnimation() {
        if animating { NSAnimationContext.endGrouping() }
        animating = false
    }

    // MARK: The frame clock

    /// Sends Frame at each display refresh while it's on.
    func setFrames(_ on: Bool) {
        if on == (frameClock != nil) { return }
        if !on {
            frameClock?.stop()
            frameClock = nil
            return
        }
        frameClock = AsukaFrameClock(view: main.content)
    }

    func tick() {
        guard frameClock != nil, let r = root else { return }
        let t = ProcessInfo.processInfo.systemUptime - asukaStart
        send(AsukaEvents.frame(r.id, t))
    }

    // MARK: Accessibility

    func announce(_ s: String) {
        NSAccessibility.post(element: NSApp as Any, notification: .announcementRequested,
                             userInfo: [.announcement: s, .priority: NSAccessibilityPriorityLevel.high.rawValue])
    }

    /// Moves VoiceOver's cursor to n's view: a layout change naming it.
    func a11yFocus(_ n: AsukaNode) {
        guard let v = n.view else { return }
        NSAccessibility.post(element: v, notification: .layoutChanged, userInfo: [.uiElements: [v]])
    }

    // MARK: Sharing

    /// The Share command: the sharing service picker for a text or a link,
    /// at the top middle of the window in use.
    func share(_ s: String) {
        let item: Any = s.contains("://") ? (URL(string: s) as Any? ?? s) : s
        let picker = NSSharingServicePicker(items: [item])
        let v = keyWin.content
        picker.show(relativeTo: CGRect(x: v.bounds.midX, y: 0, width: 1, height: 1), of: v, preferredEdge: .minY)
    }

    // MARK: Modules

    /// Runs a Call command: a module's method (ui.Call), answered with
    /// Result after the batch, as a host's modules do.
    func call(_ n: AsukaNode, _ b: [UInt8]) {
        guard b.count >= 4 else { return }
        let callID = b.u32(0)
        var r = AsukaByteReader(b, at: 4)
        let module = r.string(), method = r.string()
        let payload = Data(r.rest())
        let id = n.id
        var answered = false
        let reply: (Result<Data, Error>) -> Void = { [weak self] res in
            DispatchQueue.main.async {
                MainActor.assumeIsolated {
                    guard let self, !answered else { return }
                    answered = true
                    var out = [UInt8]()
                    withUnsafeBytes(of: callID.littleEndian) { out.append(contentsOf: $0) }
                    switch res {
                    case .success(let d): out.append(1); out.append(contentsOf: d)
                    case .failure(let e): out.append(0); out.append(contentsOf: Array(asukaErrorText(e).utf8))
                    }
                    self.send(AsukaEvents.result(id, out))
                }
            }
        }
        callModule(module, method, payload, reply)
    }

    /// Registers one of the host's modules, its methods by name.
    func registerModule(_ name: String, _ methods: [String: AsukaModuleMethod]) {
        modules[name, default: [:]].merge(methods) { _, new in new }
    }

    /// Runs a method of one of the host's modules: reply gets its answer
    /// once.
    func callModule(_ module: String, _ method: String, _ args: Data, _ reply: @escaping (Result<Data, Error>) -> Void) {
        if !modulesRegistered {
            modulesRegistered = true
            registerAllModules()
        }
        guard let m = modules[module]?[method] else {
            reply(.failure(AsukaModuleError("unsupported: \(module).\(method) isn't on macOS")))
            return
        }
        m(args, reply)
    }
}

/// When the host started: Frame events count from it.
let asukaStart = ProcessInfo.processInfo.systemUptime

/// Calls the runtime's tick at each display refresh: a display link where
/// the system has view display links (14), else a timer at 60 Hz.
@MainActor final class AsukaFrameClock: NSObject {
    private var link: AnyObject?
    private var timer: Timer?

    init(view: NSView) {
        super.init()
        if #available(macOS 14.0, *) {
            let l = view.displayLink(target: self, selector: #selector(tick(_:)))
            l.add(to: .main, forMode: .common)
            link = l
        } else {
            let t = Timer(timeInterval: 1.0 / 60, repeats: true) { _ in
                MainActor.assumeIsolated { AsukaRuntime.shared.tick() }
            }
            t.tolerance = 0
            RunLoop.main.add(t, forMode: .common)
            timer = t
        }
    }

    @objc func tick(_ sender: Any?) { AsukaRuntime.shared.tick() }

    func stop() {
        if #available(macOS 14.0, *) { (link as? CADisplayLink)?.invalidate() }
        timer?.invalidate()
    }
}

/// A native module's method: the call's bytes, and a reply (once).
public typealias AsukaModuleMethod = (_ args: Data, _ reply: @escaping (Result<Data, Error>) -> Void) -> Void

/// An error a module answers with.
struct AsukaModuleError: LocalizedError {
    let message: String
    init(_ m: String) { message = m }
    var errorDescription: String? { message }
}

/// No domain: through a package's Swift (or platform/host's call), the core gives the
/// error's message alone ("not allowed", "stopped"), which the packages
/// compare, not "message (domain code)".
extension AsukaModuleError: CustomNSError {
    static var errorDomain: String { "" }
}

/// What a module answers when the user hasn't allowed it (the packages
/// map it to their ErrNotAllowed).
let asukaNotAllowed = AsukaModuleError("not allowed")

func asukaErrorText(_ e: Error) -> String {
    (e as? LocalizedError)?.errorDescription ?? (e as NSError).localizedDescription
}

/// The host's own modules (notify, push, crash...), which the standard
/// library's packages reach through platform/host (sdk/host).
public enum AsukaHostModules {
    /// Runs module's method with args; reply gets its answer once, on the
    /// main thread.
    @MainActor public static func call(_ module: String, _ method: String, _ args: Data, _ reply: @escaping (Result<Data, Error>) -> Void) {
        AsukaRuntime.shared.callModule(module, method, args) { res in
            DispatchQueue.main.async { reply(res) }
        }
    }
}

/// Reads the little-endian values modules take: u8, u32, u64, f32, f64,
/// u32-length strings and bytes. Past the end: zeros, and bad is set.
struct AsukaByteReader {
    let b: [UInt8]
    var i: Int
    var bad = false

    init(_ b: [UInt8], at i: Int = 0) {
        self.b = b
        self.i = i
    }
    init(_ d: Data) { self.init([UInt8](d)) }

    var atEnd: Bool { i >= b.count }

    mutating func take(_ n: Int) -> ArraySlice<UInt8>? {
        guard !bad, n >= 0, i + n <= b.count else { bad = true; return nil }
        defer { i += n }
        return b[i..<i + n]
    }
    mutating func u8() -> UInt8 { take(1)?.first ?? 0 }
    mutating func u16() -> UInt16 { take(2).map { UInt16($0[$0.startIndex]) | UInt16($0[$0.startIndex + 1]) << 8 } ?? 0 }
    mutating func u32() -> UInt32 {
        guard let s = take(4) else { return 0 }
        return s.withUnsafeBytes { UInt32(littleEndian: $0.loadUnaligned(as: UInt32.self)) }
    }
    mutating func u64() -> UInt64 {
        guard let s = take(8) else { return 0 }
        return s.withUnsafeBytes { UInt64(littleEndian: $0.loadUnaligned(as: UInt64.self)) }
    }
    mutating func f32() -> Float { Float(bitPattern: u32()) }
    mutating func f64() -> Double { Double(bitPattern: u64()) }
    mutating func bytes() -> [UInt8] {
        let n = Int(u32())
        return take(n).map(Array.init) ?? []
    }
    mutating func string() -> String { String(decoding: bytes(), as: UTF8.self) }
    mutating func rest() -> [UInt8] {
        defer { i = b.count }
        return i < b.count ? Array(b[i...]) : []
    }
}

/// Writes what AsukaByteReader reads.
struct AsukaByteWriter {
    var bytes: [UInt8] = []
    mutating func u8(_ v: UInt8) { bytes.append(v) }
    mutating func bool(_ v: Bool) { bytes.append(v ? 1 : 0) }
    mutating func u32(_ v: UInt32) { withUnsafeBytes(of: v.littleEndian) { bytes.append(contentsOf: $0) } }
    mutating func u64(_ v: UInt64) { withUnsafeBytes(of: v.littleEndian) { bytes.append(contentsOf: $0) } }
    mutating func i64(_ v: Int64) { u64(UInt64(bitPattern: v)) }
    mutating func f32(_ v: Float) { u32(v.bitPattern) }
    mutating func f64(_ v: Double) { u64(v.bitPattern) }
    mutating func string(_ s: String) { let u = Array(s.utf8); u32(UInt32(u.count)); bytes.append(contentsOf: u) }
    mutating func data(_ d: [UInt8]) { u32(UInt32(d.count)); bytes.append(contentsOf: d) }
    var data: Data { Data(bytes) }
}
