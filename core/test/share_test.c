#define _POSIX_C_SOURCE 200809L  // setenv, unsetenv
// What the app asks of the system and hears from it outside its views
// (share.c): share, haptics, the clipboard, URLs, announcements, native
// modules' calls and their results, the URLs and files the app is opened
// with and what other apps share with it. Ports of ui_test.go's calls and
// share tests.
#include "uih.h"

static char log_buf[512];

static void logs(const char *s) {
	size_t n = strlen(log_buf);
	snprintf(log_buf + n, sizeof log_buf - n, "%s%s", n ? "|" : "", s);
}

static ak_view *target;

static ak_val plain_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	ak_view *t = text("Saved");
	target = ak_ui_ref(t);
	add(col, t);
	return (ak_val){.p = col};
}

static ak_span cmd_args(ak_fake *f, ak_cmd c, ak_id *on) {
	ak_fake_index_vec cmds = {0};
	ak_fake_commands(f, c, &cmds);
	ak_span s = {0};
	if (cmds.len == 1) {
		ak_op op = ak_fake_op_at(f, cmds.data[0]);
		s = op.value.s;
		if (on) *on = op.id;
	} else {
		T_FAIL("%u commands %u", cmds.len, c);
	}
	ak_vec_free(&cmds);
	return s;
}

// Each goes to the host as its command, on the root's view (a screen
// reader's focus on the view's).
TEST(system_commands) {
	ak_uitest *a = mount(plain_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_reset(f);
	ak_str *s = ak_str_c("https://asuka.dev");
	ak_ui_share(s);
	ak_ui_copy(s);
	ak_ui_open_url(s);
	ak_ui_announce(s);
	ak_release(s);
	ak_ui_haptic(AK_HAPTIC_SUCCESS);
	ak_ui_haptic(99);  // not one: nothing
	ak_ui_a11y_focus(target);
	ak_ui_flush();
	ak_id rid = root(a)->id, on = 0;
	CHECK(ak_span_eqc(cmd_args(f, AK_CMD_SHARE, &on), "https://asuka.dev") && on == rid);
	CHECK(ak_span_eqc(cmd_args(f, AK_CMD_COPY, NULL), "https://asuka.dev"));
	CHECK(ak_span_eqc(cmd_args(f, AK_CMD_OPEN_URL, NULL), "https://asuka.dev"));
	CHECK(ak_span_eqc(cmd_args(f, AK_CMD_ANNOUNCE, NULL), "https://asuka.dev"));
	ak_span h = cmd_args(f, AK_CMD_HAPTIC, NULL);
	CHECK(h.len == 1 && h.p[0] == AK_HAPTIC_SUCCESS);
	cmd_args(f, AK_CMD_A11Y_FOCUS, &on);
	CHECK(on == one(a, AK_KIND_TEXT, "Saved")->id);
	CHECK_EQ_INT(ak_ui_view_id(target), on);
	ak_release(target);
	done(a);
}

// The app's own share sheet (platform/sharesheet): its closes are the
// ShareClose command to the root, how a u8; what isn't one sends nothing.
// The process the app runs in is the host's to say, in the environment.
TEST(share_sheet) {
	ak_uitest *a = mount(plain_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_reset(f);
	ak_ui_share_close(2);
	ak_ui_share_close(7);
	ak_ui_flush();
	ak_id on = 0;
	ak_span h = cmd_args(f, AK_CMD_SHARE_CLOSE, &on);
	CHECK(h.len == 1 && h.p[0] == 2 && on == root(a)->id);
	ak_release(target);
	done(a);
	CHECK_EQ_STR(ak_ui_process(), "app");
#if !defined(_WIN32)
	setenv("ASUKA_NOTIFICATION_SERVICE", "1", 1);
	CHECK_EQ_STR(ak_ui_process(), "notification");
	setenv("ASUKA_SHARE_SHEET", "1", 1);
	CHECK_EQ_STR(ak_ui_process(), "share");
	unsetenv("ASUKA_SHARE_SHEET");
	unsetenv("ASUKA_NOTIFICATION_SERVICE");
#endif
}

// The app's accent (ui.set_accent, from its init) goes to the host as a
// root mounts, before its views, with its dark mode's; a reload that
// changes either tells the host again, one that doesn't sends nothing.
static void set_accent(uint64_t color, uint8_t flags, uint64_t dark) {
	ak_val a[3] = {{.i = (int64_t)color}, {.i = flags}, {.i = (int64_t)dark}};
	ak_builtin_find("ui.set_accent")(a, 3);
}

static uint64_t accent_of(ak_span s, uint32_t at) {
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--) v = v << 8 | (uint8_t)s.p[at + (uint32_t)i];
	return v;
}

TEST(accent) {
	uint64_t purple = 2ull << 32 | 0x7c3aedffu, lavender = 2ull << 32 | 0xb197fcffu;
	set_accent(purple, 1, lavender);
	ak_uitest *a = mount(plain_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_span s = cmd_args(f, AK_CMD_ACCENT, NULL);
	CHECK(s.len == 17 && accent_of(s, 0) == purple && s.p[8] == 1 && accent_of(s, 9) == lavender);
	// Before any view: the first op that isn't the root's.
	ak_fake_index_vec cmds = {0};
	ak_fake_commands(f, AK_CMD_ACCENT, &cmds);
	for (uint32_t i = 0; i < cmds.data[0]; i++) CHECK(ak_fake_op_at(f, i).code != AK_OP_CREATE || ak_fake_op_at(f, i).id == root(a)->id);
	ak_vec_free(&cmds);
	ak_release(target);
	ak_fake_reset(f);
	set_accent(purple, 1, lavender);  // a reload with the same one
	ak_ui_flush();
	ak_fake_commands(f, AK_CMD_ACCENT, &cmds);
	CHECK_EQ_INT(cmds.len, 0);
	ak_vec_free(&cmds);
	set_accent(purple, 1, 0);  // a reload without the dark one: the same in both
	ak_ui_flush();
	s = cmd_args(f, AK_CMD_ACCENT, NULL);
	CHECK(s.len == 17 && accent_of(s, 0) == purple && accent_of(s, 9) == 0);
	ak_fake_reset(f);
	set_accent(0, 0, 0);  // a reload of an app with none: the platform's again
	ak_ui_flush();
	s = cmd_args(f, AK_CMD_ACCENT, NULL);
	CHECK(s.len == 17 && accent_of(s, 0) == 0 && s.p[8] == 0 && accent_of(s, 9) == 0);
	done(a);
	// With none, a root sends nothing.
	a = mount(plain_app, NULL);
	ak_fake_commands(ak_uitest_host(a), AK_CMD_ACCENT, &cmds);
	CHECK_EQ_INT(cmds.len, 0);
	ak_vec_free(&cmds);
	ak_release(target);
	done(a);
}

// A preview's module (asuka dev's: ui.preview, in its init) shows dark
// over the app's own color scheme, in its text size (DevFontScale) and
// direction, in a frame of its size in the middle of the window; a
// module whose init says nothing (the app, again) shows as the app does:
// the app's color scheme back, the host's own text size, no frame.
static void preview(int64_t scheme, double font, const char *lang, const char *dir, double w, double h) {
	ak_val a[7] = {{.p = ak_str_c("Dark")}, {.i = scheme}, {.f = font}, {.p = ak_str_c(lang)}, {.p = ak_str_c(dir)}, {.f = w}, {.f = h}};
	ak_builtin_find("ui.preview")(a, 7);
	ak_release(a[0].p);
	ak_release(a[3].p);
	ak_release(a[4].p);
}

static ak_val row_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *row = ak_ui_new(AK_EL_ROW);
	add(row, text("A"));
	add(row, text("B"));
	return (ak_val){.p = row};
}

// A preview right to left after the app left to right (asuka dev's
// reloads, the direction changed while no root was mounted): its row
// mirrored, in its frame; the app again, left to right.
TEST(preview_rtl_after_the_app) {
	ak_uitest *a = mount(row_app, NULL);
	CHECK(one(a, AK_KIND_TEXT, "A")->frame[0] < one(a, AK_KIND_TEXT, "B")->frame[0]);
	done(a);
	ak_preview_begin();
	preview(AK_COLOR_SCHEME_SYSTEM, 0, "", "rtl", 200, 100);
	ak_preview_end();
	a = mount(row_app, NULL);
	CHECK(one(a, AK_KIND_TEXT, "A")->frame[0] > one(a, AK_KIND_TEXT, "B")->frame[0]);
	done(a);
	ak_preview_begin();
	ak_preview_end();
	a = mount(row_app, NULL);
	CHECK(one(a, AK_KIND_TEXT, "A")->frame[0] < one(a, AK_KIND_TEXT, "B")->frame[0]);
	done(a);
}

TEST(preview) {
	ak_ui_color_scheme(AK_COLOR_SCHEME_LIGHT);  // the app's own (setColorScheme)
	ak_preview_begin();
	preview(AK_COLOR_SCHEME_DARK, 1.5, "", "rtl", 200, 100);
	ak_preview_end();
	CHECK(ak_preview_active());
	ak_uitest *a = mount(plain_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_span s = cmd_args(f, AK_CMD_COLOR_SCHEME, NULL);
	CHECK(s.len == 1 && s.p[0] == AK_COLOR_SCHEME_DARK);
	s = cmd_args(f, AK_CMD_DEV_FONT_SCALE, NULL);
	float fs = 0;
	if (s.len == 4) memcpy(&fs, s.p, 4);
	CHECK(s.len == 4 && fs == 1.5f);
	CHECK(ak_locale_rtl());
	ak_str *path = ak_ui_path();  // its views' paths start with its name
	CHECK(ak_span_eqc(ak_span_str(path), "Dark#0"));
	ak_release(path);
	// The frame is the layout's alone: its column, in its place.
	check_frame(one(a, AK_KIND_TEXT, "Saved")->parent, 95, 372, 200, 100);
	ak_release(target);
	done(a);
	// The app again.
	ak_preview_begin();
	ak_preview_end();
	CHECK(!ak_preview_active());
	a = mount(plain_app, NULL);
	f = ak_uitest_host(a);
	s = cmd_args(f, AK_CMD_COLOR_SCHEME, NULL);
	CHECK(s.len == 1 && s.p[0] == AK_COLOR_SCHEME_LIGHT);
	ak_fake_index_vec cmds = {0};
	ak_fake_commands(f, AK_CMD_DEV_FONT_SCALE, &cmds);
	CHECK_EQ_INT(cmds.len, 0);  // the host's own, which it never left (a new host)
	ak_vec_free(&cmds);
	CHECK(!ak_locale_rtl());
	check_frame(one(a, AK_KIND_TEXT, "Saved")->parent, 0, 0, 390, 844);
	ak_release(target);
	done(a);
	ak_ui_color_scheme(AK_COLOR_SCHEME_SYSTEM);
}

// ---- Calls ----

static bool answer(void *ctx, ak_span module, ak_span method, ak_span args, ak_buf *out) {
	AK_UNUSED(ctx);
	if (ak_span_eqc(module, "secureStore") && ak_span_eqc(method, "get")) {
		ak_buf_put(out, "v:", 2);
		ak_buf_put(out, args.p, args.len);
		return true;
	}
	ak_buf_printf(out, "no method %.*s", (int)method.len, method.p);
	return false;
}

// A module's call goes as Call on the root's view; its Result resolves
// its future, with its bytes or its error.
TEST(module_calls) {
	ak_uitest *a = mount(plain_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	f->call_fn = answer;
	ak_str *m = ak_str_c("secureStore"), *get = ak_str_c("get"), *del = ak_str_c("delete");
	ak_bytes *args = ak_bytes_new("token", 5);
	ak_future *ok = ak_ui_call(m, get, args), *bad = ak_ui_call(m, del, args);
	ak_ui_flush();
	ak_uitest_settle(a);
	CHECK(ok->state == AK_FUT_DONE);
	if (ok->state == AK_FUT_DONE) {
		ak_bytes *b = ok->result.p;
		CHECK(b->len == 7 && memcmp(b->data, "v:token", 7) == 0);
	}
	CHECK(bad->state == AK_FUT_FAILED && strcmp(ak_error_message(bad->error)->data, "no method delete") == 0);
	ak_release(ok), ak_release(bad);
	// A call its root goes before answering fails.
	f->call_fn = NULL;
	ak_future *lost = ak_ui_call(m, get, args);
	ak_ui_flush();
	ak_release(target);
	ak_buf err = {0};
	ak_uitest_unmount(a, &err);
	ak_buf_free(&err);
	CHECK(lost->state == AK_FUT_FAILED);
	ak_release(lost);
	ak_release(a);
	// Without a root, a call fails at once.
	ak_future *none = ak_ui_call(m, get, NULL);
	CHECK(none->state == AK_FUT_FAILED);
	ak_release(none);
	ak_release(m), ak_release(get), ak_release(del), ak_release(args);
}

// ---- What the app is opened with, and what's shared with it ----

static ak_val log_url(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	char s[160];
	snprintf(s, sizeof s, "url:%s", ((ak_str *)a[0].p)->data);
	logs(s);
	return noval();
}

static ak_val log_files(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	const ak_arr *ps = a[0].p;
	char s[160] = "files:";
	for (uint32_t i = 0; i < ps->len; i++) {
		size_t k = strlen(s);
		snprintf(s + k, sizeof s - k, "%s%s", i ? "," : "", ((ak_str *)ak_arr_vals(ps)[i].p)->data);
	}
	logs(s);
	return noval();
}

static ak_val log_share(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(n);
	const ak_arr *items = a[0].p;
	char s[160] = "share:";
	for (uint32_t i = 0; i < items->len; i++) {
		const ak_struct *it = ak_arr_vals(items)[i].p;
		size_t k = strlen(s);
		snprintf(s + k, sizeof s - k, "%s%lld %s%s", i ? "," : "", (long long)it->f[0].i, ((ak_str *)it->f[1].p)->data, ((ak_str *)it->f[2].p)->data);
	}
	logs(s);
	return noval();
}

static void *listening;

static ak_val listener(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_closure *u = fn(log_url, NULL), *fs = fn(log_files, NULL), *sh = fn(log_share, NULL);
	ak_ui_on_open_url(u);
	ak_ui_on_open_files(fs);
	ak_ui_on_share(sh);
	ak_release(u), ak_release(fs), ak_release(sh);
	return (ak_val){.p = text("listening")};
}

static ak_val opened_app(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c), AK_UNUSED(a), AK_UNUSED(n);
	ak_view *col = ak_ui_new(AK_EL_COLUMN);
	ak_closure *cond = fn(read_cell, listening), *then = fn(listener, NULL);
	add(col, ak_ui_show(cond, then, NULL));
	ak_release(cond), ak_release(then);
	return (ak_val){.p = col};
}

static void share_items(ak_uitest *a) {
	ak_item items[] = {
		{.kind = AK_SHARE_KIND_LINK, .text = AK_STR("https://asuka.dev"), .mime = AK_STR("text/uri-list"), .size = -1},
		{.kind = AK_SHARE_KIND_IMAGE, .path = AK_STR("/caches/a.jpg"), .name = AK_STR("a.jpg"), .mime = AK_STR("image/jpeg"), .size = 9},
	};
	ak_buf b = {0};
	ak_append_items(&b, items, 2);
	dispatch(a, (ak_event){.id = root(a)->id, .kind = AK_EVENT_SHARED, .s = ak_strn(b.data, b.len)});
	ak_buf_free(&b);
}

// A URL, files or a share that come before any handler wait for the
// first; then each goes to the handlers there are, until their owner goes.
TEST(opened_with_and_shared) {
	log_buf[0] = 0;
	listening = ak_vm_cell_new((ak_val){.i = 0}, AK_K_BOOL, NULL).p;
	ak_uitest *a = mount(opened_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	ak_fake_open_url(f, "myapp://launch");
	ak_fake_open_url(f, "file:///Users/me/a%20b.txt\nfile://localhost/tmp/c.md");
	share_items(a);
	CHECK_EQ_STR(log_buf, "");
	write_i(listening, 1);
	ak_ui_flush();
	CHECK_EQ_STR(log_buf, "url:myapp://launch|files:/Users/me/a b.txt,/tmp/c.md|share:1 https://asuka.dev,2 /caches/a.jpg");
	log_buf[0] = 0;
	ak_fake_open_url(f, "https://asuka.dev/docs");
	CHECK_EQ_STR(log_buf, "url:https://asuka.dev/docs");
	write_i(listening, 0);
	ak_ui_flush();
	log_buf[0] = 0;
	ak_fake_open_url(f, "myapp://later");  // no handler: kept for the next
	CHECK_EQ_STR(log_buf, "");
	done(a);
	ak_release(listening);
}

// shareFiles: the view it points at, the type from the first file, the
// files' paths, as a call to the share module.
static bool share_module(void *ctx, ak_span module, ak_span method, ak_span args, ak_buf *out) {
	AK_UNUSED(ctx), AK_UNUSED(out);
	if (!ak_span_eqc(module, "share") || !ak_span_eqc(method, "files")) return false;
	ak_reader r = {(const uint8_t *)args.p, args.len, 0, false};
	uint32_t from = ak_read_u32(&r);
	ak_span mime = ak_read_str(&r), uti = ak_read_str(&r), title = ak_read_str(&r);
	uint32_t n = ak_read_u32(&r);
	ak_span p0 = ak_read_str(&r);
	char s[200];
	snprintf(s, sizeof s, "files:%u:%.*s:%.*s:%.*s:%u:%.*s", from != 0, (int)mime.len, mime.p, (int)uti.len, uti.p, (int)title.len, title.p, n,
	         (int)p0.len, p0.p);
	logs(s);
	return true;
}

TEST(share_files) {
	log_buf[0] = 0;
	ak_uitest *a = mount(plain_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	f->call_fn = share_module;
	ak_arr *paths = ak_arr_new(AK_K_REF, NULL, 2);
	ak_arr_push(&paths, (ak_val){.p = ak_str_c("/docs/report.pdf")});
	ak_arr_push(&paths, (ak_val){.p = ak_str_c("/docs/b.pdf")});
	ak_str *empty = ak_str_c(""), *title = ak_str_c("Report");
	ak_future *fut = ak_ui_share_files(paths, empty, empty, title, target);
	ak_ui_flush();
	ak_uitest_settle(a);
	CHECK(fut->state == AK_FUT_DONE);
	CHECK_EQ_STR(log_buf, "files:1:application/pdf::Report:2:/docs/report.pdf");
	ak_release(fut);
	ak_arr *none = ak_arr_new(AK_K_REF, NULL, 0);
	fut = ak_ui_share_files(none, empty, empty, title, NULL);
	CHECK(fut->state == AK_FUT_FAILED);
	ak_release(fut);
	ak_release(none), ak_release(paths), ak_release(empty), ak_release(title);
	ak_release(target);
	done(a);
}

// capture: the view's native view and its rect, the format, quality and
// scale, as a call to the view module, whose answer is the file's path.
static bool view_module(void *ctx, ak_span module, ak_span method, ak_span args, ak_buf *out) {
	AK_UNUSED(ctx);
	if (!ak_span_eqc(module, "view") || !ak_span_eqc(method, "capture")) return false;
	ak_reader r = {(const uint8_t *)args.p, args.len, 0, false};
	uint32_t id = ak_read_u32(&r);
	float x = ak_read_f32(&r), y = ak_read_f32(&r), w = ak_read_f32(&r), h = ak_read_f32(&r);
	uint8_t format = ak_read_u8(&r);
	float quality = ak_read_f32(&r), scale = ak_read_f32(&r);
	char s[200];
	snprintf(s, sizeof s, "capture:%u:%g,%g,%g,%g:%u:%g:%g", id != 0, x, y, w > 0 ? 1.0 : w, h > 0 ? 1.0 : h, format, quality, scale);
	logs(s);
	ak_buf_put(out, "/caches/view.png", 16);
	return true;
}

TEST(capture) {
	log_buf[0] = 0;
	ak_uitest *a = mount(plain_app, NULL);
	ak_fake *f = ak_uitest_host(a);
	f->call_fn = view_module;
	ak_future *fut = ak_ui_capture(target, 1, 0.5, 2);
	ak_ui_flush();
	ak_uitest_settle(a);
	CHECK(fut->state == AK_FUT_DONE);
	if (fut->state == AK_FUT_DONE) {
		ak_str *p = fut->result.p;
		CHECK(p && p->len == 16 && memcmp(p->data, "/caches/view.png", 16) == 0);
	}
	CHECK_EQ_STR(log_buf, "capture:1:0,0,-1,-1:1:0.5:2");
	ak_release(fut);
	fut = ak_ui_capture(NULL, 0, 0.9, 0);
	CHECK(fut->state == AK_FUT_FAILED);
	ak_release(fut);
	ak_release(target);
	done(a);
}

T_MAIN()
