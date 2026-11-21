// RichText on a Mac.
//
// A RichText is an NSTextView (TextKit's typesetter, links, selection and
// accessibility), not editable, without insets, so what's measured is
// what's drawn. Outside a Selectable it takes only clicks on links: others
// go to the views under it. Inline components are attachments of their
// size; their views are placed where the text puts them. The wire format is
// ops/rich.go's (RichText's Content, a Rich TextField's Rich).
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

/// A run's style in styled text's encoding (ops.RichStyle): zero fields
/// are the view's.
struct AsukaRichStyle: Equatable {
    var textStyle: Int32 = 0
    var size: Float = 0
    var weight: Int32 = 0
    /// 1 italic, 2 monospaced, 4 underline, 8 strikethrough, 16 inline.
    var flags: UInt8 = 0
    var color: UInt64 = 0
    var background: UInt64 = 0
    var link = ""

    var italic: Bool { flags & 1 != 0 }
    var mono: Bool { flags & 2 != 0 }
    var underline: Bool { flags & 4 != 0 }
    var strike: Bool { flags & 8 != 0 }
    var inline: Bool { flags & 16 != 0 }
}

/// Styled text as the wire has it: its styles, and its paragraphs' runs
/// (an index into the styles, and the run's text).
struct AsukaRichContent {
    var styles: [AsukaRichStyle] = []
    var paragraphs: [[(style: Int, text: String)]] = []

    /// ops.AppendRich's encoding: u32 count of styles, each u8 text style,
    /// f32 size, u8 weight, u8 flags, u64 color, u64 background, a string
    /// link; then u32 paragraphs, each u32 runs, each u32 style and a
    /// string. What's cut short is left out.
    init(_ b: [UInt8]) {
        guard b.count >= 4 else { return }
        var r = AsukaByteReader(b)
        let count = Int(r.u32())
        for _ in 0..<count {
            guard r.i + 23 <= b.count else { return }
            var s = AsukaRichStyle()
            s.textStyle = Int32(r.u8())
            s.size = r.f32()
            s.weight = Int32(r.u8())
            s.flags = r.u8()
            s.color = r.u64()
            s.background = r.u64()
            s.link = r.string()
            if r.bad { return }
            styles.append(s)
        }
        guard r.i + 4 <= b.count else { return }
        let paras = Int(r.u32())
        for _ in 0..<paras {
            guard r.i + 4 <= b.count else { return }
            let runs = Int(r.u32())
            var p: [(style: Int, text: String)] = []
            for _ in 0..<runs {
                guard r.i + 4 <= b.count else { break }
                let si = Int(r.u32())
                let text = r.string()
                if r.bad { break }
                p.append((si, text))
            }
            paragraphs.append(p)
        }
    }
}

extension NSAttributedString.Key {
    /// Marks an inline component's attachment.
    static let asukaInline = NSAttributedString.Key("asuka.inline")
}

/// A RichText's view.
final class AsukaRichTextView: NSTextView {
    @MainActor var rt: AsukaRuntime { AsukaRuntime.shared }

    override func hitTest(_ point: NSPoint) -> NSView? {
        let hit = super.hitTest(point)
        guard let n = rt.node(of: self), let r = n.rich, !r.selectOK, hit === self else { return hit }
        // A link takes the click; the rest is the view's under it.
        let local = convert(point, from: superview)
        return rt.linkAt(self, local) ? self : nil
    }

    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    // In a Selectable, its selection is the Selectable's, across its texts.
    override func mouseDown(with event: NSEvent) {
        if let n = rt.node(of: self), n.rich?.selectOK == true, !rt.linkAt(self, convert(event.locationInWindow, from: nil)),
           rt.selectableTrack(n, event) {
            return
        }
        super.mouseDown(with: event)
    }

    override func copy(_ sender: Any?) {
        if let n = rt.node(of: self), rt.copySelectable(n) { return }
        super.copy(sender)
    }

    override func selectAll(_ sender: Any?) {
        if let n = rt.node(of: self), let s = rt.selectableOf(n) {
            rt.selectAllIn(s)
            return
        }
        super.selectAll(sender)
    }

    override func validateUserInterfaceItem(_ item: NSValidatedUserInterfaceItem) -> Bool {
        if item.action == #selector(copy(_:)), let n = rt.node(of: self), let s = rt.selectableOf(n), rt.selectableText(s) != nil { return true }
        return super.validateUserInterfaceItem(item)
    }
}

/// A RichText's state: its content, its inline components' sizes, its
/// attributed string made from them.
@MainActor final class AsukaRich {
    let view: AsukaRichTextView
    var content: [UInt8] = []
    var inlines: [(id: UInt32, w: CGFloat, h: CGFloat)] = []
    var dirty = true
    /// A rebuild is queued for when the core has returned.
    var queued = false
    /// The width its text is laid out at.
    var width: CGFloat = 0
    /// It's in a Selectable: it takes clicks, to select.
    var selectOK = false

    init() {
        let v = AsukaRichTextView(frame: .zero)
        v.isEditable = false
        v.isSelectable = true
        v.isRichText = true
        v.drawsBackground = false
        v.textContainerInset = .zero
        v.textContainer?.lineFragmentPadding = 0
        v.textContainer?.widthTracksTextView = false
        v.textContainer?.containerSize = CGSize(width: asukaBig, height: asukaBig)
        v.isVerticallyResizable = false
        v.isHorizontallyResizable = false
        v.delegate = AsukaTextDelegate.shared
        v.isAutomaticLinkDetectionEnabled = false
        view = v
    }

    /// Typesets the text at width and returns its used size.
    func layoutAt(_ width: CGFloat) -> CGSize {
        guard let tc = view.textContainer, let lm = view.layoutManager else { return .zero }
        tc.containerSize = CGSize(width: width, height: asukaBig)
        lm.ensureLayout(for: tc)
        return lm.usedRect(for: tc).size
    }
}

@MainActor extension AsukaRuntime {
    func makeRich(_ n: AsukaNode) -> NSView {
        let r = AsukaRich()
        n.rich = r
        AsukaTextDelegate.shared.start()
        return r.view
    }

    func setRichProp(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        guard let r = n.rich else { return }
        switch p {
        case AsukaProp.RichText.content:
            r.content = v.data
            richInvalidate(n)
        case AsukaProp.RichText.inlines:
            r.inlines = []
            let b = v.data
            if b.count >= 4 {
                let count = Int(b.u32(0))
                for i in 0..<count {
                    let o = 4 + i * 12
                    guard o + 12 <= b.count else { break }
                    r.inlines.append((b.u32(o), CGFloat(b.f32(o + 4)), CGFloat(b.f32(o + 8))))
                }
            }
            richInvalidate(n)
        default:
            break
        }
    }

    /// Its text typeset again (a prop it's made of changed): at once if the
    /// core lays it out again (it measures it), else once the core has
    /// returned, as it shows.
    func richInvalidate(_ n: AsukaNode) {
        guard let r = n.rich else { return }
        r.dirty = true
        guard !r.queued else { return }
        r.queued = true
        afterCore { [weak self] in
            r.queued = false
            guard let self, self.alive(n), r.dirty else { return }
            self.placeInlines(n)
            r.view.needsDisplay = true
        }
    }

    /// Sets the text again the way it now reads, once the core has returned
    /// (it lays it out again only if its size may have changed).
    func richDirectionChanged(_ n: AsukaNode) {
        richInvalidate(n)
    }

    /// Its text inside its padding (ContentInset): the container is as wide
    /// as what's left, at the text view's container origin.
    func richContentInset(_ n: AsukaNode) {
        guard let r = n.rich else { return }
        r.view.textContainerInset = CGSize(width: n.inset.left, height: n.inset.top)
        placeInlines(n)
        r.view.needsDisplay = true
    }

    /// Makes the attributed string of the content, styled by the view's
    /// text props.
    func buildRich(_ n: AsukaNode) {
        guard let r = n.rich, r.dirty else { return }
        r.dirty = false
        let c = AsukaRichContent(r.content)
        let para = NSMutableParagraphStyle()
        para.alignment = alignment(n)
        para.baseWritingDirection = writingDirection(n)
        let baseColor = asukaColor(n.color) ?? .labelColor
        let fm = NSFontManager.shared
        var styles: [[NSAttributedString.Key: Any]] = []
        for s in c.styles {
            let ts = s.textStyle != 0 ? s.textStyle : n.textStyle
            let size = s.size != 0 ? s.size : n.fontSize
            let weight = s.weight != 0 ? s.weight : n.fontWeight
            var f = font(style: ts, weight: weight, size: size, family: n.fontFamily)
            if s.mono {
                let w = weight > 0 && Int(weight) < asukaWeights.count ? asukaWeights[Int(weight)] : 0
                f = NSFont.monospacedSystemFont(ofSize: f.pointSize, weight: NSFont.Weight(w))
            }
            if s.italic { f = fm.convert(f, toHaveTrait: .italicFontMask) }
            var a: [NSAttributedString.Key: Any] = [.font: f, .paragraphStyle: para]
            var color = baseColor
            if s.color != 0, let x = asukaColor(s.color) { color = x } else if !s.link.isEmpty { color = accentNSColor }
            a[.foregroundColor] = color
            if s.background != 0, let bg = asukaColor(s.background) { a[.backgroundColor] = bg }
            if s.underline { a[.underlineStyle] = NSUnderlineStyle.single.rawValue }
            if s.strike { a[.strikethroughStyle] = NSUnderlineStyle.single.rawValue }
            if !s.link.isEmpty { a[.link] = s.link }
            if s.inline { a[.asukaInline] = true }
            styles.append(a)
        }
        let out = NSMutableAttributedString()
        var inline = 0
        for (p, runs) in c.paragraphs.enumerated() {
            var last: [NSAttributedString.Key: Any]?
            for run in runs {
                guard run.style < styles.count else { continue }
                let attrs = styles[run.style]
                last = attrs
                if attrs[.asukaInline] != nil {
                    // An inline component: an attachment of its size, on
                    // the baseline.
                    let att = NSTextAttachment()
                    if inline < r.inlines.count {
                        let x = r.inlines[inline]
                        att.bounds = CGRect(x: 0, y: 0, width: x.w, height: x.h)
                    }
                    inline += 1
                    let m = NSMutableAttributedString(attributedString: NSAttributedString(attachment: att))
                    m.addAttributes(attrs, range: NSRange(location: 0, length: m.length))
                    out.append(m)
                    continue
                }
                out.append(NSAttributedString(string: run.text, attributes: attrs))
            }
            if p < c.paragraphs.count - 1 {
                out.append(NSAttributedString(string: "\n", attributes: last ?? styles.first ?? [.font: font(n), .paragraphStyle: para]))
            }
        }
        r.view.textStorage?.setAttributedString(out)
        if let tc = r.view.textContainer {
            tc.maximumNumberOfLines = n.lineLimit
            tc.lineBreakMode = n.lineLimit > 0 ? .byTruncatingTail : .byWordWrapping
        }
        r.view.linkTextAttributes = [.foregroundColor: accentNSColor, .cursor: NSCursor.pointingHand]
        r.selectOK = selectableOf(n) != nil
        r.view.setAccessibilityLabel(n.a11yLabel.isEmpty ? nil : n.a11yLabel)
        if let s = selectableOf(n) { showSelection(s) }
    }

    func measureRich(_ n: AsukaNode, _ c: AsukaOffer) -> CGSize {
        guard let r = n.rich else { return .zero }
        buildRich(n)
        guard (r.view.textStorage?.length ?? 0) > 0 else { return .zero }
        var minContent: CGFloat = 0
        if c.minContent && !c.widthKnown { minContent = richWidestWord(n) }
        let w = c.fitWidth(minContent)
        var s = r.layoutAt(w)
        if w < asukaBig { s.width = min(s.width.rounded(.up), w) }
        // Typeset again at the width it shows at, if it shows yet: before
        // it's placed, its frame typesets it (placeInlines).
        if r.width > 0 && r.width != w { _ = r.layoutAt(r.width) }
        return CGSize(width: s.width.rounded(.up), height: s.height.rounded(.up))
    }

    func richWidestWord(_ n: AsukaNode) -> CGFloat {
        guard let r = n.rich else { return 0 }
        let str = r.view.textStorage?.string ?? ""
        let attrs: [NSAttributedString.Key: Any] = [.font: font(n)]
        var widest: CGFloat = 0
        for w in str.split(whereSeparator: { $0.isWhitespace }) {
            widest = max(widest, (String(w) as NSString).size(withAttributes: attrs).width)
        }
        return widest.rounded(.up)
    }

    /// Lays the text out at the view's width and puts each inline
    /// component's view where its attachment is.
    func placeInlines(_ n: AsukaNode) {
        guard let r = n.rich else { return }
        buildRich(n)
        // Its text inside its padding (ContentInset): the container is as
        // wide as what's left, at the text view's container origin.
        r.width = n.frame.width - n.inset.right - n.inset.left
        if r.width > 0 { _ = r.layoutAt(r.width) }
        guard !r.inlines.isEmpty, let ts = r.view.textStorage, let lm = r.view.layoutManager, let tc = r.view.textContainer else { return }
        let ox = n.inset.left, oy = n.inset.top
        let s = ts.string as NSString
        var k = 0
        for i in 0..<s.length where s.character(at: i) == 0xFFFC && k < r.inlines.count {
            let x = r.inlines[k]
            k += 1
            let g = lm.glyphIndexForCharacter(at: i)
            let rect = lm.boundingRect(forGlyphRange: NSRange(location: g, length: 1), in: tc)
            guard let c = nodes[x.id], let v = c.view else { continue }
            // The attachment's box: the line's, its bottom on the baseline.
            var y = rect.minY + rect.height - x.h
            let loc = lm.location(forGlyphAt: g)
            if loc.y > 0 {
                let frag = lm.lineFragmentRect(forGlyphAt: g, effectiveRange: nil)
                y = frag.minY + loc.y - x.h
            }
            v.frame = CGRect(x: ox + rect.minX, y: oy + y, width: x.w, height: x.h)
        }
    }

    /// Whether a link is under p (in the view).
    func linkAt(_ v: NSTextView, _ p: CGPoint) -> Bool {
        guard let lm = v.layoutManager, let tc = v.textContainer, let ts = v.textStorage, ts.length > 0 else { return false }
        let o = v.textContainerOrigin
        let q = CGPoint(x: p.x - o.x, y: p.y - o.y)
        var frac: CGFloat = 0
        let g = lm.glyphIndex(for: q, in: tc, fractionOfDistanceThroughGlyph: &frac)
        let rect = lm.boundingRect(forGlyphRange: NSRange(location: g, length: 1), in: tc)
        guard rect.contains(q) || (q.x >= rect.minX && q.x <= rect.maxX && q.y >= rect.minY && q.y <= rect.maxY) else { return false }
        let i = lm.characterIndexForGlyph(at: g)
        guard i < ts.length else { return false }
        return ts.attribute(.link, at: i, effectiveRange: nil) != nil
    }

    /// A click on a rich text's link: Link, if the app listens (else
    /// NSTextView opens it).
    func linkClicked(_ tv: NSTextView, _ link: Any) -> Bool {
        guard let n = node(of: tv), n.rich != nil, n.listening(.link) else { return false }
        let s = asukaLinkString(link)
        emit(n, .link) { AsukaEvents.link($0, s) }
        return true
    }

    func richJSON(_ n: AsukaNode, _ d: inout [String: Any]) {
        guard let r = n.rich else { return }
        buildRich(n)
        let ts = r.view.textStorage ?? NSTextStorage()
        var links: [[String: Any]] = []
        ts.enumerateAttribute(.link, in: NSRange(location: 0, length: ts.length)) { v, range, _ in
            if let v { links.append(["url": asukaLinkString(v), "range": [range.location, range.length]]) }
        }
        var j: [String: Any] = ["string": ts.string.replacingOccurrences(of: "\u{FFFC}", with: ""), "links": links,
                                "selectable": r.selectOK, "runs": asukaRunsJSON(ts), "width": Double(r.width),
                                "containerInset": [Double(r.view.textContainerInset.width), Double(r.view.textContainerInset.height)],
                                "maxLines": r.view.textContainer?.maximumNumberOfLines ?? 0]
        if let lm = r.view.layoutManager, let tc = r.view.textContainer { j["used"] = asukaRect(lm.usedRect(for: tc)) }
        if ts.length > 0, let p = ts.attribute(.paragraphStyle, at: 0, effectiveRange: nil) as? NSParagraphStyle {
            j["alignment"] = asukaAlignName(p.alignment)
            j["writingDirection"] = p.baseWritingDirection.rawValue
        }
        if let s = selectionOf(n) { j["selection"] = [s.location, s.location + s.length] }
        d["rich"] = j
    }
}
