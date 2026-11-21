// Navigation and overlays (core/src/ui: nav.c, overlay.c, window.c,
// router.c, boundary.c) on the C fake host: ports of the Go ui package's
// nav_test.go, back_test.go, popover_test.go, snackbar_test.go,
// split_test.go, window_test.go, windowhandle_test.go, router_test.go,
// catch_test.go, paging_test.go and the list's sections, swipe actions
// and selection. Views are built in C, as compiled code builds them.
#include "t.h"
#include "fake/uitest.h"
#include "ui/ui_int.h"
#include "fuzz.h"

#include <math.h>
#include <stdio.h>

// ---- Helpers ----

static void setup(void) {
	ak_signal_bind_thread();
	ak_vm_init();
	ak_ui_builtins_init();
}

static void set(ak_view *v, uint16_t key, ak_val x) {
	ak_ui_set(v, key, x);
	ak_release_val(x, (ak_kind)ak_key_kinds[key]);
}
static void set_str(ak_view *v, uint16_t key, const char *s) { set(v, key, (ak_val){.p = ak_str_c(s)}); }
static bool str_is(const ak_str *a, const char *s) { return a->len == strlen(s) && memcmp(a->data, s, a->len) == 0; }
static void set_i(ak_view *v, uint16_t key, int64_t i) { ak_ui_set(v, key, (ak_val){.i = i}); }

static void add(ak_view *p, ak_view *c) {
	ak_ui_add(p, c);
	ak_release(c);
}

static ak_closure *fn(ak_invoke_fn f, const void *data) { return ak_closure_new(f, data, 0, NULL); }

static void bind(ak_view *v, uint16_t key, ak_invoke_fn f, const void *data) {
	ak_closure *c = fn(f, data);
	ak_ui_bind(v, key, c);
	ak_release(c);
}

static void on(ak_view *v, uint16_t ev, ak_invoke_fn f, const void *data) {
	ak_closure *c = fn(f, data);
	ak_ui_on(v, ev, c);
	ak_release(c);
}

static void lazy(ak_view *v, ak_invoke_fn f, const void *data) {
	ak_closure *c = fn(f, data);
	ak_ui_lazy(v, c);
	ak_release(c);
}

static void slot(ak_view *v, uint16_t key, ak_view *w) {
	ak_ui_slot(v, key, w);
	ak_release(w);
}

static ak_view *text(const char *s) {
	ak_view *t = ak_ui_new(AK_EL_TEXT);
	set_str(t, AK_KEY_TEXT_TEXT, s);
	return t;
}

static ak_view *button(const char *title, ak_invoke_fn press, const void *data) {
	ak_view *b = ak_ui_new(AK_EL_BUTTON);
	set_str(b, AK_KEY_BUTTON_TITLE, title);
	if (press) on(b, AK_EV_BUTTON_PRESS, press, data);
	return b;
}

static void *cell_int(int64_t v) { return ak_vm_cell_new((ak_val){.i = v}, AK_K_INT, NULL).p; }
static int64_t peek_i(void *cell) { return ak_vm_cell_get(cell, false).i; }
static void write_i(void *cell, int64_t v) { ak_vm_cell_set(cell, (ak_val){.i = v}); }

static ak_val read_cell(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return ak_vm_cell_get((void *)c->fn, true);
}

// write_arg is a bind's write: the cell (data) takes the event's value.
static ak_val write_arg(ak_closure *c, ak_val *a, uint32_t n) {
	if (n > 0) ak_vm_cell_set((void *)c->fn, a[0]);
	return (ak_val){0};
}

static ak_val count_up(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	(*(int *)c->fn)++;
	return (ak_val){0};
}

static ak_uitest *mount(ak_invoke_fn build, const void *data) {
	setup();
	ak_closure *c = fn(build, data);
	ak_uitest *a = ak_uitest_mount(c, NULL);
	ak_release(c);
	return a;
}

static const char *cstr(ak_buf *b) {
	ak_buf_u8(b, 0);
	b->len--;
	return (const char *)b->data;
}

static void done(ak_uitest *a) {
	ak_buf err = {0};
	if (!ak_uitest_unmount(a, &err)) T_FAIL("%s", cstr(&err));
	ak_buf_free(&err);
	ak_release(a);
	ak_ui_stats s;
	ak_ui_get_stats(&s);
	if (s.views != 0 || s.native != 0) T_FAIL("%u views, %u native views outlived the app", s.views, s.native);
}

static ak_fake_view *root(ak_uitest *a) { return ak_fake_root(ak_uitest_host(a)); }

static void check_text(ak_fake_view *v, const char *want) {
	ak_buf b = {0};
	ak_fake_text(v, &b);
	CHECK_EQ_STR(cstr(&b), want);
	ak_buf_free(&b);
}

static ak_fake_view *nth(ak_uitest *a, ak_view_kind k, uint32_t i) {
	ak_fake_view_vec vs = {0};
	ak_fake_of_kind(ak_uitest_host(a), k, &vs);
	ak_fake_view *v = i < vs.len ? vs.data[i] : NULL;
	ak_vec_free(&vs);
	return v;
}

static uint32_t count_kind(ak_uitest *a, ak_view_kind k) {
	ak_fake_view_vec vs = {0};
	ak_fake_of_kind(ak_uitest_host(a), k, &vs);
	uint32_t n = vs.len;
	ak_vec_free(&vs);
	return n;
}

static ak_fake_view *one(ak_uitest *a, ak_view_kind k, const char *s) { return ak_fake_one(ak_uitest_host(a), k, s); }

static bool span_is(ak_span s, const char *want) { return ak_span_eqc(s, want); }

static void tap(ak_uitest *a, const char *title) {
	ak_fake_view *b = one(a, AK_KIND_BUTTON, title);
	if (!b) {
		T_FAIL("no button %s", title);
		return;
	}
	ak_fake_tap(ak_uitest_host(a), b);
}

// The titles of an action menu prop, depth first, joined by "|".
static void check_actions(const ak_fake_view *v, ak_prop p, const char *want) {
	ak_action_vec items = {0};
	ak_fake_action_items(v, p, &items);
	ak_buf b = {0};
	for (uint32_t i = 0; i < items.len; i++) {
		if (i) ak_buf_u8(&b, '|');
		ak_buf_put(&b, items.data[i].title.p, items.data[i].title.len);
		if (items.data[i].flags & AK_MENU_DISABLED) ak_buf_put(&b, "(off)", 5);
	}
	CHECK_EQ_STR(cstr(&b), want);
	ak_buf_free(&b);
	ak_vec_free(&items);
}

// action makes an Action.button(title, run).
static const uint8_t button_kinds[] = {AK_K_REF, AK_K_REF, AK_K_INT, AK_K_BOOL, AK_K_BOOL, AK_K_REF};
static const uint8_t sub_kinds[] = {AK_K_REF, AK_K_REF, AK_K_REF};
static const uint8_t sec_kinds[] = {AK_K_REF, AK_K_REF};
static const ak_case action_cases[] = {{"button", 6, button_kinds, NULL, NULL}, {"submenu", 3, sub_kinds, NULL, NULL}, {"section", 2, sec_kinds, NULL, NULL}};
static const ak_shape action_shape = {"Action", 0, NULL, NULL, NULL, 3, action_cases, false, NULL};

static ak_enum *action(const char *title, int role, bool disabled, ak_invoke_fn run, const void *data) {
	ak_enum *a = ak_enum_new(&action_shape, 0);
	a->f[0].p = ak_str_c(title);
	a->f[1].p = ak_str_empty();
	a->f[2].i = role;
	a->f[3].i = disabled;
	a->f[5].p = run ? fn(run, data) : NULL;
	return a;
}

static ak_arr *actions(ak_enum **as, uint32_t n) {
	ak_arr *r = ak_arr_new(AK_K_REF, NULL, n);
	for (uint32_t i = 0; i < n; i++) ak_arr_push(&r, (ak_val){.p = as[i]});
	return r;
}

// ---- Stack: push, pop, back (nav_test.go's TestStackPushPopDisposesTheScreen) ----

static void *ticks;
static int runs;

static ak_val tick_effect(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_vm_cell_get(ticks, true);
	runs++;
	return (ak_val){0};
}

static ak_val tick_text(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	char b[32];
	snprintf(b, sizeof b, "tick %lld", (long long)ak_vm_cell_get(ticks, true).i);
	return (ak_val){.p = ak_str_c(b)};
}

static ak_val do_pop(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_ui_pop();
	return (ak_val){0};
}

static ak_val detail_screen(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_closure *e = fn(tick_effect, NULL);
	ak_vm_effect(e, false);
	ak_release(e);
	ak_view *s = ak_ui_new(AK_EL_SCREEN);
	set_str(s, AK_KEY_SCREEN_TITLE, "Detail");
	ak_view *t = ak_ui_new(AK_EL_TEXT);
	bind(t, AK_KEY_TEXT_TEXT, tick_text, NULL);
	add(s, t);
	add(s, button("Back", do_pop, NULL));
	return (ak_val){.p = s};
}

static ak_val do_push(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	ak_closure *b = fn((ak_invoke_fn)c->fn, NULL);
	ak_ui_push(b);
	ak_release(b);
	return (ak_val){0};
}

static ak_val home_screen(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *s = ak_ui_new(AK_EL_SCREEN);
	set_str(s, AK_KEY_SCREEN_TITLE, "Home");
	add(s, button("Open", do_push, (const void *)detail_screen));
	return (ak_val){.p = s};
}

static ak_val stack_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *st = ak_ui_new(AK_EL_STACK);
	lazy(st, home_screen, NULL);
	return (ak_val){.p = st};
}

TEST(stack_push_pop_disposes_the_screen) {
	ticks = cell_int(0);
	ak_uitest *a = mount(stack_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *stack = nth(a, AK_KIND_STACK, 0);
	REQUIRE(stack);
	REQUIRE(stack->children.len == 1);
	CHECK(span_is(ak_fake_string(stack->children.data[0], AK_PROP_SCREEN_TITLE), "Home"));
	tap(a, "Open");
	REQUIRE(stack->children.len == 2);
	CHECK(span_is(ak_fake_string(stack->children.data[1], AK_PROP_SCREEN_TITLE), "Detail"));
	check_text(stack->children.data[1], "tick 0|Back");
	// The screen's content fills it, as the host sized it.
	CHECK_EQ_F(stack->children.data[1]->children.data[0]->frame[2], 390);

	// Pop from code: the screen's effects stop and its views go.
	uint32_t views = f->nviews;
	tap(a, "Back");
	CHECK_EQ_INT(stack->children.len, 1);
	CHECK_EQ_INT(f->nviews, views - 3);  // the screen, its text and its button
	runs = 0;
	write_i(ticks, 10);
	ak_ui_flush();
	CHECK_EQ_INT(runs, 0);

	// The user's back gesture: the host pops, then reports.
	tap(a, "Open");
	CHECK_EQ_INT(stack->children.len, 2);
	ak_fake_back(f, stack);
	runs = 0;
	write_i(ticks, 11);
	ak_ui_flush();
	CHECK_EQ_INT(runs, 0);
	CHECK_EQ_INT(count_kind(a, AK_KIND_SCREEN), 1);
	done(a);
	ak_release(ticks);
}

// push outside a Stack throws.
static int push_errors;

static void count_error(void *ctx, void *err) {
	AK_UNUSED(ctx), AK_UNUSED(err);
	push_errors++;
}

static ak_val lone_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.p = button("Open", do_push, (const void *)detail_screen)};
}

TEST(push_outside_a_stack_throws) {
	ticks = cell_int(0);
	push_errors = 0;
	ak_signal_set_error_handler(count_error, NULL);
	ak_uitest *a = mount(lone_app, NULL);
	tap(a, "Open");
	CHECK_EQ_INT(push_errors, 1);
	done(a);
	ak_signal_set_error_handler(NULL, NULL);
	ak_release(ticks);
}

// ---- A screen's environment (TestScreenSafeAreaUsesTheScreensEnvironment) ----

static ak_val safe_content(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *s = ak_ui_new(AK_EL_SAFE_AREA);
	add(s, text("hi"));
	return (ak_val){.p = s};
}

static ak_val safe_stack(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *st = ak_ui_new(AK_EL_STACK);
	lazy(st, safe_content, NULL);
	return (ak_val){.p = st};
}

TEST(screen_safe_area_uses_the_screens_environment) {
	ak_uitest *a = mount(safe_stack, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *screen = nth(a, AK_KIND_SCREEN, 0);
	REQUIRE(screen);
	ak_environment env = f->env;
	env.safe_top = 100;  // under a navigation bar
	ak_fake_dispatch(f, &(ak_event){.id = screen->id, .kind = AK_EVENT_ENVIRONMENT, .env = env});
	ak_fake_view *t = one(a, AK_KIND_TEXT, "hi");
	REQUIRE(t);
	CHECK_EQ_F(t->frame[1], 100);
	done(a);
}

// ---- A screen's bars (TestScreenActions, TestScreenToolbarAndPrimary, TestScreenMenuButton) ----

static int saved, moved, composed;
static void *archive_title;

static ak_val bar_screen(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *s = ak_ui_new(AK_EL_SCREEN);
	set_str(s, AK_KEY_SCREEN_TITLE, "Edit");
	add(s, text("x"));
	slot(s, AK_KEY_SCREEN_TOOLBAR, button("Save", count_up, &saved));
	ak_view *tb = ak_ui_fragment();
	ak_view *arch = ak_ui_new(AK_EL_BUTTON);
	bind(arch, AK_KEY_BUTTON_TITLE, read_cell, archive_title);
	set_i(arch, AK_KEY_DISABLED, 1);
	add(tb, arch);
	ak_view *move = button("Move", NULL, NULL);
	ak_enum *items[] = {action("Inbox", 0, false, count_up, &moved), action("Trash", 2, false, NULL, NULL)};
	set(move, AK_KEY_BUTTON_MENU, (ak_val){.p = actions(items, 2)});
	add(tb, move);
	slot(s, AK_KEY_SCREEN_BOTTOM_TOOLBAR, tb);
	ak_view *p = button("Compose", count_up, &composed);
	set_str(p, AK_KEY_BUTTON_ICON, "square.and.pencil");
	slot(s, AK_KEY_SCREEN_PRIMARY, p);
	return (ak_val){.p = s};
}

static ak_val bar_stack(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *st = ak_ui_new(AK_EL_STACK);
	lazy(st, bar_screen, NULL);
	return (ak_val){.p = st};
}

TEST(screen_bars) {
	saved = moved = composed = 0;
	archive_title = ak_vm_cell_new((ak_val){.p = ak_str_c("Archive")}, AK_K_REF, NULL).p;
	ak_uitest *a = mount(bar_stack, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *screen = nth(a, AK_KIND_SCREEN, 0);
	REQUIRE(screen);
	check_actions(screen, AK_PROP_SCREEN_ACTIONS, "Save");
	check_actions(screen, AK_PROP_SCREEN_TOOLBAR, "Archive(off)|Move|Inbox|Trash");
	check_actions(screen, AK_PROP_SCREEN_PRIMARY, "Compose");
	ak_fake_choose(f, screen, 0);
	CHECK_EQ_INT(saved, 1);
	CHECK(ak_fake_press_toolbar(f, screen, "Inbox"));
	CHECK_EQ_INT(moved, 1);
	CHECK(ak_fake_press_toolbar(f, screen, "Archive"));  // disabled: nothing
	CHECK(ak_fake_press_primary(f, screen));
	CHECK_EQ_INT(composed, 1);
	// A bar button's title follows its binding: the bar is sent again,
	// once.
	ak_fake_reset(f);
	ak_vm_cell_set(archive_title, (ak_val){.p = ak_str_c("Archive all")});
	ak_ui_flush();
	check_actions(screen, AK_PROP_SCREEN_TOOLBAR, "Archive all(off)|Move|Inbox|Trash");
	uint32_t sets = 0;
	for (uint32_t i = 0; i < ak_fake_op_count(f); i++) {
		ak_op op = ak_fake_op_at(f, i);
		if (op.code == AK_OP_SET_PROP && op.id == screen->id) sets++;
	}
	CHECK_EQ_INT(sets, 1);
	done(a);
	ak_release(archive_title);
}

// ---- Going back (back_test.go) ----

static void *intercept;
static int asked;

static ak_val ask_back(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	asked++;
	return (ak_val){.i = ak_vm_cell_get(intercept, false).i};
}

static ak_val guarded_screen(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_closure *h = fn(ask_back, NULL);
	ak_ui_on_back(h);
	ak_release(h);
	ak_view *s = ak_ui_new(AK_EL_SCREEN);
	set_str(s, AK_KEY_SCREEN_TITLE, "Draft");
	add(s, text("draft"));
	return (ak_val){.p = s};
}

static ak_val guarded_home(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.p = button("Write", do_push, (const void *)guarded_screen)};
}

static ak_val guarded_stack(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *st = ak_ui_new(AK_EL_STACK);
	lazy(st, guarded_home, NULL);
	return (ak_val){.p = st};
}

TEST(on_back_screen) {
	intercept = ak_vm_cell_new((ak_val){.i = 1}, AK_K_BOOL, NULL).p;
	asked = 0;
	ak_uitest *a = mount(guarded_stack, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *stack = nth(a, AK_KIND_STACK, 0);
	tap(a, "Write");
	REQUIRE(stack->children.len == 2);
	ak_fake_view *draft = stack->children.data[1];
	CHECK(ak_fake_bool(draft, AK_PROP_INTERCEPT_BACK));
	CHECK(!ak_fake_bool(stack->children.data[0], AK_PROP_INTERCEPT_BACK));
	ak_fake_back(f, stack);  // the handler takes it: the screen stays
	CHECK_EQ_INT(asked, 1);
	CHECK_EQ_INT(stack->children.len, 2);
	ak_vm_cell_set(intercept, (ak_val){.i = 0});
	ak_fake_back(f, stack);  // it doesn't: the screen pops
	CHECK_EQ_INT(asked, 2);
	CHECK_EQ_INT(stack->children.len, 1);
	done(a);
	ak_release(intercept);
}

static ak_val root_guard_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_closure *h = fn(ask_back, NULL);
	ak_ui_on_back(h);
	ak_release(h);
	return (ak_val){.p = text("home")};
}

TEST(on_back_root) {
	intercept = ak_vm_cell_new((ak_val){.i = 1}, AK_K_BOOL, NULL).p;
	asked = 0;
	ak_uitest *a = mount(root_guard_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	CHECK(ak_fake_back_from_root(f));
	CHECK_EQ_INT(asked, 1);
	ak_vm_cell_set(intercept, (ak_val){.i = 0});
	ak_fake_reset(f);
	CHECK(ak_fake_back_from_root(f));
	ak_fake_index_vec cmds = {0};
	ak_fake_commands(f, AK_CMD_BACK_EXIT, &cmds);
	CHECK_EQ_INT(cmds.len, 1);  // nobody took it: the app leaves
	ak_vec_free(&cmds);
	done(a);
	ak_release(intercept);
}

// ---- Tabs (TestTabs, TestTabBadges) ----

static void *sel_cell, *badge;
static int pages_built;

static ak_val page(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	pages_built++;
	return (ak_val){.p = text((const char *)c->fn)};
}

static ak_val tabs_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *tabs = ak_ui_new(AK_EL_TABS);
	bind(tabs, AK_KEY_TABS_SELECTED, read_cell, sel_cell);
	on(tabs, AK_EV_TABS_CHANGE, write_arg, sel_cell);
	ak_view *t1 = ak_ui_new(AK_EL_TAB);
	set_str(t1, AK_KEY_TAB_TITLE, "Inbox");
	set_str(t1, AK_KEY_TAB_ICON, "tray");
	bind(t1, AK_KEY_TAB_BADGE, read_cell, badge);
	lazy(t1, page, "inbox page");
	add(tabs, t1);
	ak_view *t2 = ak_ui_new(AK_EL_TAB);
	set_str(t2, AK_KEY_TAB_TITLE, "Settings");
	lazy(t2, page, "settings page");
	add(tabs, t2);
	return (ak_val){.p = tabs};
}

TEST(tabs) {
	sel_cell = cell_int(0);
	badge = ak_vm_cell_new((ak_val){.p = ak_str_c("3")}, AK_K_REF, NULL).p;
	pages_built = 0;
	ak_uitest *a = mount(tabs_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *tabs = nth(a, AK_KIND_TABS, 0);
	REQUIRE(tabs && tabs->children.len == 2);
	ak_fake_view *inbox = tabs->children.data[0], *settings = tabs->children.data[1];
	CHECK(span_is(ak_fake_string(inbox, AK_PROP_SCREEN_TITLE), "Inbox"));
	CHECK(span_is(ak_fake_string(inbox, AK_PROP_SCREEN_ICON), "tray"));
	CHECK(span_is(ak_fake_string(inbox, AK_PROP_SCREEN_BADGE), "3"));
	CHECK_EQ_INT(pages_built, 1);  // the chosen tab's page alone
	check_text(inbox, "inbox page");
	CHECK_EQ_INT(settings->children.len, 0);
	// The user's choice builds its page and writes selected.
	ak_fake_select(f, tabs, 1);
	CHECK_EQ_INT(peek_i(sel_cell), 1);
	CHECK_EQ_INT(pages_built, 2);
	check_text(settings, "settings page");
	// Code's choice is sent; a page is built once.
	ak_fake_reset(f);
	write_i(sel_cell, 0);
	ak_ui_flush();
	CHECK_EQ_INT(ak_fake_int(tabs, AK_PROP_TABS_SELECTED), 0);
	CHECK_EQ_INT(pages_built, 2);
	// The badge follows its binding.
	ak_vm_cell_set(badge, (ak_val){.p = ak_str_c("")});
	ak_ui_flush();
	CHECK(span_is(ak_fake_string(inbox, AK_PROP_SCREEN_BADGE), ""));
	done(a);
	ak_release(sel_cell), ak_release(badge);
}

// ---- Sheet, present, dismiss (TestSheet, TestOnBackSheet) ----

static void *open_cell;
static int dismisses;

static ak_val do_dismiss(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_ui_dismiss();
	return (ak_val){0};
}

static ak_val sheet_content(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	add(col, text("in the sheet"));
	add(col, button("Done", do_dismiss, NULL));
	return (ak_val){.p = col};
}

static ak_val compose_screen(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	add(col, text("compose"));
	add(col, button("Send", do_dismiss, NULL));
	return (ak_val){.p = col};
}

static ak_val do_present(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_closure *b = fn(compose_screen, NULL);
	ak_ui_present(b);
	ak_release(b);
	return (ak_val){0};
}

static ak_val sheet_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	add(col, button("Compose", do_present, NULL));
	ak_view *s = ak_ui_new(AK_EL_SHEET);
	bind(s, AK_KEY_SHEET_OPEN, read_cell, open_cell);
	on(s, AK_EV_SHEET_CHANGE, write_arg, open_cell);
	on(s, AK_EV_SHEET_DISMISS, count_up, &dismisses);
	set_i(s, AK_KEY_SHEET_GRABBER, 1);
	ak_enum *none[1];
	AK_UNUSED(none);
	ak_arr *dets = ak_arr_new(AK_K_INT, NULL, 2);
	ak_arr_push(&dets, (ak_val){.i = 0});  // medium
	ak_arr_push(&dets, (ak_val){.i = 1});  // large
	set(s, AK_KEY_SHEET_DETENTS, (ak_val){.p = dets});
	lazy(s, sheet_content, NULL);
	add(col, s);
	add(col, text("below"));
	return (ak_val){.p = col};
}

TEST(sheet) {
	open_cell = ak_vm_cell_new((ak_val){.i = 0}, AK_K_BOOL, NULL).p;
	dismisses = 0;
	ak_uitest *a = mount(sheet_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	CHECK_EQ_INT(count_kind(a, AK_KIND_SHEET), 0);
	// It takes no room where it's declared.
	CHECK_EQ_F(one(a, AK_KIND_TEXT, "below")->frame[1], 44);
	ak_vm_cell_set(open_cell, (ak_val){.i = 1});
	ak_ui_flush();
	ak_fake_view *sheet = nth(a, AK_KIND_SHEET, 0);
	REQUIRE(sheet && sheet->presented);
	CHECK_EQ_INT(ak_fake_int(sheet, AK_PROP_SHEET_DETENTS), 3);
	CHECK(ak_fake_bool(sheet, AK_PROP_SHEET_GRABBER));
	CHECK_EQ_INT(ak_fake_int(sheet, AK_PROP_SHEET_WINDOW), root(a)->id);
	check_text(sheet, "in the sheet|Done");
	// Its content is laid out at the sheet's environment.
	ak_fake_report_sheet(f, sheet);
	CHECK_EQ_F(one(a, AK_KIND_TEXT, "in the sheet")->frame[2], 390);
	// The user dismisses it: open goes false, dismiss runs, it's gone.
	ak_fake_dismiss(f, sheet);
	CHECK_EQ_INT(peek_i(open_cell), 0);
	CHECK_EQ_INT(dismisses, 1);
	CHECK_EQ_INT(count_kind(a, AK_KIND_SHEET), 0);
	// dismiss() in it, likewise.
	ak_vm_cell_set(open_cell, (ak_val){.i = 1});
	ak_ui_flush();
	tap(a, "Done");
	CHECK_EQ_INT(peek_i(open_cell), 0);
	CHECK_EQ_INT(dismisses, 2);
	CHECK_EQ_INT(count_kind(a, AK_KIND_SHEET), 0);
	// present() and dismiss().
	tap(a, "Compose");
	REQUIRE(count_kind(a, AK_KIND_SHEET) == 1);
	check_text(nth(a, AK_KIND_SHEET, 0), "compose|Send");
	tap(a, "Send");
	CHECK_EQ_INT(count_kind(a, AK_KIND_SHEET), 0);
	// Open as the app goes: it goes too.
	ak_vm_cell_set(open_cell, (ak_val){.i = 1});
	ak_ui_flush();
	done(a);
	ak_release(open_cell);
}

// ---- A sheet as tall as its content (DetentFit, macOS) ----

static bool fit_wide;         // its content holds a view wider than a sheet
static int fit_min_measures;  // its text's measures at its narrowest

// fit_measure measures text as the fake host does, 8pt a character in
// 20pt lines, wrapping where it's offered less, and counts the measures
// at its narrowest (min-content).
static void fit_measure(void *ctx, ak_fake_view *v, float w, float h, float aw, float ah, float *ow, float *oh) {
	AK_UNUSED(ctx), AK_UNUSED(h), AK_UNUSED(ah);
	float full = (float)ak_fake_string(v, AK_PROP_TEXT_TEXT).len * 8;
	float at = w == w ? w : aw;
	if (aw < 0 && isinf(aw) && w != w) {
		fit_min_measures++;
		at = 80;  // its widest word
	}
	if (!(at < full)) at = full;
	*ow = at;
	*oh = at > 0 ? ceilf(full / at) * 20 : 20;
}

static ak_val fit_content(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	char words[2001];
	for (int i = 0; i < 2000; i++) words[i] = i % 10 == 9 ? ' ' : 'w';
	words[2000] = 0;
	add(col, text(words));
	if (fit_wide) {
		ak_view *v = ak_ui_new(AK_EL_VIEW);
		set_i(v, AK_KEY_WIDTH, ak_length_pt(820));
		set_i(v, AK_KEY_HEIGHT, ak_length_pt(10));
		add(col, v);
	}
	return (ak_val){.p = col};
}

static ak_val fit_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	ak_view *s = ak_ui_new(AK_EL_SHEET);
	bind(s, AK_KEY_SHEET_OPEN, read_cell, open_cell);
	ak_arr *dets = ak_arr_new(AK_K_INT, NULL, 1);
	ak_arr_push(&dets, (ak_val){.i = 2});  // fit
	set(s, AK_KEY_SHEET_DETENTS, (ak_val){.p = dets});
	lazy(s, fit_content, NULL);
	add(col, s);
	return (ak_val){.p = col};
}

// A sheet as tall as its content is laid out at a sheet's width, its text
// measured there alone; content that can't be that narrow (a view wider
// than a sheet) widens it, and only then is its text measured at its
// narrowest.
TEST(fit_sheet_measures_its_narrowest_only_for_wide_content) {
	for (int wide = 0; wide < 2; wide++) {
		fit_wide = wide, fit_min_measures = 0;
		open_cell = ak_vm_cell_new((ak_val){.i = 0}, AK_K_BOOL, NULL).p;
		setup();
		ak_closure *c = fn(fit_app, NULL);
		ak_uitest *a = ak_uitest_mount_at(c, NULL, AK_PLATFORM_MACOS, (ak_span){0});
		ak_release(c);
		ak_fake *f = ak_uitest_host(a);
		f->measure_fn = fit_measure;
		ak_vm_cell_set(open_cell, (ak_val){.i = 1});
		ak_ui_flush();
		ak_fake_view *sheet = nth(a, AK_KIND_SHEET, 0);
		REQUIRE(sheet && sheet->presented);
		ak_fake_report_sheet(f, sheet);
		ak_fake_view *t = nth(a, AK_KIND_TEXT, 0);
		REQUIRE(t);
		if (wide) {
			CHECK(fit_min_measures > 0);
			CHECK_EQ_F(t->frame[2], 820);
		} else {
			CHECK_EQ_INT(fit_min_measures, 0);
			CHECK_EQ_F(t->frame[2], f->env.width);
		}
		done(a);
		ak_release(open_cell);
	}
}

// ---- A sheet's screen (a Router's sheet route's) ----

static ak_val sheet_screen_content(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *scr = ak_ui_new(AK_EL_SCREEN);
	add(scr, text("at the top"));
	ak_view *gap = ak_ui_new(AK_EL_VIEW);
	set(gap, AK_KEY_GROW, (ak_val){.f = 1});
	add(scr, gap);
	add(scr, text("at the bottom"));
	return (ak_val){.p = scr};
}

static ak_val sheet_screen_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	ak_view *s = ak_ui_new(AK_EL_SHEET);
	bind(s, AK_KEY_SHEET_OPEN, read_cell, open_cell);
	lazy(s, sheet_screen_content, NULL);
	add(col, s);
	add(col, text("below"));
	return (ak_val){.p = col};
}

// A Screen in a sheet is as big as the sheet, whose environment the host
// reports (not the screen's): its content is laid out at the sheet's size
// (not the window's, which a sheet is shorter than), and again as the
// sheet's changes.
TEST(sheet_screen_follows_its_sheet) {
	open_cell = ak_vm_cell_new((ak_val){.i = 1}, AK_K_BOOL, NULL).p;
	ak_uitest *a = mount(sheet_screen_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *sheet = nth(a, AK_KIND_SHEET, 0);
	REQUIRE(sheet && sheet->presented);
	ak_fake_report_sheet(f, sheet);
	ak_environment e = {0};
	ak_ui_environment(&e);
	float h = e.height - e.safe_top - 10;  // the fake's sheet: below the status bar
	ak_fake_view *bottom = one(a, AK_KIND_TEXT, "at the bottom");
	REQUIRE(bottom);
	CHECK_EQ_F(bottom->frame[1] + bottom->frame[3], h);
	CHECK_EQ_F(one(a, AK_KIND_TEXT, "at the top")->frame[2], e.width);
	// The sheet grows (a detent, the keyboard): its screen with it.
	ak_environment big = e;
	big.height = h + 100;
	ak_fake_dispatch(f, &(ak_event){.id = sheet->id, .kind = AK_EVENT_ENVIRONMENT, .env = big});
	CHECK_EQ_F(bottom->frame[1] + bottom->frame[3], h + 100);
	done(a);
	ak_release(open_cell);
}

// ---- Popover (popover_test.go) ----

static ak_val pop_content(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.p = text("a fairly long line of text in the bubble")};
}

static ak_val popover_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *p = ak_ui_new(AK_EL_POPOVER);
	bind(p, AK_KEY_POPOVER_OPEN, read_cell, open_cell);
	on(p, AK_EV_POPOVER_CHANGE, write_arg, open_cell);
	ak_ui_set(p, AK_KEY_POPOVER_MAX_WIDTH, (ak_val){.f = 160});
	ak_closure *cb = fn(pop_content, NULL);
	ak_ui_slot(p, AK_KEY_POPOVER_CONTENT, (ak_view *)cb);
	ak_release(cb);
	add(p, text("anchor"));
	return (ak_val){.p = p};
}

TEST(popover) {
	open_cell = ak_vm_cell_new((ak_val){.i = 0}, AK_K_BOOL, NULL).p;
	ak_uitest *a = mount(popover_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	CHECK_EQ_INT(count_kind(a, AK_KIND_POPOVER), 0);
	ak_vm_cell_set(open_cell, (ak_val){.i = 1});
	ak_ui_flush();
	ak_fake_view *pop = nth(a, AK_KIND_POPOVER, 0);
	REQUIRE(pop && pop->presented);
	ak_fake_view *anchor = one(a, AK_KIND_TEXT, "anchor")->parent;
	CHECK_EQ_INT(ak_fake_int(pop, AK_PROP_POPOVER_ANCHOR), anchor->id);
	// As big as its content, no wider than it may be: its text wraps.
	CHECK_EQ_F(ak_fake_float(pop, AK_PROP_POPOVER_WIDTH), 160);
	CHECK(ak_fake_float(pop, AK_PROP_POPOVER_HEIGHT) > 20);
	ak_fake_dismiss(f, pop);
	CHECK_EQ_INT(peek_i(open_cell), 0);
	CHECK_EQ_INT(count_kind(a, AK_KIND_POPOVER), 0);
	done(a);
	ak_release(open_cell);
}

// ---- Alert (TestAlert, TestAlertFields) ----

static int deleted;
static ak_str *typed;

static ak_val field_changed(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c);
	if (n > 0) {
		ak_release(typed);
		typed = ak_retain(a[0].p);
	}
	return (ak_val){0};
}

static const uint8_t field_kinds[] = {AK_K_REF, AK_K_REF, AK_K_BOOL, AK_K_INT, AK_K_REF, AK_K_REF};
static const ak_shape field_shape = {"AlertField", 6, field_kinds, NULL, NULL, 0, NULL, false, NULL};

static ak_val show_alert(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_enum *items[] = {action("Cancel", AK_ALERT_BUTTON_ROLE_CANCEL, false, NULL, NULL),
	                    action("Delete", AK_ALERT_BUTTON_ROLE_DESTRUCTIVE, false, count_up, &deleted)};
	ak_arr *as = actions(items, 2);
	ak_struct *fld = ak_struct_new(&field_shape);
	fld->f[0].p = ak_str_c("draft");
	fld->f[1].p = ak_str_c("Name");
	fld->f[4].p = ak_str_empty();
	fld->f[5].p = fn(field_changed, NULL);
	ak_arr *fs = ak_arr_new(AK_K_REF, NULL, 1);
	ak_arr_push(&fs, (ak_val){.p = fld});
	ak_str *t = ak_str_c("Delete it?"), *m = ak_str_c("It can't be undone.");
	ak_ui_alert(t, m, (ak_obj *)as, false, fs);
	ak_release(t), ak_release(m), ak_release(as), ak_release(fs);
	return (ak_val){0};
}

static ak_val ok_alert(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_str *t = ak_str_c("Saved");
	ak_ui_alert(t, NULL, NULL, false, NULL);
	ak_release(t);
	return (ak_val){0};
}

static ak_val alert_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	add(col, button("Ask", show_alert, NULL));
	add(col, button("Tell", ok_alert, NULL));
	return (ak_val){.p = col};
}

TEST(alert) {
	deleted = 0;
	typed = NULL;
	ak_uitest *a = mount(alert_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	tap(a, "Ask");
	ak_fake_view *al = nth(a, AK_KIND_ALERT, 0);
	REQUIRE(al && al->presented);
	CHECK(span_is(ak_fake_string(al, AK_PROP_ALERT_TITLE), "Delete it?"));
	CHECK(span_is(ak_fake_string(al, AK_PROP_ALERT_MESSAGE), "It can't be undone."));
	check_actions(al, AK_PROP_ALERT_BUTTONS, "Cancel|Delete");
	ak_alert_field_vec fs = {0};
	ak_fake_alert_fields(al, &fs);
	CHECK_EQ_INT(fs.len, 1);
	if (fs.len) CHECK(span_is(fs.data[0].text, "draft") && span_is(fs.data[0].placeholder, "Name"));
	ak_vec_free(&fs);
	ak_fake_type_in_alert(f, al, 0, "draft 2");
	CHECK(typed && str_is(typed, "draft 2"));
	ak_fake_choose(f, al, 1);
	CHECK_EQ_INT(deleted, 1);
	CHECK_EQ_INT(count_kind(a, AK_KIND_ALERT), 0);
	// No actions: OK.
	tap(a, "Tell");
	al = nth(a, AK_KIND_ALERT, 0);
	REQUIRE(al);
	check_actions(al, AK_PROP_ALERT_BUTTONS, "OK");
	ak_fake_dismiss(f, al);
	CHECK_EQ_INT(count_kind(a, AK_KIND_ALERT), 0);
	// One showing as the app goes, goes too.
	tap(a, "Tell");
	done(a);
	ak_release(typed);
}

// ---- Menu (TestMenuAndButtonMenu) ----

static ak_val menu_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *m = ak_ui_new(AK_EL_MENU);
	ak_enum *items[] = {action("Copy", 0, false, count_up, &saved), action("Delete", 2, true, count_up, &deleted)};
	set(m, AK_KEY_MENU_ACTIONS, (ak_val){.p = actions(items, 2)});
	set_i(m, AK_KEY_MENU_LONG_PRESS, 1);
	add(m, text("hold me"));
	return (ak_val){.p = m};
}

TEST(menu) {
	saved = deleted = 0;
	ak_uitest *a = mount(menu_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *m = one(a, AK_KIND_TEXT, "hold me")->parent;
	check_actions(m, AK_PROP_MENU, "Copy|Delete(off)");
	CHECK_EQ_INT(ak_fake_int(m, AK_PROP_MENU_TRIGGER), AK_MENU_TRIGGER_LONG_PRESS);
	ak_fake_choose(f, m, 0);
	ak_fake_choose(f, m, 1);  // disabled: a host's menu out of date
	CHECK_EQ_INT(saved, 1);
	CHECK_EQ_INT(deleted, 0);
	done(a);
}

// ---- Snackbar (snackbar_test.go) ----

static int undone, snack_dismissed;
static ak_closure *dismiss_snack;

static ak_val show_snack(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	ak_enum *undo = action("Undo", 0, false, count_up, &undone);
	ak_str *m = ak_str_c((const char *)c->fn);
	ak_closure *d = fn(count_up, &snack_dismissed);
	ak_release(dismiss_snack);
	dismiss_snack = ak_ui_snackbar(m, undo, AK_SNACKBAR_DURATION_LONG, d);
	ak_release(m), ak_release(undo), ak_release(d);
	return (ak_val){0};
}

static ak_val snack_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	add(col, button("Delete", show_snack, "Deleted"));
	add(col, button("Archive", show_snack, "Archived"));
	return (ak_val){.p = col};
}

TEST(snackbar) {
	undone = snack_dismissed = 0;
	dismiss_snack = NULL;
	ak_uitest *a = mount(snack_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	tap(a, "Delete");
	ak_fake_view *s = ak_fake_snackbar(f);
	REQUIRE(s);
	CHECK(span_is(ak_fake_string(s, AK_PROP_SNACKBAR_MESSAGE), "Deleted"));
	CHECK(span_is(ak_fake_string(s, AK_PROP_SNACKBAR_ACTION), "Undo"));
	CHECK_EQ_INT(ak_fake_int(s, AK_PROP_SNACKBAR_DURATION), AK_SNACKBAR_DURATION_LONG);
	// Its action: it goes, the action runs, OnDismiss doesn't.
	ak_fake_press_snackbar_action(f, s);
	CHECK_EQ_INT(undone, 1);
	CHECK_EQ_INT(snack_dismissed, 0);
	CHECK(!ak_fake_snackbar(f));
	// A new one replaces the one showing, whose OnDismiss runs.
	tap(a, "Delete");
	tap(a, "Archive");
	CHECK_EQ_INT(snack_dismissed, 1);
	s = ak_fake_snackbar(f);
	REQUIRE(s);
	CHECK(span_is(ak_fake_string(s, AK_PROP_SNACKBAR_MESSAGE), "Archived"));
	CHECK_EQ_INT(count_kind(a, AK_KIND_SNACKBAR), 1);
	// Timing out dismisses it.
	ak_fake_time_out(f, s);
	CHECK_EQ_INT(snack_dismissed, 2);
	CHECK(!ak_fake_snackbar(f));
	// The dismiss closure; again is nothing.
	tap(a, "Delete");
	ak_val r = ak_call(dismiss_snack, NULL, 0);
	AK_UNUSED(r);
	ak_ui_flush();
	CHECK_EQ_INT(snack_dismissed, 3);
	CHECK(!ak_fake_snackbar(f));
	ak_call(dismiss_snack, NULL, 0);
	CHECK_EQ_INT(snack_dismissed, 3);
	tap(a, "Delete");  // showing as the app goes
	ak_release(dismiss_snack);
	dismiss_snack = NULL;
	done(a);
}

// ---- Boundary (catch_test.go) ----

static void *fail_cell;
static ak_closure *reset_fn;

static ak_val maybe_fail(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	if (ak_vm_cell_get(fail_cell, false).i) {
		ak_throw_msg("profile failed");
		return (ak_val){0};
	}
	return (ak_val){.p = text("profile")};
}

static ak_val do_reset(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_call(reset_fn, NULL, 0);
	return (ak_val){0};
}

static ak_val fallback(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	ak_release(reset_fn);
	reset_fn = ak_retain(a[1].p);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	ak_view *t = ak_ui_new(AK_EL_TEXT);
	ak_ui_set(t, AK_KEY_TEXT_TEXT, (ak_val){.p = ak_error_message(a[0].p)});
	add(col, t);
	add(col, button("Try again", do_reset, NULL));
	return (ak_val){.p = col};
}

static ak_val fail_later(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	if (ak_vm_cell_get(ticks, true).i > 5) {
		ak_throw_msg("too many");
		return (ak_val){0};
	}
	return (ak_val){.p = ak_str_c("ok")};
}

static ak_val boundary_content(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	ak_val v = maybe_fail(NULL, NULL, 0);
	if (ak_error_pending()) {
		ak_release(col);
		return (ak_val){0};
	}
	add(col, v.p);
	ak_view *t = ak_ui_new(AK_EL_TEXT);
	bind(t, AK_KEY_TEXT_TEXT, fail_later, NULL);
	add(col, t);
	return (ak_val){.p = col};
}

static ak_val boundary_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	add(col, text("header"));
	ak_view *b = ak_ui_new(AK_EL_BOUNDARY);
	on(b, AK_EV_BOUNDARY_ERROR, fallback, NULL);
	lazy(b, boundary_content, NULL);
	add(col, b);
	add(col, text("footer"));
	return (ak_val){.p = col};
}

TEST(boundary) {
	fail_cell = ak_vm_cell_new((ak_val){.i = 1}, AK_K_BOOL, NULL).p;
	ticks = cell_int(0);
	reset_fn = NULL;
	ak_uitest *a = mount(boundary_app, NULL);
	// Building it failed: the fallback shows, in its place.
	check_text(root(a), "header|profile failed|Try again|footer");
	// Reset builds it again.
	ak_vm_cell_set(fail_cell, (ak_val){.i = 0});
	tap(a, "Try again");
	check_text(root(a), "header|profile|ok|footer");
	// An effect in it fails later: it's disposed, the fallback shows.
	write_i(ticks, 10);
	ak_ui_flush();
	check_text(root(a), "header|too many|Try again|footer");
	write_i(ticks, 0);
	tap(a, "Try again");
	check_text(root(a), "header|profile|ok|footer");
	ak_release(reset_fn);
	reset_fn = NULL;
	done(a);
	ak_release(fail_cell), ak_release(ticks);
}

// ---- Paging and PageControl (paging_test.go) ----

static void *page_cell;

static uint32_t scroll_commands(ak_fake *f, float *x, float *y, bool *animated) {
	ak_fake_index_vec cmds = {0};
	ak_fake_commands(f, AK_CMD_SCROLL_TO_OFFSET, &cmds);
	uint32_t n = cmds.len;
	if (n) {
		ak_op op = ak_fake_op_at(f, cmds.data[n - 1]);
		memcpy(x, op.value.s.p, 4);
		memcpy(y, op.value.s.p + 4, 4);
		*animated = op.value.s.p[8] != 0;
	}
	ak_vec_free(&cmds);
	return n;
}

static ak_val paging_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	ak_ui_set(col, AK_KEY_GROW, (ak_val){.f = 1});
	ak_view *s = ak_ui_new(AK_EL_SCROLL);
	set_i(s, AK_KEY_SCROLL_HORIZONTAL, 1);
	set_i(s, AK_KEY_SCROLL_PAGING, 1);
	set_i(s, AK_KEY_HEIGHT, 200);  // points (LOWERING.md's packed lengths)
	bind(s, AK_KEY_SCROLL_PAGE, read_cell, page_cell);
	on(s, AK_EV_SCROLL_PAGE_CHANGE, write_arg, page_cell);
	add(s, text("one"));
	add(s, text("two"));
	add(s, text("three"));
	add(col, s);
	ak_view *pc = ak_ui_new(AK_EL_PAGE_CONTROL);
	set_i(pc, AK_KEY_PAGE_CONTROL_COUNT, 3);
	bind(pc, AK_KEY_PAGE_CONTROL_PAGE, read_cell, page_cell);
	on(pc, AK_EV_PAGE_CONTROL_CHANGE, write_arg, page_cell);
	add(col, pc);
	return (ak_val){.p = col};
}

TEST(paging) {
	page_cell = cell_int(0);
	ak_uitest *a = mount(paging_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *sc = nth(a, AK_KIND_SCROLL, 0), *pc = nth(a, AK_KIND_PAGE_CONTROL, 0);
	REQUIRE(sc && pc);
	ak_fake_view *two = one(a, AK_KIND_TEXT, "two");
	CHECK_EQ_F(two->frame[0], 390);
	CHECK_EQ_F(two->frame[2], 390);
	CHECK(ak_fake_bool(sc, AK_PROP_SCROLL_PAGING));
	CHECK_EQ_INT(ak_fake_int(pc, AK_PROP_PAGE_CONTROL_COUNT), 3);
	ak_fake_reset(f);
	ak_fake_dispatch(f, &(ak_event){.id = sc->id, .kind = AK_EVENT_PAGED, .i = 2});
	CHECK_EQ_INT(peek_i(page_cell), 2);
	CHECK_EQ_INT(ak_fake_int(pc, AK_PROP_PAGE_CONTROL_PAGE), 2);
	float x = 0, y = 0;
	bool an = false;
	CHECK_EQ_INT(scroll_commands(f, &x, &y, &an), 0);  // the user's page scrolls nothing
	ak_fake_select(f, pc, 1);
	CHECK_EQ_INT(scroll_commands(f, &x, &y, &an), 1);
	CHECK_EQ_F(x, 390);
	CHECK_EQ_F(y, 0);
	CHECK(an);
	done(a);
	ak_release(page_cell);
}

TEST(paging_right_to_left) {
	page_cell = cell_int(0);
	ak_ui_set_direction(true);
	ak_uitest *a = mount(paging_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *sc = nth(a, AK_KIND_SCROLL, 0);
	float x = 0, y = 0;
	bool an = true;
	CHECK_EQ_INT(scroll_commands(f, &x, &y, &an), 1);
	CHECK_EQ_F(x, 780);  // the first page right to left is the rightmost
	CHECK(!an);
	ak_fake_dispatch(f, &(ak_event){.id = sc->id, .kind = AK_EVENT_PAGED, .i = 0});  // the leftmost
	CHECK_EQ_INT(peek_i(page_cell), 2);
	done(a);
	ak_ui_set_direction(false);
	ak_release(page_cell);
}

// ---- Split (split_test.go) ----

static ak_val dest_page(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.p = text((const char *)c->fn)};
}

static ak_view *dest(const char *title, const char *section, const char *content) {
	ak_view *d = ak_ui_new(AK_EL_DESTINATION);
	set_str(d, AK_KEY_DESTINATION_TITLE, title);
	if (section) set_str(d, AK_KEY_DESTINATION_SECTION, section);
	lazy(d, dest_page, content);
	return d;
}

static ak_val split_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *s = ak_ui_new(AK_EL_SPLIT);
	set_str(s, AK_KEY_SPLIT_TITLE, "Mail");
	bind(s, AK_KEY_SPLIT_SELECTED, read_cell, sel_cell);
	on(s, AK_EV_SPLIT_CHANGE, write_arg, sel_cell);
	ak_view *in = dest("Inbox", "Mailboxes", "inbox list");
	bind(in, AK_KEY_DESTINATION_BADGE, read_cell, badge);
	add(s, in);
	add(s, dest("Sent", "Mailboxes", "sent list"));
	add(s, dest("Work", "Tags", "work list"));
	return (ak_val){.p = s};
}

TEST(split) {
	sel_cell = cell_int(0);
	badge = ak_vm_cell_new((ak_val){.p = ak_str_c("4")}, AK_K_REF, NULL).p;
	ak_uitest *a = mount(split_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *split = nth(a, AK_KIND_SPLIT, 0);
	REQUIRE(split);
	CHECK(span_is(ak_fake_string(split, AK_PROP_SPLIT_TITLE), "Mail"));
	ak_sidebar_heading_vec secs = {0};
	ak_sidebar_item_vec items = {0};
	ak_fake_sidebar(split, &secs, &items);
	CHECK_EQ_INT(secs.len, 2);
	CHECK_EQ_INT(items.len, 3);
	if (secs.len == 2 && items.len == 3) {
		CHECK(span_is(secs.data[0].title, "Mailboxes") && secs.data[0].count == 2);
		CHECK(span_is(secs.data[1].title, "Tags") && secs.data[1].count == 1);
		CHECK(span_is(items.data[0].badge, "4"));
	}
	ak_vec_free(&secs);
	ak_vec_free(&items);
	ak_fake_view *stack = nth(a, AK_KIND_STACK, 0);
	REQUIRE(stack && stack->children.len == 1);
	check_text(stack, "inbox list");
	CHECK(span_is(ak_fake_string(stack->children.data[0], AK_PROP_SCREEN_TITLE), "Inbox"));
	// Choosing another destination replaces the stack's screens at once.
	ak_fake_reset(f);
	ak_fake_select(f, split, 2);
	CHECK_EQ_INT(peek_i(sel_cell), 2);
	ak_fake_index_vec cmds = {0};
	ak_fake_commands(f, AK_CMD_REPLACE, &cmds);
	CHECK_EQ_INT(cmds.len, 1);
	ak_vec_free(&cmds);
	CHECK_EQ_INT(stack->children.len, 1);
	check_text(stack, "work list");
	// A badge follows its binding.
	ak_vm_cell_set(badge, (ak_val){.p = ak_str_c("5")});
	ak_ui_flush();
	ak_fake_sidebar(split, &secs, &items);
	if (items.len == 3) CHECK(span_is(items.data[0].badge, "5"));
	ak_vec_free(&secs);
	ak_vec_free(&items);
	done(a);
	ak_release(sel_cell), ak_release(badge);
}

// A Sidebar's destinations are in its section, unless one gives its own.
static ak_val sidebar_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *s = ak_ui_new(AK_EL_SPLIT);
	ak_view *views = ak_ui_new(AK_EL_SIDEBAR);
	set_str(views, AK_KEY_SIDEBAR_TITLE, "Views");
	add(views, dest("Text", NULL, "text page"));
	add(views, dest("Images", NULL, "images page"));
	add(s, views);
	ak_view *more = ak_ui_new(AK_EL_SIDEBAR);
	set_str(more, AK_KEY_SIDEBAR_TITLE, "More");
	add(s, more);
	add(more, dest("About", NULL, "about page"));  // after the sidebar's in the split
	add(more, dest("Help", "Help", "help page"));
	add(s, dest("Last", NULL, "last page"));
	return (ak_val){.p = s};
}

TEST(split_sidebars) {
	ak_uitest *a = mount(sidebar_app, NULL);
	ak_fake_view *split = nth(a, AK_KIND_SPLIT, 0);
	REQUIRE(split);
	ak_sidebar_heading_vec secs = {0};
	ak_sidebar_item_vec items = {0};
	ak_fake_sidebar(split, &secs, &items);
	CHECK_EQ_INT(items.len, 5);
	CHECK_EQ_INT(secs.len, 4);
	if (secs.len == 4) {
		CHECK(span_is(secs.data[0].title, "Views") && secs.data[0].count == 2);
		CHECK(span_is(secs.data[1].title, "More") && secs.data[1].count == 1);
		CHECK(span_is(secs.data[2].title, "Help") && secs.data[2].count == 1);
		CHECK(span_is(secs.data[3].title, "") && secs.data[3].count == 1);
	}
	ak_vec_free(&secs);
	ak_vec_free(&items);
	check_text(nth(a, AK_KIND_STACK, 0), "text page");
	done(a);
}

// ---- Window (window_test.go, windowhandle_test.go) ----

static int closes, asks_allowed;
static ak_view *win_ref;

static ak_val win_content(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.p = text("in the window")};
}

static ak_val window_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	add(col, text("main"));
	ak_view *w = ak_ui_new(AK_EL_WINDOW);
	bind(w, AK_KEY_WINDOW_OPEN, read_cell, open_cell);
	on(w, AK_EV_WINDOW_CHANGE, write_arg, open_cell);
	on(w, AK_EV_WINDOW_CLOSE, count_up, &closes);
	set_str(w, AK_KEY_WINDOW_TITLE, "Inspector");
	set(w, AK_KEY_WINDOW_WIDTH, (ak_val){.f = 300});
	set(w, AK_KEY_WINDOW_HEIGHT, (ak_val){.f = 400});
	set_i(w, AK_KEY_WINDOW_CENTER, 1);
	set_str(w, AK_KEY_WINDOW_STATE_KEY, "inspector");
	lazy(w, win_content, NULL);
	win_ref = ak_ui_ref(w);
	add(col, w);
	return (ak_val){.p = col};
}

TEST(window) {
	open_cell = ak_vm_cell_new((ak_val){.i = 0}, AK_K_BOOL, NULL).p;
	closes = 0;
	ak_uitest *a = mount(window_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	CHECK_EQ_INT(count_kind(a, AK_KIND_WINDOW), 0);
	// Show on a closed window opens it, through open.
	ak_ui_window_action(win_ref, AK_WINDOW_ACTION_SHOW, 0, 0);
	ak_ui_flush();
	CHECK_EQ_INT(peek_i(open_cell), 1);
	ak_fake_view *w = nth(a, AK_KIND_WINDOW, 0);
	REQUIRE(w && w->presented);
	CHECK(span_is(ak_fake_string(w, AK_PROP_WINDOW_TITLE), "Inspector"));
	ak_span cfg = ak_fake_string(w, AK_PROP_WINDOW_CONFIG);
	ak_window_config wc = ak_decode_window_config((const uint8_t *)cfg.p, cfg.len);
	CHECK_EQ_F(wc.width, 300);
	CHECK_EQ_F(wc.height, 400);
	CHECK(wc.flags & AK_WINDOW_CENTERED);
	CHECK(span_is(wc.state_key, "inspector"));
	check_text(w, "in the window");
	// Commands go to it.
	ak_fake_reset(f);
	ak_ui_window_action(win_ref, AK_WINDOW_ACTION_SIZE, 500, 600);
	ak_ui_flush();
	ak_fake_index_vec cmds = {0};
	ak_fake_commands(f, AK_CMD_WINDOW_ACTION, &cmds);
	CHECK_EQ_INT(cmds.len, 1);
	if (cmds.len) CHECK_EQ_INT(ak_fake_op_at(f, cmds.data[0]).id, w->id);
	ak_vec_free(&cmds);
	// The user closes it.
	ak_fake_dismiss(f, w);
	CHECK_EQ_INT(peek_i(open_cell), 0);
	CHECK_EQ_INT(closes, 1);
	CHECK_EQ_INT(count_kind(a, AK_KIND_WINDOW), 0);
	ak_release(win_ref);
	win_ref = NULL;
	done(a);
	ak_release(open_cell);
}

// An app without a main window (ui.main_kind none: its app block shows
// nothing outside its Windows): WindowOptions say so, hidden too, and the
// main window's commands send nothing; reloaded with a main window, its
// content's, the host's options come back.

static int main_kind;

static ak_val no_main_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_ui_main_kind(main_kind);
	return (ak_val){.p = ak_ui_new(AK_EL_COLUMN)};
}

static ak_window_config root_options(ak_fake *f, uint32_t *count) {
	ak_fake_index_vec cmds = {0};
	ak_fake_commands(f, AK_CMD_WINDOW_OPTIONS, &cmds);
	ak_window_config c = {0};
	*count = cmds.len;
	if (cmds.len) {
		ak_op op = ak_fake_op_at(f, cmds.data[cmds.len - 1]);
		c = ak_decode_window_config((const uint8_t *)op.value.s.p, op.value.s.len);
	}
	ak_vec_free(&cmds);
	return c;
}

TEST(no_main_window) {
	main_kind = AK_MAIN_NONE;
	ak_uitest *a = mount(no_main_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	uint32_t count;
	ak_window_config c = root_options(f, &count);
	CHECK_EQ_INT(count, 1);
	CHECK((c.flags & (AK_WINDOW_NONE | AK_WINDOW_HIDDEN)) == (AK_WINDOW_NONE | AK_WINDOW_HIDDEN));
	// Its handle does nothing: no command, and nothing shown.
	ak_fake_reset(f);
	ak_view *m = ak_ui_main_window_view();
	REQUIRE(m);
	ak_ui_window_action(m, AK_WINDOW_ACTION_FOCUS, 0, 0);
	ak_ui_window_action(NULL, AK_WINDOW_ACTION_SHOW, 0, 0);
	ak_ui_flush();
	ak_fake_index_vec cmds = {0};
	ak_fake_commands(f, AK_CMD_WINDOW_ACTION, &cmds);
	CHECK_EQ_INT(cmds.len, 0);
	ak_vec_free(&cmds);
	CHECK(!ak_ui_window_flag(m, AK_WINDOW_STATE_VISIBLE));
	// Its main window again, as a reload makes it: the host's options.
	ak_fake_reset(f);
	ak_ui_main_kind(AK_MAIN_CONTENT);
	ak_ui_flush();
	c = root_options(f, &count);
	CHECK_EQ_INT(count, 1);
	CHECK_EQ_INT(c.flags, 0);
	// Said once: content again sends nothing more.
	ak_fake_reset(f);
	ak_ui_main_kind(AK_MAIN_CONTENT);
	ak_ui_flush();
	root_options(f, &count);
	CHECK_EQ_INT(count, 0);
	done(a);
}

static ak_val allow_close(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.i = asks_allowed};
}

static ak_val close_ask_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_closure *h = fn(allow_close, NULL);
	ak_ui_on_close_request(h);
	ak_release(h);
	return (ak_val){.p = text("main")};
}

// The main window's close request asks; when every asker agrees, it
// closes.
TEST(main_window_close_request) {
	asks_allowed = 0;
	ak_uitest *a = mount(close_ask_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *r = root(a);
	CHECK(ak_fake_listens(r, AK_EVENT_CLOSE_REQUEST));
	ak_fake_reset(f);
	ak_fake_dispatch(f, &(ak_event){.id = r->id, .kind = AK_EVENT_CLOSE_REQUEST});
	ak_fake_index_vec cmds = {0};
	ak_fake_commands(f, AK_CMD_WINDOW_ACTION, &cmds);
	CHECK_EQ_INT(cmds.len, 0);
	asks_allowed = 1;
	ak_fake_dispatch(f, &(ak_event){.id = r->id, .kind = AK_EVENT_CLOSE_REQUEST});
	ak_fake_commands(f, AK_CMD_WINDOW_ACTION, &cmds);
	CHECK_EQ_INT(cmds.len, 1);  // Close
	ak_vec_free(&cmds);
	done(a);
}

// ---- List: sections, swipe actions, selection (TestListSwipeAndEnd, list sections) ----

static const char *const names[] = {"Ada", "Alan", "Barbara", "Bjarne", "Brian", "Claude"};
static void *sel_mask;
static int64_t selects[2];

static ak_val name_items(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_arr *r = ak_arr_new(AK_K_REF, NULL, 6);
	for (int i = 0; i < 6; i++) ak_arr_push(&r, (ak_val){.p = ak_str_c(names[i])});
	return (ak_val){.p = r};
}

static ak_val name_key(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	return (ak_val){.p = ak_retain(a[0].p)};
}

static ak_val name_row(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	ak_view *t = ak_ui_new(AK_EL_TEXT);
	ak_val v = ak_vm_acc_get(a[0].p);
	ak_ui_set(t, AK_KEY_TEXT_TEXT, v);
	ak_release(v.p);
	return (ak_val){.p = t};
}

static ak_val name_section(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	return (ak_val){.p = ak_str_new(names[a[0].i], 1)};
}

static ak_val header_text(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return ak_vm_acc_get(c->caps[0].p);
	AK_UNUSED(a);
}

static const uint8_t ref_cap[] = {AK_K_REF};

static ak_val name_header(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	ak_view *t = ak_ui_new(AK_EL_TEXT);
	ak_closure *th = ak_closure_new(header_text, NULL, 1, ref_cap);
	th->caps[0].p = ak_retain(a[0].p);
	ak_ui_bind(t, AK_KEY_TEXT_TEXT, th);
	ak_release(th);
	return (ak_val){.p = t};
}

static ak_val swipes(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	ak_enum *items[] = {action(a[0].i == 0 ? "Delete Ada" : "Delete", 2, false, count_up, &deleted)};
	return (ak_val){.p = actions(items, 1)};
}

static ak_val leading(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_enum *items[] = {action("Pin", 0, false, count_up, &saved)};
	return (ak_val){.p = actions(items, 1)};
}

static ak_val is_sel(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	return (ak_val){.i = (ak_vm_cell_get(sel_mask, true).i >> a[0].i) & 1};
}

static ak_val on_select(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	selects[0] = a[0].i, selects[1] = a[1].i;
	int64_t m = peek_i(sel_mask);
	if (a[0].i != 5) m = a[1].i ? m | (INT64_C(1) << a[0].i) : m & ~(INT64_C(1) << a[0].i);  // row 5 can't be selected
	write_i(sel_mask, m);
	return (ak_val){0};
}

static void set_fn(ak_view *v, uint16_t key, ak_invoke_fn f) {
	ak_closure *c = fn(f, NULL);
	ak_ui_set(v, key, (ak_val){.p = c});
	ak_release(c);
}

static ak_val sections_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *l = ak_ui_new(AK_EL_LIST);
	set_fn(l, AK_KEY_LIST_SECTION, name_section);
	set_fn(l, AK_KEY_LIST_HEADER, name_header);
	set_fn(l, AK_KEY_LIST_SWIPE_ACTIONS, swipes);
	set_fn(l, AK_KEY_LIST_LEADING_SWIPE_ACTIONS, leading);
	set_fn(l, AK_KEY_LIST_SELECTED, is_sel);
	set_i(l, AK_KEY_LIST_SELECTING, 1);
	on(l, AK_EV_LIST_SELECT, on_select, NULL);
	ak_closure *it = fn(name_items, NULL), *key = fn(name_key, NULL), *row = fn(name_row, NULL);
	ak_ui_lazy_for(l, it, key, row, AK_K_REF);
	ak_release(it), ak_release(key), ak_release(row);
	return (ak_val){.p = l};
}

TEST(list_sections_swipe_selection) {
	deleted = saved = 0;
	sel_mask = cell_int(1 << 2);
	ak_uitest *a = mount(sections_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *list = nth(a, AK_KIND_LIST, 0);
	REQUIRE(list);
	CHECK(ak_fake_bool(list, AK_PROP_LIST_SELECTING));
	// Three sections: A (Ada, Alan), B (three), C (one).
	CHECK_EQ_INT(list->sections.len, 3);
	if (list->sections.len == 3) {
		CHECK_EQ_INT(list->sections.data[0].start, 0);
		CHECK_EQ_INT(list->sections.data[1].start, 2);
		CHECK_EQ_INT(list->sections.data[2].start, 5);
	}
	// A header binds at -1 - its section.
	ak_fake_view *hc = ak_fake_new_cell(f);
	ak_fake_bind(f, list, -2, hc);
	check_text(hc, "B");
	ak_fake_bind(f, list, -3, hc);  // recycled for another section: it follows
	check_text(hc, "C");
	// A row: its swipe actions, and whether it's selected.
	ak_fake_view *rc = ak_fake_new_cell(f);
	ak_fake_bind(f, list, 0, rc);
	check_text(rc, "Ada");
	check_actions(rc, AK_PROP_SWIPE_ACTIONS, "Delete Ada");
	check_actions(rc, AK_PROP_LEADING_SWIPE_ACTIONS, "Pin");
	CHECK(!ak_fake_bool(rc, AK_PROP_SELECTED));
	ak_fake_choose(f, rc, 0);
	ak_fake_choose(f, rc, -1);
	CHECK_EQ_INT(deleted, 1);
	CHECK_EQ_INT(saved, 1);
	ak_fake_view *bc = ak_fake_new_cell(f);
	ak_fake_bind(f, list, 2, bc);
	CHECK(ak_fake_bool(bc, AK_PROP_SELECTED));
	// The user selects a row: select runs, and selected follows.
	ak_fake_select_row(f, rc, true);
	CHECK_EQ_INT(selects[0], 0);
	CHECK_EQ_INT(selects[1], 1);
	CHECK_EQ_INT(peek_i(sel_mask), 5);
	CHECK(ak_fake_bool(rc, AK_PROP_SELECTED));
	// What the app doesn't take goes back.
	ak_fake_view *cc = ak_fake_new_cell(f);
	ak_fake_bind(f, list, 5, cc);
	ak_fake_select_row(f, cc, true);
	CHECK(!ak_fake_bool(cc, AK_PROP_SELECTED));
	// Code's selection shows.
	write_i(sel_mask, 0);
	ak_ui_flush();
	CHECK(!ak_fake_bool(rc, AK_PROP_SELECTED));
	CHECK(!ak_fake_bool(bc, AK_PROP_SELECTED));
	done(a);
	ak_release(sel_mask);
}

// ---- Router (router_test.go) ----

static ak_buf gone;

static void note_gone(void *ctx) {
	if (gone.len) ak_buf_u8(&gone, ' ');
	ak_buf_put(&gone, ((ak_str *)ctx)->data, ((ak_str *)ctx)->len);
	ak_release(ctx);
}

// param reads a parameter of a route's Params accessor.
static ak_str *param(ak_val acc, const char *name) {
	ak_val pv = ak_vm_acc_get(acc.p);
	ak_struct *p = pv.p;
	ak_val out = {0};
	ak_str *k = ak_str_c(name);
	if (!ak_map_get(p->f[1].p, (ak_val){.p = k}, &out)) ak_map_get(p->f[2].p, (ak_val){.p = k}, &out);
	ak_release(k);
	ak_str *r = out.p ? ak_retain(out.p) : ak_str_empty();
	ak_release(p);
	return r;
}

// route_screen builds a route's screen: a Screen titled (c->fn) and the
// id parameter, showing "<title> <id>", noting its disposal.
static ak_val route_screen(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	const char *name = c->fn;
	ak_str *id = param(a[0], "id");
	char b[64];
	snprintf(b, sizeof b, "%s%s%.*s", name, id->len ? " " : "", (int)id->len, id->data);
	ak_release(id);
	ak_view *s = ak_ui_new(AK_EL_SCREEN);
	if (strcmp(name, "list") != 0 && strcmp(name, "info") != 0) {
		char t[64];
		snprintf(t, sizeof t, "Chat%s", b + strlen(name));
		set_str(s, AK_KEY_SCREEN_TITLE, t);
	}
	add(s, text(b));
	if (strcmp(name, "list") == 0) {
		ak_view *btn = button("Open 3", NULL, NULL);
		set_str(btn, AK_KEY_BUTTON_HREF, "/chat/3");
		add(s, btn);
	}
	ak_on_cleanup(note_gone, ak_str_c(b));
	return (ak_val){.p = s};
}

static ak_view *route(const char *path, ak_invoke_fn screen, const char *name) {
	ak_view *r = ak_ui_new(AK_EL_ROUTE);
	set_str(r, AK_KEY_ROUTE_PATH, path);
	if (screen) lazy(r, screen, name);
	return r;
}

static void titles(ak_fake_view *stack, const char *want) {
	ak_buf b = {0};
	for (uint32_t i = 0; i < stack->children.len; i++) {
		if (i) ak_buf_u8(&b, '|');
		ak_span t = ak_fake_string(stack->children.data[i], AK_PROP_SCREEN_TITLE);
		ak_buf_put(&b, t.p, t.len);
	}
	CHECK_EQ_STR(cstr(&b), want);
	ak_buf_free(&b);
}

static void expect_history(ak_fake *f, int32_t at, const char *want) {
	ak_fake_history *h = ak_fake_history_of(f, NULL);
	if (!h) {
		T_FAIL("no history: the router sent no Location");
		return;
	}
	ak_buf b = {0};
	ak_fake_hrefs(h, &b);
	CHECK_EQ_STR(cstr(&b), want);
	CHECK_EQ_INT(h->index, at);
	ak_buf_free(&b);
}

static void check_gone(const char *want) {
	CHECK_EQ_STR(cstr(&gone), want);
	gone.len = 0;
}

static void go(const char *href, int64_t mode) {
	ak_str *h = ak_str_c(href);
	ak_batch_begin();
	ak_ui_navigate(h, mode);
	ak_batch_end();
	ak_ui_flush();
	ak_release(h);
}

static ak_view *chats_routes(void) {
	ak_view *st = route("/", NULL, NULL);
	set_i(st, AK_KEY_ROUTE_LAYOUT, 1);  // .stack
	ak_view *list = route("", route_screen, "list");
	set_str(list, AK_KEY_ROUTE_TITLE, "Chats");
	add(st, list);
	add(st, route("chat/:id", route_screen, "chat"));
	ak_view *info = route("chat/:id/info", route_screen, "info");
	set_str(info, AK_KEY_ROUTE_TITLE, "Info");
	add(st, info);
	return st;
}

static ak_val chats_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	ak_view *r = ak_ui_new(AK_EL_ROUTER);
	if (c->fn) set_str(r, AK_KEY_ROUTER_INITIAL, c->fn);
	add(r, chats_routes());
	return (ak_val){.p = r};
}

TEST(router_navigate_push_back_replace) {
	gone.len = 0;
	ak_uitest *a = mount(chats_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *stack = nth(a, AK_KIND_STACK, 0);
	REQUIRE(stack);
	CHECK(ak_fake_bool(stack, AK_PROP_STACK_ROUTED));
	titles(stack, "Chats");
	expect_history(f, 0, "/");

	go("/chat/7", 0);
	go("/chat/7/info", 0);
	titles(stack, "Chats|Chat 7|Info");
	expect_history(f, 2, "/ /chat/7 /chat/7/info");

	// Navigate to a screen in the stack goes back to it; push pushes
	// another.
	go("/chat/7", 0);
	titles(stack, "Chats|Chat 7");
	check_gone("info 7");
	expect_history(f, 1, "/ /chat/7 /chat/7/info");
	go("/chat/7", 1);
	titles(stack, "Chats|Chat 7|Chat 7");
	expect_history(f, 2, "/ /chat/7 /chat/7");

	// Replace: the top screen goes, the history doesn't grow.
	go("/chat/8", 2);
	titles(stack, "Chats|Chat 7|Chat 8");
	check_gone("chat 7");
	expect_history(f, 2, "/ /chat/7 /chat/8");

	// The user's back gesture pops: the history goes back.
	ak_fake_back(f, stack);
	titles(stack, "Chats|Chat 7");
	check_gone("chat 8");
	expect_history(f, 1, "/ /chat/7 /chat/8");
	CHECK(ak_ui_can_go_back());
	CHECK(ak_ui_router_back());
	ak_ui_flush();
	expect_history(f, 0, "/ /chat/7 /chat/8");
	CHECK(!ak_ui_can_go_back());
	CHECK(!ak_ui_router_back());
	ak_str *h = ak_ui_router_href();
	CHECK(h && str_is(h, "/"));
	ak_release(h);

	// A Button's href goes there in the Router it's in.
	tap(a, "Open 3");
	titles(stack, "Chats|Chat 3");
	gone.len = 0;
	done(a);
	ak_buf_free(&gone);
}

// Each window's first Router has its address: a keyed Window's Router
// tells the host its route (which the host keeps under the window's key),
// apart from the main window's; the host's start is the main window's.
static ak_val two_windows_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	ak_view *r = ak_ui_new(AK_EL_ROUTER);
	add(r, chats_routes());
	add(col, r);
	ak_view *w = ak_ui_new(AK_EL_WINDOW);
	set_i(w, AK_KEY_WINDOW_OPEN, 1);
	set_str(w, AK_KEY_WINDOW_TITLE, "Chats");
	set_str(w, AK_KEY_WINDOW_STATE_KEY, "chats");
	lazy(w, chats_app, "/chat/5");
	add(col, w);
	return (ak_val){.p = col};
}

static void expect_window_history(ak_fake *f, const ak_fake_view *win, int32_t at, const char *want) {
	ak_fake_history *h = ak_fake_history_of(f, win);
	if (!h) {
		T_FAIL("no history: the window's router sent no Location");
		return;
	}
	ak_buf b = {0};
	ak_fake_hrefs(h, &b);
	CHECK_EQ_STR(cstr(&b), want);
	CHECK_EQ_INT(h->index, at);
	ak_buf_free(&b);
}

TEST(router_per_window) {
	gone.len = 0;
	ak_router_set_start(ak_cstr("/chat/9"));
	ak_uitest *a = mount(two_windows_app, NULL);
	ak_router_set_start(ak_cstr(""));
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *win = nth(a, AK_KIND_WINDOW, 0);
	REQUIRE(win && win->presented);
	// The main window starts where the host said; the Window at its own.
	expect_history(f, 0, "/chat/9");
	expect_window_history(f, win, 0, "/chat/5");
	// The Window's link goes in its Router, and only its history moves.
	ak_fake_view *wstack = NULL;
	ak_fake_view_vec stacks = {0};
	ak_fake_of_kind(f, AK_KIND_STACK, &stacks);
	for (uint32_t i = 0; i < stacks.len; i++)
		for (const ak_fake_view *p = stacks.data[i]; p; p = p->parent)
			if (p == win) wstack = stacks.data[i];
	ak_vec_free(&stacks);
	REQUIRE(wstack);
	titles(wstack, "Chats|Chat 5");
	ak_fake_back(f, wstack);
	titles(wstack, "Chats");
	// The link built its stack as one entry: back replaces it.
	expect_window_history(f, win, 0, "/");
	expect_history(f, 0, "/chat/9");
	// The host restores the Window's route (kept under its key): a
	// Location event to the Window's view goes to its Router alone.
	ak_buf ev = {0};
	ak_append_location_event(&ev, ak_cstr("/chat/3"), ak_cstr(""));
	ak_fake_dispatch(f, &(ak_event){.id = win->id, .kind = AK_EVENT_LOCATION, .s = ak_strn(ev.data, ev.len)});
	ak_buf_free(&ev);
	ak_ui_flush();
	titles(wstack, "Chats|Chat 3");
	expect_history(f, 0, "/chat/9");
	// Code outside any Router navigates the main window's.
	go("/chat/7", 0);
	expect_history(f, 1, "/chat/9 /chat/7");
	expect_window_history(f, win, 1, "/ /chat/3");
	gone.len = 0;
	done(a);
	ak_buf_free(&gone);
}

// A link builds the stack: the screens up its path under it.
TEST(router_deep_link_builds_the_stack) {
	gone.len = 0;
	ak_uitest *a = mount(chats_app, "/chat/7/info");
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *stack = nth(a, AK_KIND_STACK, 0);
	REQUIRE(stack);
	titles(stack, "Chats|Chat 7|Info");
	expect_history(f, 0, "/chat/7/info");
	check_text(stack->children.data[2], "info 7");
	// A link the app is opened with goes to it: pushed, as navigate does.
	ak_fake_open_url(f, "myapp://chat/9");
	titles(stack, "Chats|Chat 7|Info|Chat 9");
	ak_fake_open_url(f, "https://example.com/chat/7?from=mail");  // back to it, its query given
	titles(stack, "Chats|Chat 7");
	gone.len = 0;
	done(a);
	ak_buf_free(&gone);
}

// The browser's back and forward go to the entries the router made.
TEST(router_browser_back_and_forward) {
	gone.len = 0;
	ak_uitest *a = mount(chats_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *stack = nth(a, AK_KIND_STACK, 0);
	go("/chat/7", 0);
	go("/chat/7/info", 0);
	CHECK(ak_fake_go(f, NULL, -1));
	titles(stack, "Chats|Chat 7");
	expect_history(f, 1, "/ /chat/7 /chat/7/info");
	CHECK(ak_fake_go(f, NULL, 1));
	titles(stack, "Chats|Chat 7|Info");
	expect_history(f, 2, "/ /chat/7 /chat/7/info");
	gone.len = 0;
	done(a);
	ak_buf_free(&gone);
}

static ak_val tab_screen(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	ak_view *s = ak_ui_new(AK_EL_SCREEN);
	add(s, text(c->fn));
	return (ak_val){.p = s};
}

static ak_val tabs_router(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *r = ak_ui_new(AK_EL_ROUTER);
	ak_view *red = route("/", NULL, NULL);
	set_str(red, AK_KEY_ROUTE_REDIRECT, "/chats");
	add(r, red);
	ak_view *tabs = route("/", NULL, NULL);
	set_i(tabs, AK_KEY_ROUTE_LAYOUT, 2);  // .tabs
	ak_view *chats = chats_routes();
	set_str(chats, AK_KEY_ROUTE_PATH, "/chats");
	set_str(chats, AK_KEY_ROUTE_TITLE, "Chats");
	set_str(chats, AK_KEY_ROUTE_ICON, "message");
	add(tabs, chats);
	ak_view *settings = route("/settings", tab_screen, "settings page");
	set_str(settings, AK_KEY_ROUTE_TITLE, "Settings");
	add(tabs, settings);
	ak_view *sheet = route("/compose", tab_screen, "compose page");
	set_i(sheet, AK_KEY_ROUTE_SHEET, 1);
	add(tabs, sheet);
	add(r, tabs);
	return (ak_val){.p = r};
}

TEST(router_tabs_sheets_and_redirects) {
	gone.len = 0;
	ak_uitest *a = mount(tabs_router, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_view *tabs = nth(a, AK_KIND_TABS, 0);
	REQUIRE(tabs && tabs->children.len == 2);
	CHECK(span_is(ak_fake_string(tabs->children.data[0], AK_PROP_SCREEN_TITLE), "Chats"));
	CHECK_EQ_INT(ak_fake_int(tabs, AK_PROP_TABS_SELECTED), 0);
	ak_fake_view *stack = nth(a, AK_KIND_STACK, 0);
	REQUIRE(stack);
	titles(stack, "Chats");
	expect_history(f, 0, "/chats");  // "/" redirects
	// To another tab's screen: the tab is chosen.
	go("/settings", 0);
	CHECK_EQ_INT(ak_fake_int(tabs, AK_PROP_TABS_SELECTED), 1);
	check_text(tabs->children.data[1], "settings page");
	expect_history(f, 1, "/chats /settings");
	// The user's choice of a tab is the router's.
	ak_fake_select(f, tabs, 0);
	expect_history(f, 0, "/chats /settings");
	go("/chats/chat/4", 0);
	titles(stack, "Chats|Chat 4");
	// A sheet's route presents it over the tabs; the user's dismissal goes
	// back.
	go("/compose", 0);
	ak_fake_view *sheet = nth(a, AK_KIND_SHEET, 0);
	REQUIRE(sheet && sheet->presented);
	CHECK(ak_fake_bool(sheet, AK_PROP_SHEET_ROUTED));
	check_text(sheet, "compose page");
	ak_str *h = ak_ui_router_href();
	CHECK(str_is(h, "/compose"));
	ak_release(h);
	ak_fake_dismiss(f, sheet);
	CHECK_EQ_INT(count_kind(a, AK_KIND_SHEET), 0);
	h = ak_ui_router_href();
	CHECK(str_is(h, "/chats/chat/4"));
	ak_release(h);
	// dismiss on the router, likewise.
	go("/compose", 0);
	CHECK_EQ_INT(count_kind(a, AK_KIND_SHEET), 1);
	CHECK(ak_ui_router_dismiss());
	ak_ui_flush();
	CHECK_EQ_INT(count_kind(a, AK_KIND_SHEET), 0);
	gone.len = 0;
	done(a);
	ak_buf_free(&gone);
}

static void *signed_in;

static ak_val guard(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	if (ak_vm_cell_get(signed_in, false).i) return (ak_val){.p = ak_str_empty()};
	ak_str *to = a[0].p;
	ak_buf b = {0};
	ak_buf_put(&b, "/login?next=", 12);
	ak_buf_put(&b, to->data, to->len);
	ak_str *r = ak_str_new((const char *)b.data, b.len);
	ak_buf_free(&b);
	return (ak_val){.p = r};
}

static ak_val guarded_router(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *r = ak_ui_new(AK_EL_ROUTER);
	set_str(r, AK_KEY_ROUTER_INITIAL, "/admin/users");
	ak_view *slot = route("/", NULL, NULL);
	set_i(slot, AK_KEY_ROUTE_LAYOUT, 4);  // .slot
	add(slot, route("/login", tab_screen, "login page"));
	ak_view *admin = route("/admin", NULL, NULL);
	set_i(admin, AK_KEY_ROUTE_LAYOUT, 1);
	ak_closure *g = fn(guard, NULL);
	ak_ui_set(admin, AK_KEY_ROUTE_GUARD, (ak_val){.p = g});
	ak_release(g);
	add(admin, route("users", tab_screen, "users page"));
	add(slot, admin);
	ak_view *old = route("/people", NULL, NULL);
	set_str(old, AK_KEY_ROUTE_REDIRECT, "/admin/users");
	add(slot, old);
	add(r, slot);
	return (ak_val){.p = r};
}

TEST(router_guards_redirects_not_found) {
	signed_in = ak_vm_cell_new((ak_val){.i = 0}, AK_K_BOOL, NULL).p;
	ak_uitest *a = mount(guarded_router, NULL);
	ak_fake *f = ak_uitest_host(a);
	// Signed out: the guard sends the user to the login screen.
	check_text(root(a), "login page");
	expect_history(f, 0, "/login?next=/admin/users");
	ak_vm_cell_set(signed_in, (ak_val){.i = 1});
	go("/people", 0);  // redirects, and the guard lets them in
	check_text(root(a), "users page");
	ak_str *h = ak_ui_router_href();
	CHECK(str_is(h, "/admin/users"));
	ak_release(h);
	// No route takes it: the not found screen, with a way home.
	go("/nope/x", 0);
	check_text(root(a), "This screen doesn't exist.|/nope/x|Go home");
	done(a);
	ak_release(signed_in);
}

TEST(href_of) {
	setup();
	ak_str *p = ak_str_c("/chat/:id/files/*path");
	ak_arr *v = ak_arr_new(AK_K_REF, NULL, 2);
	ak_arr_push(&v, (ak_val){.p = ak_str_c("a b")});
	ak_arr_push(&v, (ak_val){.p = ak_str_c("docs/a.md")});
	ak_str *h = ak_ui_href_of(p, v);
	CHECK(h && str_is(h, "/chat/a%20b/files/docs/a.md"));
	ak_release(h);
	ak_arr *one = ak_arr_new(AK_K_REF, NULL, 1);
	ak_arr_push(&one, (ak_val){.p = ak_str_c("7")});
	CHECK(!ak_ui_href_of(p, one));  // a value short
	CHECK(ak_error_pending());
	ak_release(ak_error_take());
	ak_release(p), ak_release(v), ak_release(one);
}

// ---- Focus out of an overlay whose dismissal pops its screen ----

static ak_val ov_field(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *f = ak_ui_new(AK_EL_TEXT_FIELD);
	set_str(f, AK_KEY_TEXT_FIELD_PLACEHOLDER, "inside");
	return (ak_val){.p = f};
}

static ak_val ov_screen(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *s = ak_ui_new(AK_EL_SCREEN);
	set_str(s, AK_KEY_SCREEN_TITLE, "Pick");
	ak_view *outside = ak_ui_new(AK_EL_TEXT_FIELD);
	set_str(outside, AK_KEY_TEXT_FIELD_PLACEHOLDER, "outside");
	add(s, outside);
	ak_view *ov = ak_ui_new(AK_EL_OVERLAY);
	set_i(ov, AK_KEY_OVERLAY_OPEN, 1);
	on(ov, AK_EV_OVERLAY_DISMISS, do_pop, NULL);
	lazy(ov, ov_field, NULL);
	add(s, ov);
	return (ak_val){.p = s};
}

static ak_val ov_home(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *s = ak_ui_new(AK_EL_SCREEN);
	set_str(s, AK_KEY_SCREEN_TITLE, "Home");
	add(s, button("Open", do_push, (const void *)ov_screen));
	return (ak_val){.p = s};
}

static ak_val ov_stack_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *st = ak_ui_new(AK_EL_STACK);
	lazy(st, ov_home, NULL);
	return (ak_val){.p = st};
}

// The focus moves from a field in an overlay to one outside it; the
// overlay's dismissal pops its screen there and then, the field that had
// the focus with it: it's told it lost the focus all the same, not
// freed under the telling.
TEST(focus_out_of_an_overlay_that_pops_its_screen) {
	ak_uitest *a = mount(ov_stack_app, NULL);
	tap(a, "Open");
	REQUIRE(count_kind(a, AK_KIND_TEXT_FIELD) == 2);
	ak_fake_view *outside = NULL, *inside = NULL;
	for (uint32_t i = 0; i < 2; i++) {
		ak_fake_view *v = nth(a, AK_KIND_TEXT_FIELD, i);
		if (span_is(ak_fake_string(v, AK_PROP_TEXT_FIELD_PLACEHOLDER), "inside")) inside = v;
		else outside = v;
	}
	REQUIRE(inside && outside);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_dispatch(f, &(ak_event){.id = inside->id, .kind = AK_EVENT_FOCUS});
	ak_fake_dispatch(f, &(ak_event){.id = outside->id, .kind = AK_EVENT_FOCUS});
	CHECK_EQ_INT(count_kind(a, AK_KIND_SCREEN), 1);
	CHECK_EQ_INT(count_kind(a, AK_KIND_TEXT_FIELD), 0);
	done(a);
}

// ---- Events from a host that sends anything (fuzz.h) ----

TEST(events_from_a_host_that_sends_anything) {
	ak_invoke_fn apps[] = {stack_app, tabs_app, sheet_app, popover_app, alert_app, menu_app, snack_app, ov_stack_app, sections_app};
	int per_app = fuzz_seed(0x9e3779b97f4a7c15, 3000);
	for (size_t i = 0; i < AK_LEN(apps); i++) {
		ticks = cell_int(0);
		sel_cell = cell_int(0);
		badge = ak_vm_cell_new((ak_val){.p = ak_str_c("3")}, AK_K_REF, NULL).p;
		open_cell = ak_vm_cell_new((ak_val){.i = 0}, AK_K_BOOL, NULL).p;
		sel_mask = cell_int(1 << 2);
		typed = NULL;
		dismiss_snack = NULL;
		ak_uitest *a = mount(apps[i], NULL);
		fuzz_events(a, per_app);
		ak_release(dismiss_snack);  // a snackbar's dismissal holds its view
		dismiss_snack = NULL;
		done(a);
		ak_release(ticks), ak_release(sel_cell), ak_release(badge), ak_release(open_cell), ak_release(sel_mask);
		ak_release(typed);
	}
}

T_MAIN()
