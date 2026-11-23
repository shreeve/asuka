// style.c: the style keys every element takes:
// layout fields map to CSS (with React Native's defaults: a column,
// border-box), the look to the common wire props. Each key is applied on
// its own: a layout key derives the box's style again, a look key sends
// its prop if it changed.
//
// Values are rt values (lang/LOWERING.md): a Length packs its unit in
// bits 32-39 and its float32 value in bits 0-31; Edges, Font, Transform,
// Shadow, Gradient, Transition and Grid are structs in types.asuka's
// field order; Track and GridLine are enums with payloads.
#include "ui/ui_ext.h"
#include <stddef.h>
#include "ui/ui_int.h"

// ---- Lengths ----

ak_llength ak_length(int64_t packed) {
	uint8_t unit = (uint8_t)((uint64_t)packed >> 32);
	uint32_t bits = (uint32_t)(uint64_t)packed;
	float v;
	memcpy(&v, &bits, 4);
	switch (unit) {
	case 1: return ak_lpt(v);
	case 2: return ak_lpct(v);
	}
	return AK_LAUTO;
}

static int64_t pack(float v, uint8_t unit) {
	uint32_t bits;
	memcpy(&bits, &v, 4);
	return (int64_t)((uint64_t)unit << 32 | bits);
}

int64_t ak_length_pt(float v) { return pack(v, 1); }
int64_t ak_length_pct(float v) { return pack(v, 2); }

// ---- The entries ----

const ak_val *ak_style_get(const ak_view *e, uint16_t key) {
	for (uint32_t i = 0; i < e->style.len; i++)
		if (e->style.data[i].key == key) return &e->style.data[i].v;
	return NULL;
}

float ak_style_float(const ak_view *e, uint16_t key, float def) {
	const ak_val *v = ak_style_get(e, key);
	return v ? (float)v->f : def;
}

int64_t ak_style_int(const ak_view *e, uint16_t key, int64_t def) {
	const ak_val *v = ak_style_get(e, key);
	return v ? v->i : def;
}

static ak_struct *style_struct(const ak_view *e, uint16_t key) {
	const ak_val *v = ak_style_get(e, key);
	return v ? v->p : NULL;
}

void ak_style_free(ak_view *e) {
	for (uint32_t i = 0; i < e->style.len; i++) ak_release_val(e->style.data[i].v, (ak_kind)ak_key_kinds[e->style.data[i].key]);
	ak_vec_free(&e->style);
}

bool ak_style_logical(const ak_view *e) {
	const ak_struct *p = style_struct(e, AK_KEY_PADDING), *m = style_struct(e, AK_KEY_MARGIN);
	if (p && (p->f[4].f != 0 || p->f[5].f != 0)) return true;
	if (m && (m->f[4].f != 0 || m->f[5].f != 0)) return true;
	return ak_style_get(e, AK_KEY_START) || ak_style_get(e, AK_KEY_END);
}

// ---- Layout ----

// edges are e's sides (top, right, bottom, left), start and end on the
// sides they are in direction rtl.
static void edges(const ak_struct *s, bool rtl, float out[4]) {
	out[0] = out[1] = out[2] = out[3] = 0;
	if (!s) return;
	for (int i = 0; i < 4; i++) out[i] = (float)s->f[i].f;
	float start = (float)s->f[4].f, end = (float)s->f[5].f;
	if (start != 0) out[rtl ? 1 : 3] = start;
	if (end != 0) out[rtl ? 3 : 1] = end;
}

static uint8_t align_of(int64_t a) {
	// types.asuka's Align: auto, start, center, end, stretch, baseline.
	static const uint8_t m[] = {AK_LALIGN_AUTO, AK_LALIGN_FLEX_START, AK_LALIGN_CENTER, AK_LALIGN_FLEX_END, AK_LALIGN_STRETCH, AK_LALIGN_BASELINE};
	return a >= 0 && a < (int64_t)AK_LEN(m) ? m[a] : AK_LALIGN_AUTO;
}

static uint8_t justify_of(int64_t j) {
	static const uint8_t m[] = {AK_LCONTENT_FLEX_START, AK_LCONTENT_CENTER, AK_LCONTENT_FLEX_END, AK_LCONTENT_SPACE_BETWEEN, AK_LCONTENT_SPACE_AROUND, AK_LCONTENT_SPACE_EVENLY};
	return j >= 0 && j < (int64_t)AK_LEN(m) ? m[j] : AK_LCONTENT_FLEX_START;
}

// GridLine: auto, line(int), span(int), between(int, int).
static ak_lplacement placement(const ak_val *v) {
	ak_lplacement p = {{AK_LLINE_AUTO, 0}, {AK_LLINE_AUTO, 0}};
	const ak_enum *g = v ? v->p : NULL;
	if (!g) return p;
	switch (ak_enum_case(g)) {
	case 1: p.start = ak_lline((int)g->f[0].i); break;
	case 2: p.start = ak_lspan((int)g->f[0].i); break;
	case 3:
		p.start = ak_lline((int)g->f[0].i);
		p.end = ak_lline((int)g->f[1].i);
		break;
	}
	return p;
}

// The grid's tracks, converted: what the layout style points to.
typedef struct {
	ak_lgrid_track *cols, *rows;
	uint32_t ncols, nrows;
	ak_ltrack_size *acols, *arows;
	uint32_t nacols, narows;
	uint8_t flow;
	ak_ltrack_size sizes[];  // the tracks' sizes
} grid_mem;

// Track: auto, points(f), percent(f), fr(f), minContent, maxContent,
// minmax(Track, Track), repeat(int, [Track]), autoFill([Track]),
// autoFit([Track]).
static ak_ltrack_size track_size(const ak_enum *t) {
	if (!t) return ak_lauto_track();
	switch (ak_enum_case(t)) {
	case 1: return ak_lpx((float)t->f[0].f);
	case 2: return ak_lpercent((float)t->f[0].f);
	case 3: return ak_lfr((float)t->f[0].f);
	case 4: return ak_lmin_content();
	case 5: return ak_lmax_content();
	case 6: {
		ak_ltrack_size a = track_size(t->f[0].p), b = track_size(t->f[1].p);
		return ak_lminmax(a.min, b.max);
	}
	}
	return ak_lauto_track();
}

static uint32_t count_sizes(const ak_arr *a) {
	uint32_t n = 0;
	for (uint32_t i = 0; a && i < a->len; i++) {
		const ak_enum *t = ak_arr_vals(a)[i].p;
		uint16_t c = t ? ak_enum_case(t) : 0;
		if (c == 7) n += ((const ak_arr *)t->f[1].p)->len;
		else if (c == 8 || c == 9) n += ((const ak_arr *)t->f[0].p)->len;
		else n++;
	}
	return n;
}

static void fill_size_list(const ak_arr *a, ak_ltrack_size **out) {
	for (uint32_t i = 0; a && i < a->len; i++) *(*out)++ = track_size(ak_arr_vals(a)[i].p);
}

static ak_lgrid_track *tracks(const ak_arr *a, ak_lgrid_track *dst, ak_ltrack_size **sizes, uint32_t *n) {
	*n = 0;
	for (uint32_t i = 0; a && i < a->len; i++) {
		const ak_enum *t = ak_arr_vals(a)[i].p;
		uint16_t c = t ? ak_enum_case(t) : 0;
		ak_ltrack_size *start = *sizes;
		if (c == 7 || c == 8 || c == 9) {
			const ak_arr *list = c == 7 ? t->f[1].p : t->f[0].p;
			fill_size_list(list, sizes);
			// repeat's count, at least one and within int32 (layout makes
			// no more tracks than a grid's lines): not its low 32 bits,
			// and never a negative one, which is an auto-repeat's.
			int64_t times = t->f[0].i;
			int32_t count = c == 7 ? (times < 1 ? 1 : times > INT32_MAX ? INT32_MAX : (int32_t)times) : c == 8 ? AK_LREPEAT_AUTO_FILL : AK_LREPEAT_AUTO_FIT;
			dst[(*n)++] = ak_lrepeat(count, start, (uint32_t)(*sizes - start));
		} else {
			*(*sizes)++ = track_size(t);
			dst[(*n)++] = ak_ltrack(start);
		}
	}
	return dst;
}

// grid_convert converts a Grid (columns, rows, autoColumns, autoRows,
// dense, flowColumn) for layout, in memory of its own.
static grid_mem *grid_convert(const ak_struct *g) {
	const ak_arr *cols = g->f[0].p, *rows = g->f[1].p, *ac = g->f[2].p, *ar = g->f[3].p;
	uint32_t nsizes = count_sizes(cols) + count_sizes(rows) + count_sizes(ac) + count_sizes(ar);
	uint32_t ntracks = (cols ? cols->len : 0) + (rows ? rows->len : 0);
	// The tracks after the sizes, aligned as they need (an odd count of
	// sizes would leave them misaligned).
	size_t al = _Alignof(ak_lgrid_track);
	size_t at = (offsetof(grid_mem, sizes) + nsizes * sizeof(ak_ltrack_size) + al - 1) / al * al;
	grid_mem *m = ak_alloc(at + ntracks * sizeof(ak_lgrid_track));
	ak_ltrack_size *s = m->sizes;
	ak_lgrid_track *t = (ak_lgrid_track *)((char *)m + at);
	m->cols = tracks(cols, t, &s, &m->ncols);
	m->rows = tracks(rows, t + m->ncols, &s, &m->nrows);
	m->acols = s;
	fill_size_list(ac, &s);
	m->nacols = (uint32_t)(s - m->acols);
	m->arows = s;
	fill_size_list(ar, &s);
	m->narows = (uint32_t)(s - m->arows);
	bool dense = g->f[4].i, column = g->f[5].i;
	m->flow = column ? (dense ? AK_LFLOW_COLUMN_DENSE : AK_LFLOW_COLUMN) : (dense ? AK_LFLOW_ROW_DENSE : AK_LFLOW_ROW);
	return m;
}

// The style keys layout reads (AK_KEY_DIRECTION … AK_KEY_CLIP), found in
// one pass over a view's style: ak_style_layout reads some thirty, each a
// search of the style otherwise.
#define LKEYS (AK_KEY_CLIP + 1)
_Static_assert(AK_KEY_DIRECTION > 0 && AK_KEY_GRID_ROW < LKEYS && AK_KEY_BORDER < LKEYS, "layout keys");

static void layout_keys(const ak_view *e, const ak_val **k) {
	memset(k, 0, sizeof(const ak_val *) * LKEYS);
	for (uint32_t i = 0; i < e->style.len; i++) {
		uint16_t key = e->style.data[i].key;
		if (key < LKEYS && !k[key]) k[key] = &e->style.data[i].v;  // the first, as ak_style_get
	}
}

static float lk_float(const ak_val **k, uint16_t key) { return k[key] ? (float)k[key]->f : 0; }
static float l_shrink(float v) { return v > 0 ? v : 0; }  // CSS: a negative flex-shrink is invalid
static int64_t lk_int(const ak_val **k, uint16_t key) { return k[key] ? k[key]->i : 0; }
static bool lk_bool(const ak_val **k, uint16_t key) { return k[key] && k[key]->i; }
static ak_struct *lk_struct(const ak_val **k, uint16_t key) { return k[key] ? k[key]->p : NULL; }

// ak_style_layout maps e's style to CSS, in e's direction.
void ak_style_layout(ak_view *e, ak_lstyle *l) {
	const ak_val *k[LKEYS];
	layout_keys(e, k);
	*l = ak_lstyle_default();
	bool rtl = ak_view_rtl(e);
	l->flex_direction = AK_LCOLUMN;
	if (lk_int(k, AK_KEY_DIRECTION) == 1) l->flex_direction = AK_LROW;
	if (lk_bool(k, AK_KEY_WRAP)) l->flex_wrap = AK_LWRAP;
	l->justify_content = justify_of(lk_int(k, AK_KEY_JUSTIFY));
	l->align_items = align_of(lk_int(k, AK_KEY_ALIGN));
	l->align_self = align_of(lk_int(k, AK_KEY_ALIGN_SELF));
	float gap = lk_float(k, AK_KEY_GAP);
	l->column_gap = l->row_gap = ak_lpt(gap);
	float pad[4], mar[4];
	edges(lk_struct(k, AK_KEY_PADDING), rtl, pad);
	for (int i = 0; i < 4; i++) pad[i] += e->extra_pad[i];
	l->padding = (ak_lrect){ak_lpt(pad[0]), ak_lpt(pad[1]), ak_lpt(pad[2]), ak_lpt(pad[3])};
	edges(lk_struct(k, AK_KEY_MARGIN), rtl, mar);
	l->margin = (ak_lrect){ak_lpt(mar[0]), ak_lpt(mar[1]), ak_lpt(mar[2]), ak_lpt(mar[3])};
	l->border = ak_lrect_all(ak_lpt(lk_float(k, AK_KEY_BORDER)));
	l->width = ak_length(lk_int(k, AK_KEY_WIDTH));
	l->height = ak_length(lk_int(k, AK_KEY_HEIGHT));
	l->min_width = ak_length(lk_int(k, AK_KEY_MIN_WIDTH));
	l->min_height = ak_length(lk_int(k, AK_KEY_MIN_HEIGHT));
	l->max_width = ak_length(lk_int(k, AK_KEY_MAX_WIDTH));
	l->max_height = ak_length(lk_int(k, AK_KEY_MAX_HEIGHT));
	l->aspect_ratio = lk_float(k, AK_KEY_ASPECT_RATIO);
	l->flex_grow = lk_float(k, AK_KEY_GROW);
	// CSS's flex-shrink when it's set (0: the view keeps its size, as
	// shrink: 0 does on the web); 1 when it isn't.
	if (k[AK_KEY_SHRINK]) l->flex_shrink = l_shrink((float)k[AK_KEY_SHRINK]->f);
	l->flex_basis = ak_length(lk_int(k, AK_KEY_BASIS));
	if (lk_bool(k, AK_KEY_ABSOLUTE)) l->position = AK_LABSOLUTE;
	l->inset = (ak_lrect){ak_length(lk_int(k, AK_KEY_TOP)), ak_length(lk_int(k, AK_KEY_RIGHT)),
	                      ak_length(lk_int(k, AK_KEY_BOTTOM)), ak_length(lk_int(k, AK_KEY_LEFT))};
	ak_llength start = ak_length(lk_int(k, AK_KEY_START)), end = ak_length(lk_int(k, AK_KEY_END));
	if (!ak_llength_is_auto(start)) *(rtl ? &l->inset.right : &l->inset.left) = start;
	if (!ak_llength_is_auto(end)) *(rtl ? &l->inset.left : &l->inset.right) = end;
	if (lk_bool(k, AK_KEY_CLIP)) l->overflow_x = l->overflow_y = AK_LOVERFLOW_HIDDEN;
	if (e->grid_mem) {
		const grid_mem *g = e->grid_mem;
		l->display = AK_LDISPLAY_GRID;
		l->grid_template_columns = g->cols;
		l->ngrid_template_columns = g->ncols;
		l->grid_template_rows = g->rows;
		l->ngrid_template_rows = g->nrows;
		l->grid_auto_columns = g->acols;
		l->ngrid_auto_columns = g->nacols;
		l->grid_auto_rows = g->arows;
		l->ngrid_auto_rows = g->narows;
		l->grid_auto_flow = g->flow;
	}
	l->grid_column = placement(k[AK_KEY_GRID_COLUMN]);
	l->grid_row = placement(k[AK_KEY_GRID_ROW]);
	l->direction = ak_view_ldir(e);
	if (e->fill && ak_llength_is_auto(l->height) && l->flex_grow == 0) l->flex_grow = 1;
	if (e->cls->adjust) e->cls->adjust(e, l);
	// A view that grows into its parent's leftover space can be held to
	// its share: its automatic minimum sizes are 0, as React Native's
	// flex: 1 is (in CSS, min-height: 0), so that a growing view around a
	// Scroll scrolls rather than growing to the Scroll's content.
	if (l->flex_grow > 0) {
		if (ak_llength_is_auto(l->min_width)) l->min_width = ak_lpt(0);
		if (ak_llength_is_auto(l->min_height)) l->min_height = ak_lpt(0);
	}
}

void ak_style_relayout(ak_view *e) {
	if (!e->box) return;
	ak_lstyle l;
	ak_style_layout(e, &l);
	ak_lnode_set_style(e->box, &l);
	ak_view_send_insets(e);
}

// ---- The look ----

static bool is_layout(uint16_t key) {
	return (key >= AK_KEY_DIRECTION && key <= AK_KEY_GRID_ROW) || key == AK_KEY_BORDER || key == AK_KEY_CLIP;
}

static bool is_text(uint16_t key) { return key == AK_KEY_FONT || key == AK_KEY_LINES || key == AK_KEY_TEXT_ALIGN; }

// makes_real reports whether a container with key set to v needs a view
// of its own: a look, or an accessibility prop.
static bool makes_real(uint16_t key, ak_val v) {
	switch (key) {
	case AK_KEY_BACKGROUND:
	case AK_KEY_BORDER_COLOR:
	case AK_KEY_MATERIAL:
	case AK_KEY_ROLE:
		return v.i != 0;
	case AK_KEY_RADIUS:
	case AK_KEY_BORDER:
		return v.f != 0;
	case AK_KEY_OPACITY:
		return v.f != 1;
	case AK_KEY_HIDDEN:
	case AK_KEY_CLIP:
	case AK_KEY_A11Y_HIDDEN:
	case AK_KEY_A11Y_CHECKED:
	case AK_KEY_A11Y_EXPANDED:
	case AK_KEY_A11Y_SELECTED:
	case AK_KEY_A11Y_MODAL:
		return v.i != 0;
	case AK_KEY_A11Y_RANGE:
	case AK_KEY_A11Y_CONTROLS:
	case AK_KEY_A11Y_LABELLED_BY:
	case AK_KEY_A11Y_DESCRIBED_BY:
		return v.p != NULL;
	case AK_KEY_A11Y_VALUE:
	case AK_KEY_TRANSFORM:
	case AK_KEY_SHADOW:
	case AK_KEY_GRADIENT:
	case AK_KEY_TRANSITION:
		return v.p != NULL;
		return v.p && ((ak_str *)v.p)->len > 0;
	case AK_KEY_SHARED:
	case AK_KEY_LABEL:
	case AK_KEY_HINT:
	case AK_KEY_TOOLTIP:
		return v.p && ((ak_str *)v.p)->len > 0;
	}
	return false;
}

static void put_f32(uint8_t *p, float f) { memcpy(p, &f, 4); }
static uint64_t ak_f64_bits(double d) {
	uint64_t u;
	memcpy(&u, &d, sizeof u);
	return u;
}
static void put_u64(uint8_t *p, uint64_t v) {
	for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static void send_string(ak_view *e, ak_prop p, const ak_val *v) {
	const ak_str *s = v ? v->p : NULL;
	ak_batch_set_string(e->r->batch, e->id, p, s ? s->data : "", s ? s->len : 0);
}

// send sends key's prop, now v; prev is what was (NULL: nothing, as for
// a view that just got its native view).
static void send(ak_view *e, uint16_t key, const ak_val *v, const ak_val *prev) {
	ak_batch *b = e->r->batch;
	ak_id id = e->id;
	bool text = (e->cls->flags & AK_ELF_TEXT) != 0;
	static const ak_val zero = {0};
	ak_val x = v ? *v : zero;
	switch (key) {
	case AK_KEY_BACKGROUND: ak_batch_set_color(b, id, AK_PROP_BACKGROUND, (ak_color)x.i); break;
	case AK_KEY_RADIUS: ak_batch_set_float(b, id, AK_PROP_CORNER_RADIUS, x.f); break;
	case AK_KEY_BORDER: ak_batch_set_float(b, id, AK_PROP_BORDER_WIDTH, x.f); break;
	case AK_KEY_BORDER_COLOR: ak_batch_set_color(b, id, AK_PROP_BORDER_COLOR, (ak_color)x.i); break;
	case AK_KEY_OPACITY: ak_batch_set_float(b, id, AK_PROP_OPACITY, v ? x.f : 1); break;
	case AK_KEY_HIDDEN: ak_batch_set_bool(b, id, AK_PROP_HIDDEN, x.i != 0); break;
	case AK_KEY_CLIP: ak_batch_set_bool(b, id, AK_PROP_CLIP, x.i != 0); break;
	case AK_KEY_TRANSFORM: {
		// x, y, scale, scaleX, scaleY, rotation: on the wire translate x,
		// y, scale x, y, rotation.
		const ak_struct *t = x.p;
		uint8_t buf[20];
		float tx = 0, ty = 0, sx = 1, sy = 1, rot = 0;
		if (t) {
			tx = (float)t->f[0].f, ty = (float)t->f[1].f;
			sx = (float)(t->f[2].f * t->f[3].f), sy = (float)(t->f[2].f * t->f[4].f);
			rot = (float)t->f[5].f;
		}
		put_f32(buf, tx), put_f32(buf + 4, ty), put_f32(buf + 8, sx), put_f32(buf + 12, sy), put_f32(buf + 16, rot);
		ak_batch_set_bytes(b, id, AK_PROP_TRANSFORM, buf, sizeof buf);
		break;
	}
	case AK_KEY_SHADOW: {
		const ak_struct *s = x.p;
		uint8_t buf[20] = {0};
		if (s) {
			put_u64(buf, (uint64_t)s->f[0].i);
			put_f32(buf + 8, (float)s->f[1].f), put_f32(buf + 12, (float)s->f[2].f), put_f32(buf + 16, (float)s->f[3].f);
		}
		ak_batch_set_bytes(b, id, AK_PROP_SHADOW, buf, sizeof buf);
		break;
	}
	case AK_KEY_GRADIENT: {
		// u32 n, per color u64 color and f32 location, then f32 start x, y,
		// end x, y.
		const ak_struct *g = x.p;
		ak_buf *out = ak_batch_set_bytes_begin(b, id, AK_PROP_GRADIENT);
		const ak_arr *stops = g ? g->f[0].p : NULL;
		ak_buf_u32(out, stops ? stops->len : 0);
		for (uint32_t i = 0; stops && i < stops->len; i++) {
			const ak_struct *st = ak_arr_vals(stops)[i].p;
			ak_buf_u64(out, (uint64_t)st->f[0].i);
			ak_buf_f32(out, (float)st->f[1].f);
		}
		if (g)
			for (int i = 1; i <= 4; i++) ak_buf_f32(out, (float)g->f[i].f);
		ak_batch_set_bytes_end(b);
		break;
	}
	case AK_KEY_MATERIAL: ak_batch_set_enum(b, id, AK_PROP_MATERIAL, (int32_t)x.i); break;
	case AK_KEY_TRANSITION: {
		const ak_struct *t = x.p;
		uint8_t buf[13] = {0};
		if (t) {
			buf[0] = t->f[0].i ? 1 : 0;
			put_f32(buf + 1, (float)t->f[1].f), put_f32(buf + 5, (float)t->f[2].f), put_f32(buf + 9, (float)t->f[3].f);
		}
		ak_batch_set_bytes(b, id, AK_PROP_TRANSITION, buf, sizeof buf);
		break;
	}
	case AK_KEY_SHARED: send_string(e, AK_PROP_MATCH, v); break;
	case AK_KEY_COLOR: ak_batch_set_color(b, id, AK_PROP_COLOR, (ak_color)x.i); break;
	case AK_KEY_FONT: {
		if (!text) break;
		// style, size, weight, family, line height: each sent as it changed.
		const ak_struct *f = x.p, *o = prev ? prev->p : NULL;
		int64_t st = f ? f->f[0].i : 0, ost = o ? o->f[0].i : 0;
		double sz = f ? f->f[1].f : 0, osz = o ? o->f[1].f : 0;
		int64_t wt = f ? f->f[2].i : 0, owt = o ? o->f[2].i : 0;
		const ak_str *fam = f ? f->f[3].p : NULL, *ofam = o ? o->f[3].p : NULL;
		if (st != ost) ak_batch_set_enum(b, id, AK_PROP_TEXT_STYLE, (int32_t)st);
		if (sz != osz) ak_batch_set_float(b, id, AK_PROP_FONT_SIZE, sz);
		if (!ak_obj_eq((const ak_obj *)fam, (const ak_obj *)ofam) && (fam ? fam->len : 0) + (ofam ? ofam->len : 0) > 0)
			ak_batch_set_string(b, id, AK_PROP_FONT_FAMILY, fam ? fam->data : "", fam ? fam->len : 0);
		if (wt != owt) ak_batch_set_enum(b, id, AK_PROP_FONT_WEIGHT, (int32_t)wt);
		double lh = f ? f->f[4].f : 0, olh = o ? o->f[4].f : 0;
		if (lh != olh) ak_batch_set_float(b, id, AK_PROP_LINE_HEIGHT, lh);
		break;
	}
	case AK_KEY_TEXT_ALIGN:
		if (text) ak_batch_set_enum(b, id, AK_PROP_TEXT_ALIGN, (int32_t)x.i);
		break;
	case AK_KEY_LINES:
		if (text) ak_batch_set_int(b, id, AK_PROP_LINE_LIMIT, x.i);
		break;
	case AK_KEY_LABEL: send_string(e, AK_PROP_A11Y_LABEL, v); break;
	case AK_KEY_HINT: send_string(e, AK_PROP_A11Y_HINT, v); break;
	case AK_KEY_ROLE: ak_batch_set_enum(b, id, AK_PROP_A11Y_ROLE, (int32_t)x.i); break;
	case AK_KEY_A11Y_HIDDEN: ak_batch_set_bool(b, id, AK_PROP_A11Y_HIDDEN, x.i != 0); break;
	case AK_KEY_A11Y_CHECKED: ak_batch_set_enum(b, id, AK_PROP_A11Y_CHECKED, (int32_t)x.i); break;
	case AK_KEY_A11Y_EXPANDED: ak_batch_set_enum(b, id, AK_PROP_A11Y_EXPANDED, (int32_t)x.i); break;
	case AK_KEY_A11Y_SELECTED: ak_batch_set_bool(b, id, AK_PROP_A11Y_SELECTED, x.i != 0); break;
	case AK_KEY_A11Y_MODAL: ak_batch_set_bool(b, id, AK_PROP_A11Y_MODAL, x.i != 0); break;
	case AK_KEY_A11Y_VALUE: send_string(e, AK_PROP_A11Y_VALUE, v); break;
	case AK_KEY_A11Y_RANGE: {
		// f64 min, max, now; empty: none.
		const ak_struct *rg = x.p;
		uint8_t buf[24];
		if (rg)
			for (int i = 0; i < 3; i++) put_u64(buf + 8 * i, ak_f64_bits(rg->f[i].f));
		ak_batch_set_bytes(b, id, AK_PROP_A11Y_RANGE, buf, rg ? sizeof buf : 0);
		break;
	}
	case AK_KEY_A11Y_CONTROLS:
	case AK_KEY_A11Y_LABELLED_BY:
	case AK_KEY_A11Y_DESCRIBED_BY: ak_focus_rel_set(e, key); break;
	case AK_KEY_TOOLTIP: send_string(e, AK_PROP_TOOLTIP, v); break;
	case AK_KEY_DISABLED: ak_batch_set_bool(b, id, AK_PROP_DISABLED, x.i != 0); break;
	default:
		if (ak_ix_style_key(key)) ak_ix_style_set(e, key, x, prev);
		break;
	}
}

// is_zero reports whether v is key's zero (what a view shows unset).
static bool is_zero(uint16_t key, ak_val v) {
	switch ((ak_kind)ak_key_kinds[key]) {
	// What a key is when it's unset: opacity's 1, shrink's 1 (0 keeps a
	// view's size, so it's a value, not the default).
	case AK_K_FLOAT: return key == AK_KEY_OPACITY || key == AK_KEY_SHRINK ? v.f == 1 : v.f == 0;
	case AK_K_REF: {
		if (!v.p) return true;
		const ak_obj *o = v.p;
		if (o->tag == AK_TAG_STR) return ((const ak_str *)o)->len == 0;
		if (key == AK_KEY_FONT) {
			const ak_struct *f = v.p;
			const ak_str *fam = f->f[3].p;
			return f->f[0].i == 0 && f->f[1].f == 0 && f->f[2].i == 0 && (!fam || fam->len == 0) && f->f[4].f == 0;
		}
		return false;
	}
	default: return v.i == 0;
	}
}

// ak_style_send_all sends every key set that isn't its zero: the look of
// a view that just got its native view.
void ak_style_send_all(ak_view *e) {
	if (!e->real) return;
	for (uint32_t i = 0; i < e->style.len; i++) {
		ak_style_ent *s = &e->style.data[i];
		if (!is_layout(s->key) || s->key == AK_KEY_BORDER || s->key == AK_KEY_CLIP) {
			if (!is_zero(s->key, s->v)) send(e, s->key, &s->v, NULL);
		}
	}
}

void ak_style_set(ak_view *e, uint16_t key, ak_val v) {
	ak_kind k = (ak_kind)ak_key_kinds[key];
	ak_style_ent *ent = NULL;
	for (uint32_t i = 0; i < e->style.len; i++)
		if (e->style.data[i].key == key) ent = &e->style.data[i];
	// The same, or unset and its zero: nothing changes.
	if (ent ? ak_val_eq(ent->v, v, k) : is_zero(key, v)) return;
	ak_val prev = {0};
	bool had = ent != NULL;
	ak_retain_val(v, k);
	if (ent) {
		prev = ent->v;
		ent->v = v;
	} else {
		ak_vec_push(&e->style, ((ak_style_ent){key, v}));
	}
	if (ak_ix_style_key(key)) {
		ak_ix_style_set(e, key, v, had ? &prev : NULL);
		ak_release_val(prev, k);
		return;
	}
	if (key == AK_KEY_GRID) {
		void *old = e->grid_mem;
		e->grid_mem = v.p ? grid_convert(v.p) : NULL;
		ak_style_relayout(e);
		ak_free(old);
	} else if (key == AK_KEY_LAYOUT_DIRECTION) {
		ak_view_set_dir(e, (uint8_t)v.i);
		ak_style_relayout(e);
	} else if (is_layout(key)) {
		ak_style_relayout(e);
	}
	if (!is_layout(key) || key == AK_KEY_BORDER || key == AK_KEY_CLIP) {
		if (!e->real) {
			if ((e->cls->flags & AK_ELF_FLATTEN) && makes_real(key, v)) ak_view_make_real(e);  // sends the look so far
		} else {
			send(e, key, &v, had ? &prev : NULL);
		}
		if (is_text(key) && key != AK_KEY_TEXT_ALIGN && (e->cls->flags & AK_ELF_TEXT)) ak_view_remeasure(e);
	}
	ak_release_val(prev, k);
}
