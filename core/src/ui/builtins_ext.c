// builtins_ext.c: the builtins of the UI runtime's interaction, media,
// graphics and language parts (ui_ext.h), registered with ui.*'s
// (builtins.c). As every builtin, each borrows its arguments and returns
// an owned result. Arguments, in register order (kinds as rt.h's; "?" a
// ref that may be none; Task<T> a future the caller awaits):
//
//	name                    arguments                                      result
//	ui.variant              v: View, state: int (AK_STATE_*), key: int
//	                        (a style key), value: the key's kind            -
//	ui.variant_bind         v, state, key, thunk: fn() -> the key's kind     -
//	ui.layer                v: View, tier: int (AK_LAYER_*), state: int
//	                        (AK_STATE_*, 0: none)                          int (the layer)
//	ui.layer_set            v: View, layer: int, key: int (a style key),
//	                        value: the key's kind                          -
//	ui.layer_bind           v, layer, key, thunk: fn() -> the key's kind   -
//	ui.layer_when           v: View, layer: int, cond: fn() -> bool        -
//	ui.state                v: View, state: int                            bool (tracked)
//	ui.variant_declare      v: View, id: int (a variant's, the lowering's),
//	                        cond: fn() -> bool                             -
//	ui.layer_variant        v: View, layer: int (AK_LAYER_VARIANT's),
//	                        id: int                                        - (on while variant id holds)
//	ui.variant_read         v: View, id: int                               bool (tracked; false: v has none)
//	ui.model                acc: a prop's accessor, bound: ? (none: the
//	                        caller didn't bind the prop)                   the prop's accessor (a model)
//	ui.model_bound          model                                          bool
//	ui.model_set            model, value: the prop's kind                  - (unbound only)
//	ui.use_keys             keys: [Shortcut]                               -
//	ui.animate              anim: Animation?, body: fn()                   -
//	ui.animate_wait         anim: Animation?, body: fn()                   Task<void> (settles once
//	                                                                       it's complete)
//	ui.animating            -                                              bool
//	ui.on_frame             f: fn(Duration)                                fn() (stop)
//	ui.frames               -                                              Stream<Duration>
//	ui.play, ui.pause       v: View (a Video)                              -
//	ui.seek                 v: View, seconds: float                        -
//	ui.web_eval             v: View (a WebView), script: string            -
//	ui.web_navigate         v: View, to: int (0 back, 1 forward,
//	                        2 reload, 3 stop)                              -
//	ui.web_eval_result      v: View, script: string                        Task<string> (JSON)
//	ui.web_cookies          url: string                                    Task<string>
//	ui.insert_text          v: View (a TextField), text: string            -
//	ui.format               v: View, format: int (TextFormat), on: bool    -
//	ui.toggle_format        v: View, format: int                           -
//	ui.set_link             v: View, url: string                           -
//	ui.native               type: string ("<class>#<View>")                View
//	ui.native_set           v: View, name: string, value: the prop's
//	                        kind, desc: string (its descriptor)            -
//	ui.native_bind          v, name, thunk: fn() -> the prop's kind, desc  -
//	ui.native_on            v, name, handler: fn(params...), desc: string
//	                        (the parameters' descriptors, in order)        -
//	ui.share                text: string                                   -
//	ui.share_files          paths: [string], mime: string, uti: string,
//	                        title: string, from: View?                     Task<void>
//	imageedit.coreCapture   view: Ref, format: int, quality: float,
//	                        scale: float                                   Task<string>
//	ui.haptic               kind: int (Haptic)                             -
//	ui.copy                 text: string                                   -
//	ui.open_url             url: string                                    -
//	ui.announce             text: string                                   -
//	ui.a11y_focus           v: View                                        -
//	ui.set_accent           color: int (the wire's Color), flags: int,
//	                        dark: int (the Color in dark mode, 0: color)   -
//	ui.call                 module: string, method: string, args: bytes    Task<bytes>
//	ui.view_id              v: View                                        int
//	ui.on_open_url          f: fn(string)                                  -
//	ui.on_open_files        f: fn([string])                                -
//	ui.on_share             f: fn([SharedItem])                            -
//	host.process            -                                              string ("app", "share",
//	                                                                       "notification")
//	sharesheet.coreClose    how: int (0 done, 1 cancelled, 2 open the app) - (ShareClose)
//	i18n.locale             -                                              string (tracked)
//	i18n.set_locale         tag: string ("": the user's languages)         -
//	i18n.set_languages      tags: [string]                                 -
//	i18n.languages          -                                              [string]
//	i18n.preferred_languages -                                             [string] (tracked)
//	i18n.is_rtl             -                                              bool (tracked)
//	i18n.region             -                                              Region (tracked)
//	i18n.format_locale      -                                              string (tracked)
//	i18n.catalog            blob: bytes (i18n's Catalog.Compile)           Catalog
//	i18n.say                catalog: Catalog, key: int, args: tuple?,
//	                        lang: string? (none: the language shown)        string (tracked)
//	i18n.format_number      n: float, flags: int (1 percent, 2 integer,
//	                        4 no grouping, 8 half even), minFrac, maxFrac   string (tracked)
//	i18n.format_int         n: int, flags: int                             string (tracked)
//	i18n.format_date        ns: int, date: int, time: int (0 none, 1
//	                        short, 2 medium, 3 long, 4 full)               string (tracked)
//	i18n.format_relative    t: int, now: int (ns), style: int (0 long, 1
//	                        short, 2 narrow), numeric: bool               string (tracked)
//	i18n.autonym           tag: string                                    string
//	i18n.force              lang: string, dir: string ("rtl", "ltr", "")   - (asuka dev's --lang, --dir)
//
// canvas.* and path.*, a Canvas's Graphics and Path, are canvas.c's.
#include "ui/ui_ext.h"
#include "ui/i18n.h"
#include "native/native.h"
#include "abi/dev.h"

static ak_val none(void) { return (ak_val){0}; }

static ak_view *view_arg(ak_val *a, uint32_t n, uint32_t i, const char *name) {
	if (i >= n || !a[i].p || ((ak_obj *)a[i].p)->tag != AK_TAG_VIEW) ak_panic("%s: argument %u isn't a View", name, i);
	return a[i].p;
}

static ak_view *view_or_none(ak_val *a, uint32_t n, uint32_t i) {
	return i < n && a[i].p && ((ak_obj *)a[i].p)->tag == AK_TAG_VIEW ? a[i].p : NULL;
}

// ref_view is the view a Ref argument (its cell, or none) holds: a
// package's native fn gets the Ref itself, not its view.
static ak_view *ref_view(ak_val *a, uint32_t n, uint32_t i) {
	if (i >= n || !a[i].p) return NULL;
	ak_val v = ak_vm_cell_get(a[i].p, false);
	ak_release(v.p);  // the Ref holds it: borrowed here
	return v.p && ((ak_obj *)v.p)->tag == AK_TAG_VIEW ? v.p : NULL;
}

static ak_val b_variant(ak_val *a, uint32_t n) {
	ak_ui_variant(view_arg(a, n, 0, "ui.variant"), (uint8_t)a[1].i, (uint16_t)a[2].i, a[3]);
	return none();
}

static ak_val b_variant_bind(ak_val *a, uint32_t n) {
	ak_ui_variant_bind(view_arg(a, n, 0, "ui.variant_bind"), (uint8_t)a[1].i, (uint16_t)a[2].i, a[3].p);
	return none();
}

static ak_val b_layer(ak_val *a, uint32_t n) {
	return (ak_val){.i = ak_ui_layer(view_arg(a, n, 0, "ui.layer"), (uint8_t)a[1].i, (uint8_t)a[2].i)};
}

static ak_val b_layer_set(ak_val *a, uint32_t n) {
	ak_ui_layer_set(view_arg(a, n, 0, "ui.layer_set"), (uint16_t)a[1].i, (uint16_t)a[2].i, a[3]);
	return none();
}

static ak_val b_layer_bind(ak_val *a, uint32_t n) {
	ak_ui_layer_bind(view_arg(a, n, 0, "ui.layer_bind"), (uint16_t)a[1].i, (uint16_t)a[2].i, a[3].p);
	return none();
}

static ak_val b_layer_when(ak_val *a, uint32_t n) {
	ak_ui_layer_when(view_arg(a, n, 0, "ui.layer_when"), (uint16_t)a[1].i, a[2].p);
	return none();
}

static ak_val b_state(ak_val *a, uint32_t n) { return (ak_val){.i = ak_ui_view_state(view_arg(a, n, 0, "ui.state"), (uint8_t)a[1].i)}; }

static ak_val b_variant_declare(ak_val *a, uint32_t n) {
	ak_ui_variant_declare(view_arg(a, n, 0, "ui.variant_declare"), a[1].i, a[2].p);
	return none();
}

static ak_val b_layer_variant(ak_val *a, uint32_t n) {
	ak_ui_layer_variant(view_arg(a, n, 0, "ui.layer_variant"), (uint16_t)a[1].i, a[2].i);
	return none();
}

static ak_val b_variant_read(ak_val *a, uint32_t n) { return (ak_val){.i = ak_ui_variant_read(view_arg(a, n, 0, "ui.variant_read"), a[1].i)}; }

static ak_val b_model(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return ak_vm_model_new(a[0].p, a[1].p != NULL);
}

static ak_val b_model_bound(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.i = ak_vm_model_bound(a[0].p)};
}

static ak_val b_model_set(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	ak_vm_model_set(a[0].p, a[1]);
	return none();
}

static ak_val b_use_keys(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	ak_ui_use_keys(a[0].p);
	return none();
}

static ak_val b_animate(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	ak_ui_animate(a[0].p, a[1].p);
	return none();
}

static ak_val b_animate_wait(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_ui_animate_wait(a[0].p, a[1].p)};
}

static ak_val b_animating(ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.i = ak_ui_animating()};
}

ak_val ak_ui_frames(void);

static ak_val b_frames(ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return ak_ui_frames();
}

static ak_val b_on_frame(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_ui_on_frame(a[0].p)};
}

static ak_val b_play(ak_val *a, uint32_t n) {
	ak_ui_video_play(view_arg(a, n, 0, "ui.play"));
	return none();
}

static ak_val b_pause(ak_val *a, uint32_t n) {
	ak_ui_video_pause(view_arg(a, n, 0, "ui.pause"));
	return none();
}

static ak_val b_seek(ak_val *a, uint32_t n) {
	ak_ui_video_seek(view_arg(a, n, 0, "ui.seek"), a[1].f);
	return none();
}

static ak_val b_web_eval(ak_val *a, uint32_t n) {
	ak_ui_web_eval(view_arg(a, n, 0, "ui.web_eval"), a[1].p);
	return none();
}

static ak_val b_web_navigate(ak_val *a, uint32_t n) {
	ak_ui_web_navigate(view_arg(a, n, 0, "ui.web_navigate"), a[1].i);
	return none();
}

static ak_val b_web_eval_result(ak_val *a, uint32_t n) { return (ak_val){.p = ak_ui_web_eval_result(view_or_none(a, n, 0), a[1].p)}; }

static ak_val b_web_cookies(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_ui_web_cookies(a[0].p)};
}

static ak_val b_insert_text(ak_val *a, uint32_t n) {
	ak_ui_insert_text(view_arg(a, n, 0, "ui.insert_text"), a[1].p);
	return none();
}

static ak_val b_format(ak_val *a, uint32_t n) {
	ak_ui_format(view_arg(a, n, 0, "ui.format"), a[1].i, a[2].i ? 1 : 0);
	return none();
}

static ak_val b_toggle_format(ak_val *a, uint32_t n) {
	ak_ui_format(view_arg(a, n, 0, "ui.toggle_format"), a[1].i, 2);
	return none();
}

static ak_val b_set_link(ak_val *a, uint32_t n) {
	ak_ui_set_link(view_arg(a, n, 0, "ui.set_link"), a[1].p);
	return none();
}

static const char *desc_arg(ak_val v) {
	const ak_str *s = v.p;
	return s ? s->data : "v";
}

static ak_val b_native(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_ui_native(a[0].p)};
}

static ak_val b_native_set(ak_val *a, uint32_t n) {
	ak_ui_native_set(view_arg(a, n, 0, "ui.native_set"), a[1].p, a[2], desc_arg(a[3]), ak_native_running_types());
	return none();
}

static ak_val b_native_bind(ak_val *a, uint32_t n) {
	ak_ui_native_bind(view_arg(a, n, 0, "ui.native_bind"), a[1].p, a[2].p, desc_arg(a[3]), ak_native_running_types());
	return none();
}

static ak_val b_native_on(ak_val *a, uint32_t n) {
	ak_ui_native_on(view_arg(a, n, 0, "ui.native_on"), a[1].p, a[2].p, desc_arg(a[3]), ak_native_running_types());
	return none();
}

#define STR_CMD(fname, call)              \
	static ak_val fname(ak_val *a, uint32_t n) { \
		AK_UNUSED(n);                     \
		call(a[0].p);                     \
		return none();                    \
	}

STR_CMD(b_share, ak_ui_share)
STR_CMD(b_copy, ak_ui_copy)
STR_CMD(b_open_url, ak_ui_open_url)
STR_CMD(b_announce, ak_ui_announce)
STR_CMD(b_on_open_url, ak_ui_on_open_url)
STR_CMD(b_on_open_files, ak_ui_on_open_files)
STR_CMD(b_on_share, ak_ui_on_share)
STR_CMD(b_on_lifecycle, ak_ui_on_lifecycle)

// ui.run_action: an Action's run, as a menu's choice runs it (its task,
// when it's async, under the caller's owner).
static ak_val b_run_action(ak_val *a, uint32_t n) {
	if (n > 0) ak_run_action(NULL, a[0].p);
	return (ak_val){0};
}

// ui.dev_player: whether asuka dev runs the app here (its player).
static ak_val b_dev_player(ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.i = ak_dev_player()};
}
STR_CMD(b_on_foreground, ak_ui_on_foreground)
STR_CMD(b_on_background, ak_ui_on_background)

static ak_val b_share_files(ak_val *a, uint32_t n) { return (ak_val){.p = ak_ui_share_files(a[0].p, a[1].p, a[2].p, a[3].p, ref_view(a, n, 4))}; }

static ak_val b_capture(ak_val *a, uint32_t n) {
	if (n < 4) return none();
	return (ak_val){.p = ak_ui_capture(ref_view(a, n, 0), a[1].i, a[2].f, a[3].f)};
}

static ak_val b_color_scheme(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	ak_ui_color_scheme(a[0].i);
	return (ak_val){0};
}

static ak_val b_share_close(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	ak_ui_share_close(a[0].i);
	return none();
}

static ak_val b_process(ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	const char *p = ak_ui_process();
	return (ak_val){.p = ak_str_new(p, strlen(p))};
}

static ak_val b_haptic(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	ak_ui_haptic(a[0].i);
	return none();
}

// accessibility.focus: nothing while its Ref holds no view.
static ak_val b_a11y_focus(ak_val *a, uint32_t n) {
	ak_view *v = ref_view(a, n, 0);
	if (v) ak_ui_a11y_focus(v);
	return none();
}

static ak_val b_call(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_ui_call(a[0].p, a[1].p, a[2].p)};
}

// platform/host: the hosts' modules for the platform SDK's packages
// (sdk/host): call (a Task of the result), send (no answer), and the
// little-endian encodings the modules' arguments and results are in.

static ak_val b_host_send(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	ak_release(ak_ui_call(a[0].p, a[1].p, a[2].p));
	return none();
}

static const ak_bytes *bytes_arg(ak_val v) { return v.p; }

static ak_val le_bytes(uint64_t v, int n) {
	uint8_t b[8];
	for (int i = 0; i < n; i++) b[i] = (uint8_t)(v >> (8 * i));
	return (ak_val){.p = ak_bytes_new(b, (size_t)n)};
}

static ak_val b_host_u8(ak_val *a, uint32_t n) { AK_UNUSED(n); return le_bytes((uint64_t)a[0].i, 1); }
static ak_val b_host_u32(ak_val *a, uint32_t n) { AK_UNUSED(n); return le_bytes((uint64_t)a[0].i, 4); }
static ak_val b_host_u64(ak_val *a, uint32_t n) { AK_UNUSED(n); return le_bytes((uint64_t)a[0].i, 8); }

static ak_val b_host_f64(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	uint64_t u;
	memcpy(&u, &a[0].f, 8);
	return le_bytes(u, 8);
}

static ak_val b_host_str(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	const ak_str *s = a[0].p;
	ak_bytes *b = ak_bytes_new(NULL, 4 + (size_t)s->len);
	for (int i = 0; i < 4; i++) b->data[i] = (uint8_t)(s->len >> (8 * i));
	memcpy(b->data + 4, s->data, s->len);
	return (ak_val){.p = b};
}

// The readers take an offset; past the end, 0.
static uint64_t le_read(ak_val *a, int n) {
	const ak_bytes *b = bytes_arg(a[0]);
	int64_t at = a[1].i;
	uint64_t v = 0;
	if (!b || at < 0 || (size_t)n > b->len || (uint64_t)at > b->len - (size_t)n) return 0;
	for (int i = 0; i < n; i++) v |= (uint64_t)b->data[at + i] << (8 * i);
	return v;
}

static ak_val b_host_read_u8(ak_val *a, uint32_t n) { AK_UNUSED(n); return (ak_val){.i = (int64_t)le_read(a, 1)}; }
static ak_val b_host_read_u32(ak_val *a, uint32_t n) { AK_UNUSED(n); return (ak_val){.i = (int64_t)le_read(a, 4)}; }
static ak_val b_host_read_u64(ak_val *a, uint32_t n) { AK_UNUSED(n); return (ak_val){.i = (int64_t)le_read(a, 8)}; }

static ak_val b_host_read_f64(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	uint64_t u = le_read(a, 8);
	double f;
	memcpy(&f, &u, 8);
	return (ak_val){.f = f};
}

static ak_val b_host_read_str(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	const ak_bytes *b = bytes_arg(a[0]);
	int64_t at = a[1].i;
	uint64_t len = le_read(a, 4);
	if (!b || at < 0 || (uint64_t)at + 4 + len > b->len) return (ak_val){.p = ak_str_empty()};
	return (ak_val){.p = ak_str_new((const char *)b->data + at + 4, (size_t)len)};
}

static ak_val b_view_id(ak_val *a, uint32_t n) { return (ak_val){.i = ak_ui_view_id(view_or_none(a, n, 0))}; }

static ak_val b_locale(ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.p = ak_ui_locale()};
}

static ak_val b_set_locale(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	ak_ui_set_locale(a[0].p);
	return none();
}

static ak_val b_set_languages(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	ak_ui_set_languages(a[0].p);
	return none();
}

static ak_val b_languages(ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.p = ak_ui_languages()};
}

static ak_val b_preferred(ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.p = ak_ui_preferred_languages()};
}

static ak_val b_is_rtl(ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.i = ak_ui_is_rtl()};
}

static ak_val b_region(ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return ak_ui_region();
}

static ak_val b_format_locale(ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	return (ak_val){.p = ak_ui_format_locale()};
}

static ak_val b_catalog(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_i18n_catalog(a[0].p)};
}

static ak_val b_say(ak_val *a, uint32_t n) {
	return (ak_val){.p = ak_i18n_say(a[0].p, a[1].i, n > 2 ? a[2] : none(), n > 3 ? a[3].p : NULL)};
}

static ak_val b_format_number(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_i18n_format_number(a[0].f, (int)a[1].i, (int)a[2].i, (int)a[3].i)};
}

static ak_val b_format_decimal(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_i18n_format_decimal(a[0].p, (int)a[1].i, (int)a[2].i, (int)a[3].i, a[4].p)};
}

static ak_val b_format_int(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_i18n_format_int(a[0].i, (int)a[1].i)};
}

static ak_val b_format_date(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_i18n_format_date(a[0].i, (int)a[1].i, (int)a[2].i)};
}

static ak_val b_format_relative(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_i18n_format_relative(a[0].i, a[1].i, (int)a[2].i, a[3].i != 0)};
}

void ak_ui_force_locale(ak_str *lang, ak_str *dir);

static ak_val b_force(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	ak_ui_force_locale(a[0].p, a[1].p);
	return none();
}

static ak_val b_autonym(ak_val *a, uint32_t n) {
	AK_UNUSED(n);
	return (ak_val){.p = ak_i18n_autonym(a[0].p)};
}

void ak_canvas_builtins_init(void);
void ak_accent_builtins_init(void);

void ak_ui_ext_builtins_init(void);

void ak_ui_ext_builtins_init(void) {
	static const struct {
		const char *name;
		ak_builtin_fn fn;
	} t[] = {
		{"ui.variant", b_variant}, {"ui.variant_bind", b_variant_bind}, {"ui.layer", b_layer}, {"ui.layer_set", b_layer_set},
		{"ui.layer_bind", b_layer_bind}, {"ui.layer_when", b_layer_when}, {"ui.state", b_state}, {"ui.use_keys", b_use_keys},
		{"ui.variant_declare", b_variant_declare}, {"ui.layer_variant", b_layer_variant}, {"ui.variant_read", b_variant_read},
		{"ui.model", b_model}, {"ui.model_bound", b_model_bound}, {"ui.model_set", b_model_set},
		{"ui.animate", b_animate}, {"ui.animate_wait", b_animate_wait}, {"ui.animating", b_animating}, {"ui.on_frame", b_on_frame}, {"ui.frames", b_frames}, {"ui.play", b_play},
		{"ui.pause", b_pause}, {"ui.seek", b_seek}, {"ui.web_eval", b_web_eval}, {"ui.web_navigate", b_web_navigate},
		{"ui.web_eval_result", b_web_eval_result}, {"webview.cookies", b_web_cookies}, {"ui.insert_text", b_insert_text},
		{"ui.format", b_format}, {"ui.toggle_format", b_toggle_format}, {"ui.set_link", b_set_link}, {"ui.native", b_native},
		{"ui.native_set", b_native_set}, {"ui.native_bind", b_native_bind}, {"ui.native_on", b_native_on}, {"ui.call", b_call},
		{"device.share", b_share}, {"device.coreShareFiles", b_share_files}, {"imageedit.coreCapture", b_capture}, {"device.haptic", b_haptic},
		{"device.copy", b_copy}, {"device.openURL", b_open_url},
		{"accessibility.announce", b_announce}, {"accessibility.focus", b_a11y_focus},
		{"markdown.openLink", b_open_url}, {"base.say", b_announce},
		{"host.call", b_call}, {"host.send", b_host_send},
		{"host.u8", b_host_u8}, {"host.u32", b_host_u32}, {"host.u64", b_host_u64},
		{"host.f64", b_host_f64}, {"host.str", b_host_str},
		{"host.readU8", b_host_read_u8}, {"host.readU32", b_host_read_u32},
		{"host.readU64", b_host_read_u64}, {"host.readF64", b_host_read_f64},
		{"host.readStr", b_host_read_str}, {"host.process", b_process}, {"ui.view_id", b_view_id},
		{"sharesheet.coreClose", b_share_close},
		{"ui.on_open_url", b_on_open_url}, {"ui.on_open_files", b_on_open_files}, {"ui.on_share", b_on_share}, {"appearance.setColorScheme", b_color_scheme},
		{"ui.dev_player", b_dev_player},
		{"ui.run_action", b_run_action},
		{"ui.on_lifecycle", b_on_lifecycle}, {"ui.on_foreground", b_on_foreground}, {"ui.on_background", b_on_background},
		{"i18n.locale", b_locale}, {"i18n.set_locale", b_set_locale}, {"i18n.set_languages", b_set_languages},
		{"i18n.languages", b_languages}, {"i18n.preferred_languages", b_preferred}, {"i18n.is_rtl", b_is_rtl},
		{"i18n.region", b_region}, {"i18n.format_locale", b_format_locale}, {"i18n.catalog", b_catalog}, {"i18n.say", b_say},
		{"i18n.format_number", b_format_number}, {"i18n.format_decimal", b_format_decimal}, {"i18n.format_int", b_format_int}, {"i18n.format_date", b_format_date},
		{"i18n.format_relative", b_format_relative}, {"i18n.autonym", b_autonym}, {"i18n.force", b_force},
	};
	for (size_t i = 0; i < AK_LEN(t); i++) ak_builtin_register(t[i].name, t[i].fn);
	ak_canvas_builtins_init();
	ak_accent_builtins_init();
}
