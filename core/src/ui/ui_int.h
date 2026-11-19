// ui_int.h: the UI runtime's insides, shared by its files.
//
//	element.c   the node tree: views, fragments, flattening, native
//	            children, frames, destroying
//	root.c      roots, the flush, layout, events, the environment
//	style.c     the style keys: layout and look
//	measure.c   the shared measure cache
//	elements.c  the built-in elements' classes
//	control.c   Show, For, Switch, Dynamic; contexts, tasks
//	list.c      List: rows bound by the host
//	path.c      instance paths
//	builtins.c  the VM's ui.* builtins
//	nav.c       providers (host-sized layout roots with their own
//	            environment), going back, Stack, Screen and its bars,
//	            Tabs, Split, PageControl
//	overlay.c   Sheet, present and dismiss, Popover, Alert, Menu,
//	            Snackbar
//	window.c    Window, window handles, the main window, close guards,
//	            a window's keys
//	menubar.c   MenuBar, its Menus and MenuItems
//	lifecycle.c quit, relaunch, on quit, on secondInstance
//	router.c    Router and Route, hrefs
//	boundary.c  Boundary: error boundaries
#ifndef AK_UI_INT_H
#define AK_UI_INT_H

#include "ui/ui.h"
#include "signal/signal.h"
#include "layout/layout.h"
#include "rt/sched.h"
#include "vm/vm.h"

typedef struct ak_ui_root ak_ui_root;
typedef struct ak_el_class ak_el_class;

// A style key set on a view (its value retained when a reference).
typedef struct {
	uint16_t key;
	ak_val v;
} ak_style_ent;

// An event handler.
typedef struct {
	uint16_t ev;  // AK_EV_*
	ak_closure *c;
} ak_handler_ent;

// Class flags.
enum {
	AK_ELF_FLATTEN = 1 << 0,   // a container that makes no view until it needs one
	AK_ELF_TEXT = 1 << 1,      // draws text: takes font, lines, textAlign
	AK_ELF_MEASURED = 1 << 2,  // the host measures it
	AK_ELF_CACHE = 1 << 3,     // its size is a function of its content: the shared cache
	AK_ELF_CHILDREN = 1 << 4,  // takes children
	AK_ELF_FRAGMENT = 1 << 5,
	AK_ELF_SIZED = 1 << 6,     // hears its laid-out size (cls->sized), as a layout handler does
};

// An element's class: what's particular to a kind of element.
struct ak_el_class {
	const char *name;
	ak_view_kind kind;  // its native view's
	uint16_t flags;
	// init sets up a new view (its state, what it always sends).
	void (*init)(ak_view *e);
	// set applies one of the element's own keys (not the style's), which
	// changed: prev is what it was, NULL the first time it's set.
	void (*set)(ak_view *e, uint16_t key, ak_val v, const ak_val *prev);
	// finalize runs once its keys are set: as it's added to a parent,
	// mounted, or at the next flush.
	void (*finalize)(ak_view *e);
	// event handles an event from the host.
	void (*event)(ak_view *e, const ak_event *ev);
	// adjust changes the layout style the view's style makes.
	void (*adjust)(ak_view *e, ak_lstyle *ls);
	// add places a child (a Scroll's in its content); NULL: the view's own.
	void (*add)(ak_view *e, ak_view *child);
	// style_target is the view a style key goes to (a Scroll's content
	// for what lays its children out), NULL: none; NULL hook: e itself.
	ak_view *(*style_target)(ak_view *e, uint16_t key);
	// styled hears of a style key set on e (a List's gap).
	void (*styled)(ak_view *e, uint16_t key);
	// lazy takes the builder of its children (ui.lazy: a Tab's page, a
	// Sheet's content), which it builds when they show.
	void (*lazy)(ak_view *e, ak_closure *build);
	// sync runs at the flush, before the props are applied, for a view
	// queued with ak_view_sync (a Screen's bars, a Split's sidebar).
	void (*sync)(ak_view *e);
	// drop frees the class's state.
	void (*drop)(ak_view *e);
	// sized hears the size it's laid out at (AK_ELF_SIZED), after the
	// frames are applied, as layout handlers do: a Canvas draws for it.
	void (*sized)(ak_view *e, float w, float h);
	// destroy destroys the views it holds outside its kids and slots (an
	// Alert's Buttons), as its own are.
	void (*destroy)(ak_view *e);
};

// ak_shortcut_mods is a Shortcut's modifiers, as the Keys prop has them
// (keys.c).
uint8_t ak_shortcut_mods(const ak_struct *s);

struct ak_view {
	ak_opaque h;  // tag AK_TAG_VIEW
	ak_ui_root *r;
	const ak_el_class *cls;
	uint16_t el;  // AK_EL_*, 0 for the runtime's own views
	ak_view_kind kind;
	ak_id id;      // 0 until it has a native view
	uint8_t dir;   // AK_LAYOUT_DIRECTION_*: its own, or a parent's it took
	bool real;     // it has a native view
	bool frag;     // a fragment: no box, no view
	bool cell;     // a list cell: the host made its view
	bool dead;     // its views are destroyed
	bool finalized;
	bool kids_dirty, frame_q, layout_q;
	bool measured;  // the host measures it (ContentInset, sizes rounded up)
	bool fill;      // the root's child: grows into the screen
	bool host_sized;
	bool safe;      // a SafeArea: extra_pad follows the safe area
	bool autofocus; // takes the keyboard once it's placed (its first frame)
	ak_view *parent;  // not retained: the parent holds its children
	ak_view *up;      // not retained: where a view of no parent shows (a cell's list, a sheet's window, a popover's anchor)
	ak_view *bar;     // not retained: the view whose bar a view in a slot is in (a Screen's toolbar)
	struct ak_prov *prov;  // retained: a provider's environment (host_sized), or what a SafeArea follows
	bool sync_q;      // queued for its class's sync
	AK_VEC(ak_view *) kids;  // retained
	ak_lnode *box;           // NULL for fragments
	uint32_t native_kids;    // native children inserted into its view
	float sent[4];           // the last SetFrame
	float insets[4];         // ContentInset, as last sent
	float extra_pad[4];      // a SafeArea's insets
	AK_VEC(ak_style_ent) style;
	AK_VEC(ak_handler_ent) handlers;
	uint64_t listens[2];  // the ops events it listens for, a bit each (AK_EVENT_COUNT <= 128)
	uint64_t outer;    // the style keys its component's caller set, a bit each (ak_ui_set_outer)
	ak_owner *owner;   // retained: the owner it was made under
	ak_str *text, *text2;  // what the measure cache keys it by (Text's text; Button's title and icon)
	int32_t variant;
	void *impl;  // the class's state
	struct ak_ix *ix;  // its interaction states, variants, keys, drags (ui_ext.h)
	// A grid's tracks, converted for layout (what its style points to).
	void *grid_mem;
};

// The shared measure cache: two generations of up to 1024 entries,
// keyed by content.
typedef struct {
	ak_view_kind kind;
	int32_t variant;
	ak_str *text, *text2, *family;  // retained
	int32_t font_style, font_weight;
	float font_size;
	int64_t lines;
	float font_scale, scale;
	bool inset;
	uint32_t c[4];  // the constraints' bits
} ak_mkey;

typedef struct {
	ak_mkey k;
	uint32_t hash;  // 0: empty
	float w, h;
} ak_mentry;

#define AK_MCACHE_MAX 1024
#define AK_MCACHE_SLOTS 2048

typedef struct {
	ak_mentry *cur, *old;  // AK_MCACHE_SLOTS each
	uint32_t ncur, nold;
	uint64_t hits;
} ak_mcache;

void ak_mcache_init(ak_mcache *m);
void ak_mcache_clear(ak_mcache *m);
void ak_mcache_free(ak_mcache *m);
bool ak_mcache_get(ak_mcache *m, const ak_mkey *k, uint32_t hash, float *w, float *h);
void ak_mcache_put(ak_mcache *m, const ak_mkey *k, uint32_t hash, float w, float h);
uint32_t ak_mkey_hash(const ak_mkey *k);

typedef AK_VEC(ak_view *) ak_view_vec;

struct ak_ui_root {
	uint32_t rc;  // its views, and being mounted
	ak_host host;
	ak_batch *batch;            // where ops go
	AK_VEC(ak_batch *) spare;   // for ops written while one is applied
	ak_view *el;                // its root view
	ak_owner *owner;
	bool disposed;
	// The environment, its strings owned.
	ak_environment env;
	ak_str *env_locale, *env_tz, *env_langs;
	ak_node env_node;
	float scale;
	ak_lsize size;  // as last laid out
	ak_lpass pass;
	ak_view_vec kids_q, frame_q, init_q, layout_q, lists;
	ak_view_vec provs;   // host-sized layout roots: screens, sheets, popovers, windows
	ak_view_vec sync_q;  // views whose class syncs at the flush
	ak_view_vec shown;   // sheets, alerts and snackbars presented, held until they go
	ak_view_vec pagers;  // paging Scrolls: their pages follow their size
	struct ak_back *back;  // the root's going back (OnBack outside screens and sheets)
	AK_VEC(ak_lnode *) boxbuf;
	ak_mcache measures;
	int laying;
	uint64_t flushes;
	bool flushing;
	struct ak_rext *ext;  // animation, frames, its own layout roots (ui_ext.h)
	// The main window sized to its content (its Window's fitContent):
	// laid out at its content's size, within its least and most, which
	// WindowAction Fit sends.
	bool fit, fit_sent;
	float fit_min[2], fit_max[2];
	ak_lsize fit_laid;
	struct ak_menus *menus;  // MenuBars (menubar.c)
	// What the app's main window is (AK_MAIN_*, the app's function says
	// as it starts): its content's, its Window's, or none.
	uint8_t main_kind;
	bool none_sent;  // WindowOptions said WindowNone
};

// The app's main window (ui.main_kind): the window its app block's
// content shows in, the app block's Window, or none at all.
enum { AK_MAIN_CONTENT, AK_MAIN_WINDOW, AK_MAIN_NONE };

// The runtime's state, one per process.
typedef struct {
	ak_host host;
	bool inited;
	ak_ui_root *root;      // the mounted root (one for now; nothing assumes it)
	ak_ui_root *cur;       // the root views are being made for
	// Views by native ID: open addressing, linear probing.
	ak_view **ids;
	uint32_t ids_cap, ids_len;
	ak_id next_id;
	bool rtl;
	// An environment reported before a root mounted.
	ak_environment pending_env;
	ak_str *pending_strs[3];
	bool has_env;
	ak_ui_stats stats;
	int depth;  // dispatches and flushes in progress
} ak_ui_state;

extern ak_ui_state ak_ui;

// ---- element.c ----

ak_view *ak_view_new(ak_ui_root *r, const ak_el_class *cls, uint16_t el, bool real);
ak_view *ak_frag_new(void);
void ak_view_make_view(ak_view *e);  // gives it a native view
void ak_view_finalize(ak_view *e);
void ak_view_append(ak_view *e, ak_view *k);           // retains k
void ak_view_placed(ak_view *k);  // its layout style again, now it has a parent (cls->adjust)
void ak_view_remove(ak_view *e, ak_view *k, bool destroy);  // releases k
// Many of e's kids removed in one pass: each taken out, then all unlinked
// (released).
void ak_view_take_out(ak_view *k, bool destroy);
void ak_view_unlink_many(ak_view *e, ak_view *const *ks, uint32_t n);
void ak_view_attach_native(ak_view *e);
// Where e's first view is in its native parent (the views before it
// there), and how many it has there: its own, or its kids'. Rows put in
// one after another count from the first's place instead of walking
// their native parent's views for each (ak_view_attach_native_at).
uint32_t ak_view_native_start(ak_view *e);
uint32_t ak_view_native_count(const ak_view *e);
void ak_view_attach_native_at(ak_view *e, uint32_t idx);
void ak_view_detach_native(ak_view *e);
void ak_view_destroy(ak_view *e);
void ak_view_mark_kids(ak_view *e);
void ak_view_relink(ak_view *e);
void ak_view_queue_frame(ak_view *e);
void ak_view_emit_frame(ak_view *e);
void ak_view_send_insets(ak_view *e);
void ak_view_make_real(ak_view *e);
void ak_view_remeasure(ak_view *e);
void ak_view_mark_measured(ak_view *e);
void ak_view_measure(ak_view *e);  // the host measures it
ak_view *ak_view_native_parent(const ak_view *e);
void ak_view_each_native(ak_view *e, void (*f)(ak_view *d, void *ctx), void *ctx);
void ak_view_set_dir(ak_view *e, uint8_t dir);
// ak_view_inherit_dir gives k, placed in p, the direction p has.
void ak_view_inherit_dir(ak_view *p, ak_view *k);
uint8_t ak_view_ldir(const ak_view *e);  // its box's direction
bool ak_view_rtl(const ak_view *e);
void ak_view_listen(ak_view *e, ak_event_kind ev);
// ak_view_listening reports whether e listens for ev.
static inline bool ak_view_listening(const ak_view *e, ak_event_kind ev) { return (e->listens[ev / 64] >> (ev % 64)) & 1; }
ak_closure *ak_view_handler(const ak_view *e, uint16_t ev);
// ak_view_call calls e's handler for ev, if any, with args, under e's
// owner: an error it throws goes to the owner's catcher.
void ak_view_call(ak_view *e, uint16_t ev, ak_val *args, uint32_t nargs);
// ak_view_call_closure is the same for a handler of its own (c may be
// NULL: nothing).
void ak_view_call_closure(ak_view *e, ak_closure *c, ak_val *args, uint32_t nargs);
// ak_ui_call_under calls c under owner o (when o is alive): the contexts
// it reads (its stack, its sheet, its router) are o's, as a handler's are
// its view's. What it returns is the caller's; an error stays pending.
ak_val ak_ui_call_under(ak_owner *o, ak_closure *c, ak_val *args, uint32_t nargs);
void ak_view_put_id(ak_view *e);
void ak_view_drop_id(ak_id id);
ak_view *ak_view_of(ak_id id);
// The batch views write to.
static inline ak_batch *ak_view_batch(const ak_view *e) { return e->r->batch; }

// ---- style.c ----

// ak_style_set applies a style key (a layout field, a look, a text prop).
void ak_style_set(ak_view *e, uint16_t key, ak_val v);
const ak_val *ak_style_get(const ak_view *e, uint16_t key);  // NULL: unset
// ak_style_layout derives the view's layout style and gives it to its box.
void ak_style_relayout(ak_view *e);
void ak_style_layout(ak_view *e, ak_lstyle *out);
// ak_style_send sends the look of every key set: a view made real.
void ak_style_send_all(ak_view *e);
void ak_style_free(ak_view *e);
bool ak_style_logical(const ak_view *e);
// Lengths, as LOWERING.md packs them.
ak_llength ak_length(int64_t packed);
int64_t ak_length_pt(float v);
int64_t ak_length_pct(float v);
float ak_style_float(const ak_view *e, uint16_t key, float def);
int64_t ak_style_int(const ak_view *e, uint16_t key, int64_t def);

// ---- root.c ----

ak_ui_root *ak_ui_current(void);
void ak_root_retain(ak_ui_root *r);
void ak_root_release(ak_ui_root *r);
void ak_root_flush(ak_ui_root *r);
void ak_root_apply(ak_ui_root *r);
float ak_view_scale(const ak_view *e);
// ak_with_root makes r the root views are made for while fn runs.
void ak_with_root(ak_ui_root *r, void (*fn)(void *ctx), void *ctx);
// ak_ui_raise delivers an error a handler or a builder left pending to
// the catcher of owner (the default handler with none).
void ak_ui_raise_pending(ak_owner *owner);
// ak_ui_build calls build (a closure returning a View) with args, for
// root r, and returns the view (owned, finalized), or NULL (none, or an
// error, which it raised to the current owner's catcher).
ak_view *ak_ui_build(ak_ui_root *r, ak_closure *build, ak_val *args, uint32_t nargs);
void ak_ui_env_strings(ak_environment *dst, ak_str **strs, const ak_environment *src);

// ---- elements.c ----

const ak_el_class *ak_el_class_of(uint16_t el);
extern const ak_el_class ak_class_view, ak_class_frag, ak_class_cell;
// The ops event each AK_EV_ is reported by.
ak_event_kind ak_ev_ops(uint16_t ev);
// Encoders shared by the elements.
void ak_encode_strings(ak_buf *b, const ak_arr *a);
// Pull to refresh, a Scroll's and a List's: *shown is whether the host
// shows its spinner.
void ak_refresh_set(ak_view *e, bool *shown, ak_prop refreshing, ak_val v);
void ak_refresh_event(ak_view *e, bool *shown, ak_prop refreshing, uint16_t key, uint16_t ev);
bool ak_ui_eq(ak_val a, ak_val b, ak_kind k);

// ---- nav.c ----

// A provider's environment: a host-sized layout root's (a screen's, a
// sheet's, a window's), which the host reports with an Environment event
// to its view, starting from the root's. Content built in it finds it as
// a context (ak_prov_current), before its view exists.
typedef struct ak_prov {
	ak_opaque h;
	ak_view *view;  // not retained: the view it's the environment of, once made
	ak_environment env;
	ak_str *strs[3];
	ak_node node;  // read: follows the environment
	ak_lsize laid;  // as last laid out
	ak_lsize room;  // a fit sheet's, as last laid out
	float max_w;    // a fit root's widest (a popover's)
	bool fits;      // sized to its content: a popover's, an accessory's
	bool full_width;
	bool fit_sheet;  // a sheet as tall as its content (Detent fit)
	bool fit_wait;   // a sheet asking to fit its content waits for its room (its first Environment)
	bool fit_win;    // a window sized to its content
	bool fit_sent;
	// An enclosing provider whose environment it takes, as that changes,
	// until the host reports its own (retained): a sheet's screen, which
	// the hosts size as the sheet.
	struct ak_prov *follows;
} ak_prov;

extern const char ak_env_key[], ak_back_key[], ak_nav_key[], ak_sheet_key[], ak_window_key[], ak_router_key[];

ak_prov *ak_prov_new(ak_ui_root *r);  // starting from the root's environment
ak_prov *ak_prov_current(void);       // the nearest one, borrowed (NULL: the root's)
// ak_prov_follow makes p take of's environment, now and as it changes,
// until the host reports p's own.
void ak_prov_follow(ak_prov *p, ak_prov *of);
// ak_prov_attach makes v a provider: host-sized, laid out at its
// environment's size (ak_root_layout_provs), its environment p's.
void ak_prov_attach(ak_view *v, ak_prov *p);
// ak_view_env is the environment e is laid out in (its provider's, else
// the root's), read: a render effect that calls it follows it.
const ak_environment *ak_view_env(ak_view *e);
// ak_view_font_scale is the text size e is laid out in, as the font
// scale of its nearest provider's environment (a screen's, a sheet's: a
// host reports each), else its root's: what a measurement of its text
// is kept by. It reads no signal (layout asks it).
float ak_view_font_scale(const ak_view *e);
void ak_root_layout_provs(ak_ui_root *r, ak_lpass *p);
// ak_overlays_layout_hook lays out the open Overlays (floating.c's
// ak_overlays_layout), after the providers.
void ak_overlays_layout_hook(ak_ui_root *r, ak_lpass *p);
// ak_prov_report sets a provider's environment, reported by the host.
bool ak_prov_report(ak_id id, const ak_environment *env);
// ak_fit_sheet_platform reports whether the platform has sheets as tall as
// their content (Detent fit): macOS's and the web's.
bool ak_fit_sheet_platform(uint8_t platform);
// ak_only_kid is v made to fill its parent, as the one child of a kind
// that has one (a sheet's, a window's content): wrapped in a View when
// it makes none of its own.
ak_view *ak_only_kid(ak_ui_root *r, ak_view *v);
// ak_view_sync queues e's class's sync for the next flush.
void ak_view_sync(ak_view *e);
void ak_root_sync(ak_ui_root *r);
// ak_bar_dirty hears of a change in a view in a slot shown as a bar.
void ak_bar_dirty(ak_view *v);

// Going back (back.go): where the user's going back is taken.
typedef struct ak_back ak_back;
ak_back *ak_back_new(void (*fallback)(void *ctx), void *ctx);  // fallback NULL: the root's
void ak_back_attach(ak_back *b, ak_view *e);  // the view its Back events come to
void ak_back_run(ak_back *b);
uint32_t ak_nav_screens(ak_view *stack);  // a Stack's screens (0: not one)
bool ak_nav_pop_top(ak_view *stack);      // its top screen off, as its back button does
// ak_ui_dispatch_event delivers an event to its view, as one the host
// sent does (ak_ui_dispatch's, decoded).
void ak_ui_dispatch_event(const ak_event *ev);
// ak_view_native_rect: the view with a native view v is in, and v's rect
// in it (a width of -1: all of it); false if there's none (inspect.c).
bool ak_view_native_rect(ak_view *v, ak_view **anchor, float rect[4]);
void ak_back_release(ak_back *b);
// ak_root_event takes the root view's events: Back, the main window's.
void ak_root_event(ak_view *e, const ak_event *ev);
ak_event_kind ak_nav_ev_ops(uint16_t ev);
// ak_build_under builds build(args) for root r under owner o (made a
// current owner for the call), returning the view (finalized) or NULL.
ak_view *ak_build_under(ak_ui_root *r, ak_owner *o, ak_closure *build, ak_val *args, uint32_t nargs);
// Stacks, for the router: push a builder (under owner o, whose contexts
// the screen sees), reset to one, and hear of the user's pops.
ak_view *ak_stack_new(ak_ui_root *r);
ak_view *ak_stack_push(ak_view *stack, ak_closure *build, ak_val *args, uint32_t nargs, ak_owner *o);
void ak_stack_pop_to(ak_view *stack, uint32_t depth);
uint32_t ak_stack_depth(ak_view *stack);
ak_view *ak_stack_screen(ak_view *stack, uint32_t i);
void ak_stack_replace(ak_view *stack);
void ak_stack_remove_at(ak_view *stack, uint32_t i);
void ak_stack_on_pop(ak_view *stack, void (*fn)(void *ctx, ak_view *screen), void *ctx);
void ak_stack_on_push(ak_view *stack, void (*fn)(void *ctx, ak_view *screen), void *ctx);
void ak_screen_set_title(ak_view *screen, ak_str *title);

// ---- elements.c ----

void ak_encode_actions(ak_buf *b, const ak_arr *as);
const ak_enum *ak_find_action(const ak_arr *as, int64_t n);
uint32_t ak_action_count(const ak_arr *as);  // the numbers its items take
// ak_run_action runs an Action's run (a button's, unless it's
// disabled), its error going to e's owner's catcher.
void ak_run_action(ak_view *e, const ak_enum *a);
bool ak_scroll_laid(ak_view *e);

// ---- overlay.c, window.c, router.c ----

void ak_window_event(ak_view *e, const ak_event *ev);  // the main window's, to the root view
// ak_ui_root_fit sizes the main window to its content, within its least
// and most size (0: none), or not.
void ak_ui_root_fit(ak_ui_root *r, bool on, float min_w, float min_h, float max_w, float max_h);
const ak_el_class *ak_window_class_of(uint16_t el);  // WindowCloseGuard, WindowKeys
// Menu bars (menubar.c): the classes of MenuBar, a MenuBar's Menu,
// MenuItem and MenuSeparator; the root's Action events; a root's going.
const ak_el_class *ak_menu_class_of(uint16_t el);
const ak_el_class *ak_menu_class_here(void);  // Menu's: a MenuBar's menu inside one, else the context menu
void ak_menubar_action(ak_ui_root *r, int64_t index);
void ak_menus_free(ak_ui_root *r);
void ak_window_root_drop(ak_view *e);
ak_view *ak_window_of(ak_view *e);  // the window e is in: the root's view or a Window's
ak_view *ak_window_under(ak_owner *o);  // the window code under o runs in, by its context
ak_view *ak_alert_window(ak_ui_root *r);
void ak_router_href(ak_view *from, ak_str *href);  // a Button's href, pressed
bool ak_router_incoming(ak_span url);
void ak_router_location(ak_view *win, const ak_event *ev);
ak_view *ak_sheet_present(ak_ui_root *r, ak_owner *owner, ak_closure *build, ak_val *args, uint32_t nargs, int64_t detents, bool grabber, bool full, bool movable, bool routed, void (*dismissed)(void *ctx), void *ctx);
void ak_sheet_close(ak_view *sheet);

// ---- control.c ----

typedef struct ak_path ak_path;
// The for's reconciliation, shared with List: rows by key.
typedef struct {
	ak_val key;
	uint32_t hash;
	void *cell_item;   // the item's cell (vm), and its index's
	void *cell_index;
	void *item_acc;    // a for over a path's: its item's accessor (vm), retained
	ak_view *el;       // retained
	ak_owner *owner;
	int64_t old;
} ak_row;

// ak_row_make builds row (row(itemAcc, indexAcc)) for item at index i,
// under an owner of its own and path's segment of its key; ak_row_update
// gives a kept row its item and index; ak_row_free disposes it (its view
// is its parent's to remove).
void ak_row_make(ak_row *rw, ak_ui_root *r, ak_closure *row, ak_path *path, ak_owner *parent, ak_val item, ak_kind item_kind, int64_t i, ak_kind key_kind);
// ak_root_under makes a root owner (ak_root_new) whose parent is parent:
// it sees parent's contexts and catcher, and holds parent, not the effect
// running now.
ak_owner *ak_root_under(ak_owner *parent);
void ak_row_update(ak_row *rw, ak_val item, ak_kind item_kind, int64_t i);
void ak_row_free(ak_row *rw, ak_kind key_kind);

// A for over a path (rfcs/0013): its items are what an accessor of a
// path below a state reads, and it follows their order, not the items:
// the length, and the keys. A key that's a field path of the item (key
// t.id) is one signal for all items, read without calling anything; any
// other is the key's function of the item's accessor (and its index),
// whose reads of the item follow their paths; none keys by the index.
// A row's item is the accessor of its element at its place (its index
// cell), so it has no item cell: a write inside an item doesn't run the
// for, and a row that moves reads its item at its new place.
typedef struct {
	void *acc;           // the array's accessor, retained
	uint16_t *fields;    // a field path key's
	uint16_t nf;
	bool by_field;
	ak_closure *key;     // else the key's function, retained; NULL: by index
	void *cursor;        // its item: an accessor moved over the items
} ak_fsrc;

// ak_fsrc_init makes s over acc, keyed by key: a path's spelling of the
// item's fields (an ak_str), a function, or none.
void ak_fsrc_init(ak_fsrc *s, void *acc, ak_val key);
void ak_fsrc_free(ak_fsrc *s);
// ak_fsrc_follow reads the length, and follows the keys of a field
// path's: tracked. -1 when the read threw.
int64_t ak_fsrc_follow(ak_fsrc *s);
// ak_fsrc_items is the array now, untracked: retained, or NULL.
ak_arr *ak_fsrc_items(ak_fsrc *s);
// ak_fsrc_key is the key of items' element i (a function's may throw:
// an error pending).
ak_val ak_fsrc_key(ak_fsrc *s, ak_arr *items, int64_t i, ak_kind kk);
// ak_row_make_at builds a row of the element at index i of the array acc
// reads (item, as it is now), made by eff (its for's effect): its item,
// that element's accessor (ak_row_make's otherwise).
void ak_row_make_at(ak_row *rw, ak_ui_root *r, ak_closure *row, ak_path *path, ak_owner *parent, void *acc, ak_effect *eff, ak_val item, ak_kind item_kind, int64_t i, ak_kind key_kind);
// ak_row_move gives a kept row of a for over a path its new index, and
// its item there (what an untracked read reads until the for runs again
// after a change of its items).
void ak_row_move(ak_row *rw, ak_val item, ak_kind item_kind, int64_t i);

// ---- list.c ----

int64_t ak_list_count(ak_view *e);
void ak_list_bind(ak_view *e, int64_t index, ak_id cell);
// ak_list_row_keys hears of a container's swipeActions,
// leadingSwipeActions or selected changing (a List's row's own).
void ak_list_row_keys(ak_view *v);
void ak_list_unbind(ak_view *e, ak_id cell);
void ak_list_layout(ak_view *e, ak_lpass *p);
void ak_list_each_cell(ak_view *e, void (*f)(ak_view *c, void *ctx), void *ctx);

// ---- path.c ----

// ak_path_capture is the current path, retained, for a scope that builds
// later; ak_path_push makes the current path seg under base (count: a
// component's count added), ak_path_pop restores the one before.
ak_path *ak_path_capture(void);
void ak_path_release(ak_path *p);
ak_path *ak_path_push(ak_path *base, const char *seg, size_t n);
void ak_path_pop(ak_path *prev);
void ak_path_reset(void);

// ---- The reactive values of the VM (vm/react.c), which For's rows
// are made of ----

ak_val ak_vm_cell_new(ak_val v, ak_kind k, ak_str *keep_key);
ak_val ak_vm_cell_get(void *cell, bool track);
void ak_vm_cell_set(void *cell, ak_val v);  // takes v
bool ak_vm_cell_read_by(void *cell, const ak_comp *c);  // whole, or a path below it
ak_val ak_vm_acc_cell(void *cell_or_memo);
ak_val ak_vm_acc_get(void *acc);
ak_val ak_vm_acc_path(void *base, const uint8_t *p, uint32_t n, const ak_val *args, ak_kind k);
ak_val ak_vm_acc_row(void *base, void *index, ak_kind k, ak_effect *eff);
void ak_vm_acc_row_set(void *row, ak_val item, ak_kind ik);
void ak_vm_acc_row_gone(void *row);
ak_val ak_vm_acc_getp(void *acc, const uint8_t *p, uint32_t n, const ak_val *args);
void ak_vm_acc_keys(void *acc, const uint16_t *fields, uint16_t nf);
void ak_vm_acc_seek(void *acc, int64_t i);
void ak_vm_effect(ak_closure *c, bool render);
// The task running now (its future, borrowed) and its owner; NULL on the
// main coroutine (react.c).
ak_future *ak_vm_task_current(ak_owner **owner);
// ak_ui_load_optimistic_in is ak_ui_load_optimistic for the task whose
// future is task, owned by owner (its end: owner's disposal), or for
// synchronous code (task NULL: its end is the ready queue's next run).
void ak_ui_load_optimistic_in(ak_val loader, ak_closure *f, ak_future *task, ak_owner *owner);
// A model: the accessor of a prop its component assigns (react.c's
// "Models"). ak_vm_model_new makes one over the caller's accessor (NULL
// and an error pending when reading it threw); model_set borrows v.
ak_val ak_vm_model_new(void *inner, bool bound);
bool ak_vm_model_bound(void *model);
void ak_vm_model_set(void *model, ak_val v);

// The error overlay's (devoverlay.c): a root that mounts gets what
// shows; one that goes clears it first.
void ak_dev_overlay_mounted(ak_ui_root *r);
void ak_dev_overlay_unmount(ak_ui_root *r);

#endif
