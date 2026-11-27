// window.c: the window classes, the one window procedure (routing by
// window to its node), the app's window and its environment.
#include "host.h"
#include "parts.h"
#include "win.h"

// wh_register_classes registers the window classes, with one window
// procedure for all of them: it routes by window.
void wh_register_classes(void) {
	static bool done;
	if (done) return;
	done = true;
	HINSTANCE inst = GetModuleHandleW(NULL);
	HICON icon = wh_app_icon();
	WNDCLASSEXW c = {sizeof c};
	c.lpfnWndProc = wh_wndproc;
	c.hInstance = inst;
	c.hCursor = LoadCursorW(NULL, IDC_ARROW);
	c.style = CS_HREDRAW | CS_VREDRAW;
	c.hIcon = icon;
	c.lpszClassName = WH_CLASS_WINDOW;
	RegisterClassExW(&c);
	c.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
	c.lpszClassName = WH_CLASS_FLYOUT;
	RegisterClassExW(&c);
	c.style = CS_DBLCLKS;
	c.hIcon = NULL;
	c.lpszClassName = WH_CLASS_VIEW;
	RegisterClassExW(&c);
	c.style = 0;
	c.lpszClassName = WH_CLASS_MESSAGE;
	RegisterClassExW(&c);
}

// wh_parent_message handles what a control tells its parent (every
// control's parent is a window of the host's): commands, notifications,
// colors.
LRESULT wh_parent_message(UINT m, WPARAM wp, LPARAM lp, bool *handled) {
	*handled = false;
	node *c;
	switch (m) {
	case WM_COMMAND:
		if (lp && (c = wh_node_of((HWND)lp))) {
			wh_control_command(c, (HWND)lp, HIWORD(wp));
			*handled = true;
			return 0;
		}
		break;
	case WM_NOTIFY: {
		NMHDR *hdr = (NMHDR *)lp;
		if ((c = wh_node_of(hdr->hwndFrom))) return wh_notify(c, hdr, lp, handled);
		break;
	}
	case WM_HSCROLL:
	case WM_VSCROLL:
		if (lp && (c = wh_node_of((HWND)lp)) && c->kind == AK_KIND_SLIDER) {
			wh_slider_moved(c);
			*handled = true;
			return 0;
		}
		break;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN:
	case WM_CTLCOLORLISTBOX:
		if ((c = wh_node_of((HWND)lp))) {
			*handled = true;
			return (LRESULT)wh_control_colors(c, (HDC)wp, m);
		}
		break;
	case WM_DRAWITEM: {
		// An owner-drawn combo box's item (a Wheel's or Picker's list).
		DRAWITEMSTRUCT *d = (DRAWITEMSTRUCT *)lp;
		if (d->CtlType == ODT_COMBOBOX && (c = wh_node_of(d->hwndItem)) && c->ctl == d->hwndItem) {
			wh_combo_draw_item(c, d);
			*handled = true;
			return TRUE;
		}
		break;
	}
	}
	return 0;
}

// wh_key_window is the top-level window of the app a message is for: the
// main one, a sheet's or a Window's.
HWND wh_key_window(HWND hwnd) {
	if (!hwnd) return NULL;
	HWND w = GetAncestor(hwnd, GA_ROOT);
	if (w == H.window) return w;
	node *n = wh_node_of(w);
	if (n && (n->kind == KIND_SHEET_WINDOW || n->kind == KIND_WINDOW_FRAME || n->kind == KIND_ALERT_WINDOW)) return w;
	return NULL;
}

static LRESULT window_message(UINT m, WPARAM wp, LPARAM lp, bool *handled);

static LRESULT message(HWND hwnd, UINT m, WPARAM wp, LPARAM lp, bool *handled) {
	*handled = true;
	LRESULT r;
	bool h;
	// A menu item chosen, or its accelerator, in one of the app's windows.
	if (m == WM_COMMAND && lp == 0 && HIWORD(wp) <= 1 && wh_key_window(hwnd) == hwnd && wh_menu_command(LOWORD(wp))) return 0;
	// Owner-drawn menu items, in a menu one of the app's windows owns.
	if (m == WM_MEASUREITEM || m == WM_DRAWITEM || m == WM_MENUCHAR || m == WM_INITMENUPOPUP || m == WM_INITMENU ||
		m == WM_ENTERMENULOOP || m == WM_EXITMENULOOP) {
		r = wh_menu_item_message(hwnd, m, wp, lp, &h);
		if (h) return r;
	}
	wh_wctl *chrome = wh_map_get(&H.chrome, (uint64_t)(uintptr_t)hwnd);
	if (chrome) return wh_chrome_message(chrome, hwnd, m, wp, lp, handled);
	// One of the app's windows activated: the panels and flyouts it isn't
	// an own window of close.
	if (m == WM_ACTIVATE && LOWORD(wp) != WA_INACTIVE) wh_window_activated(hwnd);
	if (hwnd == H.msg_win) {
		switch (m) {
		case WM_RUN_POSTED:
			wh_run_posted();
			return 0;
		case WM_TIMER:
			if (wp == TIMER_FRAME) wh_tick();
			else if (wp == TIMER_ANIMATE) wh_animation_step();
			else if (wp == TIMER_HOVER) {
				POINT p;
				GetCursorPos(&p);
				wh_pointer_moved(WindowFromPoint(p), p, true);
			} else wh_msg_timer(wp);
			return 0;
		case WM_COPYDATA:
			r = wh_copy_data(lp, &h);  // another launch of the app (instance.c)
			if (h) return r;
			break;
		default:
			r = wh_module_message(m, wp, lp, &h);  // the modules' (tray, audio...)
			if (h) return r;
		}
		*handled = false;
		return 0;
	}
	if (hwnd == H.window) return window_message(m, wp, lp, handled);
	node *n = wh_node_of(hwnd);
	if (!n) {
		*handled = false;
		return 0;
	}
	if (n->kind == AK_KIND_NATIVE) {
		r = wh_native_message(n, m, wp, lp, &h);
		if (h) return r;
	}
	r = wh_parent_message(m, wp, lp, &h);
	if (h) return r;
	switch (n->kind) {
	case KIND_SHEET_WINDOW: return wh_sheet_message(n, m, wp, lp, handled);
	case KIND_POPOVER_WINDOW: return wh_popover_message(n, m, wp, lp, handled);
	case KIND_WINDOW_FRAME: return wh_window_frame_message(n, m, wp, lp, handled);
	case KIND_ALERT_WINDOW: return wh_alert_dialog_message(n, m, wp, lp, handled);
	}
	return wh_view_message(n, hwnd, m, wp, lp, handled);
}

LRESULT CALLBACK wh_wndproc(HWND hwnd, UINT m, WPARAM wp, LPARAM lp) {
	if (m == WM_DESTROY) wh_window_destroyed(hwnd);
	bool handled = false;
	LRESULT r = message(hwnd, m, wp, lp, &handled);
	if (handled) return r;
	return DefWindowProcW(hwnd, m, wp, lp);
}

// A window's client size, in points, when the app gives none: a desktop
// window's, as Electron's are.
#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600

// wh_make_window makes the app's window, 800 by 600 points (no taller
// than the work area), centered in the work area.
void wh_make_window(void) {
	DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
	H.window = CreateWindowExW(0, WH_CLASS_WINDOW, H.app_namew, style, 0, 0, 100, 100, NULL, NULL, GetModuleHandleW(NULL), NULL);
	// Before anything that sends the window messages: SetWindowPos sends
	// WM_WINDOWPOSCHANGING before it returns, which window_message hands
	// the main control.
	H.main_ctl = wh_wctl_new(H.window, true);
	H.dpi = wh_dpi(H.window);
	H.scale = (float)H.dpi / 96;
	wh_set_dark_title_bar(H.window, H.dark);
	RECT r = {0, 0, wh_px(NULL, DEFAULT_WIDTH), wh_px(NULL, DEFAULT_HEIGHT)};
	wh_adjust_window_rect(&r, style, 0, H.dpi);
	RECT work = wh_work_area(H.window);
	int w = r.right - r.left, ht = wh_mini(r.bottom - r.top, work.bottom - work.top);
	int x = work.left + ((work.right - work.left) - w) / 2, y = work.top + ((work.bottom - work.top) - ht) / 2;
	SetWindowPos(H.window, NULL, x, y, w, ht, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
	wh_accept_drops(H.window);
}

static LRESULT window_message(UINT m, WPARAM wp, LPARAM lp, bool *handled) {
	*handled = true;
	LRESULT r;
	bool h;
	if (!H.main_ctl) {
		*handled = false;  // CreateWindow's own messages: the window's default handling
		return 0;
	}
	// asuka dev's error overlay follows it (deverror.c), and its performance
	// overlay (devperf.c).
	if (m == WM_WINDOWPOSCHANGED) {
		wh_dev_error_follow();
		wh_dev_perf_follow();
	}
	r = wh_ctl_message(H.main_ctl, m, wp, lp, &h);
	if (h) return r;
	wh_badge_message(m);
	wh_dock_message(m);
	// Closing it, and the session ending, quit the app.
	r = wh_quit_message(m, wp, lp, &h);
	if (h) return r;
	r = wh_menu_bar_message(H.window, m, wp, lp, &h);
	if (h) return r;
	switch (m) {
	case WM_SIZE:
		if (H.root && H.root->hwnd) {
			RECT c;
			GetClientRect(H.window, &c);
			SetWindowPos(H.root->hwnd, NULL, 0, 0, c.right, c.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
		}
		wh_environment_changed();
		return 0;
	case WM_ERASEBKGND: {
		RECT c;
		GetClientRect(H.window, &c);
		HBRUSH b = CreateSolidBrush(rgba_ref(wh_bg_of(H.window)));
		FillRect((HDC)wp, &c, b);
		DeleteObject(b);
		return 1;
	}
	case WM_DPICHANGED:
		wh_dpi_changed(LOWORD(wp), (RECT *)lp);
		return 0;
	case WM_SETTINGCHANGE:
		wh_setting_changed((const wchar_t *)lp);
		*handled = false;
		return 0;
	case WM_THEMECHANGED:
		wh_setting_changed(L"ImmersiveColorSet");
		*handled = false;
		return 0;
	case WM_ACTIVATE:
		if (LOWORD(wp) == WA_INACTIVE) {
			wh_lifecycle(AK_LIFECYCLE_BACKGROUND);
		} else {
			H.in_use = H.window;
			wh_lifecycle(AK_LIFECYCLE_FOREGROUND);
		}
		*handled = false;
		return 0;
	case WM_CLOSE:
		wh_close_main();
		return 0;
	case WM_DESTROY:
		wh_remove_tray_icons();
		PostQuitMessage(0);
		return 0;
	}
	if (H.root) {
		r = wh_parent_message(m, wp, lp, &h);
		if (h) return r;
	}
	*handled = false;
	return 0;
}

// wh_environment is the main window's: its client area's size, its DPI
// as the scale, the text size (asuka dev's preview's, over the user's),
// dark mode, animations (Reduce Motion), the time zone, the user's
// languages and regional formats. A desktop window has no keyboard over
// it, and no safe area but a hidden title bar's strip at the top, where
// its caption buttons are (SafeTop).
void wh_environment(wh_env *out) {
	memset(out, 0, sizeof *out);
	RECT r;
	GetClientRect(H.window, &r);
	ak_environment *e = &out->env;
	e->width = wh_pt(NULL, r.right), e->height = wh_pt(NULL, r.bottom);
	e->safe_top = wh_safe_top(H.main_ctl);
	e->scale = H.scale;
	e->font_scale = H.dev_font_scale > 0 ? H.dev_font_scale : wh_text_scale();
	e->dark = H.dark;
	BOOL anim = TRUE, reader = FALSE;
	SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &anim, 0);
	SystemParametersInfoW(SPI_GETSCREENREADER, 0, &reader, 0);
	e->reduce_motion = !anim;
	e->screen_reader = reader != 0;
	e->platform = AK_PLATFORM_WINDOWS;
	e->width_class = ak_width_class_of(e->width), e->height_class = ak_height_class_of(e->height);
	wh_intl_env(out);
	e->locale = ak_cstr(out->locale), e->time_zone = ak_cstr(out->time_zone), e->languages = ak_cstr(out->languages);
}

static bool env_equal(const wh_env *a, const wh_env *b) {
	const ak_environment *x = &a->env, *y = &b->env;
	return x->width == y->width && x->height == y->height && x->safe_top == y->safe_top && x->safe_right == y->safe_right &&
		x->safe_bottom == y->safe_bottom && x->safe_left == y->safe_left && x->keyboard == y->keyboard && x->scale == y->scale &&
		x->font_scale == y->font_scale && x->dark == y->dark && x->reduce_motion == y->reduce_motion &&
		x->screen_reader == y->screen_reader && x->platform == y->platform && x->first_weekday == y->first_weekday &&
		x->hour_cycle == y->hour_cycle && strcmp(a->locale, b->locale) == 0 && strcmp(a->time_zone, b->time_zone) == 0 &&
		strcmp(a->languages, b->languages) == 0;
}

// wh_report_environment sends root the environment e.
void wh_report_environment(node *root, const wh_env *e) {
	ak_event ev = {.id = root->id, .kind = AK_EVENT_ENVIRONMENT, .env = e->env};
	ev.env.locale = ak_cstr(e->locale), ev.env.time_zone = ak_cstr(e->time_zone), ev.env.languages = ak_cstr(e->languages);
	if (root == H.root) {
		H.core_env = *e;
		H.core_env.env.locale = ak_cstr(H.core_env.locale);
		H.core_env.env.time_zone = ak_cstr(H.core_env.time_zone);
		H.core_env.env.languages = ak_cstr(H.core_env.languages);
	}
	wh_dispatch(&ev);
}

// wh_environment_changed reports a new environment to the root, and to the
// screens and sheets the host sizes.
void wh_environment_changed(void) {
	if (!H.window) return;
	wh_env env;
	wh_environment(&env);
	if (env_equal(&env, &H.env)) {
		wh_layout_nav();
		return;
	}
	bool fonts = env.env.font_scale != H.env.env.font_scale;
	H.env = env;
	H.env.env.locale = ak_cstr(H.env.locale), H.env.env.time_zone = ak_cstr(H.env.time_zone), H.env.env.languages = ak_cstr(H.env.languages);
	if (fonts) {
		wh_clear_fonts();
		uint32_t i = 0;
		void *v;
		while (wh_map_next(&H.nodes, &i, NULL, &v)) wh_apply_font(v);
		wh_dev_error_restyle();
	}
	if (H.root) wh_report_environment(H.root, &H.env);
	wh_layout_nav();
}

// wh_dpi_changed follows the main window to a monitor of another DPI: its
// views at the new scale, the window at the size Windows suggests, the
// environment reported. The Windows and sheets keep theirs.
void wh_dpi_changed(int dpi, const RECT *s) {
	H.dpi = dpi;
	H.scale = (float)dpi / 96;
	wh_rescale(H.window, 0);
	wh_rescale(H.park, 0);
	SetWindowPos(H.window, NULL, s->left, s->top, s->right - s->left, s->bottom - s->top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
	wh_environment_changed();
	RedrawWindow(H.window, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_FRAME);
	DrawMenuBar(H.window);
}

// set_scale sets the scale of a window's views, and lists them.
static uint32_t set_scale(HWND hwnd, float k, node ***out) {
	HWND *list;
	uint32_t n = wh_children(hwnd, &list);
	list = wh_realloc(list, (n + 1) * sizeof *list);
	list[n++] = hwnd;
	node **ns = wh_alloc(n * sizeof *ns);
	uint32_t got = 0;
	for (uint32_t i = 0; i < n; i++) {
		node *x = wh_node_of(list[i]);
		if (x && x->hwnd == list[i] && x->kind != KIND_WINDOW_FRAME && x->kind != KIND_SHEET_WINDOW && x->kind != KIND_ALERT_WINDOW) {
			x->k = wh_k_for(k, wh_zoom_up_to(list[i], hwnd));
			ns[got++] = x;
		}
	}
	free(list);
	*out = ns;
	return got;
}

void wh_set_scale(HWND hwnd, float k) {
	node **ns;
	set_scale(hwnd, k, &ns);
	free(ns);
}

// wh_rescale gives a window and the views in it a new scale, k (0: the
// main window's): their fonts again, their frames in pixels again, their
// lists tiled again.
void wh_rescale(HWND hwnd, float k) {
	node **ns;
	uint32_t n = set_scale(hwnd, k, &ns);
	for (uint32_t i = 0; i < n; i++) {
		node *x = ns[i];
		wh_apply_font(x);
		if (x->id && !(x->id & AK_HOST_ID_BIT) && !x->cell && (!x->nav || !wh_nav_sized_by_host(x))) wh_place_frame(x);
		if (x->list) wh_list_rebind(x);
		if (x->bar) {
			wh_bars_reset(x);
			wh_update_bars(x);
		}
	}
	free(ns);
}

// wh_setting_changed follows the system's settings: dark mode and the
// accent (ImmersiveColorSet), the locale (intl), the text size.
void wh_setting_changed(const wchar_t *what) {
	if (what && wcscmp(what, L"intl") == 0) {
		wh_read_intl();
	} else if (!what || !*what || wcscmp(what, L"ImmersiveColorSet") == 0) {
		bool dark = !wh_apps_use_light_theme();
		if (H.scheme) dark = H.scheme == AK_COLOR_SCHEME_DARK;
		if (dark != H.dark || (what && wcscmp(what, L"ImmersiveColorSet") == 0)) wh_set_dark(dark);
	}
	wh_environment_changed();
}

// wh_set_dark redraws the app light or dark.
void wh_set_dark(bool dark) {
	H.dark = dark;
	wh_set_app_dark(dark);
	wh_palette(&H.theme, dark, wh_accent(dark));
	wh_clear_menu_styles();
	wh_clear_brushes();
	wh_set_dark_title_bar(H.window, dark);
	for (uint32_t i = 0; i < H.nsheets; i++) wh_nav_window_dark(H.sheets[i], dark);
	for (uint32_t i = 0; i < H.nwins; i++) wh_nav_window_dark(H.wins[i], dark);
	uint32_t i = 0;
	void *v;
	while (wh_map_next(&H.nodes, &i, NULL, &v)) {
		node *n = v;
		if (n->kind == AK_KIND_ALERT) wh_nav_window_dark(n, dark);
		wh_theme_control(n);
		wh_retheme_rich(n);
		if (n->web) wh_web_color_scheme(n, dark);
	}
	RedrawWindow(H.window, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_FRAME);
	DrawMenuBar(H.window);
	for (uint32_t j = 0; j < H.nsheets; j++) wh_nav_window_redraw(H.sheets[j]);
	for (uint32_t j = 0; j < H.nwins; j++) wh_nav_window_redraw(H.wins[j]);
	wh_layout_all_chrome();  // hidden title bars' buttons, in the new colors
	wh_dev_error_restyle();
}

void wh_lifecycle(int state) {
	if (state == AK_LIFECYCLE_BACKGROUND) wh_flush_routes();
	if (!H.root) return;
	ak_event e = {.id = H.root->id, .kind = AK_EVENT_LIFECYCLE, .i = state};
	wh_dispatch(&e);
}

// wh_key_message handles the app's keys before the dialog navigation
// does: F5 refreshes, Escape dismisses a sheet, Alt+Left goes back.
bool wh_key_message(MSG *m) {
	if (m->message != WM_KEYDOWN && m->message != WM_SYSKEYDOWN) return false;
	switch (m->wParam) {
	case VK_F5:
		if (m->message == WM_KEYDOWN && !wh_key_down(VK_CONTROL) && !wh_key_down(VK_SHIFT)) return wh_refresh_key(m);
		break;
	case VK_ESCAPE: {
		if (wh_escape_dismisses(m)) return true;  // a panel, a flyout
		HWND w = GetAncestor(m->hwnd, GA_ROOT);
		if (w != H.window) {
			node *n = wh_node_of(w);
			if (n && n->kind == KIND_SHEET_WINDOW) return wh_cancel_sheet(n), true;
			if (n && n->kind == KIND_POPOVER_WINDOW) return wh_cancel_popover(n), true;
			if (n && n->kind == KIND_ALERT_WINDOW) return wh_cancel_alert_dialog(n), true;
		}
		break;
	}
	case VK_LEFT:
	case VK_RIGHT:
		// Alt+Left, and Alt+Right right to left, where the back arrow points.
		if (m->message == WM_SYSKEYDOWN && (m->wParam == VK_LEFT || H.rtl)) {
			node *r = wh_root_of(GetAncestor(m->hwnd, GA_ROOT));
			if (r) {
				wh_back(r);
				return true;
			}
		}
		break;
	}
	return false;
}

// wh_least_size is the main window's least client size: WindowOptions',
// or 320 points square.
void wh_least_size(float out[2]) {
	out[0] = H.min_size[0] > 0 ? H.min_size[0] : 320;
	out[1] = H.min_size[1] > 0 ? H.min_size[1] : 320;
}

HWND wh_window_in_use(void) {
	if (H.in_use && IsWindow(H.in_use) && IsWindowVisible(H.in_use)) return H.in_use;
	return H.window;
}
