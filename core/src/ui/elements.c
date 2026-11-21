// elements.c: the built-in elements: a class each, and the protocol that sets their keys, binds them and
// handles their events.
//
// A control whose value the user changes (a text field's text, a
// toggle's on, a slider's value) keeps what the host last reported and
// sends the value only when it differs from that: typing never round
// trips, and IME composition is never disturbed.
#include "ui/ui_ext.h"

#include <stdio.h>

// ---- Helpers ----

bool ak_ui_eq(ak_val a, ak_val b, ak_kind k) { return ak_val_eq(a, b, k); }

static ak_str *str_of(ak_val v) { return v.p; }

static void send_str(ak_view *e, ak_prop p, ak_val v) {
	const ak_str *s = v.p;
	ak_batch_set_string(e->r->batch, e->id, p, s ? s->data : "", s ? s->len : 0);
}

static void set_text(ak_str **dst, ak_val v) {
	ak_str *s = ak_retain(str_of(v));
	ak_release(*dst);
	*dst = s;
}

// ak_encode_strings appends a list of strings: u32 count, then each
// u32-length.
void ak_encode_strings(ak_buf *b, const ak_arr *a) {
	ak_buf_u32(b, a ? a->len : 0);
	for (uint32_t i = 0; a && i < a->len; i++) {
		const ak_str *s = ak_arr_vals(a)[i].p;
		ak_buf_str(b, s ? s->data : "", s ? s->len : 0);
	}
}

static void send_strings(ak_view *e, ak_prop p, const ak_arr *a) {
	ak_buf *b = ak_batch_set_bytes_begin(e->r->batch, e->id, p);
	ak_encode_strings(b, a);
	ak_batch_set_bytes_end(e->r->batch);
}

static bool key_set(const ak_view *e, uint16_t key) { return ak_style_get(e, key) != NULL; }

static double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static void call0(ak_view *e, uint16_t ev) { ak_view_call(e, ev, NULL, 0); }

// ---- The ops event each AK_EV_ is reported by ----

ak_event_kind ak_ev_ops(uint16_t ev) {
	switch (ev) {
	case AK_EV_VIEW_PRESS:
	case AK_EV_ROW_PRESS:
	case AK_EV_COLUMN_PRESS:
	case AK_EV_SAFE_AREA_PRESS:
	case AK_EV_BUTTON_PRESS:
		return AK_EVENT_PRESS;
	case AK_EV_VIEW_LONG_PRESS:
	case AK_EV_ROW_LONG_PRESS:
	case AK_EV_COLUMN_LONG_PRESS:
	case AK_EV_SAFE_AREA_LONG_PRESS:
		return AK_EVENT_LONG_PRESS;
	case AK_EV_TEXT_FIELD_CHANGE: return AK_EVENT_TEXT_CHANGED;
	case AK_EV_TEXT_FIELD_SUBMIT: return AK_EVENT_SUBMIT;
	case AK_EV_TEXT_FIELD_FOCUS: return AK_EVENT_FOCUS;
	case AK_EV_TEXT_FIELD_BLUR: return AK_EVENT_BLUR;
	case AK_EV_TEXT_FIELD_SELECT: return AK_EVENT_SELECTION_CHANGED;
	case AK_EV_TOGGLE_CHANGE: return AK_EVENT_TOGGLED;
	case AK_EV_SLIDER_CHANGE:
	case AK_EV_STEPPER_CHANGE:
		return AK_EVENT_VALUE_CHANGED;
	case AK_EV_SEGMENTED_CHANGE:
	case AK_EV_PICKER_CHANGE:
		return AK_EVENT_SELECTED;
	case AK_EV_IMAGE_LOADED: return AK_EVENT_LOADED;
	case AK_EV_IMAGE_ERROR: return AK_EVENT_FAILED;
	case AK_EV_SCROLL_SCROLL:
	case AK_EV_LIST_SCROLL:
		return AK_EVENT_SCROLLED;
	case AK_EV_SCROLL_REFRESH:
	case AK_EV_LIST_REFRESH:
		return AK_EVENT_REFRESH;
	case AK_EV_SCROLL_PAGE_CHANGE: return AK_EVENT_PAGED;
	}
	ak_event_kind k = ak_nav_ev_ops(ev);  // the navigation's,
	return k ? k : ak_ix_ev_ops(ev);      // the extensions' (ui_ext.h), else the runtime's own: layout, a list's end
}

// ---- Containers: View, Row, Column, SafeArea ----

const ak_el_class ak_class_frag = {.name = "Fragment", .kind = AK_KIND_VIEW, .flags = AK_ELF_FRAGMENT | AK_ELF_CHILDREN};
const ak_el_class ak_class_view = {.name = "View", .kind = AK_KIND_VIEW, .flags = AK_ELF_CHILDREN};
// A list cell is as wide as the row it's laid out at.
static void cell_adjust(ak_view *e, ak_lstyle *ls) {
	AK_UNUSED(e);
	ls->width = ak_lpct(100);
}

const ak_el_class ak_class_cell = {.name = "Cell", .kind = AK_KIND_VIEW, .flags = AK_ELF_CHILDREN, .adjust = cell_adjust};

static uint16_t container_ev(const ak_view *e, int which) {
	static const uint16_t evs[][3] = {
		{0, 0, 0},
		{AK_EV_VIEW_PRESS, AK_EV_VIEW_LONG_PRESS, AK_EV_VIEW_LAYOUT},
		{AK_EV_ROW_PRESS, AK_EV_ROW_LONG_PRESS, AK_EV_ROW_LAYOUT},
		{AK_EV_COLUMN_PRESS, AK_EV_COLUMN_LONG_PRESS, AK_EV_COLUMN_LAYOUT},
		{AK_EV_SAFE_AREA_PRESS, AK_EV_SAFE_AREA_LONG_PRESS, AK_EV_SAFE_AREA_LAYOUT},
	};
	return e->el <= AK_EL_SAFE_AREA ? evs[e->el][which] : 0;
}

static void container_event(ak_view *e, const ak_event *ev) {
	if (ev->kind == AK_EVENT_PRESS) {
		call0(e, container_ev(e, 0));
		const ak_val *href = ak_style_get(e, ak_ext_key(e, AK_KEY_VIEW_HREF));
		if (href && href->p && ((ak_str *)href->p)->len && !e->dead) ak_router_href(e, href->p);
	} else if (ev->kind == AK_EVENT_LONG_PRESS) {
		call0(e, container_ev(e, 1));
	}
}

static void row_init(ak_view *e) { ak_style_set(e, AK_KEY_DIRECTION, (ak_val){.i = 1}); }

// A SafeArea is padded by the safe area's insets (the notch, the home
// indicator, the system bars) and the keyboard, over its own padding.
static void safe_run(void *ctx) {
	ak_view *e = ctx;
	const ak_environment *env = ak_view_env(e);  // its screen's, sheet's, window's, else the root's
	float pad[4] = {env->safe_top, env->safe_right, ak_maxf(env->safe_bottom, env->keyboard), env->safe_left};
	if (memcmp(pad, e->extra_pad, sizeof pad) == 0) return;
	memcpy(e->extra_pad, pad, sizeof pad);
	ak_style_relayout(e);
}

static void safe_drop(void *ctx) { ak_release(ctx); }

static const ak_comp_info safe_info = {.drop = safe_drop, .label = "SafeArea"};

static void safe_init(ak_view *e) {
	e->safe = true;
	e->prov = ak_prov_current();  // the environment it's built in
	if (e->prov) ak_retain(e->prov);
	ak_effect_new(safe_run, ak_retain(e), AK_EFFECT_RENDER, &safe_info);
}

static const ak_el_class cls_view = {.name = "View", .kind = AK_KIND_VIEW, .flags = AK_ELF_FLATTEN | AK_ELF_CHILDREN, .set = ak_ix_container_set, .event = container_event};
static const ak_el_class cls_row = {.name = "Row", .kind = AK_KIND_VIEW, .flags = AK_ELF_FLATTEN | AK_ELF_CHILDREN, .init = row_init, .set = ak_ix_container_set, .event = container_event};
static const ak_el_class cls_safe = {.name = "SafeArea", .kind = AK_KIND_VIEW, .flags = AK_ELF_FLATTEN | AK_ELF_CHILDREN, .init = safe_init, .set = ak_ix_container_set, .event = container_event};

// ---- Text, Symbol ----

static void measured_init(ak_view *e) { ak_view_measure(e); }

static void text_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	if (key == AK_KEY_TEXT_TEXT) {
		set_text(&e->text, v);
		ak_text_send(e);  // a selectable Text's goes to its RichText (rich.c)
	}
	// selectable: at finalize (rich.c).
}

static void symbol_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	if (key == AK_KEY_SYMBOL_NAME) {
		set_text(&e->text, v);
		send_str(e, AK_PROP_SYMBOL_NAME, v);
		ak_view_remeasure(e);
	}
}

static const ak_el_class cls_text = {.name = "Text", .kind = AK_KIND_TEXT, .flags = AK_ELF_TEXT | AK_ELF_MEASURED | AK_ELF_CACHE, .init = measured_init, .set = text_set,
                                     .finalize = ak_text_finalize, .style_target = ak_text_style_target};
static const ak_el_class cls_symbol = {.name = "Symbol", .kind = AK_KIND_SYMBOL, .flags = AK_ELF_TEXT | AK_ELF_MEASURED | AK_ELF_CACHE, .init = measured_init, .set = symbol_set};

// ---- Button ----

// A menu's actions (types.asuka's Action: button(title, icon, role,
// disabled, checked, run), submenu(title, icon, items), section(title,
// items)), encoded as ops/actions.go has them. Items are numbered depth
// first; an Action event carries the chosen one's number.
#define ACTION_DEPTH 16

static void append_actions(ak_buf *b, const ak_arr *as, int depth) {
	ak_buf_u32(b, as ? as->len : 0);
	for (uint32_t i = 0; as && i < as->len; i++) {
		const ak_enum *a = ak_arr_vals(as)[i].p;
		uint16_t c = a ? ak_enum_case(a) : 0;
		const ak_str *title = a ? a->f[0].p : NULL, *icon = NULL;
		uint8_t role = 0, flags = 0;
		const ak_arr *items = NULL;
		if (c == 0 && a) {
			icon = a->f[1].p;
			role = (uint8_t)a->f[2].i;
			if (a->f[3].i) flags |= AK_MENU_DISABLED;
			if (a->f[4].i) flags |= AK_MENU_CHECKED;
		} else if (c == 1) {
			icon = a->f[1].p;
			items = a->f[2].p;
		} else if (c == 2) {
			items = a->f[1].p;
			flags |= AK_MENU_INLINE;
		}
		bool sub = (c == 1 || c == 2) && depth < ACTION_DEPTH;
		if (sub) flags |= AK_MENU_SUBMENU;
		else flags &= (uint8_t)~AK_MENU_INLINE;
		ak_buf_u8(b, role);
		ak_buf_str(b, title ? title->data : "", title ? title->len : 0);
		ak_buf_str(b, icon ? icon->data : "", icon ? icon->len : 0);
		ak_buf_u8(b, flags);
		if (sub) append_actions(b, items, depth + 1);
	}
}

// find_action finds the item numbered *n, depth first.
static const ak_enum *find_action(const ak_arr *as, int64_t *n, int depth) {
	for (uint32_t i = 0; as && i < as->len; i++) {
		const ak_enum *a = ak_arr_vals(as)[i].p;
		if ((*n)-- == 0) return a;
		uint16_t c = a ? ak_enum_case(a) : 0;
		if ((c == 1 || c == 2) && depth < ACTION_DEPTH) {
			const ak_enum *f = find_action(c == 1 ? a->f[2].p : a->f[1].p, n, depth + 1);
			if (f) return f;
		}
	}
	return NULL;
}

void ak_encode_actions(ak_buf *b, const ak_arr *as) { append_actions(b, as, 0); }

const ak_enum *ak_find_action(const ak_arr *as, int64_t n) { return n < 0 ? NULL : find_action(as, &n, 0); }

static uint32_t count_actions(const ak_arr *as, int depth) {
	uint32_t n = 0;
	for (uint32_t i = 0; as && i < as->len; i++) {
		const ak_enum *a = ak_arr_vals(as)[i].p;
		uint16_t c = a ? ak_enum_case(a) : 0;
		n++;
		if ((c == 1 || c == 2) && depth < ACTION_DEPTH) n += count_actions(c == 1 ? a->f[2].p : a->f[1].p, depth + 1);
	}
	return n;
}

uint32_t ak_action_count(const ak_arr *as) { return count_actions(as, 0); }

void ak_run_action(ak_view *e, const ak_enum *a) {
	if (!a || ak_enum_case(a) != 0 || a->f[3].i || !a->f[5].p) return;  // a submenu, disabled, or nothing to run
	ak_closure *run = a->f[5].p;
	ak_retain(run);
	ak_val r = ak_ui_call_under(e ? e->owner : NULL, run, NULL, 0);  // the screen's, where it was made
	if (!ak_error_pending()) ak_release(r.p);
	else ak_ui_raise_pending(e ? e->owner : NULL);
	ak_release(run);
}

static void button_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	ak_batch *b = e->r->batch;
	switch (key) {
	case AK_KEY_BUTTON_TITLE:
		set_text(&e->text, v);  // the measure cache's key, with the icon
		send_str(e, AK_PROP_BUTTON_TITLE, v);
		ak_view_remeasure(e);
		break;
	case AK_KEY_BUTTON_VARIANT:
		if (!prev && v.i == 0) break;  // plain: the platform's default
		e->variant = (int32_t)v.i;
		ak_batch_set_enum(b, e->id, AK_PROP_BUTTON_VARIANT, (int32_t)v.i);
		ak_view_remeasure(e);
		break;
	case AK_KEY_BUTTON_ICON:
		set_text(&e->text2, v);
		send_str(e, AK_PROP_BUTTON_ICON, v);
		ak_view_remeasure(e);
		break;
	case AK_KEY_BUTTON_MENU: {
		ak_buf *out = ak_batch_set_bytes_begin(b, e->id, AK_PROP_MENU);
		append_actions(out, v.p, 0);
		ak_batch_set_bytes_end(b);
		ak_view_listen(e, AK_EVENT_ACTION);
		break;
	}
	case AK_KEY_BUTTON_HREF:
		ak_view_listen(e, AK_EVENT_PRESS);  // the router goes there (router.c)
		break;
	}
}

static void button_event(ak_view *e, const ak_event *ev) {
	if (ev->kind == AK_EVENT_PRESS) {
		call0(e, AK_EV_BUTTON_PRESS);
		const ak_val *href = ak_style_get(e, AK_KEY_BUTTON_HREF);
		if (href && href->p && ((ak_str *)href->p)->len && !e->dead) ak_router_href(e, href->p);
	} else if (ev->kind == AK_EVENT_ACTION) {
		const ak_val *menu = ak_style_get(e, AK_KEY_BUTTON_MENU);
		ak_run_action(e, menu ? ak_find_action(menu->p, ev->i) : NULL);
	}
}

static const ak_el_class cls_button = {.name = "Button", .kind = AK_KIND_BUTTON, .flags = AK_ELF_TEXT | AK_ELF_MEASURED | AK_ELF_CACHE, .init = measured_init, .set = button_set, .event = button_event};

// ---- TextField ----

typedef struct {
	ak_str *reported;  // the text the field shows, as last sent or reported
	bool known;
	uint32_t sel[2];   // the selection, as last sent or reported (UTF-16)
	bool sel_known;
	bool multiline;
	bool focused;  // as last reported, or asked for
} field_state;

static int rune_len(uint8_t c) { return c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4; }

// utf16_offset is the UTF-16 offset of byte offset b in s (as UIKit and
// Android count), b clamped to s and moved back to a character's start.
static uint32_t utf16_offset(const ak_str *s, int64_t b) {
	uint32_t n = 0;
	for (uint32_t i = 0; s && i < s->len;) {
		if ((int64_t)i >= b) break;
		int l = rune_len((uint8_t)s->data[i]);
		n += l == 4 ? 2 : 1;
		i += (uint32_t)l;
	}
	return n;
}

// byte_offset is the byte offset in s of UTF-16 offset u (clamped).
static int64_t byte_offset(const ak_str *s, uint32_t u) {
	uint32_t n = 0, i = 0;
	while (s && i < s->len) {
		if (n >= u) return i;
		int l = rune_len((uint8_t)s->data[i]);
		n += l == 4 ? 2 : 1;
		i += (uint32_t)l;
	}
	return s ? s->len : 0;
}

static void field_init(ak_view *e) {
	ak_view_measure(e);
	e->impl = ak_alloc(sizeof(field_state));
}

static void field_drop(ak_view *e) {
	field_state *f = e->impl;
	if (!f) return;
	ak_release(f->reported);
	ak_free(f);
}

static void field_send_selection(ak_view *e, const ak_struct *r) {
	field_state *f = e->impl;
	int64_t len = f->reported ? f->reported->len : 0;
	int64_t start = r ? r->f[0].i : 0, end = r ? r->f[1].i : 0;
	start = start < 0 ? 0 : start > len ? len : start;
	end = end < 0 ? 0 : end > len ? len : end;
	if (end < start) {
		int64_t t = start;
		start = end, end = t;
	}
	uint32_t u[2] = {utf16_offset(f->reported, start), utf16_offset(f->reported, end)};
	if (f->sel_known && u[0] == f->sel[0] && u[1] == f->sel[1]) return;
	f->sel[0] = u[0], f->sel[1] = u[1], f->sel_known = true;
	uint8_t b[8];
	for (int i = 0; i < 4; i++) b[i] = (uint8_t)(u[0] >> (8 * i)), b[4 + i] = (uint8_t)(u[1] >> (8 * i));
	ak_batch_set_bytes(e->r->batch, e->id, AK_PROP_TEXT_FIELD_SELECTION, b, 8);
}

static void field_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	field_state *f = e->impl;
	ak_batch *b = e->r->batch;
	ak_id id = e->id;
	switch (key) {
	case AK_KEY_TEXT_FIELD_TEXT: {
		ak_str *s = v.p;
		if (ak_field_rich(e)) break;  // a rich field's text is its rich's, which it follows
		if (f->known && ak_obj_eq((const ak_obj *)s, (const ak_obj *)f->reported)) break;
		set_text(&f->reported, v);
		f->known = true;
		send_str(e, AK_PROP_TEXT_FIELD_VALUE, v);
		if (f->multiline) ak_view_remeasure(e);
		break;
	}
	case AK_KEY_TEXT_FIELD_PLACEHOLDER: send_str(e, AK_PROP_TEXT_FIELD_PLACEHOLDER, v); break;
	case AK_KEY_TEXT_FIELD_KEYBOARD:
		if (prev || v.i) ak_batch_set_enum(b, id, AK_PROP_TEXT_FIELD_KEYBOARD, (int32_t)v.i);
		break;
	case AK_KEY_TEXT_FIELD_RETURN_KEY:
		if (prev || v.i) ak_batch_set_enum(b, id, AK_PROP_TEXT_FIELD_RETURN_KEY, (int32_t)v.i);
		break;
	case AK_KEY_TEXT_FIELD_SECURE:
		if (prev || v.i) ak_batch_set_bool(b, id, AK_PROP_TEXT_FIELD_SECURE, v.i != 0);
		break;
	case AK_KEY_TEXT_FIELD_AUTOCAPITALIZE:
		if (prev || v.i) ak_batch_set_enum(b, id, AK_PROP_TEXT_FIELD_AUTOCAPITALIZE, (int32_t)v.i);
		break;
	case AK_KEY_TEXT_FIELD_AUTOCORRECT:
		if (prev || v.i) ak_batch_set_enum(b, id, AK_PROP_TEXT_FIELD_AUTOCORRECT, (int32_t)v.i);
		break;
	case AK_KEY_TEXT_FIELD_CONTENT_TYPE: send_str(e, AK_PROP_TEXT_FIELD_CONTENT_TYPE, v); break;
	case AK_KEY_TEXT_FIELD_MULTILINE:
		f->multiline = v.i != 0;
		if (prev || v.i) ak_batch_set_bool(b, id, AK_PROP_TEXT_FIELD_MULTILINE, v.i != 0);
		ak_view_remeasure(e);
		break;
	case AK_KEY_TEXT_FIELD_PLAIN:
		if (prev || v.i) ak_batch_set_bool(b, id, AK_PROP_TEXT_FIELD_PLAIN, v.i != 0);
		break;
	case AK_KEY_TEXT_FIELD_AUTOFOCUS: break;  // at finalize
	case AK_KEY_TEXT_FIELD_SELECTION: field_send_selection(e, v.p); break;
	case AK_KEY_TEXT_FIELD_FOCUSED:
		// bind focused: true takes the keyboard, false gives it up.
		if ((v.i != 0) == f->focused) break;
		f->focused = v.i != 0;
		if (!e->finalized) {
			if (v.i) e->autofocus = true;  // with its first frame
		} else if (v.i) {
			ak_ui_focus(e);
		} else {
			ak_ui_blur(e);
		}
		break;
	default: ak_field_set_ext(e, key, v, prev); break;  // rich.c
	}
}

void ak_field_text_reported(ak_view *e, ak_str *s) {
	field_state *f = e->impl;
	ak_retain(s);
	ak_release(f->reported);
	f->reported = s;
	f->known = true;
}

static void field_finalize(ak_view *e) {
	field_state *f = e->impl;
	// The field reports its text when something follows it: a handler, a
	// bound text, its height (a multiline field's is its text's).
	if (ak_view_handler(e, AK_EV_TEXT_FIELD_CHANGE) || key_set(e, AK_KEY_TEXT_FIELD_TEXT) || f->multiline) ak_view_listen(e, AK_EVENT_TEXT_CHANGED);
	if (key_set(e, AK_KEY_TEXT_FIELD_SELECTION)) ak_view_listen(e, AK_EVENT_SELECTION_CHANGED);
	// Its focus, which the core follows (focus.c): the focused state of
	// the views around it, an overlay that closes as the focus leaves.
	ak_view_listen(e, AK_EVENT_FOCUS);
	ak_view_listen(e, AK_EVENT_BLUR);
	const ak_val *af = ak_style_get(e, AK_KEY_TEXT_FIELD_AUTOFOCUS);
	if (af && af->i) e->autofocus = true;  // with its first frame
	ak_field_finalize_ext(e);
}

static void field_event(ak_view *e, const ak_event *ev) {
	field_state *f = e->impl;
	if (ak_field_event_ext(e, ev)) return;
	switch (ev->kind) {
	case AK_EVENT_TEXT_CHANGED: {
		ak_str *s = ak_str_new(ev->s.p, ev->s.len);
		ak_release(f->reported);
		f->reported = s;
		f->known = true;
		if (f->multiline) ak_view_remeasure(e);  // its height is its text's
		ak_val arg = {.p = s};
		ak_retain(s);
		ak_view_call(e, AK_EV_TEXT_FIELD_CHANGE, &arg, 1);
		ak_release(s);
		break;
	}
	case AK_EVENT_SUBMIT: call0(e, AK_EV_TEXT_FIELD_SUBMIT); break;
	case AK_EVENT_FOCUS:
	case AK_EVENT_BLUR: {
		bool on = ev->kind == AK_EVENT_FOCUS;
		f->focused = on;
		ak_focus_report(e, on ? 2 : 0);
		call0(e, on ? AK_EV_TEXT_FIELD_FOCUS : AK_EV_TEXT_FIELD_BLUR);
		ak_val arg = {.i = on};
		ak_view_call(e, AK_EV_TEXT_FIELD_FOCUS_CHANGE, &arg, 1);
		break;
	}
	case AK_EVENT_SELECTION_CHANGED: {
		if (ev->s.len < 8) break;
		const uint8_t *p = (const uint8_t *)ev->s.p;
		uint32_t u0 = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
		uint32_t u1 = (uint32_t)p[4] | (uint32_t)p[5] << 8 | (uint32_t)p[6] << 16 | (uint32_t)p[7] << 24;
		f->sel[0] = u0, f->sel[1] = u1, f->sel_known = true;
		ak_struct *r = ak_struct_new(&ak_ui_shape_text_range);
		r->f[0].i = byte_offset(f->reported, u0);
		r->f[1].i = byte_offset(f->reported, u1);
		ak_val arg = {.p = r};
		ak_view_call(e, AK_EV_TEXT_FIELD_SELECT, &arg, 1);
		ak_release(r);
		break;
	}
	}
}

static const ak_el_class cls_field = {.name = "TextField", .kind = AK_KIND_TEXT_FIELD, .flags = AK_ELF_TEXT | AK_ELF_MEASURED, .init = field_init,
                                      .set = field_set, .finalize = field_finalize, .event = field_event, .drop = field_drop};

// ---- Controls with a value the user changes ----

// The value the host shows, as last sent or reported.
typedef struct {
	ak_val reported;
	bool known;
	bool indeterminate;  // Progress: sent as under way
} value_state;

static void value_init(ak_view *e) {
	ak_view_measure(e);
	e->impl = ak_alloc(sizeof(value_state));
}

static void value_drop(ak_view *e) { ak_free(e->impl); }

// controlled sends v as prop p unless the host shows it already.
static void controlled(ak_view *e, ak_prop p, uint8_t type, ak_val v) {
	value_state *s = e->impl;
	bool same = type == AK_T_FLOAT ? s->reported.f == v.f : s->reported.i == v.i;
	if (s->known && same) return;
	s->reported = v;
	s->known = true;
	ak_batch *b = e->r->batch;
	switch (type) {
	case AK_T_BOOL: ak_batch_set_bool(b, e->id, p, v.i != 0); break;
	case AK_T_INT: ak_batch_set_int(b, e->id, p, v.i); break;
	case AK_T_FLOAT: ak_batch_set_float(b, e->id, p, v.f); break;
	case AK_T_COLOR: ak_batch_set_color(b, e->id, p, (ak_color)v.i); break;
	}
}

static void reported(ak_view *e, ak_val v) {
	value_state *s = e->impl;
	s->reported = v;
	s->known = true;
}

static void toggle_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	switch (key) {
	case AK_KEY_TOGGLE_ON: controlled(e, AK_PROP_TOGGLE_ON, AK_T_BOOL, v); break;
	case AK_KEY_TOGGLE_TITLE: send_str(e, AK_PROP_A11Y_LABEL, v); break;
	case AK_KEY_TOGGLE_CHECKBOX:
		if (!prev && !v.i) break;
		ak_batch_set_bool(e->r->batch, e->id, AK_PROP_TOGGLE_CHECKBOX, v.i != 0);  // before it's measured: its view is made again
		e->variant = v.i ? 1 : 0;  // measured apart from switches
		ak_view_remeasure(e);
		break;
	}
}

static void toggle_event(ak_view *e, const ak_event *ev) {
	if (ev->kind != AK_EVENT_TOGGLED) return;
	ak_val v = {.i = ev->b};
	reported(e, v);
	ak_view_call(e, AK_EV_TOGGLE_CHANGE, &v, 1);
}

static void toggle_finalize(ak_view *e) {
	if (key_set(e, AK_KEY_TOGGLE_ON)) ak_view_listen(e, AK_EVENT_TOGGLED);
}

static const ak_el_class cls_toggle = {.name = "Toggle", .kind = AK_KIND_TOGGLE, .flags = AK_ELF_MEASURED | AK_ELF_CACHE, .init = value_init,
                                       .set = toggle_set, .finalize = toggle_finalize, .event = toggle_event, .drop = value_drop};

static void range_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	ak_batch *b = e->r->batch;
	switch (key) {
	case AK_KEY_SLIDER_VALUE: controlled(e, AK_PROP_SLIDER_VALUE, AK_T_FLOAT, v); break;
	case AK_KEY_STEPPER_VALUE: controlled(e, AK_PROP_STEPPER_VALUE, AK_T_FLOAT, v); break;
	// The bounds go at finalize, then as they change.
	case AK_KEY_SLIDER_MIN:
		if (e->finalized) ak_batch_set_float(b, e->id, AK_PROP_SLIDER_MIN, v.f);
		break;
	case AK_KEY_SLIDER_MAX:
		if (e->finalized) ak_batch_set_float(b, e->id, AK_PROP_SLIDER_MAX, v.f);
		break;
	case AK_KEY_STEPPER_MIN:
		if (e->finalized) ak_batch_set_float(b, e->id, AK_PROP_STEPPER_MIN, v.f);
		break;
	case AK_KEY_STEPPER_MAX:
		if (e->finalized) ak_batch_set_float(b, e->id, AK_PROP_STEPPER_MAX, v.f);
		break;
	case AK_KEY_STEPPER_STEP:
		if (e->finalized) ak_batch_set_float(b, e->id, AK_PROP_STEPPER_STEP, v.f > 0 ? v.f : 1);
		break;
	}
}

static double key_float(const ak_view *e, uint16_t key, double def) {
	const ak_val *v = ak_style_get(e, key);
	return v ? v->f : def;
}

static void slider_finalize(ak_view *e) {
	ak_batch *b = e->r->batch;
	ak_batch_set_float(b, e->id, AK_PROP_SLIDER_MIN, key_float(e, AK_KEY_SLIDER_MIN, 0));
	ak_batch_set_float(b, e->id, AK_PROP_SLIDER_MAX, key_float(e, AK_KEY_SLIDER_MAX, 1));
	if (key_set(e, AK_KEY_SLIDER_VALUE)) ak_view_listen(e, AK_EVENT_VALUE_CHANGED);
}

static void stepper_finalize(ak_view *e) {
	ak_batch *b = e->r->batch;
	double step = key_float(e, AK_KEY_STEPPER_STEP, 1);
	ak_batch_set_float(b, e->id, AK_PROP_STEPPER_MIN, key_float(e, AK_KEY_STEPPER_MIN, 0));
	ak_batch_set_float(b, e->id, AK_PROP_STEPPER_MAX, key_float(e, AK_KEY_STEPPER_MAX, 100));
	ak_batch_set_float(b, e->id, AK_PROP_STEPPER_STEP, step > 0 ? step : 1);
	if (key_set(e, AK_KEY_STEPPER_VALUE)) ak_view_listen(e, AK_EVENT_VALUE_CHANGED);
}

static void range_event(ak_view *e, const ak_event *ev) {
	if (ev->kind != AK_EVENT_VALUE_CHANGED) return;
	ak_val v = {.f = ev->f};
	reported(e, v);
	ak_view_call(e, e->el == AK_EL_SLIDER ? AK_EV_SLIDER_CHANGE : AK_EV_STEPPER_CHANGE, &v, 1);
}

static const ak_el_class cls_slider = {.name = "Slider", .kind = AK_KIND_SLIDER, .flags = AK_ELF_MEASURED, .init = value_init,
                                       .set = range_set, .finalize = slider_finalize, .event = range_event, .drop = value_drop};
static const ak_el_class cls_stepper = {.name = "Stepper", .kind = AK_KIND_STEPPER, .flags = AK_ELF_MEASURED, .init = value_init,
                                        .set = range_set, .finalize = stepper_finalize, .event = range_event, .drop = value_drop};

static void choice_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	switch (key) {
	case AK_KEY_SEGMENTED_SEGMENTS:
		send_strings(e, AK_PROP_SEGMENTED_SEGMENTS, v.p);
		ak_view_remeasure(e);
		break;
	case AK_KEY_SEGMENTED_SELECTED: controlled(e, AK_PROP_SEGMENTED_SELECTED, AK_T_INT, v); break;
	case AK_KEY_PICKER_ITEMS:
		send_strings(e, AK_PROP_PICKER_ITEMS, v.p);
		ak_view_remeasure(e);
		break;
	case AK_KEY_PICKER_PROMPT:
		send_str(e, AK_PROP_PICKER_LABEL, v);
		ak_view_remeasure(e);
		break;
	case AK_KEY_PICKER_SELECTED: {
		value_state *s = e->impl;
		if (s->known && s->reported.i != v.i) ak_view_remeasure(e);  // a pop-up button is as wide as its title
		controlled(e, AK_PROP_PICKER_SELECTED, AK_T_INT, v);
		break;
	}
	}
	AK_UNUSED(prev);
}

static void segmented_finalize(ak_view *e) {
	if (!key_set(e, AK_KEY_SEGMENTED_SEGMENTS)) send_strings(e, AK_PROP_SEGMENTED_SEGMENTS, NULL);
	if (key_set(e, AK_KEY_SEGMENTED_SELECTED) || ak_view_handler(e, AK_EV_SEGMENTED_CHANGE)) ak_view_listen(e, AK_EVENT_SELECTED);
}

static void picker_finalize(ak_view *e) {
	if (!key_set(e, AK_KEY_PICKER_ITEMS)) send_strings(e, AK_PROP_PICKER_ITEMS, NULL);
	if (!key_set(e, AK_KEY_PICKER_SELECTED)) controlled(e, AK_PROP_PICKER_SELECTED, AK_T_INT, (ak_val){.i = -1});
	ak_view_listen(e, AK_EVENT_SELECTED);
}

static void choice_event(ak_view *e, const ak_event *ev) {
	if (ev->kind != AK_EVENT_SELECTED) return;
	value_state *s = e->impl;
	ak_val v = {.i = ev->i};
	if (e->el == AK_EL_PICKER && (!s->known || s->reported.i != v.i)) ak_view_remeasure(e);
	reported(e, v);
	ak_view_call(e, e->el == AK_EL_SEGMENTED ? AK_EV_SEGMENTED_CHANGE : AK_EV_PICKER_CHANGE, &v, 1);
}

static const ak_el_class cls_segmented = {.name = "Segmented", .kind = AK_KIND_SEGMENTED, .flags = AK_ELF_TEXT | AK_ELF_MEASURED, .init = value_init,
                                          .set = choice_set, .finalize = segmented_finalize, .event = choice_event, .drop = value_drop};
static const ak_el_class cls_picker = {.name = "Picker", .kind = AK_KIND_PICKER, .flags = AK_ELF_TEXT | AK_ELF_MEASURED, .init = value_init,
                                       .set = choice_set, .finalize = picker_finalize, .event = choice_event, .drop = value_drop};

// ---- Wheel, ColorPicker, DatePicker ----

static void wheel_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	if (key == AK_KEY_WHEEL_ITEMS) send_strings(e, AK_PROP_WHEEL_ITEMS, v.p);
	else if (key == AK_KEY_WHEEL_SELECTED) controlled(e, AK_PROP_WHEEL_SELECTED, AK_T_INT, v);
}

static void wheel_finalize(ak_view *e) {
	if (!key_set(e, AK_KEY_WHEEL_ITEMS)) send_strings(e, AK_PROP_WHEEL_ITEMS, NULL);
	ak_view_listen(e, AK_EVENT_SELECTED);
}

static void wheel_event(ak_view *e, const ak_event *ev) {
	if (ev->kind != AK_EVENT_SELECTED) return;
	ak_val v = {.i = ev->i};
	reported(e, v);
	ak_view_call(e, AK_EV_WHEEL_CHANGE, &v, 1);
}

static const ak_el_class cls_wheel = {.name = "Wheel", .kind = AK_KIND_WHEEL, .flags = AK_ELF_MEASURED, .init = value_init,
                                      .set = wheel_set, .finalize = wheel_finalize, .event = wheel_event, .drop = value_drop};

static void color_picker_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	switch (key) {
	case AK_KEY_COLOR_PICKER_VALUE: controlled(e, AK_PROP_COLOR_PICKER_VALUE, AK_T_COLOR, v); break;
	case AK_KEY_COLOR_PICKER_ALPHA: ak_batch_set_bool(e->r->batch, e->id, AK_PROP_COLOR_PICKER_ALPHA, v.i != 0); break;
	case AK_KEY_COLOR_PICKER_TITLE: send_str(e, AK_PROP_COLOR_PICKER_TITLE, v); break;
	}
}

static void color_picker_finalize(ak_view *e) { ak_view_listen(e, AK_EVENT_COLOR_CHANGED); }

static void color_picker_event(ak_view *e, const ak_event *ev) {
	if (ev->kind != AK_EVENT_COLOR_CHANGED) return;
	ak_val v = {.i = ev->i};
	reported(e, v);
	ak_view_call(e, AK_EV_COLOR_PICKER_CHANGE, &v, 1);
}

static const ak_el_class cls_color_picker = {.name = "ColorPicker", .kind = AK_KIND_COLOR_PICKER, .flags = AK_ELF_MEASURED | AK_ELF_CACHE,
                                             .init = value_init, .set = color_picker_set, .finalize = color_picker_finalize,
                                             .event = color_picker_event, .drop = value_drop};

static void date_picker_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	ak_batch *b = e->r->batch;
	switch (key) {
	case AK_KEY_DATE_PICKER_VALUE: controlled(e, AK_PROP_DATE_PICKER_VALUE, AK_T_INT, v); break;
	case AK_KEY_DATE_PICKER_MODE:
		ak_batch_set_enum(b, e->id, AK_PROP_DATE_PICKER_MODE, (int32_t)v.i);
		ak_view_remeasure(e);  // a date and a time are wider
		break;
	case AK_KEY_DATE_PICKER_MIN: ak_batch_set_int(b, e->id, AK_PROP_DATE_PICKER_MIN, v.i); break;
	case AK_KEY_DATE_PICKER_MAX: ak_batch_set_int(b, e->id, AK_PROP_DATE_PICKER_MAX, v.i); break;
	case AK_KEY_DATE_PICKER_INLINE: ak_batch_set_bool(b, e->id, AK_PROP_DATE_PICKER_INLINE, v.i != 0); break;
	}
}

static void date_picker_finalize(ak_view *e) { ak_view_listen(e, AK_EVENT_VALUE_CHANGED); }

static void date_picker_event(ak_view *e, const ak_event *ev) {
	if (ev->kind != AK_EVENT_VALUE_CHANGED) return;
	ak_val v = {.i = (int64_t)ev->f};  // Unix milliseconds
	reported(e, v);
	ak_view_call(e, AK_EV_DATE_PICKER_CHANGE, &v, 1);
}

static const ak_el_class cls_date_picker = {.name = "DatePicker", .kind = AK_KIND_DATE_PICKER, .flags = AK_ELF_MEASURED, .init = value_init,
                                            .set = date_picker_set, .finalize = date_picker_finalize, .event = date_picker_event,
                                            .drop = value_drop};

// ---- Progress, ProgressRing, Activity ----

static void progress_value(ak_view *e, ak_val v, ak_prop indeterminate, ak_prop value) {
	value_state *s = e->impl;
	ak_batch *b = e->r->batch;
	const ak_box *box = v.p;
	if (!box) {
		if (!s->indeterminate) ak_batch_set_bool(b, e->id, indeterminate, true);
		s->indeterminate = true;
		return;
	}
	if (s->indeterminate) ak_batch_set_bool(b, e->id, indeterminate, false);
	s->indeterminate = false;
	ak_batch_set_float(b, e->id, value, clamp01(box->v.f));
}

static void progress_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	if (key == AK_KEY_PROGRESS_VALUE) progress_value(e, v, AK_PROP_PROGRESS_INDETERMINATE, AK_PROP_PROGRESS_VALUE);
	else if (key == AK_KEY_PROGRESS_RING_VALUE) progress_value(e, v, AK_PROP_PROGRESS_RING_INDETERMINATE, AK_PROP_PROGRESS_RING_VALUE);
}

// A Progress with no value is under way, how far unknown; a ProgressRing
// with none is the platform's spinner (told before it's measured: its
// view is another).
static void progress_finalize(ak_view *e) {
	uint16_t key = e->el == AK_EL_PROGRESS ? AK_KEY_PROGRESS_VALUE : AK_KEY_PROGRESS_RING_VALUE;
	const ak_val *v = ak_style_get(e, key);
	if (!v || !v->p) {
		value_state *s = e->impl;
		if (!s->indeterminate) ak_batch_set_bool(e->r->batch, e->id, e->el == AK_EL_PROGRESS ? AK_PROP_PROGRESS_INDETERMINATE : AK_PROP_PROGRESS_RING_INDETERMINATE, true);
		s->indeterminate = true;
	}
}

static const ak_el_class cls_progress = {.name = "Progress", .kind = AK_KIND_PROGRESS, .flags = AK_ELF_TEXT | AK_ELF_MEASURED, .init = value_init,
                                         .set = progress_set, .finalize = progress_finalize, .drop = value_drop};
static const ak_el_class cls_ring = {.name = "ProgressRing", .kind = AK_KIND_PROGRESS_RING, .flags = AK_ELF_MEASURED, .init = value_init,
                                     .set = progress_set, .finalize = progress_finalize, .drop = value_drop};

static void activity_init(ak_view *e) {
	ak_view_measure(e);
	ak_batch_set_bool(e->r->batch, e->id, AK_PROP_ACTIVITY_ANIMATING, true);
}

static const ak_el_class cls_activity = {.name = "Activity", .kind = AK_KIND_ACTIVITY, .flags = AK_ELF_MEASURED | AK_ELF_CACHE, .init = activity_init};

// ---- Image (media.c) ----

// ---- Scroll ----

// A Scroll's own box, and the box its children are stacked in.
typedef struct {
	ak_view *content;  // a kid of the scroll view
	bool refresh_shown;  // the host shows its spinner
	// A paging scroll's: its page as last reported or scrolled to, and its
	// size along its axis (a page's), which its content's grid tracks are.
	int64_t page;
	bool page_set;     // its page key is set: it scrolls there
	int64_t shown;     // the page the host shows (-1: none yet)
	float page_size, laid;
	ak_ltrack_size track;
} scroll_state;

static bool horizontal(const ak_view *e) {
	const ak_val *v = ak_style_get(e, e->el == AK_EL_SCROLL ? AK_KEY_SCROLL_HORIZONTAL : 0);
	return v && v->i;
}

static void scroll_adjust(ak_view *e, ak_lstyle *ls) {
	ls->flex_direction = AK_LCOLUMN;
	if (horizontal(e)) {
		ls->flex_direction = AK_LROW;
		ls->overflow_x = AK_LOVERFLOW_SCROLL;
		// Right to left, the content is laid out from the right, but it
		// sits at the scroll view's origin, where a platform's scroll view
		// wants it: the host starts at its end, not the layout.
		if (ak_view_rtl(e)) ls->direction = AK_LDIR_LTR;
	} else {
		ls->overflow_y = AK_LOVERFLOW_SCROLL;
	}
	ls->align_items = AK_LALIGN_STRETCH;
}

static bool paging(const ak_view *e) {
	const ak_val *v = ak_style_get(e, AK_KEY_SCROLL_PAGING);
	return v && v->i;
}

static void content_adjust(ak_view *e, ak_lstyle *cs) {
	cs->flex_shrink = 0;  // the content keeps its size; that's the point
	ak_view *s = e->parent;
	bool h = s && horizontal(s);
	cs->flex_direction = h ? AK_LROW : AK_LCOLUMN;
	if (s && s->el == AK_EL_SCROLL && paging(s)) {
		// Each child is a page, exactly as big as the scroll view along its
		// axis: a track each of a grid that flows along it.
		scroll_state *st = s->impl;
		if (st->page_size > 0) {
			st->track = ak_lpx(st->page_size);
			cs->display = AK_LDISPLAY_GRID;
			if (h) {
				cs->grid_auto_flow = AK_LFLOW_COLUMN;
				cs->grid_auto_columns = &st->track;
				cs->ngrid_auto_columns = 1;
			} else {
				cs->grid_auto_flow = AK_LFLOW_ROW;
				cs->grid_auto_rows = &st->track;
				cs->ngrid_auto_rows = 1;
			}
		}
	}
	if (h && ak_view_rtl(e)) {
		cs->direction = AK_LDIR_RTL;
		if (ak_llength_is_auto(cs->min_width)) cs->min_width = ak_lpct(100);  // short content hugs the right
	}
}

static const ak_el_class cls_content = {.name = "View", .kind = AK_KIND_VIEW, .flags = AK_ELF_CHILDREN, .adjust = content_adjust};

static void scroll_init(ak_view *e) {
	scroll_state *s = ak_alloc(sizeof *s);
	s->shown = -1;
	e->impl = s;
	s->content = ak_view_new(e->r, &cls_content, 0, true);
	ak_view_append(e, s->content);
	ak_release(s->content);  // e holds it
}

static void scroll_drop(ak_view *e) { ak_free(e->impl); }

static void scroll_add(ak_view *e, ak_view *child) { ak_view_append(((scroll_state *)e->impl)->content, child); }

// The keys that lay its content out go to the content's box.
static ak_view *scroll_style_target(ak_view *e, uint16_t key) {
	switch (key) {
	case AK_KEY_WRAP:
	case AK_KEY_JUSTIFY:
	case AK_KEY_ALIGN:
	case AK_KEY_GAP:
	case AK_KEY_PADDING:
		return ((scroll_state *)e->impl)->content;
	case AK_KEY_DIRECTION:
		return NULL;  // horizontal says
	}
	return e;
}

// refreshable: pull to refresh on a Scroll or List, while refreshing says
// the spinner shows.
void ak_refresh_set(ak_view *e, bool *shown, ak_prop refreshing, ak_val v) {
	if ((v.i != 0) != *shown) {
		*shown = v.i != 0;
		ak_batch_set_bool(e->r->batch, e->id, refreshing, *shown);
	}
}

void ak_refresh_event(ak_view *e, bool *shown, ak_prop refreshing, uint16_t key, uint16_t ev) {
	*shown = true;  // the host shows its spinner
	call0(e, ev);
	const ak_val *v = ak_style_get(e, key);
	if (!v || !v->i) {
		*shown = false;
		ak_batch_set_bool(e->r->batch, e->id, refreshing, false);
	}
}

// logical turns a page from the physical start (left, top) into one
// from the reading start, and back: right to left, horizontally, the
// first is the rightmost.
static int64_t logical(ak_view *e, int64_t i) {
	scroll_state *s = e->impl;
	if (!horizontal(e) || !ak_view_rtl(s->content)) return i;
	int64_t n = (int64_t)ak_lnode_child_count(s->content->box) - 1 - i;
	return n > 0 ? n : 0;
}

// scroll_to_page scrolls a paging Scroll to its page once it's laid out:
// animated, unless it's the first or its size changed (which keeps the
// page, at once).
static void scroll_to_page(ak_view *e) {
	scroll_state *s = e->impl;
	float sz = s->page_size;
	if (sz <= 0 || !s->page_set || (s->page == s->shown && sz == s->laid)) return;
	bool animated = s->laid == sz && s->shown >= 0;
	s->shown = s->page, s->laid = sz;
	float off = (float)logical(e, s->page) * sz;
	bool h = horizontal(e);
	ak_ui_scroll_to(e, h ? off : 0, h ? 0 : off, animated);
}

// ak_scroll_laid follows a paging Scroll's size once it's laid out: its
// pages are as big, and it's scrolled to its page. It reports whether
// its content must be laid out again.
bool ak_scroll_laid(ak_view *e) {
	scroll_state *s = e->impl;
	if (e->dead || !paging(e)) return false;
	ak_lframe f = ak_lnode_frame(e->box);
	float sz = horizontal(e) ? f.w : f.h;
	if (sz == s->page_size) {
		scroll_to_page(e);
		return false;
	}
	s->page_size = sz;
	ak_style_relayout(s->content);
	return true;
}

static void scroll_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	scroll_state *s = e->impl;
	switch (key) {
	case AK_KEY_SCROLL_HORIZONTAL:
		if (prev || v.i) ak_batch_set_bool(e->r->batch, e->id, AK_PROP_SCROLL_HORIZONTAL, v.i != 0);
		ak_style_relayout(e);
		ak_style_relayout(s->content);
		break;
	case AK_KEY_SCROLL_REFRESHING: ak_refresh_set(e, &s->refresh_shown, AK_PROP_SCROLL_REFRESHING, v); break;
	case AK_KEY_SCROLL_PAGING:
		if (prev || v.i) ak_batch_set_bool(e->r->batch, e->id, AK_PROP_SCROLL_PAGING, v.i != 0);
		if (v.i) {
			ak_view_listen(e, AK_EVENT_PAGED);
			ak_vec_push(&e->r->pagers, (ak_view *)ak_retain(e));
		}
		ak_style_relayout(s->content);
		break;
	case AK_KEY_SCROLL_PAGE:
		// Scrolled to the page, unless the host shows it (the user's swipe
		// reported it).
		s->page = v.i, s->page_set = true;
		scroll_to_page(e);
		break;
	}
}

static void scroll_finalize(ak_view *e) {
	if (ak_view_handler(e, AK_EV_SCROLL_REFRESH)) ak_batch_set_bool(e->r->batch, e->id, AK_PROP_SCROLL_REFRESHABLE, true);
}

static void scroll_event(ak_view *e, const ak_event *ev) {
	scroll_state *s = e->impl;
	if (ev->kind == AK_EVENT_SCROLLED) {
		ak_val args[2] = {{.f = (float)ev->x}, {.f = (float)ev->y}};
		ak_view_call(e, AK_EV_SCROLL_SCROLL, args, 2);
	} else if (ev->kind == AK_EVENT_REFRESH) {
		ak_refresh_event(e, &s->refresh_shown, AK_PROP_SCROLL_REFRESHING, AK_KEY_SCROLL_REFRESHING, AK_EV_SCROLL_REFRESH);
	} else if (ev->kind == AK_EVENT_PAGED) {
		int64_t page = logical(e, ev->i);
		s->shown = page;
		if (s->page_set) s->page = page;
		ak_val arg = {.i = page};
		ak_view_call(e, AK_EV_SCROLL_PAGE_CHANGE, &arg, 1);
	}
}

static const ak_el_class cls_scroll = {.name = "Scroll", .kind = AK_KIND_SCROLL, .flags = AK_ELF_CHILDREN, .init = scroll_init, .set = scroll_set,
                                       .finalize = scroll_finalize, .event = scroll_event, .adjust = scroll_adjust, .add = scroll_add,
                                       .style_target = scroll_style_target, .drop = scroll_drop};

// ---- List (list.c), the navigation's (nav.c) ----

extern const ak_el_class ak_class_list;
const ak_el_class *ak_nav_class_of(uint16_t el);

// ---- Classes by element ----

const ak_el_class *ak_el_class_of(uint16_t el) {
	switch (el) {
	case AK_EL_VIEW: return &cls_view;
	case AK_EL_ROW: return &cls_row;
	case AK_EL_COLUMN: return &cls_view;
	case AK_EL_SAFE_AREA: return &cls_safe;
	case AK_EL_TEXT: return &cls_text;
	case AK_EL_BUTTON: return &cls_button;
	case AK_EL_TEXT_FIELD: return &cls_field;
	case AK_EL_TOGGLE: return &cls_toggle;
	case AK_EL_SLIDER: return &cls_slider;
	case AK_EL_STEPPER: return &cls_stepper;
	case AK_EL_SEGMENTED: return &cls_segmented;
	case AK_EL_PICKER: return &cls_picker;
	case AK_EL_WHEEL: return &cls_wheel;
	case AK_EL_COLOR_PICKER: return &cls_color_picker;
	case AK_EL_DATE_PICKER: return &cls_date_picker;
	case AK_EL_PROGRESS: return &cls_progress;
	case AK_EL_PROGRESS_RING: return &cls_ring;
	case AK_EL_ACTIVITY: return &cls_activity;
	case AK_EL_SYMBOL: return &cls_symbol;
	case AK_EL_IMAGE: return &ak_class_image;
	case AK_EL_SCROLL: return &cls_scroll;
	case AK_EL_LIST: return &ak_class_list;
	}
	const ak_el_class *c = ak_nav_class_of(el);
	if (!c) c = ak_window_class_of(el);
	if (!c) c = ak_menu_class_of(el);
	return c ? c : ak_ext_class_of(el);
}

// ---- The protocol ----

ak_view *ak_ui_new(uint16_t el) {
	// A Menu in a MenuBar is one of its menus.
	const ak_el_class *cls = el == AK_EL_MENU ? ak_menu_class_here() : ak_el_class_of(el);
	if (!cls) ak_panic("ui: no element %u", el);
	ak_ui_root *r = ak_ui_current();
	ak_view *e = ak_view_new(r, cls, el, !(cls->flags & AK_ELF_FLATTEN));
	// Finalized as it's placed, or at the next flush (a view only
	// referred to).
	ak_vec_push(&r->init_q, (ak_view *)ak_retain(e));
	return e;
}

static void check_key(const ak_view *v, uint16_t key) {
	if (key == 0 || key >= AK_KEY__COUNT) ak_panic("ui: no key %u", key);
	uint8_t el = ak_key_els[key];
	if (key < AK_KEY__STYLE_END ? v->frag : el != v->el)
		ak_panic("ui: %s takes no key %s", v->el ? ak_el_names[v->el] : v->cls->name, ak_key_names[key]);
}

_Static_assert(AK_KEY__STYLE_END <= 64, "ak_view's outer has a bit per style key");

static void set_key(ak_view *v, uint16_t key, ak_val value, bool outer);

void ak_ui_set(ak_view *v, uint16_t key, ak_val value) { set_key(v, key, value, false); }

void ak_ui_set_outer(ak_view *v, uint16_t key, ak_val value) { set_key(v, key, value, true); }

static void set_key(ak_view *v, uint16_t key, ak_val value, bool outer) {
	check_key(v, key);
	if (v->dead) return;
	if (v->bar) ak_bar_dirty(v);  // a Button in a Screen's bar: the bar is sent again
	if (key < AK_KEY__STYLE_END) {
		// The caller's style is over the component's own.
		if (outer) v->outer |= 1ull << key;
		else if (v->outer >> key & 1) return;
		if (v->ix && ak_ix_set_base(v, key, value)) return;  // a key its variants set (interaction.c)
		ak_view *t = v->cls->style_target ? v->cls->style_target(v, key) : v;
		if (t) ak_style_set(t, key, value);
		if (v->cls->styled) v->cls->styled(v, key);
		return;
	}
	// An element's own key: stored with the style's, applied when it
	// changed (the first time always).
	ak_kind k = (ak_kind)ak_key_kinds[key];
	ak_style_ent *ent = NULL;
	for (uint32_t i = 0; i < v->style.len; i++)
		if (v->style.data[i].key == key) ent = &v->style.data[i];
	if (ent && ak_val_eq(ent->v, value, k)) return;
	ak_retain_val(value, k);
	ak_val prev = {0};
	bool had = ent != NULL;
	if (ent) {
		prev = ent->v;
		ent->v = value;
	} else {
		ak_vec_push(&v->style, ((ak_style_ent){key, value}));
	}
	if (v->cls->set) v->cls->set(v, key, value, had ? &prev : NULL);
	ak_release_val(prev, k);
}

// A binding: a key that follows a closure, in a render effect.
typedef struct {
	ak_view *v;
	ak_closure *thunk;
	uint16_t key;
	bool outer;
	ak_comp_info info;
} binding;

static void binding_run(void *ctx) {
	binding *b = ctx;
	ak_val v = ak_call(b->thunk, NULL, 0);
	if (ak_error_pending()) {
		void *err = ak_error_take();
		if (ak_is_cancel(err)) ak_release(err);
		else ak_raise(err);  // delivered to the owner's catcher once the run returns
		return;
	}
	set_key(b->v, b->key, v, b->outer);
	ak_release_val(v, (ak_kind)ak_key_kinds[b->key]);
}

static void binding_drop(void *ctx) {
	binding *b = ctx;
	ak_release(b->v);
	ak_release(b->thunk);
	ak_free(b);
}

// binding_describe names a binding as its source says it: "Card's text
// binding at card.asuka:30".
static void binding_describe(void *ctx, char *buf, size_t n) {
	binding *b = ctx;
	char what[64];
	snprintf(what, sizeof what, "%s binding", ak_key_names[b->key]);
	ak_vm_where(b->thunk, what, buf, n);
}

static const void *binding_site(void *ctx) { return ak_vm_where_site(((binding *)ctx)->thunk); }

static void bind_key(ak_view *v, uint16_t key, ak_closure *thunk, bool outer) {
	check_key(v, key);
	if (outer && key >= AK_KEY__STYLE_END) ak_panic("ui: %s isn't a style key", ak_key_names[key]);
	binding *b = ak_alloc(sizeof *b);
	b->v = ak_retain(v);
	b->thunk = ak_retain(thunk);
	b->key = key;
	b->outer = outer;
	b->info = (ak_comp_info){.drop = binding_drop, .label = ak_key_names[key], .describe = binding_describe, .site = binding_site};
	ak_effect_new(binding_run, b, AK_EFFECT_RENDER, &b->info);
}

void ak_ui_bind(ak_view *v, uint16_t key, ak_closure *thunk) { bind_key(v, key, thunk, false); }

void ak_ui_bind_outer(ak_view *v, uint16_t key, ak_closure *thunk) {
	if (key < AK_KEY__STYLE_END) v->outer |= 1ull << key;  // before the component's own effects run again
	bind_key(v, key, thunk, true);
}

void ak_ui_on(ak_view *v, uint16_t ev, ak_closure *handler) {
	if (ev == 0 || ev >= AK_EV__COUNT) ak_panic("ui: no event %u", ev);
	if (ak_ev_els[ev] != v->el) ak_panic("ui: %s has no event %s", v->el ? ak_el_names[v->el] : v->cls->name, ak_ev_names[ev]);
	if (!handler) return;
	ak_retain(handler);
	bool found = false;
	for (uint32_t i = 0; i < v->handlers.len; i++) {
		if (v->handlers.data[i].ev == ev) {
			ak_release(v->handlers.data[i].c);
			v->handlers.data[i].c = handler;
			found = true;
		}
	}
	if (!found) ak_vec_push(&v->handlers, ((ak_handler_ent){ev, handler}));
	// A container with a handler has a view of its own.
	if (!v->real && (v->cls->flags & (AK_ELF_FLATTEN | AK_ELF_FRAGMENT)) == AK_ELF_FLATTEN) ak_view_make_real(v);
	ak_event_kind k = ak_ev_ops(ev);
	if (k) ak_view_listen(v, k);
	ak_ix_on(v, ev);
}

void ak_ui_add(ak_view *v, ak_view *child) {
	if (!child) return;
	if (!(v->cls->flags & AK_ELF_CHILDREN)) ak_panic("ui: %s takes no children", v->el ? ak_el_names[v->el] : v->cls->name);
	if (v->cls->add) v->cls->add(v, child);
	else ak_view_append(v, child);
}

// A slot takes a view, or a builder (a closure returning one) for a lazy
// slot (a Popover's content), which the element builds when it shows.
void ak_ui_slot(ak_view *v, uint16_t slot, ak_view *child) {
	if (child && ((ak_obj *)child)->tag == AK_TAG_VIEW) ak_view_finalize(child);
	ak_ui_set(v, slot, (ak_val){.p = child});
}

void ak_ui_lazy(ak_view *v, ak_closure *build) {
	if (!v->cls->lazy) ak_panic("ui: %s builds no children lazily", v->el ? ak_el_names[v->el] : v->cls->name);
	v->cls->lazy(v, build);
}

ak_view *ak_ui_ref(ak_view *v) { return ak_retain(v); }

ak_view *ak_ui_fragment(void) { return ak_frag_new(); }

// ---- Commands ----

void ak_ui_focus(ak_view *v) {
	if (v && (v->el == AK_EL_WINDOW || v == v->r->el)) {  // a window's (the root: the main one's): it comes to the front
		ak_ui_window_action(v, AK_WINDOW_ACTION_FOCUS, 0, 0);
		return;
	}
	if (v && v->real && !v->dead) ak_batch_command(v->r->batch, v->id, AK_CMD_FOCUS, NULL, 0);
}

void ak_ui_blur(ak_view *v) {
	if (v && v->real && !v->dead) ak_batch_command(v->r->batch, v->id, AK_CMD_BLUR, NULL, 0);
}

// ak_ui_scroll_to_end scrolls a Scroll or List to its end: its bottom, a
// horizontal Scroll's trailing edge.
void ak_ui_scroll_to_end(ak_view *v, bool animated) {
	if (!v || v->dead) return;
	const ak_val *h = v->kind == AK_KIND_SCROLL ? ak_style_get(v, AK_KEY_SCROLL_HORIZONTAL) : NULL;
	bool across = h && h->i;
	ak_ui_scroll_to(v, across ? 1e9f : 0, across ? 0 : 1e9f, animated);
}

// ak_ui_scroll_to_item scrolls a List's item i (its index among its
// items) into view.
void ak_ui_scroll_to_item(ak_view *v, int64_t i, bool animated) {
	if (!v || v->dead || v->kind != AK_KIND_LIST || i < 0 || i > UINT32_MAX) return;
	uint8_t args[5];
	uint32_t u = (uint32_t)i;
	memcpy(args, &u, 4);
	args[4] = animated ? 1 : 0;
	ak_batch_command(v->r->batch, v->id, AK_CMD_SCROLL_TO, args, sizeof args);
}

// ak_ui_scroll_to scrolls a Scroll or List to the content offset x, y
// (points from its content's top left, as its scroll event reports; past
// the end is the end).
void ak_ui_scroll_to(ak_view *v, float x, float y, bool animated) {
	if (!v || v->dead || (v->kind != AK_KIND_SCROLL && v->kind != AK_KIND_LIST)) return;
	uint8_t args[9];
	memcpy(args, &x, 4);
	memcpy(args + 4, &y, 4);
	args[8] = animated ? 1 : 0;
	ak_batch_command(v->r->batch, v->id, AK_CMD_SCROLL_TO_OFFSET, args, sizeof args);
}
