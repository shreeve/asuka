// host.h: the Windows host's own declarations (RFC 0008's H2): Win32 and
// the common controls v6, GDI+'s flat API, DWM and uxtheme, in C17, the
// C core linked in. DESIGN.md's "How the Windows host maps Asuka" says what
// it does; each file of src/ is a part of it.
//
// One thread: the core, every window and every call here run on the main
// thread, which runs the message loop (main.c). Other threads reach it with
// wh_post. The core's batches are decoded with its own wire decoder
// (core/src/ops: ak_batch_next, the payloads' decoders), in place.
#ifndef ASUKA_WIN_HOST_H
#define ASUKA_WIN_HOST_H

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <ctype.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asuka.h"
#include "asuka_desktop.h"
#include "desktop.h"
#include "ops/ops.h"

#define WH_LEN(a) (sizeof(a) / sizeof((a)[0]))
static inline float wh_minf(float a, float b) { return a < b ? a : b; }
static inline float wh_maxf(float a, float b) { return a > b ? a : b; }
static inline int wh_mini(int a, int b) { return a < b ? a : b; }
static inline int wh_maxi(int a, int b) { return a > b ? a : b; }
static inline float wh_clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline double wh_clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

// ---- Memory and strings ----

void *wh_alloc(size_t n);  // zeroed; aborts on failure
void *wh_realloc(void *p, size_t n);
char *wh_strdup(const char *s);
char *wh_strndup(const char *s, size_t n);

// A UTF-16 string a view keeps: set again, it reuses its memory.
typedef struct {
	wchar_t *p;  // NUL-terminated (NULL while empty and never set)
	int n;       // its length in UTF-16 units
	int cap;
} wstr;

void wstr_set(wstr *s, ak_span utf8);
void wstr_setw(wstr *s, const wchar_t *w, int n);
void wstr_free(wstr *s);
static inline const wchar_t *wstr_c(const wstr *s) { return s->p ? s->p : L""; }

// wh_wide is utf8 as a new NUL-terminated UTF-16 string (free it);
// wh_utf8 the other way.
wchar_t *wh_wide(const char *utf8, int n);
char *wh_utf8(const wchar_t *w, int n, int *len);
static inline wchar_t *wh_wspan(ak_span s) { return wh_wide(s.p, (int)s.len); }

// A map of 64-bit keys to pointers (open addressing): the nodes by ID, by
// window.
typedef struct {
	uint64_t *keys;
	void **vals;
	uint32_t cap, len;
} wh_map;

void *wh_map_get(const wh_map *m, uint64_t k);
void wh_map_put(wh_map *m, uint64_t k, void *v);
void wh_map_del(wh_map *m, uint64_t k);
// Iterating: i from 0 while wh_map_next finds one.
bool wh_map_next(const wh_map *m, uint32_t *i, uint64_t *k, void **v);

// ---- Colors ----

typedef struct {
	uint8_t r, g, b, a;
} rgba;

static inline COLORREF rgba_ref(rgba c) { return RGB(c.r, c.g, c.b); }
static inline ARGB rgba_argb(rgba c) { return (ARGB)c.a << 24 | (ARGB)c.r << 16 | (ARGB)c.g << 8 | c.b; }
static inline rgba rgba_hex(uint32_t v) { return (rgba){(uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v, 255}; }
rgba rgba_alpha(rgba c, float a);
rgba rgba_over(rgba c, rgba bg);  // c drawn over an opaque background: GDI text has no alpha

// Windows 11's colors (WinUI's Fluent theme resources), light or dark,
// with the system's accent (theme.c).
typedef struct {
	bool dark;
	rgba label, secondary, tertiary, disabled;
	rgba bg, card, layer, separator, stroke, card_stroke;
	rgba control, control_hot, control_pressed, control_stroke;
	rgba subtle_hot, subtle_pressed;
	rgba accent, accent_hot, accent_pressed, on_accent, accent_text;
	rgba red, orange, yellow, green, blue, purple, pink, gray;
	rgba knob, track, field_bg, field_focus_bg;
} palette;

// ---- Views ----

// Kinds of the views the host makes itself, past the schema's.
enum {
	KIND_ROW = 0xF000,
	KIND_CELL,
	KIND_HEADER,
	KIND_TAB_BAR,
	KIND_SHEET_WINDOW,
	KIND_SCROLL_BAR,
	KIND_WINDOW_FRAME,   // a Window's top-level window
	KIND_POPOVER_WINDOW, // a Popover's popup window
	KIND_SIDEBAR,        // a SplitView's pane
	KIND_COMMAND_BAR,    // a stack's command bar (cmdbar.c)
	KIND_ALERT_WINDOW,   // an alert's dialog (alertdialog.c)
	KIND_PICTURE,        // a view drawn for another (picture.c)
	KIND_DEV_ERROR,      // asuka dev's error overlay's buttons (deverror.c)
};

// An item of a menu of actions (a Menu prop, a Screen's bar buttons, an
// Alert's buttons), its items a submenu's or a section's.
typedef struct action {
	uint8_t role;  // AK_ALERT_BUTTON_ROLE_*
	wchar_t *title;
	char *icon;
	int64_t index;  // its number, depth first: what Action carries
	uint8_t flags;  // AK_MENU_*
	struct action *items;
	uint32_t count;
} action;

typedef struct {
	action *v;
	uint32_t n;
} actions;

void actions_decode(actions *a, const uint8_t *b, size_t n);
void actions_free(actions *a);
actions actions_flat(const actions *a);  // numbered by place, without children

// A view's key (its Keys prop): a name and modifiers (keys.c).
typedef struct {
	char name[16];
	uint8_t mods;
} wh_key;

typedef struct node node;

// Kind-specific state, each its file's.
typedef struct wh_list wh_list;
typedef struct wh_rowview wh_rowview;
typedef struct wh_cell wh_cell;
typedef struct wh_nav wh_nav;
typedef struct wh_rich wh_rich;
typedef struct wh_image wh_image;
typedef struct wh_shape wh_shape;
typedef struct wh_web wh_web;
typedef struct wh_snack wh_snack;
typedef struct wh_scrollbar wh_scrollbar;
typedef struct wh_refresher wh_refresher;
typedef struct wh_picture wh_picture;
typedef struct wh_shader wh_shader;
typedef struct wh_split wh_split;
typedef struct wh_native wh_native;
typedef struct wh_field wh_field;  // a Rich TextField's (richfield.c)
typedef struct wh_uia wh_uia;      // a view's UI Automation provider

// A transform (Style.Transform): translation in points, scale, rotation
// in degrees clockwise, around the view's center.
typedef struct {
	float tx, ty, sx, sy, rot;
} xform;

struct node {
	uint32_t id;
	uint16_t kind;
	HWND hwnd;  // its window
	HWND ctl;   // a control inside it (a TextField's EDIT)
	HWND ctl2;  // a second one (a Stepper's plus)
	node **kids;
	uint32_t nkids, capkids;
	node *parent;
	// Destroyed: no longer the core's, its events go nowhere. Its memory
	// goes once the message the main loop dispatched has returned
	// (wh_free_dead): the handler that emitted the event that destroyed
	// it may still name it.
	bool dead;
	uint64_t listens[2];  // bits by event kind
	uint8_t dir;          // AK_LAYOUT_DIRECTION_*
	float frame[4];       // x, y, width, height in points, in its native parent
	float k;              // pixels per point of its top-level window; 0: the main window's

	// Text props.
	int32_t text_style, font_weight, text_align;
	float font_size;
	char *font_family;
	ak_color color;
	int line_limit;

	// Visual props.
	ak_color background, border_color;
	float corner_radius, border_width;
	uint8_t *gradient;
	uint32_t gradient_len;
	uint8_t *shadow;
	uint32_t shadow_len;
	float opacity;
	bool hidden;
	xform xf;
	float zoom;            // the scale it gives what's in it; 0: 1
	wh_picture *pict;      // its picture, rotated or faded (picture.c)
	int32_t material;      // Style.Material
	bool clip;             // Style.Clip
	HRGN clip_rgn;         // its rounded clip's region (clip.c)
	double clip_r;         // and its radius, in pixels
	bool disabled;

	actions menu;
	int32_t menu_trigger;

	// The mouse on a view of the host's own drawing.
	bool hot, pressed, long_fired, panning, tracking, releasing, space_down, focused, placed, dying;
	POINT press_at, pan_last;
	int64_t pan_time;  // QPC ticks
	void *two_finger;  // a pinch or rotate under way (gesture.c)
	node *search_of;   // the header whose search field it is (search.c)
	node *alert_of;    // a field of an alert's dialog: the alert
	wh_native *native; // a Native's: the app's view (native.c)
	wh_refresher *refresh;
	uint8_t *transition;
	uint32_t transition_len;
	char *match;  // Style.Match's key

	// Drag and drop.
	char **drop_accept;
	uint32_t ndrop_accept;
	uint8_t *drag_items;  // DragItems, as the wire has them
	uint32_t drag_items_len;

	// Kind-specific.
	wstr title, message;  // Button, Screen, Alert
	int32_t variant;
	char *icon;
	wstr badge;    // a tab's
	actions acts;  // a Screen's bar buttons, an Alert's buttons
	int32_t alert_style;
	wstr text;     // Text; a TextField's last reported value
	wstr placeholder;
	bool multiline, plain, setting_text, secure, rich_text;
	char **paste_types;
	uint32_t npaste_types;
	wh_field *field;   // a Rich TextField's (richfield.c)
	float inset[4];    // ContentInset: top, right, bottom, left points
	uint32_t selection[2];  // a TextField's (UTF-16)
	bool sel_queued, edit_rtl;
	uint32_t edit_side;
	int32_t keyboard;
	wchar_t **items;  // Segmented, Wheel, Picker
	uint32_t nitems;
	wstr picker_label;
	HWND combo_list;
	bool combo_hot;
	int selected;
	int pages;
	bool paging;
	int32_t wheel_sum;
	double value, shown, min, max, step;
	bool on, indeterminate, animating;
	double phase;
	int32_t date_mode;
	bool date_inline;
	bool horizontal;
	float scroll_x, scroll_y, scroll_end, content_w, content_h;
	int32_t wheel_acc;
	wh_image *image;
	char *symbol;
	wh_list *list;
	wh_scrollbar *bar;   // a Scroll's or List's thumb
	wh_scrollbar *sbar;  // the thumb a KIND_SCROLL_BAR view is
	wh_rowview *row;
	wh_cell *cell;
	wh_nav *nav;
	wh_snack *snack;
	wh_rich *rich;
	wh_shape *shape;
	wh_web *web;
	wh_shader *shader;
	wh_split *split;
	node *pane_node;  // a SplitView's pane
	bool presented;
	int64_t detents;
	bool intercept_back;
	wstr tooltip;
	wh_key *keys;
	uint32_t nkeys;
	uint8_t *canvas;
	uint32_t canvas_len;
	bool checkbox;
	wstr a11y_label, a11y_hint;
	int32_t a11y_role;
	bool a11y_hidden, adjustable;
	// RFC 0010's: the keyboard's focus (Focusable set at all: a control's
	// tab stop follows it), screen readers' states and relations.
	bool focusable, focusable_set, no_focus_on_press, no_focus_ring, focus_trap;
	int32_t a11y_checked, a11y_expanded;  // AK_A11Y_CHECKED_*, AK_A11Y_EXPANDED_*
	bool a11y_selected, a11y_modal, has_range;
	wstr a11y_value;
	double range[3];  // A11yRange: min, max, now
	uint32_t a11y_controls, a11y_labelled_by, a11y_described_by;
	uint16_t key_surrogate;  // a WM_CHAR's high surrogate, waiting for its low (KeyText)
	wchar_t **a11y_actions;
	uint32_t na11y_actions;
	wh_uia *uia;
	ak_color color_value;
	// A host-made view's messages, before the view's own.
	LRESULT (*host_msg)(node *n, UINT m, WPARAM wp, LPARAM lp, bool *handled);
	void *host_data;
};

static inline bool node_listening(const node *n, int e) { return e < 128 && (n->listens[e / 64] >> (e % 64) & 1); }

// ---- The host ----

typedef struct wh_wctl wh_wctl;  // a window the app controls (winctl.c)
typedef struct wh_tray wh_tray;

typedef struct {
	void (*fn)(void *ctx);
	void *ctx;
} wh_job;

typedef struct {
	wh_job *v;
	uint32_t n, cap;
} wh_jobs;

typedef struct {
	ak_environment env;
	char locale[64], time_zone[64], languages[512];
} wh_env;

typedef struct {
	wh_map nodes;   // by ID
	wh_map by_hwnd; // by window: its view's or control's node
	node *root;
	HWND window;    // the app's window
	HWND park;      // a hidden window views wait in until they're inserted
	HWND msg_win;   // a message-only window: wh_post's wake-ups
	HWND tips;      // the tooltip control, made with the first tooltip
	wh_env env, core_env;
	int dpi;
	float scale;  // pixels per point: dpi / 96
	bool dark;
	palette theme;
	ak_color accent;
	ak_color accent_dark;  // the accent in dark mode (0: accent's)
	HDC measure_dc;

	int depth;          // calls into the core in progress
	ak_buf pending;     // events raised meanwhile: u32 length, then each
	size_t pending_at;  // where the next to send starts
	wh_jobs later;      // work to do once the core has returned
	ak_buf evbuf;       // an event being encoded
	node **dead;        // nodes destroyed, freed by wh_free_dead
	uint32_t ndead, capdead;

	SRWLOCK post_mu;
	wh_jobs posted;
	bool post_sent;

	uint32_t next_cell;  // the host's own views' IDs (AK_HOST_ID_BIT)
	char *app_name, *app_id, *assets;
	wchar_t *app_namew;
	dsk_config config;
	dsk_dirs dirs;
	char locale[64];  // the language the app shows
	bool rtl;         // and its direction
	char saved_locale[64];
	char *locale_path;

	bool batch_frames;
	int modal;  // modal loops (menus, dialogs) running
	bool quitting, started, snapshotting, scripted;
	bool launched_for_toast;  // COM started the app for a toast's click
	bool keep_running;  // the app keeps running with no window (app.KeepRunning)
	bool no_main;       // the app has no main window (WindowNone): H.window never shows

	node **sheets;
	uint32_t nsheets;
	node **wins;  // the Windows open, in the order they opened
	uint32_t nwins;
	HWND in_use;  // the app's window activated last
	float min_size[2];

	wh_wctl *main_ctl;
	wh_map ctls;    // the windows the app controls, by window
	wh_map chrome;  // their hidden title bars' windows
	dsk_winstate *states;
	dsk_routes *routes;

	int round_clips, materials, zoomed;
	node *drawing;  // the view a picture draws
	HWND skip_tree;

	uint8_t key_ups[256];  // keys a view took: their letting go goes nowhere
	int scheme;  // ASUKA_COLOR_SCHEME, or the Appearance command: 0 the system's, 1 light, 2 dark
} wh_host;

extern wh_host H;

// ---- host.c: nodes, batches, events, posting ----

node *wh_node(uint32_t id);
node *wh_node_of(HWND hwnd);
void wh_bind(node *n, HWND hwnd);  // n answers for hwnd's messages
void wh_unbind(HWND hwnd);
node *wh_node_new(uint32_t id, uint16_t kind);
uint32_t wh_host_id(void);  // a new ID of a view the host makes
void wh_node_add(node *n);

void wh_post(void (*fn)(void *ctx), void *ctx);  // any thread
void wh_run_posted(void);
void wh_after_core(void (*fn)(void *ctx), void *ctx);
void wh_enter(void (*fn)(void *ctx), void *ctx);  // a call into the core
void wh_core_begin(void);
void wh_core_end(void);
// wh_emit sends e to the core if it listens on n (e->id is set to n's).
void wh_emit(node *n, ak_event *e);
// wh_dispatch sends e to the core, or queues it while a call into it is
// in progress.
void wh_dispatch(const ak_event *e);
void wh_emit_kind(node *n, int kind);
void wh_emit_bool(node *n, int kind, bool b);
void wh_emit_int(node *n, int kind, int64_t i);
void wh_emit_float(node *n, int kind, double f);
void wh_emit_str(node *n, int kind, const char *s, size_t len);
void wh_emit_wstr(node *n, int kind, const wchar_t *s, int len);
void wh_emit_bytes(node *n, int kind, const void *b, size_t len);

void wh_apply(const uint8_t *buf, size_t len);
void wh_measure(uint32_t id, float w, float h, float aw, float ah, float *ow, float *oh);
void wh_start_core(void);

void wh_insert(node *p, node *c, uint32_t index);
void wh_remove(node *p, node *c);
void wh_destroy(uint32_t id);
void wh_release(node *n);
void wh_free_dead(void);
void wh_forget_tree(HWND hwnd);
void wh_set_root(node *n);
void wh_set_frame(node *n, const float f[4]);
void wh_place_frame(node *n);
void wh_place_at(node *n, const float f[4]);
void wh_resized(node *n);
float wh_scale_of(const node *n);
float wh_scale_of_k(float k);
int wh_dpi_of(const node *n);
int wh_px(const node *n, float v);
float wh_pt(const node *n, int v);
HWND wh_top_of_children(node *p);
void wh_adopt(node *p, node *c);
bool wh_is_rtl(const node *n);
bool wh_reads_rtl(const node *n);
bool wh_first_strong_rtl(const wchar_t *s, int len, bool none);
void wh_fail(const char *fmt, ...);
void wh_log(const char *fmt, ...);

// ---- window.c: the app's window, classes, the environment ----

#define WH_CLASS_WINDOW L"AsukaWindow"
#define WH_CLASS_VIEW L"AsukaView"
#define WH_CLASS_MESSAGE L"AsukaMessage"
#define WH_CLASS_FLYOUT L"AsukaFlyout"
#define WH_CLASS_OWN L"AsukaOwn"

enum {
	WM_RUN_POSTED = WM_APP + 1,
	WM_FRAME = WM_APP + 2,
	WM_SPEECH = WM_APP + 3,
	WM_TRAY = WM_APP + 4,
	WM_DEFERRED = WM_APP + 5,
};

enum {
	TIMER_LONG_PRESS = 1,
	TIMER_SPIN = 2,
	TIMER_ANIMATE = 3,
	TIMER_FRAME = 4,
	TIMER_BARS = 5,
	TIMER_HOVER = 6,
	TIMER_SHADER = 7,
	TIMER_PINCH = 8,
	TIMER_SNACK = 9,
	TIMER_ROUTES = 10,
	TIMER_SCRIPT = 11,
};

void wh_register_classes(void);
LRESULT CALLBACK wh_wndproc(HWND hwnd, UINT m, WPARAM wp, LPARAM lp);
void wh_make_window(void);
void wh_environment(wh_env *e);
void wh_environment_changed(void);
void wh_report_environment(node *root, const wh_env *e);
void wh_dpi_changed(int dpi, const RECT *suggested);
void wh_rescale(HWND hwnd, float k);
void wh_setting_changed(const wchar_t *what);
void wh_set_dark(bool dark);
void wh_lifecycle(int state);
bool wh_key_message(MSG *m);
void wh_least_size(float out[2]);
LRESULT wh_parent_message(UINT m, WPARAM wp, LPARAM lp, bool *handled);
HWND wh_key_window(HWND hwnd);
HWND wh_window_in_use(void);

// ---- views.c: making views, their props ----

HWND wh_make_view(DWORD style);
void wh_make_control(node *n, const wchar_t *cls, DWORD style, DWORD ex);
void wh_make_control_in(node *n, const wchar_t *cls, DWORD style, DWORD ex);
void wh_create(uint32_t id, uint16_t kind);
void wh_set_own(node *n, uint16_t p, const ak_value *v);
void wh_set_common(node *n, uint16_t p, const ak_value *v);
void wh_theme_control(node *n);
void wh_listen(node *n, int e);
void wh_control_command(node *n, HWND ctl, UINT code);
LRESULT wh_notify(node *n, NMHDR *hdr, LPARAM lp, bool *handled);
HBRUSH wh_control_colors(node *n, HDC dc, UINT m);
LRESULT wh_view_message(node *n, HWND hwnd, UINT m, WPARAM wp, LPARAM lp, bool *handled);
void wh_redraw(node *n);
void wh_redraw_control(node *n);
void wh_apply_hidden(node *n);
void wh_apply_font(node *n);
bool wh_pressable(const node *n);
void wh_update_tab_stop(node *n);
bool wh_takes_focus(const node *n);
bool wh_interactive(const node *n);
bool wh_tracks_press(const node *n);
bool wh_draggable(const node *n);
bool wh_draws_states(const node *n);
void wh_spin(node *n);
void wh_set_direction(node *n, uint8_t d);
void wh_direction_changed(node *n);
void wh_apply_slider(node *n);
void wh_press(node *n, HWND hwnd);
void wh_long_press(node *n, HWND hwnd);
void wh_cancel_press(node *n);
void wh_set_pressed(node *n, bool on);
void wh_end_press(node *n, HWND hwnd, bool fire);
uint32_t wh_decode_strings(wchar_t ***out, const uint8_t *b, size_t n);
void wh_free_strings(wchar_t **s, uint32_t n);
uint32_t wh_decode_cstrings(char ***out, const uint8_t *b, size_t n);
void wh_free_cstrings(char **s, uint32_t n);
float wh_f32(const uint8_t *b);
uint32_t wh_u32(const uint8_t *b);
uint64_t wh_u64(const uint8_t *b);

// ---- theme.c: colors, fonts ----

void wh_palette(palette *p, bool dark, ak_color accent);
ak_color wh_accent(bool dark);
bool wh_resolve(ak_color c, rgba *out);
HBRUSH wh_brush(rgba c);
void wh_clear_brushes(void);
rgba wh_backdrop(const node *n);
rgba wh_text_color(const node *n, rgba def);

typedef struct {
	float size;
	int32_t weight;
	const char *family;
	bool italic, underline, strike, mono, gray;
	float k;  // pixels per point: its window's (0: the main window's)
} font_spec;

font_spec wh_spec(int32_t style, int32_t weight, float size, const char *family);
font_spec wh_node_spec(const node *n);
// wh_face is the family a spec is drawn in (buf: room for a named one).
const wchar_t *wh_face(const font_spec *s, wchar_t *buf, size_t cap);
HFONT wh_font(font_spec s);
HFONT wh_node_font(const node *n);
HFONT wh_body_font(const node *n);
HFONT wh_icon_font(const node *n, float size);
const wchar_t *wh_icon_face(void);
void wh_clear_fonts(void);
float wh_font_scale(void);
bool wh_windows11(void);

// ---- gdip.c: GDI+'s flat API ----

typedef GpGraphics *gfx;
gfx g_new(HDC dc);
void g_close(gfx g);
void g_fill_rect(gfx g, ARGB c, float x, float y, float w, float h);
void g_fill_round_rect(gfx g, ARGB c, float x, float y, float w, float h, float r);
void g_stroke_round_rect(gfx g, ARGB c, float width, float x, float y, float w, float h, float r);
void g_fill_ellipse(gfx g, ARGB c, float x, float y, float w, float h);
void g_draw_arc(gfx g, ARGB c, float width, int cap, float x, float y, float w, float h, float start, float sweep);
void g_draw_line(gfx g, ARGB c, float width, float x0, float y0, float x1, float y1);
GpPath *g_path(void);
void g_path_round_rect(GpPath *p, float x, float y, float w, float h, float r);
void g_clip_path(gfx g, GpPath *p);
void g_reset_clip(gfx g);
void g_draw_image(gfx g, GpBitmap *b, float alpha, float x, float y, float w, float h, float sx, float sy, float sw, float sh);
GpBitmap *g_bitmap(int w, int h, const uint8_t *pbgra);  // a copy of premultiplied BGRA
void wh_gdiplus_start(void);

// ---- paint.c ----

void wh_paint_window(node *n, HWND hwnd);
void wh_paint(node *n, HWND hwnd, HDC dc, RECT r);
void wh_paint_own(node *n, HWND hwnd, HDC dc, RECT r);
void wh_paint_behind(HWND hwnd, HDC dc);
void wh_paint_tree(HWND hwnd, HWND parent, HDC dc);
bool wh_opaque(const node *n);
void wh_focus_ring(const node *n, gfx g, float w, float h, float radius);
bool wh_keyboard_cues(HWND hwnd);
RECT wh_inset_rect(const node *n, RECT r);
UINT wh_text_format(const node *n);
rgba wh_color_behind(node *n);
float wh_own_alpha(const node *n);
rgba wh_bg_in(HWND hwnd);  // the background of the top-level window hwnd is in
rgba wh_bg_of(HWND top);
UINT wh_text_direction(bool rtl);
UINT wh_start_side(bool rtl);
bool wh_aligned_right(int32_t align, bool rtl);

// ---- text.c: measuring ----

void wh_measure_node(node *n, float cw, float ch, float aw, float ah, float *ow, float *oh);
SIZE wh_text_extent(HDC dc, const wchar_t *s, int n);

// ---- controls.c ----

LRESULT wh_draw_button(node *n, NMCUSTOMDRAW *cd);
LRESULT wh_draw_toggle(node *n, NMCUSTOMDRAW *cd);
LRESULT wh_draw_slider(node *n, NMCUSTOMDRAW *cd);
void wh_make_field(node *n);
void wh_make_edit(node *n);
void wh_set_field(node *n, uint16_t p, const ak_value *v);
void wh_layout_field(node *n);
void wh_align_field(node *n);
void wh_paint_field(node *n, gfx g, float w, float h);
void wh_layout_stepper(node *n);
void wh_select_segment(node *n, int i);
int wh_segment_at(node *n, int x);
void wh_paint_segmented(node *n, HDC dc, gfx g, RECT r);
void wh_slider_moved(node *n);
void wh_pick_color(node *n);
void wh_name_stepper(node *n);

// ---- symbols.c ----

uint16_t wh_symbol_glyph(const char *name);
uint16_t wh_directed_glyph(const char *name, bool rtl);
void wh_warn_symbol(const char *name);

// ---- words.c: Asuka's own words, in the app's language ----

const wchar_t *wh_word(const char *key);

#endif
