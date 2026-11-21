// The common props (Style): colors, fonts, the layer's background, border,
// corners, shadow, gradient, transform and material, accessibility.
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

/// An ops color as an NSColor (nil for the platform's default).
@MainActor func asukaColor(_ v: UInt64) -> NSColor? {
    switch v >> 32 {
    case 1:
        // Its opacity (Color.Opacity) is in bits 24-31, as transparency.
        let a = 255 - CGFloat((v >> 24) & 0xff)
        guard let c = semanticColor(AsukaSemanticColor(rawValue: Int32(truncatingIfNeeded: v & 0xffff))) else { return nil }
        return a == 255 ? c : c.withAlphaComponent(a / 255)
    case 2:
        let c = UInt32(truncatingIfNeeded: v)
        return NSColor(srgbRed: CGFloat((c >> 24) & 0xff) / 255, green: CGFloat((c >> 16) & 0xff) / 255,
                       blue: CGFloat((c >> 8) & 0xff) / 255, alpha: CGFloat(c & 0xff) / 255)
    default:
        return nil
    }
}

/// Asuka's semantic colors as AppKit's.
@MainActor func semanticColor(_ s: AsukaSemanticColor?) -> NSColor? {
    switch s {
    case .label: return .labelColor
    case .secondaryLabel: return .secondaryLabelColor
    case .tertiaryLabel: return .tertiaryLabelColor
    case .background, .groupedBackground: return .windowBackgroundColor
    case .secondaryBackground, .secondaryGroupedBackground: return .controlBackgroundColor
    case .separator: return .separatorColor
    case .accent: return accentNSColor
    case .red: return .systemRed
    case .orange: return .systemOrange
    case .yellow: return .systemYellow
    case .green: return .systemGreen
    case .blue: return .systemBlue
    case .purple: return .systemPurple
    case .pink: return .systemPink
    case .gray: return .systemGray
    case .clear: return .clear
    case .onAccent: return .white
    default: return nil
    }
}

/// The app's accent, or the system's.
@MainActor var accentNSColor: NSColor {
    let a = AsukaRuntime.shared.accent
    if a >> 32 == 2, let c = asukaColor(a) { return c }
    return .controlAccentColor
}

/// An NSColor as an RGBA ops color.
func opsColor(_ c: NSColor) -> UInt64 {
    guard let s = c.usingColorSpace(.sRGB) else { return 0 }
    func ch(_ v: CGFloat) -> UInt64 { UInt64((min(max(v, 0), 1) * 255).rounded()) }
    return 2 << 32 | ch(s.redComponent) << 24 | ch(s.greenComponent) << 16 | ch(s.blueComponent) << 8 | ch(s.alphaComponent)
}

/// Little-endian reads of a prop's bytes.
extension Array where Element == UInt8 {
    func f32(_ i: Int) -> Float {
        guard i + 4 <= count else { return 0 }
        return withUnsafeBytes { Float(bitPattern: UInt32(littleEndian: $0.loadUnaligned(fromByteOffset: i, as: UInt32.self))) }
    }
    func u32(_ i: Int) -> UInt32 {
        guard i + 4 <= count else { return 0 }
        return withUnsafeBytes { UInt32(littleEndian: $0.loadUnaligned(fromByteOffset: i, as: UInt32.self)) }
    }
    func u64(_ i: Int) -> UInt64 {
        guard i + 8 <= count else { return 0 }
        return withUnsafeBytes { UInt64(littleEndian: $0.loadUnaligned(fromByteOffset: i, as: UInt64.self)) }
    }
}

extension AsukaValue {
    /// Bytes, copied (a value's are valid only during the call).
    var data: [UInt8] {
        if case .bytes(let b) = self { return Array(b) }
        return []
    }
    var edges: (top: CGFloat, right: CGFloat, bottom: CGFloat, left: CGFloat) {
        if case .edges(let t, let r, let b, let l) = self { return (CGFloat(t), CGFloat(r), CGFloat(b), CGFloat(l)) }
        return (0, 0, 0, 0)
    }
}

/// A u32 count, then u32-length strings.
func asukaStrings(_ b: [UInt8]) -> [String] {
    guard b.count >= 4 else { return [] }
    let n = Int(b.u32(0))
    var i = 4
    var out: [String] = []
    for _ in 0..<n {
        guard i + 4 <= b.count else { break }
        let l = Int(b.u32(i))
        i += 4
        guard i + l <= b.count else { break }
        out.append(String(decoding: b[i..<i + l], as: UTF8.self))
        i += l
    }
    return out
}

@MainActor extension AsukaRuntime {
    /// The layer of n's view, which is made layer-backed.
    func layer(_ n: AsukaNode) -> CALayer? {
        guard let v = n.view else { return nil }
        if !v.wantsLayer { v.wantsLayer = true }
        return v.layer
    }

    /// Runs f with the window's appearance current, so the dynamic system
    /// colors resolve to it (a layer takes a fixed CGColor).
    func withAppearance(_ n: AsukaNode? = nil, _ f: () -> Void) {
        let a = n?.view?.window?.effectiveAppearance ?? main?.window.effectiveAppearance ?? NSApp.effectiveAppearance
        if #available(macOS 11.0, *) {
            a.performAsCurrentDrawingAppearance(f)
        } else {
            let prev = NSAppearance.current
            NSAppearance.current = a
            f()
            NSAppearance.current = prev
        }
    }

    func setCommon(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        switch p {
        case AsukaProp.layoutDirection:
            n.direction = AsukaLayoutDirection(rawValue: v.enumValue) ?? .inherit
            applyDirection(n)
            return
        case AsukaProp.keys:
            if case .bytes(let b) = v { n.keys = AsukaKeys.decode(b) } else { n.keys = [] }
            keysChanged(n)
            return
        case AsukaProp.focusable, AsukaProp.noFocusOnPress, AsukaProp.noFocusRing, AsukaProp.focusTrap:
            setFocusProp(n, p, v)
            return
        case AsukaProp.a11yRole, AsukaProp.a11yHint, AsukaProp.a11yChecked, AsukaProp.a11yExpanded, AsukaProp.a11ySelected,
             AsukaProp.a11yValue, AsukaProp.a11yRange, AsukaProp.a11yModal, AsukaProp.a11yControls, AsukaProp.a11yLabelledBy,
             AsukaProp.a11yDescribedBy:
            setA11yProp(n, p, v) // A11y.swift
            return
        default:
            break
        }
        guard let view = n.view else {
            setNavCommon(n, p, v)
            return
        }
        switch p {
        case AsukaProp.contentInset:
            n.inset = v.edges
            applyContentInset(n)
        case AsukaProp.background:
            n.background = v.color
            if n.kind == .button, let b = view as? NSButton {
                b.bezelColor = asukaColor(n.background)
                return
            }
            withAppearance(n) { layer(n)?.backgroundColor = asukaColor(n.background)?.cgColor }
            if n.list != nil { listBackground(n) }
        case AsukaProp.cornerRadius:
            n.cornerRadius = CGFloat(v.float)
            applyCorner(n)
        case AsukaProp.borderWidth:
            n.borderWidth = CGFloat(v.float)
            layer(n)?.borderWidth = n.borderWidth
        case AsukaProp.borderColor:
            n.borderColor = v.color
            withAppearance(n) { layer(n)?.borderColor = asukaColor(n.borderColor)?.cgColor }
        case AsukaProp.opacity:
            n.opacity = CGFloat(v.float)
            if animating { view.animator().alphaValue = n.opacity } else { view.alphaValue = n.opacity }
        case AsukaProp.hidden:
            view.isHidden = v.bool
            if v.bool { viewLeft(n) }
        case AsukaProp.clip:
            n.clip = v.bool
            updateClip(n)
        case AsukaProp.a11yLabel:
            n.a11yLabel = v.string
            let target: NSView = n.textView ?? view
            target.setAccessibilityLabel(v.string.isEmpty ? nil : v.string)
            if !v.string.isEmpty && n.kind == .view { view.setAccessibilityElement(true) }
        case AsukaProp.a11yHidden:
            view.setAccessibilityElement(!v.bool)
            if v.bool { hideA11y(view) }
        case AsukaProp.a11yActions:
            setA11yActions(n, asukaStrings(v.data))
        case AsukaProp.a11yAdjustable:
            n.adjustable = v.bool
            if n.adjustable && (n.a11y?.role ?? .none) == .none {
                view.setAccessibilityElement(true)
                view.setAccessibilityRole(.incrementor)
            }
        case AsukaProp.color:
            n.color = v.color
            applyColor(n)
        case AsukaProp.textStyle:
            n.textStyle = v.enumValue
            applyFont(n)
        case AsukaProp.fontSize:
            n.fontSize = Float(v.float)
            applyFont(n)
        case AsukaProp.fontWeight:
            n.fontWeight = v.enumValue
            applyFont(n)
        case AsukaProp.fontFamily:
            n.fontFamily = v.string
            applyFont(n)
        case AsukaProp.textAlign:
            n.textAlign = v.enumValue
            if n.rich != nil { richInvalidate(n) } else { applyTextAlign(n) }
        case AsukaProp.lineLimit:
            n.lineLimit = Int(v.int)
            applyLineLimit(n)
        case AsukaProp.disabled:
            if let c = view as? NSControl { c.isEnabled = !v.bool }
            if view is NSButton && n.kind == .button { buttonTitle(n) } // a filled button's title stays white
            if v.bool || n.focus != nil { focusProps(n).disabled = v.bool } // it takes no focus (Focus.swift)
            if n.textView != nil { n.textView?.isEditable = !v.bool }
            if n.web != nil || n.kind == .view { view.alphaValue = v.bool ? 0.5 : n.opacity }
        case AsukaProp.menu:
            if case .bytes(let b) = v { n.menuActions = AsukaActions.decode(b) } else { n.menuActions = [] }
            setMenu(n)
        case AsukaProp.menuTrigger:
            n.menuTrigger = v.enumValue
            setMenu(n)
        case AsukaProp.transform:
            n.transform = v.data
            setTransform(n)
        case AsukaProp.shadow:
            n.shadow = v.data
            withAppearance(n) { setShadow(n) }
        case AsukaProp.gradient:
            n.gradient = v.data
            withAppearance(n) { setGradient(n) }
        case AsukaProp.material:
            setMaterial(n, AsukaMaterial(rawValue: v.enumValue) ?? .none)
        case AsukaProp.swipeActions:
            if case .bytes(let b) = v { n.cell?.swipe = AsukaActions.decode(b) }
        case AsukaProp.leadingSwipeActions:
            if case .bytes(let b) = v { n.cell?.leading = AsukaActions.decode(b) }
        case AsukaProp.selected:
            n.cell?.setSelected(v.bool)
        case AsukaProp.tooltip:
            n.tooltip = v.string
            view.toolTip = v.string.isEmpty ? nil : v.string
        case AsukaProp.interceptBack:
            n.interceptBack = v.bool // ui.OnBack: the back button and Escape ask the core
        case AsukaProp.transition:
            let b = v.data
            n.transition = b.count >= 13 ? b : []
        case AsukaProp.match:
            setMatch(n, v.string)
        default:
            break
        }
    }

    func hideA11y(_ v: NSView) {
        for s in v.subviews {
            s.setAccessibilityElement(false)
            hideA11y(s)
        }
    }

    /// Gives the view VoiceOver's custom actions, which send A11yAction
    /// with their index.
    func setA11yActions(_ n: AsukaNode, _ names: [String]) {
        guard let v = n.view else { return }
        let id = n.id
        v.setAccessibilityCustomActions(names.enumerated().map { i, name in
            NSAccessibilityCustomAction(name: name) { [weak self] in
                self?.emit(id, .a11yAction) { AsukaEvents.a11yAction(id, Int64(i)) }
                return true
            }
        })
        if !names.isEmpty { v.setAccessibilityElement(true) }
    }

    /// Puts n's content inside its padding and border (ContentInset).
    func applyContentInset(_ n: AsukaNode) {
        let i = n.inset
        if n.rich != nil {
            richContentInset(n)
        } else if let tv = n.textView, let sv = n.view as? NSScrollView {
            _ = tv
            sv.automaticallyAdjustsContentInsets = false
            sv.contentInsets = NSEdgeInsets(top: i.top, left: i.left, bottom: i.bottom, right: i.right)
        } else if n.kind == .text {
            n.view?.needsDisplay = true
        } else if n.kind == .textField, let f = n.view as? NSTextField {
            f.needsDisplay = true
            // A field being edited moves its field editor into it.
            if let ed = f.currentEditor(), let cell = f.cell { ed.frame = cell.drawingRect(forBounds: f.bounds) }
        }
    }

    // MARK: Layers

    /// Rounds the layer, at most half the shorter side (999: a capsule),
    /// as CSS and the other hosts clamp it.
    func applyCorner(_ n: AsukaNode) {
        guard let v = n.view, n.cornerRadius != 0 || v.wantsLayer, let l = layer(n) else { return }
        let r = n.cornerOf
        l.cornerRadius = r
        n.clipView?.layer?.cornerRadius = r
        updateShadowPath(n)
        if let e = n.effect {
            e.wantsLayer = true
            e.layer?.cornerRadius = r
            e.layer?.masksToBounds = true
        }
        if n.kind == .image || n.kind == .video { l.masksToBounds = r > 0 || n.clip }
        if let g = n.gradientLayer { g.cornerRadius = r }
    }

    /// Sets a layer shadow: color, blur radius, offset, cast by the view's
    /// shape (its bounds, rounded), as on iOS.
    func setShadow(_ n: AsukaNode) {
        guard let l = layer(n) else { return }
        let b = n.shadow
        guard n.hasShadow else {
            l.shadowOpacity = 0
            l.shadowPath = nil
            updateClip(n)
            return
        }
        l.shadowColor = asukaColor(b.u64(0))?.cgColor
        l.shadowOpacity = 1
        // Radius is a blur radius, as CSS and SwiftUI mean it: 2σ. Core
        // Animation's is σ.
        l.shadowRadius = CGFloat(b.f32(8)) / 2
        // y down: the layers under the window's flipped content view have it.
        l.shadowOffset = CGSize(width: CGFloat(b.f32(12)), height: CGFloat(b.f32(16)))
        updateShadowPath(n)
        updateClip(n)
    }

    /// Sets n's shadow on its layer again as its view moves into a window:
    /// AppKit owns a layer-backed view's layer's shadow, and sets it from
    /// the view's own (none) then.
    func reassertShadow(_ n: AsukaNode) {
        if n.hasShadow { withAppearance(n) { setShadow(n) } }
    }

    /// The shape n's shadow is cast by: its bounds, rounded by its corner
    /// radius.
    func updateShadowPath(_ n: AsukaNode) {
        guard n.hasShadow, let l = layer(n) else { return }
        let b = CGRect(origin: .zero, size: n.frame.size)
        let r = n.cornerOf
        l.shadowPath = r > 0 && b.width > 0 && b.height > 0 ? CGPath(roundedRect: b, cornerWidth: r, cornerHeight: r, transform: nil) : CGPath(rect: b, transform: nil)
    }

    /// Clips n's children to its bounds, rounded by its corner radius,
    /// while Clip is set. A layer that masks to its bounds masks its own
    /// shadow too, so a view with a shadow clips its children in a view
    /// of their own that fills it, and keeps its shadow.
    func updateClip(_ n: AsukaNode) {
        guard let v = n.view else { return }
        let split = n.clip && n.hasShadow && n.kind == .view && n.cell == nil
        if split && n.clipView == nil {
            let cv = AsukaView(frame: CGRect(origin: .zero, size: n.frame.size))
            cv.wantsLayer = true
            v.addSubview(cv)
            for c in n.children { if let cvw = c.view, cvw.superview === v { cv.addSubview(cvw) } }
            n.clipView = cv
        } else if !split, let cv = n.clipView {
            for c in n.children { if let cvw = c.view, cvw.superview === cv { v.addSubview(cvw) } }
            cv.removeFromSuperview()
            n.clipView = nil
        }
        guard let l = layer(n) else { return }
        let own = n.clip && n.clipView == nil
        if #available(macOS 14.0, *) { v.clipsToBounds = own }
        if n.kind != .image && n.kind != .video { l.masksToBounds = own } else { l.masksToBounds = own || n.cornerOf > 0 }
        if let cl = n.clipView?.layer {
            cl.masksToBounds = true
            cl.cornerRadius = n.cornerOf
        }
    }

    /// Puts a CAGradientLayer under the view's content.
    func setGradient(_ n: AsukaNode) {
        guard let l = layer(n) else { return }
        let b = n.gradient
        let count = Int(b.u32(0))
        guard b.count >= 4, count > 0, b.count >= 4 + count * 12 + 16 else {
            n.gradientLayer?.removeFromSuperlayer()
            n.gradientLayer = nil
            return
        }
        let g: CAGradientLayer
        if let x = n.gradientLayer { g = x } else {
            g = CAGradientLayer()
            l.insertSublayer(g, at: 0)
            n.gradientLayer = g
        }
        var colors: [CGColor] = []
        var locs: [NSNumber] = []
        for i in 0..<count {
            let o = 4 + i * 12
            colors.append(asukaColor(b.u64(o))?.cgColor ?? NSColor.clear.cgColor)
            locs.append(NSNumber(value: b.f32(o + 8)))
        }
        let o = 4 + count * 12
        g.colors = colors
        g.locations = locs
        // The layer's unit space has y down, as the layers under the
        // window's flipped content view do.
        g.startPoint = CGPoint(x: CGFloat(b.f32(o)), y: CGFloat(b.f32(o + 4)))
        g.endPoint = CGPoint(x: CGFloat(b.f32(o + 8)), y: CGFloat(b.f32(o + 12)))
        layoutGradient(n, g)
    }

    func layoutGradient(_ n: AsukaNode, _ g: CAGradientLayer) {
        CATransaction.begin()
        if !animating { CATransaction.setDisableActions(true) }
        g.frame = CGRect(origin: .zero, size: n.frame.size)
        g.cornerRadius = n.view?.layer?.cornerRadius ?? 0
        CATransaction.commit()
    }

    /// Puts an NSVisualEffectView behind the view's content.
    func setMaterial(_ n: AsukaNode, _ m: AsukaMaterial) {
        guard let v = n.view else { return }
        if m == .none {
            n.effect?.removeFromSuperview()
            n.effect = nil
            return
        }
        if n.effect == nil {
            let e = NSVisualEffectView(frame: CGRect(origin: .zero, size: n.frame.size))
            e.blendingMode = .withinWindow
            e.state = .active
            v.addSubview(e, positioned: .below, relativeTo: nil)
            n.effect = e
            applyCorner(n)
        }
        switch m {
        case .ultraThin, .thin: n.effect?.material = .hudWindow
        case .regular: n.effect?.material = .underWindowBackground
        case .thick: n.effect?.material = .popover
        case .bar: n.effect?.material = .titlebar
        default: break
        }
    }

    /// Shows Style.Transform (translate, scale, rotate, around the center)
    /// on the view's layer.
    ///
    /// AppKit sets a layer-backed view's layer's own transform from the
    /// view, to the identity, whenever the window displays, so the
    /// transform is held by an animation that never ends: what's drawn is
    /// its value. It goes from what's drawn now, so an animated batch
    /// animates it. Layout and clicks don't see it, as on the other
    /// platforms.
    func setTransform(_ n: AsukaNode) {
        guard let l = layer(n) else { return }
        let b = n.transform
        var t = CATransform3DIdentity
        if b.count >= 20 {
            let tx = CGFloat(b.f32(0)), ty = CGFloat(b.f32(4))
            let sx = CGFloat(b.f32(8)), sy = CGFloat(b.f32(12))
            let rot = CGFloat(b.f32(16)) * .pi / 180
            // AppKit puts a layer's anchor at its origin: turn around the center.
            let cx = n.frame.width / 2, cy = n.frame.height / 2
            let c = cos(rot), s = sin(rot)
            // M = T(c + t) · R · S · T(-c), with y down, as the layers under
            // the window's flipped content view have it: a clockwise turn on
            // screen is a positive angle.
            let a = c * sx, bb = s * sx
            let cc = -s * sy, d = c * sy
            t.m11 = a; t.m12 = bb; t.m21 = cc; t.m22 = d
            t.m41 = cx + tx - (a * cx + cc * cy)
            t.m42 = cy + ty - (bb * cx + d * cy)
        }
        let key = "asuka.transform"
        let held = l.animation(forKey: key) != nil
        if CATransform3DEqualToTransform(t, n.held) && (held || b.count < 20) { return }
        n.held = t
        if b.count < 20 && !animating {
            l.removeAnimation(forKey: key)
            return
        }
        var from = NSValue(caTransform3D: t)
        if animating, let p = l.presentation(), let cur = p.value(forKey: "transform") as? NSValue { from = cur }
        let anim = CABasicAnimation(keyPath: "transform")
        anim.fromValue = from
        anim.toValue = NSValue(caTransform3D: t)
        if animating {
            anim.duration = animation.duration
            anim.timingFunction = NSAnimationContext.current.timingFunction
        }
        anim.isRemovedOnCompletion = false
        anim.fillMode = .forwards
        l.add(anim, forKey: key)
    }

    /// Sets the layer's colors from n's props again: a layer's CGColor is
    /// fixed, so they're set again when the appearance changes.
    func applyLayerColors(_ n: AsukaNode) {
        guard let v = n.view else { return }
        if n.background != 0 || n.borderColor != 0 || n.hasShadow {
            if n.kind == .button, n.background != 0, let b = v as? NSButton {
                b.bezelColor = asukaColor(n.background)
            } else {
                if n.background != 0 { layer(n)?.backgroundColor = asukaColor(n.background)?.cgColor }
                if n.borderColor != 0 { layer(n)?.borderColor = asukaColor(n.borderColor)?.cgColor }
            }
            if n.hasShadow { setShadow(n) }
        }
        if !n.gradient.isEmpty { setGradient(n) }
        partColors(n)
    }

    // MARK: Text

    /// The NSFont for a node's text props.
    func font(_ n: AsukaNode) -> NSFont { font(style: n.textStyle, weight: n.fontWeight, size: n.fontSize, family: n.fontFamily) }

    func font(style: Int32, weight: Int32, size: Float, family: String) -> NSFont {
        let base = styleFont(style)
        let pt = size > 0 ? CGFloat(size) : base.pointSize
        if !family.isEmpty, let f = familyFont(family, pt, weight) { return f }
        if weight > 0, Int(weight) < asukaWeights.count {
            return NSFont.systemFont(ofSize: pt, weight: NSFont.Weight(asukaWeights[Int(weight)]))
        }
        if size > 0 {
            // The text style's font (its weight: a headline's bold) at the size.
            return NSFont(descriptor: base.fontDescriptor, size: pt) ?? base
        }
        return base
    }

    func applyFont(_ n: AsukaNode) {
        guard let v = n.view else { return }
        if n.rich != nil {
            richInvalidate(n)
        } else if n.kind == .symbol {
            applySymbol(n)
        } else if n.kind == .button, let b = v as? NSButton {
            b.font = font(n)
            buttonTitle(n)
        } else if let tv = n.textView {
            tv.font = font(n)
        } else if let c = v as? NSControl {
            c.font = font(n)
        }
    }

    /// Sets the text color of texts and fields, the tint of controls and
    /// images.
    func applyColor(_ n: AsukaNode) {
        guard let v = n.view else { return }
        let c = asukaColor(n.color)
        switch n.kind {
        case .text, .textField:
            if let tv = n.textView { tv.textColor = c ?? .labelColor } else { (v as? NSTextField)?.textColor = c ?? .labelColor }
        case .button:
            if (c != nil && n.variant == AsukaButtonVariant.plain.rawValue) || macVariant(n) == .filled { buttonTitle(n) }
            (v as? NSButton)?.contentTintColor = c
        case .richText:
            richInvalidate(n)
        case .image, .symbol:
            (v as? NSImageView)?.contentTintColor = c
        case .progressRing:
            ringColor(n)
        case .activity, .progress:
            break // AppKit's indicators draw in the accent color; no tint.
        default:
            (v as? NSButton)?.contentTintColor = c
            (v as? NSSlider)?.trackFillColor = c
        }
    }

    func applyLineLimit(_ n: AsukaNode) {
        switch n.kind {
        case .text:
            guard let t = n.view as? NSTextField else { return }
            t.maximumNumberOfLines = n.lineLimit
            t.cell?.truncatesLastVisibleLine = n.lineLimit > 0
            t.lineBreakMode = n.lineLimit == 1 ? .byTruncatingTail : .byWordWrapping
        case .richText:
            richInvalidate(n)
        case .textField:
            if n.textView != nil { n.view?.needsLayout = true }
        default:
            break
        }
    }
}

/// NSFont.Weight's values for Asuka's weights.
let asukaWeights: [CGFloat] = [0, -0.8, -0.6, -0.4, 0, 0.23, 0.3, 0.4, 0.56, 0.62]

private var styleFonts: [Int32: NSFont] = [:]

/// The system font of a text style (macOS's sizes: body 13).
@MainActor func styleFont(_ style: Int32) -> NSFont {
    let s = style <= 0 || style > 11 ? AsukaTextStyle.body.rawValue : style
    if let f = styleFonts[s] { return f }
    let ts: NSFont.TextStyle
    switch AsukaTextStyle(rawValue: s) {
    case .largeTitle: ts = .largeTitle
    case .title: ts = .title1
    case .title2: ts = .title2
    case .title3: ts = .title3
    case .headline: ts = .headline
    case .subheadline: ts = .subheadline
    case .callout: ts = .callout
    case .footnote: ts = .footnote
    case .caption: ts = .caption1
    case .caption2: ts = .caption2
    default: ts = .body
    }
    let f = NSFont.preferredFont(forTextStyle: ts, options: [:])
    styleFonts[s] = f
    return f
}
