// element.c: the node tree over native views. Real views
// have a native view; flattened containers only take part in layout;
// fragments (Show, For, Dynamic, a component's several views) have
// neither, their children splicing into their parent.
#include "ui/ui_ext.h"

ak_ui_state ak_ui;

// ---- Views by ID ----

static uint32_t id_slot(ak_id id, uint32_t cap) { return (uint32_t)(id * UINT32_C(2654435761)) & (cap - 1); }

static void ids_insert(ak_view **tab, uint32_t cap, ak_view *e) {
	uint32_t i = id_slot(e->id, cap);
	while (tab[i]) i = (i + 1) & (cap - 1);
	tab[i] = e;
}

void ak_view_put_id(ak_view *e) {
	if (ak_ui.ids_len + 1 > ak_ui.ids_cap / 2) {
		uint32_t nc = ak_ui.ids_cap ? ak_ui.ids_cap * 2 : 256;
		ak_view **nt = ak_alloc((size_t)nc * sizeof *nt);
		for (uint32_t i = 0; i < ak_ui.ids_cap; i++)
			if (ak_ui.ids[i]) ids_insert(nt, nc, ak_ui.ids[i]);
		ak_free(ak_ui.ids);
		ak_ui.ids = nt;
		ak_ui.ids_cap = nc;
	}
	ids_insert(ak_ui.ids, ak_ui.ids_cap, e);
	ak_ui.ids_len++;
}

ak_view *ak_view_of(ak_id id) {
	if (!ak_ui.ids_cap || !id) return NULL;
	for (uint32_t i = id_slot(id, ak_ui.ids_cap);; i = (i + 1) & (ak_ui.ids_cap - 1)) {
		ak_view *e = ak_ui.ids[i];
		if (!e) return NULL;
		if (e->id == id) return e;
	}
}

void ak_view_drop_id(ak_id id) {
	if (!ak_ui.ids_cap || !id) return;
	uint32_t cap = ak_ui.ids_cap, i = id_slot(id, cap);
	for (;; i = (i + 1) & (cap - 1)) {
		if (!ak_ui.ids[i]) return;
		if (ak_ui.ids[i]->id == id) break;
	}
	ak_ui.ids[i] = NULL;
	ak_ui.ids_len--;
	// Shift back what probed past the hole.
	for (uint32_t j = (i + 1) & (cap - 1); ak_ui.ids[j]; j = (j + 1) & (cap - 1)) {
		uint32_t h = id_slot(ak_ui.ids[j]->id, cap);
		bool move = (j > i) ? (h <= i || h > j) : (h <= i && h > j);
		if (move) {
			ak_ui.ids[i] = ak_ui.ids[j];
			ak_ui.ids[j] = NULL;
			i = j;
		}
	}
}

// ---- Making and freeing ----

static void view_drop(ak_opaque *o);

ak_view *ak_view_new(ak_ui_root *r, const ak_el_class *cls, uint16_t el, bool real) {
	ak_view *e = ak_opaque_new(AK_TAG_VIEW, sizeof(ak_view), view_drop);
	e->r = r;
	ak_root_retain(r);
	e->cls = cls;
	e->el = el;
	e->kind = cls->kind;
	for (int i = 0; i < 4; i++) e->sent[i] = AK_NAN;
	e->owner = ak_current_owner();
	if (e->owner) ak_owner_retain(e->owner);
	ak_ui.stats.views++;
	if (cls->flags & AK_ELF_FRAGMENT) {
		e->frag = true;
	} else {
		e->box = ak_lnode_new();
		ak_lnode_set_context(e->box, e);
		ak_style_relayout(e);
	}
	if (real) ak_view_make_view(e);
	if (cls->init) cls->init(e);
	return e;
}

ak_view *ak_frag_new(void) { return ak_view_new(ak_ui_current(), &ak_class_frag, 0, false); }

void ak_view_make_view(ak_view *e) {
	e->real = true;
	e->id = ++ak_ui.next_id;
	ak_view_put_id(e);
	ak_ui.stats.native++;
	ak_batch_create(e->r->batch, e->id, e->kind);
	if (e->dir != AK_LAYOUT_DIRECTION_INHERIT) ak_batch_set_enum(e->r->batch, e->id, AK_PROP_LAYOUT_DIRECTION, e->dir);
	// What it listens for, if it got handlers before it had a view: its
	// bits, in order.
	for (int w = 0; w < 2; w++)
		for (uint64_t bits = e->listens[w]; bits; bits &= bits - 1) {
			ak_event_kind k = (ak_event_kind)(w * 64 + __builtin_ctzll(bits));
			if (k >= 1 && k < AK_EVENT_COUNT) ak_batch_listen(e->r->batch, e->id, k);
		}
	ak_focus_view_made(e);
}

static void view_drop(ak_opaque *o) {
	ak_view *e = (ak_view *)o;
	// An orphan (made, never placed, its last reference gone): its views
	// go with it.
	if (!e->dead) ak_view_destroy(e);
	if (e->id && ak_view_of(e->id) == e) ak_view_drop_id(e->id);
	// Its box first: its kids' boxes are orphaned, not each searched for
	// and moved out of it (n² for a long list's).
	if (e->box) ak_lnode_free(e->box);
	e->box = NULL;
	for (uint32_t i = 0; i < e->kids.len; i++) {
		ak_view *k = e->kids.data[i];
		if (k->parent == e) k->parent = NULL;
		ak_release(k);
	}
	ak_vec_free(&e->kids);
	if (e->cls->drop) e->cls->drop(e);
	ak_ix_free(e);
	ak_release(e->prov);
	ak_style_free(e);
	for (uint32_t i = 0; i < e->handlers.len; i++) ak_release(e->handlers.data[i].c);
	ak_vec_free(&e->handlers);
	ak_release(e->text);
	ak_release(e->text2);
	ak_free(e->grid_mem);
	if (e->owner) ak_owner_release(e->owner);
	ak_ui.stats.views--;
	ak_root_release(e->r);
}

// ak_view_finalize runs the class's finalize once the view's keys are
// set: as it's placed, or at the next flush.
void ak_view_finalize(ak_view *e) {
	if (!e || e->finalized) return;
	e->finalized = true;
	if (e->cls->finalize) e->cls->finalize(e);
}

// ---- Layout children ----

// ak_view_remeasure: what e shows changed (its text, its image), not its
// style: its ancestors are laid out again only if its size does.
void ak_view_remeasure(ak_view *e) {
	if (e->box) ak_lnode_mark_content_dirty(e->box);
}

void ak_view_mark_kids(ak_view *e) {
	if (e->bar) ak_bar_dirty(e);  // a fragment in a Screen's bar: the bar is sent again
	ak_view *l = e;
	while (l && l->frag) l = l->parent;
	if (!l || l->kids_dirty) return;
	l->kids_dirty = true;
	ak_vec_push(&l->r->kids_q, (ak_view *)ak_retain(l));
}

static void collect(ak_view *k, ak_ui_root *r) {
	if (k->host_sized) return;  // a layout root of its own
	if (k->frag) {
		for (uint32_t i = 0; i < k->kids.len; i++) collect(k->kids.data[i], r);
		return;
	}
	ak_vec_push(&r->boxbuf, k->box);
}

// ak_view_relink rebuilds e's box's children from its children,
// expanding fragments.
void ak_view_relink(ak_view *e) {
	ak_ui_root *r = e->r;
	r->boxbuf.len = 0;
	for (uint32_t i = 0; i < e->kids.len; i++) collect(e->kids.data[i], r);
	ak_lnode_set_children(e->box, r->boxbuf.data, r->boxbuf.len);
	r->boxbuf.len = 0;
}

// ---- Native children ----

ak_view *ak_view_native_parent(const ak_view *e) {
	for (ak_view *p = e->parent; p; p = p->parent)
		if (p->real) return p;
	return NULL;
}

void ak_view_each_native(ak_view *e, void (*f)(ak_view *d, void *ctx), void *ctx) {
	if (e->real) {
		f(e, ctx);
		return;
	}
	for (uint32_t i = 0; i < e->kids.len; i++) ak_view_each_native(e->kids.data[i], f, ctx);
}

// native_index counts the views under p (not crossing into views) that
// come before target.
static bool native_index(const ak_view *p, const ak_view *target, uint32_t *n) {
	for (uint32_t i = 0; i < p->kids.len; i++) {
		const ak_view *k = p->kids.data[i];
		if (k == target) return true;
		if (k->real) {
			(*n)++;
			continue;
		}
		if (native_index(k, target, n)) return true;
	}
	return false;
}

typedef struct {
	ak_view *p;
	uint32_t idx;
} attach_ctx;

static void attach_one(ak_view *d, void *ctx) {
	attach_ctx *a = ctx;
	ak_batch_insert(a->p->r->batch, a->p->id, d->id, a->idx);
	a->idx++;
	a->p->native_kids++;
	ak_view_queue_frame(d);
}

// ak_view_attach_native inserts the views of e into its native parent.
void ak_view_attach_native(ak_view *e) {
	ak_view *p = ak_view_native_parent(e);
	if (!p) return;
	attach_ctx a = {p, 0};
	if (e->parent == p && p->kids.len > 0 && p->kids.data[p->kids.len - 1] == e) a.idx = p->native_kids;
	else native_index(p, e, &a.idx);
	ak_view_each_native(e, attach_one, &a);
}

uint32_t ak_view_native_start(ak_view *e) {
	ak_view *p = ak_view_native_parent(e);
	uint32_t n = 0;
	if (p) native_index(p, e, &n);
	return n;
}

uint32_t ak_view_native_count(const ak_view *e) {
	if (e->real) return 1;
	uint32_t n = 0;
	for (uint32_t i = 0; i < e->kids.len; i++) n += ak_view_native_count(e->kids.data[i]);
	return n;
}

void ak_view_attach_native_at(ak_view *e, uint32_t idx) {
	ak_view *p = ak_view_native_parent(e);
	if (!p) return;
	attach_ctx a = {p, idx};
	ak_view_each_native(e, attach_one, &a);
}

static void detach_one(ak_view *d, void *ctx) {
	ak_view *p = ctx;
	ak_batch_remove(p->r->batch, p->id, d->id);
	p->native_kids--;
}

// ak_view_detach_native removes the views of e from its native parent.
void ak_view_detach_native(ak_view *e) {
	ak_view *p = ak_view_native_parent(e);
	if (p) ak_view_each_native(e, detach_one, p);
}

// own_dir is the direction a view has of its own (its layoutDirection
// key), not taken from a parent.
static bool own_dir(const ak_view *e) { return ak_style_get(e, AK_KEY_LAYOUT_DIRECTION) != NULL; }

// take_dir gives k and what's in it the direction dir, as a
// WithDirection gives everything built in it, down to the views with
// one of their own.
static void take_dir(ak_view *k, uint8_t dir) {
	if (own_dir(k) || k->dir == dir) return;
	ak_view_set_dir(k, dir);
	for (uint32_t i = 0; i < k->kids.len; i++) take_dir(k->kids.data[i], dir);
}

void ak_view_inherit_dir(ak_view *e, ak_view *k) {
	for (ak_view *p = e; p; p = p->parent ? p->parent : p->up) {
		if (p->dir != AK_LAYOUT_DIRECTION_INHERIT) {
			take_dir(k, p->dir);
			return;
		}
	}
}

// ak_view_placed gives a view that's just been placed, and the views of
// a fragment placed (a for's, an if's), their layout style again when
// their class adjusts it by where they are (a ProgressRing by its
// parent's align): it was made before they had a parent.
void ak_view_placed(ak_view *k) {
	if (k->cls->adjust && k->box) ak_style_relayout(k);
	if (k->frag)
		for (uint32_t i = 0; i < k->kids.len; i++) ak_view_placed(k->kids.data[i]);
}

void ak_view_append(ak_view *e, ak_view *k) {
	if (!k) return;
	ak_view_finalize(k);
	if (k->parent) ak_panic("ui: a view added twice (it has a parent already)");
	k->parent = e;
	ak_vec_push(&e->kids, (ak_view *)ak_retain(k));
	ak_view_placed(k);
	ak_view_inherit_dir(e, k);
	ak_view_mark_kids(e);
	ak_view_attach_native(k);
}

static void uncount(ak_view *d, void *ctx) {
	AK_UNUSED(d);
	((ak_view *)ctx)->native_kids--;
}

// ak_view_remove takes k out of e; with destroy, its views are destroyed
// (they leave their parents with them).
void ak_view_remove(ak_view *e, ak_view *k, bool destroy) {
	if (destroy) {
		ak_view *p = ak_view_native_parent(k);
		if (p) ak_view_each_native(k, uncount, p);
		ak_view_destroy(k);
	} else {
		ak_view_detach_native(k);
	}
	for (uint32_t i = 0; i < e->kids.len; i++) {
		if (e->kids.data[i] == k) {
			memmove(e->kids.data + i, e->kids.data + i + 1, (e->kids.len - i - 1) * sizeof(ak_view *));
			e->kids.len--;
			break;
		}
	}
	k->parent = NULL;
	ak_view_mark_kids(e);
	ak_release(k);
}

// Removing many of e's kids, as ak_view_remove does each, in one pass
// over e's kids and over their boxes' parents' children (removing a long
// list's rows one by one costs n²): ak_view_take_out each, then
// ak_view_unlink_many them all. In between they're out of the tree (no
// parent) but still in e's kids, which hold them.
void ak_view_take_out(ak_view *k, bool destroy) {
	if (destroy) {
		ak_view *p = ak_view_native_parent(k);
		if (p) ak_view_each_native(k, uncount, p);
		ak_view_destroy(k);
	} else {
		ak_view_detach_native(k);
	}
	k->parent = NULL;  // the mark: the kids e keeps have e
}

void ak_view_unlink_many(ak_view *e, ak_view *const *ks, uint32_t n) {
	if (n == 0) return;
	ak_ui_root *r = e->r;
	uint32_t j = 0;
	for (uint32_t i = 0; i < e->kids.len; i++)
		if (e->kids.data[i]->parent == e) e->kids.data[j++] = e->kids.data[i];
	e->kids.len = j;
	// Their boxes, out of their parent's at once (a fragment's, as
	// collect finds them).
	uint32_t base = r->boxbuf.len;
	for (uint32_t i = 0; i < n; i++) collect(ks[i], r);
	ak_lnode_detach(r->boxbuf.data + base, r->boxbuf.len - base);
	r->boxbuf.len = base;
	ak_view_mark_kids(e);
	for (uint32_t i = 0; i < n; i++) ak_release(ks[i]);
}

// ak_view_destroy destroys the views of e's subtree.
void ak_view_destroy(ak_view *e) {
	if (e->dead) return;
	if (e->real) ak_focus_destroyed(e);
	e->dead = true;
	if (e->real && !e->cell) {
		ak_batch_destroy(e->r->batch, e->id);
		ak_view_drop_id(e->id);
		ak_ui.stats.native--;
	}
	for (uint32_t i = 0; i < e->kids.len; i++) ak_view_destroy(e->kids.data[i]);
	if (e->cls->destroy) e->cls->destroy(e);
	// Its slots' views (a Screen's bar buttons) go with it.
	for (uint32_t i = 0; i < e->style.len; i++) {
		ak_style_ent *s = &e->style.data[i];
		if (s->key >= AK_KEY__STYLE_END && ak_key_kinds[s->key] == AK_K_REF && s->v.p && ((ak_obj *)s->v.p)->tag == AK_TAG_VIEW)
			ak_view_destroy(s->v.p);
	}
}

// ---- Frames ----

void ak_view_queue_frame(ak_view *e) {
	if (e->frame_q) return;
	e->frame_q = true;
	ak_vec_push(&e->r->frame_q, (ak_view *)ak_retain(e));
}

static float snap(float v, float scale) { return (float)(round((double)(v * scale)) / scale); }

// ceil_px rounds a size up to whole pixels, ignoring float noise.
static float ceil_px(float v, float scale) { return (float)(ceil((double)(v * scale) - 1e-3) / scale); }

// ak_view_emit_frame sends e's frame relative to its native parent,
// snapped to the pixel grid, if it changed.
void ak_view_emit_frame(ak_view *e) {
	e->frame_q = false;
	if (e->dead || !e->real || !e->box || e->host_sized) return;
	ak_lframe fr = ak_lnode_frame(e->box);
	float x = fr.x, y = fr.y;
	for (ak_view *p = e->parent; p && !p->real; p = p->parent) {
		if (p->box) {
			ak_lframe pf = ak_lnode_frame(p->box);
			x += pf.x;
			y += pf.y;
		}
	}
	float s = ak_view_scale(e);
	float sx = snap(x, s), sy = snap(y, s);
	float f[4] = {sx, sy, snap(x + fr.w, s) - sx, snap(y + fr.h, s) - sy};
	if (e->measured) {
		// Snapping the edges can make content the host measured a pixel
		// smaller, and a label a pixel narrower wraps: round its size up.
		f[2] = ak_maxf(f[2], ceil_px(fr.w, s));
		f[3] = ak_maxf(f[3], ceil_px(fr.h, s));
	}
	ak_view_send_insets(e);
	if (memcmp(f, e->sent, sizeof f) != 0) {
		memcpy(e->sent, f, sizeof f);
		ak_batch_set_frame(e->r->batch, e->id, f[0], f[1], f[2], f[3]);
		if (e->autofocus) {
			// In its window and laid out: it can take the keyboard. A
			// container gives it to the first view in it that takes it.
			e->autofocus = false;
			if (e->el >= AK_EL_VIEW && e->el <= AK_EL_SAFE_AREA) ak_focus_autofocus(e);
			else ak_batch_command(e->r->batch, e->id, AK_CMD_FOCUS, NULL, 0);
		}
		if (!e->layout_q && ((e->cls->flags & AK_ELF_SIZED) || ak_view_handler(e, AK_EV_VIEW_LAYOUT) || ak_view_handler(e, AK_EV_ROW_LAYOUT) ||
		                     ak_view_handler(e, AK_EV_COLUMN_LAYOUT) || ak_view_handler(e, AK_EV_SAFE_AREA_LAYOUT))) {
			e->layout_q = true;
			ak_vec_push(&e->r->layout_q, (ak_view *)ak_retain(e));
		}
	}
}

// ak_view_send_insets tells the host where a measured view's padding and
// border are inside its frame, when that changed: the host draws its
// content inside them, and measures its content alone. Sent with the
// style, so the host has it before it measures (a button's padding
// replaces its own), and again with the frame (a percentage, resolved).
void ak_view_send_insets(ak_view *e) {
	if (!e->measured || !e->real) return;
	ak_ledges in = ak_lnode_content_insets(e->box);
	if (e->kind == AK_KIND_BUTTON && !ak_style_get(e, AK_KEY_PADDING)) {
		// A button's inset replaces its own padding, a padding of 0
		// included; a border alone leaves that padding, as CSS's does,
		// and its title is centered.
		in = (ak_ledges){0};
	}
	float c[4] = {in.top, in.right, in.bottom, in.left};
	if (memcmp(c, e->insets, sizeof c) != 0) {
		memcpy(e->insets, c, sizeof c);
		ak_batch_set_edges(e->r->batch, e->id, AK_PROP_CONTENT_INSET, c[0], c[1], c[2], c[3]);
	}
}

// ---- Flattening ----

typedef struct {
	ak_view *e;
	uint32_t idx;
} into_ctx;

static void insert_into(ak_view *d, void *ctx) {
	into_ctx *a = ctx;
	ak_batch_insert(a->e->r->batch, a->e->id, d->id, a->idx);
	a->idx++;
	ak_view_queue_frame(d);
}

// ak_view_make_real gives a flattened container a view and moves its
// children's views into it.
void ak_view_make_real(ak_view *e) {
	if (e->real) return;
	ak_view *p = ak_view_native_parent(e);
	if (p) {
		for (uint32_t i = 0; i < e->kids.len; i++) ak_view_detach_native(e->kids.data[i]);
	}
	ak_view_make_view(e);
	into_ctx a = {e, 0};
	for (uint32_t i = 0; i < e->kids.len; i++) ak_view_each_native(e->kids.data[i], insert_into, &a);
	e->native_kids = a.idx;
	if (p) ak_view_attach_native(e);
	ak_view_queue_frame(e);
	// Its look so far, which it couldn't send without a view.
	ak_style_send_all(e);
}

// ---- Measuring ----

static float key_bits_f(float f) { return f; }

static ak_lsize measure_cb(void *ctx, ak_lconstraints c) {
	ak_view *e = ctx;
	ak_ui_root *r = e->r;
	float w = 0, h = 0;
	if (!(e->cls->flags & AK_ELF_CACHE)) {
		r->host.measure(r->host.ctx, e->id, c.width, c.height, c.avail_w, c.avail_h, &w, &h);
		ak_ui.stats.measures++;
		return (ak_lsize){w, h};
	}
	ak_mkey k = {0};
	k.kind = e->kind;
	k.variant = e->variant;
	k.text = e->text;
	k.text2 = e->text2;
	const ak_val *font = ak_style_get(e, AK_KEY_FONT);
	if (font && font->p) {
		const ak_struct *f = font->p;
		k.font_style = (int32_t)f->f[0].i;
		k.font_size = (float)f->f[1].f;
		k.font_weight = (int32_t)f->f[2].i;
		k.family = f->f[3].p;
		k.line_height = (float)f->f[4].f;
	}
	k.lines = ak_style_int(e, AK_KEY_LINES, 0);
	k.font_scale = ak_view_font_scale(e);
	k.scale = ak_view_scale(e);
	k.inset = e->insets[0] != 0 || e->insets[1] != 0 || e->insets[2] != 0 || e->insets[3] != 0;
	float cs[4] = {key_bits_f(c.width), key_bits_f(c.height), key_bits_f(c.avail_w), key_bits_f(c.avail_h)};
	memcpy(k.c, cs, sizeof cs);
	uint32_t hash = ak_mkey_hash(&k);
	if (ak_mcache_get(&r->measures, &k, hash, &w, &h)) {
		ak_ui.stats.measure_hits++;
		return (ak_lsize){w, h};
	}
	r->host.measure(r->host.ctx, e->id, c.width, c.height, c.avail_w, c.avail_h, &w, &h);
	ak_ui.stats.measures++;
	ak_mcache_put(&r->measures, &k, hash, w, h);
	return (ak_lsize){w, h};
}

void ak_view_measure(ak_view *e) {
	e->measured = true;
	ak_lnode_set_measure(e->box, measure_cb, e);
}

static void mark_cell(ak_view *c, void *ctx) {
	AK_UNUSED(ctx);
	ak_view_mark_measured(c);
}

// ak_view_mark_measured marks the measured views (text, controls) of e's
// subtree, list cells included, for measuring again, and its frames for
// snapping again (to another scale's pixels).
void ak_view_mark_measured(ak_view *e) {
	if (e->box && e->real) {
		if (e->kind != AK_KIND_VIEW) ak_lnode_mark_dirty(e->box);
		ak_view_queue_frame(e);
	}
	for (uint32_t i = 0; i < e->kids.len; i++) ak_view_mark_measured(e->kids.data[i]);
	if (e->kind == AK_KIND_LIST && e->el == AK_EL_LIST) ak_list_each_cell(e, mark_cell, NULL);
}

// ---- Direction ----

void ak_view_set_dir(ak_view *e, uint8_t dir) {
	if (e->dir == dir) return;
	e->dir = dir;
	if (e->real) ak_batch_set_enum(e->r->batch, e->id, AK_PROP_LAYOUT_DIRECTION, dir);
	if (e->box) ak_style_relayout(e);
}

// ak_view_ldir is what e's box is told: only a layout root (it has no
// parent to inherit from) and a view with a direction say.
uint8_t ak_view_ldir(const ak_view *e) {
	switch (e->dir) {
	case AK_LAYOUT_DIRECTION_LEFT_TO_RIGHT: return AK_LDIR_LTR;
	case AK_LAYOUT_DIRECTION_RIGHT_TO_LEFT: return AK_LDIR_RTL;
	}
	if ((e->r && e == e->r->el) || e->host_sized || e->cell) return ak_ui.rtl ? AK_LDIR_RTL : AK_LDIR_LTR;
	return AK_LDIR_INHERIT;
}

// ak_view_rtl reports whether e's logical sides are mirrored.
bool ak_view_rtl(const ak_view *e) {
	switch (e->dir) {
	case AK_LAYOUT_DIRECTION_LEFT_TO_RIGHT: return false;
	case AK_LAYOUT_DIRECTION_RIGHT_TO_LEFT: return true;
	}
	return ak_ui.rtl;
}

// ---- Events ----

_Static_assert(AK_EVENT_COUNT <= 128, "a view's listens holds 128 events");

void ak_view_listen(ak_view *e, ak_event_kind ev) {
	if (ak_view_listening(e, ev)) return;
	e->listens[ev / 64] |= UINT64_C(1) << (ev % 64);
	if (e->real) ak_batch_listen(e->r->batch, e->id, ev);
}

ak_closure *ak_view_handler(const ak_view *e, uint16_t ev) {
	for (uint32_t i = 0; i < e->handlers.len; i++)
		if (e->handlers.data[i].ev == ev) return e->handlers.data[i].c;
	return NULL;
}

typedef struct {
	ak_closure *c;
	ak_val *args;
	uint32_t nargs;
} call_ctx;

static void call_in_owner(void *ctx) {
	call_ctx *cc = ctx;
	ak_val v = ak_call(cc->c, cc->args, cc->nargs);
	// Nothing, or an async handler's future: its task runs on its own.
	if (!ak_error_pending()) ak_release(v.p);
}

typedef struct {
	ak_closure *c;
	ak_val *args;
	uint32_t nargs;
	ak_val out;
} under_ctx;

static void call_under(void *ctx) {
	under_ctx *u = ctx;
	u->out = ak_call(u->c, u->args, u->nargs);
}

ak_val ak_ui_call_under(ak_owner *o, ak_closure *c, ak_val *args, uint32_t nargs) {
	under_ctx u = {c, args, nargs, {0}};
	if (o && !ak_owner_disposed(o)) ak_with_owner(o, call_under, &u);
	else call_under(&u);
	return u.out;
}

void ak_view_call(ak_view *e, uint16_t ev, ak_val *args, uint32_t nargs) { ak_view_call_closure(e, ak_view_handler(e, ev), args, nargs); }

void ak_view_call_closure(ak_view *e, ak_closure *c, ak_val *args, uint32_t nargs) {
	if (!c) return;
	ak_retain(c);
	ak_retain(e);
	call_ctx cc = {c, args, nargs};
	ak_owner *o = e->owner && !ak_owner_disposed(e->owner) ? e->owner : NULL;
	if (o) ak_with_owner(o, call_in_owner, &cc);
	else call_in_owner(&cc);
	if (ak_error_pending()) ak_ui_raise_pending(o);
	ak_release(e);
	ak_release(c);
}

uint32_t ak_ui_id(const ak_view *v) { return v && v->real ? v->id : 0; }
