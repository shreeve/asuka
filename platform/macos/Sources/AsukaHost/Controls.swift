// AppKit's controls for Asuka's: NSTextField (labels and fields; secure ones
// NSSecureTextField, multiline ones an NSTextView: TextInput.swift),
// NSButton (Plain is borderless text in the accent color, Filled the accent
// bezel), NSSwitch, NSSlider, NSStepper, NSSegmentedControl, NSPopUpButton
// for Wheel and Picker, NSDatePicker, NSColorWell, NSProgressIndicator,
// NSScrollView. The other kinds' views are their files'.
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

@MainActor extension AsukaRuntime {
    /// Makes the view of a kind that isn't navigation's.
    func makeView(_ n: AsukaNode, _ kind: AsukaKind) -> NSView {
        switch kind {
        case .view, .selectable, .native:
            // A Native's view holds the app's (Native.swift); a Selectable's
            // texts select together (RichText.swift).
            return AsukaView(frame: .zero)
        case .accessory:
            // A field's bar over the keyboard: a Mac has no on-screen
            // keyboard, so it's made and never shown.
            return AsukaView(frame: .zero)
        case .text:
            let t = AsukaLabel(frame: .zero)
            t.isEditable = false
            t.isSelectable = false
            t.isBezeled = false
            t.drawsBackground = false
            t.lineBreakMode = .byWordWrapping
            t.cell?.wraps = true
            t.cell?.isScrollable = false
            t.font = styleFont(0)
            t.textColor = .labelColor
            return t
        case .button:
            let b = NSButton(frame: .zero)
            b.cell = AsukaButtonCell(textCell: "")
            b.bezelStyle = .rounded
            b.setButtonType(.momentaryPushIn)
            b.title = ""
            b.target = self
            b.action = #selector(act(_:))
            return b
        case .textField:
            return makeField(n, secure: false)
        case .toggle:
            let s = NSSwitch(frame: .zero)
            s.target = self
            s.action = #selector(act(_:))
            return s
        case .slider:
            let s = NSSlider(frame: .zero)
            s.minValue = 0
            s.maxValue = 1
            s.isContinuous = true
            s.target = self
            s.action = #selector(act(_:))
            n.maxValue = 1
            return s
        case .stepper:
            let s = NSStepper(frame: .zero)
            s.maxValue = 100
            s.target = self
            s.action = #selector(act(_:))
            return s
        case .scroll:
            let s = AsukaScrollView(frame: .zero)
            s.hasVerticalScroller = true
            s.autohidesScrollers = true
            s.drawsBackground = false
            s.contentView.drawsBackground = false
            s.automaticallyAdjustsContentInsets = false
            return s
        case .activity:
            let p = NSProgressIndicator(frame: .zero)
            p.style = .spinning
            p.isIndeterminate = true
            p.isDisplayedWhenStopped = false
            p.controlSize = .regular
            return p
        case .progress:
            let p = NSProgressIndicator(frame: .zero)
            p.style = .bar
            p.isIndeterminate = false
            p.minValue = 0
            p.maxValue = 1
            return p
        case .segmented:
            let s = NSSegmentedControl(frame: .zero)
            s.trackingMode = .selectOne
            s.target = self
            s.action = #selector(act(_:))
            return s
        case .wheel, .picker:
            // macOS picks one of many from a pop-up button.
            let p = NSPopUpButton(frame: .zero, pullsDown: false)
            p.target = self
            p.action = #selector(act(_:))
            if kind == .picker { n.selected = -1 } // none, till the core says
            return p
        case .datePicker:
            let d = NSDatePicker(frame: .zero)
            d.datePickerStyle = .textFieldAndStepper
            d.datePickerElements = .yearMonthDay
            d.isBezeled = true
            d.drawsBackground = true
            d.target = self
            d.action = #selector(act(_:))
            return d
        case .colorPicker:
            _ = AsukaColorWell.panelTitle // before a well names it
            let w = AsukaColorWell(frame: .zero)
            if #available(macOS 13.0, *) { w.colorWellStyle = .minimal } // a swatch that opens the picker
            w.target = self
            w.action = #selector(act(_:))
            return w
        case .image, .symbol:
            return makeImage(n, kind)
        case .list:
            return makeList(n)
        case .progressRing:
            return makeRing(n)
        case .pageControl:
            return makePageDots(n)
        case .canvas:
            return makeCanvas(n)
        case .split:
            return makeSplit(n)
        case .shader:
            return makeShader(n)
        case .richText:
            return makeRich(n)
        case .shape:
            return makeShape(n)
        case .webView:
            return makeWeb(n)
        case .video:
            return makeVideo(n)
        default:
            fatalError("asuka: the macOS host can't show \(kind.name)")
        }
    }

    /// Makes a TextField's view: a field, or a secure field.
    func makeField(_ n: AsukaNode, secure: Bool) -> NSTextField {
        let f: NSTextField
        if secure {
            f = AsukaSecureField(frame: .zero)
        } else {
            f = AsukaTextField(frame: .zero)
            // Its own cell, which puts its text inside its padding.
            f.cell = AsukaTextFieldCell(textCell: "")
        }
        f.isBezeled = true
        f.bezelStyle = .roundedBezel
        f.isEditable = true
        f.isSelectable = true
        f.delegate = self
        f.cell?.isScrollable = true
        f.cell?.usesSingleLineMode = true
        f.font = styleFont(0)
        return f
    }

    func setOwn(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        guard let kind = n.kind else { return }
        switch kind {
        case .text:
            if p == AsukaProp.Text.text {
                n.text = v.string
                (n.view as? NSTextField)?.stringValue = v.string
            }
        case .button:
            guard let b = n.view as? NSButton else { return }
            switch p {
            case AsukaProp.Button.title:
                n.title = v.string
                buttonTitle(n)
            case AsukaProp.Button.variant:
                n.variant = v.enumValue
                buttonVariant(n)
            case AsukaProp.Button.icon:
                n.icon = v.string
                if let img = symbolImage(v.string) {
                    b.image = img
                    b.imagePosition = .imageLeading
                } else {
                    b.image = nil
                }
                buttonTitle(n)
            default: break
            }
        case .textField:
            setField(n, p, v)
        case .toggle:
            switch p {
            case AsukaProp.Toggle.on:
                if let s = n.view as? NSSwitch { s.state = v.bool ? .on : .off }
                if let b = n.view as? NSButton { b.state = v.bool ? .on : .off }
            case AsukaProp.Toggle.checkbox:
                if v.bool, let s = n.view as? NSSwitch {
                    // A check box in place of its switch.
                    let c = NSButton(checkboxWithTitle: "", target: self, action: #selector(act(_:)))
                    c.state = s.state
                    c.isEnabled = s.isEnabled
                    replaceView(n, c)
                }
            default: break
            }
        case .slider:
            guard let s = n.view as? NSSlider else { return }
            switch p {
            case AsukaProp.Slider.value: s.doubleValue = v.float
            case AsukaProp.Slider.min: s.minValue = v.float
            case AsukaProp.Slider.max: s.maxValue = v.float
            default: break
            }
        case .stepper:
            guard let s = n.view as? NSStepper else { return }
            switch p {
            case AsukaProp.Stepper.value: s.doubleValue = v.float
            case AsukaProp.Stepper.min: s.minValue = v.float
            case AsukaProp.Stepper.max: s.maxValue = v.float
            case AsukaProp.Stepper.step: s.increment = v.float
            default: break
            }
        case .scroll:
            guard let s = n.view as? NSScrollView else { return }
            switch p {
            case AsukaProp.Scroll.horizontal:
                n.horizontal = v.bool
                s.hasHorizontalScroller = v.bool && !n.paging
                s.hasVerticalScroller = !v.bool && !n.paging
            case AsukaProp.Scroll.paging:
                setPaging(n, v.bool)
            case AsukaProp.Scroll.refreshable:
                setRefreshable(n, v.bool)
            case AsukaProp.Scroll.refreshing:
                setRefreshing(n, v.bool)
            default: break
            }
        case .activity:
            if p == AsukaProp.Activity.animating, let a = n.view as? NSProgressIndicator {
                if v.bool { a.startAnimation(nil) } else { a.stopAnimation(nil) }
            }
        case .progress:
            guard let a = n.view as? NSProgressIndicator else { return }
            switch p {
            case AsukaProp.Progress.value: a.doubleValue = v.float
            case AsukaProp.Progress.indeterminate:
                a.isIndeterminate = v.bool
                if v.bool { a.startAnimation(nil) } else { a.stopAnimation(nil) }
            default: break
            }
        case .segmented:
            guard let s = n.view as? NSSegmentedControl else { return }
            switch p {
            case AsukaProp.Segmented.segments:
                n.items = asukaStrings(v.data)
                s.segmentCount = n.items.count
                for (i, t) in n.items.enumerated() { s.setLabel(t, forSegment: i) }
                s.selectedSegment = n.selected
            case AsukaProp.Segmented.selected:
                n.selected = Int(v.int)
                s.selectedSegment = n.selected
            default: break
            }
        case .wheel:
            guard let b = n.view as? NSPopUpButton else { return }
            switch p {
            case AsukaProp.Wheel.items:
                n.items = asukaStrings(v.data)
                b.removeAllItems()
                // addItem(withTitle:) drops duplicate titles; menu items don't.
                for t in n.items { b.menu?.addItem(withTitle: t, action: nil, keyEquivalent: "") }
                setMenuDirection(b.menu, isRTL(n))
                b.selectItem(at: n.selected)
            case AsukaProp.Wheel.selected:
                n.selected = Int(v.int)
                b.selectItem(at: n.selected)
            default: break
            }
        case .picker:
            guard let b = n.view as? NSPopUpButton else { return }
            switch p {
            case AsukaProp.Picker.items:
                n.items = asukaStrings(v.data)
                fillPicker(n, b)
            case AsukaProp.Picker.selected:
                n.selected = Int(v.int)
                selectPicker(n, b)
            case AsukaProp.Picker.label:
                n.pickerLabel = v.string
                if b.numberOfItems == 0 { fillPicker(n, b) } else { b.item(at: 0)?.title = v.string }
                if n.a11yLabel.isEmpty { b.setAccessibilityLabel(v.string) }
                b.synchronizeTitleAndSelectedItem()
            default: break
            }
        case .datePicker:
            guard let d = n.view as? NSDatePicker else { return }
            switch p {
            case AsukaProp.DatePicker.value:
                d.dateValue = Date(timeIntervalSince1970: Double(v.int) / 1000)
            case AsukaProp.DatePicker.mode:
                switch AsukaDateMode(rawValue: v.enumValue) {
                case .time: d.datePickerElements = .hourMinute
                case .dateTime: d.datePickerElements = [.yearMonthDay, .hourMinute]
                default: d.datePickerElements = .yearMonthDay
                }
            case AsukaProp.DatePicker.inline:
                // The Mac's calendar and clock in place: a month to pick a
                // day from, an analog clock for the time.
                d.datePickerStyle = v.bool ? .clockAndCalendar : .textFieldAndStepper
            case AsukaProp.DatePicker.min:
                d.minDate = v.int != 0 ? Date(timeIntervalSince1970: Double(v.int) / 1000) : nil
            case AsukaProp.DatePicker.max:
                d.maxDate = v.int != 0 ? Date(timeIntervalSince1970: Double(v.int) / 1000) : nil
            default: break
            }
        case .colorPicker:
            guard let w = n.view as? AsukaColorWell else { return }
            switch p {
            case AsukaProp.ColorPicker.value:
                if let c = asukaColor(v.color) { w.color = c }
            case AsukaProp.ColorPicker.alpha:
                w.showsAlpha = v.bool
                if w.isActive { w.applyToPanel() }
            case AsukaProp.ColorPicker.title:
                w.panelTitle = v.string
                if w.isActive { w.applyToPanel() }
            default: break
            }
        case .view:
            switch p {
            case AsukaProp.View.dropAccept: setDropAccept(n, v.data)
            case AsukaProp.View.dragItems: setDragItems(n, v.data)
            default: break
            }
        case .image, .symbol:
            setImageProp(n, p, v)
        case .overlay:
            setOverlayProp(n, p, v) // Overlay.swift
        case .list:
            switch p {
            case AsukaProp.List.refreshable: setRefreshable(n, v.bool)
            case AsukaProp.List.refreshing: setRefreshing(n, v.bool)
            default: setListProp(n, p, v)
            }
        case .progressRing: setRingProp(n, p, v)
        case .pageControl: setPageDotsProp(n, p, v)
        case .canvas:
            if p == AsukaProp.Canvas.commands {
                n.canvas = v.data
                n.view?.needsDisplay = true
            }
        case .split: setSplitProp(n, p, v)
        case .shader: setShaderProp(n, p, v)
        case .richText: setRichProp(n, p, v)
        case .shape: setShapeProp(n, p, v)
        case .native: setNativeProp(n, p, v)
        case .webView: setWebProp(n, p, v)
        case .video: setVideoProp(n, p, v)
        default:
            setNavProp(n, p, v)
        }
    }

    // MARK: Buttons

    /// The variant a Mac's button looks like: glass is macOS 26's glass
    /// bezel; before, it's the push button, and prominent glass the default
    /// button.
    func macVariant(_ n: AsukaNode) -> AsukaButtonVariant {
        let v = AsukaButtonVariant(rawValue: n.variant) ?? .plain
        if hasGlass { return v }
        switch v {
        case .glass: return .gray
        case .glassProminent: return .filled
        default: return v
        }
    }

    /// Whether buttons can take macOS 26's glass bezel.
    var hasGlass: Bool {
        if #available(macOS 26.0, *) { return true }
        return false
    }

    /// Maps a variant to a macOS button: Plain is borderless text in the
    /// accent color, Filled the accent-colored default button, Glass and
    /// GlassProminent the glass bezel (macOS 26), the others push buttons.
    func buttonVariant(_ n: AsukaNode) {
        guard let b = n.view as? NSButton else { return }
        if #available(macOS 26.0, *) {
            b.tintProminence = .automatic
            b.borderShape = .automatic
        }
        switch macVariant(n) {
        case .plain:
            b.isBordered = false
            b.bezelColor = nil
        case .filled:
            b.isBordered = true
            b.bezelStyle = .rounded
            if n.background == 0 { b.bezelColor = accentNSColor }
        case .glass, .glassProminent:
            b.isBordered = true
            if #available(macOS 26.0, *) {
                b.bezelStyle = .glass
                b.bezelColor = nil
                if n.variant == AsukaButtonVariant.glassProminent.rawValue {
                    b.tintProminence = .primary
                    if n.background == 0 { b.bezelColor = accentNSColor }
                }
            }
        default:
            b.isBordered = true
            b.bezelStyle = .rounded
        }
        buttonTitle(n)
    }

    /// Sets a button's title, colored for a plain or tinted button (the
    /// bezel can't take a color for the text), and white on a disabled
    /// filled one, whose bezel AppKit only lightens: its gray disabled text
    /// would all but vanish on it.
    func buttonTitle(_ n: AsukaNode) {
        guard let b = n.view as? NSButton else { return }
        glassShape(n)
        var color: NSColor?
        switch macVariant(n) {
        case .plain: color = asukaColor(n.color) ?? accentNSColor
        case .tinted: color = accentNSColor
        case .filled: if !b.isEnabled && n.color == 0 { color = .alternateSelectedControlTextColor }
        default: break
        }
        (b.cell as? AsukaButtonCell)?.titleAsGiven = macVariant(n) == .filled && !b.isEnabled
        if color == nil && n.color != 0 { color = asukaColor(n.color) }
        guard let c = color else {
            // A title colored before (a filled button that was disabled)
            // takes AppKit's color again: setting the title alone leaves
            // the color it had.
            if b.attributedTitle.length > 0 { b.attributedTitle = NSAttributedString() }
            b.title = n.title
            return
        }
        b.attributedTitle = NSAttributedString(string: n.title, attributes: [.foregroundColor: c, .font: b.font ?? styleFont(0)])
    }

    /// A glass button's shape: a circle around an icon alone, else a
    /// capsule, as the system's glass buttons are.
    func glassShape(_ n: AsukaNode) {
        guard #available(macOS 26.0, *), let b = n.view as? NSButton else { return }
        let v = macVariant(n)
        guard v == .glass || v == .glassProminent else { return }
        b.borderShape = n.title.isEmpty && !n.icon.isEmpty ? .circle : .capsule
    }

    // MARK: Pickers

    /// Fills a Picker's pop-up button: first a hidden item whose title is
    /// its label, shown while none is chosen (AppKit shows the selected
    /// item's title, and a hidden item isn't in the menu), then its items.
    func fillPicker(_ n: AsukaNode, _ b: NSPopUpButton) {
        b.removeAllItems()
        guard let m = b.menu else { return }
        let label = m.addItem(withTitle: n.pickerLabel, action: nil, keyEquivalent: "")
        label.isHidden = true
        for t in n.items { m.addItem(withTitle: t, action: nil, keyEquivalent: "") }
        setMenuDirection(m, isRTL(n))
        selectPicker(n, b)
    }

    /// Selects a Picker's chosen item, or its label's.
    func selectPicker(_ n: AsukaNode, _ b: NSPopUpButton) {
        let i = n.selected < 0 || n.selected >= n.items.count ? 0 : n.selected + 1
        b.selectItem(at: i)
    }

    // MARK: Fields

    func setField(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        switch p {
        case AsukaProp.TextField.value:
            n.text = v.string
            if let tv = n.textView {
                if tv.string != v.string { tv.string = v.string }
                applyTextAlign(n)
            } else if let f = n.view as? NSTextField, f.stringValue != v.string {
                f.stringValue = v.string
            }
        case AsukaProp.TextField.placeholder:
            n.placeholder = v.string
            if let f = n.view as? NSTextField { f.placeholderString = v.string } else { textViewPlaceholder(n) }
        case AsukaProp.TextField.secure:
            n.secure = v.bool
            guard n.textView == nil, let old = n.view as? NSTextField, v.bool != (old is NSSecureTextField) else { return }
            let f = makeField(n, secure: v.bool)
            f.stringValue = n.text
            f.placeholderString = n.placeholder
            replaceView(n, f)
            if n.plain { plainField(n) }
        case AsukaProp.TextField.multiline:
            if v.bool && n.textView == nil { makeMultiline(n) }
        case AsukaProp.TextField.plain:
            n.plain = v.bool
            plainField(n)
        case AsukaProp.TextField.selection:
            setSelection(n, v.data)
        case AsukaProp.TextField.pasteTypes:
            n.pasteTypes = asukaStrings(v.data)
        case AsukaProp.TextField.rich:
            setRichField(n, v.data)
        case AsukaProp.TextField.autocorrect:
            n.textView?.isAutomaticSpellingCorrectionEnabled = AsukaAutocorrect(rawValue: v.enumValue) != .no
        case AsukaProp.TextField.contentType:
            if #available(macOS 11.0, *), let f = n.view as? NSTextField {
                switch v.string {
                case "username": f.contentType = .username
                case "password", "newPassword": f.contentType = .password
                case "oneTimeCode": f.contentType = .oneTimeCode
                default: break
                }
            }
        default:
            break
        }
    }

    /// Draws n's field as Plain says: without its bezel, background and
    /// focus ring, or with them.
    func plainField(_ n: AsukaNode) {
        if n.textView != nil {
            plainTextView(n)
            return
        }
        guard let f = n.view as? NSTextField else { return }
        f.isBezeled = !n.plain
        f.isBordered = false
        f.drawsBackground = !n.plain
        f.focusRingType = n.plain ? .none : .default
    }

    /// A field got the keyboard's focus.
    func fieldFocused(_ n: AsukaNode) {
        emit(n, .focus) { AsukaEvents.focus($0) }
        applyPendingSelection(n)
    }

    // MARK: Actions

    /// A control's action.
    @objc func act(_ sender: Any?) {
        guard let v = sender as? NSView, let n = node(of: v) else {
            barItem(sender)
            return
        }
        switch n.kind {
        case .button:
            if n.menu != nil && n.menuTrigger == AsukaMenuTrigger.tap.rawValue {
                popUp(n)
                return
            }
            emit(n, .press) { AsukaEvents.press($0) }
        case .toggle:
            let on = (sender as? NSSwitch)?.state == .on || (sender as? NSButton)?.state == .on
            emit(n, .toggled) { AsukaEvents.toggled($0, on) }
        case .slider, .stepper:
            let d = (sender as? NSControl)?.doubleValue ?? 0
            emit(n, .valueChanged) { AsukaEvents.valueChanged($0, d) }
        case .segmented:
            n.selected = (sender as? NSSegmentedControl)?.selectedSegment ?? 0
            let i = Int64(n.selected)
            emit(n, .selected) { AsukaEvents.selected($0, i) }
        case .wheel:
            n.selected = (sender as? NSPopUpButton)?.indexOfSelectedItem ?? 0
            let i = Int64(n.selected)
            emit(n, .selected) { AsukaEvents.selected($0, i) }
        case .picker:
            // Its first item is the label's (fillPicker).
            n.selected = ((sender as? NSPopUpButton)?.indexOfSelectedItem ?? 0) - 1
            let i = Int64(n.selected)
            emit(n, .selected) { AsukaEvents.selected($0, i) }
        case .datePicker:
            let t = (sender as? NSDatePicker)?.dateValue.timeIntervalSince1970 ?? 0
            let ms = (t * 1000).rounded()
            emit(n, .valueChanged) { AsukaEvents.valueChanged($0, ms) }
        case .colorPicker:
            guard let c = (sender as? NSColorWell)?.color else { return }
            let col = Int64(bitPattern: opsColor(c))
            emit(n, .colorChanged) { AsukaEvents.colorChanged($0, col) }
        case .snackbar:
            snackPressed(n, sender)
        default:
            break
        }
    }
}

extension AsukaRuntime: NSTextFieldDelegate {
    public func controlTextDidChange(_ note: Notification) {
        guard let f = note.object as? NSTextField else { return }
        guard let n = node(of: f), n.kind == .textField else {
            if !alertFieldChanged(f) { searchChanged(f) }
            return
        }
        n.text = f.stringValue
        let s = n.text
        emit(n, .textChanged) { AsukaEvents.textChanged($0, s) }
    }

    public func controlTextDidEndEditing(_ note: Notification) {
        guard let f = note.object as? NSView, let n = node(of: f) else { return }
        emit(n, .blur) { AsukaEvents.blur($0) }
    }

    /// Return in a field: Submit. Escape in a flyout's or a panel's field
    /// closes it, as menus close.
    public func control(_ control: NSControl, textView: NSTextView, doCommandBy cmd: Selector) -> Bool {
        if cmd == #selector(NSResponder.cancelOperation(_:)), let w = control.window, escapeFloating(w) { return true }
        guard let n = node(of: control) else {
            if let handled = alertFieldCommand(control, cmd) { return handled }
            return searchCommand(control, cmd)
        }
        if cmd == #selector(NSResponder.insertNewline(_:)) {
            emit(n, .submit) { AsukaEvents.submit($0) }
            return n.listening(.submit)
        }
        return textCommand(n, cmd)
    }
}

/// A Scroll's view.
class AsukaScrollView: NSScrollView {
    override var isFlipped: Bool { true }
}

/// ColorPicker's well: the color panel is the app's one (NSColorPanel's
/// shared panel), so a well's title (ColorPicker.Title) and whether it
/// shows opacity (Alpha) go to the panel as the well activates (a click
/// opens the panel for it), and at once if it's the active one.
final class AsukaColorWell: NSColorWell {
    var panelTitle = ""
    var showsAlpha = false

    /// The panel's own title ("Colors", in the user's language), as it was
    /// before a well named it.
    @MainActor static let panelTitle: String = NSColorPanel.shared.title

    override func activate(_ exclusive: Bool) {
        super.activate(exclusive)
        applyToPanel()
    }

    func applyToPanel() {
        let p = NSColorPanel.shared
        p.showsAlpha = showsAlpha
        p.title = panelTitle.isEmpty ? Self.panelTitle : panelTitle
    }

    override func removeFromSuperview() {
        if isActive { deactivate() }
        super.removeFromSuperview()
    }
}

/// A button's cell. AppKit draws a disabled button's title in its dimmed
/// color whatever its attributed title says; a disabled filled button's
/// (titleAsGiven) is drawn as its attributed title says while its window
/// is key, white on the accent bezel AppKit only lightens. In a window
/// that isn't key the bezel is gray, and AppKit's title is drawn.
final class AsukaButtonCell: NSButtonCell {
    var titleAsGiven = false

    override func drawTitle(_ title: NSAttributedString, withFrame frame: NSRect, in controlView: NSView) -> NSRect {
        guard titleAsGiven, !isEnabled, controlView.window?.isKeyWindow == true else {
            return super.drawTitle(title, withFrame: frame, in: controlView)
        }
        let t = attributedTitle
        let s = t.size()
        let r = NSRect(x: frame.midX - s.width / 2, y: frame.midY - s.height / 2, width: s.width, height: s.height)
        t.draw(in: r)
        return r
    }
}
