// window.c: the app's windows. The main one holds the root; a Window
// node has one of its own. Each is an AdwApplicationWindow (an AdwWindow
// for a Window) whose content is an AdwToolbarView: one header bar, as
// GNOME's apps have (chrome.c), and under it, in an AdwToastOverlay, a
// view of the host's that holds the root at its size, whose allocation is
// the environment the core lays out at.
#include "host.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void lx_watch_window(win *w);
void lx_watch_window_state(win *w);
void lx_update_lifecycle(void);

int lx_window_size(float f) { return (int)lroundf(f); }

static gboolean main_close_request(GtkWindow *window, gpointer data) {
	(void)window;
	win *w = data;
	// The user closing it: the core asks first while it listens for
	// CloseRequest (the window stays). Then, for an app that keeps running
	// with no window, it hides, the root mounted and the app in the
	// background; else it closes, which quits the app.
	if (!w->forced && H.root && listening(H.root, AK_EVENT_CLOSE_REQUEST)) {
		lx_emit(H.root, (ak_event){.kind = AK_EVENT_CLOSE_REQUEST});
		return 1;
	}
	if ((H.cfg.keep_running || H.cfg.background) && !H.quitting) {
		lx_save_window(w);
		gtk_widget_set_visible((GtkWidget *)w->window, 0);
		return 1;
	}
	// Closing the app's last window quits it: on quit's handlers may keep
	// it open.
	if (lx_closing_last(w) && !lx_ask_quit(false)) return 1;
	lx_save_window(w);
	return 0;
}

win *lx_new_main_window(void) {
	win *w = lx_new_win(g_object_new(lx_window_type, "application", H.app, NULL));  // an AdwApplicationWindow (view.c)
	const char *t = getenv("ASUKA_TEST");
	if (t && *t) gtk_window_set_decorated(w->window, 0);
	str_setc(&w->name, H.app_name);
	gtk_window_set_title(w->window, H.app_name);
	adw_window_title_set_title(w->title, H.app_name);
	gtk_window_set_default_size(w->window, 800, 600 + AK_HEADER_HEIGHT);
	g_signal_connect(w->window, "close-request", G_CALLBACK(main_close_request), w);
	return w;
}

static void on_active(GObject *o, GParamSpec *ps, gpointer data) {
	(void)ps;
	if (gtk_window_is_active((GtkWindow *)o)) H.active_win = data;
}

// lx_new_win wraps a GtkWindow in the host's structure.
win *lx_new_win(GtkWindow *window) {
	win *w = calloc(1, sizeof *w);
	if (!w) abort();
	w->window = window;
	w->toolbar = (AdwToolbarView *)adw_toolbar_view_new();
	w->header = (AdwHeaderBar *)adw_header_bar_new();
	w->title = (AdwWindowTitle *)adw_window_title_new("", "");
	g_object_ref_sink(w->title);  // kept while a tabs' switcher takes its place
	adw_header_bar_set_title_widget(w->header, (GtkWidget *)w->title);
	adw_toolbar_view_add_top_bar(w->toolbar, (GtkWidget *)w->header);
	w->content = lx_view_new();
	w->toasts = (AdwToastOverlay *)adw_toast_overlay_new();
	adw_toast_overlay_set_child(w->toasts, w->content);
	adw_toolbar_view_set_content(w->toolbar, (GtkWidget *)w->toasts);
	if (AK_IS(window, adw_application_window_get_type))
		adw_application_window_set_content((AdwApplicationWindow *)window, (GtkWidget *)w->toolbar);
	else
		adw_window_set_content((AdwWindow *)window, (GtkWidget *)w->toolbar);
	ak_vec_push(&H.wins, w);
	lx_route_keys(w);
	lx_watch_focus(w);
	lx_watch_window(w);
	g_signal_connect(window, "notify::is-active", G_CALLBACK(on_active), w);
	lx_watch_window_state(w);
	return w;
}

win *lx_win_by_content(GtkWidget *w) {
	if (H.main && H.main->content == w) return H.main;
	for (uint32_t i = 0; i < H.wins.len; i++)
		if (H.wins.data[i]->content == w) return H.wins.data[i];
	return NULL;
}

win *lx_win_by_window(gpointer window) {
	for (uint32_t i = 0; i < H.wins.len; i++)
		if ((gpointer)H.wins.data[i]->window == window) return H.wins.data[i];
	return NULL;
}

void lx_win_set_root(win *w, node *n) {
	if (w->root == n) return;
	if (w->root && w->root->widget && gtk_widget_get_parent(w->root->widget) == w->content) gtk_widget_unparent(w->root->widget);
	w->root = n;
	lx_detach(n->widget);
	gtk_widget_set_parent(n->widget, w->content);
	gtk_widget_queue_allocate(w->content);
}

static bool env_eq(const ak_environment *a, const ak_environment *b) {
	return a->width == b->width && a->height == b->height && a->safe_top == b->safe_top && a->safe_right == b->safe_right &&
		a->safe_bottom == b->safe_bottom && a->safe_left == b->safe_left && a->keyboard == b->keyboard && a->scale == b->scale &&
		a->font_scale == b->font_scale && a->dark == b->dark && a->reduce_motion == b->reduce_motion && a->screen_reader == b->screen_reader &&
		a->platform == b->platform && a->width_class == b->width_class && a->height_class == b->height_class &&
		ak_span_eq(a->locale, b->locale) && ak_span_eq(a->time_zone, b->time_zone) && ak_span_eq(a->languages, b->languages) &&
		a->first_weekday == b->first_weekday && a->hour_cycle == b->hour_cycle;
}

// lx_win_allocated: the window's content view was sized: the root takes
// the size, and the core hears of it.
void lx_win_allocated(win *w, int width, int height) {
	lx_desk_allocated(w);
	float fw = (float)width, fh = (float)height;
	if (w->root && w->root->widget) lx_allocate(w->root->widget, width, height, 0, 0, NULL);
	if (fw == w->env_w && fh == w->env_h) return;
	w->env_w = fw;
	w->env_h = fh;
	lx_queue_window_state(w);
	lx_size_covers(w);
	lx_dev_error_resized(w);
	if (w->node) return;  // a Window's node is its root: it reports its own environment
	ak_environment env;
	lx_win_env(w, &env);
	if (env_eq(&env, &H.env)) return;
	H.env = env;
	if (H.root) {
		ak_event ev = {.id = H.root->id, .kind = AK_EVENT_ENVIRONMENT, .env = env};
		lx_dispatch(&ev);
	}
}

static bool bool_prop(gpointer o, const char *name) {
	gboolean b = 0;
	g_object_get(o, name, &b, NULL);
	return b != 0;
}

// ak_environment is a window's: its content's size, its display's scale,
// the desktop's settings.
void lx_win_env(win *w, ak_environment *env) {
	memset(env, 0, sizeof *env);
	env->width = w->env_w;
	env->height = w->env_h;
	env->scale = 1;
	env->font_scale = H.font_scale;
	env->dark = H.dark;
	env->platform = AK_PLATFORM_LINUX;
	snprintf(H.env_locale, sizeof H.env_locale, "%s", lx_posix_locale());
	snprintf(H.env_tz, sizeof H.env_tz, "%s", lx_time_zone());
	snprintf(H.env_langs, sizeof H.env_langs, "%s", S(H.languages));
	env->locale = ak_cstr(H.env_locale);
	env->languages = ak_cstr(H.env_langs);
	env->time_zone = ak_cstr(H.env_tz);
	if (env->width == 0 && env->height == 0) {
		int dw = 0, dh = 0;
		gtk_window_get_default_size(w->window, &dw, &dh);
		env->width = (float)dw;
		int h = dh - lx_header_room(w);
		env->height = (float)(h > 0 ? h : 0);
	}
	GtkNative *nat = gtk_widget_get_native(w->content);
	if (nat) {
		GdkSurface *s = gtk_native_get_surface(nat);
		if (s) env->scale = gdk_surface_get_scale ? (float)gdk_surface_get_scale(s) : (float)gdk_surface_get_scale_factor(s);
	}
	if (env->scale <= 0) env->scale = 1;
	if (w->cfg.title_bar == AK_TITLE_BAR_HIDDEN) {
		// The content goes under the header bar: SafeArea keeps clear of
		// it, its window buttons.
		int top = adw_toolbar_view_get_top_bar_height(w->toolbar);
		env->safe_top = (float)(top > 0 ? top : AK_HEADER_HEIGHT);
	}
	GtkSettings *s = gtk_settings_get_default();
	if (s) env->reduce_motion = !bool_prop(s, "gtk-enable-animations");
	env->screen_reader = H.screen_reader;
	env->hour_cycle = H.hour_cycle;
	env->width_class = ak_width_class_of(env->width);
	env->height_class = ak_height_class_of(env->height);
}

// lx_env_changed reports every window's environment again.
void lx_env_changed(void) {
	if (!H.main) return;
	ak_environment env;
	lx_win_env(H.main, &env);
	if (!env_eq(&env, &H.env)) {
		H.env = env;
		if (H.root) {
			ak_event ev = {.id = H.root->id, .kind = AK_EVENT_ENVIRONMENT, .env = env};
			lx_dispatch(&ev);
		}
	}
	lx_roots_env_changed();
}

static void on_dark(GObject *sm, GParamSpec *ps, gpointer data) {
	(void)ps;
	(void)data;
	H.dark = adw_style_manager_get_dark((AdwStyleManager *)sm);
	if (H.accent_dark) lx_update_css();  // the style's accent
	lx_theme_changed();
	lx_tray_style_changed();
}

static void on_contrast(GObject *sm, GParamSpec *ps, gpointer data) {
	(void)sm;
	(void)ps;
	(void)data;
	lx_theme_changed();
}

static void on_setting(GObject *s, GParamSpec *ps, gpointer data) {
	(void)s;
	(void)ps;
	(void)data;
	free(H.base.family);
	H.base = lx_system_font();
	H.font_scale = H.dev_font_scale > 0 ? H.dev_font_scale : lx_text_scale();
	lx_fonts_changed();
}

// lx_watch_settings follows the desktop's settings: dark style,
// animations, text scaling, the font.
void lx_watch_settings(void) {
	AdwStyleManager *sm = adw_style_manager_get_default();
	g_signal_connect(sm, "notify::dark", G_CALLBACK(on_dark), NULL);
	g_signal_connect(sm, "notify::high-contrast", G_CALLBACK(on_contrast), NULL);
	GtkSettings *s = gtk_settings_get_default();
	if (s) {
		g_signal_connect(s, "notify::gtk-enable-animations", G_CALLBACK(on_setting), NULL);
		g_signal_connect(s, "notify::gtk-xft-dpi", G_CALLBACK(on_setting), NULL);
		g_signal_connect(s, "notify::gtk-font-name", G_CALLBACK(on_setting), NULL);
	}
}

// folder_label is the folder of the file at path as a title bar shows
// it: the home folder as ~ (malloc'd).
static char *folder_label(const char *path) {
	if (str_empty(path)) return strdup("");
	char *dir = g_path_get_dirname(path);
	const char *home = g_get_home_dir();
	char *out;
	size_t hn = home ? strlen(home) : 0;
	if (hn && strcmp(dir, home) == 0)
		out = strdup("~");
	else if (hn && strncmp(dir, home, hn) == 0 && dir[hn] == '/') {
		out = malloc(strlen(dir) - hn + 2);
		if (!out) abort();
		sprintf(out, "~%s", dir + hn);
	} else
		out = strdup(dir);
	g_free(dir);
	return out;
}

void lx_update_title(win *w) {
	char *t;
	if (w->edited) {
		// As gedit and GNOME Text Editor mark it.
		t = malloc(strlen(S(w->name)) + 2);
		if (!t) abort();
		sprintf(t, "*%s", S(w->name));
	} else {
		t = strdup(S(w->name));
	}
	adw_window_title_set_title(w->title, !str_empty(w->chrome.title) ? w->chrome.title : t);
	if (!str_empty(w->file) || w->subtitled) {
		// A document's folder under its name, as GNOME Text Editor shows it.
		char *f = folder_label(w->file);
		adw_window_title_set_subtitle(w->title, f);
		free(f);
		w->subtitled = !str_empty(w->file);
	}
	gtk_window_set_title(w->window, !str_empty(w->chrome.title) && !w->node ? w->chrome.title : t);
	free(t);
}

// lx_set_window_options applies WindowOptions to the main window, before
// it shows.
// An app without a main window (WindowNone) never shows it; one reloaded
// with a main window again shows it.
void lx_set_window_options(ak_span args) {
	ak_window_config c = ak_decode_window_config((const uint8_t *)args.p, args.len);
	win *w = H.main;
	if (!w) return;
	bool was_none = H.no_main;
	H.no_main = (c.flags & AK_WINDOW_NONE) != 0;
	if (c.title.len) str_set(&w->name, c.title);
	lx_update_title(w);
	lx_configure(w, &c);
	if (!H.started) return;
	if (H.no_main && gtk_widget_get_visible((GtkWidget *)w->window)) gtk_widget_set_visible((GtkWidget *)w->window, 0);
	else if (was_none && !H.no_main && !w->hidden) lx_show_win(w, true);
}

void lx_free_win(win *w) {
	for (uint32_t i = 0; i < H.wins.len; i++)
		if (H.wins.data[i] == w) {
			memmove(H.wins.data + i, H.wins.data + i + 1, (H.wins.len - i - 1) * sizeof *H.wins.data);
			H.wins.len--;
			break;
		}
	if (H.active_win == w) H.active_win = NULL;
	lx_dev_error_closing(w);
	ak_vec_free(&w->desk.region);
	// Its memory stays: signals of its window may still come as GTK
	// finishes destroying it, and nodes may name it; it's small.
}
