// rich.c: styled text: RichText and its
// inline components, Selectable, a selectable Text, and a TextField's
// rich text, its paste types, its formats and its accessory (the bar on
// top of the keyboard).
//
// Spans (types.asuka's Span) cross as ops/rich.go has styled text:
// styles in the order runs first use them, a paragraph per line. A
// span's zero fields are the element's (its font's, its color).
#include "ui/ui_ext.h"

// ---- Spans to the wire ----

static const ak_shape *const no_shapes[12];
static const uint8_t font_kinds[] = {AK_K_INT, AK_K_FLOAT, AK_K_INT, AK_K_REF, AK_K_FLOAT};
static const char *const font_names[] = {"style", "size", "weight", "family", "lineHeight"};
const ak_shape ak_ui_shape_font = {"Font", 5, font_kinds, no_shapes, font_names, 0, NULL, false, NULL};

static const uint8_t span_kinds[] = {AK_K_REF, AK_K_REF, AK_K_BOOL, AK_K_BOOL, AK_K_BOOL, AK_K_BOOL, AK_K_INT, AK_K_INT, AK_K_REF, AK_K_REF};
static const char *const span_names[] = {"text", "font", "italic", "mono", "underline", "strike", "color", "background", "link", "inline"};
static const ak_shape *const span_shapes[] = {NULL, &ak_ui_shape_font, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
const ak_shape ak_ui_shape_span = {"Span", 10, span_kinds, span_shapes, span_names, 0, NULL, false, NULL};

static const uint8_t formats_kinds[] = {AK_K_BOOL, AK_K_BOOL, AK_K_BOOL, AK_K_BOOL, AK_K_BOOL, AK_K_REF};
static const char *const formats_names[] = {"bold", "italic", "underline", "strike", "mono", "link"};
const ak_shape ak_ui_shape_text_formats = {"TextFormats", 6, formats_kinds, no_shapes, formats_names, 0, NULL, false, NULL};

// Span: text, font, italic, mono, underline, strike, color, background,
// link, inline (Inline: key, build).
enum { SP_TEXT, SP_FONT, SP_ITALIC, SP_MONO, SP_UNDERLINE, SP_STRIKE, SP_COLOR, SP_BACKGROUND, SP_LINK, SP_INLINE };

static const char object_char[] = "\xEF\xBF\xBC";  // U+FFFC: an inline component's place in the text

static ak_rich_style style_of(const ak_struct *s) {
	ak_rich_style st = {0};
	const ak_struct *f = s->f[SP_FONT].p;
	if (f) {
		st.text_style = (uint8_t)f->f[0].i;
		st.size = (float)f->f[1].f;
		st.weight = (uint8_t)f->f[2].i;
	}
	st.flags = (uint8_t)((s->f[SP_ITALIC].i ? AK_RICH_ITALIC : 0) | (s->f[SP_MONO].i ? AK_RICH_MONO : 0) |
	                     (s->f[SP_UNDERLINE].i ? AK_RICH_UNDERLINE : 0) | (s->f[SP_STRIKE].i ? AK_RICH_STRIKE : 0));
	st.color = (ak_color)s->f[SP_COLOR].i;
	st.background = (ak_color)s->f[SP_BACKGROUND].i;
	st.link = ak_span_str(s->f[SP_LINK].p);
	return st;
}

static ak_str *inline_key(const ak_struct *s) {
	const ak_struct *in = s->f[SP_INLINE].p;
	return in ? in->f[0].p : NULL;
}

// encode_rich appends spans to b; with inlines, an inline span is the
// object character (each key once), else it's left out (a field's).
static void encode_rich(ak_buf *b, const ak_arr *spans, bool inlines) {
	uint32_t n = spans ? spans->len : 0;
	ak_rich_run *runs = n ? ak_alloc(n * sizeof(ak_rich_run)) : NULL;
	uint32_t m = 0;
	for (uint32_t i = 0; i < n; i++) {
		const ak_struct *s = ak_arr_vals(spans)[i].p;
		if (!s) continue;
		ak_rich_run r = {style_of(s), ak_span_str(s->f[SP_TEXT].p)};
		ak_str *key = inline_key(s);
		if (s->f[SP_INLINE].p) {
			if (!inlines) continue;
			bool seen = false;
			for (uint32_t j = 0; j < i && !seen; j++) {
				const ak_struct *t = ak_arr_vals(spans)[j].p;
				seen = t && t->f[SP_INLINE].p && ak_str_eq(inline_key(t), key);
			}
			if (seen) continue;
			r.style.flags |= AK_RICH_INLINE;
			r.text = (ak_span){object_char, 3};
		}
		runs[m++] = r;
	}
	ak_append_rich(b, runs, m);
	ak_free(runs);
}

// decode_spans reads styled text an editing host sends: a span per run,
// each paragraph after the first starting a new line (on the last span,
// or a span of its own).
static ak_arr *decode_spans(ak_span b) {
	ak_rich_run_vec runs = {0};
	ak_decode_rich((const uint8_t *)b.p, b.len, &runs);
	ak_arr *out = ak_arr_new(AK_K_REF, &ak_ui_shape_span, runs.len);
	bool first_para = true;
	for (uint32_t i = 0; i < runs.len; i++) {
		ak_rich_run *r = &runs.data[i];
		bool nl = r->text.len == 1 && r->text.p[0] == '\n' && !first_para;
		first_para = false;
		if (nl && out->len) {
			ak_struct *last = ak_arr_vals(out)[out->len - 1].p;
			if (!last->f[SP_INLINE].p) {
				ak_str *nl_str = ak_str_c("\n");
				ak_str *t = ak_str_concat(last->f[SP_TEXT].p, nl_str);
				ak_release(nl_str);
				ak_release(last->f[SP_TEXT].p);
				last->f[SP_TEXT].p = t;
				continue;
			}
		}
		ak_struct *s = ak_struct_new(&ak_ui_shape_span);
		s->f[SP_TEXT].p = ak_str_span(r->text);
		ak_struct *f = ak_struct_new(&ak_ui_shape_font);
		if (!nl) {
			f->f[0].i = r->style.text_style, f->f[1].f = r->style.size, f->f[2].i = r->style.weight;
			s->f[SP_ITALIC].i = (r->style.flags & AK_RICH_ITALIC) != 0;
			s->f[SP_MONO].i = (r->style.flags & AK_RICH_MONO) != 0;
			s->f[SP_UNDERLINE].i = (r->style.flags & AK_RICH_UNDERLINE) != 0;
			s->f[SP_STRIKE].i = (r->style.flags & AK_RICH_STRIKE) != 0;
			s->f[SP_COLOR].i = (int64_t)r->style.color;
			s->f[SP_BACKGROUND].i = (int64_t)r->style.background;
		}
		f->f[3].p = ak_str_empty();
		s->f[SP_FONT].p = f;
		s->f[SP_LINK].p = ak_str_span(nl ? (ak_span){"", 0} : r->style.link);
		ak_arr_push(&out, (ak_val){.p = s});
	}
	ak_vec_free(&runs);
	return out;
}

// plain_text is spans' text without its styles (an inline component is
// the object character).
static ak_str *plain_text(const ak_arr *spans) {
	ak_strb b = {0};
	for (uint32_t i = 0; spans && i < spans->len; i++) {
		const ak_struct *s = ak_arr_vals(spans)[i].p;
		if (!s) continue;
		if (s->f[SP_INLINE].p) ak_strb_cstr(&b, object_char, 3);
		else ak_strb_str(&b, s->f[SP_TEXT].p);
	}
	return ak_strb_finish(&b);
}

// ---- RichText ----

typedef struct ak_rich ak_rich;

typedef struct {
	ak_str *key;
	ak_view *holder;  // retained: a layout root of its own, a child of the RichText's view
	ak_owner *owner;  // the component's
	float width;      // the width it was laid out within
} inline_kid;

struct ak_rich {
	ak_view *e;  // not retained: the view holds this
	AK_VEC(inline_kid) kids;  // in the order of their spans
	ak_buf sent;      // the Content prop, as last sent
	bool known;
	ak_buf inl_sent;  // the Inlines prop, as last sent
	bool dirty;       // the components changed: send their sizes again
	bool listed;      // in the root's inlines
};

static const ak_el_class cls_holder;

static void kid_dispose(inline_kid *k) {
	if (k->owner) {
		ak_owner_dispose(k->owner);  // and lets it go, as a row's (control.c)
		k->owner = NULL;
	}
}

static void rich_init(ak_view *e) {
	ak_view_measure(e);
	ak_rich *st = ak_alloc(sizeof *st);
	st->e = e;
	e->impl = st;
}

static void unlist(ak_rich *st) {
	if (!st->listed || !st->e->r->ext) return;
	ak_rext *x = st->e->r->ext;
	for (uint32_t i = 0; i < x->inlines.len; i++) {
		if (x->inlines.data[i] != st) continue;
		x->inlines.data[i] = x->inlines.data[--x->inlines.len];
		break;
	}
	st->listed = false;
}

static void rich_drop(ak_view *e) {
	ak_rich *st = e->impl;
	unlist(st);
	for (uint32_t i = 0; i < st->kids.len; i++) {
		kid_dispose(&st->kids.data[i]);
		ak_release(st->kids.data[i].key);
		ak_release(st->kids.data[i].holder);
	}
	ak_vec_free(&st->kids);
	ak_buf_free(&st->sent);
	ak_buf_free(&st->inl_sent);
	ak_free(st);
}

void ak_rich_inlines_free(ak_rext *x) { ak_vec_free(&x->inlines); }

// The holder of an inline component: a column at its content's size,
// laid out on its own, in the app's direction.
static void holder_adjust(ak_view *e, ak_lstyle *ls) {
	AK_UNUSED(e);
	ls->align_items = AK_LALIGN_FLEX_START;
}

static const ak_el_class cls_holder = {.name = "View", .kind = AK_KIND_VIEW, .flags = AK_ELF_CHILDREN, .adjust = holder_adjust};

typedef struct {
	ak_rich *st;
	inline_kid *k;
	ak_closure *build;
} build_ctx;

static void build_in(void *ctx) {
	build_ctx *b = ctx;
	ak_view *v = ak_ui_build(b->st->e->r, b->build, NULL, 0);
	if (v) {
		ak_view_append(b->k->holder, v);
		ak_release(v);
	}
}

static void build_kid(ak_rich *st, inline_kid *k, ak_closure *build) {
	ak_view *e = st->e;
	k->holder = ak_view_new(e->r, &cls_holder, 0, true);
	k->holder->host_sized = true;  // not in the RichText's box: a layout root
	ak_style_relayout(k->holder);
	k->width = -1;
	k->owner = ak_root_under(e->owner && !ak_owner_disposed(e->owner) ? e->owner : ak_current_owner());
	build_ctx b = {st, k, build};
	ak_with_owner(k->owner, build_in, &b);
}

// sync makes the components the spans' inlines, in their order: new keys
// are built, gone ones disposed.
static void sync(ak_rich *st, const ak_arr *spans) {
	ak_view *e = st->e;
	uint32_t n = spans ? spans->len : 0;
	const ak_struct **ins = n ? ak_alloc(n * sizeof(ak_struct *)) : NULL;
	uint32_t m = 0;
	for (uint32_t i = 0; i < n; i++) {
		const ak_struct *s = ak_arr_vals(spans)[i].p;
		if (!s || !s->f[SP_INLINE].p) continue;
		bool seen = false;
		for (uint32_t j = 0; j < m && !seen; j++) seen = ak_str_eq(ins[j]->f[0].p, inline_key(s));
		if (!seen) ins[m++] = s->f[SP_INLINE].p;
	}
	bool same = m == st->kids.len;
	for (uint32_t i = 0; same && i < m; i++) same = ak_str_eq(st->kids.data[i].key, ins[i]->f[0].p);
	if (same) {
		ak_free(ins);
		return;
	}
	if (!st->listed) {
		ak_vec_push(&ak_rext_of(e->r)->inlines, st);
		st->listed = true;
	}
	// The old ones out: gone, or detached to come back in order.
	AK_VEC(inline_kid) old = {0};
	for (uint32_t i = 0; i < st->kids.len; i++) {
		inline_kid *k = &st->kids.data[i];
		bool wanted = false;
		for (uint32_t j = 0; j < m && !wanted; j++) wanted = ak_str_eq(k->key, ins[j]->f[0].p);
		if (!wanted) {
			kid_dispose(k);
			ak_view_remove(e, k->holder, true);
			ak_release(k->key);
			ak_release(k->holder);
		} else {
			ak_retain(k->holder);
			ak_view_remove(e, k->holder, false);
			ak_vec_push(&old, *k);
		}
	}
	st->kids.len = 0;
	for (uint32_t i = 0; i < m; i++) {
		ak_str *key = ins[i]->f[0].p;
		inline_kid k = {0};
		bool found = false;
		for (uint32_t j = 0; j < old.len && !found; j++) {
			if (!old.data[j].key || !ak_str_eq(old.data[j].key, key)) continue;
			k = old.data[j];
			old.data[j].key = NULL;
			found = true;
		}
		if (!found) {
			k.key = ak_retain(key);
			build_kid(st, &k, ins[i]->f[1].p);
		}
		ak_vec_push(&st->kids, k);
		ak_view_append(e, k.holder);
		if (found) ak_release(k.holder);  // the detached reference; the parent and the kid hold it
	}
	ak_vec_free(&old);
	ak_free(ins);
	st->dirty = true;
}

static void rich_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	if (key != AK_KEY_RICH_TEXT_TEXT) return;
	ak_rich *st = e->impl;
	void *tok = ak_untrack_begin();
	sync(st, v.p);
	ak_untrack_end(tok);
	ak_buf b = {0};
	encode_rich(&b, v.p, true);
	if (!(st->known && st->sent.len == b.len && memcmp(st->sent.data, b.data, b.len) == 0)) {
		st->known = true;
		st->sent.len = 0;
		ak_buf_put(&st->sent, b.data, b.len);
		ak_batch_set_bytes(ak_view_batch(e), e->id, AK_PROP_RICH_TEXT_CONTENT, st->sent.data, st->sent.len);
		ak_view_remeasure(e);
	}
	ak_buf_free(&b);
}

static void rich_event(ak_view *e, const ak_event *ev) {
	if (ev->kind != AK_EVENT_LINK) return;
	ak_str *u = ak_str_span(ev->s);
	ak_val arg = {.p = u};
	ak_view_call(e, AK_EV_RICH_TEXT_LINK, &arg, 1);
	ak_release(u);
}

const ak_el_class ak_class_rich = {.name = "RichText", .kind = AK_KIND_RICH_TEXT, .flags = AK_ELF_TEXT | AK_ELF_MEASURED, .init = rich_init,
                                   .set = rich_set, .event = rich_event, .drop = rich_drop};

const ak_el_class ak_class_selectable = {.name = "Selectable", .kind = AK_KIND_SELECTABLE, .flags = AK_ELF_CHILDREN};

// layout_kids lays out a RichText's components at their own size,
// within width, and tells the host their sizes if they changed (the
// RichText measures again).
static bool layout_kids(ak_rich *st, float width, ak_lpass *p) {
	ak_view *e = st->e;
	ak_buf *b = &ak_rext_of(e->r)->scratch;
	for (uint32_t i = 0; i < st->kids.len; i++) {
		inline_kid *k = &st->kids.data[i];
		if (!ak_lnode_dirty(k->holder->box) && k->width == width) continue;
		k->width = width;
		// Its max-content size; within width when that's wider.
		ak_lsize s = ak_lnode_layout(k->holder->box, (ak_lsize){AK_LMAX_CONTENT, AK_LMAX_CONTENT}, p);
		if (s.w > width) s = ak_lnode_layout(k->holder->box, (ak_lsize){width, AK_LMAX_CONTENT}, p);
		// At the origin: the host places it on its run.
		float f[4] = {0, 0, s.w, s.h};
		if (memcmp(f, k->holder->sent, sizeof f) != 0) {
			memcpy(k->holder->sent, f, sizeof f);
			ak_batch_set_frame(ak_view_batch(e), k->holder->id, 0, 0, s.w, s.h);
		}
		st->dirty = true;
	}
	if (!st->dirty) return false;
	st->dirty = false;
	b->len = 0;
	ak_buf_u32(b, st->kids.len);
	for (uint32_t i = 0; i < st->kids.len; i++) {
		ak_lframe fr = ak_lnode_frame(st->kids.data[i].holder->box);
		ak_buf_u32(b, st->kids.data[i].holder->id);
		ak_buf_f32(b, fr.w);
		ak_buf_f32(b, fr.h);
	}
	if (st->inl_sent.len == b->len && memcmp(st->inl_sent.data, b->data, b->len) == 0) return false;
	st->inl_sent.len = 0;
	ak_buf_put(&st->inl_sent, b->data, b->len);
	ak_batch_set_bytes(ak_view_batch(e), e->id, AK_PROP_RICH_TEXT_INLINES, st->inl_sent.data, st->inl_sent.len);
	ak_view_remeasure(e);
	return true;
}

void ak_ext_layout_inlines(ak_ui_root *r, ak_lpass *p) {
	ak_rext *x = r->ext;
	if (!x || !x->inlines.len) return;
	float width = r->env.width;
	bool told = false;
	for (uint32_t i = 0; i < x->inlines.len; i++) {
		ak_rich *st = x->inlines.data[i];
		if (!st->e->dead) told |= layout_kids(st, width, p);
	}
	// The host measures the texts with their components' sizes: told
	// first (a crossing more, only when they changed).
	if (told) ak_root_apply(r);
}

// ---- Accessories ----

// An accessory is a layout root as wide as the root and as tall as its
// content, which the host shows on top of the keyboard.
static void accessory_adjust(ak_view *e, ak_lstyle *ls) {
	ls->width = ak_lpt(e->r->env.width);
	ls->flex_direction = AK_LCOLUMN;
}

static const ak_el_class cls_accessory = {.name = "Accessory", .kind = AK_KIND_ACCESSORY, .flags = AK_ELF_CHILDREN, .adjust = accessory_adjust};

void ak_ext_layout(ak_ui_root *r, ak_lpass *p) {
	ak_rext *x = r->ext;
	if (!x) return;
	uint32_t live = 0;
	for (uint32_t i = 0; i < x->roots.len; i++) {
		ak_view *a = x->roots.data[i];
		if (a->dead) {
			ak_release(a);
			continue;
		}
		x->roots.data[live++] = a;
		float w = r->env.width;
		if (w <= 0) continue;
		const ak_lstyle *st = ak_lnode_style(a->box);
		if (st->width.v != w) ak_style_relayout(a);
		if (!ak_lnode_dirty(a->box) && a->sent[2] == w) continue;
		ak_lsize s = ak_lnode_layout(a->box, (ak_lsize){w, AK_LMAX_CONTENT}, p);
		if (s.w != a->sent[2] || s.h != a->sent[3]) {
			a->sent[2] = s.w, a->sent[3] = s.h;
			ak_batch_set_float(r->batch, a->id, AK_PROP_ACCESSORY_WIDTH, s.w);
			ak_batch_set_float(r->batch, a->id, AK_PROP_ACCESSORY_HEIGHT, s.h);
		}
	}
	x->roots.len = live;
}

static void accessory_gone(void *ctx) {
	ak_view *a = ctx;
	ak_view_destroy(a);
	ak_release(a);
}

static void set_accessory(ak_view *field, ak_view *content) {
	ak_ui_root *r = field->r;
	ak_view *a = ak_view_new(r, &cls_accessory, 0, true);
	a->host_sized = true;
	a->sent[2] = a->sent[3] = -1;
	ak_style_relayout(a);
	if (content) ak_view_append(a, content);
	ak_vec_push(&ak_rext_of(r)->roots, (ak_view *)ak_retain(a));
	ak_batch_set_int(r->batch, field->id, AK_PROP_TEXT_FIELD_ACCESSORY, a->id);
	// Built with the field, gone with it.
	ak_on_cleanup(accessory_gone, a);
}

// ---- A selectable Text ----

// A selectable Text is a Selectable holding a RichText of its text, made
// as its keys are all set: its view becomes the Selectable's, and its
// text keys go to the RichText.
static const ak_el_class cls_inner = {.name = "RichText", .kind = AK_KIND_RICH_TEXT, .flags = AK_ELF_TEXT | AK_ELF_MEASURED};

static bool text_key(uint16_t key) { return key == AK_KEY_FONT || key == AK_KEY_COLOR || key == AK_KEY_LINES || key == AK_KEY_TEXT_ALIGN; }

ak_view *ak_text_style_target(ak_view *e, uint16_t key) { return e->impl && text_key(key) ? e->impl : e; }

void ak_text_send(ak_view *e) {
	ak_view *inner = e->impl;
	const ak_str *s = e->text;
	if (!inner) {
		ak_batch_set_string(ak_view_batch(e), e->id, AK_PROP_TEXT_TEXT, s ? s->data : "", s ? s->len : 0);
		ak_view_remeasure(e);
		return;
	}
	ak_rich_run run = {.text = ak_span_str(s)};
	ak_buf *b = ak_batch_set_bytes_begin(ak_view_batch(inner), inner->id, AK_PROP_RICH_TEXT_CONTENT);
	ak_append_rich(b, &run, s && s->len ? 1 : 0);
	ak_batch_set_bytes_end(ak_view_batch(inner));
	ak_view_remeasure(inner);
}

void ak_text_finalize(ak_view *e) {
	const ak_val *sel = ak_style_get(e, AK_KEY_TEXT_SELECTABLE);
	if (!sel || !sel->i || e->dead || e->impl) return;
	ak_ui_root *r = e->r;
	// Its view is made again, a Selectable's.
	ak_batch_destroy(r->batch, e->id);
	ak_view_drop_id(e->id);
	ak_ui.stats.native--;
	e->kind = AK_KIND_SELECTABLE;
	e->measured = false;
	memset(e->insets, 0, sizeof e->insets);
	ak_lnode_set_measure(e->box, NULL, NULL);
	ak_view_make_view(e);
	ak_view *inner = ak_view_new(r, &cls_inner, 0, true);
	ak_view_measure(inner);
	e->impl = inner;
	// Its text keys are the RichText's.
	for (uint32_t i = 0; i < e->style.len;) {
		ak_style_ent ent = e->style.data[i];
		if (!text_key(ent.key)) {
			i++;
			continue;
		}
		ak_style_set(inner, ent.key, ent.v);
		ak_release_val(ent.v, (ak_kind)ak_key_kinds[ent.key]);
		memmove(e->style.data + i, e->style.data + i + 1, (e->style.len - i - 1) * sizeof(ak_style_ent));
		e->style.len--;
	}
	ak_style_send_all(e);
	ak_text_send(e);
	ak_view_append(e, inner);
	ak_release(inner);  // e holds it
	ak_view_queue_frame(e);
}

// ---- A rich TextField ----

bool ak_field_rich(const ak_view *e) { return ak_style_get(e, AK_KEY_TEXT_FIELD_RICH) != NULL; }

// What a rich field's state keeps: the rich text it last sent or heard,
// encoded.
typedef struct {
	ak_buf reported;
	bool known;
} rich_field;

typedef struct {
	ak_view *e;
	rich_field *f;
} rich_field_ent;

static AK_VEC(rich_field_ent) rich_fields;

static rich_field *field_rich(ak_view *e, bool make) {
	for (uint32_t i = 0; i < rich_fields.len; i++)
		if (rich_fields.data[i].e == e) return rich_fields.data[i].f;
	if (!make) return NULL;
	rich_field *f = ak_alloc(sizeof *f);
	ak_vec_push(&rich_fields, ((rich_field_ent){e, f}));
	return f;
}

static void field_rich_free(void *ctx) {
	ak_view *e = ctx;
	for (uint32_t i = 0; i < rich_fields.len; i++) {
		if (rich_fields.data[i].e != e) continue;
		ak_buf_free(&rich_fields.data[i].f->reported);
		ak_free(rich_fields.data[i].f);
		rich_fields.data[i] = rich_fields.data[--rich_fields.len];
		break;
	}
	if (!rich_fields.len) ak_vec_free(&rich_fields);
	ak_release(e);
}

static void on_field_gone(void *ctx) { ak_on_cleanup(field_rich_free, ak_retain(ctx)); }

static void set_rich(ak_view *e, const ak_arr *spans, const ak_val *prev) {
	rich_field *f = field_rich(e, false);
	if (!f) {
		f = field_rich(e, true);
		// Gone with the field's owner (not the binding's, which runs again).
		if (e->owner && !ak_owner_disposed(e->owner)) ak_with_owner(e->owner, on_field_gone, e);
		else ak_on_cleanup(field_rich_free, ak_retain(e));
		// A rich field is a multiline one.
		ak_ui_set(e, AK_KEY_TEXT_FIELD_MULTILINE, (ak_val){.i = 1});
		ak_view_listen(e, AK_EVENT_RICH_CHANGED);
	}
	AK_UNUSED(prev);
	ak_buf b = {0};
	encode_rich(&b, spans, false);
	if (f->known && f->reported.len == b.len && memcmp(f->reported.data, b.data, b.len) == 0) {
		ak_buf_free(&b);
		return;
	}
	f->known = true;
	f->reported.len = 0;
	ak_buf_put(&f->reported, b.data, b.len);
	ak_batch_set_bytes(ak_view_batch(e), e->id, AK_PROP_TEXT_FIELD_RICH, b.data, b.len);
	ak_buf_free(&b);
	ak_view_remeasure(e);
	// The field's text is its plain text: a bound text follows it, as it
	// follows typing.
	ak_str *plain = plain_text(spans);
	ak_field_text_reported(e, plain);
	ak_val arg = {.p = plain};
	ak_view_call(e, AK_EV_TEXT_FIELD_CHANGE, &arg, 1);
	ak_release(plain);
}

void ak_field_set_ext(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	switch (key) {
	case AK_KEY_TEXT_FIELD_RICH: set_rich(e, v.p, prev); break;
	case AK_KEY_TEXT_FIELD_PASTE_TYPES: {
		const ak_arr *a = v.p;
		if (!prev && (!a || !a->len)) break;
		ak_buf *b = ak_batch_set_bytes_begin(ak_view_batch(e), e->id, AK_PROP_TEXT_FIELD_PASTE_TYPES);
		ak_encode_strings(b, a);
		ak_batch_set_bytes_end(ak_view_batch(e));
		break;
	}
	case AK_KEY_TEXT_FIELD_ACCESSORY:
		if (!prev && v.p) set_accessory(e, v.p);
		break;
	}
}

void ak_field_finalize_ext(ak_view *e) {
	const ak_val *pt = ak_style_get(e, AK_KEY_TEXT_FIELD_PASTE_TYPES);
	if (pt && pt->p && ((ak_arr *)pt->p)->len && ak_view_handler(e, AK_EV_TEXT_FIELD_PASTE)) ak_view_listen(e, AK_EVENT_PASTED);
}

bool ak_field_event_ext(ak_view *e, const ak_event *ev) {
	switch (ev->kind) {
	case AK_EVENT_RICH_CHANGED: {
		rich_field *f = field_rich(e, false);
		if (!f) return true;
		f->known = true;
		f->reported.len = 0;
		ak_buf_put(&f->reported, ev->s.p, ev->s.len);
		ak_arr *spans = decode_spans(ev->s);
		ak_view_remeasure(e);  // its height is its text's
		ak_val arg = {.p = spans};
		ak_view_call(e, AK_EV_TEXT_FIELD_RICH_CHANGE, &arg, 1);
		ak_release(spans);
		return true;
	}
	case AK_EVENT_FORMATS: {
		ak_reader r = {(const uint8_t *)ev->s.p, ev->s.len, 0, false};
		uint8_t bits = ak_read_u8(&r);
		ak_span link = ak_read_str(&r);
		ak_struct *s = ak_struct_new(&ak_ui_shape_text_formats);
		for (int i = 0; i < 5; i++) s->f[i].i = (bits >> i) & 1;
		s->f[5].p = ak_str_span(link);
		ak_val arg = {.p = s};
		ak_view_call(e, AK_EV_TEXT_FIELD_FORMATS, &arg, 1);
		ak_release(s);
		return true;
	}
	case AK_EVENT_PASTED: {
		ak_item_vec items = {0};
		ak_decode_items((const uint8_t *)ev->s.p, ev->s.len, &items);
		if (items.len) {
			ak_arr *a = ak_items_value(items.data, items.len, false);
			ak_val arg = {.p = a};
			ak_view_call(e, AK_EV_TEXT_FIELD_PASTE, &arg, 1);
			ak_release(a);
		}
		ak_vec_free(&items);
		return true;
	}
	}
	return false;
}

// ---- A field's commands ----

static bool is_field(const ak_view *v) { return v && v->kind == AK_KIND_TEXT_FIELD && !v->dead; }

// ak_ui_insert_text puts text into a TextField as if it were typed: in
// place of the selection, or at the cursor, undoably.
void ak_ui_insert_text(ak_view *v, ak_str *text) {
	if (is_field(v)) ak_batch_command(ak_view_batch(v), v->id, AK_CMD_INSERT_TEXT, text ? text->data : "", text ? text->len : 0);
}

// ak_ui_format turns a TextFormat (its case) on or off for a rich field's
// selection (with none, for what's typed next); how 2 toggles it.
void ak_ui_format(ak_view *v, int64_t format, int64_t how) {
	if (!is_field(v) || format < 0 || format > 4) return;
	uint8_t args[2] = {(uint8_t)(1u << format), (uint8_t)how};
	ak_batch_command(ak_view_batch(v), v->id, AK_CMD_FORMAT, args, 2);
}

// ak_ui_set_link links a rich field's selection to url ("" removes it).
void ak_ui_set_link(ak_view *v, ak_str *url) {
	if (is_field(v)) ak_batch_command(ak_view_batch(v), v->id, AK_CMD_SET_LINK, url ? url->data : "", url ? url->len : 0);
}
