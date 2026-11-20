#if canImport(UIKit)
import UIKit
#if canImport(AsukaOps)
import AsukaOps
#endif

/// A multiline TextField: a UITextView with a placeholder, bordered as a
/// rounded-rect field is. It grows with its text up to maxLines lines,
/// then scrolls.
final class AsukaTextView: UITextView {
    var asukaID: UInt32 = 0
    var maxLines = 0
    let placeholderLabel = UILabel()
    /// TextFieldProps.Plain: no border, background nor inset, the text
    /// alone.
    var plain = false {
        didSet {
            layer.borderWidth = plain ? 0 : 1 / UIScreen.main.scale
            backgroundColor = plain ? .clear : .systemBackground
            updateInsets()
            textContainer.lineFragmentPadding = plain ? 0 : 5
            setNeedsLayout()
        }
    }

    /// ContentInset: its padding, inside its own inset.
    var contentInset_: UIEdgeInsets = .zero {
        didSet { if contentInset_ != oldValue { updateInsets(); setNeedsLayout() } }
    }

    private func updateInsets() {
        let own = plain ? UIEdgeInsets.zero : UIEdgeInsets(top: 7, left: 3, bottom: 7, right: 3)
        textContainerInset = UIEdgeInsets(top: own.top + contentInset_.top, left: own.left + contentInset_.left,
                                          bottom: own.bottom + contentInset_.bottom, right: own.right + contentInset_.right)
    }

    /// As AsukaTextField's: focus coming from another field keeps the
    /// keyboard where it is while their accessories swap.
    override func becomeFirstResponder() -> Bool {
        if !isFirstResponder { AsukaRuntime.shared.controller?.holdKeyboard() }
        return super.becomeFirstResponder()
    }

    var placeholder: String? {
        get { placeholderLabel.text }
        set { placeholderLabel.text = newValue; setNeedsLayout() }
    }

    /// The app's label, or, as a UITextField's, the placeholder.
    var label: String?
    override var accessibilityLabel: String? {
        get { label ?? placeholderLabel.text }
        set { label = newValue }
    }

    override var font: UIFont? {
        didSet { placeholderLabel.font = font }
    }

    override var text: String! {
        didSet { placeholderLabel.isHidden = !(text ?? "").isEmpty }
    }

    /// TextFieldProps.PasteTypes: content of these types pastes into the
    /// app's OnPaste, not the text.
    var pasteTypes: [String] = []
    /// TextFieldProps.Rich: its text is styled (RichField.swift); the
    /// core is setting it; the styles at the cursor as last reported.
    var rich = false
    var settingRich = false
    var lastFormats = AsukaFormat(rawValue: 0xff)
    var lastLink = ""

    override func canPerformAction(_ action: Selector, withSender sender: Any?) -> Bool {
        if action == #selector(paste(_:)), AsukaPaste.available(pasteTypes) { return true }
        return super.canPerformAction(action, withSender: sender)
    }

    override func paste(_ sender: Any?) {
        if AsukaPaste.take(asukaID, pasteTypes) { return }
        super.paste(sender)
    }

    init() {
        super.init(frame: .zero, textContainer: nil)
        font = .preferredFont(forTextStyle: .body)
        adjustsFontForContentSizeCategory = true
        backgroundColor = .systemBackground
        layer.cornerRadius = 5
        layer.borderWidth = 1 / UIScreen.main.scale
        textContainerInset = UIEdgeInsets(top: 7, left: 3, bottom: 7, right: 3)
        isScrollEnabled = false
        placeholderLabel.textColor = .placeholderText
        placeholderLabel.font = font
        placeholderLabel.adjustsFontForContentSizeCategory = true
        placeholderLabel.numberOfLines = 0
        placeholderLabel.isAccessibilityElement = false
        addSubview(placeholderLabel)
        updateBorder()
        if #available(iOS 17, *) {
            registerForTraitChanges([UITraitUserInterfaceStyle.self]) { (v: AsukaTextView, _: UITraitCollection) in v.updateBorder() }
        }
    }
    required init?(coder: NSCoder) { fatalError() }

    /// A text area that grows with its text doesn't scroll: what holds it does.
    /// Keeps the caret in sight in that, above the keyboard, as it moves.
    func revealCaret() {
        guard isFirstResponder, let at = selectedTextRange?.end else { return }
        AsukaRuntime.shared.reveal(self, caretRect(for: at), animated: false)
    }

    /// As AsukaLabel's, its placeholder's too.
    var holdsFont = false
    override func didMoveToWindow() {
        super.didMoveToWindow()
        if holdsFont, window != nil {
            holdsFont = false
            adjustsFontForContentSizeCategory = true
            placeholderLabel.adjustsFontForContentSizeCategory = true
        }
    }

    private func updateBorder() {
        layer.borderColor = UIColor.systemGray4.resolvedColor(with: traitCollection).cgColor
    }

    override func traitCollectionDidChange(_ previous: UITraitCollection?) {
        super.traitCollectionDidChange(previous)
        if #unavailable(iOS 17) { updateBorder() }
    }

    private var laidOutHeight: CGFloat = 0

    override func layoutSubviews() {
        // Grown by a new line: now there's somewhere for the caret to go.
        if bounds.height != laidOutHeight {
            laidOutHeight = bounds.height
            DispatchQueue.main.async { [weak self] in self?.revealCaret() }
        }
        super.layoutSubviews()
        let pad = textContainer.lineFragmentPadding
        let x = textContainerInset.left + pad
        let w = bounds.width - x - textContainerInset.right - pad
        placeholderLabel.frame = CGRect(x: x, y: textContainerInset.top, width: w, height: placeholderLabel.sizeThatFits(CGSize(width: w, height: .greatestFiniteMagnitude)).height)
    }

    /// The height it wants at width: its text's (at least a line), up to
    /// maxLines lines; past them it scrolls.
    func height(at width: CGFloat) -> CGFloat {
        let insets = textContainerInset.top + textContainerInset.bottom
        let line = (font ?? .preferredFont(forTextStyle: .body)).lineHeight
        var h = sizeThatFits(CGSize(width: width, height: .greatestFiniteMagnitude)).height
        h = max(h, ceil(line + insets))
        // A placeholder can run longer than a line: it's what's showing.
        if (text ?? "").isEmpty, !(placeholderLabel.text ?? "").isEmpty {
            let pad = textContainer.lineFragmentPadding
            let w = width - textContainerInset.left - textContainerInset.right - 2 * pad
            h = max(h, ceil(placeholderLabel.sizeThatFits(CGSize(width: w, height: .greatestFiniteMagnitude)).height + insets))
        }
        if maxLines > 0 {
            let cap = ceil(line * CGFloat(maxLines) + insets)
            isScrollEnabled = h > cap
            h = min(h, cap)
        } else {
            isScrollEnabled = false
        }
        return h
    }
}

@MainActor extension AsukaRuntime {
    /// Makes a TextField multiline: its UITextField becomes a AsukaTextView
    /// (it's sent before the field is inserted; one in a view is replaced).
    func makeMultiline(_ n: AsukaNode) {
        guard let old = n.view as? AsukaTextField else { return }
        let t = AsukaTextView()
        t.asukaID = n.id
        t.tag = old.tag
        t.delegate = self
        t.text = old.text
        t.placeholder = old.placeholder
        t.keyboardType = old.keyboardType
        t.autocapitalizationType = old.autocapitalizationType
        t.autocorrectionType = old.autocorrectionType
        t.textContentType = old.textContentType
        t.inputAccessoryView = old.inputAccessoryView
        t.plain = old.plain
        // What props set on the field before multiline came (they go in
        // the order the app wrote them): its padding, and the app's own
        // background, corners and border in place of the text view's.
        t.contentInset_ = old.contentInset_
        t.isHidden = old.isHidden
        t.alpha = old.alpha
        t.transform = old.transform
        if let c = old.backgroundColor { t.backgroundColor = c }
        if n.cornerRadius != 0 {
            t.layer.cornerCurve = old.layer.cornerCurve
            t.layer.cornerRadius = old.layer.cornerRadius
        }
        if old.layer.borderWidth != 0 { t.layer.borderWidth = old.layer.borderWidth }
        if n.borderColor != 0 { t.layer.borderColor = old.layer.borderColor }
        if let p = old.superview {
            p.insertSubview(t, aboveSubview: old)
            t.frame = old.frame
            old.removeFromSuperview()
        }
        n.view = t
        n.applyFont()
        n.applyColor()
        setAttribute(t, from: .unspecified, to: old.semanticContentAttribute, forced: [:], redo: false)
        n.applyTextAlign()
    }
}

extension AsukaRuntime: UITextViewDelegate {
    public func textViewDidChange(_ textView: UITextView) {
        guard let t = textView as? AsukaTextView else { return }
        t.placeholderLabel.isHidden = !t.text.isEmpty
        let id = t.asukaID, s = t.text ?? ""
        emit(id, .textChanged) { AsukaEvents.textChanged(id, s) }
        richChanged(t)
        t.revealCaret()
    }

    public func textViewDidChangeSelection(_ textView: UITextView) {
        guard let t = textView as? AsukaTextView else { return }
        t.revealCaret()
        reportSelection(t.asukaID)
        reportFormats(t)
    }

    public func textViewDidBeginEditing(_ textView: UITextView) {
        guard let t = textView as? AsukaTextView else { return }
        let id = t.asukaID
        focusMoved(to: id, how: 1) // the field's own Focus, not the view's that had it
        emit(id, .focus) { AsukaEvents.focus(id) }
        revealOnFocus()
    }

    public func textViewDidEndEditing(_ textView: UITextView) {
        guard let t = textView as? AsukaTextView else { return }
        let id = t.asukaID
        emit(id, .blur) { AsukaEvents.blur(id) }
    }
}
#endif
