// overlay.c: what shows over the screens: Sheet
// and present/dismiss, Popover, Alert and action sheets, Menu (and
// context menus), Snackbar.
//
// A sheet or popover is a provider (a host-sized layout root with its
// own environment) of no parent: its up is the window it's on, or the
// view it points at. Its content is built when it opens, under an owner
// of its own (a root under what declared or presented it), and disposed
// when it closes. An alert or a snackbar is a view of no parent too,
// held by its root until it goes (ak_ui_root.shown).
#include "ui/ui_int.h"

static const ak_val *key_of(const ak_view *e, uint16_t key) { return ak_style_get(e, key); }

static void hold(ak_view *v) { ak_vec_push(&v->r->shown, (ak_view *)ak_retain(v)); }

// unhold lets go of a view the root held while it showed.
static void unhold(ak_view *v) {
	ak_ui_root *r = v->r;
	for (uint32_t i = 0; i < r->shown.len; i++) {
		if (r->shown.data[i] == v) {
			memmove(r->shown.data + i, r->shown.data + i + 1, (r->shown.len - i - 1) * sizeof(ak_view *));
			r->shown.len--;
			ak_release(v);
			return;
		}
	}
}

// detents_bits is a [Detent]'s bits, as the wire has them (1 medium, 2
// large, 4 fit).
static int64_t detents_bits(const ak_arr *a) {
	int64_t b = 0;
	for (uint32_t i = 0; a && i < a->len; i++) b |= INT64_C(1) << ak_arr_vals(a)[i].i;
	return b;
}

ak_view *ak_alert_window(ak_ui_root *r) {
	void *v = NULL;
	if (ak_context_lookup(ak_window_key, &v)) {
		ak_view *w = ak_window_of(v);
		if (w && !w->dead) return w;
	}
	return r->el;
}

// ---- Sheets ----

typedef struct {
	ak_owner *owner;  // its content's
	ak_back *back;
	void (*dismissed)(void *ctx);  // the user's dismissal (and going back's, and dismiss())
	void *ctx;
	bool closed;
} sheet_t;

static void sheet_view_drop(ak_view *e) {
	sheet_t *s = e->impl;
	if (!s) return;
	if (s->owner) ak_owner_dispose(s->owner);
	ak_back_release(s->back);
	ak_free(s);
}

static void sheet_dismissed(void *ctx) {
	ak_view *e = ctx;
	sheet_t *s = e->impl;
	if (s->closed || e->dead) return;
	ak_retain(e);
	if (s->dismissed) s->dismissed(s->ctx);
	else ak_sheet_close(e);
	ak_release(e);
}

static void sheet_view_event(ak_view *e, const ak_event *ev) {
	sheet_t *s = e->impl;
	if (ev->kind == AK_EVENT_DISMISSED) sheet_dismissed(e);
	else if (ev->kind == AK_EVENT_BACK && s->back) ak_back_run(s->back);
}

static void prov_adjust(ak_view *e, ak_lstyle *ls) {
	ls->flex_direction = AK_LCOLUMN;
	ak_prov *p = e->prov;
	if (p && (p->fits || p->fit_win || p->fit_sheet)) return;
	ls->width = ak_lpct(100), ls->height = ak_lpct(100);
}

static const ak_el_class cls_sheet_view = {.name = "Sheet", .kind = AK_KIND_SHEET, .flags = AK_ELF_CHILDREN, .event = sheet_view_event,
                                           .adjust = prov_adjust, .drop = sheet_view_drop};

void ak_sheet_close(ak_view *e) {
	sheet_t *s = e->impl;
	if (!s || s->closed) return;
	s->closed = true;
	if (s->owner) {
		ak_owner *o = s->owner;
		s->owner = NULL;
		ak_owner_dispose(o);
	}
	ak_view_destroy(e);
	unhold(e);
}

ak_view *ak_sheet_present(ak_ui_root *r, ak_owner *owner, ak_closure *build, ak_val *args, uint32_t nargs, int64_t detents, bool grabber, bool full,
                          bool routed, void (*dismissed)(void *ctx), void *ctx) {
	ak_view *up = ak_alert_window(r);  // the window it's declared in
	ak_view *e = ak_view_new(r, &cls_sheet_view, 0, true);
	sheet_t *s = ak_alloc(sizeof *s);
	e->impl = s;
	s->dismissed = dismissed;
	s->ctx = ctx;
	ak_prov *pv = ak_prov_new(r);
	// As tall as its content where the platform has such a sheet; a
	// sheet can open before the root's environment names the platform
	// (the web's), so its own first one settles it (ak_prov_report).
	pv->fit_wait = !full && (detents & 4);
	pv->fit_sheet = pv->fit_wait && ak_fit_sheet_platform(r->env.platform);
	ak_prov_attach(e, pv);
	ak_release(pv);
	e->up = up;
	ak_batch *b = r->batch;
	ak_batch_set_int(b, e->id, AK_PROP_SHEET_WINDOW, up->id);
	if (full) {
		ak_batch_set_bool(b, e->id, AK_PROP_SHEET_FULL_SCREEN, true);
	} else {
		if (detents) ak_batch_set_int(b, e->id, AK_PROP_SHEET_DETENTS, detents);
		if (grabber) ak_batch_set_bool(b, e->id, AK_PROP_SHEET_GRABBER, true);
	}
	if (routed) ak_batch_set_bool(b, e->id, AK_PROP_SHEET_ROUTED, true);
	ak_view_listen(e, AK_EVENT_DISMISSED);
	e->finalized = true;
	hold(e);
	ak_release(e);  // the root holds it while it shows
	s->owner = ak_root_under(owner);
	s->back = ak_back_new(sheet_dismissed, e);
	ak_back_attach(s->back, e);
	ak_owner_set_context(s->owner, ak_back_key, s->back);
	ak_owner_set_context(s->owner, ak_env_key, pv);
	ak_owner_set_context(s->owner, ak_sheet_key, e);
	ak_view *v = ak_build_under(r, s->owner, build, args, nargs);
	if (v) {
		ak_view *k = ak_only_kid(r, v);
		ak_view_append(e, k);
		ak_release(k);
	}
	ak_batch_command(b, e->id, AK_CMD_PRESENT, NULL, 0);
	return e;
}

// The Sheet element: the sheet shows while open is true.
typedef struct {
	ak_closure *build;
	ak_view *shown;  // the sheet presented (retained)
} sheet_el;

static void sheet_el_close(ak_view *e) {
	sheet_el *s = e->impl;
	if (!s->shown) return;
	ak_view *v = s->shown;
	s->shown = NULL;
	ak_sheet_close(v);
	ak_release(v);
}

static void sheet_el_dismissed(void *ctx) {
	ak_view *e = ctx;
	ak_retain(e);
	sheet_el_close(e);
	ak_val f = {.i = 0};
	ak_view_call(e, AK_EV_SHEET_CHANGE, &f, 1);
	ak_view_call(e, AK_EV_SHEET_DISMISS, NULL, 0);
	ak_release(e);
}

static void sheet_el_open(ak_view *e) {
	sheet_el *s = e->impl;
	if (s->shown || !s->build || e->dead || !e->finalized) return;
	const ak_val *d = key_of(e, AK_KEY_SHEET_DETENTS), *g = key_of(e, AK_KEY_SHEET_GRABBER), *f = key_of(e, AK_KEY_SHEET_FULL_SCREEN);
	ak_view *v = ak_sheet_present(e->r, e->owner, s->build, NULL, 0, d ? detents_bits(d->p) : 0, g && g->i, f && f->i, false, sheet_el_dismissed, e);
	s->shown = ak_retain(v);
}

static void sheet_el_cleanup(void *ctx) {
	ak_view *e = ctx;
	sheet_el_close(e);
	ak_release(e);
}

static void sheet_el_init(ak_view *e) {
	e->impl = ak_alloc(sizeof(sheet_el));
	ak_on_cleanup(sheet_el_cleanup, ak_retain(e));
}

static void sheet_el_drop(ak_view *e) {
	sheet_el *s = e->impl;
	if (!s) return;
	ak_release(s->build);
	ak_release(s->shown);
	ak_free(s);
}

static void sheet_el_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	if (key != AK_KEY_SHEET_OPEN) return;
	if (v.i) sheet_el_open(e);
	else sheet_el_close(e);
}

static void sheet_el_finalize(ak_view *e) {
	const ak_val *o = key_of(e, AK_KEY_SHEET_OPEN);
	if (o && o->i) sheet_el_open(e);
}

static void sheet_el_lazy(ak_view *e, ak_closure *build) {
	sheet_el *s = e->impl;
	ak_retain(build);
	ak_release(s->build);
	s->build = build;
	const ak_val *o = key_of(e, AK_KEY_SHEET_OPEN);
	if (o && o->i) sheet_el_open(e);
}

const ak_el_class ak_class_sheet = {.name = "Sheet", .kind = AK_KIND_VIEW, .flags = AK_ELF_FRAGMENT | AK_ELF_FLATTEN | AK_ELF_CHILDREN,
                                    .init = sheet_el_init, .set = sheet_el_set, .finalize = sheet_el_finalize, .lazy = sheet_el_lazy,
                                    .drop = sheet_el_drop};

// present and dismiss: a sheet presented by code, closed by the user, by
// dismiss() in it, or as what presented it goes.
static void presented_dismissed(void *ctx) { ak_sheet_close(ctx); }

static void presented_cleanup(void *ctx) {
	ak_view *e = ctx;
	ak_sheet_close(e);
	ak_release(e);
}

void ak_ui_present(ak_closure *build) {
	ak_ui_root *r = ak_ui_current();
	ak_owner *o = ak_current_owner();
	ak_view *e = ak_sheet_present(r, o, build, NULL, 0, 0, false, false, false, presented_dismissed, NULL);
	((sheet_t *)e->impl)->ctx = e;
	ak_on_cleanup(presented_cleanup, ak_retain(e));
}

void ak_ui_dismiss(void) {
	void *v = NULL;
	if (!ak_context_lookup(ak_sheet_key, &v)) {
		ak_throw_msg("dismiss: not in a sheet");
		return;
	}
	sheet_dismissed(v);  // as the user's dismissal
}

// ---- Popover ----

typedef struct {
	ak_view *pop;     // the popover shown (retained)
	ak_owner *owner;  // its content's
	ak_back *back;
} popover_t;

static void pop_view_event(ak_view *e, const ak_event *ev);

static const ak_el_class cls_pop_view = {.name = "Popover", .kind = AK_KIND_POPOVER, .flags = AK_ELF_CHILDREN, .event = pop_view_event,
                                         .adjust = prov_adjust};

static void pop_close(ak_view *anchor) {
	popover_t *p = anchor->impl;
	if (!p->pop) return;
	ak_view *v = p->pop;
	p->pop = NULL;
	if (p->owner) {
		ak_owner *o = p->owner;
		p->owner = NULL;
		ak_owner_dispose(o);
	}
	ak_back_release(p->back);
	p->back = NULL;
	ak_view_destroy(v);
	ak_release(v);
}

static void pop_dismissed(void *ctx) {
	ak_view *anchor = ctx;
	popover_t *p = anchor->impl;
	if (!p->pop) return;
	ak_retain(anchor);
	pop_close(anchor);
	ak_val f = {.i = 0};
	ak_view_call(anchor, AK_EV_POPOVER_CHANGE, &f, 1);
	ak_view_call(anchor, AK_EV_POPOVER_DISMISS, NULL, 0);
	ak_release(anchor);
}

static void pop_view_event(ak_view *e, const ak_event *ev) {
	ak_view *anchor = e->up;
	if (!anchor || anchor->dead) return;
	popover_t *p = anchor->impl;
	if (p->pop != e) return;
	if (ev->kind == AK_EVENT_DISMISSED) pop_dismissed(anchor);
	else if (ev->kind == AK_EVENT_BACK && p->back) ak_back_run(p->back);
}

static void pop_open(ak_view *anchor) {
	popover_t *p = anchor->impl;
	const ak_val *c = key_of(anchor, AK_KEY_POPOVER_CONTENT);
	if (p->pop || anchor->dead || !anchor->finalized || !c || !c->p) return;
	ak_ui_root *r = anchor->r;
	ak_view *e = ak_view_new(r, &cls_pop_view, 0, true);
	ak_prov *pv = ak_prov_new(r);
	pv->fits = true;
	const ak_val *mw = key_of(anchor, AK_KEY_POPOVER_MAX_WIDTH);
	pv->max_w = mw && mw->f > 0 ? (float)mw->f : 320;
	ak_prov_attach(e, pv);
	ak_release(pv);
	e->up = anchor;
	e->finalized = true;
	p->pop = e;
	ak_batch_set_int(r->batch, e->id, AK_PROP_POPOVER_ANCHOR, anchor->id);
	ak_view_listen(e, AK_EVENT_DISMISSED);
	p->owner = ak_root_under(anchor->owner);
	p->back = ak_back_new(pop_dismissed, anchor);
	ak_back_attach(p->back, e);
	ak_owner_set_context(p->owner, ak_back_key, p->back);
	ak_owner_set_context(p->owner, ak_env_key, pv);
	ak_val content = *c;
	ak_view *v = NULL;
	if (((ak_obj *)content.p)->tag == AK_TAG_CLOSURE) {
		v = ak_build_under(r, p->owner, content.p, NULL, 0);
	} else if (!((ak_view *)content.p)->parent) {
		v = ak_retain(content.p);  // built already (C code's)
	}
	if (v) {
		ak_view_append(e, v);
		ak_release(v);
	}
	ak_batch_command(r->batch, e->id, AK_CMD_PRESENT, NULL, 0);
}

static void pop_cleanup(void *ctx) {
	ak_view *anchor = ctx;
	pop_close(anchor);
	ak_release(anchor);
}

static void popover_init(ak_view *e) {
	e->impl = ak_alloc(sizeof(popover_t));
	ak_on_cleanup(pop_cleanup, ak_retain(e));
}

static void popover_drop(ak_view *e) {
	popover_t *p = e->impl;
	if (!p) return;
	if (p->pop) {
		if (p->owner) ak_owner_dispose(p->owner);
		ak_back_release(p->back);
		ak_view_destroy(p->pop);
		ak_release(p->pop);
	}
	ak_free(p);
}

static void popover_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	if (key == AK_KEY_POPOVER_OPEN) {
		if (v.i) pop_open(e);
		else pop_close(e);
	} else if (key == AK_KEY_POPOVER_CONTENT) {
		const ak_val *o = key_of(e, AK_KEY_POPOVER_OPEN);
		if (o && o->i) pop_open(e);
	}
}

static void popover_finalize(ak_view *e) {
	const ak_val *o = key_of(e, AK_KEY_POPOVER_OPEN);
	if (o && o->i) pop_open(e);
}

const ak_el_class ak_class_popover = {.name = "Popover", .kind = AK_KIND_VIEW, .flags = AK_ELF_CHILDREN, .init = popover_init, .set = popover_set,
                                      .finalize = popover_finalize, .drop = popover_drop};

// ---- Menu ----

static void menu_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	ak_batch *b = e->r->batch;
	if (key == AK_KEY_MENU_ACTIONS) {
		ak_buf *out = ak_batch_set_bytes_begin(b, e->id, AK_PROP_MENU);
		ak_encode_actions(out, v.p);
		ak_batch_set_bytes_end(b);
		ak_view_listen(e, AK_EVENT_ACTION);
	} else if (key == AK_KEY_MENU_LONG_PRESS) {
		if (prev || v.i) ak_batch_set_enum(b, e->id, AK_PROP_MENU_TRIGGER, v.i ? AK_MENU_TRIGGER_LONG_PRESS : AK_MENU_TRIGGER_TAP);
	}
}

static void menu_event(ak_view *e, const ak_event *ev) {
	if (ev->kind != AK_EVENT_ACTION) return;
	const ak_val *as = key_of(e, AK_KEY_MENU_ACTIONS);
	ak_run_action(e, as ? ak_find_action(as->p, ev->i) : NULL);
}

const ak_el_class ak_class_menu = {.name = "Menu", .kind = AK_KIND_VIEW, .flags = AK_ELF_CHILDREN, .set = menu_set, .event = menu_event};

// ---- Alert ----

typedef struct {
	ak_arr *actions;     // as last sent (retained)
	ak_closure *thunk;   // what makes them, followed while it shows (NULL: they're static)
	ak_arr *fields;      // its fields (retained)
	ak_owner *owner;     // its buttons' effect; its cleanup destroys it
	ak_buf sent;
	bool closed;
} alert_t;

// encode_flat encodes an alert's buttons: flat (an alert shows no
// submenu), each with its role, title, icon, disabled and checked.
static void encode_flat(ak_buf *b, const ak_arr *as) {
	ak_buf_u32(b, as ? as->len : 0);
	for (uint32_t i = 0; as && i < as->len; i++) {
		const ak_enum *a = ak_arr_vals(as)[i].p;
		uint16_t c = a ? ak_enum_case(a) : 0;
		const ak_str *title = a ? a->f[0].p : NULL, *icon = c <= 1 && a ? a->f[1].p : NULL;
		uint8_t role = 0, flags = 0;
		if (a && c == 0) {
			role = (uint8_t)a->f[2].i;
			if (a->f[3].i) flags |= AK_MENU_DISABLED;
			if (a->f[4].i) flags |= AK_MENU_CHECKED;
		}
		ak_buf_u8(b, role);
		ak_buf_str(b, title ? title->data : "", title ? title->len : 0);
		ak_buf_str(b, icon ? icon->data : "", icon ? icon->len : 0);
		ak_buf_u8(b, flags);
	}
}

static void alert_send(ak_view *e, ak_arr *as) {
	alert_t *a = e->impl;
	ak_buf b = {0};
	encode_flat(&b, as);
	if (a->sent.len != b.len || memcmp(a->sent.data, b.data, b.len) != 0) {
		if (!e->dead) ak_batch_set_bytes(e->r->batch, e->id, AK_PROP_ALERT_BUTTONS, b.data, b.len);
		ak_buf_free(&a->sent);
		a->sent = b;
	} else {
		ak_buf_free(&b);
	}
	ak_retain(as);
	ak_release(a->actions);
	a->actions = as;
}

static void alert_run(void *ctx) {
	ak_view *e = ctx;
	alert_t *a = e->impl;
	ak_val v = ak_call(a->thunk, NULL, 0);
	if (ak_error_pending()) {
		void *err = ak_error_take();
		if (ak_is_cancel(err)) ak_release(err);
		else ak_raise(err);
		return;
	}
	alert_send(e, v.p);
	ak_release(v.p);
}

static void alert_close(ak_view *e) {
	alert_t *a = e->impl;
	if (a->closed) return;
	a->closed = true;
	ak_retain(e);
	ak_owner *o = a->owner;
	a->owner = NULL;
	if (o) {
		ak_owner_dispose(o);  // nothing, when it's its disposal that closes it
		ak_owner_release(o);
	}
	ak_view_destroy(e);
	unhold(e);
	ak_release(e);
}

static void alert_cleanup(void *ctx) { alert_close(ctx); }

// new_child makes an owner under the current one, disposed with it, kept
// (retained) until it's disposed.
static void new_child(void *ctx) {
	ak_owner *o = ak_owner_new();
	ak_owner_retain(o);
	*(ak_owner **)ctx = o;
}

static void release_ctx(void *ctx) { ak_release(ctx); }

static const ak_comp_info alert_info = {.drop = release_ctx, .label = "Alert"};

static void alert_event(ak_view *e, const ak_event *ev) {
	alert_t *a = e->impl;
	switch (ev->kind) {
	case AK_EVENT_ACTION: {
		ak_arr *as = a->actions ? ak_retain(a->actions) : NULL;
		ak_retain(e);
		alert_close(e);
		if (as && ev->i >= 0 && ev->i < as->len) {
			const ak_enum *x = ak_arr_vals(as)[ev->i].p;
			// A disabled one: the host's alert was out of date.
			if (x && ak_enum_case(x) == 0 && !x->f[3].i) ak_run_action(e, x);
		}
		ak_release(as);
		ak_release(e);
		break;
	}
	case AK_EVENT_DISMISSED: alert_close(e); break;
	case AK_EVENT_FIELD_CHANGED: {
		uint32_t i;
		ak_span text;
		if (!ak_decode_field_changed((const uint8_t *)ev->s.p, ev->s.len, &i, &text) || !a->fields || i >= a->fields->len || a->closed) break;
		const ak_struct *f = ak_arr_vals(a->fields)[i].p;
		ak_closure *change = f ? f->f[5].p : NULL;
		if (!change) break;
		ak_str *t = ak_str_new(text.p, text.len);
		ak_val arg = {.p = t};
		ak_retain(change);
		ak_val r = ak_call(change, &arg, 1);
		if (!ak_error_pending()) ak_release(r.p);
		else ak_ui_raise_pending(NULL);
		ak_release(change);
		ak_release(t);
		break;
	}
	}
}

static void alert_drop(ak_view *e) {
	alert_t *a = e->impl;
	if (!a) return;
	ak_release(a->actions);
	ak_release(a->thunk);
	ak_release(a->fields);
	ak_buf_free(&a->sent);
	ak_free(a);
}

static const ak_el_class cls_alert = {.name = "Alert", .kind = AK_KIND_ALERT, .event = alert_event, .drop = alert_drop};

typedef struct {
	ak_view *e;
} alert_setup;

static void alert_effects(void *ctx) {
	alert_setup *s = ctx;
	ak_view *e = s->e;
	alert_t *a = e->impl;
	ak_on_cleanup(alert_cleanup, e);  // goes with its root (unmounted, reloaded)
	if (a->thunk) ak_effect_new(alert_run, ak_retain(e), AK_EFFECT_RENDER, &alert_info);
}

// ak_ui_alert shows an alert (or an action sheet) now: actions are an
// [Action], or a closure returning one that the alert follows while it
// shows (a button disabled while a field is empty); none is a single OK.
// Fields ([AlertField]) are text fields under the message.
void ak_ui_alert(ak_str *title, ak_str *message, ak_obj *actions, bool action_sheet, ak_arr *fields) {
	ak_ui_root *r = ak_ui_current();
	ak_view *e = ak_view_new(r, &cls_alert, 0, true);
	alert_t *a = ak_alloc(sizeof *a);
	e->impl = a;
	e->finalized = true;
	ak_batch *b = r->batch;
	ak_batch_set_int(b, e->id, AK_PROP_ALERT_WINDOW, ak_alert_window(r)->id);
	if (title && title->len) ak_batch_set_string(b, e->id, AK_PROP_ALERT_TITLE, title->data, title->len);
	if (message && message->len) ak_batch_set_string(b, e->id, AK_PROP_ALERT_MESSAGE, message->data, message->len);
	if (actions && actions->tag == AK_TAG_CLOSURE) {
		a->thunk = ak_retain((ak_closure *)actions);
	} else {
		ak_arr *as = (ak_arr *)actions;
		if (!as || as->len == 0) {
			// A single OK.
			static const uint8_t k[] = {AK_K_REF, AK_K_REF, AK_K_INT, AK_K_BOOL, AK_K_BOOL, AK_K_REF};
			static const ak_case cases[] = {{"button", 6, k, NULL, NULL}};
			static const ak_shape action = {"Action", 0, NULL, NULL, NULL, 1, cases, false, NULL};
			ak_enum *ok = ak_enum_new(&action, 0);
			ok->f[0].p = ak_str_c("OK");
			ok->f[1].p = ak_str_empty();
			ok->f[2].i = AK_ALERT_BUTTON_ROLE_CANCEL;
			as = ak_arr_new(AK_K_REF, NULL, 1);
			ak_arr_push(&as, (ak_val){.p = ok});
			alert_send(e, as);
			ak_release(as);
		} else {
			alert_send(e, as);
		}
	}
	if (action_sheet) ak_batch_set_enum(b, e->id, AK_PROP_ALERT_STYLE, AK_ALERT_STYLE_ACTION_SHEET);
	if (fields && fields->len && !action_sheet) {
		a->fields = ak_retain(fields);
		ak_alert_field *fs = ak_alloc(fields->len * sizeof *fs);
		for (uint32_t i = 0; i < fields->len; i++) {
			const ak_struct *f = ak_arr_vals(fields)[i].p;
			const ak_str *text = f->f[0].p, *ph = f->f[1].p, *ct = f->f[4].p;
			fs[i] = (ak_alert_field){ph ? ak_strn(ph->data, ph->len) : ak_cstr(""), text ? ak_strn(text->data, text->len) : ak_cstr(""),
			                         f->f[2].i ? AK_ALERT_FIELD_SECURE : 0, (uint8_t)f->f[3].i, ct ? ak_strn(ct->data, ct->len) : ak_cstr("")};
		}
		ak_buf *out = ak_batch_set_bytes_begin(b, e->id, AK_PROP_ALERT_FIELDS);
		ak_append_alert_fields(out, fs, fields->len);
		ak_batch_set_bytes_end(b);
		ak_free(fs);
		ak_view_listen(e, AK_EVENT_FIELD_CHANGED);
	}
	ak_view_listen(e, AK_EVENT_ACTION);
	ak_view_listen(e, AK_EVENT_DISMISSED);
	hold(e);
	// Its own owner, under the root's: its buttons follow their thunk while
	// it shows, and it goes with the root.
	ak_ui_root *pr = ak_ui.cur;
	ak_ui.cur = r;
	ak_with_owner(r->owner, new_child, &a->owner);
	alert_setup st = {e};
	ak_with_owner(a->owner, alert_effects, &st);
	ak_ui.cur = pr;
	ak_batch_command(b, e->id, AK_CMD_PRESENT, NULL, 0);
	ak_release(e);  // the root holds it
}

// ---- The Alert element ----
//
// Alert(title) { bind open: asking; message: …; Button(…) { role: …; on
// press { … } }; TextField { … } }: an alert shown while open is true. The
// Buttons and TextFields in its block are its buttons and fields, not
// views of the screen (as a Screen's bar's Buttons are its items): the
// alert sends them, again as one of them changes while it shows, runs a
// Button's press when it's chosen and a field's change as the user
// types. Choosing a button, or the user dismissing it, writes open
// false.

typedef struct {
	AK_VEC(ak_view *) kids;             // what's in its block (retained)
	AK_VEC(ak_view *) buttons, fields;  // its Buttons and TextFields, as last gathered (retained)
	ak_view *shown;                     // the alert showing (retained)
	ak_id win;                          // the window it's declared in
	ak_buf sent;                        // its buttons, as last sent
} alert_el;

typedef struct {
	ak_view *el;  // the element (not retained; NULL once it's let go)
} alert_shown;

static void release_views(void *vec) {
	AK_VEC(ak_view *) *v = vec;
	for (uint32_t i = 0; i < v->len; i++) ak_release(v->data[i]);
	v->len = 0;
}

// gather collects the Buttons and TextFields of what's in the block, in
// order, marking them the alert's: a change to them syncs it.
static void gather(ak_view *e, ak_view *v, alert_el *a) {
	if (!v) return;
	v->bar = e;
	if (v->el == AK_EL_BUTTON) {
		ak_vec_push(&a->buttons, (ak_view *)ak_retain(v));
		return;
	}
	if (v->el == AK_EL_TEXT_FIELD) {
		ak_vec_push(&a->fields, (ak_view *)ak_retain(v));
		return;
	}
	for (uint32_t i = 0; i < v->kids.len; i++) gather(e, v->kids.data[i], a);
}

static void regather(ak_view *e) {
	alert_el *a = e->impl;
	release_views(&a->buttons);
	release_views(&a->fields);
	for (uint32_t i = 0; i < a->kids.len; i++) gather(e, a->kids.data[i], a);
}

static ak_str *el_str(const ak_view *v, uint16_t key) {
	const ak_val *x = key_of(v, key);
	return x ? x->p : NULL;
}

static void el_put(ak_buf *b, const ak_str *s) { ak_buf_str(b, s ? s->data : "", s ? s->len : 0); }

// encode_alert_buttons encodes its Buttons as an alert's (flat: each its
// role, title, icon and disabled); with none, a single OK.
static void encode_alert_buttons(ak_buf *b, const alert_el *a) {
	if (!a->buttons.len) {
		ak_buf_u32(b, 1);
		ak_buf_u8(b, AK_ALERT_BUTTON_ROLE_CANCEL);
		ak_buf_str(b, "OK", 2);
		ak_buf_str(b, "", 0);
		ak_buf_u8(b, 0);
		return;
	}
	ak_buf_u32(b, a->buttons.len);
	for (uint32_t i = 0; i < a->buttons.len; i++) {
		ak_view *x = a->buttons.data[i];
		const ak_val *role = key_of(x, AK_KEY_BUTTON_ROLE), *d = key_of(x, AK_KEY_DISABLED);
		ak_buf_u8(b, role ? (uint8_t)role->i : 0);
		el_put(b, el_str(x, AK_KEY_BUTTON_TITLE));
		el_put(b, el_str(x, AK_KEY_BUTTON_ICON));
		ak_buf_u8(b, d && d->i ? AK_MENU_DISABLED : 0);
	}
}

static void el_send_buttons(ak_view *e) {
	alert_el *a = e->impl;
	if (!a->shown) return;
	ak_buf b = {0};
	encode_alert_buttons(&b, a);
	if (a->sent.len != b.len || memcmp(a->sent.data, b.data, b.len) != 0) {
		ak_batch_set_bytes(e->r->batch, a->shown->id, AK_PROP_ALERT_BUTTONS, b.data, b.len);
		ak_buf_free(&a->sent);
		a->sent = b;
	} else {
		ak_buf_free(&b);
	}
}

// el_close lets the alert showing go; the user's closing (a button, a
// dismissal) writes open false.
static void el_close(ak_view *e, bool user) {
	alert_el *a = e->impl;
	ak_view *v = a->shown;
	if (!v) return;
	a->shown = NULL;
	ak_buf_free(&a->sent);
	((alert_shown *)v->impl)->el = NULL;
	ak_view_destroy(v);
	unhold(v);
	ak_release(v);
	if (user) {
		ak_val off = {.i = 0};
		ak_view_call(e, AK_EV_ALERT_CHANGE, &off, 1);
	}
}

static void shown_event(ak_view *v, const ak_event *ev) {
	alert_shown *s = v->impl;
	ak_view *e = s->el;
	if (!e) return;
	alert_el *a = e->impl;
	ak_retain(e);
	switch (ev->kind) {
	case AK_EVENT_ACTION: {
		ak_view *x = ev->i >= 0 && ev->i < a->buttons.len ? ak_retain(a->buttons.data[ev->i]) : NULL;
		el_close(e, true);
		const ak_val *d = x ? key_of(x, AK_KEY_DISABLED) : NULL;
		if (x && !(d && d->i) && !x->dead) {  // a disabled one: the host's alert was out of date
			ak_view_call(x, AK_EV_BUTTON_PRESS, NULL, 0);
			const ak_val *href = key_of(x, AK_KEY_BUTTON_HREF);
			if (href && href->p && ((ak_str *)href->p)->len) ak_router_href(e, href->p);
		}
		ak_release(x);
		break;
	}
	case AK_EVENT_DISMISSED:
		el_close(e, true);
		ak_view_call(e, AK_EV_ALERT_DISMISS, NULL, 0);
		break;
	case AK_EVENT_FIELD_CHANGED: {
		uint32_t i;
		ak_span text;
		if (!ak_decode_field_changed((const uint8_t *)ev->s.p, ev->s.len, &i, &text) || i >= a->fields.len) break;
		ak_view *x = ak_retain(a->fields.data[i]);
		ak_val arg = {.p = ak_str_new(text.p, text.len)};
		ak_view_call(x, AK_EV_TEXT_FIELD_CHANGE, &arg, 1);
		ak_release(arg.p);
		ak_release(x);
		break;
	}
	}
	ak_release(e);
}

static void shown_drop(ak_view *v) { ak_free(v->impl); }

static const ak_el_class cls_alert_shown = {.name = "Alert", .kind = AK_KIND_ALERT, .event = shown_event, .drop = shown_drop};

static void el_open(ak_view *e) {
	alert_el *a = e->impl;
	if (a->shown || e->dead || !e->finalized) return;
	ak_ui_root *r = e->r;
	ak_view *v = ak_view_new(r, &cls_alert_shown, 0, true);
	alert_shown *s = ak_alloc(sizeof *s);
	s->el = e;
	v->impl = s;
	v->finalized = true;
	a->shown = v;  // the element holds it
	ak_batch *b = r->batch;
	ak_batch_set_int(b, v->id, AK_PROP_ALERT_WINDOW, a->win ? a->win : r->el->id);
	ak_str *title = el_str(e, AK_KEY_ALERT_TITLE), *msg = el_str(e, AK_KEY_ALERT_MESSAGE);
	if (title && title->len) ak_batch_set_string(b, v->id, AK_PROP_ALERT_TITLE, title->data, title->len);
	if (msg && msg->len) ak_batch_set_string(b, v->id, AK_PROP_ALERT_MESSAGE, msg->data, msg->len);
	const ak_val *sheet = key_of(e, AK_KEY_ALERT_ACTION_SHEET);
	bool action_sheet = sheet && sheet->i;
	if (action_sheet) ak_batch_set_enum(b, v->id, AK_PROP_ALERT_STYLE, AK_ALERT_STYLE_ACTION_SHEET);
	regather(e);
	el_send_buttons(e);
	if (a->fields.len && !action_sheet) {
		ak_alert_field *fs = ak_alloc(a->fields.len * sizeof *fs);
		for (uint32_t i = 0; i < a->fields.len; i++) {
			ak_view *x = a->fields.data[i];
			const ak_str *text = el_str(x, AK_KEY_TEXT_FIELD_TEXT), *ph = el_str(x, AK_KEY_TEXT_FIELD_PLACEHOLDER), *ct = el_str(x, AK_KEY_TEXT_FIELD_CONTENT_TYPE);
			const ak_val *sec = key_of(x, AK_KEY_TEXT_FIELD_SECURE), *kb = key_of(x, AK_KEY_TEXT_FIELD_KEYBOARD);
			fs[i] = (ak_alert_field){ph ? ak_strn(ph->data, ph->len) : ak_cstr(""), text ? ak_strn(text->data, text->len) : ak_cstr(""),
			                         sec && sec->i ? AK_ALERT_FIELD_SECURE : 0, kb ? (uint8_t)kb->i : 0, ct ? ak_strn(ct->data, ct->len) : ak_cstr("")};
		}
		ak_buf *out = ak_batch_set_bytes_begin(b, v->id, AK_PROP_ALERT_FIELDS);
		ak_append_alert_fields(out, fs, a->fields.len);
		ak_batch_set_bytes_end(b);
		ak_free(fs);
		ak_view_listen(v, AK_EVENT_FIELD_CHANGED);
	}
	ak_view_listen(v, AK_EVENT_ACTION);
	ak_view_listen(v, AK_EVENT_DISMISSED);
	hold(v);
	ak_batch_command(b, v->id, AK_CMD_PRESENT, NULL, 0);
}

static void el_cleanup(void *ctx) {
	ak_view *e = ctx;
	el_close(e, false);
	ak_release(e);
}

static void alert_el_init(ak_view *e) {
	alert_el *a = ak_alloc(sizeof *a);
	e->impl = a;
	a->win = ak_alert_window(e->r)->id;
	ak_on_cleanup(el_cleanup, ak_retain(e));
}

static void alert_el_drop(ak_view *e) {
	alert_el *a = e->impl;
	if (!a) return;
	release_views(&a->kids);
	release_views(&a->buttons);
	release_views(&a->fields);
	ak_vec_free(&a->kids);
	ak_vec_free(&a->buttons);
	ak_vec_free(&a->fields);
	ak_release(a->shown);
	ak_buf_free(&a->sent);
	ak_free(a);
}

static void alert_el_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	alert_el *a = e->impl;
	switch (key) {
	case AK_KEY_ALERT_OPEN:
		if (v.i) el_open(e);
		else el_close(e, false);
		break;
	case AK_KEY_ALERT_TITLE:
	case AK_KEY_ALERT_MESSAGE:
		if (a->shown) {
			const ak_str *t = v.p;
			ak_batch_set_string(e->r->batch, a->shown->id, key == AK_KEY_ALERT_TITLE ? AK_PROP_ALERT_TITLE : AK_PROP_ALERT_MESSAGE, t ? t->data : "", t ? t->len : 0);
		}
		break;
	}
}

static void alert_el_add(ak_view *e, ak_view *child) {
	alert_el *a = e->impl;
	ak_vec_push(&a->kids, (ak_view *)ak_retain(child));
	child->bar = e;
	ak_view_sync(e);
}

// alert_el_sync gathers its Buttons and TextFields again, and sends its
// buttons while it shows.
static void alert_el_sync(ak_view *e) {
	regather(e);
	el_send_buttons(e);
}

static void alert_el_destroy(ak_view *e) {
	alert_el *a = e->impl;
	el_close(e, false);
	for (uint32_t i = 0; i < a->kids.len; i++) ak_view_destroy(a->kids.data[i]);
}

static void alert_el_finalize(ak_view *e) {
	const ak_val *o = key_of(e, AK_KEY_ALERT_OPEN);
	if (o && o->i) el_open(e);
}

const ak_el_class ak_class_alert = {.name = "Alert", .kind = AK_KIND_VIEW, .flags = AK_ELF_FRAGMENT | AK_ELF_FLATTEN | AK_ELF_CHILDREN,
                                    .init = alert_el_init, .set = alert_el_set, .finalize = alert_el_finalize, .add = alert_el_add,
                                    .sync = alert_el_sync, .drop = alert_el_drop, .destroy = alert_el_destroy};

// ---- Snackbar ----

typedef struct {
	ak_view *win;  // the window it's on (not retained)
	ak_closure *run;        // its action's
	ak_closure *dismissed;  // OnDismiss
	ak_owner *owner;
	ak_owner *from;  // the code's that showed it (retained): its action runs there
	bool closed;
} snack_t;

static bool snack_close(ak_view *e) {
	snack_t *s = e->impl;
	if (s->closed) return false;
	s->closed = true;
	ak_retain(e);
	ak_owner *o = s->owner;
	s->owner = NULL;
	if (o) {
		ak_owner_dispose(o);
		ak_owner_release(o);
	}
	ak_view_destroy(e);
	unhold(e);
	ak_release(e);
	return true;
}

static void call_closure(ak_closure *c, ak_owner *owner) {
	if (!c) return;
	ak_retain(c);
	ak_val r = ak_ui_call_under(owner, c, NULL, 0);
	if (!ak_error_pending()) ak_release(r.p);
	else ak_ui_raise_pending(owner);
	ak_release(c);
}

// snack_dismiss closes it and runs its OnDismiss, if it was showing.
static void snack_dismiss(ak_view *e) {
	snack_t *s = e->impl;
	ak_retain(e);
	ak_closure *d = s->dismissed ? ak_retain(s->dismissed) : NULL;
	if (snack_close(e)) call_closure(d, s->from);
	ak_release(d);
	ak_release(e);
}

static void snack_event(ak_view *e, const ak_event *ev) {
	snack_t *s = e->impl;
	if (ev->kind == AK_EVENT_ACTION) {
		ak_retain(e);
		ak_closure *run = s->run ? ak_retain(s->run) : NULL;
		if (snack_close(e)) call_closure(run, s->from);
		ak_release(run);
		ak_release(e);
	} else if (ev->kind == AK_EVENT_DISMISSED) {
		snack_dismiss(e);
	}
}

static void snack_drop(ak_view *e) {
	snack_t *s = e->impl;
	if (!s) return;
	ak_release(s->run);
	ak_release(s->dismissed);
	if (s->from) ak_owner_release(s->from);
	ak_free(s);
}

static const ak_el_class cls_snack = {.name = "Snackbar", .kind = AK_KIND_SNACKBAR, .event = snack_event, .drop = snack_drop};

static void snack_cleanup(void *ctx) { snack_close(ctx); }

static void snack_owned(void *ctx) { ak_on_cleanup(snack_cleanup, ctx); }

static ak_val snack_dismiss_fn(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	snack_dismiss(c->caps[0].p);
	return (ak_val){0};
}

static const uint8_t ref_cap[] = {AK_K_REF};

// ak_ui_snackbar shows a brief message at the bottom of the window now,
// with an optional action (an Action's title and run), and returns a
// closure that dismisses it. A new one replaces the one showing on its
// window, whose OnDismiss runs.
ak_closure *ak_ui_snackbar(ak_str *message, const ak_enum *action, int64_t duration, ak_closure *on_dismiss) {
	ak_ui_root *r = ak_ui_current();
	ak_view *win = ak_alert_window(r);
	for (bool again = true; again;) {
		again = false;
		for (uint32_t i = 0; i < r->shown.len; i++) {
			ak_view *o = r->shown.data[i];
			if (o->cls == &cls_snack && ((snack_t *)o->impl)->win == win && !((snack_t *)o->impl)->closed) {
				snack_dismiss(o);  // its OnDismiss might show another
				again = true;
				break;
			}
		}
	}
	ak_view *e = ak_view_new(r, &cls_snack, 0, true);
	snack_t *s = ak_alloc(sizeof *s);
	e->impl = s;
	e->finalized = true;
	s->win = win;
	s->dismissed = on_dismiss ? ak_retain(on_dismiss) : NULL;
	s->from = ak_current_owner();
	if (s->from) ak_owner_retain(s->from);
	ak_batch *b = r->batch;
	ak_batch_set_int(b, e->id, AK_PROP_SNACKBAR_WINDOW, win->id);
	if (message && message->len) ak_batch_set_string(b, e->id, AK_PROP_SNACKBAR_MESSAGE, message->data, message->len);
	if (duration != AK_SNACKBAR_DURATION_SHORT) ak_batch_set_enum(b, e->id, AK_PROP_SNACKBAR_DURATION, (int32_t)duration);
	if (action) {
		const ak_str *t = action->f[0].p;
		if (t && t->len) ak_batch_set_string(b, e->id, AK_PROP_SNACKBAR_ACTION, t->data, t->len);
		if (ak_enum_case(action) == 0 && action->f[5].p) s->run = ak_retain(action->f[5].p);
	}
	ak_view_listen(e, AK_EVENT_ACTION);
	ak_view_listen(e, AK_EVENT_DISMISSED);
	hold(e);
	ak_with_owner(r->owner, new_child, &s->owner);  // goes with its root
	ak_with_owner(s->owner, snack_owned, e);
	ak_batch_command(b, e->id, AK_CMD_PRESENT, NULL, 0);
	ak_closure *c = ak_closure_new(snack_dismiss_fn, NULL, 1, ref_cap);
	c->caps[0].p = e;  // the closure holds it
	return c;
}
