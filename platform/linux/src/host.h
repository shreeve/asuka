// host.h: the Linux host's own structures, shared by its files.
//
// The host is GTK 4 and libadwaita, driven from C17 (gtk.h's declarations,
// resolved as it starts), in the app's process, with the core linked in:
// the core calls the callbacks of asuka.h (apply a batch, measure a view,
// post), the host calls asuka_event and friends. Everything here runs on
// the main thread (GLib's default main context) but lx_post, which any
// thread may call.
//
// Each node the core makes is an AsukaView (view.c), a GtkWidget of the
// host's own class whose instance points back at its node: it draws the
// node's background, border, corners, gradient and shadow, and places its
// children at the frames the core gives them. A control (a label, a
// button, an entry) is the only child of its node's view, filling it less
// the content inset.
#ifndef AK_LINUX_HOST_H
#define AK_LINUX_HOST_H

#include "asuka.h"
#include "gtk.h"
#include "ops/ops.h"
#include "desktop.h"
#include "asuka_desktop.h"

typedef struct node node;
typedef struct win win;
typedef struct action action;
typedef AK_VEC(action) action_vec;
typedef AK_VEC(node *) node_vec;
typedef AK_VEC(GtkWidget *) widget_vec;

// Strings the host owns: NULL is "".
static inline const char *S(const char *s) { return s ? s : ""; }
void str_set(char **dst, ak_span s);
void str_setc(char **dst, const char *s);
char *str_dup(ak_span s);
static inline bool str_empty(const char *s) { return !s || !*s; }
static inline bool str_eq(const char *a, const char *b) { return strcmp(S(a), S(b)) == 0; }
// lx_buf_cstr is the buffer's bytes NUL-terminated (the NUL not counted).
const char *lx_buf_cstr(ak_buf *b);

// An action of a menu, as the host keeps it: an item and its submenu's
// or section's items.
struct action {
	uint8_t role;  // AK_ALERT_BUTTON_ROLE_*
	char *title, *icon;
	int64_t index;  // its number in its menu, depth first: what Action carries
	uint8_t flags;  // AK_MENU_*
	action_vec items;
};
static inline bool action_is(const action *a, uint8_t f) { return (a->flags & f) == f; }
void actions_decode(action_vec *out, ak_span b, int64_t base);
void actions_free(action_vec *v);
bool actions_same(const action_vec *a, const action_vec *b);
void actions_copy(action_vec *dst, const action_vec *src);

typedef struct {
	ak_color color;
	float blur, dx, dy;
} shadow_t;

typedef struct {
	ak_color color;
	float location;
} grad_stop;

typedef struct {
	bool set;
	float tx, ty, sx, sy;
	float rotation;  // degrees, clockwise
} transform_t;

// Area state, each in its own file.
typedef struct nav nav;
typedef struct scroll_view scroll_view;
typedef struct events events;
typedef struct field field;
typedef struct list_state list_state;
typedef struct tween tween;
typedef struct transition transition;
typedef struct image_extra image_extra;
typedef struct date_picker date_picker;
typedef struct dnd_state dnd_state;
typedef struct a11y_state a11y_state;
typedef struct search_field search_field;

struct node {
	ak_id id;
	ak_view_kind kind;
	GtkWidget *widget;  // its AsukaView
	GtkWidget *inner;   // the control in it (NULL: none), filling it less the inset
	node_vec children;
	node *parent;
	uint64_t listens[2];  // bits by event kind
	win *win;             // the window it's in, for a host-sized root (a Window's)
	bool dead;            // destroyed: its memory goes once nothing points at it

	float frame[4];  // x, y, width, height: points, in its parent
	bool placed;     // has had a frame
	bool hidden;

	// Text props.
	int32_t text_style, font_weight, text_align;
	float font_size;
	char *font_family;
	ak_color color;
	int line_limit;
	float inset[4];  // ContentInset: top, right, bottom, left

	// Visual props, drawn by the view.
	ak_color background, border_color;
	float corner_radius, border_width, opacity;
	shadow_t shadow;
	AK_VEC(grad_stop) gradient;
	float gradient_line[4];
	transform_t transform;
	bool clip;
	int32_t material;

	action_vec menu;  // the Menu prop
	bool has_menu;
	int32_t menu_trigger;

	// Kind-specific.
	char *title, *message;  // Button, Screen, Alert
	int32_t variant;
	char *icon;
	action_vec actions;  // a Screen's bar buttons, an Alert's buttons
	int32_t alert_style;
	char *text;  // Text, TextField's last reported value
	bool multiline, plain, secure;
	char *placeholder;
	AK_VEC(char *) items;  // Segmented, Wheel, Picker
	int selected;
	double value, min, max, step;
	char *source;  // Image
	int32_t image_mode;
	char *symbol;
	GdkTexture *texture;  // an Image's decoded picture
	image_extra *imgx;
	scroll_view *scroll;
	nav *nav;
	bool disabled;
	char *a11y_label;
	bool intercept_back;
	int32_t dir;  // LayoutDirection
	bool rtl;
	bool updating;  // the host is setting the control's value: no event
	gint64 pulsed;  // an indeterminate Progress's last pulse (µs)

	field *field;
	events *events;
	transition *transition;
	tween *tw;  // its animation in progress (anim.c)
	char *match_key;  // Style.Match's
	bool match_in;    // given its key during an animation: maybe coming
	date_picker *date;
	dnd_state *dnd;
	a11y_state *a11y;
	list_state *list;
	void *ext;  // an area's own state (a web view's, a video's, a canvas's…)
	void (*ext_free)(node *n);
	ak_key_vec keys;  // the Keys prop (names point into keys_buf)
	ak_buf keys_buf;
	GtkWidget *aux;  // a second widget of an area's (a list cell's row…)
	ak_id list_owner;  // a cell: the List it's bound into

	// The keyboard's focus (focus.c): the props as set, and the key
	// controller that sends KeyText.
	bool focusable, no_focus_on_press, no_focus_ring, focus_trap;
	GtkEventController *key_text;
	// Screen readers (a11y.c): A11yRole and A11yHidden as the app set
	// them; how many modal views over it hide it from screen readers.
	int32_t role;
	bool a11y_hidden;
	uint16_t modal_hidden;
};

static inline bool listening(const node *n, ak_event_kind e) {
	return n && e < 128 && (n->listens[e / 64] & (UINT64_C(1) << (e % 64)));
}

// A top-level window of the app's: the main one, holding the root, or a
// Window node's. Its content is an AdwToolbarView: the header bar (the
// window's title bar, with the innermost stack's back button, title and
// bar buttons), and under it a view of the host's that holds the root at
// its size.
typedef struct {
	bool back;
	char *title;
	action_vec actions;
	node *screen, *stack, *tabs, *split;
	char *search;
	int menus;
	widget_vec packed;
} chrome_t;

// A window's state as a panel, a flyout, a window sized to its content
// or one the mouse goes through (desk.c).
typedef struct {
	ak_flyout flyout;
	bool has_flyout;
	float fit_w, fit_h;
	bool fitted, wait_fit, wait_focus;
	guint wait_src;
	bool placed;  // put at its X, Y or centered once (not a panel or flyout)
	double placed_w, placed_h;
	bool through, region_set, clock_hook, on_top, states_set, hooked;
	AK_VEC(float) region;  // the input region's rects as last set: x, y, width, height in the window (points)
	GtkEventController *keys;  // a panel's or flyout's Escape
	guint dismiss_src;
	bool held;   // lost the keyboard to a window over it, which keeps it open
	bool layer;  // a layer-shell surface (Wayland)
} desk_t;

struct win {
	GtkWindow *window;  // AdwApplicationWindow, or AdwWindow
	AdwToolbarView *toolbar;
	AdwHeaderBar *header;
	AdwWindowTitle *title;
	GtkWidget *content;  // the host's AsukaView under the header
	AdwToastOverlay *toasts;
	search_field *search;
	bool restored;
	node *root;
	node *node;  // the Window node (NULL for the main window)

	char *name;  // its title (the app's name for the main window, unless set)
	bool edited;
	char *file;
	bool subtitled;
	float env_w, env_h;
	chrome_t chrome;
	bool closing;

	ak_window_config cfg;
	char *cfg_title, *cfg_key;  // cfg's spans, kept
	bool configured;
	bool hidden;  // kept hidden until a Show or Focus
	bool forced;  // closing for good: the core's Close
	char *state_key;
	GtkCssProvider *css;
	char *css_name;
	ak_window_state sent;
	bool reported;
	desk_t desk;
	GtkEventController *keys;  // its key controller (keys.c)
	ak_id focused;  // the node the keyboard's focus was last in (focus.c)
};

// The palette: libadwaita's colors for the current style.
typedef struct {
	GdkRGBA fg, bg, view, card, header;
	GdkRGBA accent, accent_bg, on_accent;
	GdkRGBA red, orange, yellow, green;
	GdkRGBA blue, purple, pink, gray;
} palette;

typedef struct {
	char *family;
	float px;  // its size in points at 96 DPI: Asuka's points
} base_font;

// An animation's timing (AnimateBegin's).
typedef struct {
	double duration, delay;  // seconds
	int curve;               // AK_ANIMATION_CURVE_*
	int32_t repeat;          // times more; -1 forever
	bool autoreverse;
	double damping, velocity;
} animation;
// anim_progress is how far an animation is after elapsed seconds: its
// curve's value, *done once it's over.
double anim_progress(const animation *a, double elapsed, bool *done);
double lx_now(void);  // seconds, monotonic

typedef void (*later_fn)(void *ctx);
typedef struct {
	later_fn fn;
	void *ctx;
} later_t;

// What a module's method answers with: once, on the main thread.
typedef struct reply reply;
typedef void (*method_fn)(ak_span args, reply *r);
typedef struct {
	const char *module, *name;
	method_fn fn;
} method_t;


typedef struct {
	// The nodes, by ID (an open-addressed table).
	node **table;
	uint32_t table_cap, table_len;
	node *root;
	GtkApplication *app;
	win *main;
	AK_VEC(win *) wins;
	ak_environment env;
	char env_locale[64], env_tz[64], env_langs[256];

	int depth;          // calls into the core in progress
	ak_buf pending;     // events raised meanwhile: u32 length, then the event
	size_t pending_off;  // the next to send
	AK_VEC(later_t) later;  // work to do once the core has returned
	ak_buf ev;  // an event's encoding, reused

	bool animating;
	animation anim;
	ak_color accent;
	ak_color accent_dark;  // the accent in dark mode (0: accent's)
	bool dark;
	palette theme;
	ak_id next_cell;
	char *assets;
	char *app_name, *app_id;
	base_font base;
	float font_scale;
	float dev_font_scale;  // asuka dev's preview's text size (DevFontScale): 0 the desktop's
	GtkCssProvider *css;

	char *locale;
	bool rtl;
	char *languages;

	int next_win;
	win *active_win;
	bool screen_reader;
	uint8_t hour_cycle;
	bool chrome_queued;
	int menus_version;
	GMenuModel *app_menu;
	AK_VEC(char *) app_actions;
	bool frames;
	bool in_background;
	bool quitting, quit_allowed;
	bool started;
	bool no_main;  // the app has no main window (WindowNone): H.main never shows
	dsk_config cfg;
	bool dev;  // under asuka dev: a player
	bool automating;
	GSimpleActionGroup *menu_group;  // the popped up menu's actions
	// The last input was a key (not a pointer's press): focus that comes
	// now is visible (focus.c).
	bool key_input;
	bool focus_moving;  // a focus trap is moving the focus: no FocusChange yet
} host;

extern host H;

// host.c
node *node_get(ak_id id);
void lx_dispatch(const ak_event *ev);
void lx_emit(node *n, ak_event ev);
void lx_after_core(later_fn fn, void *ctx);
// lx_post runs fn(ctx) on the main thread soon. Any thread.
void lx_post(later_fn fn, void *ctx);
void lx_enter_begin(void);
void lx_enter_end(void);
void lx_detach(GtkWidget *w);
void lx_insert(node *p, node *c, uint32_t index);
void lx_remove(node *p, node *c);
void lx_release(node *n);
void lx_fail(const char *fmt, ...) AK_PRINTF(1, 2);
void lx_set_inner(node *n, GtkWidget *c);
void lx_connect(node *n, gpointer obj, const char *sig, GCallback cb);
bool lx_automating(void);
void lx_log(const char *fmt, ...) AK_PRINTF(1, 2);
float lx_scale_of(node *n);
static inline bool dsk_rect_has(dsk_rect r, double x, double y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }
void lx_env_changed(void);
void lx_queue_draw(node *n);
void lx_apply_batch(const uint8_t *buf, size_t len);
void lx_measure_cb(uint32_t id, float width, float height, float aw, float ah, float *ow, float *oh);

// view.c
extern GType lx_view_type, lx_window_type;
void lx_register_classes(void);
GtkWidget *lx_view_new(void);
bool lx_is_view(gpointer w);
node *lx_view_node(GtkWidget *w);
void lx_view_set_node(GtkWidget *w, node *n);
void lx_allocate(GtkWidget *w, int width, int height, float x, float y, const transform_t *t);
void lx_size(node *n, GtkWidget *w, float *width, float *height);
bool lx_is_layout_root(const node *n);
void lx_root_allocated(node *n, int width, int height);
void lx_snapshot_children(GtkWidget *w, GtkSnapshot *snap);

// areas.c: the kinds and props beyond the controls. Each area's file
// (list.c, canvas.c, web.c…) has one of these; the host asks each in
// turn, and the first that takes a call has it.
typedef struct {
	const char *name;
	bool (*create)(node *n);
	bool (*prop)(node *n, ak_prop p, const ak_value *v);
	bool (*common)(node *n, ak_prop p, const ak_value *v);
	void (*text)(node *n);
	bool (*listen)(node *n, ak_event_kind e);
	bool (*command)(node *n, ak_cmd c, ak_span args);
	bool (*measure)(node *n, float cw, float ch, float aw, float ah, float *ow, float *oh);
	bool (*draw)(node *n, GtkSnapshot *snap, const graphene_rect_t *b);
	void (*scrolled)(node *n);
	bool (*scroll_to)(node *n, double x, double y, bool animated);
	// frame takes a node's frame instead of the host's placing it (a
	// list's cells, a split view's detail): true, taken.
	bool (*frame)(node *n, const float f[4]);
	bool (*insert)(node *p, node *c, uint32_t index);
	bool (*remove)(node *p, node *c);
	void (*release)(node *n);
	// dump writes what an area shows beyond its children (a list's
	// rows) for ASUKA_DUMP, a line each at depth.
	void (*dump)(node *n, int depth, ak_buf *b);
	void (*modules)(void);
} area;
extern const area *const lx_areas[];
extern const size_t lx_area_count;

// props.c, views.c
void lx_create(ak_id id, ak_view_kind kind);
void lx_set_common(node *n, ak_prop p, const ak_value *v);
void lx_set_own(node *n, ak_prop p, const ak_value *v);
void lx_apply_text(node *n);
void lx_style_button(node *n);
PangoAttrList *lx_attrs(node *n, bool with_color);
void lx_set_label_attrs(node *n, GtkWidget *label);
const char *lx_family(node *n);
int lx_physical_align(node *n);
float lx_xalign(node *n);
void lx_pressed(node *n);
void lx_release_controls(node *n);
GtkWidget *lx_button_label(GtkWidget *b);

// theme.c
void lx_load_theme(void);
void lx_update_css(void);
void lx_read_palette(GtkWidget *w);
bool lx_resolve(ak_color c, GdkRGBA *out);
void lx_theme_changed(void);
void lx_font(node *n, float *size, int *weight);
base_font lx_system_font(void);
float lx_text_scale(void);
void lx_set_dev_font_scale(float s);
void lx_fonts_changed(void);
ak_color lx_parse_accent(const char *s);
const char *lx_custom_family(const char *name);
static inline uint16_t u16c(float f) { return (uint16_t)(fminf(fmaxf(f, 0), 1) * 65535 + 0.5f); }
static inline uint8_t u8c(float f) { return (uint8_t)(fminf(fmaxf(f, 0), 1) * 255 + 0.5f); }

// measure.c
void lx_measure(node *n, float cw, float ch, float aw, float ah, float *ow, float *oh);
void lx_widget_size(GtkWidget *w, float width, float *ow, float *oh);
float lx_line_height(node *n);
float lx_fit(float known, float avail, float min_content);
bool lx_finite(float f);
#define AK_BIG 1e6f

// window.c, winctl.c, chrome.c
win *lx_new_main_window(void);
win *lx_new_win(GtkWindow *window);
win *lx_win_by_content(GtkWidget *w);
win *lx_win_by_window(gpointer window);
void lx_win_set_root(win *w, node *n);
void lx_win_allocated(win *w, int width, int height);
void lx_win_env(win *w, ak_environment *env);
void lx_watch_settings(void);
void lx_set_window_options(ak_span args);
void lx_update_title(win *w);
void lx_configure(win *w, const ak_window_config *c);
void lx_show_main(void);
void lx_show_win(win *w, bool present);
void lx_window_action(win *w, ak_span args);
void lx_queue_window_state(win *w);
win *lx_win_for(node *n);
win *lx_win_of(node *n);
// lx_web_view is a WebView node's WebKitWebView (web.c), NULL for any other.
void *lx_web_view(node *n);
node *lx_window_node(win *w);
int lx_header_room(win *w);
void lx_window_css(win *w);
void lx_drop_window_css(win *w);
void lx_queue_chrome(void);
void lx_update_chrome(void);
bool lx_go_back(node *stack);
bool lx_shows_menu(win *w);
void lx_free_win(win *w);
void lx_add_node(node *n);
#define AK_HEADER_HEIGHT 47
int lx_window_size(float f);

// nav.c
bool lx_create_nav(node *n);
bool lx_set_nav_prop(node *n, ak_prop p, const ak_value *v);
bool lx_insert_nav(node *p, node *c, uint32_t index);
bool lx_remove_nav(node *p, node *c);
bool lx_destroy_nav(node *n);
void lx_present(node *n);
void lx_roots_env_changed(void);
void lx_root_env(node *n, ak_environment *env);
void lx_size_covers(win *w);
win *lx_declared_win(node *n);
void lx_tabs_resized(node *n);
void lx_add_child(node *p, node *c, uint32_t index);
void lx_open_window(node *n);
GtkWidget *lx_page_child(node *n);
void lx_nav_free(node *n);
struct nav {
	GtkWidget *view;    // a Stack's AdwNavigationView; Tabs' AdwViewStack
	gpointer page;      // a Screen's AdwNavigationPage, or its AdwViewStackPage in Tabs
	AdwDialog *dialog;  // a Sheet's AdwDialog, an Alert's AdwAlertDialog
	ak_id window;       // a Sheet's or Alert's Window
	GtkWidget *bar;     // Tabs' AdwViewSwitcherBar
	GtkWidget *switcher;  // Tabs' AdwViewSwitcher, for the header bar
	bool narrow, popping, gone;
	search_field *search;
	int pending;  // -2: a Replace
	char *search_ph, *search_tx;
	ak_buf fields_buf;
	ak_alert_field_vec fields;
	widget_vec rows;
	char *badge;
	int64_t detents;
	bool full;
	float env_w, env_h;
	action_vec toolbar, primary;
	void *bottom;  // the screen's bottom bar (toolbar.c)
	AdwToast *toast;
	int32_t duration;
	GtkWidget *page_child;  // what the page shows (the screen's view, or a toolbar view around it)
};

// events.c, interaction.c, keys.c, a11y.c, dragdrop.c
void lx_listen(node *n, ak_event_kind e);
void lx_unlisten(node *n, ak_event_kind e);
void lx_release_events(node *n);
void lx_press(node *n);
void lx_make_pressable(node *n);
void lx_route_keys(win *w);
void lx_set_keys(node *n, ak_span b);
void lx_update_a11y(node *n);
void lx_set_a11y(node *n, ak_prop p, const ak_value *v);
void lx_announce(ak_span text);
void lx_set_a11y_actions(node *n, ak_span b);
void lx_set_adjustable(node *n, bool on);
void lx_release_a11y(node *n);
void lx_set_drag_items(node *n, ak_span b);
void lx_set_drop_accept(node *n, ak_span b);
bool lx_draggable(node *n);
void lx_set_selected_state(node *n, bool on);

// focus.c: the keyboard's focus on any view (RFC 0010)
void lx_set_focus_prop(node *n, ak_prop p, const ak_value *v);
void lx_update_focusable(node *n);
void lx_watch_focus(win *w);
void lx_focus_report(win *w);
void lx_focus_command(node *n, bool focus);
void lx_listen_key_text(node *n);
gboolean lx_trap_focus(GtkWidget *w, GtkDirectionType d);
GtkWidget *lx_control_of(node *n);

// overlay.c: Overlay, a layer over a layout root (RFC 0010)
bool lx_overlay_modal(node *n);
void lx_overlay_env(node *n, ak_environment *env);
void lx_overlays_outside_press(GtkWidget *window, GdkSurface *surface, double x, double y);
bool lx_overlay_back(void);
GtkWidget *lx_overlay_root_widget(node *n);
void lx_overlays_check(void);

// a11y.c: screen readers' states (RFC 0010)
void lx_a11y_dump(node *n, ak_buf *b);
void lx_modal_refresh(void);
void lx_modal_changed(void);
void lx_modal_watch(node *n);

// menus.c
void lx_menu_changed(node *n);
void lx_popup_menu(node *n, double x, double y);
void lx_set_menu_bar(ak_span args);
GMenuModel *lx_menu_model(const action_vec *items, const char *prefix);
void lx_insert_menu_actions(GtkWidget *w, const char *group, ak_id target, const action_vec *items);

// scroll.c, list.c, pager.c, refresh.c
void lx_make_scroll(node *n, bool horizontal);
void lx_scroll_set_content(node *n, node *c);
bool lx_set_scroll_prop(node *n, ak_prop p, const ak_value *v);
void lx_scroll_to(node *n, double x, double y, bool animated);
void lx_scroll_moved(node *n);
bool lx_create_list(node *n);
bool lx_set_list_prop(node *n, ak_prop p, const ak_value *v);
bool lx_list_command(node *n, ak_cmd c, ak_span args);
bool lx_list_frame(node *n, const float f[4]);
void lx_list_scroll_to_offset(node *n, double x, double y, bool animated);
void lx_release_list(node *n);
bool lx_list_insert(node *p, node *c, uint32_t index);
bool lx_list_remove(node *p, node *c);
struct scroll_view {
	GtkWidget *sw, *viewport;
	bool horizontal;
	node *content;
	GtkAdjustment *hadj, *vadj;
	double reported[2];
	void *spinner_state;  // a refreshable one's (scroll.c)
	void *pager;          // a paging one's
};
void lx_refresh_key(win *w);
void lx_set_refreshing(node *n, bool on);
void lx_set_paging(node *n, bool on);
void lx_release_scroll(node *n);
void lx_release_list(node *n);

// field.c, richfield.c
void lx_make_field(node *n);
void lx_set_field_prop(node *n, ak_prop p, const ak_value *v);
void lx_style_field(node *n);
void lx_focus_field(node *n);
void lx_insert_text(node *n, ak_span text);
float lx_measure_multiline(node *n, float width);
void lx_release_field(node *n);
bool lx_composing(GtkWidget *w);
void lx_watch_preedit(GtkWidget *w);
void lx_field_command(node *n, ak_cmd c, ak_span args);

// controls.c
void lx_make_toggle(node *n, bool checkbox);
void lx_set_toggle(node *n);
void lx_make_stepper(node *n);
void lx_set_stepper_prop(node *n, ak_prop p, const ak_value *v);
void lx_make_color_picker(node *n);
void lx_set_color_picker_prop(node *n, ak_prop p, const ak_value *v);
void lx_set_indeterminate(node *n, bool on);
void lx_build_segments(node *n);
void lx_select_segment(node *n);
void lx_make_date_picker(node *n);
void lx_set_date_prop(node *n, ak_prop p, const ak_value *v);
void lx_release_date(node *n);
bool lx_create_picker(node *n);
bool lx_set_picker_prop(node *n, ak_prop p, const ak_value *v);
void lx_style_picker(node *n);
bool lx_create_progress_ring(node *n);
bool lx_set_progress_ring_prop(node *n, ak_prop p, const ak_value *v);
bool lx_draw_progress_ring(node *n, GtkSnapshot *snap, const graphene_rect_t *b);
bool lx_measure_more(node *n, float cw, float ch, float aw, float ah, float *ow, float *oh);

// image.c, symbols.c, shape.c, canvas.c, shader.c, video.c, web.c
void lx_set_image_prop(node *n, ak_prop p, const ak_value *v);
void lx_draw_image(node *n, GtkSnapshot *snap, const graphene_rect_t *b);
void lx_release_image(node *n);
typedef void (*lx_texture_fn)(void *ctx, GdkTexture *t, const char *err);
void lx_load_texture(const char *src, lx_texture_fn cb, void *ctx);
void lx_draw_symbol(node *n, GtkSnapshot *snap, const graphene_rect_t *b);
void lx_style_symbol(node *n);
const char *lx_icon_name(const char *symbol);
void lx_symbols_init(void);
bool lx_draw_more(node *n, GtkSnapshot *snap, const graphene_rect_t *b);
bool lx_create_more(node *n);
bool lx_set_own_more(node *n, ak_prop p, const ak_value *v);
bool lx_command_more(node *n, ak_cmd c, ak_span args);
void lx_release_more(node *n);
bool lx_set_common_more(node *n, ak_prop p, const ak_value *v);

// anim.c, match.c
void lx_begin_animation(ak_span args);
void lx_end_animation(void);
void lx_animate(node *n, const float old[4]);
void lx_animate_opacity(node *n);
void lx_animate_transform(node *n, transform_t t);
const float *lx_frame_now(node *n);
void lx_set_transition(node *n, ak_span b);
bool lx_exit(node *n);
void lx_enter_from(node *n);
void lx_relayout(node *n);
void lx_set_frames(bool on);
void lx_set_match(node *n, ak_span key);
void lx_release_anim(node *n);

// lifecycle.c, quit.c, state.c, routes.c, locale.c
void lx_watch_app(void);
void lx_lifecycle_check(void);
bool lx_ask_quit(bool ending);
void lx_request_quit(void);
void lx_quit_command(ak_span args);
bool lx_closing_last(win *w);
void lx_closed_window(void);
void lx_save_window(win *w);
void lx_restore_window(win *w);
void lx_save_windows(void);
void lx_route_location(node *n, ak_span args);
void lx_route_start(void);
void lx_routes_flush(void);
void lx_listen_location(node *n);
void lx_init_locale(void);
void lx_set_locale(ak_span args);
void lx_save_locale(ak_span tag);
const char *lx_posix_locale(void);
const char *lx_time_zone(void);
const char *lx_user_languages(void);
void lx_watch_signals(void);
void lx_apply_direction(node *n);
void lx_apply_direction_inherit(node *n, void *ctx);
void lx_watch_memory(void);

// services.c: the host's modules
void lx_call(node *root, ak_span args);
void reply_ok(reply *r, const void *p, size_t n);
void reply_err(reply *r, const char *fmt, ...) AK_PRINTF(2, 3);
void reply_str(reply *r, const char *s);
void lx_register_methods(const method_t *m, size_t n);
method_fn lx_method(const char *module, const char *name);
void lx_open_url(const char *u);
// lx_word is a word of Asuka's (internal/l10n's key) in the app's language.
const char *lx_word(const char *key);
const char *lx_badged(const char *title, const char *badge);
const char *lx_word_app(const char *key, const char *app);
char *lx_file_mime(const char *path);  // dragdrop.c (free it)
void lx_toast(const char *text);
void lx_modules_init(void);

// tray.c, desk.c, deskwin.c
void lx_tray_style_changed(void);
// lx_tray_click is where a tray icon was last clicked (screen pixels):
// false if no panel said.
bool lx_tray_click(uint32_t icon, int *x, int *y);
void lx_tray_flyout_closed(uint32_t icon);
void lx_show_desk(win *w, bool present);
void lx_watch_window(win *w);
void lx_configure_desk(win *w, const ak_window_config *c);
void lx_desk_allocated(win *w);
void lx_set_flyout(win *w, ak_span b);
void lx_drop_desk(win *w);
void lx_fit_window(win *w, float x, float y);
void lx_set_click_through(win *w, bool on);
void lx_set_takes_clicks(node *n, bool on);
void lx_set_on_top(win *w, bool on);
bool lx_on_top(win *w);
void lx_move_to(win *w, float x, float y);
void lx_center(win *w);
bool lx_frameless(win *w);

void lx_each_node(void (*f)(node *n, void *ctx), void *ctx);
void lx_snapshot(const char *path);
bool lx_go_back_front(void);
node *lx_field_of_widget(GtkWidget *w);
// automation.c, crash.c
void lx_dump_node(node *n, int depth, ak_buf *b);
char *lx_accelerator(const char *key, uint8_t mods);  // menus.c (free it)
bool lx_key_to_nodes(const char *key, uint8_t mods);  // keys.c
bool lx_activate_app_item(const char *title);        // menus.c
void lx_snapshot_window(GtkWindow *window, const char *path);
void lx_automation(void);
void lx_crash_start(void);

// deverror.c: asuka dev's error overlay (DevError, DevErrorClear)
void lx_dev_error(node *n, ak_span args);
void lx_dev_error_clear(void);
void lx_dev_highlight(node *n, ak_span args);  // asuka dev's inspector's (view.c)
void lx_dev_highlight_clear(void);
void lx_dev_perf(node *n, ak_span args);  // asuka dev's performance overlay (view.c)
void lx_dev_perf_dump(ak_buf *b);
void lx_dev_error_raise(void);
void lx_dev_error_resized(win *w);
void lx_dev_error_closing(win *w);
bool lx_dev_error_over(win *w);
bool lx_dev_error_settling(void);
bool lx_dev_error_tap(const char *text);
bool lx_dev_error_escape(void);
void lx_dev_error_dump(ak_buf *b);
void lx_dev_error_texts(ak_buf *b);

// files.c: launches and second instances
void lx_watch_launches(void);
void lx_incoming_url(const char *u);

// The C API of asuka_desktop.h this host implements, and what it reads.
const char *lx_data_dir(void);
const char *lx_config_dir(void);
const char *lx_cache_dir(void);
const char *lx_logs_dir(void);
// lx_dirs_init finds the app's directories (desktop.h's dsk_dirs):
// its data in $XDG_DATA_HOME/<id> (Flatpak's own), ASUKA_DATA set to it
// for what reads it, before anything does.
void lx_dirs_init(const char *argv0);
// lx_assets_dir is where the app's assets are: ASUKA_ASSETS (asuka dev),
// assets beside the executable, or share/<exe>/assets of its prefix.
char *lx_assets_dir(const char *argv0);
// Relaunching (the Quit command's relaunch flag) and the login item's
// launch (relaunch.c).
void lx_relaunch_set(bool on);
bool lx_relaunch_asked(void);
void lx_relaunch_if_asked(char **argv);
extern const char *(*lx_relaunch_exe)(void);
extern bool lx_opened_at_login;

// dbus.c: the D-Bus services' helpers.
typedef void (*lx_dbus_reply)(void *ctx, GVariant *reply, const char *err);
GDBusConnection *lx_session_bus(void);
GDBusConnection *lx_private_bus(char **err);
void lx_close_bus(GDBusConnection *c);
void lx_dbus_call(GDBusConnection *conn, const char *dest, const char *path, const char *iface, const char *method, GVariant *params, int timeout_ms, lx_dbus_reply cb, void *ctx);
guint lx_dbus_export(GDBusConnection *conn, const char *path, const char *xml, const char *iface, const GDBusInterfaceVTable *vt, void *ctx, char **err);
void lx_dbus_emit(GDBusConnection *conn, const char *path, const char *iface, const char *name, GVariant *params);

// The desktop portal.
#define LX_PORTAL "org.freedesktop.portal.Desktop"
#define LX_PORTAL_PATH "/org/freedesktop/portal/desktop"
typedef void (*lx_portal_done)(void *ctx, uint32_t code, GVariant *results, const char *err);
void lx_portal_request(const char *iface, const char *method, GVariant *(*params)(const char *token, void *ctx), lx_portal_done done, void *ctx);
bool lx_no_portal(const char *err);
void lx_portal_request_fds(const char *iface, const char *method, GVariant *(*params)(const char *token, void *ctx), GUnixFDList *fds, lx_portal_done done, void *ctx);
bool lx_in_flatpak(void);  // login.c
void lx_local_files(const char *const *sources, const char *const *types, size_t n, void (*done)(void *ctx, char **paths, size_t n, const char *err), void *ctx);  // compose.c
void lx_key_gdk_name(const char *key, char *out, size_t n);  // menus.c

#endif
