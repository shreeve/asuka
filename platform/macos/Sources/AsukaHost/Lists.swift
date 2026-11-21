// Lists (List): core-virtualized, as on Windows.
//
// A List is a scroll view whose rows the host places itself: as it
// scrolls, rows coming into view take a row view (a recycled one, or a new
// one) and its cells, host-made views the core binds rows into through
// asuka_list_bind; rows going out are kept for reuse. A row's height is its
// cells' (a grid's row holds Columns of them), from their frames; rows not
// yet bound are estimated. (NSTableView would ask for heights through a
// delegate as it lays out, while the core is the one that knows them, and
// only once it has laid the row out.)
//
// With sections, each section's header is a row of its own (one cell,
// bound with index -1 - the section's); a plain or bare list's header stays
// at the top of the list while its section's rows scroll under it, as
// NSTableView's floating group rows do.
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif
#if canImport(CAsuka)
@_implementationOnly import CAsuka
#endif

/// A section as the core sent it: its first item, and its ID.
struct AsukaListSection {
    var start: Int
    var id: Int64
}

/// A row the list shows: a section's header, or items (a grid's row holds
/// up to Columns of them).
struct AsukaListRow {
    var header = false
    var section = 0
    var first = 0 // the first item's index
    var count = 0
}

/// A row's view: its cells side by side, its separator.
@MainActor final class AsukaRowView {
    let view = AsukaView(frame: .zero)
    var cells: [AsukaListCell] = []
    var sep: NSBox?
    var row = -1

    init() { view.wantsLayer = true }
}

/// A List's state: its rows, their heights and tops, the row views shown
/// and the ones kept for reuse.
@MainActor final class AsukaList: NSObject {
    weak var n: AsukaNode?
    let scroll: AsukaScrollView
    /// The flipped document view the rows are in.
    let doc = AsukaView(frame: .zero)
    var ids: [Int64] = []
    var rowOf: [Int64: Int] = [:]
    /// By item ID, or a header's by its section's ID.
    var heights: [Int64: CGFloat] = [:]
    /// The rows shown: headers and items.
    var rows: [AsukaListRow] = []
    /// Each row's top, then the total height.
    var tops: [CGFloat] = []
    var shown: [Int: AsukaRowView] = [:]
    var free: [AsukaRowView] = []
    var columns = 1
    var gap: CGFloat = 0
    var width: CGFloat = 0
    var est: CGFloat = 44
    // An inset grouped list's: its look, its rows' inset from each side
    // (and from its top and bottom), and the boxes behind them (a
    // section's each).
    var appearance = AsukaListAppearance.plain
    var inset: CGFloat = 0
    var groups: [NSView] = []
    var sections: [AsukaListSection] = []

    // Anchored at the bottom: rows that don't fill it sit at its bottom
    // (lead is the room above them), and while it's at its end it stays
    // there; settling marks a scroll the host makes itself.
    var bottom = false
    var stuck = true
    var lead: CGFloat = 0
    var settling = false

    var selecting = false

    var pending: [Int64] = []
    var pendingS: [AsukaListSection] = []
    /// pending waits for the core to return.
    var updated = false
    var tiling = false
    var queued = false
    var rebind = false
    /// An update was shown.
    var applied = false

    var rt: AsukaRuntime { AsukaRuntime.shared }

    init(_ n: AsukaNode) {
        self.n = n
        scroll = AsukaScrollView(frame: .zero)
        super.init()
        scroll.hasVerticalScroller = true
        scroll.autohidesScrollers = true
        scroll.automaticallyAdjustsContentInsets = false
        scroll.drawsBackground = false
        scroll.documentView = doc
        let clip = scroll.contentView
        clip.drawsBackground = false
        clip.postsBoundsChangedNotifications = true
        NotificationCenter.default.addObserver(self, selector: #selector(clipScrolled(_:)), name: NSView.boundsDidChangeNotification, object: clip)
    }

    /// The clip view scrolled. One that reports Scrolled (the runtime's
    /// observer) tiles there.
    @objc func clipScrolled(_ note: Notification) {
        guard let n, !n.observesScroll else { return }
        rt.listScrolled(scroll.contentView)
    }

    func stop() {
        NotificationCenter.default.removeObserver(self, name: NSView.boundsDidChangeNotification, object: scroll.contentView)
    }

    func setProp(_ p: UInt16, _ v: AsukaValue) {
        switch p {
        case AsukaProp.List.columns:
            columns = max(Int(v.int), 1)
        case AsukaProp.List.gap:
            gap = CGFloat(v.float)
        case AsukaProp.List.appearance:
            appearance = AsukaListAppearance(rawValue: v.enumValue) ?? .plain
        case AsukaProp.List.inset:
            inset = CGFloat(v.float)
        case AsukaProp.List.anchor:
            bottom = AsukaScrollAnchor(rawValue: v.enumValue) == .bottom
            stuck = true
        case AsukaProp.List.selecting:
            setSelecting(v.bool || v.int != 0)
            return
        default:
            return
        }
        rebind = true
        queue()
    }

    /// Whether it's an inset grouped list: its rows in one rounded box, as
    /// System Settings' grouped forms are.
    var grouped: Bool { appearance == .insetGrouped && columns <= 1 }

    /// Whether its headers stay at its top: a plain or bare list's.
    var sticky: Bool { !sections.isEmpty && appearance != .insetGrouped }

    /// The space above the first row and below the last.
    var pad: CGFloat { grouped ? inset : 0 }

    /// The first group's box (the only one without sections).
    var group: NSView? { groups.first }

    /// Puts the groups' boxes behind the rows (a section's each): a faint
    /// fill over the window's background, rounded, as System Settings'
    /// groups are.
    func placeGroup() {
        let boxes = grouped && !ids.isEmpty ? max(sections.count, 1) : 0
        while groups.count < boxes {
            let g = NSView(frame: .zero)
            g.wantsLayer = true
            g.layer?.cornerRadius = 10
            doc.addSubview(g, positioned: .below, relativeTo: nil) // below the rows
            groups.append(g)
            colors()
        }
        for (i, g) in groups.enumerated() {
            if i >= boxes {
                g.isHidden = true
                continue
            }
            // From its first item row to its last.
            var first = -1, last = -1
            for (r, row) in rows.enumerated() where !row.header && (sections.isEmpty || row.section == i) {
                if first < 0 { first = r }
                last = r
            }
            if first < 0 {
                g.isHidden = true
                continue
            }
            g.isHidden = false
            g.frame = CGRect(x: inset, y: tops[first], width: max(width - 2 * inset, 0),
                             height: tops[last] + rowHeight(last) - gapBelow(last) - tops[first])
        }
    }

    /// Sets the groups' color again (a layer's CGColor is fixed), and the
    /// pinned headers' background.
    func colors() {
        let fill: NSColor
        if #available(macOS 14.0, *) { fill = .quaternarySystemFill } else { fill = .quaternaryLabelColor }
        rt.withAppearance(n) {
            for g in groups { g.layer?.backgroundColor = fill.cgColor }
            for rv in shown.values { headerBackground(rv) }
            for rv in shown.values { for c in rv.cells { highlight(c) } }
        }
    }

    /// Takes the core's new item IDs and sections; rows bind again once
    /// the core has returned, so binding never re-enters a flush.
    func update(_ args: UnsafeRawBufferPointer) {
        guard args.count >= 4 else { return }
        var r = AsukaByteReader(Array(args))
        let count = Int(r.u32())
        var ids: [Int64] = []
        ids.reserveCapacity(min(count, args.count / 8))
        for _ in 0..<count {
            guard r.i + 8 <= args.count else { break }
            ids.append(Int64(bitPattern: r.u64()))
        }
        var secs: [AsukaListSection] = []
        if r.i + 4 <= args.count {
            let ns = Int(r.u32())
            for _ in 0..<ns {
                guard r.i + 12 <= args.count else { break }
                let start = Int(r.u32())
                secs.append(AsukaListSection(start: start, id: Int64(bitPattern: r.u64())))
            }
        }
        pending = ids
        pendingS = secs
        updated = true
        rebind = true
        queue()
    }

    /// Applies an update waiting, if any. Rows added or removed above the
    /// ones in view keep those where they are, unless the list is at the
    /// edge it's anchored to: at its top, new rows there show; at its end,
    /// anchored at the bottom, it goes to its new end.
    func flush() {
        guard updated else {
            tile()
            return
        }
        guard rt.depth == 0 else {
            // In a call into the core (a command): the rows' place and the
            // content's height now, binding once it has returned.
            take()
            measureTops()
            doc.setFrameSize(CGSize(width: width, height: tops.last ?? 0))
            queue()
            return
        }
        var (keep, dy, ok) = anchorRow()
        if bottom && stuck || !bottom && atStart { ok = false }
        let appended = applied && pending.count > ids.count && (ids.isEmpty || pending.last != ids.last)
        take()
        // New rows at the end of a chat at its end scroll into view.
        let animate = appended && bottom && stuck && scroll.window?.isVisible == true
        tileKeeping(keep, dy, ok, animate)
        applied = applied || !ids.isEmpty
        if animate { toEnd(true) }
    }

    /// Takes the update waiting: its items and sections, the heights of
    /// those still in it.
    private func take() {
        guard updated else { return }
        updated = false
        ids = pending
        sections = pendingS
        pending = []
        pendingS = []
        rowOf.removeAll(keepingCapacity: true)
        for (i, id) in ids.enumerated() { rowOf[id] = i }
        var live: [Int64: CGFloat] = [:]
        live.reserveCapacity(ids.count + sections.count)
        for id in ids { if let v = heights[id] { live[id] = v } }
        for s in sections { if let v = heights[s.id] { live[s.id] = v } }
        heights = live
        makeRows()
    }

    /// Lays the items and the sections' headers out in rows.
    func makeRows() {
        rows.removeAll(keepingCapacity: true)
        func add(_ sec: Int, _ from: Int, _ to: Int) {
            var i = from
            while i < to {
                rows.append(AsukaListRow(section: sec, first: i, count: min(columns, to - i)))
                i += columns
            }
        }
        if sections.isEmpty {
            add(0, 0, ids.count)
            return
        }
        for (s, sec) in sections.enumerated() {
            let end = s + 1 < sections.count ? sections[s + 1].start : ids.count
            rows.append(AsukaListRow(header: true, section: s))
            add(s, min(sec.start, ids.count), min(max(end, sec.start), ids.count))
        }
    }

    /// Tiles once the core has returned.
    func queue() {
        guard !queued else { return }
        queued = true
        rt.afterCore { [weak self] in
            guard let self else { return }
            self.queued = false
            guard let n = self.n, self.rt.alive(n) else { return }
            autoreleasepool { self.flush() }
        }
    }

    /// The ID a row's height is kept under: its first item's, or its
    /// section's.
    func rowKey(_ r: Int) -> Int64 {
        let row = rows[r]
        return row.header ? sections[row.section].id : ids[row.first]
    }

    /// A row's height: its cells' tallest, estimated until they are bound
    /// (a grid's rows add the gap below, but a section's last).
    func rowHeight(_ r: Int) -> CGFloat {
        let row = rows[r]
        var height: CGFloat = 0
        var known = false
        if row.header, let v = heights[sections[row.section].id] {
            height = v
            known = true
        }
        for i in 0..<row.count {
            if let v = heights[ids[row.first + i]] {
                height = max(height, v)
                known = true
            }
        }
        if !known { height = est }
        return height + gapBelow(r)
    }

    /// The gap below a grid's row, when another row of its section follows.
    func gapBelow(_ r: Int) -> CGFloat {
        columns > 1 && !rows[r].header && r + 1 < rows.count && !rows[r + 1].header ? gap : 0
    }

    func measureTops() {
        tops.removeAll(keepingCapacity: true)
        var y = pad
        for r in 0..<rows.count {
            tops.append(y)
            y += rowHeight(r)
        }
        tops.append(y + pad)
        // Anchored at the bottom, rows that don't fill it sit at its bottom.
        lead = 0
        if bottom {
            let vh = scroll.contentView.bounds.height
            if let total = tops.last, total < vh {
                lead = vh - total
                for i in tops.indices { tops[i] += lead }
            }
        }
    }

    /// The clip view's bounds: what the list shows.
    var visible: CGRect { scroll.contentView.bounds }

    /// Shows the rows in view (and a screen's worth around them), binds the
    /// ones new to the view, and places them. Rows above the one at the top
    /// that change height as they bind (an estimate made real) don't move
    /// what's in view; a list anchored at its bottom and at its end stays
    /// there.
    func tile() {
        let (key, dy, ok) = anchorRow()
        tileKeeping(key, dy, ok, false)
    }

    /// Tiles, keeping the row with key dy below the top (if ok); holdEnd
    /// leaves a list at its end where it is, for a scroll to its new end to
    /// animate.
    func tileKeeping(_ key: Int64, _ dy: CGFloat, _ ok: Bool, _ holdEnd: Bool) {
        guard !tiling, let n else { return }
        guard rt.depth == 0 else {
            queue() // binding calls into the core: once it has returned
            return
        }
        tiling = true
        // The rows bound are placed below, after their bindings return.
        rt.holdHover()
        defer {
            tiling = false
            rt.releaseHover()
        }
        for _ in 0..<4 {
            measureTops()
            let total = tops.last ?? 0
            doc.setFrameSize(CGSize(width: width, height: total))
            if bottom && stuck {
                if !settling && !holdEnd { toEnd(false) }
            } else if ok {
                restore(key, dy)
            }
            let vb = visible
            let top = vb.minY - vb.height / 2, bot = vb.minY + vb.height * 1.5
            let count = rows.count
            let first = asukaSearch(count) { tops[$0 + 1] > top }
            let last = asukaSearch(count) { tops[$0] >= bot }
            let pin = pinnedRow(vb.minY)
            // Rows gone from view go back to the pool.
            for (r, rv) in shown where (r < first || r >= last) && r != pin {
                shown.removeValue(forKey: r)
                rv.view.isHidden = true
                free.append(rv)
            }
            var changed = false
            func show(_ r: Int) {
                var rv = shown[r]
                let fresh = rv == nil
                if rv == nil {
                    rv = takeRow()
                    shown[r] = rv
                }
                guard let rv else { return }
                if fresh || rebind || rv.row != r {
                    rv.row = r
                    let before = rowHeight(r)
                    bind(n, rv, r)
                    // The binding may have changed what the list shows (an
                    // app's handler ran): the rows are made again.
                    if r >= rows.count { return }
                    if rowHeight(r) != before { changed = true }
                }
            }
            if first < last { for r in first..<last where r < rows.count { show(r) } }
            if pin >= 0 && (pin < first || pin >= last) && pin < rows.count { show(pin) }
            rebind = false
            if !changed { break }
        }
        if tops.count != rows.count + 1 { measureTops() }
        for (r, rv) in shown {
            guard r < rows.count else {
                shown.removeValue(forKey: r)
                rv.view.isHidden = true
                free.append(rv)
                continue
            }
            place(rv, r)
        }
        placeGroup()
    }

    /// The row at the top of what the list shows, by its key, and how far
    /// its top is below the top of the view.
    func anchorRow() -> (Int64, CGFloat, Bool) {
        guard !rows.isEmpty, tops.count == rows.count + 1 else { return (0, 0, false) }
        let y = visible.minY
        let r = asukaSearch(rows.count) { tops[$0 + 1] > y }
        guard r < rows.count else { return (0, 0, false) }
        return (rowKey(r), tops[r] - y, true)
    }

    /// Scrolls so that the row with key is dy below the top again.
    func restore(_ key: Int64, _ dy: CGFloat) {
        for r in rows.indices where rowKey(r) == key {
            let y = tops[r] - dy
            if y != visible.minY { setOffset(y, false) }
            return
        }
    }

    /// Scrolls to the list's end.
    func toEnd(_ animated: Bool) {
        let vb = visible
        let end = max((tops.last ?? 0) - vb.height, 0)
        if end != vb.minY { setOffset(end, animated) }
    }

    /// Scrolls the list itself (not the user): what it shows doesn't move a
    /// list anchored at its bottom off its end, or back, until it's done.
    func setOffset(_ y: CGFloat, _ animated: Bool) {
        guard let n else { return }
        let x = visible.minX
        settling = true
        if !animated {
            rt.scrollToOffset(n, x, y, false)
            settling = false
            return
        }
        NSAnimationContext.runAnimationGroup({ ctx in
            ctx.duration = 0.25
            rt.scrollToOffset(n, x, y, true)
        }, completionHandler: { [weak self] in
            MainActor.assumeIsolated {
                guard let self else { return }
                self.settling = false
                // Not a list gone while it scrolled (its screen closed).
                if self.bottom, let n = self.n, self.rt.alive(n) { self.stuck = self.atEnd }
            }
        })
    }

    var atStart: Bool { visible.minY <= 0.5 }

    var atEnd: Bool {
        let vb = visible
        return tops.isEmpty || vb.minY + vb.height >= (tops.last ?? 0) - 1
    }

    /// The header row that stays at the top of a plain list: its section's,
    /// once its own place has scrolled above y (-1: none).
    func pinnedRow(_ y: CGFloat) -> Int {
        guard sticky, !rows.isEmpty, tops.count == rows.count + 1 else { return -1 }
        let r = asukaSearch(rows.count) { tops[$0 + 1] > y }
        guard r < rows.count else { return -1 }
        let s = rows[r].section
        var h = r
        while h >= 0 {
            if rows[h].header && rows[h].section == s { return tops[h] >= y ? -1 : h }
            h -= 1
        }
        return -1
    }

    func takeRow() -> AsukaRowView {
        if let rv = free.popLast() {
            rv.view.isHidden = false
            return rv
        }
        let rv = AsukaRowView()
        doc.addSubview(rv.view)
        return rv
    }

    /// Binds row r's items (or its section's header) into the row view's
    /// cells.
    func bind(_ n: AsukaNode, _ rv: AsukaRowView, _ r: Int) {
        let row = rows[r]
        let k = row.header ? 1 : row.count
        while rv.cells.count < k { rv.cells.append(newCell(rv)) }
        for (i, c) in rv.cells.enumerated() { c.content.isHidden = i >= k }
        headerBackground(rv)
        for i in 0..<k {
            let c = rv.cells[i]
            c.header = row.header
            var index = row.first + i
            if row.header {
                c.item = sections[row.section].id
                index = -1 - row.section
            } else {
                c.item = ids[index]
            }
            if let v = heights[c.item] { c.height = v }
            let list = n.id, cell = c.node.id, at = Int64(index)
            rt.enter { asuka_list_bind(list, at, cell) }
            guard rt.alive(n), r < rows.count else { return }
            highlight(c)
        }
    }

    /// Gives a header row of a plain list the window's background, so the
    /// rows it's pinned over don't show through.
    func headerBackground(_ rv: AsukaRowView) {
        guard rv.row >= 0, rv.row < rows.count else { return }
        if rows[rv.row].header && sticky {
            rt.withAppearance(n) { rv.view.layer?.backgroundColor = NSColor.windowBackgroundColor.cgColor }
        } else {
            rv.view.layer?.backgroundColor = nil
        }
    }

    func newCell(_ rv: AsukaRowView) -> AsukaListCell {
        let c = AsukaListCell(self, rv)
        c.content.wantsLayer = true
        c.node = rt.makeCell(c)
        rv.view.addSubview(c.content)
        return c
    }

    /// Puts a row view at its row's top (a pinned header at the top of what
    /// shows, pushed up by the next header) and lays out its cells: a grid's
    /// side by side, each (width - gap × (columns - 1)) / columns wide.
    func place(_ rv: AsukaRowView, _ r: Int) {
        let height = rowHeight(r)
        var x: CGFloat = 0, w = width
        if grouped {
            x = inset
            w = max(width - 2 * inset, 0)
        }
        var y = tops[r]
        let row = rows[r]
        if row.header && sticky {
            let vb = visible
            if y < vb.minY {
                y = vb.minY
                for k in (r + 1)..<max(rows.count, r + 1) where rows[k].header {
                    y = min(y, tops[k] - height)
                    break
                }
            }
            // Over the rows that scroll under it.
            doc.addSubview(rv.view, positioned: .above, relativeTo: nil)
        }
        rv.view.frame = CGRect(x: x, y: y, width: w, height: height)
        if columns <= 1 || row.header {
            for c in rv.cells { c.content.frame = CGRect(x: 0, y: 0, width: w, height: c.height) }
            separator(rv, r, w, height)
            return
        }
        rv.sep?.isHidden = true
        let cw = (width - gap * CGFloat(columns - 1)) / CGFloat(columns)
        let rtl = n.map { rt.isRTL($0) } ?? false
        for (i, c) in rv.cells.enumerated() {
            // A grid's columns run the way the list reads.
            let col = rtl ? columns - 1 - i : i
            c.content.frame = CGRect(x: CGFloat(col) * (cw + gap), y: 0, width: cw, height: c.height)
        }
    }

    /// A list row's hairline at its bottom, but the last row's and a
    /// section's last's (nor a header's); in a group, inset from the box's
    /// sides.
    func separator(_ rv: AsukaRowView, _ r: Int, _ w: CGFloat, _ height: CGFloat) {
        guard appearance != .bare else { return }
        if rv.sep == nil {
            let b = NSBox(frame: .zero)
            b.boxType = .separator
            rv.view.addSubview(b)
            rv.sep = b
        }
        let x: CGFloat = grouped ? 10 : 0
        rv.sep?.frame = CGRect(x: x, y: height - 1, width: max(w - 2 * x, 0), height: 1)
        rv.sep?.isHidden = r >= rows.count - 1 || rows[r].header || rows[r + 1].header
    }

    /// Lays the rows out at the list's new width; one at its end stays
    /// there.
    func resized() {
        guard let n else { return }
        let w = n.frame.width
        if w != width {
            width = w
            rebind = true // the core lays the rows out at the new width
        }
        queue()
    }

    /// Tiles a list as it scrolls; the user's scrolling moves a list
    /// anchored at its bottom off its end, or back.
    func scrolled() {
        if bottom && !settling && !tiling { stuck = atEnd }
        tile()
    }

    func scrollTo(_ index: Int, _ animated: Bool) {
        flush()
        guard index >= 0, index < ids.count, tops.count == rows.count + 1 else { return }
        // The item's row (headers in between have no items: not a search).
        guard let row = rows.firstIndex(where: { !$0.header && index >= $0.first && index < $0.first + $0.count }) else { return }
        let b = visible
        var top = tops[row]
        let bot = tops[row + 1]
        if sticky {
            // Below the header pinned over it.
            var r = row
            while r >= 0 {
                if rows[r].header {
                    top -= rowHeight(r)
                    break
                }
                r -= 1
            }
        }
        var y = b.minY
        if top < y { y = top } else if bot > y + b.height { y = bot - b.height }
        if bottom { stuck = index == ids.count - 1 }
        setOffset(y, animated)
    }

    /// Puts the list in selection mode: a click on a row selects or
    /// deselects it (RowSelected), and selected rows are highlighted, as a
    /// table's selected rows are.
    func setSelecting(_ on: Bool) {
        guard on != selecting else { return }
        selecting = on
        for rv in shown.values { for c in rv.cells { highlight(c) } }
    }

    /// Shows whether c's row is selected: the selected content background
    /// (the accent, unemphasized when the window isn't key).
    func highlight(_ c: AsukaListCell) {
        guard let layer = c.content.layer, !c.header else { return }
        if selecting && c.selected {
            rt.withAppearance(n) {
                var color = NSColor.selectedContentBackgroundColor
                if let w = c.content.window, !w.isKeyWindow { color = .unemphasizedSelectedContentBackgroundColor }
                layer.backgroundColor = color.cgColor
            }
            layer.cornerRadius = 6
            return
        }
        layer.backgroundColor = nil
    }

    /// Every cell the list made: its rows' shown and kept.
    var allCells: [AsukaListCell] { (Array(shown.values) + free).flatMap { $0.cells } }
}

/// A list's cell: a view of the host's the core binds rows into.
@MainActor final class AsukaListCell: NSObject {
    /// The view its row's views go in.
    let content: NSView = AsukaView(frame: .zero)
    var swipe: [AsukaActionItem] = []
    var leading: [AsukaActionItem] = []
    weak var list: AsukaList?
    weak var rowView: AsukaRowView?
    /// Its node (the runtime's makeCell).
    var node: AsukaNode!
    var item: Int64 = 0
    var height: CGFloat = 0
    var header = false
    /// The core says so.
    var selected = false
    /// Its context menu: its swipe actions; the Action each item sends.
    var menu: NSMenu?
    var menuTags: [Int64] = []

    init(_ list: AsukaList, _ rv: AsukaRowView) {
        self.list = list
        rowView = rv
        super.init()
    }

    /// Takes a row's height from its cell's frame.
    func setHeight(_ h: CGFloat) {
        height = h
        guard let l = list else { return }
        if let old = l.heights[item], old == h { return }
        if l.heights[item] == nil && l.heights.count < 64 && !header {
            // The estimate follows the first rows'.
            l.est = (l.est * CGFloat(l.heights.count) + h) / CGFloat(l.heights.count + 1)
        }
        l.heights[item] = h
        if !l.tiling { l.queue() }
    }

    /// The core's word on whether its row is selected.
    func setSelected(_ on: Bool) {
        selected = on
        list?.highlight(self)
    }

    /// An item of its context menu: a swipe action (a leading one's Action
    /// is -1 - its index).
    @objc func swipeItem(_ sender: NSMenuItem) {
        let i = sender.tag
        guard i >= 0, i < menuTags.count else { return }
        let tag = menuTags[i]
        AsukaRuntime.shared.emit(node, .action) { AsukaEvents.action($0, tag) }
    }
}

/// The first index in 0..<count for which pred holds (count if none),
/// pred false then true across it (sort.Search).
func asukaSearch(_ count: Int, _ pred: (Int) -> Bool) -> Int {
    var lo = 0, hi = count
    while lo < hi {
        let mid = (lo + hi) / 2
        if pred(mid) { hi = mid } else { lo = mid + 1 }
    }
    return lo
}

@MainActor extension AsukaRuntime {
    func makeList(_ n: AsukaNode) -> NSView {
        let l = AsukaList(n)
        n.list = l
        return l.scroll
    }

    func setListProp(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        n.list?.setProp(p, v)
    }

    func listUpdate(_ n: AsukaNode, _ args: UnsafeRawBufferPointer) {
        n.list?.update(args)
    }

    func listScrollTo(_ n: AsukaNode, _ index: Int, _ animated: Bool) {
        guard let l = n.list else { return }
        // Once the core has returned: what it sent with it is applied, and
        // binding doesn't re-enter it.
        afterCore { [weak self] in
            guard let self, self.alive(n) else { return }
            l.scrollTo(index, animated)
        }
    }

    /// Binds what a list has pending before it's scrolled.
    func listFlush(_ n: AsukaNode) {
        n.list?.flush()
    }

    /// A list's clip view scrolled: the rows shown bound.
    func listScrolled(_ clip: NSClipView) {
        guard let n = node(of: clip.superview), let l = n.list else { return }
        l.scrolled()
    }

    /// The rows a list shows, and their cells' nodes (for dumps).
    func listRows(_ n: AsukaNode) -> [(Int, [AsukaNode])] {
        guard let l = n.list else { return [] }
        return l.shown.keys.sorted().compactMap { r in
            guard let rv = l.shown[r] else { return nil }
            return (r, rv.cells.filter { !$0.content.isHidden }.map { $0.node })
        }
    }

    func listBackground(_ n: AsukaNode) {
        n.list?.scroll.drawsBackground = false
    }

    /// A row's context menu: its swipe actions, leading then trailing (a
    /// Mac has no swipes for them but the trackpad's, which a list of its
    /// own wouldn't see). A destructive one's title is red.
    func cellMenu(_ c: AsukaListCell) -> NSMenu? {
        if let m = c.menu { byMenu.removeValue(forKey: ObjectIdentifier(m)) }
        c.menu = nil
        c.menuTags = []
        var all: [AsukaActionItem] = []
        for (i, a) in c.leading.enumerated() {
            all.append(a)
            c.menuTags.append(Int64(-1 - i))
        }
        for (i, a) in c.swipe.enumerated() {
            all.append(a)
            c.menuTags.append(Int64(i))
        }
        guard !all.isEmpty, !(c.list?.selecting ?? false) else { return nil }
        let m = NSMenu(title: "")
        m.autoenablesItems = false
        m.userInterfaceLayoutDirection = rtl ? .rightToLeft : .leftToRight
        for (i, a) in all.enumerated() {
            // A swipe's: flat.
            let it = NSMenuItem(title: a.title, action: #selector(AsukaListCell.swipeItem(_:)), keyEquivalent: "")
            it.target = c
            it.tag = i
            it.isEnabled = !a.disabled
            if let img = symbolImage(a.icon) { it.image = img }
            if a.role == AsukaAlertButtonRole.destructive.rawValue {
                it.attributedTitle = NSAttributedString(string: a.title, attributes: [.foregroundColor: NSColor.systemRed])
            }
            m.addItem(it)
        }
        c.menu = m
        return m
    }

    /// Toggles the selection of the row v is in, if it's in a selecting
    /// list (false: it isn't). ⌘ on a row of a list that isn't selecting
    /// begins selecting there (RowSelectBegin), as ⌘-click starts a
    /// selection of many in Finder, when the app listens.
    func selectClick(_ v: NSView, _ e: NSEvent?) -> Bool {
        var x: NSView? = v
        while let view = x {
            defer { x = view.superview }
            guard let n = node(of: view), let c = n.cell, let l = c.list else { continue }
            if c.header { return false }
            if !l.selecting {
                guard let e, e.modifierFlags.contains(.command), n.listening(.rowSelectBegin) else { return false }
                emit(n, .rowSelectBegin) { AsukaEvents.rowSelectBegin($0) }
                return true
            }
            c.selected.toggle()
            l.highlight(c)
            let on = c.selected
            emit(n, .rowSelected) { AsukaEvents.rowSelected($0, on) }
            return true
        }
        return false
    }

    // Hooks (Parts.swift).
    func listDestroyed(_ n: AsukaNode) {
        if n.paging { setPaging(n, false) } // its observer goes
    }

    func listReleased(_ n: AsukaNode) {
        releaseRefresh(n)
        guard let l = n.list else { return }
        l.stop()
        // Its cells are the host's: they go with it.
        for c in l.allCells {
            guard let cn = c.node else { continue }
            if let m = c.menu { byMenu.removeValue(forKey: ObjectIdentifier(m)) }
            if nodes[cn.id] === cn { nodes.removeValue(forKey: cn.id) }
            byView.removeValue(forKey: ObjectIdentifier(c.content))
        }
        l.shown = [:]
        l.free = []
    }

    func listPlaced(_ n: AsukaNode) {
        n.list?.resized()
        if let r = n.refresh, r.refreshing { placeSpinner(n, r) }
    }

    func listColors(_ n: AsukaNode) {
        n.list?.colors()
    }

    func listDirection(_ n: AsukaNode, _ changed: Bool) {
        if changed, let l = n.list {
            l.rebind = true
            l.queue()
        }
        if changed, n.dots != nil { n.view?.needsDisplay = true }
    }

    func listBatchApplied(_ frames: Bool) {}

    func listJSON(_ n: AsukaNode, _ d: inout [String: Any]) {
        if let l = n.list {
            let vb = l.visible
            var j: [String: Any] = [
                "rows": l.rows.count, "items": l.ids.count, "sections": l.sections.count, "shown": l.shown.count,
                "free": l.free.count, "total": Double(l.tops.last ?? 0), "lead": Double(l.lead), "stuck": l.stuck,
                "bottom": l.bottom, "grouped": l.grouped, "inset": Double(l.inset), "width": Double(l.width),
                "columns": l.columns, "selecting": l.selecting, "estimate": Double(l.est),
                "visible": [Double(vb.minX), Double(vb.minY), Double(vb.width), Double(vb.height)],
                "shownRows": l.shown.keys.sorted(),
            ]
            if l.tops.count == l.rows.count + 1 {
                j["tops"] = l.shown.keys.sorted().map { [$0: Double(l.tops[$0])] }.reduce(into: [String: Double]()) { a, e in
                    for (k, v) in e { a[String(k)] = v }
                }
                j["rowHeights"] = l.shown.keys.sorted().reduce(into: [String: Double]()) { $0[String($1)] = Double(l.rowHeight($1)) }
            }
            let (key, dy, ok) = l.anchorRow()
            if ok { j["anchor"] = ["key": key, "dy": Double(dy)] }
            if let g = l.group, !g.isHidden { j["group"] = [Double(g.frame.minX), Double(g.frame.minY), Double(g.frame.width), Double(g.frame.height)] }
            j["groups"] = l.groups.filter { !$0.isHidden }.count
            d["list"] = j
        }
        if let c = n.cell, let rv = c.rowView {
            let f = rv.view.frame
            var j: [String: Any] = ["rowFrame": [Double(f.minX), Double(f.minY), Double(f.width), Double(f.height)],
                                    "header": c.header, "selected": c.selected, "item": c.item,
                                    "highlighted": c.content.layer?.backgroundColor != nil]
            if let s = rv.sep { j["separator"] = !s.isHidden }
            if let bg = rv.view.layer?.backgroundColor { j["rowBackground"] = cgColorHex(bg) }
            if !c.swipe.isEmpty || !c.leading.isEmpty {
                j["contextMenu"] = (cellMenu(c)?.items ?? []).map { it -> [String: Any] in
                    var m: [String: Any] = ["title": it.title, "enabled": it.isEnabled]
                    if let a = it.attributedTitle, a.length > 0, let col = a.attribute(.foregroundColor, at: 0, effectiveRange: nil) as? NSColor {
                        m["color"] = colorHex(col)
                    }
                    return m
                }
            }
            d["cell"] = j
        }
        refreshJSON(n, &d)
        dotsJSON(n, &d)
        scrollJSON(n, &d)
        // A pop-up's menu reads the way its button does (locale_test.go's
        // TestControlsMirror).
        if let p = n.view as? NSPopUpButton, let m = p.menu { d["menuRTL"] = m.userInterfaceLayoutDirection == .rightToLeft }
    }
}
