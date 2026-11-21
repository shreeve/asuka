// commands.c: the commands the core sends a view: focus, a list's updates,
// a sheet's or alert's Present, animations, opening a URL, a module's call,
// the clipboard, media, frames, announcements, sharing, scrolling, the
// accent and color scheme, the menu bar, windows' options and actions, a
// router's address, the language, quitting; asuka dev's error overlay.
#include "win.h"

void wh_set_accent(ak_color c, ak_color dark) {
	if (c == H.accent && dark == H.accent_dark) return;
	H.accent = c, H.accent_dark = dark;
	wh_palette(&H.theme, H.dark, wh_accent(H.dark));
	RedrawWindow(H.window, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
	wh_dev_error_restyle();
}

// wh_set_color_scheme shows the app light or dark whatever the system's
// setting, or follows it again.
void wh_set_color_scheme(int s) {
	H.scheme = s;
	bool dark = !wh_apps_use_light_theme();
	if (s == AK_COLOR_SCHEME_LIGHT) dark = false;
	else if (s == AK_COLOR_SCHEME_DARK) dark = true;
	if (dark != H.dark) {
		wh_set_dark(dark);
		wh_environment_changed();
	}
}

// ---- Frames: Frame about each display refresh (a 16 ms timer: Win32 has
// no vsync callback for a window) while it's on ----

static bool frames;

static void set_frames(bool on) {
	if (on == frames) return;
	frames = on;
	if (on) SetTimer(H.msg_win, TIMER_FRAME, 16, NULL);
	else KillTimer(H.msg_win, TIMER_FRAME);
}

void wh_tick(void) {
	if (!frames || !H.root) return;
	static double start;
	if (!start) start = wh_seconds();
	ak_event e = {.id = H.root->id, .kind = AK_EVENT_FRAME, .f = wh_seconds() - start};
	wh_dispatch(&e);
}

void wh_msg_timer(WPARAM id) {
	if (id == TIMER_SCRIPT) wh_automation_timer();
	else if (id == TIMER_ROUTES) {
		KillTimer(H.msg_win, TIMER_ROUTES);
		wh_flush_routes();
	} else wh_module_timer(id);
}

// ---- A router's address, kept by its window's key (asuka-routes.json),
// written a moment after it changes and as the app goes to the
// background or quits: the main window's router starts there
// (asuka_set_start), a keyed Window's is given it as its Router starts
// listening for Location ----

static void location(const uint8_t *args, size_t len) {
	ak_location l;
	if (!ak_decode_location(args, len, &l)) return;
	// Its window's key: the main window's "main", sent as none.
	char key[256] = "main";
	if (l.window.len) snprintf(key, sizeof key, "%.*s", (int)(l.window.len < 255 ? l.window.len : 255), l.window.p);
	if (dsk_routes_set(H.routes, key, l.href.p, l.href.len, l.state.p, l.state.len)) SetTimer(H.msg_win, TIMER_ROUTES, 300, NULL);
}

void wh_flush_routes(void) { dsk_routes_flush(H.routes); }

static void give_route(void *ctx) {
	node *n = wh_node((uint32_t)(uintptr_t)ctx);
	// The main window's router started there (asuka_set_start, main.c).
	if (!n || n == H.root) return;
	wh_wctl *c = wh_ctl_for(n);
	const char *key = c ? c->key : NULL;
	const char *href, *state;
	size_t sl;
	if (!key || !dsk_routes_location(H.routes, key, &href, &state, &sl)) return;
	ak_buf b = {0};
	ak_append_location_event(&b, ak_cstr(href), ak_strn(state, sl));
	wh_emit_bytes(n, AK_EVENT_LOCATION, b.data, b.len);
	ak_buf_free(&b);
}

// wh_listen_location follows a window's Router starting to listen for
// Location: where it was left comes back, once the batch is applied.
void wh_listen_location(node *n) {
	static wh_map given;  // once a node: a window's router starts there once
	if (wh_map_get(&given, n->id)) return;
	wh_map_put(&given, n->id, (void *)1);
	wh_after_core(give_route, (void *)(uintptr_t)n->id);
}

// ---- Clipboard, URLs ----

static void set_clipboard_text(const char *s, size_t n) {
	wchar_t *w = wh_wide(s, (int)n);
	size_t bytes = (wcslen(w) + 1) * sizeof(wchar_t);
	if (OpenClipboard(H.window)) {
		EmptyClipboard();
		HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (g) {
			memcpy(GlobalLock(g), w, bytes);
			GlobalUnlock(g);
			if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
		}
		CloseClipboard();
	}
	free(w);
}

void wh_open_url(const char *s, size_t n) {
	wchar_t *w = wh_wide(s, (int)n);
	ShellExecuteW(NULL, L"open", w, NULL, NULL, SW_SHOWNORMAL);
	free(w);
}

void wh_command(node *n, uint16_t cmd, const uint8_t *args, size_t len) {
	switch (cmd) {
	case AK_CMD_FOCUS:
		if (n->ctl) SetFocus(n->ctl);
		else if (n->hwnd) SetFocus(n->hwnd);
		break;
	case AK_CMD_BLUR: {
		HWND f = GetFocus();
		if (f && (f == n->ctl || f == n->hwnd)) SetFocus(GetAncestor(f, GA_ROOT));
		break;
	}
	case AK_CMD_LIST_UPDATE:
		if (n->list) wh_list_update(n, args, len);
		break;
	case AK_CMD_INSERT_TEXT:
		if (n->kind == AK_KIND_TEXT_FIELD) wh_insert_text(n, (const char *)args, len);
		break;
	case AK_CMD_FORMAT:
		wh_format_rich(n, args, len);
		break;
	case AK_CMD_SET_LINK:
		wh_set_rich_link(n, (const char *)args, len);
		break;
	case AK_CMD_PRESENT:
		wh_present(n);
		break;
	case AK_CMD_HAPTIC:
		break;  // a PC has no haptics to play
	case AK_CMD_ANIMATE_BEGIN:
		wh_begin_animation(args, len);
		break;
	case AK_CMD_ANIMATE_END:
		if (H.batch_frames) wh_run_matches();  // the frames are applied: they step with the rest
		wh_end_animation();
		break;
	case AK_CMD_OPEN_URL:
		wh_open_url((const char *)args, len);
		break;
	case AK_CMD_CALL:
		wh_call(n, args, len);
		break;
	case AK_CMD_COPY:
		set_clipboard_text((const char *)args, len);
		break;
	case AK_CMD_PLAY:
	case AK_CMD_PAUSE:
	case AK_CMD_SEEK:
	case AK_CMD_EVAL:
	case AK_CMD_WEB_NAVIGATE:
		if (n->web) wh_web_command(n, cmd, args, len);
		break;
	case AK_CMD_FRAMES:
		set_frames(len > 0 && args[0]);
		break;
	case AK_CMD_ANNOUNCE:
		wh_announce((const char *)args, len);
		break;
	case AK_CMD_A11Y_FOCUS:
		// Narrator follows the focus event to the view.
		if (n->hwnd) NotifyWinEvent(EVENT_OBJECT_FOCUS, n->hwnd, OBJID_CLIENT, CHILDID_SELF);
		break;
	case AK_CMD_SHARE:
		wh_share_text((const char *)args, len);  // Windows's share UI
		break;
	case AK_CMD_SCROLL_TO:
		if (n->list && len >= 5) wh_list_scroll_to(n, wh_u32(args));
		break;
	case AK_CMD_SCROLL_TO_OFFSET:
		if (len >= 9) wh_scroll_to_offset(n, wh_f32(args), wh_f32(args + 4));
		break;
	case AK_CMD_ACCENT:
		if (len >= 8) wh_set_accent(wh_u64(args), len >= 17 ? wh_u64(args + 9) : 0);  // the dark one after Android's flags
		break;
	case AK_CMD_MENU_BAR:
		wh_set_menu_bar(args, len);
		break;
	case AK_CMD_WINDOW_OPTIONS:
		wh_window_options(n, args, len);
		break;
	case AK_CMD_WINDOW_ACTION:
		wh_window_action(n, args, len);
		break;
	case AK_CMD_WINDOW_TITLE:
		wh_set_window_title((const char *)args, len);
		break;
	case AK_CMD_LOCATION:
		location(args, len);
		break;
	case AK_CMD_LOCALE:
		wh_set_locale(args, len);
		break;
	case AK_CMD_SAVE_LOCALE:
		wh_save_locale((const char *)args, len);
		break;
	case AK_CMD_COLOR_SCHEME:
		if (len > 0) wh_set_color_scheme(args[0]);
		break;
	case AK_CMD_QUIT:
		wh_quit(len > 0 && (args[0] & 1));
		break;
	case AK_CMD_DEV_ERROR:
		wh_dev_error(n, args, len);  // asuka dev's error overlay (deverror.c)
		break;
	case AK_CMD_DEV_ERROR_CLEAR:
		wh_dev_error_clear();
		break;
	case AK_CMD_DEV_HIGHLIGHT:
		wh_dev_highlight(n, args, len);
		break;
	case AK_CMD_DEV_HIGHLIGHT_CLEAR:
		wh_dev_highlight_clear();
		break;
	}
}
