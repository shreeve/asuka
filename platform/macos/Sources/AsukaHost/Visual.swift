// The visual part of the Mac host: what each kind it owns does as the
// core's ops come (its files: Image.swift, Shape.swift, Canvas.swift,
// Shader.swift, ProgressRing.swift, Video.swift, Web.swift,
// WebModules.swift, NativeViews.swift; presses, hover, gestures and drag
// and drop are Interaction.swift's, Gestures.swift's and DragDrop.swift's),
// and the hooks the foundation calls (Parts.swift).
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

/// What the visual part keeps for the app as a whole (an extension can't
/// add stored properties to the runtime): the press under way, the drag
/// over the app and the app's own drag, hover checks, the shaders' clock,
/// the images' cache.
@MainActor final class AsukaVisual: NSObject {
    static let shared = AsukaVisual()

    /// The primary button's press, from its down to its up.
    var mouse = AsukaMouseState()
    /// The drag over the app's drop targets.
    var over = AsukaDropState()
    /// The app's own drag under way.
    var drag: AsukaDragSource?
    /// Views to check for the pointer once the core has returned.
    var hoverCheck: [AsukaNode] = []
    var hoverQueued = false
    /// Hover checks wait while a part places the views the core bound into
    /// it (a List's rows): until then they aren't where they'll show.
    var hoverHeld = 0
    /// A trackpad's two-finger scroll over a view that takes swipes.
    var wheelSwipe = AsukaWheelSwipe()
    /// The animated shaders' clock.
    var shaderClock: AsukaFrameClockTarget?
    /// The rings' colors follow the windows' states: observed once.
    var ringsObserved = false
    /// Whether the app's activation is followed (hover, presses).
    var appObserved = false
    /// Drops' directories made, for their names.
    var dropSeq = 0
    /// A row of a selecting list takes a click to select it: the list
    /// part's (list.go's selectClick), set by it; nil: no list selects.
    var listSelectClick: ((NSView) -> Bool)?

    // Test hooks (the automation's: VisualAutomation.swift).
    /// Where the pointer is in a window, in its coordinates: a test's
    /// stand-in for the window server's (nil: the real pointer).
    var hoverProbe: ((NSWindow) -> CGPoint?)?
    /// Drags record what AppKit would get instead of beginning a session
    /// (a session needs the window server's mouse).
    var fakeSessions = false
    var dragLog: [String] = []
}

@MainActor extension AsukaRuntime {
    var visual: AsukaVisual { AsukaVisual.shared }

    // MARK: Hooks (Parts.swift)

    /// n is being destroyed: what its parts keep that the core no longer
    /// needs. Its view may stay a while (a transition).
    func visualDestroyed(_ n: AsukaNode) {
        if let s = n.shader, s.animated {
            s.animated = false
            afterCore { self.updateShaderClock() } // once it's gone from the nodes
        }
    }

    /// n's view goes: what its parts hold.
    func visualReleased(_ n: AsukaNode) {
        dropInteraction(n)
        n.web.map { closeWeb(n, $0) }
        n.web = nil
        n.video?.close()
        n.video = nil
        n.ring?.release()
        releaseImage(n)
    }

    /// n has a new frame: what follows its size.
    func visualPlaced(_ n: AsukaNode) {
        if n.shader != nil { layoutShader(n) }
        if n.interaction?.hoverArea != nil { checkHover(n) } // moved under the pointer
        if let s = n.shape { s.resized(n) }
        if let r = n.ring { r.layout(self, n) }
        if n.image != nil { layoutImage(n) }
    }

    /// The appearance changed: the colors set as fixed CGColors.
    func visualColors(_ n: AsukaNode) {
        if let s = n.shape { withAppearance(n) { s.colors(n) } }
        if let r = n.ring { r.restyle(self, n) }
        if n.kind == .canvas { n.view?.needsDisplay = true }
        if let x = n.image, let shown = x.shown { setPlaceholderImage(n, shown, x.tinted) }
    }

    /// n's direction was applied: a symbol mirrors with it.
    func visualDirection(_ n: AsukaNode, _ changed: Bool) {
        if changed && n.kind == .symbol { n.view?.needsDisplay = true }
    }

    /// What the visual part shows of n, for the automation's tree.
    func visualJSON(_ n: AsukaNode, _ d: inout [String: Any]) {
        if let x = n.image { imageJSON(n, x, &d) }
        if n.kind == .symbol, let iv = n.view as? NSImageView {
            d["symbol"] = n.symbol
            if let img = iv.image { d["imageSize"] = [Double(img.size.width), Double(img.size.height)] }
            if let c = iv.contentTintColor { d["tint"] = colorHex(c) }
        }
        if let s = n.shape { shapeJSON(s, &d) }
        if let r = n.ring { ringJSON(n, r, &d) }
        if let s = n.shader { shaderJSON(n, s, &d) }
        if let w = n.web { webJSON(n, w, &d) }
        if let v = n.video { videoJSON(v, &d) }
        if let h = n.native { nativeJSON(n, h, &d) }
        if n.kind == .canvas { d["canvasBytes"] = n.canvas.count }
        interactionJSON(n, &d)
        layerJSON(n, &d)
        if n === root { d["visual"] = visualStateJSON() }
    }

    /// What the tests of the common props read of n's layers and controls
    /// (Props.swift's and Controls.swift's): its shadow, its transform, its
    /// clip view, a glass button's shape, a pop-up's menu, a text view's
    /// label.
    func layerJSON(_ n: AsukaNode, _ d: inout [String: Any]) {
        guard let v = n.view else { return }
        if let l = v.layer {
            if l.shadowOpacity != 0 || n.hasShadow {
                d["shadowOpacity"] = Double(l.shadowOpacity)
                d["shadowOffset"] = [Double(l.shadowOffset.width), Double(l.shadowOffset.height)]
                d["shadowPath"] = l.shadowPath != nil
            }
            if let a = l.animation(forKey: "asuka.transform") {
                d["transformAnimation"] = a.duration
                d["held"] = [Double(n.held.m11), Double(n.held.m22), Double(n.held.m41), Double(n.held.m42)]
            }
        }
        if let c = n.clipView {
            d["clipView"] = ["frame": [Double(c.frame.minX), Double(c.frame.minY), Double(c.frame.width), Double(c.frame.height)],
                             "masks": c.layer?.masksToBounds ?? false, "cornerRadius": Double(c.layer?.cornerRadius ?? 0),
                             "children": n.children.filter { $0.view?.superview === c }.count]
        }
        if !n.children.isEmpty {
            d["childrenInView"] = n.children.filter { $0.view?.superview === v }.count
        }
        if let b = v as? NSButton, #available(macOS 26.0, *) {
            d["borderShape"] = b.borderShape.rawValue
            d["tintProminence"] = b.tintProminence.rawValue
        }
        if let p = v as? NSPopUpButton {
            d["menuShown"] = p.itemArray.filter { !$0.isHidden }.map { $0.title }
        }
        if let tv = n.textView, let l = tv.accessibilityLabel() { d["textViewA11yLabel"] = l }
    }

    /// The host's module of this part: "web" (a WebView's script and the
    /// web views' cookies).
    func registerVisualModules() {
        registerWebModules()
    }

    // MARK: Props

    func setImageProp(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        if n.kind == .symbol {
            if p == AsukaProp.Symbol.name {
                n.symbol = v.string
                applySymbol(n)
            }
            return
        }
        setImage(n, p, v)
    }
}
