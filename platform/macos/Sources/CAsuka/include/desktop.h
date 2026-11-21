// desktop.h: the C the Windows and Linux hosts share (RFC 0008, "Shared
// C for Windows and Linux"), kept small: what both need and isn't a
// platform's.
//
//   - JSON, read into a tree and written (the app's settings, the state
//     files, a module's arguments);
//   - the app's settings (internal/appconfig's asuka-app.json);
//   - the app's directories (each platform's place for them);
//   - the state files: where the windows were (asuka-window.json) and
//     where their routers were (asuka-routes.json);
//   - files by UTF-8 path (Windows' are UTF-16);
//   - where a flyout window goes against its anchor;
//   - animated images, decoded and played (internal/animimage);
//   - Asuka's own words in the app's language (internal/l10n).
//
// Nothing here is thread-safe unless it says so: the hosts call it on
// their main thread. It's portable C17 (POSIX on Linux, Win32 on
// Windows), with nothing of either host's: platform/desktop/desktop_test.go
// tests it on any system.
//
// The API is stable: what's added is added in commits of platform/desktop
// alone, each noted here:
//   - dsk_flyout_place, dsk_flyout_center: flyouts' and panels' places;
//   - dsk_word, dsk_word_format: Asuka's own words;
//   - dsk_anim_*: animated images (GIF, APNG, WebP) and their player;
//   - dsk_exists; dsk_jw_json: a value of JSON written elsewhere;
//   - dsk_routes_path keeps none under asuka dev (ASUKA_DEV), and keeps
//     them under automation when ASUKA_ROUTES=1 (the hosts' tests).
#ifndef ASUKA_DESKTOP_SHARED_H
#define ASUKA_DESKTOP_SHARED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- JSON ----

enum { DSK_JNULL, DSK_JFALSE, DSK_JTRUE, DSK_JNUM, DSK_JSTR, DSK_JARR, DSK_JOBJ };

// A value of a parsed document. Strings are NUL-terminated UTF-8 (len
// without the NUL); an array's or object's items are kids (an object's
// with their keys).
typedef struct dsk_json {
	int type;
	double num;
	char *str;
	size_t len;
	struct dsk_json *kids;
	char **keys;
	size_t n;
} dsk_json;

// dsk_json_parse parses text: NULL if it isn't JSON. dsk_json_free frees
// what it returned.
dsk_json *dsk_json_parse(const char *text, size_t len);
void dsk_json_free(dsk_json *v);
// dsk_json_get is an object's member, NULL if it has none (or v isn't an
// object).
const dsk_json *dsk_json_get(const dsk_json *v, const char *key);
// The member's value as a type, or def.
const char *dsk_json_str(const dsk_json *v, const char *key, const char *def);
double dsk_json_num(const dsk_json *v, const char *key, double def);
bool dsk_json_bool(const dsk_json *v, const char *key, bool def);

// A JSON writer: a growing buffer. dsk_jw_take hands its text over (the
// caller frees it); a writer that failed to allocate has err set.
typedef struct {
	char *p;
	size_t len, cap;
	bool err;
	bool comma;  // a value written in the open object or array
} dsk_jw;

void dsk_jw_raw(dsk_jw *w, const char *s, size_t n);
// dsk_jw_json writes a value that's JSON already (an array, an object, a
// string written elsewhere), as dsk_jw_str writes a string's.
void dsk_jw_json(dsk_jw *w, const char *json, size_t n);
void dsk_jw_obj(dsk_jw *w);   // {
void dsk_jw_end(dsk_jw *w);   // }
void dsk_jw_arr(dsk_jw *w);   // [
void dsk_jw_aend(dsk_jw *w);  // ]
void dsk_jw_key(dsk_jw *w, const char *key);
void dsk_jw_str(dsk_jw *w, const char *s);
void dsk_jw_strn(dsk_jw *w, const char *s, size_t n);
void dsk_jw_num(dsk_jw *w, double v);
void dsk_jw_int(dsk_jw *w, int64_t v);
void dsk_jw_bool(dsk_jw *w, bool v);
// dsk_jw_value writes a parsed value back.
void dsk_jw_value(dsk_jw *w, const dsk_json *v);
char *dsk_jw_take(dsk_jw *w, size_t *len);
void dsk_jw_free(dsk_jw *w);

// ---- Files ----

// dsk_read_file reads a whole file (NUL-terminated past *len): NULL if it
// can't. Free it with free.
char *dsk_read_file(const char *path, size_t *len);
// dsk_write_file writes a file whole, through a temporary file renamed
// into place, making its directory first.
bool dsk_write_file(const char *path, const void *data, size_t len);
// dsk_exists reports whether a file or directory is at path.
bool dsk_exists(const char *path);
// dsk_mkdirs makes a directory and its parents.
bool dsk_mkdirs(const char *path);
// dsk_join is a, the platform's separator and b (free it).
char *dsk_join(const char *a, const char *b);
// dsk_windows_name reports whether s (n bytes of UTF-8) names an entry of
// a directory on Windows as written, and nothing else: not empty, . or
// .., with no character Windows keeps out of names (< > : " / \ | ? *
// and the controls; a colon would name a drive, C:x, or a file's stream,
// x:s), not ending in a dot or a space (Windows drops them: "x." is
// "x", ".. " is ".."), and not a device's name (CON, PRN, AUX, NUL,
// COM and LPT with a digit or a superscript one, CONIN$, CONOUT$), with
// any extension or spaces after it ("nul.txt", "COM1 .log").
bool dsk_windows_name(const char *s, size_t n);
// dsk_getenv is an environment variable, UTF-8, or NULL when it's unset
// or empty (free it).
char *dsk_getenv(const char *name);
// dsk_exe_path is this executable's path (free it).
char *dsk_exe_path(void);
// dsk_dir is path's directory (free it); dsk_base its last element.
char *dsk_dir(const char *path);
const char *dsk_base(const char *path);

// ---- The app's settings (asuka-app.json) ----

typedef struct {
	char *name, *id, *version, *build, *accent, *accent_dark, *scheme, *file_types;
	char *lang, *direction;
	bool background, keep_running, multiple_instances;
	char *update_feed, *update_key, *update_target;
	// The executable's alternate icons' names, comma-separated (Windows).
	char *alternate_icons;
} dsk_config;

// dsk_config_load reads the settings: from ASUKA_APP_CONFIG, else
// asuka-app.json beside the executable, else in an install prefix's
// share/<exe>/ (Linux: bin/<exe>). Missing, it's all empty.
void dsk_config_load(dsk_config *c);
void dsk_config_free(dsk_config *c);

// ---- The app's directories ----

// The platforms the rules differ by.
enum { DSK_WINDOWS, DSK_LINUX };

// dsk_dirs is where the app keeps its files, by each platform's rules:
// ASUKA_DATA (else data, which the host names: Local AppData\<Name>,
// $XDG_DATA_HOME/<id>), ASUKA_CONFIG, ASUKA_CACHE, ASUKA_LOGS overriding
// each. The strings are the caller's to free.
typedef struct {
	char *data, *config, *cache, *logs;
} dsk_dirs;

void dsk_dirs_find(dsk_dirs *d, int platform, const char *data);
void dsk_dirs_free(dsk_dirs *d);

// ---- Where the windows were (asuka-window.json) ----

// A store of states by key, each a JSON object the host makes (Windows:
// its rectangle in pixels, DPI and maximized; Linux: its size). A file an
// older host wrote, the main window's fields alone, reads as "main"'s.
typedef struct dsk_winstate dsk_winstate;

// dsk_winstate_open reads the file at path (NULL: a store that keeps
// nothing, for tests and automation runs).
dsk_winstate *dsk_winstate_open(const char *path);
void dsk_winstate_close(dsk_winstate *s);
// dsk_winstate_load is key's state, NULL if none (the store's).
const dsk_json *dsk_winstate_load(dsk_winstate *s, const char *key);
// dsk_winstate_save keeps key's state (JSON text of an object), writing
// the file if it changed.
void dsk_winstate_save(dsk_winstate *s, const char *key, const char *json);
void dsk_winstate_forget(dsk_winstate *s, const char *key);

// ---- Where the routers were (asuka-routes.json) ----

typedef struct dsk_routes dsk_routes;

// dsk_routes_path is the file's path ("" keeps none: tests, automation,
// asuka dev, whose reloads keep the routers where they are, no data
// directory; free it). ASUKA_ROUTES=1 keeps them under automation too,
// for the hosts' tests of reopening.
char *dsk_routes_path(void);
dsk_routes *dsk_routes_open(const char *path);
void dsk_routes_close(dsk_routes *s);
// dsk_routes_location reports where key's router was: its href and state
// (the store's), false if nowhere.
bool dsk_routes_location(dsk_routes *s, const char *key, const char **href, const char **state, size_t *state_len);
// dsk_routes_set keeps key's address; it reports whether that changed
// (the host then writes it a moment later with dsk_routes_flush, off the
// main thread if it likes: flush takes the store's lock).
bool dsk_routes_set(dsk_routes *s, const char *key, const char *href, size_t href_len, const char *state, size_t state_len);
void dsk_routes_flush(dsk_routes *s);

// ---- Flyouts and panels ----

// A rectangle, y down, in a host's own units (points or pixels).
typedef struct {
	double x, y, w, h;
} dsk_rect;

// dsk_flyout_center is a w by h rectangle centered across r, its top a
// third of the way down the room left (as Spotlight's panel shows), within
// r.
dsk_rect dsk_flyout_center(double w, double h, dsk_rect r);
// dsk_flyout_place is the frame of a w by h flyout next to anchor, gap
// away, as placement (an AK_PLACEMENT_*: the side, then start, center or
// end) says: flipped to the other side where that has no room and the
// other has, lined up along the side (flipped to the other end where that
// has no room), then slid back into work (the work area of the anchor's
// display) and shrunk to it, as GTK places menus.
dsk_rect dsk_flyout_place(dsk_rect anchor, double w, double h, int placement, double gap, dsk_rect work);

// ---- Asuka's own words (internal/l10n's catalogs, generated into
// src/words_gen.c: go generate ./platform/desktop) ----

// dsk_word is a word of Asuka's (its key: "back", "search", "ok"...) in
// locale's language (a BCP 47 tag, _ or -; its catalog's, else English),
// UTF-8, as the catalog has it ({app} not filled in).
const char *dsk_word(const char *locale, const char *key);
// dsk_word_format writes the word into out (cap bytes, NUL-terminated),
// its {name}s filled from vars (name, value pairs, nvars strings) and a
// {count, plural, ...} by count's case (=n, one, other; # the count). It
// returns the length written.
size_t dsk_word_format(char *out, size_t cap, const char *locale, const char *key, const char *const *vars, size_t nvars);

// ---- Animated images (internal/animimage) ----
//
// Where the platform decodes a still, not an animation (WIC, GDK): GIF is
// decoded here, APNG's and animated WebP's frames are read here and each
// one's picture (a PNG or WebP file made of its chunks) decoded by the
// host's still decoder. Pixels are RGBA, not premultiplied.

enum { DSK_DISPOSE_NONE, DSK_DISPOSE_BACKGROUND, DSK_DISPOSE_PREVIOUS };

// A frame: its picture, w by h at x, y of the canvas, shown for delay_ms
// (10 ms or less is 100, as browsers show it); what happens to its
// rectangle before the next (DSK_DISPOSE_*); drawn over what's there
// (blend) or replacing it.
typedef struct {
	int x, y, w, h;
	uint8_t *rgba;
	int delay_ms, disposal;
	bool blend;
} dsk_anim_frame;

// An animation: its canvas's size, its frames, and how many times it
// plays (0: forever).
typedef struct {
	int w, h, loops;
	dsk_anim_frame *frames;
	size_t n;
} dsk_anim;

// A still decoder: a still image file's pixels (RGBA, malloc'd: the
// caller frees them) and size; false if it can't.
typedef bool (*dsk_still_decoder)(void *ctx, const uint8_t *data, size_t len, uint8_t **rgba, int *w, int *h);

// dsk_anim_is_animated reports whether data looks like an animated GIF,
// APNG or WebP, from its headers, without decoding it.
bool dsk_anim_is_animated(const uint8_t *data, size_t len);
// dsk_anim_decode reads an animated GIF, APNG or WebP; NULL for a still
// one (one frame), another format, or one it can't read. still decodes
// APNG's and WebP's frames (NULL: they aren't read). Any thread.
dsk_anim *dsk_anim_decode(const uint8_t *data, size_t len, dsk_still_decoder still, void *ctx);
void dsk_anim_free(dsk_anim *a);

// A player composites an animation's frames in order onto its canvas
// (a->w by a->h, RGBA not premultiplied), which keeps the last frame
// drawn.
typedef struct {
	const dsk_anim *a;
	uint8_t *canvas;
	uint8_t *saved;  // DSK_DISPOSE_PREVIOUS's: the rectangle before the frame
	size_t next;     // the frame dsk_anim_next draws
	int loop;        // loops finished
	int last;        // the frame on the canvas (-1 before the first), its disposal pending
} dsk_anim_player;

// dsk_anim_player_init readies a player at the first frame (false: no
// memory); dsk_anim_player_free lets its canvas go.
bool dsk_anim_player_init(dsk_anim_player *p, const dsk_anim *a);
void dsk_anim_player_free(dsk_anim_player *p);
// dsk_anim_next draws the next frame on the canvas and sets how long it
// shows; false once the animation has played its loops (the canvas keeps
// its last frame).
bool dsk_anim_next(dsk_anim_player *p, int *delay_ms);

#ifdef __cplusplus
}
#endif

#endif
