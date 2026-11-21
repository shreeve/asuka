// nav.c: navigation: providers, the
// host-sized layout roots with an environment of their own (screens,
// tabs' pages, sheets, popovers, windows); going back (OnBack); Stack and
// Screen, push and pop; a Screen's bars (its actions, toolbar and primary
// slots: the Buttons in them, sent as action menus); Tabs; Split; and
// PageControl.
//
// A pushed screen is built under an owner of its own, a root under the
// stack's creator, where the stack (push, pop), the screen's environment
// and its going back are contexts: they exist before the Screen element
// a screen declaration makes, which adopts them as it's made.
#include "ui/ui_int.h"

const char ak_env_key[] = "env", ak_back_key[] = "back", ak_nav_key[] = "nav", ak_sheet_key[] = "sheet", ak_window_key[] = "window",
           ak_router_key[] = "router";

static void call0(ak_view *e, uint16_t ev) { ak_view_call(e, ev, NULL, 0); }

static void send_str(ak_view *e, ak_prop p, ak_val v) {
	const ak_str *s = v.p;
	ak_batch_set_string(e->r->batch, e->id, p, s ? s->data : "", s ? s->len : 0);
}

static const ak_val *key_of(const ak_view *e, uint16_t key) { return ak_style_get(e, key); }

static int64_t key_int(const ak_view *e, uint16_t key, int64_t def) {
	const ak_val *v = ak_style_get(e, key);
	return v ? v->i : def;
}

// grow_adjust gives a container that fills its space (Stack, Tabs,
// Split) grow 1 unless its style sizes it.
static void grow_adjust(ak_view *e, ak_lstyle *ls) {
	AK_UNUSED(e);
	if (ak_llength_is_auto(ls->height) && ls->flex_grow == 0) ls->flex_grow = 1;
}

// ---- Building under an owner ----

typedef struct {
	ak_ui_root *r;
	ak_closure *build;
	ak_val *args;
	uint32_t nargs;
	ak_view *out;
} under_ctx;

static void build_fn(void *ctx) {
	under_ctx *u = ctx;
	u->out = ak_ui_build(u->r, u->build, u->args, u->nargs);
}

ak_view *ak_build_under(ak_ui_root *r, ak_owner *o, ak_closure *build, ak_val *args, uint32_t nargs) {
	under_ctx u = {r, build, args, nargs, NULL};
	if (!build) return NULL;
	ak_with_owner(o, build_fn, &u);
	return u.out;
}

ak_view *ak_only_kid(ak_ui_root *r, ak_view *v) {
	v->fill = true;
	if (!v->frag) ak_style_relayout(v);
	if (v->real && !v->frag) return v;
	ak_view *w = ak_view_new(r, &ak_class_view, 0, true);
	w->fill = true;
	ak_style_relayout(w);
	ak_view_append(w, v);
	ak_release(v);
	return w;
}

static void fill(ak_view *v) {
	v->fill = true;
	if (!v->frag) ak_style_relayout(v);
}

// ---- Providers ----

static void prov_drop(ak_opaque *o) {
	ak_prov *p = (ak_prov *)o;
	for (int i = 0; i < 3; i++) ak_release(p->strs[i]);
	ak_release(p->follows);
	ak_signal_drop(&p->node);
}

ak_prov *ak_prov_new(ak_ui_root *r) {
	ak_prov *p = ak_opaque_new(AK_TAG_OPAQUE, sizeof(ak_prov), prov_drop);
	ak_ui_env_strings(&p->env, p->strs, &r->env);
	return p;
}

bool ak_fit_sheet_platform(uint8_t platform) { return platform == AK_PLATFORM_MACOS || platform == AK_PLATFORM_WEB; }

ak_prov *ak_prov_current(void) {
	void *v = NULL;
	return ak_context_lookup(ak_env_key, &v) ? v : NULL;
}

void ak_prov_attach(ak_view *v, ak_prov *p) {
	ak_retain(p);
	ak_release(v->prov);
	v->prov = p;
	p->view = v;
	v->host_sized = true;
	ak_vec_push(&v->r->provs, (ak_view *)ak_retain(v));
	ak_style_relayout(v);
}

// prov_adjust lays a provider out as the root is: a column filling the
// size the host gives it; one sized to its content has no size of its
// own.
static void prov_adjust(ak_view *e, ak_lstyle *ls) {
	ls->flex_direction = AK_LCOLUMN;
	ak_prov *p = e->prov;
	if (p && (p->fits || p->fit_win || p->fit_sheet)) return;  // the layout sizes it
	ls->width = ak_lpct(100), ls->height = ak_lpct(100);
}

const ak_environment *ak_view_env(ak_view *e) {
	for (ak_view *x = e; x; x = x->parent ? x->parent : x->up) {
		if (x->prov) {
			ak_signal_read(&x->prov->node);
			return &x->prov->env;
		}
	}
	ak_signal_read(&e->r->env_node);
	return &e->r->env;
}

static bool env_same(const ak_environment *a, const ak_environment *b) {
	return a->width == b->width && a->height == b->height && a->safe_top == b->safe_top && a->safe_right == b->safe_right &&
	       a->safe_bottom == b->safe_bottom && a->safe_left == b->safe_left && a->keyboard == b->keyboard && a->scale == b->scale &&
	       a->font_scale == b->font_scale && a->dark == b->dark && a->reduce_motion == b->reduce_motion &&
	       a->screen_reader == b->screen_reader && a->platform == b->platform && a->width_class == b->width_class &&
	       a->height_class == b->height_class && ak_span_eq(a->locale, b->locale) && ak_span_eq(a->time_zone, b->time_zone) &&
	       ak_span_eq(a->languages, b->languages) && a->first_weekday == b->first_weekday && a->hour_cycle == b->hour_cycle;
}

// prov_take sets p's environment to env: what reads it follows, and it's
// laid out again. False when it's the same.
static bool prov_take(ak_prov *p, const ak_environment *env) {
	if (env_same(env, &p->env)) return false;
	float old = p->env.scale;
	ak_ui_env_strings(&p->env, p->strs, env);
	ak_signal_write(&p->node);
	if (p->view) {
		ak_lnode_mark_dirty(p->view->box);
		if (env->scale > 0 && env->scale != old) ak_view_mark_measured(p->view);
	}
	return true;
}

// followers_take passes p's environment on to the providers that follow
// it, and theirs.
static void followers_take(ak_ui_root *r, ak_prov *p) {
	for (uint32_t i = 0; i < r->provs.len; i++) {
		ak_view *v = r->provs.data[i];
		ak_prov *f = v->prov;
		if (v->dead || !f || f->follows != p) continue;
		if (prov_take(f, &p->env)) followers_take(r, f);
	}
}

void ak_prov_follow(ak_prov *p, ak_prov *of) {
	ak_retain(of);
	ak_release(p->follows);
	p->follows = of;
	prov_take(p, &of->env);
}

bool ak_prov_report(ak_id id, const ak_environment *in) {
	ak_view *v = ak_view_of(id);
	if (!v || v->dead || !v->host_sized || !v->prov) return false;
	ak_prov *p = v->prov;
	ak_environment env = *in;
	if (env.width_class == AK_SIZE_CLASS_UNSPECIFIED) env.width_class = ak_width_class_of(env.width);
	if (env.height_class == AK_SIZE_CLASS_UNSPECIFIED) env.height_class = ak_height_class_of(env.height);
	if (p->fit_wait) {
		// A sheet asking to be as tall as its content is one where the
		// platform its first environment names has such a sheet.
		bool fit = ak_fit_sheet_platform(env.platform);
		if (fit != p->fit_sheet) {
			p->fit_sheet = fit;
			ak_style_relayout(v);
		}
	}
	p->fit_wait = false;  // its own room, which a fit sheet waits for
	if (p->follows) {
		// The host sizes it itself: it no longer follows.
		ak_release(p->follows);
		p->follows = NULL;
	}
	if (!prov_take(p, &env)) {
		ak_view_remeasure(v);  // laid out (a fit sheet's first room)
		return true;
	}
	followers_take(v->r, p);
	return true;
}

// layout_fit lays a content-sized root out at its content's size (a
// popover's, no wider than it may be nor taller than most of the root;
// an accessory's, as wide as the root), and tells the host its size when
// it changes.
static void layout_fit(ak_ui_root *r, ak_view *e, ak_lpass *lp) {
	ak_prov *p = e->prov;
	ak_lstyle *st = ak_lnode_style(e->box);
	if (p->full_width) {
		float w = r->env.width;
		if (w <= 0 || (!ak_lnode_dirty(e->box) && p->laid.w == w)) return;
		if (st->width.u != AK_LUNIT_POINT || st->width.v != w) {
			ak_lstyle ns = *st;
			ns.width = ak_lpt(w);
			ak_lnode_set_style(e->box, &ns);
		}
		ak_lsize s = ak_lnode_layout(e->box, (ak_lsize){w, AK_LMAX_CONTENT}, lp);
		if (s.w != p->laid.w || s.h != p->laid.h) {
			p->laid = s;
			ak_batch_set_float(r->batch, e->id, AK_PROP_ACCESSORY_WIDTH, s.w);
			ak_batch_set_float(r->batch, e->id, AK_PROP_ACCESSORY_HEIGHT, s.h);
		}
		return;
	}
	if (!ak_lnode_dirty(e->box) && p->laid.w > 0) return;
	ak_llength mh = r->env.height > 0 ? ak_lpt(r->env.height * 0.8f) : AK_LAUTO;
	if (st->max_height.u != mh.u || st->max_height.v != mh.v) {
		ak_lstyle ns = *st;
		ns.max_height = mh;
		ak_lnode_set_style(e->box, &ns);
	}
	// Its content's natural width; too wide, laid out again at the widest
	// it may be, which its text wraps to.
	ak_lsize s = ak_lnode_layout(e->box, (ak_lsize){AK_LMAX_CONTENT, AK_LMAX_CONTENT}, lp);
	if (s.w > p->max_w) s = ak_lnode_layout(e->box, (ak_lsize){p->max_w, AK_LMAX_CONTENT}, lp);
	if (s.w != p->laid.w || s.h != p->laid.h) {
		p->laid = s;
		ak_batch_set_float(r->batch, e->id, AK_PROP_POPOVER_WIDTH, s.w);
		ak_batch_set_float(r->batch, e->id, AK_PROP_POPOVER_HEIGHT, s.h);
	}
}

static void send_fit(ak_ui_root *r, ak_view *e, ak_lsize s) {
	uint8_t args[9];
	args[0] = AK_WINDOW_ACTION_FIT;
	memcpy(args + 1, &s.w, 4);
	memcpy(args + 5, &s.h, 4);
	ak_batch_command(r->batch, e->id, AK_CMD_WINDOW_ACTION, args, sizeof args);
}

// fit_sheet_style gives a sheet's root its width (auto: its content's)
// and its height: its content's, up to most.
static void fit_sheet_style(ak_view *e, ak_llength width, float most) {
	ak_lstyle *st = ak_lnode_style(e->box);
	if (st->width.u != width.u || st->width.v != width.v || !ak_llength_is_auto(st->height) ||
	    st->max_height.u != AK_LUNIT_POINT || st->max_height.v != most) {
		ak_lstyle ns = *st;
		ns.width = width, ns.height = AK_LAUTO, ns.max_height = ak_lpt(most);
		ak_lnode_set_style(e->box, &ns);
	}
}

// fit_sheet_overflows says whether anything under n, as last laid out,
// reaches past right, x being n's left in the root's coordinates.
static bool fit_sheet_overflows(ak_lnode *n, float x, float right) {
	uint32_t k;
	ak_lnode *const *c = ak_lnode_children(n, &k);
	for (uint32_t i = 0; i < k; i++) {
		ak_lframe f = ak_lnode_frame(c[i]);
		if (x + f.x + f.w > right + 0.5f || fit_sheet_overflows(c[i], x + f.x, right)) return true;
	}
	return false;
}

// layout_fit_sheet lays a sheet as tall as its content out at the width
// its environment gives, and its content's height there, up to the
// environment's height, and sends WindowAction Fit as that changes. Only
// content that reaches past that width (a fixed width) is laid out at its
// narrowest, to widen the sheet to it: text, which can always wrap, isn't
// measured at its narrowest otherwise.
static void layout_fit_sheet(ak_ui_root *r, ak_view *e, ak_lpass *lp) {
	ak_prov *p = e->prov;
	ak_lsize room = {p->env.width, p->env.height};
	if (p->fit_wait || room.w <= 0 || room.h <= 0 ||
	    (!ak_lnode_dirty(e->box) && p->fit_sent && room.w == p->room.w && room.h == p->room.h))
		return;
	fit_sheet_style(e, ak_lpt(room.w), room.h);
	ak_lsize s = ak_lnode_layout(e->box, (ak_lsize){room.w, AK_LMAX_CONTENT}, lp);
	if (fit_sheet_overflows(e->box, 0, s.w)) {
		fit_sheet_style(e, AK_LAUTO, room.h);
		float w = ak_lnode_layout(e->box, (ak_lsize){AK_LMIN_CONTENT, AK_LMAX_CONTENT}, lp).w;
		if (!(w > room.w)) w = room.w;
		fit_sheet_style(e, ak_lpt(w), room.h);
		s = ak_lnode_layout(e->box, (ak_lsize){w, AK_LMAX_CONTENT}, lp);
	}
	p->room = room;
	if (p->fit_sent && s.w == p->laid.w && s.h == p->laid.h) return;
	p->laid = s, p->fit_sent = true;
	send_fit(r, e, s);
	if (p->fit_once) {
		// A resizable movable sheet: as big as its content as it opens,
		// and after that as big as the user makes it, its content filling
		// it. One that isn't resizable follows its content, as any fit
		// sheet.
		p->fit_once = p->fit_sheet = false;
		ak_lstyle ns = *ak_lnode_style(e->box);
		ns.width = ak_lpct(100), ns.height = ak_lpct(100), ns.max_height = AK_LAUTO;
		ak_lnode_set_style(e->box, &ns);
	}
}

// layout_fit_window lays a window sized to its content out at its
// content's size, within its least and most size.
static void layout_fit_window(ak_ui_root *r, ak_view *e, ak_lpass *lp) {
	ak_prov *p = e->prov;
	if (!ak_lnode_dirty(e->box) && p->fit_sent) return;
	ak_lsize s = ak_lnode_layout(e->box, (ak_lsize){AK_LMAX_CONTENT, AK_LMAX_CONTENT}, lp);
	const ak_lstyle *st = ak_lnode_style(e->box);
	if (st->max_width.u == AK_LUNIT_POINT && s.w >= st->max_width.v)
		s = ak_lnode_layout(e->box, (ak_lsize){st->max_width.v, AK_LMAX_CONTENT}, lp);
	if (p->fit_sent && s.w == p->laid.w && s.h == p->laid.h) return;
	p->laid = s, p->fit_sent = true;
	send_fit(r, e, s);
}

void ak_root_layout_provs(ak_ui_root *r, ak_lpass *lp) {
	uint32_t live = 0;
	for (uint32_t i = 0; i < r->provs.len; i++) {
		ak_view *e = r->provs.data[i];
		if (e->dead) {
			ak_release(e);
			continue;
		}
		r->provs.data[live++] = e;
		ak_prov *p = e->prov;
		if (p->fit_win) layout_fit_window(r, e, lp);
		else if (p->fit_sheet) layout_fit_sheet(r, e, lp);
		else if (p->fits) layout_fit(r, e, lp);
		else {
			ak_lsize size = {p->env.width, p->env.height};
			if (size.w > 0 && size.h > 0 && (ak_lnode_dirty(e->box) || size.w != p->laid.w || size.h != p->laid.h)) {
				p->laid = size;
				ak_lnode_layout(e->box, size, lp);
			}
		}
	}
	r->provs.len = live;
	ak_overlays_layout_hook(r, lp);  // after the roots their anchors are in
}

// ---- Syncing at the flush ----

void ak_view_sync(ak_view *e) {
	if (e->sync_q || e->dead) return;
	e->sync_q = true;
	ak_vec_push(&e->r->sync_q, (ak_view *)ak_retain(e));
}

void ak_root_sync(ak_ui_root *r) {
	for (uint32_t i = 0; i < r->sync_q.len; i++) {
		ak_view *e = r->sync_q.data[i];
		e->sync_q = false;
		if (!e->dead && e->cls->sync) e->cls->sync(e);
		ak_release(e);
	}
	r->sync_q.len = 0;
}

void ak_bar_dirty(ak_view *v) {
	if (v->bar && !v->bar->dead) ak_view_sync(v->bar);
}

// ---- Going back ----

struct ak_back {
	ak_opaque h;
	ak_view *e;  // not retained: the view its Back events come to (NULL: none yet, or gone)
	AK_VEC(ak_closure *) fns;  // OnBack's, in the order they came (retained)
	void (*fallback)(void *ctx);  // what going back does when no handler takes it (NULL: the host leaves the app)
	void *ctx;
	bool listens;
};

static void back_drop(ak_opaque *o) {
	ak_back *b = (ak_back *)o;
	for (uint32_t i = 0; i < b->fns.len; i++) ak_release(b->fns.data[i]);
	ak_vec_free(&b->fns);
}

ak_back *ak_back_new(void (*fallback)(void *ctx), void *ctx) {
	ak_back *b = ak_opaque_new(AK_TAG_OPAQUE, sizeof(ak_back), back_drop);
	b->fallback = fallback;
	b->ctx = ctx;
	return b;
}

void ak_back_release(ak_back *b) {
	if (!b) return;
	b->e = NULL;
	b->fallback = NULL;
	ak_release(b);
}

static void back_intercept(ak_back *b, bool on) {
	if (b->e && !b->e->dead) ak_batch_set_bool(b->e->r->batch, b->e->id, AK_PROP_INTERCEPT_BACK, on);
}

void ak_back_attach(ak_back *b, ak_view *e) {
	b->e = e;
	if (b->fns.len) {
		b->listens = true;
		ak_view_listen(e, AK_EVENT_BACK);
		back_intercept(b, true);
	}
}

typedef struct {
	ak_back *b;  // retained
	ak_closure *c;
} back_reg;

static void back_remove(void *ctx) {
	back_reg *g = ctx;
	ak_back *b = g->b;
	for (uint32_t i = 0; i < b->fns.len; i++) {
		if (b->fns.data[i] == g->c) {
			memmove(b->fns.data + i, b->fns.data + i + 1, (b->fns.len - i - 1) * sizeof(ak_closure *));
			b->fns.len--;
			ak_release(g->c);
			break;
		}
	}
	if (b->fns.len == 0) back_intercept(b, false);
	ak_release(b);
	ak_free(g);
}

// ak_ui_on_back takes the user's going back from where the component is
// (OnBack): a pushed screen, a sheet or popover, or the app (the root).
// handler returns true when it took it.
void ak_ui_on_back(ak_closure *handler) {
	void *v = NULL;
	ak_back *b = NULL;
	if (ak_context_lookup(ak_back_key, &v)) b = v;
	if (!b) {
		ak_ui_root *r = ak_ui_current();
		if (!r->back) {
			r->back = ak_back_new(NULL, NULL);
			ak_back_attach(r->back, r->el);
		}
		b = r->back;
	}
	ak_vec_push(&b->fns, (ak_closure *)ak_retain(handler));
	if (b->e && !b->listens) {
		b->listens = true;
		ak_view_listen(b->e, AK_EVENT_BACK);
	}
	if (b->fns.len == 1) back_intercept(b, true);
	back_reg *g = ak_alloc(sizeof *g);
	g->b = ak_retain(b);
	g->c = handler;
	ak_on_cleanup(back_remove, g);
}

// ak_back_run is the host's Back: the handlers, latest first, then the
// default.
void ak_back_run(ak_back *b) {
	ak_retain(b);
	for (uint32_t i = b->fns.len; i-- > 0;) {
		if (i >= b->fns.len) continue;  // a handler removed others
		ak_closure *c = ak_retain(b->fns.data[i]);
		ak_val took = ak_ui_call_under(b->e ? b->e->owner : NULL, c, NULL, 0);
		ak_release(c);
		if (ak_error_pending()) {
			ak_ui_raise_pending(b->e ? b->e->owner : NULL);
			ak_release(b);
			return;
		}
		if (took.i) {
			ak_release(b);
			return;
		}
	}
	if (b->e && !b->e->dead) {
		if (b->fallback) b->fallback(b->ctx);
		else ak_batch_command(b->e->r->batch, b->e->id, AK_CMD_BACK_EXIT, NULL, 0);
	}
	ak_release(b);
}

// ---- Stack ----

typedef struct {
	ak_view *screen;  // retained
	ak_owner *owner;  // its content's, disposed as it's popped
} entry;

typedef struct {
	AK_VEC(entry) screens;
	ak_owner *owner;  // what made it (retained): its screens' owners' parent
	ak_closure *first;  // its first screen's builder, until it's built
	void (*popped)(void *ctx, ak_view *screen);  // a Router hears of the screens it lost
	void *popped_ctx;
	void (*pushed)(void *ctx, ak_view *screen);  // and of those pushed
	void *pushed_ctx;
	uint32_t building;  // screens being built, not yet pushed: depth() counts them
	bool disposed;
} stack_t;

static const ak_el_class cls_screen;

static void stack_remove(ak_view *stack, uint32_t i) {
	stack_t *st = stack->impl;
	entry en = st->screens.data[i];
	memmove(st->screens.data + i, st->screens.data + i + 1, (st->screens.len - i - 1) * sizeof(entry));
	st->screens.len--;
	ak_owner_dispose(en.owner);
	if (en.screen->parent == stack) ak_view_remove(stack, en.screen, true);
	else ak_view_destroy(en.screen);
	if (st->popped) st->popped(st->popped_ctx, en.screen);
	ak_release(en.screen);
}

// Its screens go with what made it.
static void stack_dispose(void *ctx) {
	ak_view *e = ctx;
	stack_t *st = e->impl;
	st->disposed = true;
	while (st->screens.len) stack_remove(e, st->screens.len - 1);
	ak_release(e);
}

static void stack_init(ak_view *e) {
	stack_t *st = ak_alloc(sizeof *st);
	e->impl = st;
	st->owner = ak_current_owner();
	if (st->owner) ak_owner_retain(st->owner);
	ak_view_listen(e, AK_EVENT_POPPED);
	ak_on_cleanup(stack_dispose, ak_retain(e));
}

static void stack_drop(ak_view *e) {
	stack_t *st = e->impl;
	if (!st) return;
	for (uint32_t i = 0; i < st->screens.len; i++) {
		ak_owner_dispose(st->screens.data[i].owner);
		ak_release(st->screens.data[i].screen);
	}
	ak_vec_free(&st->screens);
	ak_release(st->first);
	if (st->owner) ak_owner_release(st->owner);
	ak_free(st);
}

static void stack_event(ak_view *e, const ak_event *ev) {
	stack_t *st = e->impl;
	if (ev->kind != AK_EVENT_POPPED) return;
	uint32_t n = st->screens.len;
	if (n > 1 && st->screens.data[n - 1].screen->id == (ak_id)ev->i) stack_remove(e, n - 1);
}

static void screen_back(void *ctx) {
	ak_view *screen = ctx;
	ak_view *stack = screen->parent;
	if (!stack || stack->el != AK_EL_STACK) return;
	stack_t *st = stack->impl;
	for (uint32_t i = 0; i < st->screens.len; i++)
		if (st->screens.data[i].screen == screen) {
			stack_remove(stack, i);
			return;
		}
}

typedef struct {
	ak_ui_root *r;
	ak_view *out;
} new_screen_ctx;

static void new_screen_fn(void *ctx) {
	new_screen_ctx *c = ctx;
	c->out = ak_view_new(c->r, &cls_screen, AK_EL_SCREEN, true);
	ak_view_finalize(c->out);
}

ak_view *ak_stack_push(ak_view *stack, ak_closure *build, ak_val *args, uint32_t nargs, ak_owner *o) {
	stack_t *st = stack->impl;
	ak_ui_root *r = stack->r;
	if (st->disposed || stack->dead) return NULL;
	bool first = st->screens.len == 0;
	// Its owner, with the contexts the screen's content finds: the stack,
	// its environment, and (above the first) its going back, made before
	// the Screen element that adopts them.
	ak_owner *so = ak_root_under(o ? o : st->owner);
	ak_prov *pv = ak_prov_new(r);
	ak_back *bk = first ? NULL : ak_back_new(screen_back, NULL);
	ak_owner_set_context(so, ak_nav_key, stack);
	ak_owner_set_context(so, ak_env_key, pv);
	if (bk) ak_owner_set_context(so, ak_back_key, bk);
	st->building++;
	ak_view *v = ak_build_under(r, so, build, args, nargs);
	st->building--;
	ak_view *screen = NULL;
	if (v && v->el == AK_EL_SCREEN && v->prov == pv) {
		screen = v;  // a screen declaration's: it adopted them
	} else {
		new_screen_ctx c = {r, NULL};
		ak_with_owner(so, new_screen_fn, &c);
		screen = c.out;
		if (v) {
			fill(v);
			ak_view_append(screen, v);
			ak_release(v);
		}
	}
	ak_release(pv);
	if (bk) {
		bk->ctx = screen;
		ak_release(bk);  // the screen holds it
	}
	ak_vec_push(&st->screens, ((entry){(ak_view *)ak_retain(screen), so}));
	ak_view_append(stack, screen);  // the host pushes it
	ak_release(screen);
	if (st->pushed) st->pushed(st->pushed_ctx, screen);
	return screen;
}

static void stack_lazy(ak_view *e, ak_closure *build) {
	stack_t *st = e->impl;
	if (st->screens.len) ak_panic("ui: a Stack has one first screen");
	ak_stack_push(e, build, NULL, 0, NULL);
}

// A child added (by C code, which builds it first) is the first screen.
static ak_val built(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.p = ak_retain(c->caps[0].p)};
}

static const uint8_t ref_cap[] = {AK_K_REF};

static void stack_add(ak_view *e, ak_view *child) {
	ak_closure *c = ak_closure_new(built, NULL, 1, ref_cap);
	c->caps[0].p = ak_retain(child);
	stack_lazy(e, c);
	ak_release(c);
}

ak_view *ak_stack_new(ak_ui_root *r) {
	extern const ak_el_class ak_class_stack;
	return ak_view_new(r, &ak_class_stack, AK_EL_STACK, true);
}

uint32_t ak_stack_depth(ak_view *stack) { return ((stack_t *)stack->impl)->screens.len; }

ak_view *ak_stack_screen(ak_view *stack, uint32_t i) {
	stack_t *st = stack->impl;
	return i < st->screens.len ? st->screens.data[i].screen : NULL;
}

void ak_stack_pop_to(ak_view *stack, uint32_t depth) {
	stack_t *st = stack->impl;
	while (st->screens.len > depth) stack_remove(stack, st->screens.len - 1);
}

// ak_stack_replace empties the stack for a screen that replaces them at
// once, with no pop or push (a Split's destination changing).
void ak_stack_replace(ak_view *stack) {
	ak_batch_command(stack->r->batch, stack->id, AK_CMD_REPLACE, NULL, 0);
	ak_stack_pop_to(stack, 0);
}

void ak_stack_remove_at(ak_view *stack, uint32_t i) {
	if (i < ((stack_t *)stack->impl)->screens.len) stack_remove(stack, i);
}

void ak_stack_on_push(ak_view *stack, void (*fn)(void *ctx, ak_view *screen), void *ctx) {
	stack_t *st = stack->impl;
	st->pushed = fn;
	st->pushed_ctx = ctx;
}

void ak_stack_on_pop(ak_view *stack, void (*fn)(void *ctx, ak_view *screen), void *ctx) {
	stack_t *st = stack->impl;
	st->popped = fn;
	st->popped_ctx = ctx;
}

const ak_el_class ak_class_stack = {.name = "Stack", .kind = AK_KIND_STACK, .flags = AK_ELF_CHILDREN, .init = stack_init, .event = stack_event,
                                    .adjust = grow_adjust, .add = stack_add, .lazy = stack_lazy, .drop = stack_drop};

static ak_view *current_stack(void) {
	void *v = NULL;
	return ak_context_lookup(ak_nav_key, &v) ? v : NULL;
}

void ak_ui_push(ak_closure *build) {
	ak_view *stack = current_stack();
	if (!stack) {
		ak_throw_msg("push: not in a Stack");
		return;
	}
	ak_retain(stack);
	ak_stack_push(stack, build, NULL, 0, NULL);
	ak_release(stack);
}

void ak_ui_pop(void) {
	ak_view *stack = current_stack();
	if (!stack) {
		ak_throw_msg("pop: not in a Stack");
		return;
	}
	stack_t *st = stack->impl;
	if (st->screens.len > 1) stack_remove(stack, st->screens.len - 1);
}

// ak_ui_depth is how many screens the nearest stack holds, with the one
// being built (a screen's own reads it at its place).
int64_t ak_ui_depth(void) {
	ak_view *stack = current_stack();
	if (!stack) return 0;
	stack_t *st = stack->impl;
	return st->screens.len + st->building;
}

// ---- Screen ----

enum { BAR_ACTIONS, BAR_TOOLBAR, BAR_PRIMARY, NBARS };

static const uint16_t bar_keys[NBARS] = {AK_KEY_SCREEN_TOOLBAR, AK_KEY_SCREEN_BOTTOM_TOOLBAR, AK_KEY_SCREEN_PRIMARY};
static const ak_prop bar_props[NBARS] = {AK_PROP_SCREEN_ACTIONS, AK_PROP_SCREEN_TOOLBAR, AK_PROP_SCREEN_PRIMARY};

typedef struct {
	ak_back *back;  // retained: its going back (NULL: a stack's first screen, or a screen of no stack)
	AK_VEC(ak_view *) items[NBARS];  // each bar's Buttons, as last sent (retained)
	ak_buf sent[NBARS], buf;
	bool has[NBARS];
	ak_str *search;  // the search field's text, as last sent or reported
	bool search_known;
	bool listens_action;
} screen_t;

static void screen_init(ak_view *e) {
	screen_t *s = ak_alloc(sizeof *s);
	e->impl = s;
	// The environment and going back made for it (a pushed screen's), or
	// its own.
	void *v = NULL;
	ak_prov *pv = ak_context_lookup(ak_env_key, &v) ? v : NULL;
	if (pv && !pv->view) {
		ak_prov_attach(e, pv);
	} else {
		// Its own; in a sheet (or a window) that already has one, it's as
		// big as that, as its environment says, until the host says
		// otherwise.
		ak_prov *own = ak_prov_new(e->r);
		if (pv) ak_prov_follow(own, pv);
		ak_prov_attach(e, own);
		ak_release(own);
	}
	v = NULL;
	ak_back *bk = ak_context_lookup(ak_back_key, &v) ? v : NULL;
	if (bk && !bk->e && bk->fallback == screen_back) {
		s->back = ak_retain(bk);
		ak_back_attach(bk, e);
	}
}

static void unbar(ak_view *v) {
	if (!v || ((ak_obj *)v)->tag != AK_TAG_VIEW) return;
	v->bar = NULL;
	for (uint32_t i = 0; i < v->kids.len; i++) unbar(v->kids.data[i]);
}

static void screen_drop(ak_view *e) {
	screen_t *s = e->impl;
	if (!s) return;
	for (int k = 0; k < NBARS; k++) {
		const ak_val *v = key_of(e, bar_keys[k]);
		if (v) unbar(v->p);
		for (uint32_t i = 0; i < s->items[k].len; i++) ak_release(s->items[k].data[i]);
		ak_vec_free(&s->items[k]);
		ak_buf_free(&s->sent[k]);
	}
	ak_buf_free(&s->buf);
	ak_back_release(s->back);
	ak_release(s->search);
	ak_free(s);
}

void ak_screen_set_title(ak_view *screen, ak_str *title) {
	if (screen->el != AK_EL_SCREEN || key_of(screen, AK_KEY_SCREEN_TITLE)) return;
	ak_ui_set(screen, AK_KEY_SCREEN_TITLE, (ak_val){.p = title});
}

static void screen_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	screen_t *s = e->impl;
	ak_batch *b = e->r->batch;
	switch (key) {
	case AK_KEY_SCREEN_TITLE: send_str(e, AK_PROP_SCREEN_TITLE, v); break;
	case AK_KEY_SCREEN_LARGE_TITLE:
		if (prev || v.i) ak_batch_set_bool(b, e->id, AK_PROP_SCREEN_LARGE_TITLE, v.i != 0);
		break;
	case AK_KEY_SCREEN_BACK_GESTURE:
		if (prev || !v.i) ak_batch_set_bool(b, e->id, AK_PROP_SCREEN_BACK_GESTURE, v.i != 0);
		break;
	case AK_KEY_SCREEN_ZOOM_FROM: send_str(e, AK_PROP_SCREEN_ZOOM_FROM, v); break;
	case AK_KEY_SCREEN_BAR_COLOR:
		if (prev || v.i) ak_batch_set_color(b, e->id, AK_PROP_SCREEN_BAR_COLOR, (ak_color)v.i);
		break;
	case AK_KEY_SCREEN_SEARCH: {
		// Typing never round trips: sent only when it isn't what the field
		// shows.
		ak_str *t = v.p;
		if (s->search_known && ak_obj_eq((const ak_obj *)t, (const ak_obj *)s->search)) break;
		ak_retain(t);
		ak_release(s->search);
		s->search = t;
		s->search_known = true;
		send_str(e, AK_PROP_SCREEN_SEARCH_TEXT, v);
		break;
	}
	case AK_KEY_SCREEN_SEARCH_PLACEHOLDER:
		if (e->finalized) send_str(e, AK_PROP_SCREEN_SEARCH, v);
		break;
	case AK_KEY_SCREEN_SEARCH_BUTTON:
		if (prev || v.i) ak_batch_set_bool(b, e->id, AK_PROP_SCREEN_SEARCH_BUTTON, v.i != 0);
		break;
	case AK_KEY_SCREEN_PRIMARY_EXTENDED:
		if (prev || v.i) ak_batch_set_bool(b, e->id, AK_PROP_SCREEN_PRIMARY_EXTENDED, v.i != 0);
		break;
	case AK_KEY_SCREEN_BOTTOM_TOOLBAR:
	case AK_KEY_SCREEN_TOOLBAR:
	case AK_KEY_SCREEN_PRIMARY:
		if (prev) unbar(prev->p);
		ak_view_sync(e);
		break;
	}
}

static void screen_finalize(ak_view *e) {
	// A search field when there's something to search.
	if (key_of(e, AK_KEY_SCREEN_SEARCH) || ak_view_handler(e, AK_EV_SCREEN_SEARCH_CHANGE) || ak_view_handler(e, AK_EV_SCREEN_SEARCH_SUBMIT)) {
		const ak_val *ph = key_of(e, AK_KEY_SCREEN_SEARCH_PLACEHOLDER);
		const ak_str *p = ph ? ph->p : NULL;
		if (p && p->len) ak_batch_set_string(e->r->batch, e->id, AK_PROP_SCREEN_SEARCH, p->data, p->len);
		else ak_batch_set_string(e->r->batch, e->id, AK_PROP_SCREEN_SEARCH, "Search", 6);
		ak_view_listen(e, AK_EVENT_TEXT_CHANGED);
		if (ak_view_handler(e, AK_EV_SCREEN_SEARCH_SUBMIT)) ak_view_listen(e, AK_EVENT_SUBMIT);
	}
}

// collect gathers the Buttons of a bar's slot, in order, marking what's
// in it as the bar's: a change to them sends the bar again.
static void collect(ak_view *bar, ak_view *v, screen_t *s, int k) {
	if (!v) return;
	v->bar = bar;
	if (v->el == AK_EL_BUTTON) {
		ak_vec_push(&s->items[k], (ak_view *)ak_retain(v));
		return;
	}
	for (uint32_t i = 0; i < v->kids.len; i++) collect(bar, v->kids.data[i], s, k);
}

static ak_str *str_key(const ak_view *v, uint16_t key) {
	const ak_val *x = key_of(v, key);
	return x ? x->p : NULL;
}

static void put_str(ak_buf *b, const ak_str *s) { ak_buf_str(b, s ? s->data : "", s ? s->len : 0); }

// encode_buttons encodes Buttons as an action menu: each its title, icon
// and disabled; one with a menu opens it.
static void encode_buttons(ak_buf *b, ak_view *const *bs, uint32_t n) {
	ak_buf_u32(b, n);
	for (uint32_t i = 0; i < n; i++) {
		ak_view *x = bs[i];
		const ak_val *menu = key_of(x, AK_KEY_BUTTON_MENU);
		const ak_arr *m = menu ? menu->p : NULL;
		uint8_t flags = 0;
		const ak_val *d = key_of(x, AK_KEY_DISABLED);
		if (d && d->i) flags |= AK_MENU_DISABLED;
		if (m && m->len) flags |= AK_MENU_SUBMENU;
		const ak_val *role = key_of(x, AK_KEY_BUTTON_ROLE);
		ak_buf_u8(b, role ? (uint8_t)role->i : 0);
		put_str(b, str_key(x, AK_KEY_BUTTON_TITLE));
		put_str(b, str_key(x, AK_KEY_BUTTON_ICON));
		ak_buf_u8(b, flags);
		if (flags & AK_MENU_SUBMENU) ak_encode_actions(b, m);
	}
}

static void screen_sync(ak_view *e) {
	screen_t *s = e->impl;
	for (int k = 0; k < NBARS; k++) {
		const ak_val *v = key_of(e, bar_keys[k]);
		ak_view *slot = v ? v->p : NULL;
		for (uint32_t i = 0; i < s->items[k].len; i++) ak_release(s->items[k].data[i]);
		s->items[k].len = 0;
		if (slot && ((ak_obj *)slot)->tag == AK_TAG_VIEW) collect(e, slot, s, k);
		uint32_t n = s->items[k].len;
		if (k == BAR_PRIMARY && n > 1) n = 1;  // one main action
		if (n == 0 && !s->has[k]) continue;
		s->buf.len = 0;
		encode_buttons(&s->buf, s->items[k].data, n);
		if (s->has[k] && s->sent[k].len == s->buf.len && memcmp(s->sent[k].data, s->buf.data, s->buf.len) == 0) continue;
		s->has[k] = n > 0;
		ak_buf t = s->sent[k];
		s->sent[k] = s->buf;
		s->buf = t;
		ak_batch_set_bytes(e->r->batch, e->id, bar_props[k], s->sent[k].data, s->sent[k].len);
		if (n && !s->listens_action) {
			s->listens_action = true;
			ak_view_listen(e, AK_EVENT_ACTION);
		}
	}
}

// bar_action runs the item numbered n of bar k: a Button's press, or an
// item of its menu.
static void bar_action(ak_view *e, int k, int64_t n) {
	screen_t *s = e->impl;
	uint32_t count = s->items[k].len;
	if (k == BAR_PRIMARY && count > 1) count = 1;
	for (uint32_t i = 0; i < count && n >= 0; i++) {
		ak_view *x = s->items[k].data[i];
		const ak_val *menu = key_of(x, AK_KEY_BUTTON_MENU);
		const ak_arr *m = menu ? menu->p : NULL;
		if (n == 0) {
			const ak_val *d = key_of(x, AK_KEY_DISABLED);
			if ((d && d->i) || (m && m->len) || x->dead) return;  // a host's bar was out of date; or it opens its menu
			ak_retain(x);
			ak_view_call(x, AK_EV_BUTTON_PRESS, NULL, 0);
			const ak_val *href = key_of(x, AK_KEY_BUTTON_HREF);
			if (href && href->p && ((ak_str *)href->p)->len) ak_router_href(e, href->p);
			ak_release(x);
			return;
		}
		n--;
		if (m && m->len) {
			int64_t c = ak_action_count(m);
			if (n < c) {
				ak_run_action(x, ak_find_action(m, n));
				return;
			}
			n -= c;
		}
	}
}

static void screen_event(ak_view *e, const ak_event *ev) {
	screen_t *s = e->impl;
	switch (ev->kind) {
	case AK_EVENT_ACTION:
		if (ev->i >= AK_SCREEN_PRIMARY_ACTION) bar_action(e, BAR_PRIMARY, ev->i - AK_SCREEN_PRIMARY_ACTION);
		else if (ev->i >= AK_SCREEN_TOOLBAR_ACTIONS) bar_action(e, BAR_TOOLBAR, ev->i - AK_SCREEN_TOOLBAR_ACTIONS);
		else bar_action(e, BAR_ACTIONS, ev->i);
		break;
	case AK_EVENT_BACK:
		if (s->back) ak_back_run(s->back);
		break;
	case AK_EVENT_TEXT_CHANGED: {
		ak_str *t = ak_str_new(ev->s.p, ev->s.len);
		ak_release(s->search);
		s->search = t;
		s->search_known = true;
		ak_val arg = {.p = t};
		ak_retain(t);
		ak_view_call(e, AK_EV_SCREEN_SEARCH_CHANGE, &arg, 1);
		ak_release(t);
		break;
	}
	case AK_EVENT_SUBMIT: call0(e, AK_EV_SCREEN_SEARCH_SUBMIT); break;
	}
}

static const ak_el_class cls_screen = {.name = "Screen", .kind = AK_KIND_SCREEN, .flags = AK_ELF_CHILDREN, .init = screen_init, .set = screen_set,
                                       .finalize = screen_finalize, .event = screen_event, .adjust = prov_adjust, .sync = screen_sync,
                                       .drop = screen_drop};

// ---- Tabs ----

typedef struct {
	int64_t reported;  // the tab the host shows, as last sent or reported
	bool known;
} tabs_t;

typedef struct {
	ak_closure *build;  // its page's builder, until it's built
	ak_owner *owner;    // its page's, once built
	bool built;
} tab_t;

static void tab_build(ak_view *tab) {
	tab_t *t = tab->impl;
	if (t->built || !t->build || tab->dead) return;
	t->built = true;
	ak_closure *build = t->build;
	t->build = NULL;
	t->owner = ak_root_under(tab->owner);
	ak_owner_set_context(t->owner, ak_env_key, tab->prov);
	ak_view *v = ak_build_under(tab->r, t->owner, build, NULL, 0);
	ak_release(build);
	if (v) {
		fill(v);
		ak_view_append(tab, v);
		ak_release(v);
	}
}

// Its page goes with what made it.
static void tab_dispose(void *ctx) {
	ak_view *tab = ctx;
	tab_t *t = tab->impl;
	if (t->owner) {
		ak_owner *o = t->owner;
		t->owner = NULL;
		ak_owner_dispose(o);
	}
	ak_release(tab);
}

static void tab_init(ak_view *e) {
	e->impl = ak_alloc(sizeof(tab_t));
	ak_prov *pv = ak_prov_new(e->r);
	ak_prov_attach(e, pv);
	ak_release(pv);
	ak_on_cleanup(tab_dispose, ak_retain(e));
}

static void tab_drop(ak_view *e) {
	tab_t *t = e->impl;
	if (!t) return;
	if (t->owner) ak_owner_dispose(t->owner);
	ak_release(t->build);
	ak_free(t);
}

static void tab_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	switch (key) {
	case AK_KEY_TAB_TITLE: send_str(e, AK_PROP_SCREEN_TITLE, v); break;
	case AK_KEY_TAB_ICON: send_str(e, AK_PROP_SCREEN_ICON, v); break;
	case AK_KEY_TAB_BADGE:
		if (prev || (v.p && ((ak_str *)v.p)->len)) send_str(e, AK_PROP_SCREEN_BADGE, v);
		break;
	}
}

static void tab_lazy(ak_view *e, ak_closure *build) {
	tab_t *t = e->impl;
	ak_retain(build);
	ak_release(t->build);
	t->build = build;
	ak_view *tabs = e->parent;
	if (tabs && tabs->el == AK_EL_TABS && tabs->finalized) {
		int64_t sel = key_int(tabs, AK_KEY_TABS_SELECTED, 0);
		if (sel >= 0 && sel < tabs->kids.len && tabs->kids.data[sel] == e) tab_build(e);
	}
}

static const ak_el_class cls_tab = {.name = "Tab", .kind = AK_KIND_SCREEN, .flags = AK_ELF_CHILDREN, .init = tab_init, .set = tab_set,
                                    .adjust = prov_adjust, .lazy = tab_lazy, .drop = tab_drop};

static void tabs_show(ak_view *e, int64_t i) {
	if (i >= 0 && i < e->kids.len) {
		ak_view *t = e->kids.data[i];
		if (t->el == AK_EL_TAB) tab_build(t);
	}
}

static void tabs_init(ak_view *e) { e->impl = ak_alloc(sizeof(tabs_t)); }

static void tabs_drop(ak_view *e) { ak_free(e->impl); }

static void tabs_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	tabs_t *t = e->impl;
	if (key != AK_KEY_TABS_SELECTED) return;
	if (!(t->known && t->reported == v.i)) {
		t->reported = v.i, t->known = true;
		ak_batch_set_int(e->r->batch, e->id, AK_PROP_TABS_SELECTED, v.i);
	}
	if (e->finalized) tabs_show(e, v.i);
}

static void tabs_finalize(ak_view *e) {
	ak_view_listen(e, AK_EVENT_SELECTED);
	tabs_show(e, key_int(e, AK_KEY_TABS_SELECTED, 0));
}

static void tabs_add(ak_view *e, ak_view *child) {
	if (child->el != AK_EL_TAB) ak_panic("ui: Tabs holds Tabs, not a %s", child->el ? ak_el_names[child->el] : child->cls->name);
	ak_view_append(e, child);
	if (e->finalized && key_int(e, AK_KEY_TABS_SELECTED, 0) == e->kids.len - 1) tab_build(child);
}

static void tabs_event(ak_view *e, const ak_event *ev) {
	tabs_t *t = e->impl;
	if (ev->kind != AK_EVENT_SELECTED) return;
	t->reported = ev->i, t->known = true;
	tabs_show(e, ev->i);
	ak_val arg = {.i = ev->i};
	ak_view_call(e, AK_EV_TABS_CHANGE, &arg, 1);
}

static const ak_el_class cls_tabs = {.name = "Tabs", .kind = AK_KIND_TABS, .flags = AK_ELF_CHILDREN, .init = tabs_init, .set = tabs_set,
                                     .finalize = tabs_finalize, .event = tabs_event, .adjust = grow_adjust, .add = tabs_add, .drop = tabs_drop};

// ---- Split ----

typedef struct {
	ak_view *stack;  // the detail, its kid
	AK_VEC(ak_view *) dests;  // its Destinations (retained)
	AK_VEC(ak_view *) groups;  // its Sidebars (retained)
	int64_t reported, shown;
	bool known;
	ak_buf sent;
} split_t;

typedef struct {
	ak_closure *build;
	ak_view *group;  // the Sidebar it's in, if any (its split keeps it)
} dest_t;

// A Sidebar: Destinations under a heading, which it hands its Split.
typedef struct {
	AK_VEC(ak_view *) dests;  // retained until it's in a Split
} group_t;

static void split_init(ak_view *e) {
	split_t *s = ak_alloc(sizeof *s);
	e->impl = s;
	s->shown = -1;
	s->stack = ak_stack_new(e->r);
	ak_view_finalize(s->stack);
	ak_view_append(e, s->stack);
	ak_release(s->stack);
}

static void split_drop(ak_view *e) {
	split_t *s = e->impl;
	if (!s) return;
	for (uint32_t i = 0; i < s->dests.len; i++) {
		s->dests.data[i]->up = NULL;
		dest_t *dt = s->dests.data[i]->impl;
		if (dt) dt->group = NULL;
		ak_release(s->dests.data[i]);
	}
	ak_vec_free(&s->dests);
	for (uint32_t i = 0; i < s->groups.len; i++) {
		s->groups.data[i]->up = NULL;
		ak_release(s->groups.data[i]);
	}
	ak_vec_free(&s->groups);
	ak_buf_free(&s->sent);
	ak_free(s);
}

static void split_show(ak_view *e, int64_t i) {
	split_t *s = e->impl;
	if (i == s->shown || i < 0 || i >= s->dests.len || e->dead) return;
	s->shown = i;
	ak_view *d = s->dests.data[i];
	dest_t *dt = d->impl;
	if (ak_stack_depth(s->stack)) ak_stack_replace(s->stack);
	ak_view *screen = ak_stack_push(s->stack, dt->build, NULL, 0, d->owner);
	// Its first screen's title is the destination's unless it has one.
	ak_str *title = str_key(d, AK_KEY_DESTINATION_TITLE);
	if (screen && title) ak_screen_set_title(screen, title);
}

static void split_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	split_t *s = e->impl;
	if (key == AK_KEY_SPLIT_TITLE) {
		send_str(e, AK_PROP_SPLIT_TITLE, v);
	} else if (key == AK_KEY_SPLIT_SELECTED) {
		if (!(s->known && s->reported == v.i)) {
			s->reported = v.i, s->known = true;
			ak_batch_set_int(e->r->batch, e->id, AK_PROP_SPLIT_SELECTED, v.i);
		}
		if (e->finalized) split_show(e, v.i);
	}
}

static void split_finalize(ak_view *e) {
	split_t *s = e->impl;
	ak_view_listen(e, AK_EVENT_SELECTED);
	if (!s->known) {
		s->known = true;
		ak_batch_set_int(e->r->batch, e->id, AK_PROP_SPLIT_SELECTED, 0);
	}
	split_show(e, key_int(e, AK_KEY_SPLIT_SELECTED, 0));
}

// split_take adds a destination, of group (a Sidebar) or none.
static void split_take(ak_view *e, ak_view *d, ak_view *group) {
	split_t *s = e->impl;
	ak_view_finalize(d);
	d->up = e;
	((dest_t *)d->impl)->group = group;
	ak_vec_push(&s->dests, (ak_view *)ak_retain(d));
}

static void split_add(ak_view *e, ak_view *child) {
	split_t *s = e->impl;
	if (child->el == AK_EL_SIDEBAR) {
		ak_view_finalize(child);
		child->up = e;
		ak_vec_push(&s->groups, (ak_view *)ak_retain(child));
		group_t *g = child->impl;
		for (uint32_t i = 0; i < g->dests.len; i++) {
			split_take(e, g->dests.data[i], child);
			ak_release(g->dests.data[i]);
		}
		g->dests.len = 0;
	} else if (child->el == AK_EL_DESTINATION) {
		split_take(e, child, NULL);
	} else {
		ak_panic("ui: Split holds Destinations and Sidebars, not a %s", child->el ? ak_el_names[child->el] : child->cls->name);
	}
	ak_view_sync(e);
	if (e->finalized && s->shown < 0) split_show(e, key_int(e, AK_KEY_SPLIT_SELECTED, 0));
}

// split_add_dest adds a destination of group's once group is in the split.
static void split_add_dest(ak_view *e, ak_view *d, ak_view *group) {
	split_t *s = e->impl;
	split_take(e, d, group);
	ak_view_sync(e);
	if (e->finalized && s->shown < 0) split_show(e, key_int(e, AK_KEY_SPLIT_SELECTED, 0));
}

// split_sync sends the sidebar: destinations next to each other with the
// same section are one.
static void split_sync(ak_view *e) {
	split_t *s = e->impl;
	uint32_t n = s->dests.len;
	ak_sidebar_item *items = ak_alloc((n ? n : 1) * sizeof *items);
	ak_sidebar_section *secs = ak_alloc((n ? n : 1) * sizeof *secs);
	uint32_t ns = 0;
	const ak_str *last = NULL;
	for (uint32_t i = 0; i < n; i++) {
		ak_view *d = s->dests.data[i];
		const ak_str *t = str_key(d, AK_KEY_DESTINATION_TITLE), *ic = str_key(d, AK_KEY_DESTINATION_ICON),
		             *bd = str_key(d, AK_KEY_DESTINATION_BADGE), *sec = str_key(d, AK_KEY_DESTINATION_SECTION);
		ak_view *group = ((dest_t *)d->impl)->group;
		if (!sec && group) sec = str_key(group, AK_KEY_SIDEBAR_TITLE);
		items[i] = (ak_sidebar_item){t ? ak_strn(t->data, t->len) : ak_cstr(""), ic ? ak_strn(ic->data, ic->len) : ak_cstr(""),
		                             bd ? ak_strn(bd->data, bd->len) : ak_cstr("")};
		bool same = ns > 0 && ((!sec && !last) || (sec && last && ak_str_eq(sec, last)) || (!sec && last && !last->len) || (sec && !last && !sec->len));
		if (!same) {
			secs[ns++] = (ak_sidebar_section){sec ? ak_strn(sec->data, sec->len) : ak_cstr(""), &items[i], 0};
			last = sec;
		}
		secs[ns - 1].count++;
	}
	ak_buf b = {0};
	ak_append_sidebar(&b, secs, ns);
	if (b.len != s->sent.len || memcmp(b.data, s->sent.data, b.len) != 0) {
		ak_batch_set_bytes(e->r->batch, e->id, AK_PROP_SPLIT_ITEMS, b.data, b.len);
		ak_buf_free(&s->sent);
		s->sent = b;
	} else {
		ak_buf_free(&b);
	}
	ak_free(items);
	ak_free(secs);
}

static void split_event(ak_view *e, const ak_event *ev) {
	split_t *s = e->impl;
	if (ev->kind != AK_EVENT_SELECTED) return;
	s->reported = ev->i, s->known = true;
	split_show(e, ev->i);
	ak_val arg = {.i = ev->i};
	ak_view_call(e, AK_EV_SPLIT_CHANGE, &arg, 1);
}

static const ak_el_class cls_split = {.name = "Split", .kind = AK_KIND_SPLIT, .flags = AK_ELF_CHILDREN, .init = split_init, .set = split_set,
                                      .finalize = split_finalize, .event = split_event, .adjust = grow_adjust, .add = split_add,
                                      .sync = split_sync, .drop = split_drop};

static void dest_init(ak_view *e) { e->impl = ak_alloc(sizeof(dest_t)); }

static void dest_drop(ak_view *e) {
	dest_t *d = e->impl;
	if (!d) return;
	ak_release(d->build);
	ak_free(d);
}

static void dest_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(key), AK_UNUSED(v), AK_UNUSED(prev);
	if (e->up && !e->up->dead) ak_view_sync(e->up);  // its split's sidebar
}

static void dest_lazy(ak_view *e, ak_closure *build) {
	dest_t *d = e->impl;
	ak_retain(build);
	ak_release(d->build);
	d->build = build;
}

static void dest_add(ak_view *e, ak_view *child) {
	// Its content, built by C code first.
	ak_closure *c = ak_closure_new(built, NULL, 1, ref_cap);
	c->caps[0].p = ak_retain(child);
	dest_lazy(e, c);
	ak_release(c);
}

static const ak_el_class cls_dest = {.name = "Destination", .kind = AK_KIND_VIEW, .flags = AK_ELF_FRAGMENT | AK_ELF_FLATTEN | AK_ELF_CHILDREN,
                                     .init = dest_init, .set = dest_set, .add = dest_add, .lazy = dest_lazy, .drop = dest_drop};

static void group_init(ak_view *e) { e->impl = ak_alloc(sizeof(group_t)); }

static void group_drop(ak_view *e) {
	group_t *g = e->impl;
	if (!g) return;
	for (uint32_t i = 0; i < g->dests.len; i++) ak_release(g->dests.data[i]);
	ak_vec_free(&g->dests);
	ak_free(g);
}

static void group_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(key), AK_UNUSED(v), AK_UNUSED(prev);
	if (e->up && !e->up->dead) ak_view_sync(e->up);  // its split's sidebar
}

static void group_add(ak_view *e, ak_view *child) {
	if (child->el != AK_EL_DESTINATION) ak_panic("ui: Sidebar holds Destinations, not a %s", child->el ? ak_el_names[child->el] : child->cls->name);
	if (e->up && !e->up->dead) {
		split_add_dest(e->up, child, e);
		return;
	}
	ak_vec_push(&((group_t *)e->impl)->dests, (ak_view *)ak_retain(child));
}

static const ak_el_class cls_group = {.name = "Sidebar", .kind = AK_KIND_VIEW, .flags = AK_ELF_FRAGMENT | AK_ELF_FLATTEN | AK_ELF_CHILDREN,
                                      .init = group_init, .set = group_set, .add = group_add, .drop = group_drop};

// ---- PageControl ----

typedef struct {
	int64_t reported;
	bool known;
} pages_t;

static void pages_init(ak_view *e) {
	ak_view_measure(e);
	e->impl = ak_alloc(sizeof(pages_t));
}

static void pages_drop(ak_view *e) { ak_free(e->impl); }

static void pages_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	pages_t *p = e->impl;
	if (key == AK_KEY_PAGE_CONTROL_COUNT) {
		ak_batch_set_int(e->r->batch, e->id, AK_PROP_PAGE_CONTROL_COUNT, v.i);
		ak_view_remeasure(e);
	} else if (key == AK_KEY_PAGE_CONTROL_PAGE) {
		if (p->known && p->reported == v.i) return;
		p->reported = v.i, p->known = true;
		ak_batch_set_int(e->r->batch, e->id, AK_PROP_PAGE_CONTROL_PAGE, v.i);
	}
}

static void pages_finalize(ak_view *e) {
	if (key_of(e, AK_KEY_PAGE_CONTROL_PAGE) || ak_view_handler(e, AK_EV_PAGE_CONTROL_CHANGE)) ak_view_listen(e, AK_EVENT_SELECTED);
}

static void pages_event(ak_view *e, const ak_event *ev) {
	pages_t *p = e->impl;
	if (ev->kind != AK_EVENT_SELECTED) return;
	p->reported = ev->i, p->known = true;
	ak_val arg = {.i = ev->i};
	ak_view_call(e, AK_EV_PAGE_CONTROL_CHANGE, &arg, 1);
}

static const ak_el_class cls_pages = {.name = "PageControl", .kind = AK_KIND_PAGE_CONTROL, .flags = AK_ELF_MEASURED | AK_ELF_CACHE,
                                      .init = pages_init, .set = pages_set, .finalize = pages_finalize, .event = pages_event,
                                      .drop = pages_drop};

// ---- Classes, events ----

extern const ak_el_class ak_class_sheet, ak_class_alert, ak_class_popover, ak_class_menu, ak_class_window, ak_class_router, ak_class_route, ak_class_boundary,
    ak_class_overlay;

const ak_el_class *ak_nav_class_of(uint16_t el) {
	switch (el) {
	case AK_EL_STACK: return &ak_class_stack;
	case AK_EL_SCREEN: return &cls_screen;
	case AK_EL_TABS: return &cls_tabs;
	case AK_EL_TAB: return &cls_tab;
	case AK_EL_SPLIT: return &cls_split;
	case AK_EL_DESTINATION: return &cls_dest;
	case AK_EL_SIDEBAR: return &cls_group;
	case AK_EL_PAGE_CONTROL: return &cls_pages;
	case AK_EL_SHEET: return &ak_class_sheet;
	case AK_EL_ALERT: return &ak_class_alert;
	case AK_EL_POPOVER: return &ak_class_popover;
	case AK_EL_MENU: return &ak_class_menu;
	case AK_EL_WINDOW: return &ak_class_window;
	case AK_EL_ROUTER: return &ak_class_router;
	case AK_EL_ROUTE: return &ak_class_route;
	case AK_EL_BOUNDARY: return &ak_class_boundary;
	case AK_EL_OVERLAY: return &ak_class_overlay;
	}
	return NULL;
}

// ak_nav_ev_ops is the ops event each of these elements' events is
// reported by, where a handler alone makes it listen; the others listen
// as they need.
ak_event_kind ak_nav_ev_ops(uint16_t ev) {
	switch (ev) {
	case AK_EV_TABS_CHANGE:
	case AK_EV_SPLIT_CHANGE:
	case AK_EV_PAGE_CONTROL_CHANGE:
		return AK_EVENT_SELECTED;
	case AK_EV_WINDOW_CLOSE_REQUEST: return AK_EVENT_CLOSE_REQUEST;
	}
	return 0;
}

void ak_root_event(ak_view *e, const ak_event *ev) {
	ak_ui_root *r = e->r;
	switch (ev->kind) {
	case AK_EVENT_BACK:
		if (r->back) ak_back_run(r->back);
		break;
	case AK_EVENT_LOCATION: ak_router_location(e, ev); break;
	case AK_EVENT_ACTION: ak_menubar_action(r, ev->i); break;  // the menu bar's
	default: ak_window_event(e, ev); break;  // the main window's
	}
}
