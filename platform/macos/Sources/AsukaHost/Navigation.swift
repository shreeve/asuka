// Navigation on a Mac:
//
//   - A Stack shows its top Screen, which fills it; pushes and pops slide.
//     The window's toolbar is the innermost shown stack's: a back button,
//     the top screen's bar buttons and search field, and the screen's
//     title as the window's (Toolbar.swift).
//   - Tabs are an NSTabView, its pages the Screens.
//   - A Sheet is a sheet window over the app's; Escape dismisses it. A
//     FullScreen one is as big as all of its window's content under the
//     title bar and toolbar, and follows the window as it resizes: a Mac
//     has no full-screen modal of its own, and a sheet is how AppKit takes
//     a window for a task; so does Escape.
//   - An Alert is an NSAlert, shown as a sheet.
//
// Screens and sheets are sized by the host, which reports each one's
// Environment. Windows (their options, state, panels and flyouts) are
// Windows.swift's and Flyouts.swift's; the split view Split.swift's,
// popovers Popover.swift's, snackbars Snackbar.swift's, transitions and
// matched views Transitions.swift's, menus Menus.swift's.
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

/// The room a sheet keeps from its window's sides, 40 on each.
let asukaSheetMargin: CGFloat = 80

/// What navigation keeps of a node (stacks, screens, tabs, sheets, alerts,
/// windows).
@MainActor final class AsukaNav {
    /// The environment last reported to it (a screen's, a sheet's, a
    /// Window's).
    var env = AsukaEnvironment()
    var reported = false

    /// Stack: Replace's, until its next insert: changes happen at once.
    var instant = false

    // Screen.
    var backGesture = true
    var search = ""
    var searchText = ""
    var tabItem: AsukaTabViewItem?
    /// Its Toolbar's and Primary's items, numbered as their Action events
    /// are (AsukaActions.toolbarBase, primaryBase + the item's number):
    /// the window's toolbar shows them after its bar buttons.
    var toolbar: [AsukaActionItem] = []
    var primary: [AsukaActionItem] = []
    var badge = "" // a tab's
    /// Popped by the user: it slides out, its views kept until it has.
    var slidingOut = false

    // Tabs.
    var setting = false
    var reversed = false // its items are in the reverse order of its children

    // Sheet and Alert; on, the node of the window it's declared in (its
    // Window prop: the root, a Window node), which it goes on.
    /// A sheet's window, or a Window's.
    var window: NSWindow?
    var on: UInt32 = 0
    var deferred = false // presenting waits for its window to open
    var full = false // a Sheet's FullScreen: as big as its window's content
    var detents: Int64 = 0
    /// Shows a sheet as tall as its content once the core has sized it
    /// (fitSheet): before, it would slide in at its room's size.
    var begin: (() -> Void)?
    /// The least content size a fit sheet's window had before the sheet
    /// held its width (holdSheetWidth), given back as the sheet goes.
    var parentLeast: CGSize?
    var alert: NSAlert?
    var alertStyle: Int32 = 0
    var order: [Int] = [] // an alert's buttons' indexes, in the alert's order
    var fields: [AsukaAlertField] = []
    var inputs: [NSTextField] = [] // its fields, while it shows
    var defBtn: NSButton? // its default button, which return in its last field presses

    // Window: its options (Config), a flyout's anchor (Flyout), and the
    // document it shows: edited, its file.
    var cfg = AsukaWindowConfig()
    var flyout = AsukaFlyout()
    var edited = false
    var file = ""

    /// Whether the host sizes n (a screen, a sheet's content), not the core.
    func sizedByHost(_ n: AsukaNode) -> Bool {
        n.kind == .screen || n.kind == .sheet || n.kind == .window || n.kind == .popover
    }
}

/// What a window's title and toolbar show: the innermost shown stack's top
/// screen.
@MainActor final class AsukaChrome {
    weak var stack: AsukaNode?
    weak var screen: AsukaNode?
    var back = false
    var actions: [AsukaActionItem] = []
    var search = ""
    var searchField: NSSearchField?
    var ids: [NSToolbarItem.Identifier] = []
    var toolbar: NSToolbar?
    /// The bar buttons' menus, registered in byMenu.
    var menus: [NSMenu] = []
}

/// A tab whose title for VoiceOver says its badge after its title, a
/// count as so many items ("Inbox, 3 items"), rather than its label's
/// parentheses.
final class AsukaTabViewItem: NSTabViewItem {
    @MainActor var screenID: UInt32 { (identifier as? NSNumber)?.uint32Value ?? 0 }

    /// What VoiceOver hears of it.
    @MainActor var spoken: String {
        guard let n = AsukaRuntime.shared.nodes[screenID], let nav = n.nav, !nav.badge.isEmpty else { return label }
        return asukaBadged(n.title, nav.badge)
    }

    override func accessibilityAttributeValue(_ attribute: NSAccessibility.Attribute) -> Any? {
        if attribute == .title { return MainActor.assumeIsolated { spoken } }
        return super.accessibilityAttributeValue(attribute)
    }
}

/// A sheet's window: Escape dismisses it, past what its content does with
/// it.
final class AsukaSheetWindow: NSWindow {
    override func cancelOperation(_ sender: Any?) {
        guard let v = contentView else { return }
        MainActor.assumeIsolated { AsukaRuntime.shared.cancel(v) }
    }
}

/// The delegate and target of navigation's AppKit objects (tab views,
/// toolbars, popovers, toolbar items and menu items), and what navigation
/// keeps of the app as a whole.
@MainActor final class AsukaNavTarget: NSObject {
    static let shared = AsukaNavTarget()
    var rt: AsukaRuntime { AsukaRuntime.shared }

    /// An updateWindowChrome waits for the core to return.
    var chromeQueued = false
    /// A windowsChanged waits for the core to return.
    var winQueued = false
    /// The text fields of the alerts showing: their node and index.
    var alertFields: [ObjectIdentifier: (id: UInt32, index: Int)] = [:]
    /// Views going out (a transition, a match, a popped screen): what's in
    /// them stays with them until they've gone.
    var exiting: [ObjectIdentifier: NSView] = [:]
    /// The split views, by their tables.
    var splits: [ObjectIdentifier: AsukaNode] = [:]
    /// Style.Match's (Transitions.swift).
    var match = AsukaMatching()
    /// The monitor of clicks in other apps, made as the first flyout or
    /// panel shows.
    var outsideClicks: Any?
    /// The app hid a window (WindowAction Hide) and hasn't shown one or
    /// had one closed since (Windows.swift's windowsHiddenByApp).
    var hidByApp = false
}

@MainActor extension AsukaRuntime {
    var navTarget: AsukaNavTarget { AsukaNavTarget.shared }

    func createNav(_ n: AsukaNode) -> Bool {
        switch n.kind {
        case .stack:
            let v = AsukaView(frame: .zero)
            n.view = v
            layer(n)?.masksToBounds = true
        case .screen:
            n.view = AsukaView(frame: .zero)
            n.nav = AsukaNav()
            n.view?.autoresizingMask = []
            return true
        case .tabs:
            let v = NSTabView(frame: .zero)
            v.delegate = navTarget
            v.tabViewType = .topTabsBezelBorder
            n.view = v
        case .sheet, .window:
            n.view = AsukaView(frame: .zero)
        case .popover:
            n.view = AsukaView(frame: .zero)
            n.popover = AsukaPopover()
        case .alert:
            break
        case .snackbar:
            n.snack = AsukaSnack()
        case .overlay:
            makeOverlay(n) // Overlay.swift
            return true
        default:
            return false
        }
        n.nav = AsukaNav()
        n.view?.autoresizingMask = []
        return true
    }

    func insertNav(_ p: AsukaNode, _ c: AsukaNode, _ index: Int) -> Bool {
        if p.kind == .stack && c.kind == .screen {
            let i = min(index, p.children.count)
            let top = topScreen(p)
            p.children.insert(c, at: i)
            c.parent = p
            guard let pv = p.view, let cv = c.view else { return true }
            let b = pv.bounds
            cv.frame = b
            if c.background == 0 {
                withAppearance(c) { layer(c)?.backgroundColor = NSColor.windowBackgroundColor.cgColor }
            }
            if i == p.children.count - 1 {
                pv.addSubview(cv)
                if let top, !(p.nav?.instant ?? false) { slide(p, c, top, true) }
                p.nav?.instant = false
            } else {
                // Below the top: shown when the screens above it are popped.
                cv.isHidden = true
                if let next = p.children[i + 1].view { pv.addSubview(cv, positioned: .below, relativeTo: next) } else { pv.addSubview(cv) }
            }
            reportScreen(c, b.size)
            updateWindowChrome()
            return true
        }
        if p.kind == .tabs && c.kind == .screen {
            let i = min(index, p.children.count)
            p.children.insert(c, at: i)
            c.parent = p
            guard let tv = p.view as? NSTabView, let nav = c.nav else { return true }
            let it = AsukaTabViewItem(identifier: NSNumber(value: c.id))
            it.label = tabLabel(c)
            it.view = c.view
            if let img = symbolImage(c.icon) { it.image = img }
            nav.tabItem = it
            p.nav?.setting = true
            tv.insertTabViewItem(it, at: tabIndex(p, i, p.children.count))
            selectTab(p)
            p.nav?.setting = false
            layoutScreens(p)
            updateWindowChrome()
            return true
        }
        return false
    }

    /// A tab's label: its title, and its badge after it in parentheses, as
    /// Mac apps count things in a tab's or a window's title ("Inbox (3)").
    /// NSTabView draws its tabs itself, its items' labels alone (it never
    /// calls an item's drawLabel:inRect:), so a badge can't be drawn beside
    /// it.
    func tabLabel(_ n: AsukaNode) -> String {
        guard let b = n.nav?.badge, !b.isEmpty else { return n.title }
        return n.title + " (" + b + ")"
    }

    /// Where the item of child i goes among count items of a Tabs.
    func tabIndex(_ n: AsukaNode, _ i: Int, _ count: Int) -> Int {
        n.nav?.reversed == true ? count - 1 - i : i
    }

    /// Selects the selected child's item, wherever it is.
    func selectTab(_ n: AsukaNode) {
        guard n.selected >= 0, n.selected < n.children.count, let it = n.children[n.selected].nav?.tabItem,
              let tv = n.view as? NSTabView, it.tabView === tv else { return }
        tv.selectTabViewItem(it)
    }

    /// Whether a Tabs' items are in the reverse order of its children.
    /// NSTabView follows the app's direction, not its own.
    func tabsReversed(_ n: AsukaNode) -> Bool { isRTL(n) != appRTL }

    /// Puts a Tabs' items in the order they show: where the app's direction
    /// and AppKit's differ, the host reverses them.
    func orderTabs(_ n: AsukaNode) {
        guard let nav = n.nav, let tv = n.view as? NSTabView else { return }
        let rev = tabsReversed(n)
        if nav.reversed == rev { return }
        nav.reversed = rev
        guard n.children.count >= 2 else { return }
        nav.setting = true
        for c in n.children { if let it = c.nav?.tabItem, it.tabView === tv { tv.removeTabViewItem(it) } }
        for (i, c) in n.children.enumerated() {
            if let it = c.nav?.tabItem { tv.insertTabViewItem(it, at: tabIndex(n, i, i + 1)) }
        }
        selectTab(n)
        nav.setting = false
        layoutScreens(n)
    }

    func removeNav(_ p: AsukaNode, _ c: AsukaNode) -> Bool {
        if p.kind == .tabs, let it = c.nav?.tabItem {
            p.nav?.setting = true
            if let tv = p.view as? NSTabView, it.tabView === tv { tv.removeTabViewItem(it) }
            p.nav?.setting = false
            c.nav?.tabItem = nil
            updateWindowChrome()
            return true
        }
        if p.kind == .stack {
            if !(c.nav?.slidingOut ?? false) { c.view?.removeFromSuperview() }
            updateWindowChrome()
            return true
        }
        return false
    }

    /// Destroys a screen, sheet or alert; false: an ordinary destroy.
    func destroyNav(_ n: AsukaNode) -> Bool {
        guard let nav = n.nav else { return false }
        switch n.kind {
        case .screen:
            if let p = n.parent, p.kind == .stack {
                let wasTop = topScreen(p) === n
                if let i = p.children.firstIndex(where: { $0 === n }) { p.children.remove(at: i) }
                n.parent = nil
                let below = topScreen(p)
                if wasTop, let b = below { b.view?.isHidden = false }
                for c in n.children { c.parent = nil }
                if nav.slidingOut {
                    // Popped by the user: its views go once its slide is done.
                    releaseParts(n)
                    forget(n)
                } else if wasTop && !n.dying, let b = below, !p.instantNav, p.view?.window != nil {
                    // Popped by the app: it slides out, then goes.
                    n.dying = true
                    slide(p, b, n, false)
                    releaseParts(n)
                    forget(n)
                } else {
                    release(n)
                }
                updateWindowChrome()
                return true
            }
            if let it = nav.tabItem, let tv = it.tabView { tv.removeTabViewItem(it) }
            nav.tabItem = nil
            if let p = n.parent { remove(p, n) }
            for c in n.children { c.parent = nil }
            release(n)
            updateWindowChrome()
            return true
        case .sheet:
            n.dying = true
            releaseSheetWidth(n)
            if let w = nav.window {
                if let p = w.sheetParent { p.endSheet(w) }
                w.orderOut(nil)
                if let c = w.contentView { byView.removeValue(forKey: ObjectIdentifier(c)) }
                nav.window = nil
                nav.begin = nil
            }
            for c in n.children { c.parent = nil }
            release(n)
            return true
        case .popover:
            n.dying = true
            closePopover(n)
            for c in n.children { c.parent = nil }
            release(n)
            return true
        case .window:
            n.dying = true
            closeWindowOf(n)
            for c in n.children { c.parent = nil }
            release(n)
            return true
        case .alert:
            n.dying = true
            forgetAlertFields(n)
            if let a = nav.alert {
                let w = a.window
                if w.isVisible, let p = w.sheetParent { p.endSheet(w) }
                nav.alert = nil
            }
            return true
        case .snackbar:
            n.dying = true
            destroySnack(n)
            return true
        default:
            // A stack or tabs: an ordinary view.
            afterCore { [weak self] in self?.updateWindowChrome() }
            return false
        }
    }

    func topScreen(_ stack: AsukaNode) -> AsukaNode? {
        stack.children.last { !$0.dying }
    }

    func liveScreens(_ stack: AsukaNode) -> Int { stack.children.filter { !$0.dying }.count }

    /// Animates a push (in comes from the right over out, which moves a
    /// little left) or a pop (out leaves to the right), as macOS navigates;
    /// the other way round where the stack reads right to left.
    func slide(_ stack: AsukaNode, _ inN: AsukaNode, _ out: AsukaNode, _ push: Bool) {
        guard let sv = stack.view, let inView = inN.view, let outView = out.view else { return }
        let b = sv.bounds
        let w = b.width
        // Where a screen coming on a push starts, and one going on a pop
        // ends.
        let away = isRTL(stack) ? -w : w
        if sv.window == nil || env.reduceMotion || w <= 0 {
            outView.isHidden = push
            if !push { slidOut(out, outView) }
            return
        }
        func at(_ x: CGFloat) -> CGRect { CGRect(origin: CGPoint(x: x, y: 0), size: b.size) }
        if push {
            inView.frame = at(away)
        } else {
            inView.frame = at(-away / 3)
            inView.isHidden = false
            sv.addSubview(outView, positioned: .above, relativeTo: inView)
            out.nav?.slidingOut = true
            navTarget.exiting[ObjectIdentifier(outView)] = outView
        }
        NSAnimationContext.runAnimationGroup({ ctx in
            ctx.duration = 0.3
            ctx.allowsImplicitAnimation = true
            ctx.timingFunction = CAMediaTimingFunction(name: .easeInEaseOut)
            inView.animator().frame = at(0)
            outView.animator().frame = push ? at(-away / 3) : at(away)
        }, completionHandler: {
            MainActor.assumeIsolated {
                let rt = AsukaRuntime.shared
                if push {
                    if let n = rt.node(of: outView), rt.topScreen(stack) !== n {
                        outView.isHidden = true
                        outView.frame = at(0)
                    }
                } else {
                    rt.slidOut(out, outView)
                }
            }
        })
    }

    /// A screen popped has slid out: its view goes, and it, if the core
    /// has destroyed it meanwhile.
    func slidOut(_ out: AsukaNode, _ v: NSView) {
        navTarget.exiting.removeValue(forKey: ObjectIdentifier(v))
        out.nav?.slidingOut = false
        if alive(out) && !out.dying { return }
        v.removeFromSuperview()
        if alive(out) { return } // the core destroys it next
        if node(of: v) === out { release(out) }
    }

    /// Sizes a stack's screens, or a tabs' pages, to it and reports their
    /// environments.
    func layoutScreens(_ n: AsukaNode) {
        switch n.kind {
        case .stack:
            guard let v = n.view else { return }
            let b = v.bounds
            for c in n.children where !(c.nav?.slidingOut ?? false) {
                if c.view?.frame != b, !c.dying { c.view?.frame = b }
                reportScreen(c, b.size)
            }
        case .tabs:
            guard let tv = n.view as? NSTabView else { return }
            let r = tv.contentRect
            for c in n.children { reportScreen(c, r.size) }
        default:
            break
        }
    }

    /// The size of a sheet on over: its window's content less a margin, at
    /// most 640 wide, or half as tall for a medium sheet. A sheet as tall
    /// as its content (DetentFit) is that tall at most.
    func sheetRoom(_ n: AsukaNode, _ over: AsukaWin) -> CGSize {
        let cs = over.content.bounds.size
        var size = CGSize(width: min(max(cs.width - asukaSheetMargin, 320), 640), height: max(cs.height - 100, 240))
        if n.nav?.detents == 1 { size.height = max(cs.height / 2, 240) } // medium only
        return size
    }

    /// Whether n is a sheet as tall as its content (DetentFit), which the
    /// core sizes.
    func fitsContent(_ n: AsukaNode) -> Bool {
        n.kind == .sheet && (n.nav?.detents ?? 0) & 4 != 0 && !(n.nav?.full ?? false)
    }

    /// Sizes a sheet as tall as its content to the size the core laid it
    /// out at (wider than its room when its content can't be narrower),
    /// its top edge kept and centred where it was, its window held wide
    /// enough for it.
    func fitSheet(_ n: AsukaNode, _ width: Float, _ height: Float) {
        guard let nav = n.nav, let w = nav.window else { return }
        if width > 0 && height > 0 {
            holdSheetWidth(n, CGFloat(width))
            let f = w.frame
            let top = CGPoint(x: f.midX - CGFloat(width) / 2, y: f.maxY)
            let size = CGSize(width: CGFloat(width), height: CGFloat(height))
            w.setContentSize(size)
            w.setFrameTopLeftPoint(top)
            n.view?.frame = CGRect(origin: .zero, size: size)
        }
        // The first size shows it, once the batch that placed its content
        // is applied.
        if let begin = nav.begin {
            nav.begin = nil
            afterCore(begin)
        }
    }

    /// Keeps the window a fit sheet is on as wide as the sheet and the
    /// margin around it while it shows: the window's least width is
    /// raised to that, and a window already narrower widens about its
    /// centre, as far as its screen allows.
    func holdSheetWidth(_ n: AsukaNode, _ width: CGFloat) {
        guard let nav = n.nav, let over = onWin(n) else { return }
        let pw = over.window
        let need = width + asukaSheetMargin
        if nav.parentLeast == nil { nav.parentLeast = pw.contentMinSize }
        let least = nav.parentLeast ?? .zero
        pw.contentMinSize = CGSize(width: max(least.width, need), height: least.height)
        let have = over.content.bounds.width
        guard have < need else { return }
        var f = pw.frame
        f.origin.x -= (need - have) / 2
        f.size.width += need - have
        if let vis = pw.screen?.visibleFrame {
            f.size.width = min(f.width, vis.width)
            f.origin.x = min(max(f.minX, vis.minX), vis.maxX - f.width)
        }
        pw.setFrame(f, display: true)
    }

    /// Gives a fit sheet's window back its own least size as the sheet
    /// goes.
    func releaseSheetWidth(_ n: AsukaNode) {
        guard let nav = n.nav, let least = nav.parentLeast else { return }
        nav.parentLeast = nil
        onWin(n)?.window.contentMinSize = least
    }

    /// Sizes every host-sized view again (a window resized).
    func layoutNav() {
        for n in nodes.values {
            switch n.kind {
            case .stack, .tabs:
                layoutScreens(n)
            case .sheet, .window:
                guard let nav = n.nav, let w = nav.window, let cv = w.contentView else { continue }
                if nav.full { fitCover(n) }
                let b = cv.bounds
                n.view?.frame = b
                if fitsContent(n), let over = onWin(n) {
                    // The room it has, which the core fits it in.
                    reportScreen(n, sheetRoom(n, over))
                    continue
                }
                reportScreen(n, b.size)
            default:
                break
            }
        }
        placeSnacks()
        syncOverlays()
    }

    /// Sends a screen's or sheet's environment if it changed.
    func reportScreen(_ n: AsukaNode, _ s: CGSize) {
        guard let nav = n.nav, s.width > 0 else { return }
        var e = env
        e.width = Float(s.width)
        e.height = Float(s.height)
        e.safeTop = 0; e.safeRight = 0; e.safeBottom = 0; e.safeLeft = 0
        if n.kind == .window, let w = n.window { e.safeTop = safeTop(w) }
        // Its window's display's scale: a Window on another display than
        // the main window's has its own.
        if let w = n.view?.window, w.backingScaleFactor > 0 { e.scale = Float(w.backingScaleFactor) }
        if nav.reported && e == nav.env { return }
        nav.env = e
        nav.reported = true
        send(AsukaEvents.environment(n.id, e))
    }

    func setNavCommon(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        // An alert has no view: its common props are AppKit's.
        if p == AsukaProp.interceptBack { n.interceptBack = v.bool }
    }

    func setNavProp(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        guard let nav = n.nav else { return }
        switch n.kind {
        case .screen:
            switch p {
            case AsukaProp.Screen.title:
                n.title = v.string
                nav.tabItem?.label = tabLabel(n)
            case AsukaProp.Screen.badge:
                // The tab's label alone changes.
                nav.badge = v.string
                nav.tabItem?.label = tabLabel(n)
                return
            case AsukaProp.Screen.actions:
                n.actions = asukaActionItems(v)
            case AsukaProp.Screen.toolbar:
                nav.toolbar = asukaRebased(asukaActionItems(v), AsukaActions.toolbarBase)
            case AsukaProp.Screen.primary:
                nav.primary = asukaRebased(asukaActionItems(v), AsukaActions.primaryBase)
            case AsukaProp.Screen.backGesture:
                nav.backGesture = v.int != 0 || v.bool
            case AsukaProp.Screen.icon:
                n.icon = v.string
                if let it = nav.tabItem { it.image = symbolImage(v.string) }
            case AsukaProp.Screen.search:
                nav.search = v.string
            case AsukaProp.Screen.searchText:
                nav.searchText = v.string
                for w in allWins where w.chrome.screen === n { w.chrome.searchField?.stringValue = v.string }
                return
            default:
                return
            }
            updateWindowChrome()
        case .tabs:
            if p == AsukaProp.Tabs.selected {
                n.selected = Int(v.int)
                nav.setting = true
                selectTab(n)
                nav.setting = false
                updateWindowChrome()
            }
        case .sheet:
            switch p {
            case AsukaProp.Sheet.detents: nav.detents = v.int
            case AsukaProp.Sheet.window: nav.on = UInt32(truncatingIfNeeded: v.int)
            case AsukaProp.Sheet.fullScreen: nav.full = v.int != 0 || v.bool
            default: break
            }
        case .popover:
            setPopoverProp(n, p, v)
        case .window:
            setWindowProp(n, p, v)
        case .alert:
            switch p {
            case AsukaProp.Alert.title: n.title = v.string
            case AsukaProp.Alert.message: n.message = v.string
            case AsukaProp.Alert.buttons:
                n.actions = asukaActionItems(v)
                updateAlertButtons(n)
            case AsukaProp.Alert.style: nav.alertStyle = Int32(truncatingIfNeeded: v.int)
            case AsukaProp.Alert.window: nav.on = UInt32(truncatingIfNeeded: v.int)
            case AsukaProp.Alert.fields:
                if case .bytes(let b) = v { nav.fields = AsukaAlertFields.decode(b) } else { nav.fields = [] }
            default: break
            }
        case .snackbar:
            setSnackProp(n, p, v)
        default:
            break
        }
    }

    /// The user chose a tab.
    func tabSelected(_ tv: NSTabView, _ item: NSTabViewItem?) {
        guard let n = node(of: tv), let nav = n.nav, !nav.setting, let item else { return }
        guard let i = n.children.firstIndex(where: { $0.nav?.tabItem === item }) else { return }
        n.selected = i
        emit(n, .selected) { AsukaEvents.selected($0, Int64(i)) }
        layoutScreens(n)
        updateWindowChrome()
    }

    /// Chooses the tab titled title, as a click on it would.
    func tapTab(_ title: String) -> Bool {
        for n in nodes.values where n.kind == .tabs {
            guard let tv = n.view as? NSTabView, tv.window?.isVisible == true, !tv.isHiddenOrHasHiddenAncestor else { continue }
            for it in tv.tabViewItems where it.label == title || (n.children.first { $0.nav?.tabItem === it }?.title == title) {
                tv.selectTabViewItem(it)
                return true
            }
        }
        return false
    }

    /// The node of the window a sheet or an alert is declared in (its
    /// Window prop): the root, or a Window node; the root without one.
    func onNode(_ n: AsukaNode) -> AsukaNode? {
        if let on = n.nav?.on, let x = nodes[on], !x.dying, x.kind == .window || x === main?.root { return x }
        return main?.root
    }

    /// The window a sheet or an alert goes on: the one it's declared in;
    /// nil while it isn't open yet (a Window opening in the same batch,
    /// after it).
    func onWin(_ n: AsukaNode) -> AsukaWin? {
        guard let x = onNode(n) else { return main }
        if x.kind == .window { return x.window }
        return main
    }

    /// Shows a sheet or an alert, on the window it's declared in; once the
    /// batch is applied if that window isn't open yet.
    func present(_ n: AsukaNode) {
        guard let nav = n.nav else { return }
        if n.kind == .sheet || n.kind == .alert || n.kind == .snackbar, onWin(n) == nil {
            if nav.deferred { return } // its window never opened
            nav.deferred = true
            afterCore { [weak self] in
                guard let self, self.alive(n), !n.dying else { return }
                self.present(n)
            }
            return
        }
        switch n.kind {
        case .sheet: presentSheet(n)
        case .window: openWindow(n)
        case .popover: presentPopover(n)
        case .snackbar: presentSnack(n)
        case .alert: presentAlert(n)
        default: break
        }
    }

    func presentSheet(_ n: AsukaNode) {
        guard let nav = n.nav, nav.window == nil, let over = onWin(n), let view = n.view else { return }
        var size = sheetRoom(n, over)
        var style: NSWindow.StyleMask = [.titled, .resizable]
        if fitsContent(n) {
            // Its height is its content's: the user doesn't resize it. It
            // opens at its room, which the core fits it in (WindowAction
            // Fit, fitSheet) before it shows.
            style = [.titled]
        }
        if nav.full {
            // Its size is its window's, so the user doesn't resize it.
            size = coverSize(over.window)
            style = [.titled]
        }
        let w = AsukaSheetWindow(contentRect: CGRect(origin: .zero, size: size), styleMask: style, backing: .buffered, defer: false)
        w.isReleasedWhenClosed = false
        let content = AsukaView(frame: CGRect(origin: .zero, size: size))
        content.autoresizesSubviews = false
        w.contentView = content
        view.frame = CGRect(origin: .zero, size: size)
        content.addSubview(view)
        byView[ObjectIdentifier(content)] = n
        nav.window = w
        n.presented = true
        // It shows once it's laid out at its size: the core hears of the
        // size once the batch is applied, and lays it out then. Shown
        // before, it slid in empty (laid out at its window's size), a
        // field's focus ring at its corner.
        let begin: () -> Void = { [weak self] in
            guard let self, self.alive(n), !n.dying, n.nav?.window === w else { return }
            over.window.beginSheet(w, completionHandler: nil)
        }
        if fitsContent(n) {
            // It shows at its content's size: the core's first Fit, for the
            // room reported here or as the window lays out, begins it.
            nav.begin = begin
        }
        afterCore { [weak self] in
            guard let self, self.alive(n), !n.dying, n.nav?.window === w else { return }
            self.reportScreen(n, size)
            if !self.fitsContent(n) { begin() }
        }
    }

    /// The size of a full-screen sheet over window w: all of its content
    /// under the title bar and toolbar (contentLayoutRect), where AppKit
    /// hangs a sheet.
    func coverSize(_ w: NSWindow) -> CGSize { w.contentLayoutRect.size }

    /// Sizes a full-screen sheet's window to its window's content again
    /// (that window resized; AppKit keeps the sheet where it hangs).
    func fitCover(_ n: AsukaNode) {
        guard let w = n.nav?.window, let p = w.sheetParent else { return }
        let want = coverSize(p)
        if w.contentView?.bounds.size == want { return }
        w.setContentSize(want)
    }

    // MARK: Alerts

    func presentAlert(_ n: AsukaNode) {
        guard let nav = n.nav, nav.alert == nil, let over = onWin(n) else { return }
        let a = NSAlert()
        a.messageText = n.title
        a.informativeText = n.message
        // The default button is the first, rightmost; cancel goes last.
        let cancel = AsukaAlertButtonRole.cancel.rawValue
        var order: [Int] = []
        for (i, b) in n.actions.enumerated() where b.role != cancel { order.append(i) }
        for (i, b) in n.actions.enumerated() where b.role == cancel { order.append(i) }
        for i in order {
            let b = n.actions[i]
            let btn = a.addButton(withTitle: b.title)
            if b.role == cancel {
                btn.keyEquivalent = "\u{1b}"
            } else if b.role == AsukaAlertButtonRole.destructive.rawValue, #available(macOS 11.0, *) {
                btn.hasDestructiveAction = true
            }
            if b.disabled { btn.isEnabled = false }
        }
        if order.isEmpty { a.addButton(withTitle: asukaWord("ok")) }
        nav.alert = a
        nav.order = order
        if nav.alertStyle == AsukaAlertStyle.alert.rawValue && !nav.fields.isEmpty { addAlertFields(n, a) }
        let id = n.id
        a.beginSheetModal(for: over.window) { resp in
            MainActor.assumeIsolated {
                let rt = AsukaRuntime.shared
                guard let n = rt.nodes[id] else { return }
                rt.forgetAlertFields(n)
                guard !n.dying, let nav = n.nav else { return }
                nav.alert = nil
                let i = resp.rawValue - NSApplication.ModalResponse.alertFirstButtonReturn.rawValue
                if i >= 0 && i < nav.order.count {
                    let k = Int64(nav.order[i])
                    rt.emit(n, .action) { AsukaEvents.action($0, k) }
                } else {
                    rt.emit(n, .dismissed) { AsukaEvents.dismissed($0) }
                }
            }
        }
        if let first = nav.inputs.first {
            alertDefaultButton(n, a)
            a.window.makeFirstResponder(first)
        }
    }

    /// Gives an alert its text fields: a column of NSTextFields
    /// (NSSecureTextFields for passwords) as its accessory view, the first
    /// the window's first responder, Tab going from one to the next.
    /// Typing reports FieldChanged; return presses the default button, the
    /// first whose role is Default, which takes the key equivalent.
    func addAlertFields(_ n: AsukaNode, _ a: NSAlert) {
        guard let nav = n.nav else { return }
        // As wide as the alert's text (NSAlert's own is about this wide).
        let width: CGFloat = 228, height: CGFloat = 22, gap: CGFloat = 8
        let count = nav.fields.count
        let total = CGFloat(count) * height + CGFloat(count - 1) * gap
        let box = NSView(frame: CGRect(x: 0, y: 0, width: width, height: total))
        nav.inputs = []
        for (i, f) in nav.fields.enumerated() {
            let tf: NSTextField = f.secure ? NSSecureTextField(frame: .zero) : NSTextField(frame: .zero)
            // From the top: a view's y is from its bottom.
            tf.frame = CGRect(x: 0, y: total - CGFloat(i + 1) * height - CGFloat(i) * gap, width: width, height: height)
            tf.stringValue = f.text
            tf.placeholderString = f.placeholder
            tf.setAccessibilityLabel(f.placeholder)
            tf.usesSingleLineMode = true
            tf.cell?.isScrollable = true
            if #available(macOS 11.0, *), let t = asukaContentType(f.contentType) { tf.contentType = t }
            tf.delegate = self
            box.addSubview(tf)
            navTarget.alertFields[ObjectIdentifier(tf)] = (n.id, i)
            nav.inputs.append(tf)
        }
        for (i, tf) in nav.inputs.enumerated() { tf.nextKeyView = nav.inputs[(i + 1) % count] }
        a.accessoryView = box
        a.layout()
        a.window.initialFirstResponder = nav.inputs.first
    }

    /// Gives return to an alert's default button, the first whose role is
    /// Default, wherever it is among them (NSAlert gives it to the first,
    /// as it shows).
    func alertDefaultButton(_ n: AsukaNode, _ a: NSAlert) {
        guard let nav = n.nav else { return }
        let def = n.actions.firstIndex { $0.role == AsukaAlertButtonRole.default.rawValue } ?? -1
        for (k, i) in nav.order.enumerated() where k < a.buttons.count {
            let btn = a.buttons[k]
            if i == def {
                btn.keyEquivalent = "\r"
                nav.defBtn = btn
            } else if btn.keyEquivalent == "\r" {
                btn.keyEquivalent = ""
            }
        }
    }

    /// Forgets an alert's fields, gone or going.
    func forgetAlertFields(_ n: AsukaNode) {
        guard let nav = n.nav else { return }
        for tf in nav.inputs {
            tf.delegate = nil
            navTarget.alertFields.removeValue(forKey: ObjectIdentifier(tf))
        }
        nav.inputs = []
        nav.defBtn = nil
    }

    /// Reports the user's typing in an alert's field; false if f isn't one.
    func alertFieldChanged(_ f: NSTextField) -> Bool {
        guard let a = navTarget.alertFields[ObjectIdentifier(f)] else { return false }
        if let n = nodes[a.id], !n.dying {
            let b = AsukaAlertFields.changed(a.index, f.stringValue)
            emit(n, .fieldChanged) { AsukaEvents.fieldChanged($0, b) }
        }
        return true
    }

    /// Return in an alert's field: before the last field it goes on to the
    /// next; in the last it presses the default button, if it's enabled.
    /// nil if c isn't one.
    func alertFieldCommand(_ c: NSControl, _ cmd: Selector) -> Bool? {
        guard let a = navTarget.alertFields[ObjectIdentifier(c)] else { return nil }
        guard cmd == #selector(NSResponder.insertNewline(_:)) else { return false }
        guard let n = nodes[a.id], !n.dying, let nav = n.nav else { return true }
        if a.index + 1 < nav.inputs.count {
            c.window?.makeFirstResponder(nav.inputs[a.index + 1])
            return true
        }
        if let b = nav.defBtn, b.isEnabled { b.performClick(nil) }
        return true
    }

    /// Shows a presented alert's buttons' new titles and whether they're
    /// enabled.
    func updateAlertButtons(_ n: AsukaNode) {
        guard let nav = n.nav, let a = nav.alert, !n.dying else { return }
        for (k, i) in nav.order.enumerated() {
            guard k < a.buttons.count, i < n.actions.count else { break }
            let b = n.actions[i]
            a.buttons[k].title = b.title
            a.buttons[k].isEnabled = !b.disabled
        }
    }

    /// Escape: a sheet's user dismissal.
    func cancel(_ v: NSView) {
        let w = v.window
        for n in nodes.values where n.kind == .sheet && !n.dying && n.nav?.window != nil && n.nav?.window === w {
            if n.interceptBack {
                emit(n, .back) { AsukaEvents.back($0) }
                return
            }
            emit(n, .dismissed) { AsukaEvents.dismissed($0) }
            return
        }
        // Left by the content: a flyout or a panel closes, as menus do.
        if let w { _ = escapeFloating(w) }
    }

    // MARK: Starting

    /// After the core started the app: the main window configured (its
    /// options, or the host's) and shown, unless it opens hidden or the
    /// app runs in the background.
    func startWindows() {
        windowsStarted = true
        // No WindowOptions: the host's, kept under "main".
        if !main.configured { configure(main, AsukaWindowConfig()) }
        // Shown (automation: without taking the user's focus) unless the
        // app shows it itself, or runs in the background (no window until
        // it shows one; automation shows it still).
        if !main.cfg.flags.contains(.hidden) && (!AsukaShell.shared.inBackground || AsukaAutomation.shared.active) {
            showWin(main, true)
        }
    }

    // MARK: Hooks

    /// Its matched key is matchOut's to let go of (Transitions.swift): it's
    /// asked after this.
    func navDestroyed(_ n: AsukaNode) { overlayDestroyed(n) }

    func navPlaced(_ n: AsukaNode) {}

    func navDirection(_ n: AsukaNode, _ changed: Bool) {
        switch n.kind {
        case .tabs: orderTabs(n)
        case .split: if changed { splitDirectionChanged(n) }
        default: break
        }
    }

    func navBatchApplied(_ frames: Bool) {
        matchBatchApplied(frames)
        syncOverlays() // Overlay.swift: in place, on top, their anchors reported
    }

    /// What navigation adds to a node's JSON: a stack's top screen, a
    /// tab's label and what VoiceOver hears of it, a sheet's window, an
    /// alert's buttons and fields, a Window's window, a popover's, a
    /// snackbar's, a split view's rows.
    func navJSON(_ n: AsukaNode, _ d: inout [String: Any]) {
        overlayJSON(n, &d)
        guard let nav = n.nav else {
            if n.split != nil { splitJSON(n, &d) }
            return
        }
        switch n.kind {
        case .stack:
            if let t = topScreen(n) { d["topScreen"] = t.id }
            d["liveScreens"] = liveScreens(n)
        case .screen:
            if let it = nav.tabItem {
                d["tabLabel"] = it.label
                d["tabSpoken"] = it.spoken
                d["tabHasImage"] = it.image != nil
                d["tabItemID"] = ObjectIdentifier(it).hashValue
            }
            if !nav.badge.isEmpty { d["badge"] = nav.badge }
            if !nav.search.isEmpty { d["search"] = nav.search }
        case .tabs:
            if let tv = n.view as? NSTabView {
                d["tabs"] = tv.tabViewItems.map { $0.label }
                if let s = tv.selectedTabViewItem { d["selectedTab"] = s.label }
            }
            d["selected"] = n.selected
        case .sheet:
            if let w = nav.window {
                d["sheetWindow"] = ["visible": w.isVisible, "styleMask": w.styleMask.rawValue,
                                    "content": asukaRectJSON(w.contentView?.bounds ?? .zero), "frame": asukaRectJSON(w.frame),
                                    "isSheet": w.sheetParent != nil, "parent": w.sheetParent?.title ?? ""] as [String: Any]
                if let over = onWin(n) {
                    let r = sheetRoom(n, over)
                    d["sheetRoom"] = [Double(r.width), Double(r.height)]
                    d["cover"] = [Double(coverSize(over.window).width), Double(coverSize(over.window).height)]
                }
            }
            d["env"] = [Double(nav.env.width), Double(nav.env.height)]
        case .alert:
            if let a = nav.alert {
                d["alert"] = alertJSON(n, a)
            }
        case .window:
            if let w = n.window { d["windowTitle"] = w.window.title }
            d["env"] = [Double(nav.env.width), Double(nav.env.height), Double(nav.env.safeTop)]
        case .popover:
            popoverJSON(n, &d)
        case .snackbar:
            snackJSON(n, &d)
        default:
            break
        }
    }

    func alertJSON(_ n: AsukaNode, _ a: NSAlert) -> [String: Any] {
        let w = a.window
        var fr: NSResponder? = w.firstResponder
        if let tv = fr as? NSTextView, tv.isFieldEditor { fr = tv.delegate as? NSResponder }
        let inputs = n.nav?.inputs ?? []
        var d: [String: Any] = [
            "title": a.messageText, "message": a.informativeText, "visible": w.isVisible, "isSheet": w.sheetParent != nil,
            "buttons": a.buttons.map { b -> [String: Any] in
                var x: [String: Any] = ["title": b.title, "key": b.keyEquivalent, "enabled": b.isEnabled]
                if #available(macOS 11.0, *) { x["destructive"] = b.hasDestructiveAction }
                return x
            },
            "fields": inputs.map { f -> [String: Any] in
                ["class": String(describing: type(of: f)), "stringValue": f.stringValue, "placeholder": f.placeholderString ?? "",
                 "secure": f is NSSecureTextField]
            },
            "hasAccessory": a.accessoryView != nil,
        ]
        if let f = fr as? NSTextField, let i = inputs.firstIndex(where: { $0 === f }) { d["editing"] = i }
        return d
    }

    /// What navigation adds to a window's JSON: its kind and options, its
    /// state as reported, its toolbar's items' details.
    func navWindowJSON(_ w: AsukaWin, _ d: inout [String: Any]) {
        let nw = w.window
        d["floating"] = w.cfg.floating
        d["isPanel"] = nw is NSPanel
        d["canBecomeMain"] = nw.canBecomeMain
        d["collectionBehavior"] = nw.collectionBehavior.rawValue
        d["ignoresMouse"] = nw.ignoresMouseEvents
        d["hasShadow"] = nw.hasShadow
        d["excludedFromWindowsMenu"] = nw.isExcludedFromWindowsMenu
        d["titlebarTransparent"] = nw.titlebarAppearsTransparent
        d["titleHidden"] = nw.titleVisibility == .hidden
        d["zoomEnabled"] = nw.standardWindowButton(.zoomButton)?.isEnabled ?? false
        d["safeTop"] = Double(safeTop(w))
        d["topLeft"] = [Double(nw.frame.minX), Double(asukaMainScreenHeight() - nw.frame.maxY)]
        // Its content's top left on the screen, y down from the main
        // display's top; the work area of the display with the pointer.
        let cs = nw.convertToScreen(w.content.convert(w.content.bounds, to: nil))
        let c = asukaYDown(cs)
        d["contentTopLeft"] = [Double(c.x), Double(c.y)]
        let (px, py) = pointer()
        let work = workArea(px, py)
        d["pointerWorkArea"] = [Double(work.x), Double(work.y), Double(work.w), Double(work.h)]
        let fd = asukaYDown(nw.frame)
        d["frameYDown"] = [Double(fd.x), Double(fd.y), Double(fd.w), Double(fd.h)]
        // A flyout's anchor on the screen as it was last placed against,
        // y down, and its display's work area.
        if w.isFlyout, let a = w.shell.placedAgainst {
            d["flyoutAnchor"] = [Double(a.x), Double(a.y), Double(a.w), Double(a.h)]
            let aw = workArea(a.x + a.w / 2, a.y + a.h / 2)
            d["anchorWorkArea"] = [Double(aw.x), Double(aw.y), Double(aw.w), Double(aw.h)]
        }
        d["childWindows"] = (nw.childWindows ?? []).map { $0.title }
        if let p = nw.sheetParent { d["sheetParent"] = p.title }
        d["isKeyWindow"] = nw.isKeyWindow
        d["autosaveName"] = nw.frameAutosaveName
        if let bg = nw.backgroundColor { d["background"] = colorHex(bg) }
        if let r = w.root { d["node"] = r.id }
        if let s = nw.attachedSheet { d["sheetVisible"] = s.isVisible }
        if let c = nw.contentView?.layer, c.cornerRadius > 0 { d["cornerRadius"] = Double(c.cornerRadius) }
        d["hasMaterial"] = w.content.subviews.contains { $0 is NSVisualEffectView && AsukaRuntime.shared.node(of: $0) == nil }
        if let tb = w.chrome.toolbar {
            d["toolbarItems"] = tb.items.map { it -> [String: Any] in
                var x: [String: Any] = ["id": it.itemIdentifier.rawValue, "label": it.label, "enabled": it.isEnabled, "tag": it.tag,
                                        "hasImage": it.image != nil]
                if #available(macOS 26.0, *) { x["prominent"] = it.style == .prominent }
                if let m = (it as? NSMenuToolbarItem)?.menu { x["menu"] = AsukaAutomation.shared.menuJSON(m) }
                if let s = it as? NSSearchToolbarItem { x["search"] = s.searchField.stringValue; x["placeholder"] = s.searchField.placeholderString ?? "" }
                if #available(macOS 11.0, *) { x["navigational"] = it.isNavigational }
                return x
            }
        }
        if w.shell.stateSent { d["state"] = w.shell.state.json }
        // The views going out in it (a transition's, a match's, a popped
        // screen's): their frames in their superviews, their alpha, whether
        // they wait in the content view.
        d["exiting"] = navTarget.exiting.values.filter { $0.window === nw }.map { v -> [String: Any] in
            ["frame": asukaRectJSON(v.frame), "alpha": Double(v.alphaValue), "inContent": v.superview === w.content]
        }
        d["matchWaiting"] = navTarget.match.outs.count
    }

    func registerNavModules() {}

    // MARK: Automation

    /// The presented nodes (sheets, alerts, popovers, snackbars), which
    /// are no view's children, for the automation's tree, oldest first.
    func presentedRoots() -> [AsukaNode] {
        nodes.values.filter { n in
            guard n.parent == nil, n !== root, !n.dying else { return false }
            switch n.kind {
            case .sheet, .alert, .popover, .snackbar, .overlay: return true
            default: return false
            }
        }.sorted { $0.id < $1.id }
    }

    /// The alert showing (the newest), if one does.
    func showingAlert() -> AsukaNode? {
        nodes.values.filter { $0.kind == .alert && !$0.dying && $0.nav?.alert?.window.isVisible == true }.max { $0.id < $1.id }
    }

    /// Presses the button titled title of the alert showing, as a click
    /// would.
    func tapAlertButton(_ title: String) -> Bool {
        guard let n = showingAlert(), let a = n.nav?.alert, let b = a.buttons.first(where: { $0.title == title }), b.isEnabled else { return false }
        b.performClick(nil)
        return true
    }

    /// The alert's field being edited, or its first, given the focus.
    func editingAlertField(_ n: AsukaNode) -> NSTextField? {
        guard let nav = n.nav, let a = nav.alert else { return nil }
        var r: NSResponder? = a.window.firstResponder
        if let tv = r as? NSTextView, tv.isFieldEditor { r = tv.delegate as? NSResponder }
        if let f = r as? NSTextField, nav.inputs.contains(where: { $0 === f }) { return f }
        guard let f = nav.inputs.first else { return nil }
        a.window.makeFirstResponder(f)
        return f
    }

    /// Navigation's and windows' automation steps; nil: not one of them, ""
    /// done, else what went wrong.
    ///   - pick <title> chooses the item of a menu a view, a button or a
    ///     bar button shows
    ///   - alerttype <text> types text over the alert's field being edited;
    ///     alertreturn presses return in it
    ///   - cancel [title] is Escape in the window in use (or the one
    ///     titled title): its sheet, a flyout, a panel
    ///   - dismiss closes the popover showing, as a click outside it
    ///   - search <text> types into the toolbar's search field;
    ///     searchsubmit presses return in it
    ///   - snackhover on|off moves the mouse over the snackbar or off it
    ///   - closewindow <title> closes the window titled title as its close
    ///     button does; focuswindow <title> makes it the key window
    ///   - movewindow [title] <x>,<y> moves it as the user drags it
    ///   - resignkey [title] takes the keyboard from it (the user going to
    ///     another app); outsideclick is a click in another app
    func navStep(_ verb: String, _ arg: String) -> String? {
        func window(_ title: String) -> NSWindow? {
            if title.isEmpty { return keyWin.window }
            return allWins.first { $0.window.title == title }?.window ?? NSApp.windows.first { $0.title == title }
        }
        switch verb {
        case "pick":
            return pickMenuItem(arg) ?? ""
        case "alerttype":
            guard let n = showingAlert(), let f = editingAlertField(n) else { return "no alert field shows" }
            if let tv = f.currentEditor() as? NSTextView {
                tv.selectAll(nil)
                tv.insertText(arg, replacementRange: tv.selectedRange())
            } else {
                f.stringValue = arg
                _ = alertFieldChanged(f)
            }
            return ""
        case "alertreturn":
            guard let n = showingAlert(), let f = editingAlertField(n) else { return "no alert field shows" }
            _ = alertFieldCommand(f, #selector(NSResponder.insertNewline(_:)))
            return ""
        case "cancel":
            guard let w = window(arg) else { return "no window \(arg)" }
            let target = w.attachedSheet ?? w
            guard let c = target.contentView else { return "no content" }
            cancel(c)
            return ""
        case "dismiss":
            return dismissPopover() ? "" : "no popover shows"
        case "search":
            return typeSearch(arg) ? "" : "no search field shows"
        case "searchsubmit":
            return submitSearch() ? "" : "no search field shows"
        case "snackhover":
            return hoverSnack(arg != "off") ? "" : "no snackbar shows"
        case "closewindow":
            guard let w = window(arg) else { return "no window \(arg)" }
            w.performClose(nil)
            return ""
        case "focuswindow":
            guard let w = window(arg) else { return "no window \(arg)" }
            w.makeKeyAndOrderFront(nil)
            return ""
        case "movewindow":
            // movewindow [title] <x>,<y>: the user drags it there (its
            // frame's top left, y down from the main display's top).
            var parts = arg.split(separator: " ").map(String.init)
            guard let at = parts.popLast() else { return "want movewindow [title] <x>,<y>" }
            let xy = at.split(separator: ",").compactMap { Double($0) }
            guard xy.count == 2 else { return "want movewindow [title] <x>,<y>" }
            guard let w = window(parts.joined(separator: " ")) else { return "no window \(parts.joined(separator: " "))" }
            setTopLeft(w, CGFloat(xy[0]), CGFloat(xy[1]))
            return ""
        case "resignkey":
            // resignkey [title]: the window stops being the key window, as
            // the user going to another app's does (a flyout or a panel
            // closes).
            guard let w = window(arg) else { return "no window \(arg)" }
            w.resignKey()
            resignedKey(w)
            return ""
        case "outsideclick":
            // A click in another app (a flyout or a panel closes).
            for x in allWins { dismissFloating(x, true) }
            return ""
        default:
            return nil
        }
    }
}

extension AsukaNode {
    /// A stack's Replace: its next insert happens at once.
    @MainActor var instantNav: Bool { nav?.instant ?? false }
}

extension AsukaNavTarget: NSTabViewDelegate {
    func tabView(_ tabView: NSTabView, didSelect tabViewItem: NSTabViewItem?) {
        rt.tabSelected(tabView, tabViewItem)
    }
}

/// A Menu's, a screen's bar buttons' or an alert's buttons' items.
func asukaActionItems(_ v: AsukaValue) -> [AsukaActionItem] {
    if case .bytes(let b) = v { return AsukaActions.decode(b) }
    return []
}

/// Items numbered base + their number (a screen's Toolbar's and
/// Primary's Action events).
func asukaRebased(_ items: [AsukaActionItem], _ base: Int64) -> [AsukaActionItem] {
    items.map { var it = $0; it.index += base; it.items = asukaRebased(it.items, base); return it }
}

/// An AutoFill hint as NSTextContentType.
@available(macOS 11.0, *)
func asukaContentType(_ s: String) -> NSTextContentType? {
    switch s {
    case "username": return .username
    case "password", "newPassword": return .password
    case "oneTimeCode": return .oneTimeCode
    default: return nil
    }
}

func asukaRectJSON(_ r: CGRect) -> [Double] { [Double(r.minX), Double(r.minY), Double(r.width), Double(r.height)] }
