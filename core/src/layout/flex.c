// flex.c: flexbox, following CSS Flexible Box Layout §9.
#include "layout/internal.h"

typedef struct {
	ak_lnode *node;
	ak_lsize size;  // style size, border box; NaN: auto
	ak_lsize min, max;
	ak_ledges margin;
	l_bedges auto_m;
	ak_lsize pb;
	uint8_t align;
	float grow;
	float shrink;
	float basis;     // flex base size, border box
	float inner;     // basis minus padding and border
	float min_main;  // resolved minimum main size; NaN until needed
	float known_x;   // cross size known when measuring the content
	ak_lsize hypo;   // hypothetical border-box size
	float target;    // main size being resolved
	bool frozen;
	float violation;
	float cross;     // used cross size
	float baseline;  // ascent from the top of the margin box (rows)
	float pos_m;
	float pos_x;
	bool content;  // basis came from measuring content
} flex_item;

typedef struct {
	int start, end;
	float cross;
	float pos;
	float ascent;
} flex_line;

static float outer_main(const flex_item *it, int m) { return it->target + l_sum_axis(it->margin, m); }

static float min_main_size(flex_item *it, int m, int x, ak_lsize parent, ak_lsize avail, ak_lpass *p);
static void resolve_flexible(flex_item *line, int nline, float inner_m, float gap, int m, int x, ak_lsize parent, ak_lsize avail, ak_lpass *p);
static float flex_intrinsic_main(flex_item *items, flex_line *lines, int nlines, int m, int x, float gap, bool wrap,
	ak_lsize parent, ak_lsize avail, ak_lpass *p);
static void layout_absolute(ak_lnode *n, ak_lsize size, ak_ledges bor, ak_ledges inset, int m, bool reverse, ak_lpass *p);

static l_output flex(ak_lnode *n, const l_input *in, ak_lpass *p) {
	const ak_lstyle *st = &n->style;
	int m = l_flex_is_row(st->flex_direction) ? 0 : 1;
	int x = 1 - m;
	bool reverse = l_flex_is_reverse(st->flex_direction);
	// The main axis runs against x or y on a reversed direction, and a
	// row's also runs against x in a right-to-left box, whose start is the
	// right.
	bool mirror_m = reverse != (m == 0 && n->rtl);
	// Likewise the cross axis of a right-to-left column.
	bool mirror_x = m == 1 && n->rtl;
	bool wrap = st->flex_wrap != AK_LNOWRAP;
	bool wrap_reverse = st->flex_wrap == AK_LWRAP_REVERSE;

	ak_ledges pad, bor;
	l_padding_border(n, in->parent.w, &pad, &bor);
	ak_ledges inset = l_content_inset(n, pad, bor, l_scroll_gutter(n));
	ak_lsize pbs = l_sum_both(inset);
	ak_lsize pb_only = {pad.left + pad.right + bor.left + bor.right, pad.top + pad.bottom + bor.top + bor.bottom};
	ak_ledges margin = l_margins_of(n, in->parent.w);

	ak_lsize size = in->known;
	ak_lsize min_s = {LNAN, LNAN}, max_s = {LNAN, LNAN};
	if (in->sizing == SIZE_INHERENT) {
		ak_lsize ss = l_style_size(n, in->parent);
		size = (ak_lsize){l_orf(size.w, ss.w), l_orf(size.h, ss.h)};
		min_s = l_style_min(n, in->parent);
		max_s = l_style_max(n, in->parent);
	}
	size.w = l_clampn(size.w, min_s.w, max_s.w);
	size.h = l_clampn(size.h, min_s.h, max_s.h);
	if (in->mode == MODE_SIZE && l_isdef(size.w) && l_isdef(size.h)) {
		return (l_output){.size = {l_maxf(size.w, pb_only.w), l_maxf(size.h, pb_only.h)}, .baseline = LNAN};
	}

	ak_lsize inner_size = {l_maxf(0, size.w - pbs.w), l_maxf(0, size.h - pbs.h)};
	if (l_isnan(size.w)) inner_size.w = LNAN;
	if (l_isnan(size.h)) inner_size.h = LNAN;
	ak_lsize avail;
	for (int a = 0; a < 2; a++) {
		float v = l_get(in->avail, a);
		if (l_isdef(l_get(inner_size, a))) {
			v = l_get(inner_size, a);
		} else if (l_definite(v)) {
			v = l_clampn(v - l_sum_axis(margin, a), l_get(min_s, a), l_get(max_s, a)) - l_get(pbs, a);
			v = l_maxf(v, 0);
		}
		l_set(&avail, a, v);
	}
	ak_lsize gaps = {l_or0(l_resolve(st->column_gap, inner_size.w)), l_or0(l_resolve(st->row_gap, inner_size.h))};
	float gap_m = l_get(gaps, m), gap_x = l_get(gaps, x);

	// 1. Collect the items.
	flex_item *items = L_SCRATCH(p, flex_item, n->nchildren);
	int nitems = 0;
	for (uint32_t ci = 0; ci < n->nchildren; ci++) {
		ak_lnode *ch = n->children[ci];
		const ak_lstyle *cs = &ch->style;
		if (cs->display == AK_LDISPLAY_NONE) {
			if (in->mode == MODE_LAYOUT) {
				l_place(p, ch, 0, 0, (ak_lsize){0, 0});
				l_hide(ch, p);
			}
			continue;
		}
		if (cs->position == AK_LABSOLUTE) continue;
		flex_item it = {.node = ch, .min_main = LNAN, .baseline = LNAN};
		it.size = l_style_size(ch, inner_size);
		it.min = l_style_min(ch, inner_size);
		it.max = l_style_max(ch, inner_size);
		it.margin = l_margins_of(ch, inner_size.w);
		it.auto_m = l_auto_margins(cs);
		ak_ledges cpad, cbor;
		l_padding_border(ch, inner_size.w, &cpad, &cbor);
		it.pb = (ak_lsize){cpad.left + cpad.right + cbor.left + cbor.right, cpad.top + cpad.bottom + cbor.top + cbor.bottom};
		it.align = cs->align_self;
		if (it.align == AK_LALIGN_AUTO) it.align = st->align_items;
		if (it.align == AK_LALIGN_AUTO) it.align = AK_LALIGN_STRETCH;
		it.align = l_resolve_self_align(it.align, m == 1, n->rtl, l_rtl_under(ch, n->rtl));
		it.grow = cs->flex_grow;
		it.shrink = cs->flex_shrink;
		items[nitems++] = it;
	}

	// A single-line column whose width isn't known (sized to its
	// content): its width first, its widest item's, as a browser lays a
	// column out at its intrinsic width, so its items stretch to it and
	// their heights are the ones at it (a text's, wrapped there), not at
	// their own narrowest.
	if (m == 1 && !wrap && nitems > 0 && l_isnan(inner_size.w) && !l_definite(avail.w)) {
		float cross = 0;
		for (int i = 0; i < nitems; i++) {
			flex_item *it = &items[i];
			ak_lsize ca = {avail.w, AK_LMAX_CONTENT};
			ak_lsize o = l_measure_size(it->node, (l_input){.sizing = SIZE_INHERENT, .known = {LNAN, LNAN}, .parent = inner_size, .avail = ca}, p);
			cross = l_maxf(cross, o.w + l_sum_axis(it->margin, 0));
		}
		avail.w = l_maxf(0, l_clampn(cross + pbs.w, min_s.w, max_s.w) - pbs.w);
	}

	// 2. Flex base sizes and hypothetical main sizes (§9.2.3).
	for (int i = 0; i < nitems; i++) {
		flex_item *it = &items[i];
		ak_lnode *ch = it->node;
		float avail_x = l_get(avail, x);
		if (l_definite(avail_x)) {
			avail_x = l_clampn(avail_x, l_get(it->min, x) + l_sum_axis(it->margin, x), l_get(it->max, x) + l_sum_axis(it->margin, x));
		}
		it->known_x = l_get(it->size, x);
		if (l_isnan(it->known_x) && it->align == AK_LALIGN_STRETCH && l_definite(avail_x) && !l_bstart(it->auto_m, x) && !l_bend(it->auto_m, x)) {
			it->known_x = avail_x - l_sum_axis(it->margin, x);
		}
		float b = l_resolve(ch->style.flex_basis, l_get(inner_size, m));
		if (l_isdef(b)) b += l_get(l_box_adjust(ch, inner_size.w), m);
		float ratio = ch->style.aspect_ratio;
		if (l_isdef(b)) {
		} else if (ratio > 0 && l_isdef(it->known_x)) {
			// A definite cross size, set or stretched (§9.2 3B, §9.8): the
			// base size follows from the aspect ratio.
			b = m == 0 ? it->known_x * ratio : it->known_x / ratio;
		} else if (l_isdef(l_get(it->size, m))) {
			b = l_get(it->size, m);
		} else {
			ak_lsize known = {LNAN, LNAN};
			l_set(&known, x, it->known_x);
			ak_lsize ca = {0, 0};
			l_set(&ca, m, AK_LMAX_CONTENT);
			if (l_get(in->avail, m) == AK_LMIN_CONTENT) l_set(&ca, m, AK_LMIN_CONTENT);
			l_set(&ca, x, avail_x);
			b = l_get(l_measure_size(ch, (l_input){.sizing = SIZE_CONTENT, .known = known, .parent = inner_size, .avail = ca}, p), m);
			it->content = true;
		}
		it->basis = l_maxf(b, l_get(it->pb, m));
		it->inner = l_maxf(0, it->basis - l_get(it->pb, m));
		float h = it->basis;
		if (!it->content) h = l_maxf(h, min_main_size(it, m, x, inner_size, avail, p));
		h = l_maybe_min(h, l_get(it->max, m));
		if (l_isdef(l_get(it->min, m))) h = l_maxf(h, l_get(it->min, m));
		l_set(&it->hypo, m, l_maxf(h, l_get(it->pb, m)));
	}

	// 3. Lines. A column without a definite height wraps only at its
	// max-height: its auto height is its max-content height.
	flex_line *lines = L_SCRATCH(p, flex_line, nitems > 0 ? nitems : 1);
	int nlines = 0;
	float break_at = l_get(avail, m);
	if (m == 1 && l_isnan(inner_size.h)) {
		break_at = AK_LMAX_CONTENT;
		float v = max_s.h;
		if (l_isdef(v)) break_at = l_maxf(0, v - pbs.h);
	}
	if (!wrap || break_at == AK_LMAX_CONTENT) {
		lines[nlines++] = (flex_line){.start = 0, .end = nitems};
	} else {
		int start = 0;
		float used = 0;
		for (int i = 0; i < nitems; i++) {
			float outer = l_get(items[i].hypo, m) + l_sum_axis(items[i].margin, m);
			if (i > start && (break_at == AK_LMIN_CONTENT || used + gap_m + outer > break_at + 0.0001f)) {
				lines[nlines++] = (flex_line){.start = start, .end = i};
				start = i;
				used = outer;
			} else if (i > start) {
				used += gap_m + outer;
			} else {
				used = outer;
			}
		}
		lines[nlines++] = (flex_line){.start = start, .end = nitems};
	}

	// 4. The container's inner main size.
	float inner_m = l_get(inner_size, m);
	if (l_isnan(inner_m)) {
		inner_m = flex_intrinsic_main(items, lines, nlines, m, x, gap_m, wrap, inner_size, avail, p);
		float outer = l_clampn(inner_m + l_get(pbs, m), l_get(min_s, m), l_get(max_s, m));
		inner_m = l_maxf(0, outer - l_get(pbs, m));
	}

	// 5. Resolve flexible lengths (§9.7).
	for (int li = 0; li < nlines; li++) {
		resolve_flexible(items + lines[li].start, lines[li].end - lines[li].start, inner_m, gap_m, m, x, inner_size, avail, p);
	}

	// 6. Hypothetical cross sizes.
	bool is_baseline_row = m == 0;
	for (int i = 0; i < nitems; i++) {
		flex_item *it = &items[i];
		ak_lsize known = {LNAN, LNAN};
		l_set(&known, m, it->target);
		l_set(&known, x, l_get(it->size, x));
		float r = it->node->style.aspect_ratio;
		if (r > 0 && l_isnan(l_get(known, x))) {
			if (m == 0) l_set(&known, x, l_clampn(it->target / r, l_get(it->min, x), l_get(it->max, x)));
			else l_set(&known, x, l_clampn(it->target * r, l_get(it->min, x), l_get(it->max, x)));
		}
		ak_lsize ca = {0, 0};
		l_set(&ca, m, inner_m);
		l_set(&ca, x, l_get(avail, x));
		float cross;
		if (!wrap && l_isdef(l_get(inner_size, x)) && it->align == AK_LALIGN_STRETCH && l_isnan(l_get(known, x)) &&
			!l_bstart(it->auto_m, x) && !l_bend(it->auto_m, x)) {
			// Stretched to the line, which is the container's cross size: no
			// need to measure.
			cross = l_get(inner_size, x) - l_sum_axis(it->margin, x);
		} else if (is_baseline_row && it->align == AK_LALIGN_BASELINE) {
			l_output out = l_compute(it->node, (l_input){.mode = MODE_LAYOUT, .known = known, .parent = inner_size, .avail = ca}, p);
			cross = l_get(out.size, x);
			float bl = out.baseline;
			if (l_isnan(bl)) bl = out.size.h;
			it->baseline = bl + it->margin.top;
		} else {
			cross = l_get(l_measure_size(it->node, (l_input){.known = known, .parent = inner_size, .avail = ca}, p), x);
		}
		cross = l_clampn(cross, l_get(it->min, x), l_get(it->max, x));
		l_set(&it->hypo, x, l_maxf(cross, l_get(it->pb, x)));
	}

	// 7. Line cross sizes.
	float inner_x = l_get(inner_size, x);
	for (int li = 0; li < nlines; li++) {
		flex_line *l = &lines[li];
		if (!wrap && l_isdef(inner_x)) {
			l->cross = inner_x;
			continue;
		}
		float max_cross = 0, max_asc = 0, max_desc = 0;
		for (int i = l->start; i < l->end; i++) {
			flex_item *it = &items[i];
			float outer = l_get(it->hypo, x) + l_sum_axis(it->margin, x);
			if (is_baseline_row && it->align == AK_LALIGN_BASELINE && !it->auto_m.top && !it->auto_m.bottom) {
				max_asc = l_maxf(max_asc, it->baseline);
				max_desc = l_maxf(max_desc, outer - it->baseline);
			} else {
				max_cross = l_maxf(max_cross, outer);
			}
		}
		l->ascent = max_asc;
		l->cross = l_maxf(max_cross, max_asc + max_desc);
		if (!wrap) l->cross = l_clampn(l->cross + l_get(pbs, x), l_get(min_s, x), l_get(max_s, x)) - l_get(pbs, x);
	}
	for (int li = 0; li < nlines; li++) {  // baseline ascent for lines with a definite size
		if (lines[li].ascent == 0) {
			for (int i = lines[li].start; i < lines[li].end; i++) {
				if (items[i].align == AK_LALIGN_BASELINE && is_baseline_row) lines[li].ascent = l_maxf(lines[li].ascent, items[i].baseline);
			}
		}
	}

	// 8. The container's cross size, and align-content: stretch.
	float total_x = 0;
	for (int li = 0; li < nlines; li++) total_x += lines[li].cross;
	total_x += gap_x * (float)(nlines - 1);
	if (l_isnan(inner_x)) inner_x = l_maxf(0, l_clampn(total_x + l_get(pbs, x), l_get(min_s, x), l_get(max_s, x)) - l_get(pbs, x));
	uint8_t ac = st->align_content;
	if (wrap && (ac == AK_LCONTENT_NORMAL || ac == AK_LCONTENT_STRETCH) && inner_x > total_x && nlines > 0) {
		float extra = (inner_x - total_x) / (float)nlines;
		for (int li = 0; li < nlines; li++) lines[li].cross += extra;
	}

	// 9. Used cross sizes.
	for (int li = 0; li < nlines; li++) {
		flex_line *l = &lines[li];
		for (int i = l->start; i < l->end; i++) {
			flex_item *it = &items[i];
			it->cross = l_get(it->hypo, x);
			if (it->align == AK_LALIGN_STRETCH && l_isnan(l_get(it->size, x)) && !l_bstart(it->auto_m, x) && !l_bend(it->auto_m, x)) {
				float c = l_clampn(l->cross - l_sum_axis(it->margin, x), l_get(it->min, x), l_get(it->max, x));
				it->cross = l_maxf(c, l_get(it->pb, x));
			}
		}
	}

	ak_lsize out_size = {0, 0};
	l_set(&out_size, m, inner_m + l_get(pbs, m));
	l_set(&out_size, x, inner_x + l_get(pbs, x));
	out_size.w = l_maxf(out_size.w, pb_only.w);
	out_size.h = l_maxf(out_size.h, pb_only.h);
	if (l_isdef(size.w)) out_size.w = l_maxf(size.w, pb_only.w);
	if (l_isdef(size.h)) out_size.h = l_maxf(size.h, pb_only.h);
	if (in->mode == MODE_SIZE) return (l_output){.size = out_size, .baseline = LNAN};

	// 10. Main-axis alignment.
	for (int li = 0; li < nlines; li++) {
		flex_line *l = &lines[li];
		flex_item *line = items + l->start;
		int nline = l->end - l->start;
		float used = gap_m * (float)(nline - 1);
		int autos = 0;
		for (int i = 0; i < nline; i++) {
			used += outer_main(&line[i], m);
			if (l_bstart(line[i].auto_m, m)) autos++;
			if (l_bend(line[i].auto_m, m)) autos++;
		}
		float free = inner_m - used;
		float lead = 0, between = gap_m;
		float per_auto = 0;
		if (autos > 0 && free > 0) {
			per_auto = free / (float)autos;
		} else {  // auto margins take no negative space
			uint8_t jc = st->justify_content;
			if (reverse) {
				switch (jc) {
				case AK_LCONTENT_START: jc = AK_LCONTENT_FLEX_END; break;
				case AK_LCONTENT_END: jc = AK_LCONTENT_FLEX_START; break;
				case AK_LCONTENT_SPACE_AROUND:
				case AK_LCONTENT_SPACE_EVENLY:
					if (free < 0) jc = AK_LCONTENT_FLEX_END;  // these overflow from start, not main-start
					break;
				}
			}
			float l2, b2;
			l_distribute(jc, free, nline, &l2, &b2);
			lead = l2;
			between = gap_m + b2;
		}
		float pos = lead;
		for (int i = 0; i < nline; i++) {
			flex_item *it = &line[i];
			float ms = l_start(it->margin, m), me = l_end(it->margin, m);
			bool as = l_bstart(it->auto_m, m), ae = l_bend(it->auto_m, m);
			if (mirror_m) {  // positions run from the far side: it comes first
				float t = ms;
				ms = me;
				me = t;
				bool tb = as;
				as = ae;
				ae = tb;
			}
			if (as) pos += per_auto;
			pos += ms;
			it->pos_m = pos;
			pos += it->target + me;
			if (ae) pos += per_auto;
			pos += between;
		}
		if (mirror_m) {
			for (int i = 0; i < nline; i++) line[i].pos_m = inner_m - line[i].pos_m - line[i].target;
		}
	}

	// 11. Lines along the cross axis (align-content) and items in them.
	float free = inner_x - total_x;
	if (wrap && (ac == AK_LCONTENT_NORMAL || ac == AK_LCONTENT_STRETCH) && free > 0) free = 0;  // already given to the lines
	float lead = 0, between = gap_x;
	if (wrap) {
		uint8_t ac2 = ac;
		if (wrap_reverse) {
			// The lines get mirrored below, which is right for flex-start and
			// flex-end, whose cross-start has moved, and not for start and end.
			if (ac2 == AK_LCONTENT_START) ac2 = AK_LCONTENT_END;
			else if (ac2 == AK_LCONTENT_END) ac2 = AK_LCONTENT_START;
			else if (free < 0 && (ac2 == AK_LCONTENT_SPACE_AROUND || ac2 == AK_LCONTENT_SPACE_EVENLY)) ac2 = AK_LCONTENT_END;
		}
		float l2, b2;
		l_distribute(ac2, free, nlines, &l2, &b2);
		lead = l2;
		between = gap_x + b2;
	}
	float pos = lead;
	for (int li = 0; li < nlines; li++) {
		lines[li].pos = pos;
		pos += lines[li].cross + between;
	}
	if (wrap_reverse) {
		for (int li = 0; li < nlines; li++) lines[li].pos = inner_x - lines[li].pos - lines[li].cross;
	}
	float container_baseline = LNAN;
	for (int li = 0; li < nlines; li++) {
		flex_line *l = &lines[li];
		for (int i = l->start; i < l->end; i++) {
			flex_item *it = &items[i];
			float outer = it->cross + l_sum_axis(it->margin, x);
			float ifree = l->cross - outer;
			float off = l_start(it->margin, x);
			bool as = l_bstart(it->auto_m, x), ae = l_bend(it->auto_m, x);
			if (mirror_x) {
				off = l_end(it->margin, x);
				bool t = as;
				as = ae;
				ae = t;
			}
			if (as && ae) {
				off += l_maxf(0, ifree) / 2;
			} else if (as) {
				off += l_maxf(0, ifree);
			} else if (ae) {
			} else {
				uint8_t align = it->align;
				if (wrap_reverse) {
					switch (align) {
					case AK_LALIGN_FLEX_START:
					case AK_LALIGN_STRETCH: align = AK_LALIGN_FLEX_END; break;
					case AK_LALIGN_FLEX_END: align = AK_LALIGN_FLEX_START; break;
					}
				}
				switch (align) {
				case AK_LALIGN_END:
				case AK_LALIGN_FLEX_END: off += ifree; break;
				case AK_LALIGN_CENTER: off += ifree / 2; break;
				case AK_LALIGN_BASELINE:
					if (is_baseline_row) off = l->ascent - it->baseline + l_start(it->margin, x);
					break;
				}
			}
			it->pos_x = l->pos + off;
			if (mirror_x) it->pos_x = inner_x - it->pos_x - it->cross;
		}
	}

	// 12. Final layout of each item. The content box hangs from the inline
	// start's edge, which shows when padding and scrollbars overrun the
	// box.
	float content_x = inset.left;
	if (n->rtl) {
		content_x = out_size.w - inset.right - inner_m;
		if (m == 1) content_x = out_size.w - inset.right - inner_x;
	}
	for (int i = 0; i < nitems; i++) {
		flex_item *it = &items[i];
		ak_lsize known = {0, 0};
		l_set(&known, m, it->target);
		l_set(&known, x, it->cross);
		ak_lsize ca = {0, 0};
		l_set(&ca, m, inner_m);
		l_set(&ca, x, inner_x);
		l_output out = l_compute(it->node, (l_input){.mode = MODE_LAYOUT, .known = known, .parent = inner_size, .avail = ca}, p);
		float px, py;
		if (m == 0) {
			px = content_x + it->pos_m;
			py = inset.top + it->pos_x;
		} else {
			px = content_x + it->pos_x;
			py = inset.top + it->pos_m;
		}
		l_relative_offset(it->node, &px, &py, inner_size);
		l_place(p, it->node, px, py, out.size);
		if (l_isnan(container_baseline) && i < lines[0].end) {
			float bl = out.baseline;
			if (l_isnan(bl)) bl = out.size.h;
			if (m == 1 || it->align == AK_LALIGN_BASELINE || i == 0) container_baseline = py + bl;
		}
	}
	if (nitems == 0) container_baseline = LNAN;

	// 13. Absolutely positioned children.
	layout_absolute(n, out_size, bor, inset, m, reverse, p);
	return (l_output){.size = out_size, .baseline = container_baseline};
}

l_output l_flex(ak_lnode *n, const l_input *in, ak_lpass *p) {
	l_mark mk = l_scratch_mark(p);
	l_output out = flex(n, in, p);
	l_scratch_release(p, mk);
	return out;
}

// l_relative_offset applies a relatively positioned node's insets. With
// both left and right, the parent's inline start wins.
void l_relative_offset(const ak_lnode *ch, float *x, float *y, ak_lsize parent) {
	const ak_lrect *ins = &ch->style.inset;
	if (ch->style.position != AK_LRELATIVE ||
		(ak_llength_is_auto(ins->left) && ak_llength_is_auto(ins->right) && ak_llength_is_auto(ins->top) && ak_llength_is_auto(ins->bottom))) {
		return;
	}
	float l = l_resolve(ins->left, parent.w), r = l_resolve(ins->right, parent.w);
	if (l_isdef(l) && !(ch->parent->rtl && l_isdef(r))) *x += l;
	else if (l_isdef(r)) *x -= r;
	float t = l_resolve(ins->top, parent.h);
	if (l_isdef(t)) {
		*y += t;
	} else {
		float b = l_resolve(ins->bottom, parent.h);
		if (l_isdef(b)) *y -= b;
	}
}

// layout_absolute places a flex container's absolutely positioned
// children: by their insets in the padding box, else where they would be
// as the container's only item.
static void layout_absolute(ak_lnode *n, ak_lsize size, ak_ledges bor, ak_ledges inset, int m, bool reverse, ak_lpass *p) {
	const ak_lstyle *st = &n->style;
	for (uint32_t i = 0; i < n->nchildren; i++) {
		ak_lnode *ch = n->children[i];
		if (ch->style.position != AK_LABSOLUTE || ch->style.display == AK_LDISPLAY_NONE) continue;
		ak_lsize gutter = l_scroll_gutter(n);
		float ax = bor.left;
		if (n->rtl) ax += gutter.w;
		float aw = size.w - bor.left - bor.right - gutter.w;
		float ah = size.h - bor.top - bor.bottom - gutter.h;
		float cw = size.w - inset.left - inset.right, ch2 = size.h - inset.top - inset.bottom;
		float frac[2];
		uint8_t jc = st->justify_content;
		float fm = 0;
		switch (jc) {
		case AK_LCONTENT_CENTER:
		case AK_LCONTENT_SPACE_AROUND:
		case AK_LCONTENT_SPACE_EVENLY: fm = 0.5f; break;
		case AK_LCONTENT_END: fm = 1; break;
		case AK_LCONTENT_FLEX_END: fm = reverse ? 0 : 1; break;
		case AK_LCONTENT_FLEX_START:
		case AK_LCONTENT_NORMAL:
		case AK_LCONTENT_START:
		case AK_LCONTENT_STRETCH:
		case AK_LCONTENT_SPACE_BETWEEN:
			if (reverse && jc != AK_LCONTENT_START) fm = 1;
			break;
		}
		uint8_t align = ch->style.align_self;
		if (align == AK_LALIGN_AUTO) align = st->align_items;
		align = l_resolve_self_align(align, m == 1, n->rtl, l_rtl_under(ch, n->rtl));
		float fx = 0;
		switch (align) {
		case AK_LALIGN_CENTER: fx = 0.5f; break;
		case AK_LALIGN_END: fx = 1; break;
		case AK_LALIGN_FLEX_END:
			if (st->flex_wrap != AK_LWRAP_REVERSE) fx = 1;
			break;
		case AK_LALIGN_FLEX_START:
		case AK_LALIGN_STRETCH:
		case AK_LALIGN_AUTO:
		case AK_LALIGN_BASELINE:
			if (st->flex_wrap == AK_LWRAP_REVERSE) fx = 1;
			break;
		}
		if (n->rtl) {  // the fractions are from the left: the inline axis is mirrored
			if (m == 0) fm = 1 - fm;
			else fx = 1 - fx;
		}
		frac[m] = fm;
		frac[1 - m] = fx;
		l_place_absolute_aligned(p, ch, ax, bor.top, aw, ah, inset.left, inset.top, cw, ch2, frac);
	}
}

// l_resolve_self_align turns self-start and self-end into the
// container's start or end. On the inline axis that depends on whether
// the item goes the container's way; the block axis runs down in both.
uint8_t l_resolve_self_align(uint8_t al, bool inline_axis, bool container_rtl, bool item_rtl) {
	if (al != AK_LALIGN_SELF_START && al != AK_LALIGN_SELF_END) return al;
	if ((al == AK_LALIGN_SELF_START) == (!inline_axis || container_rtl == item_rtl)) return AK_LALIGN_START;
	return AK_LALIGN_END;
}

// l_distribute returns the leading space and the extra space between n
// things for a content distribution of free space. Negative free space
// falls back to start, as browsers do.
void l_distribute(uint8_t c, float free, int n, float *lead, float *between) {
	*lead = 0;
	*between = 0;
	if (n == 0) return;
	if (free < 0) {
		switch (c) {
		case AK_LCONTENT_SPACE_BETWEEN:
		case AK_LCONTENT_SPACE_AROUND:
		case AK_LCONTENT_SPACE_EVENLY:
		case AK_LCONTENT_STRETCH:
		case AK_LCONTENT_NORMAL: c = AK_LCONTENT_START; break;
		}
	}
	switch (c) {
	case AK_LCONTENT_CENTER: *lead = free / 2; return;
	case AK_LCONTENT_END:
	case AK_LCONTENT_FLEX_END: *lead = free; return;
	case AK_LCONTENT_SPACE_BETWEEN:
		if (n > 1) *between = free / (float)(n - 1);
		return;
	case AK_LCONTENT_SPACE_AROUND:
		*lead = free / (float)(2 * n);
		*between = free / (float)n;
		return;
	case AK_LCONTENT_SPACE_EVENLY:
		*lead = free / (float)(n + 1);
		*between = free / (float)(n + 1);
		return;
	}
}

// min_main_size is the item's resolved minimum main size: its min-width
// or min-height, or else its automatic minimum (its min-content size,
// capped by its size and max-size; 0 for scroll containers).
static float min_main_size(flex_item *it, int m, int x, ak_lsize parent, ak_lsize avail, ak_lpass *p) {
	if (l_isdef(it->min_main)) return it->min_main;
	float v = l_get(it->min, m);
	if (l_isdef(v)) {
	} else if (l_is_scroll_container(it->node)) {
		v = 0;
	} else {
		ak_lsize known = {LNAN, LNAN};
		l_set(&known, x, it->known_x);
		ak_lsize ca = {0, 0};
		l_set(&ca, m, AK_LMIN_CONTENT);
		l_set(&ca, x, l_get(avail, x));
		v = l_get(l_measure_size(it->node, (l_input){.sizing = SIZE_CONTENT, .known = known, .parent = parent, .avail = ca}, p), m);
		v = l_maybe_min(v, l_get(it->size, m));
		v = l_maybe_min(v, l_get(it->max, m));
	}
	it->min_main = l_maxf(v, l_get(it->pb, m));
	return it->min_main;
}

static float free_space(const flex_item *line, int nline, float inner_m, float gap, int m) {
	float f = inner_m - gap * (float)(nline - 1);
	for (int i = 0; i < nline; i++) {
		const flex_item *it = &line[i];
		f -= l_sum_axis(it->margin, m);
		if (it->frozen) f -= it->target;
		else f -= it->basis;
	}
	return f;
}

static void resolve_flexible(flex_item *line, int nline, float inner_m, float gap, int m, int x, ak_lsize parent, ak_lsize avail, ak_lpass *p) {
	float used = gap * (float)(nline - 1);
	for (int i = 0; i < nline; i++) used += l_get(line[i].hypo, m) + l_sum_axis(line[i].margin, m);
	bool growing = used < inner_m;
	if (l_absf(used - inner_m) < 0.0001f) {  // it fits exactly: nothing flexes
		for (int i = 0; i < nline; i++) line[i].target = l_get(line[i].hypo, m);
		return;
	}
	for (int i = 0; i < nline; i++) {
		flex_item *it = &line[i];
		it->target = l_get(it->hypo, m);
		it->frozen = (growing && it->grow == 0) || (!growing && it->shrink == 0) ||
			(growing && it->basis > l_get(it->hypo, m)) || (!growing && it->basis < l_get(it->hypo, m));
	}
	float initial = free_space(line, nline, inner_m, gap, m);
	for (int iter = 0; iter < nline + 1; iter++) {
		bool unfrozen = false;
		for (int i = 0; i < nline; i++) {
			if (!line[i].frozen) {
				unfrozen = true;
				break;
			}
		}
		if (!unfrozen) break;
		float remaining = free_space(line, nline, inner_m, gap, m);
		float sum_grow = 0, sum_shrink = 0;
		for (int i = 0; i < nline; i++) {
			if (!line[i].frozen) {
				sum_grow += line[i].grow;
				sum_shrink += line[i].shrink * line[i].inner;
			}
		}
		float sum_factors = sum_grow;
		if (!growing) {
			sum_factors = 0;
			for (int i = 0; i < nline; i++) {
				if (!line[i].frozen) sum_factors += line[i].shrink;
			}
		}
		if (sum_factors < 1) {
			float v = initial * sum_factors;
			if (l_absf(v) < l_absf(remaining)) remaining = v;
		}
		for (int i = 0; i < nline; i++) {
			flex_item *it = &line[i];
			if (it->frozen) continue;
			it->target = it->basis;
			if (remaining != 0) {
				if (growing && sum_grow > 0) it->target = it->basis + remaining * it->grow / sum_grow;
				else if (!growing && sum_shrink > 0) it->target = it->basis + remaining * (it->shrink * it->inner) / sum_shrink;
			}
		}
		float total = 0;
		for (int i = 0; i < nline; i++) {
			flex_item *it = &line[i];
			if (it->frozen) continue;
			float c = l_maybe_min(it->target, l_get(it->max, m));
			// Growing past a content-based basis can't go below the item's
			// min-content size: skip measuring it.
			if (!(growing && it->content && c >= it->basis)) c = l_maxf(c, min_main_size(it, m, x, parent, avail, p));
			else if (l_isdef(l_get(it->min, m))) c = l_maxf(c, l_get(it->min, m));
			it->violation = c - it->target;
			it->target = c;
			total += it->violation;
		}
		for (int i = 0; i < nline; i++) {
			flex_item *it = &line[i];
			if (it->frozen) continue;
			if (total == 0) it->frozen = true;
			else if (total > 0 && it->violation > 0) it->frozen = true;
			else if (total < 0 && it->violation < 0) it->frozen = true;
		}
	}
}

static float item_len(const flex_item *it, int m) {
	return l_maxf(l_maybe_max(it->basis, l_get(it->min, m)) + l_sum_axis(it->margin, m), l_get(it->pb, m));
}

// flex_intrinsic_main is the inner main size of a container whose main
// size is not known (§9.9.1, as browsers implement it: each item adds
// its own flex fraction rather than the line's largest).
static float flex_intrinsic_main(flex_item *items, flex_line *lines, int nlines, int m, int x, float gap, bool wrap,
	ak_lsize parent, ak_lsize avail, ak_lpass *p) {
	float am = l_get(avail, m);
	if (l_definite(am) || (am == AK_LMIN_CONTENT && wrap)) {
		float longest = 0;
		for (int li = 0; li < nlines; li++) {
			const flex_line *l = &lines[li];
			float s = gap * (float)(l->end - l->start - 1);
			for (int i = l->start; i < l->end; i++) s += item_len(&items[i], m);
			longest = l_maxf(longest, s);
		}
		if (l_definite(am) && nlines > 1) longest = l_maxf(longest, am);
		return longest;
	}
	float result = 0;
	for (int li = 0; li < nlines; li++) {
		const flex_line *l = &lines[li];
		float sum = 0, largest = 0;
		for (int i = l->start; i < l->end; i++) {
			flex_item *it = &items[i];
			float mm = l_sum_axis(it->margin, m);
			float smin = l_get(it->min, m), spref = l_get(it->size, m), smax = l_get(it->max, m);
			float clamp_basis = it->basis;
			if (m == 0) clamp_basis = l_maybe_max(it->basis, spref);
			// Under max-content the content contribution is at least the
			// min-content size, so only min-content sizing needs it.
			float min_main = l_get(it->pb, m);
			if (am == AK_LMIN_CONTENT || !it->content) min_main = min_main_size(it, m, x, parent, avail, p);
			float lo = min_main;
			if (it->shrink == 0) lo = l_maxf(l_maybe_max(clamp_basis, smin), min_main);
			else if (l_isdef(smin)) lo = l_maxf(smin, min_main);
			float hi = LINF;
			if (it->grow == 0) hi = l_maybe_min(clamp_basis, smax);
			else if (l_isdef(smax)) hi = smax;
			float contrib;
			if (l_isdef(spref) && (hi <= lo || hi <= spref)) {
				contrib = l_maxf(l_minf(spref, hi), lo) + mm;
			} else if (hi <= lo) {
				contrib = lo + mm;
			} else if (l_is_scroll_container(it->node)) {
				contrib = it->basis + mm;
			} else if (l_isdef(spref)) {
				float v = l_maxf(spref, l_get(it->pb, m));
				if (m == 1) v = l_maxf(v, it->basis);
				contrib = l_clampn(v + mm, smin, smax);
			} else {
				float ax = l_get(avail, x);
				if (l_definite(ax)) {
					ax = l_maxf(0, l_orf(l_get(parent, x), ax) - l_sum_axis(it->margin, x));
					ax = l_clampn(ax, l_get(it->min, x), l_get(it->max, x));
				}
				ak_lsize known = {LNAN, LNAN};
				l_set(&known, x, l_clampn(l_get(it->size, x), l_get(it->min, x), l_get(it->max, x)));
				if (it->align == AK_LALIGN_STRETCH && l_isnan(l_get(known, x)) && l_definite(ax)) l_set(&known, x, ax);
				ak_lsize ca = {0, 0};
				l_set(&ca, m, am);
				l_set(&ca, x, ax);
				float v = l_get(l_measure_size(it->node, (l_input){.sizing = SIZE_CONTENT, .known = known, .parent = parent, .avail = ca}, p), m);
				float r = it->node->style.aspect_ratio;
				if (r > 0 && l_isdef(l_get(known, x))) {
					if (m == 0) v = l_maxf(v, l_get(known, x) * r);
					else v = l_maxf(v, l_get(known, x) / r);
				}
				if (m == 1) v = l_maxf(v, it->basis);
				contrib = l_clampn(v + mm, smin, smax);
			}
			float diff = contrib - mm - it->basis;
			float size = it->basis;
			if (diff > 0) {
				size += diff;
			} else if (diff < 0) {
				float scaled = l_maxf(1, it->shrink) * it->inner;
				if (scaled != 0) size += diff;
			}
			size += mm;
			largest = l_maxf(largest, size);
			sum += size;
		}
		sum += gap * (float)(l->end - l->start - 1);
		if (wrap && m == 0) sum = l_maxf(sum, largest);
		result = l_maxf(result, sum);
	}
	return result;
}
