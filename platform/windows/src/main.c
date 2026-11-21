// main.c: the app's executable: the host starts (the app's settings, one
// instance, its crash record, its windows), then the core with the app
// compiled into it, or under asuka dev the bytecode the CLI sends
// (asuka_dev_connect), and the message loop runs until the app quits.
#include "win.h"

// ---- The core's callbacks ----

static void cb_apply(const uint8_t *buf, size_t len) { wh_apply(buf, len); }

static void cb_measure(uint32_t id, float w, float h, float aw, float ah, float *ow, float *oh) { wh_measure(id, w, h, aw, ah, ow, oh); }

static void run_core_posted(void *ctx) {
	(void)ctx;
	asuka_run_posted();
}

static void cb_post(void) { wh_post(run_core_posted, NULL); }

// ---- The app's names and places ----

static char *app_name(void) {
	if (H.config.name) return wh_strdup(H.config.name);
	char *exe = dsk_exe_path();
	const char *b = exe ? dsk_base(exe) : "asuka";
	size_t n = strlen(b);
	if (n > 4 && _stricmp(b + n - 4, ".exe") == 0) n -= 4;
	char *name = wh_strndup(b, n);
	free(exe);
	return name;
}

// assets_dir is where the app's assets are: ASUKA_ASSETS, assets beside
// the executable (asuka build), or assets in the working directory.
static char *assets_dir(void) {
	char *d = dsk_getenv("ASUKA_ASSETS");
	if (d) return d;
	char *exe = dsk_exe_path();
	if (exe) {
		char *dir = dsk_dir(exe);
		char *a = dsk_join(dir, "assets");
		free(dir);
		free(exe);
		wchar_t *w = wh_wide(a, (int)strlen(a));
		DWORD attr = GetFileAttributesW(w);
		free(w);
		if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) return a;
		free(a);
	}
	return wh_strdup("assets");
}

// The app's data: Local AppData\<name>, made by what writes there.
static void find_dirs(void) {
	char *data = dsk_getenv("ASUKA_DATA");
	if (!data) {
		char *base = dsk_getenv("LOCALAPPDATA");
		if (!base) base = dsk_getenv("APPDATA");
		data = base ? dsk_join(base, H.app_name) : wh_strdup(H.app_name);
		free(base);
		wchar_t *w = wh_wide(data, (int)strlen(data));
		SetEnvironmentVariableW(L"ASUKA_DATA", w);
		free(w);
	}
	dsk_dirs_find(&H.dirs, DSK_WINDOWS, data);
	free(data);
}

static bool automated(void) {
	const char *keys[] = {"ASUKA_SNAPSHOT", "ASUKA_SCRIPT", "ASUKA_TEST"};
	for (size_t i = 0; i < WH_LEN(keys); i++) {
		char *v = dsk_getenv(keys[i]);
		if (v) {
			free(v);
			return true;
		}
	}
	return false;
}

// ---- Starting ----

// hex_color is asuka-app.json's "#rrggbb" or "#rrggbbaa" as a color.
static ak_color hex_color(const char *hex) {
	unsigned r = 0, g = 0, b = 0, a = 255;
	const char *s = hex[0] == '#' ? hex + 1 : hex;
	if (strlen(s) == 8) sscanf(s, "%2x%2x%2x%2x", &r, &g, &b, &a);
	else sscanf(s, "%2x%2x%2x", &r, &g, &b);
	return ak_rgba((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
}

// new_host makes the host's windows: the message window, the parking
// window views wait in, the app's window.
static void new_host(void) {
	wh_dpi_aware();
	wh_activate_common_controls();
	OleInitialize(NULL);  // drag and drop, COM
	wh_gdiplus_start();
	LoadLibraryW(L"Msftedit.dll");  // rich edits
	wh_register_classes();
	InitializeSRWLock(&H.post_mu);
	H.app_namew = wh_wide(H.app_name, (int)strlen(H.app_name));
	H.measure_dc = CreateCompatibleDC(NULL);
	// HWND_MESSAGE; titled, for a running-once app, so that a second
	// launch finds it (instance.c).
	H.msg_win = CreateWindowExW(0, WH_CLASS_MESSAGE, wh_instance_title(), 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);
	H.park = CreateWindowExW(0, WH_CLASS_WINDOW, L"", WS_POPUP | WS_CLIPCHILDREN, 0, 0, 0, 0, NULL, NULL, GetModuleHandleW(NULL), NULL);
	// A process's first ShowWindow takes the show its launcher asked for
	// (STARTUPINFO's: hidden, from some), whatever it's given: the parking
	// window takes it. The main window shows as the launch asked where
	// that's minimized or maximized (wh_show_ctl).
	ShowWindow(H.park, SW_HIDE);
	H.dark = !wh_apps_use_light_theme();
	char *scheme = dsk_getenv("ASUKA_COLOR_SCHEME");  // automation: snapshots in either
	if (scheme && !strcmp(scheme, "light")) H.dark = false, H.scheme = AK_COLOR_SCHEME_LIGHT;
	if (scheme && !strcmp(scheme, "dark")) H.dark = true, H.scheme = AK_COLOR_SCHEME_DARK;
	free(scheme);
	if (H.config.accent) H.accent = hex_color(H.config.accent);
	if (H.config.accent_dark) H.accent_dark = hex_color(H.config.accent_dark);
	wh_set_app_dark(H.dark);
	wh_palette(&H.theme, H.dark, wh_accent(H.dark));
	wh_read_intl();
	wh_make_window();
	wh_environment(&H.env);
	H.env.env.locale = ak_cstr(H.env.locale), H.env.env.time_zone = ak_cstr(H.env.time_zone), H.env.env.languages = ak_cstr(H.env.languages);
}

static asuka_host_callbacks callbacks;

// start_core gives the core its host and starts the app: the bytecode
// asuka dev sends when ASUKA_DEV (host:port) is set, else the app
// compiled into this executable.
static bool start_core(void) {
	callbacks.apply = cb_apply;
	callbacks.measure = cb_measure;
	callbacks.post = cb_post;
	wh_native_host_callbacks(&callbacks);  // the native packages' registry, and platform/host's modules
	asuka_set_app_dir(H.dirs.data);
	// Its other directories (std/fs's configDir, cacheDir, logDir): the
	// host's own, so the two agree.
	asuka_set_dir(ASUKA_DIR_CONFIG, H.dirs.config);
	asuka_set_dir(ASUKA_DIR_CACHE, H.dirs.cache);
	asuka_set_dir(ASUKA_DIR_LOGS, H.dirs.logs);
	asuka_start(&callbacks);
	wh_report_environment(&(node){.id = 0}, &H.env);
	// The main window's router starts where the window was left
	// (asuka-routes.json; none under asuka dev or automation).
	const char *href, *state;
	size_t sl;
	if (H.routes && dsk_routes_location(H.routes, "main", &href, &state, &sl)) asuka_set_start(href);
	char *err = NULL;
	char *dev = dsk_getenv("ASUKA_DEV");
	int r;
	if (dev) {
		char *colon = strrchr(dev, ':');
		if (!colon) {
			wh_log("ASUKA_DEV=\"%s\": want host:port", dev);
			free(dev);
			return false;
		}
		*colon = 0;
		r = asuka_dev_connect(dev, atoi(colon + 1), "{\"platform\":\"windows\",\"device\":\"desktop\"}", &err);
		free(dev);
	} else if (asuka_has_app()) {
		r = asuka_run(&err);
	} else {
		wh_log("this build has no app compiled in, and ASUKA_DEV isn't set");
		return false;
	}
	if (r != 0) {
		wh_log("%s", err ? err : "the core failed (see its log)");
		if (err) asuka_free_string(err);
		return false;
	}
	return true;
}

// stdin_stop watches standard input under asuka dev (ASUKA_STOP=stdin):
// Windows sends a process no SIGINT, so asuka dev closes it to stop the
// app.
static DWORD WINAPI stdin_stop(void *ctx) {
	(void)ctx;
	HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
	char buf[256];
	DWORD n;
	while (ReadFile(in, buf, sizeof buf, &n, NULL) && n > 0) {
	}
	ExitProcess(0);
	return 0;
}

static BOOL WINAPI console_ctrl(DWORD type) {
	if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
		PostMessageW(H.window, WM_CLOSE, 0, 0);
		return TRUE;
	}
	return FALSE;
}

// ---- The message loop ----

// wh_pre_translate handles a key message for one of the app's windows
// before it's dispatched: a view's Keys, the menus' shortcuts (against
// the window it's for), the host's keys, and Tab and arrow key navigation
// among the controls, as in a dialog.
bool wh_pre_translate(MSG *m) {
	if (wh_dev_error_key(m)) return true;  // asuka dev's error overlay's, over all
	if (wh_route_key(m)) return true;  // the views' Keys, first
	HWND w = wh_key_window(m->hwnd);
	if (!w) return false;
	node *d = wh_node_of(w);
	if (d && d->kind == KIND_ALERT_WINDOW) {
		// An alert's dialog is modal, as a task dialog is: the menus'
		// shortcuts wait, and its fields keep Ctrl+C and the like.
		return wh_key_message(m) || IsDialogMessageW(w, m);
	}
	return wh_accelerate(w, m) || wh_key_message(m) || wh_tab_key(m) || IsDialogMessageW(w, m);
}

static void loop(void) {
	MSG m;
	for (;;) {
		wh_free_dead();  // between messages: no handler names them
		if (GetMessageW(&m, NULL, 0, 0) <= 0) break;
		if (m.message == WM_MOUSEMOVE) {
			wh_mouse_over(m.hwnd);
			wh_pointer_moved(m.hwnd, m.pt, true);
		}
		else if (m.message == WM_MOUSELEAVE) wh_mouse_left(m.hwnd);
		wh_input_seen(&m);
		bool press = wh_is_press_message(m.message);
		if (press) wh_overlay_pointer_down(m.hwnd, m.pt);  // an overlay hears of a press outside it before its target does
		if (wh_pre_translate(&m)) continue;
		TranslateMessage(&m);
		if (press) wh_pressing(true);
		DispatchMessageW(&m);
		if (press) wh_pressing(false);
	}
}

static void mount_failed(void *ctx) {
	(void)ctx;
	ExitProcess(1);
}

static void start(void *ctx) {
	bool *ok = ctx;
	*ok = start_core();
}

int wh_main(int argc, wchar_t **argv) {
	dsk_config_load(&H.config);
	H.app_name = app_name();
	H.app_id = H.config.id ? wh_strdup(H.config.id) : NULL;
	find_dirs();
	H.snapshotting = getenv("ASUKA_SNAPSHOT") != NULL;
	H.scripted = getenv("ASUKA_SCRIPT") != NULL;
	H.assets = assets_dir();
	wh_take_login_arg(&argc, argv);
	wh_take_toast_launch(&argc, argv);  // COM started it for a toast's click
	wh_relaunch_wait();  // a relaunch waits for the process it came from
	// One instance: a second launch hands the first its arguments, and
	// quits.
	if (!wh_take_instance(argc, argv)) return 0;
	wh_update_clean();  // what an update left
	wh_crash_start();
	// The process's WebView2 environment, whoever makes it first: videos
	// play without a click, as a WebView's autoPlay asks.
	SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", L"--autoplay-policy=no-user-gesture-required");
	if (!automated()) {
		char *p = dsk_join(H.dirs.data, "asuka-window.json");
		H.states = dsk_winstate_open(p);
		free(p);
		char *lp = dsk_join(H.dirs.data, "locale");
		H.locale_path = lp;
	}
	char *rp = dsk_routes_path();
	H.routes = dsk_routes_open(rp);
	free(rp);
	wh_read_saved_locale();
	new_host();
	wh_focus_start();  // the views' focus, reported from the first batch's on
	char *stop = dsk_getenv("ASUKA_STOP");
	if (stop && !strcmp(stop, "stdin")) CloseHandle(CreateThread(NULL, 0, stdin_stop, NULL, 0, NULL));
	free(stop);
	SetConsoleCtrlHandler(console_ctrl, TRUE);
	bool ok = false;
	wh_enter(start, &ok);
	if (!ok) {
		mount_failed(NULL);
		return 1;
	}
	wh_after_launch(argc, argv);  // the links it was opened with, its toasts, the quit's handling
	if (H.launched_for_toast) {
		wh_wait_for_toast();  // shown if the click opens the app
	} else if (H.config.background && !H.snapshotting && !H.scripted) {
		H.started = true;  // in the background: no window at launch
	} else {
		wh_show_main();
	}
	wh_automation();
	loop();
	wh_free_dead();
	wh_flush_routes();
	wh_finish_quit();  // a relaunch, if the quit asked for one
	return 0;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show) {
	(void)inst, (void)prev, (void)cmd, (void)show;
	int argc = 0;
	wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	return wh_main(argc, argv);
}

int wmain(int argc, wchar_t **argv) { return wh_main(argc, argv); }
