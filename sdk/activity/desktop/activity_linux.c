// platform/activity on Linux: on each of the app's windows (and the ones
// it opens: gtk_window_get_toplevels' changes), controllers in the
// capture phase, which see an event before the window's widgets do and
// never claim nor stop it: a click gesture for any button's press and a
// touch's beginning, a key controller for keys, a scroll controller for
// the wheel and the touchpad; each reaches a popover's widgets too.
// Times are CLOCK_BOOTTIME's, which goes on while the computer sleeps.
// Main thread, where GTK's signals come.
#include "asuka_linux.h"
#include "activity.asuka.h"

#include <stdlib.h>
#include <time.h>

#ifndef CLOCK_BOOTTIME
#define CLOCK_BOOTTIME 7
#endif

typedef struct listener {
	activity_fn input;
	asuka_call *call;
	struct listener *next;
} listener;

static int64_t last;  // when the last input was, in nanoseconds
static int64_t told;  // when the listeners last heard of one
static listener *listeners;
static bool watching;

static int64_t now(void) {
	struct timespec ts;
	clock_gettime(CLOCK_BOOTTIME, &ts);
	return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

static void input(void) {
	int64_t t = now();
	last = t;
	if (!listeners || t - told < 1000000000) return;
	told = t;
	for (listener *l = listeners; l; l = l->next) activity_fn_call(l->input);
}

static void on_press(gpointer g, gint n, gdouble x, gdouble y, gpointer d) { input(); }

static gboolean on_key(gpointer c, guint keyval, guint keycode, GdkModifierType state, gpointer d) {
	input();
	return 0;
}

static gboolean on_scroll(gpointer c, gdouble dx, gdouble dy, gpointer d) {
	input();
	return 0;
}

static void capture(GtkWidget *w, GtkEventController *c, const char *signal, GCallback fn) {
	G(gtk_event_controller_set_propagation_phase)(c, GTK_PHASE_CAPTURE);
	if (G(gtk_event_controller_set_propagation_limit)) G(gtk_event_controller_set_propagation_limit)(c, GTK_LIMIT_NONE);
	g_signal_connect(c, signal, fn, NULL);
	G(gtk_widget_add_controller)(w, c);
}

// observe watches a window (once).
static void observe(GtkWidget *w) {
	if (!w || G(g_object_get_data)((GObject *)w, "asuka-activity")) return;
	G(g_object_set_data)((GObject *)w, "asuka-activity", (gpointer)1);
	GtkGesture *press = G(gtk_gesture_click_new)();
	G(gtk_gesture_single_set_button)((GtkGestureSingle *)press, 0);
	capture(w, (GtkEventController *)press, "pressed", (GCallback)on_press);
	capture(w, G(gtk_event_controller_key_new)(), "key-pressed", (GCallback)on_key);
	capture(w, G(gtk_event_controller_scroll_new)(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES), "scroll", (GCallback)on_scroll);
}

static void observe_from(GListModel *tops, guint from, guint n) {
	for (guint i = from; i < from + n; i++) {
		GtkWidget *w = G(g_list_model_get_item)(tops, i);
		observe(w);
		if (w) G(g_object_unref)(w);
	}
}

static void on_toplevels(GListModel *tops, guint at, guint removed, guint added, gpointer d) { observe_from(tops, at, added); }

// watch starts, once GTK is (the app's host starts it before the app;
// this runs again as the watcher is read).
static void watch(void) {
	if (watching || !G(gtk_is_initialized) || !G(gtk_is_initialized)()) return;
	watching = true;
	GListModel *tops = G(gtk_window_get_toplevels)();
	observe_from(tops, 0, G(g_list_model_get_n_items)(tops));
	g_signal_connect(tops, "items-changed", (GCallback)on_toplevels, NULL);
}

static int64_t m_idle(activity_Watcher *self, asuka_error *err) {
	watch();
	int64_t d = now() - last;
	return d > 0 ? d : 0;
}

static void stop(void *ctx) {
	listener *l = ctx;
	for (listener **q = &listeners; *q; q = &(*q)->next)
		if (*q == l) {
			*q = l->next;
			break;
		}
	asuka_error e = {0};
	asuka_error_cancelled(&e);
	asuka_call_fail(l->call, &e);
	free(l);
}

static void m_watch(activity_Watcher *self, activity_fn in, asuka_call *call) {
	watch();
	listener *l = calloc(1, sizeof *l);
	l->input = in;
	l->call = call;
	l->next = listeners;
	listeners = l;
	asuka_call_on_cancel(call, stop, l);
}

static const activity_Watcher_vtable vt = {.idle = m_idle, .watch = m_watch};

activity_Watcher *activity_make_Watcher(asuka_error *err) {
	if (!asuka_linux()) {
		asuka_error_unsupported(err, "no GLib");
		return NULL;
	}
	if (!last) last = now();
	watch();
	activity_Watcher *w = malloc(sizeof *w);
	w->vt = &vt;
	return w;
}
