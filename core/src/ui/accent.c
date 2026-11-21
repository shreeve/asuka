// accent.c: the app's accent color (asuka.toml's accent), the one color
// its brand sets on the platform's controls. The compiler gives it to
// the core in the app's init (ui.set_accent: the wire's Color, 0 for the
// platform's; flags: 1, Android's wallpaper colors; and the Color in dark
// mode, asuka.toml's accentDark, 0 for the same one), so it travels
// with the app's code: asuka dev's player takes each app's and drops it
// between apps. The host is told with the Accent command, before any
// view is made, as a root mounts, and again when a reload changes it.
#include "ui/ui_ext.h"

static struct {
	uint64_t color;
	uint8_t flags;
	uint64_t dark;
	uint64_t sent_color;  // what the host was last told
	uint8_t sent_flags;
	uint64_t sent_dark;
} A;

static void send(ak_ui_root *r) {
	if (!r || !r->el || r->disposed) return;
	if (A.color == A.sent_color && A.flags == A.sent_flags && A.dark == A.sent_dark) return;
	A.sent_color = A.color, A.sent_flags = A.flags, A.sent_dark = A.dark;
	ak_buf *b = ak_batch_command_begin(r->batch, r->el->id, AK_CMD_ACCENT);
	ak_buf_u64(b, A.color);
	ak_buf_u8(b, A.flags);
	ak_buf_u64(b, A.dark);
	ak_batch_command_end(r->batch);
}

// ak_accent_host forgets what a host was told: a new one knows nothing.
void ak_accent_host(void) { A.sent_color = 0, A.sent_flags = 0, A.sent_dark = 0; }

// ak_accent_mounted tells the host of a root that's mounting (before its
// views).
void ak_accent_mounted(ak_ui_root *r) { send(r); }

static ak_val b_set_accent(ak_val *a, uint32_t n) {
	A.color = (uint64_t)a[0].i, A.flags = (uint8_t)a[1].i;
	A.dark = n > 2 ? (uint64_t)a[2].i : 0;
	send(ak_ui.root);  // a reload's, into the app that's showing
	return (ak_val){0};
}

void ak_accent_builtins_init(void);
void ak_accent_builtins_init(void) { ak_builtin_register("ui.set_accent", b_set_accent); }
