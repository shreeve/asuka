// Debug automation, for development, the e2e tests and conformance:
//
//   - ASUKA_SCRIPT runs steps, separated by ";", once the window has shown,
//     each after the last settles:
//       - tap:<text> presses what shows the text: a button, a toggle, a
//         segment, a sidebar's row, a tab, a toolbar's item, a view that
//         takes presses (or holds one); a field by its placeholder takes
//         the focus
//       - type:<text> types into the field with the focus; submit presses
//         Return in it
//       - key:<shortcut> presses keys in the window in use (cmd+k,
//         shift+tab, escape, f5); ctrl and primary are ⌘, as on other
//         systems they're the shortcut key, and macctrl is ⌃
//       - back pops the screen, escape presses Escape, close closes the
//         window in use, as its close button
//       - scroll:<y> scrolls the innermost scroll view showing to y points
//       - resize:<w>,<h> sizes the main window's content
//       - menu:<title> chooses the menu bar's item, tab:<title> a tab
//       - wait:<duration> waits (500ms, 2s), snap:<file.png> writes a
//         snapshot
//       - waitfor:<file> waits until the file exists (a minute at most)
//       - until:<text> waits until the window shows text (a minute at most)
//     then ASUKA_SNAPSHOT is written, if it's set.
//   - ASUKA_SNAPSHOT=file.png writes the window as a PNG once it has shown,
//     without taking focus (ASUKA_DUMP=1 writes the view tree beside it:
//     file.png.txt), then quits (unless ASUKA_SNAPSHOT_STAY).
//   - ASUKA_COMMANDS=dir: commands written to dir/asuka-commands.txt, one a
//     line, are run and the file removed: the script's steps as
//     "<verb> <arg>", and dump, which writes what the screen shows (each
//     text, field's text, button's title or accessibility label, a line
//     each, in creation order) to dir/asuka-dump.txt; json, which writes
//     the view tree, the windows, the menu bar and the tray as JSON to
//     dir/asuka-tree.json (the host's tests read it); snap <file> writes
//     a snapshot. Each command's errors go to dir/asuka-errors.txt.
//   - asuka dev's error overlay (DevError.swift) is in each: tap presses its
//     buttons, the dumps have its texts, the JSON its devError.
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

@MainActor final class AsukaAutomation {
    static let shared = AsukaAutomation()
    var rt: AsukaRuntime { AsukaRuntime.shared }
    let env = ProcessInfo.processInfo.environment

    /// Whether the app runs for automation (a snapshot, a script, the
    /// commands): it keeps nothing in the user's defaults, takes no focus.
    var active: Bool { env["ASUKA_SNAPSHOT"] != nil || env["ASUKA_SCRIPT"] != nil || env["ASUKA_COMMANDS"] != nil }

    func start() {
        if env["ASUKA_PRINT_WINDOW"] != nil { print("asuka: window", rt.main.window.windowNumber) }
        if let dir = env["ASUKA_COMMANDS"], !dir.isEmpty { watchCommands(dir) }
        let script = env["ASUKA_SCRIPT"] ?? "", path = env["ASUKA_SNAPSHOT"] ?? ""
        guard !script.isEmpty || !path.isEmpty else { return }
        Task { @MainActor in
            try? await Task.sleep(nanoseconds: 1_500_000_000) // images and fonts load
            if !script.isEmpty { await self.run(script) }
            guard !path.isEmpty else { return }
            self.snapshot(path)
            if self.env["ASUKA_DUMP"] != nil { try? (self.dump() + AsukaShell.shared.trayDump).write(toFile: path + ".txt", atomically: true, encoding: .utf8) }
            if self.env["ASUKA_SNAPSHOT_STAY"] == nil { exit(0) }
        }
    }

    // MARK: Scripts

    func run(_ script: String) async {
        for raw in script.split(separator: ";") {
            let s = raw.trimmingCharacters(in: .whitespaces)
            guard !s.isEmpty else { continue }
            if let err = await step(s) { FileHandle.standardError.write(Data("asuka: script: \(s): \(err)\n".utf8)) }
        }
    }

    /// Runs one step; its error, if it has one.
    func step(_ s: String) async -> String? {
        let parts = s.split(separator: ":", maxSplits: 1)
        let verb = String(parts.first ?? ""), arg = parts.count > 1 ? String(parts[1]) : ""
        switch verb {
        case "wait":
            if let d = asukaDuration(arg) { try? await Task.sleep(nanoseconds: UInt64(d * 1e9)) }
            return nil
        case "waitfor":
            for _ in 0..<1200 where !FileManager.default.fileExists(atPath: arg) { try? await Task.sleep(nanoseconds: 50_000_000) }
            return nil
        case "until":
            // Until the window shows text (a minute at most): what the app
            // does on its own, a host's answer, comes in its time.
            for _ in 0..<1200 {
                if dump().contains(arg) { break }
                try? await Task.sleep(nanoseconds: 50_000_000)
            }
            return nil
        default:
            let err = now(verb, arg)
            try? await Task.sleep(nanoseconds: 400_000_000) // animations settle
            return err
        }
    }

    /// Runs a step that doesn't wait: its error, if it has one.
    func now(_ verb: String, _ arg: String) -> String? {
        switch verb {
        case "tap": return tap(arg)
        case "type":
            guard let n = focusedField() ?? firstField() else { return "no field has the focus" }
            if n.view?.window?.firstResponder == nil || focusedField() == nil { focus(n) }
            rt.insertText(n, arg)
        case "submit": return pressKeys("enter")
        case "key": return pressKeys(arg)
        case "escape": return pressKeys("escape")
        case "back": rt.back(rt.keyWin)
        case "close": rt.keyWin.window.performClose(nil)
        case "scroll":
            guard let y = Double(arg) else { return "want scroll:<y>" }
            var s: AsukaNode?
            for n in rt.nodes.values where (n.kind == .scroll || n.kind == .list) && showing(n) && (s == nil || n.id > s!.id) { s = n }
            guard let s else { return "no scroll view shows" }
            rt.scrollToOffset(s, 0, CGFloat(y), false)
        case "resize":
            let wh = arg.split(separator: ",")
            guard wh.count == 2, let w = Double(wh[0]), let h = Double(wh[1]) else { return "want resize:<width>,<height>" }
            rt.main.window.setContentSize(CGSize(width: w, height: h))
        case "menu": return chooseMenu(NSApp.mainMenu, arg)
        case "tab": return rt.tapTab(arg) ? nil : "no tab \(arg)"
        case "snap":
            snapshot(arg)
            if env["ASUKA_DUMP"] != nil { try? (dump() + AsukaShell.shared.trayDump).write(toFile: arg + ".txt", atomically: true, encoding: .utf8) }
        default:
            if let err = rt.modules2Step(verb, arg) { return err } // access, sheet, closepicker (Modules2.swift)
            // The shell's (trayclick, traymenu, dockmenu, reopen, hotkey, panel):
            // "" done.
            if let r = AsukaShell.shared.step(verb, arg) { return r.isEmpty ? nil : r }
            // Navigation's and windows' (pick, alerttype, alertreturn, cancel,
            // dismiss, search, snackhover, closewindow, focuswindow, action):
            // "" done (Navigation.swift).
            if let r = rt.navStep(verb, arg) { return r.isEmpty ? nil : r }
            // Lists' and text's (select, mark, undo, paste, link, click,
            // rowaction, pull, endscroll, dot, selectall, ...): Text.swift.
            if let r = rt.listTextStep(verb, arg) { return r }
            // The visual part's (mouse, hover, hoverprobe, appactive, hide,
            // a11ypress, pasteboard, dragging, fakedrag, dragmask, enddrag,
            // nativeemit, module): "" done (VisualAutomation.swift).
            if let r = rt.visualStep(verb, arg) { return r.isEmpty ? nil : r }
            return "unknown step"
        }
        return nil
    }

    /// Whether n's view is in a window that shows, and neither it nor what
    /// holds it is hidden.
    func showing(_ n: AsukaNode) -> Bool {
        guard let v = n.view, !v.isHiddenOrHasHiddenAncestor, let w = v.window else { return false }
        return w.isVisible
    }

    /// Presses what shows text, as a click on it would.
    func tap(_ text: String) -> String? {
        // asuka dev's error overlay is over everything (DevError.swift).
        if let r = AsukaDevErrors.shared.tap(text) { return r.isEmpty ? nil : r }
        var found: AsukaNode?
        for n in rt.nodes.values where showing(n) {
            let match = n.text == text && n.kind != .textField || n.title == text && n.kind == .button
                || n.kind == .textField && n.placeholder == text || n.a11yLabel == text
            if match && (found == nil || n.id > found!.id) { found = n }
        }
        // AppKit's own: a sidebar's row, a segment, a tab, a toolbar item;
        // over a text that takes no presses (a title the content repeats).
        func appKit() -> Bool { rt.tapSidebar(text) || tapSegment(text) || rt.tapTab(text) || rt.tapToolbar(text) }
        guard let f = found else { return appKit() ? nil : "nothing shows \"\(text)\"" }
        var t: AsukaNode? = f
        while let x = t {
            if x.kind == .textField {
                focus(x)
                return nil
            }
            if x.kind == .button || x.kind == .toggle {
                if x.menu != nil && x.menuTrigger == AsukaMenuTrigger.tap.rawValue { return "\"\(text)\" opens a menu, which would hold the script" }
                (x.view as? NSButton)?.performClick(nil)
                if let s = x.view as? NSSwitch { s.state = s.state == .on ? .off : .on; rt.act(s) }
                return nil
            }
            if x.listening(.press) || x.listening(.pressed) {
                rt.emit(x, .pressed) { AsukaEvents.pressed($0, true) }
                if rt.alive(x) { rt.emit(x, .press) { AsukaEvents.press($0) } }
                if rt.alive(x) { rt.emit(x, .pressed) { AsukaEvents.pressed($0, false) } }
                return nil
            }
            t = x.parent
        }
        return appKit() ? nil : "\"\(text)\" takes no presses"
    }

    func focus(_ n: AsukaNode) {
        guard let v = n.textView ?? n.view else { return }
        v.window?.makeFirstResponder(v)
    }

    /// Chooses a segmented control's segment by its label.
    func tapSegment(_ text: String) -> Bool {
        for n in rt.nodes.values where n.kind == .segmented && showing(n) {
            guard let s = n.view as? NSSegmentedControl else { continue }
            for i in 0..<s.segmentCount where s.label(forSegment: i) == text {
                s.selectedSegment = i
                rt.act(s)
                return true
            }
        }
        return false
    }

    /// The text field a window's first responder edits: the front window's
    /// that has one (a sheet's, over its window).
    func focusedField() -> AsukaNode? {
        let ws = [NSApp.keyWindow].compactMap { $0 } + NSApp.orderedWindows
        for w in ws where w.isVisible {
            var r: NSResponder? = w.firstResponder
            if let tv = r as? NSTextView, tv.isFieldEditor { r = tv.delegate as? NSResponder } // a one-line field's editor: the field
            var v = r as? NSView
            while let x = v {
                if let n = rt.node(of: x), n.kind == .textField { return n }
                v = x.superview
            }
        }
        return nil
    }

    /// The first text field on screen.
    func firstField() -> AsukaNode? {
        rt.nodes.values.filter { $0.kind == .textField && showing($0) }.min { $0.id < $1.id }
    }

    /// Performs the item titled title, in menu or its submenus.
    func chooseMenu(_ menu: NSMenu?, _ title: String) -> String? {
        guard let menu else { return "no menu bar" }
        for (i, it) in menu.items.enumerated() {
            if let sub = it.submenu {
                sub.update()
                if chooseMenu(sub, title) == nil { return nil }
                continue
            }
            if it.title == title {
                if !it.isEnabled { return "\"\(title)\" is disabled" }
                menu.performActionForItem(at: i)
                return nil
            }
        }
        return "no menu item \"\(title)\""
    }

    /// Presses a shortcut (cmd+shift+k) in the window in use, as the window
    /// server would send it: the menu bar's key equivalents, the key monitor
    /// and the first responder see it.
    func pressKeys(_ shortcut: String) -> String? {
        let parts = shortcut.lowercased().split(separator: "+").map(String.init)
        var flags: NSEvent.ModifierFlags = []
        for m in parts.dropLast() {
            switch m {
            case "cmd", "command", "ctrl", "control", "primary": flags.insert(.command)
            case "macctrl": flags.insert(.control)
            case "shift": flags.insert(.shift)
            case "alt", "option", "opt": flags.insert(.option)
            default: return "no modifier \(m)"
            }
        }
        guard let key = parts.last else { return "no key" }
        var code: UInt16 = 0xffff
        var chars = ""
        if let k = asukaScriptKeys[key] {
            code = k.0
            chars = k.1
        } else if key.count == 1 {
            chars = key
            if let c = asukaCharCodes[key.first!] { code = c }
        } else {
            return "no key \(key)"
        }
        let w = AsukaDevErrors.shared.panel.flatMap { $0.isVisible ? $0 : nil } ?? rt.keyWin.window // asuka dev's overlay takes keys
        let typed = flags.contains(.shift) ? chars.uppercased() : chars
        for type in [NSEvent.EventType.keyDown, .keyUp] {
            guard let ev = NSEvent.keyEvent(with: type, location: .zero, modifierFlags: flags, timestamp: ProcessInfo.processInfo.systemUptime,
                                            windowNumber: w.windowNumber, context: nil, characters: typed, charactersIgnoringModifiers: chars,
                                            isARepeat: false, keyCode: code) else { continue }
            NSApp.postEvent(ev, atStart: false)
        }
        return nil
    }

    // MARK: Snapshots

    /// Writes the window's content as a PNG, and each tray icon's beside it.
    func snapshot(_ path: String) {
        var w = rt.main.window
        if !w.isVisible { w = rt.keyWin.window } // the main window hidden: the one in use
        guard var v = w.contentView else { return }
        let content = path.hasSuffix(".content.png") || path.hasSuffix(".content.display.png")
        if !content, let f = v.superview { v = f } // the frame view: the title bar and toolbar too
        // The window server's image, as the window shows: a view can't draw
        // what the system composites (Liquid Glass, a sidebar's or a bar's
        // material). Drawn by the view when the window server gives none.
        if !writeWindowPNG(w, v, path) { writeViewPNG(v, path) }
        AsukaShell.shared.traySnapshots(path)
    }

    /// w and what shows over it, front first: its sheets and its child
    /// windows, theirs in turn.
    func windowStack(_ w: NSWindow) -> [CGWindowID] {
        var out: [CGWindowID] = []
        if let s = w.attachedSheet, s.isVisible { out += windowStack(s) }
        for c in w.childWindows ?? [] where c.isVisible { out = windowStack(c) + out }
        return out + [CGWindowID(w.windowNumber)]
    }

    func writeWindowPNG(_ w: NSWindow, _ v: NSView, _ path: String) -> Bool {
        guard w.isVisible, let capture = asukaWindowListCreateImageFromArray else { return false }
        var ids = windowStack(w).map { UnsafeRawPointer(bitPattern: UInt($0)) }
        guard let arr = CFArrayCreate(nil, &ids, ids.count, nil) else { return false }
        // The window's own bounds, in the window server's space (from the
        // main display's top left).
        let f = w.frame
        let top = NSScreen.screens.first?.frame.height ?? f.maxY
        let bounds = CGRect(x: f.minX, y: top - f.minY - f.height, width: f.width, height: f.height)
        guard var img = capture(bounds, arr, 1 << 0 | 1 << 3)?.takeRetainedValue() else { return false } // ignoring framing, best resolution
        if let frame = w.contentView?.superview, v !== frame {
            let fb = frame.bounds
            let k = CGFloat(img.width) / fb.width
            var r = v.convert(v.bounds, to: frame)
            if !frame.isFlipped { r.origin.y = fb.height - r.minY - r.height }
            guard let part = img.cropping(to: CGRect(x: r.minX * k, y: r.minY * k, width: r.width * k, height: r.height * k)) else { return false }
            img = part
        }
        // In sRGB: the window server's image is in the display's color
        // space (a P3 display's red isn't sRGB's), and what's compared is
        // the app's colors as it gave them. A path ending .display.png
        // keeps the display's: the window server blends there (a
        // half-transparent view over another), so a blend is measured
        // there (conformance), converting it would bend it.
        if !path.hasSuffix(".display.png"), let srgb = CGColorSpace(name: CGColorSpace.sRGB), img.colorSpace?.name != CGColorSpace.sRGB,
           let ctx = CGContext(data: nil, width: img.width, height: img.height, bitsPerComponent: 8, bytesPerRow: 0, space: srgb,
                               bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) {
            ctx.interpolationQuality = .none
            ctx.draw(img, in: CGRect(x: 0, y: 0, width: img.width, height: img.height))
            if let converted = ctx.makeImage() { img = converted }
        }
        let rep = NSBitmapImageRep(cgImage: img)
        guard let data = rep.representation(using: .png, properties: [:]) else { return false }
        return (try? data.write(to: URL(fileURLWithPath: path))) != nil
    }

    func writeViewPNG(_ v: NSView, _ path: String) {
        guard let rep = v.bitmapImageRepForCachingDisplay(in: v.bounds) else { return }
        v.cacheDisplay(in: v.bounds, to: rep)
        try? rep.representation(using: .png, properties: [:])?.write(to: URL(fileURLWithPath: path))
    }

    // MARK: Dumps

    /// Describes the native view tree: each node's kind, frame and what it
    /// shows; then the windows.
    func dump() -> String {
        var b = ""
        func walk(_ n: AsukaNode, _ depth: Int) {
            b += String(repeating: "  ", count: depth)
            b += "\(n.id) \(n.kind?.name ?? "Cell")"
            if let v = n.view {
                let f = v.frame
                b += String(format: " [%.0f,%.0f %.0fx%.0f]", f.minX, f.minY, f.width, f.height)
                if v.isHidden { b += " hidden" }
                if v.superview == nil { b += " detached" }
                if v.window == nil { b += " nowindow" }
                if v.userInterfaceLayoutDirection == .rightToLeft { b += " rtl" }
            }
            if !n.text.isEmpty { b += " \"\(n.text)\"" }
            if !n.title.isEmpty { b += " title=\"\(n.title)\"" }
            b += "\n"
            for c in n.children { walk(c, depth + 1) }
            for (row, cells) in rt.listRows(n) {
                for c in cells {
                    b += String(repeating: "  ", count: depth + 1) + "row \(row):\n"
                    walk(c, depth + 2)
                }
            }
        }
        if let r = rt.root { walk(r, 0) }
        for w in rt.wins.values { if let r = w.root, r !== rt.root, r.parent == nil { walk(r, 0) } }
        return b + AsukaDevErrors.shared.dump() + dumpWindows()
    }

    /// The app's windows that show, front to back: their titles, the
    /// document's dot and file, and the File menu.
    func dumpWindows() -> String {
        var b = ""
        for nw in NSApp.orderedWindows where rt.win(of: nw) != nil && nw.isVisible {
            b += "window \"\(nw.title)\""
            if nw.isDocumentEdited { b += " edited" }
            if let u = nw.representedURL { b += " file=\"\(u.path)\"" }
            b += "\n"
        }
        if let bar = NSApp.mainMenu, bar.numberOfItems > 1, let m = bar.item(at: 1)?.submenu {
            b += "File:"
            for it in m.items {
                if it.isSeparatorItem { b += " |"; continue }
                b += " \"\(it.title)\""
                if !it.keyEquivalent.isEmpty { b += "(\(it.keyEquivalent))" }
            }
            b += "\n"
        }
        return b
    }

    /// What the screen shows: each shown node's text (a field's too), else
    /// a button's title, else its accessibility label, a line each, in
    /// creation order.
    func screenTexts() -> String {
        var lines: [String] = []
        for n in rt.nodes.values.sorted(by: { $0.id < $1.id }) where showing(n) {
            if !n.text.isEmpty { lines.append(n.text) }
            else if n.kind == .button && !n.title.isEmpty { lines.append(n.title) }
            else if !n.a11yLabel.isEmpty { lines.append(n.a11yLabel) }
        }
        lines += AsukaDevErrors.shared.texts() // asuka dev's overlay, over them
        return lines.joined(separator: "\n") + "\n"
    }

    /// The view tree, the windows, the menu bar and the tray, as JSON.
    func tree() -> [String: Any] {
        var roots: [[String: Any]] = []
        if let r = rt.root { roots.append(nodeJSON(r)) }
        // Other windows' content (a Window's), after the main window's, as
        // the dump has it.
        for w in rt.wins.values { if let r = w.root, r !== rt.root, r.parent == nil { roots.append(nodeJSON(r)) } }
        // What's presented (sheets, alerts, popovers, snackbars): no view's
        // child (Navigation.swift).
        for n in rt.presentedRoots() { roots.append(nodeJSON(n)) }
        var windows: [[String: Any]] = []
        for w in rt.allWins { windows.append(windowJSON(w)) }
        var out: [String: Any] = ["roots": roots, "windows": windows, "menuBar": menuJSON(NSApp.mainMenu)]
        out["environment"] = ["width": rt.env.width, "height": rt.env.height, "scale": rt.env.scale, "dark": rt.env.dark,
                              "safeTop": rt.env.safeTop, "locale": rt.env.locale]
        out["keyWindow"] = NSApp.keyWindow?.title ?? ""
        if let e = AsukaDevErrors.shared.json() { out["devError"] = e }
        AsukaShell.shared.dumpJSON(&out)
        rt.modules1JSON(&out)
        rt.modules2JSON(&out)
        return out
    }

    func nodeJSON(_ n: AsukaNode) -> [String: Any] {
        var d: [String: Any] = ["id": n.id, "kind": n.kind?.name ?? "Cell"]
        if let v = n.view {
            d["class"] = String(describing: type(of: v))
            d["frame"] = rectJSON(v.frame)
            if let w = v.window, let c = w.contentView { d["inWindow"] = rectJSON(v.convert(v.bounds, to: c)) }
            d["hidden"] = v.isHiddenOrHasHiddenAncestor
            d["shown"] = showing(n)
            d["alpha"] = Double(v.alphaValue)
            if let w = v.window { d["window"] = w.title }
            if v.userInterfaceLayoutDirection == .rightToLeft { d["rtl"] = true }
            if let l = v.layer {
                if let bg = l.backgroundColor { d["layerBackground"] = cgColorHex(bg) }
                if l.cornerRadius != 0 { d["layerCornerRadius"] = Double(l.cornerRadius) }
                if l.borderWidth != 0 { d["layerBorderWidth"] = Double(l.borderWidth) }
                if l.shadowOpacity != 0 { d["layerShadowRadius"] = Double(l.shadowRadius) }
                if l.masksToBounds { d["layerMasks"] = true }
            }
            if let c = v as? NSControl {
                d["enabled"] = c.isEnabled
                d["stringValue"] = c.stringValue
                if let f = c.font { d["font"] = ["size": Double(f.pointSize), "name": f.fontName] }
                d["alignment"] = c.alignment.rawValue
            }
            if let t = v as? NSTextField, let col = t.textColor { d["textColor"] = colorHex(col) }
            if let b = v as? NSButton {
                d["title"] = b.title
                d["state"] = b.state.rawValue
                d["bordered"] = b.isBordered
                d["bezelStyle"] = b.bezelStyle.rawValue
                if b.image != nil { d["hasImage"] = true }
                if b.attributedTitle.length > 0, let c = b.attributedTitle.attribute(.foregroundColor, at: 0, effectiveRange: nil) as? NSColor {
                    d["titleColor"] = colorHex(c)
                }
                if (b.cell as? AsukaButtonCell)?.titleAsGiven == true { d["titleAsGiven"] = true }
            }
            if let s = v as? NSSwitch { d["state"] = s.state.rawValue }
            if let s = v as? NSSlider { d["doubleValue"] = s.doubleValue; d["min"] = s.minValue; d["max"] = s.maxValue }
            if let s = v as? NSStepper { d["doubleValue"] = s.doubleValue; d["min"] = s.minValue; d["max"] = s.maxValue }
            if let s = v as? NSSegmentedControl {
                d["segments"] = (0..<s.segmentCount).map { s.label(forSegment: $0) ?? "" }
                d["selectedSegment"] = s.selectedSegment
            }
            if let p = v as? NSPopUpButton {
                d["items"] = p.itemTitles
                d["selectedIndex"] = p.indexOfSelectedItem
                d["titleOfSelected"] = p.titleOfSelectedItem ?? ""
            }
            if let p = v as? NSProgressIndicator { d["doubleValue"] = p.doubleValue; d["indeterminate"] = p.isIndeterminate }
            if let dp = v as? NSDatePicker { d["dateValue"] = dp.dateValue.timeIntervalSince1970 * 1000 }
            if let tv = n.textView { d["textViewString"] = tv.string }
            if let a = v.accessibilityLabel(), !a.isEmpty { d["a11yLabel"] = a }
            if let r = v.accessibilityRole() { d["a11yRole"] = r.rawValue }
            d["a11yElement"] = v.isAccessibilityElement()
            if let t = v.toolTip { d["tooltip"] = t }
        }
        if !n.text.isEmpty { d["text"] = n.text }
        if !n.title.isEmpty { d["nodeTitle"] = n.title }
        if !n.placeholder.isEmpty { d["placeholder"] = n.placeholder }
        if n.menu != nil { d["menu"] = menuJSON(n.menu) }
        rt.partJSON(n, &d)
        var kids: [[String: Any]] = n.children.map { nodeJSON($0) }
        for (row, cells) in rt.listRows(n) {
            for c in cells {
                var j = nodeJSON(c)
                j["row"] = row
                kids.append(j)
            }
        }
        d["children"] = kids
        return d
    }

    func windowJSON(_ w: AsukaWin) -> [String: Any] {
        let nw = w.window
        var d: [String: Any] = ["title": nw.title, "visible": nw.isVisible, "key": nw.isKeyWindow, "main": w.isMain,
                                "frame": rectJSON(nw.frame), "content": rectJSON(w.content.bounds),
                                "styleMask": nw.styleMask.rawValue, "level": nw.level.rawValue, "miniaturized": nw.isMiniaturized,
                                "zoomed": nw.isZoomed, "edited": nw.isDocumentEdited, "opaque": nw.isOpaque,
                                "minSize": sizeJSON(nw.contentMinSize), "maxSize": sizeJSON(nw.contentMaxSize),
                                "class": String(describing: type(of: nw))]
        if let u = nw.representedURL { d["file"] = u.path }
        d["sharingType"] = nw.sharingType.rawValue // platform/screen's preventCapture
        if let tb = nw.toolbar {
            d["toolbar"] = tb.items.map { it -> [String: Any] in
                ["id": it.itemIdentifier.rawValue, "label": it.label, "enabled": it.isEnabled, "toolTip": it.toolTip ?? ""]
            }
        }
        if let s = nw.attachedSheet { d["sheet"] = ["title": s.title, "frame": rectJSON(s.frame), "class": String(describing: type(of: s))] }
        if let p = nw.parent { d["parent"] = p.title }
        rt.windowPartJSON(w, &d)
        return d
    }

    func menuJSON(_ m: NSMenu?) -> [[String: Any]] {
        guard let m else { return [] }
        return m.items.map { it in
            var d: [String: Any] = ["title": it.title]
            if it.isSeparatorItem { d["separator"] = true }
            if !it.keyEquivalent.isEmpty {
                d["key"] = it.keyEquivalent
                d["mods"] = it.keyEquivalentModifierMask.rawValue
            }
            if !it.isEnabled { d["disabled"] = true }
            if it.state == .on { d["checked"] = true }
            if it.isHidden { d["hidden"] = true }
            if it.image != nil { d["hasImage"] = true }
            if let a = it.attributedTitle, a.length > 0, let c = a.attribute(.foregroundColor, at: 0, effectiveRange: nil) as? NSColor { d["color"] = colorHex(c) }
            if let s = it.submenu { d["items"] = menuJSON(s) }
            return d
        }
    }

    func rectJSON(_ r: CGRect) -> [Double] { [Double(r.minX), Double(r.minY), Double(r.width), Double(r.height)] }
    func sizeJSON(_ s: CGSize) -> [Double] { [Double(s.width), Double(s.height)] }

    // MARK: Commands

    func watchCommands(_ dir: String) {
        let file = URL(fileURLWithPath: dir).appendingPathComponent("asuka-commands.txt")
        Task { @MainActor in
            while true {
                try? await Task.sleep(nanoseconds: 50_000_000)
                guard let text = try? String(contentsOf: file, encoding: .utf8) else { continue }
                try? FileManager.default.removeItem(at: file)
                for raw in text.split(separator: "\n") {
                    let line = raw.trimmingCharacters(in: .whitespaces)
                    guard !line.isEmpty else { continue }
                    let parts = line.split(separator: " ", maxSplits: 1)
                    let verb = String(parts[0]), arg = parts.count > 1 ? String(parts[1]) : ""
                    var err: String?
                    switch verb {
                    case "dump":
                        write(screenTexts(), dir, "asuka-dump.txt")
                    case "tree":
                        write(dump(), dir, "asuka-tree.txt")
                    case "json":
                        if let data = try? JSONSerialization.data(withJSONObject: tree(), options: [.prettyPrinted, .sortedKeys]) {
                            write(String(decoding: data, as: UTF8.self), dir, "asuka-tree.json")
                        }
                    default:
                        err = await step(verb + ":" + arg)
                    }
                    if let err { appendError("\(line): \(err)", dir) }
                }
                write("", dir, "asuka-done.txt")
            }
        }
    }

    /// Writes a file whole: to a part, then moved, so a reader never reads
    /// half of it.
    func write(_ s: String, _ dir: String, _ name: String) {
        let u = URL(fileURLWithPath: dir).appendingPathComponent(name)
        let part = u.appendingPathExtension("part")
        try? s.write(to: part, atomically: false, encoding: .utf8)
        _ = try? FileManager.default.replaceItemAt(u, withItemAt: part)
        if !FileManager.default.fileExists(atPath: u.path) { try? FileManager.default.moveItem(at: part, to: u) }
    }

    func appendError(_ s: String, _ dir: String) {
        FileHandle.standardError.write(Data("asuka: automation: \(s)\n".utf8))
        let u = URL(fileURLWithPath: dir).appendingPathComponent("asuka-errors.txt")
        let old = (try? String(contentsOf: u, encoding: .utf8)) ?? ""
        try? (old + s + "\n").write(to: u, atomically: true, encoding: .utf8)
    }
}

/// "500ms", "2s", "1m" as seconds.
func asukaDuration(_ s: String) -> Double? {
    for (suffix, k) in [("ms", 0.001), ("s", 1.0), ("m", 60.0)] where s.hasSuffix(suffix) {
        if let v = Double(s.dropLast(suffix.count)) { return v * k }
    }
    return Double(s)
}

/// An NSColor as #rrggbbaa in sRGB.
func colorHex(_ c: NSColor) -> String {
    guard let s = c.usingColorSpace(.sRGB) else { return "" }
    func h(_ v: CGFloat) -> String { String(format: "%02x", Int((min(max(v, 0), 1) * 255).rounded())) }
    return "#" + h(s.redComponent) + h(s.greenComponent) + h(s.blueComponent) + h(s.alphaComponent)
}

func cgColorHex(_ c: CGColor) -> String {
    guard let n = NSColor(cgColor: c) else { return "" }
    return colorHex(n)
}

/// Key codes (a US keyboard's) and the characters they type.
let asukaScriptKeys: [String: (UInt16, String)] = [
    "enter": (0x24, "\r"), "return": (0x24, "\r"), "escape": (0x35, "\u{1b}"), "esc": (0x35, "\u{1b}"),
    "tab": (0x30, "\t"), "space": (0x31, " "), "backspace": (0x33, "\u{7f}"), "delete": (0x75, "\u{f728}"),
    "up": (0x7e, "\u{f700}"), "down": (0x7d, "\u{f701}"), "left": (0x7b, "\u{f702}"), "right": (0x7c, "\u{f703}"),
    "home": (0x73, "\u{f729}"), "end": (0x77, "\u{f72b}"), "pageup": (0x74, "\u{f72c}"), "pagedown": (0x79, "\u{f72d}"),
    "f1": (0x7a, "\u{f704}"), "f2": (0x78, "\u{f705}"), "f3": (0x63, "\u{f706}"), "f4": (0x76, "\u{f707}"),
    "f5": (0x60, "\u{f708}"), "f6": (0x61, "\u{f709}"), "f7": (0x62, "\u{f70a}"), "f8": (0x64, "\u{f70b}"),
    "f9": (0x65, "\u{f70c}"), "f10": (0x6d, "\u{f70d}"), "f11": (0x67, "\u{f70e}"), "f12": (0x6f, "\u{f70f}"),
]

/// The keys of a US keyboard's characters.
let asukaCharCodes: [Character: UInt16] = [
    "a": 0, "s": 1, "d": 2, "f": 3, "h": 4, "g": 5, "z": 6, "x": 7, "c": 8, "v": 9, "b": 11, "q": 12,
    "w": 13, "e": 14, "r": 15, "y": 16, "t": 17, "1": 18, "2": 19, "3": 20, "4": 21, "6": 22, "5": 23,
    "=": 24, "9": 25, "7": 26, "-": 27, "8": 28, "0": 29, "]": 30, "o": 31, "u": 32, "[": 33, "i": 34,
    "p": 35, "l": 37, "j": 38, "'": 39, "k": 40, ";": 41, "\\": 42, ",": 43, "/": 44, "n": 45, "m": 46,
    ".": 47, "`": 50,
]

/// CGWindowListCreateImageFromArray: gone from the SDK's headers since
/// macOS 15, still in the library.
let asukaWindowListCreateImageFromArray: (@convention(c) (CGRect, CFArray, UInt32) -> Unmanaged<CGImage>?)? = {
    guard let h = dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics", RTLD_LAZY),
          let f = dlsym(h, "CGWindowListCreateImageFromArray") else { return nil }
    return unsafeBitCast(f, to: (@convention(c) (CGRect, CFArray, UInt32) -> Unmanaged<CGImage>?).self)
}()
