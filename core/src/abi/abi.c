#define _POSIX_C_SOURCE 200809L  // setenv under -std=c17
// abi.c: the C ABI of core/include/asuka.h: what hosts call.
//
// asuka_start takes the host's vtable: apply and measure become the UI
// runtime's host (ak_ui_init), post the scheduler's wake (any thread asks
// the host to call asuka_run_posted soon). Events, list calls and posted
// work then come in on the main thread, each flushed before it returns.
//
// Native packages go through the vtable's native_* (core/src/native);
// asuka dev's player connects with asuka_dev_connect (core/src/abi/dev.c),
// and a reload keeps state cells by their instance paths (vm/keep.c).
#include "asuka.h"
#include "ui/ui.h"
#include "vm/vm.h"
#include "rt/sched.h"
#include "signal/signal.h"
#include "native/native.h"
#include "vm/keep.h"
#include "abi/dev.h"
#include "abi/trace.h"
#include "base/thread.h"
#include "std/std.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static asuka_host_callbacks host;
static void dispatch_early(void);
static bool started;
static ak_module *module;
static ak_owner *module_owner;  // what the module's init made (its state)

static void host_apply(void *ctx, const uint8_t *buf, size_t len) {
	AK_UNUSED(ctx);
	host.apply(buf, len);
}

static void host_measure(void *ctx, uint32_t id, float w, float h, float aw, float ah, float *ow, float *oh) {
	AK_UNUSED(ctx);
	if (host.measure_v) {
		float in[4] = {w, h, aw, ah}, out[2] = {0, 0};
		host.measure_v(id, in, out);
		*ow = out[0];
		*oh = out[1];
		return;
	}
	host.measure(id, w, h, aw, ah, ow, oh);
}

static void wake(void) {
	if (host.post) host.post();
}

static bool dev_player;  // asuka dev runs the app here (ak_dev_player)
static void dev_report(void *err, bool didnt_start);

static void on_error(void *ctx, void *err) {
	AK_UNUSED(ctx);
	if (ak_is_cancel(err)) return;
	// An error nothing caught (no Boundary around it): the app's default,
	// logged. Under asuka dev the overlay shows it, and the CLI prints it.
	if (dev_player) {
		dev_report(err, false);
		return;
	}
	fprintf(stderr, "asuka: %s\n", ak_error_message(err)->data);
}

// A host's callbacks made by Go on Windows (purego's, syscall.NewCallback:
// the desktop host) don't keep the registers the platform's ABI says a
// callee keeps, the floating-point ones among them (d8-d15 on arm64,
// xmm6-xmm15 on x64): the core's layout, holding floats in them across a
// measure, found them zeroed. So on Windows each callback is called from
// a function of the core's own that says it clobbers them: the compiler
// saves them as it enters it and puts them back as it returns, whatever
// the callback did.
#if defined(_WIN32) && defined(__GNUC__) && defined(__aarch64__)
#define HOST_KEEPS                                                                                                      \
	__asm__ volatile("" ::: "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15", "x19", "x20", "x21", "x22", "x23", \
	                 "x24", "x25", "x26", "x27", "x28", "memory")
#elif defined(_WIN32) && defined(__GNUC__) && defined(__x86_64__)
#define HOST_KEEPS                                                                                                       \
	__asm__ volatile("" ::: "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15", "rbx", \
	                 "rsi", "rdi", "r12", "r13", "r14", "r15", "memory")
#endif

#ifdef HOST_KEEPS
static asuka_host_callbacks given;  // the host's own

#define NOINLINE __attribute__((noinline))
static NOINLINE void k_apply(const uint8_t *b, size_t n) {
	given.apply(b, n);
	HOST_KEEPS;
}
static NOINLINE void k_measure(uint32_t id, float w, float h, float aw, float ah, float *ow, float *oh) {
	given.measure(id, w, h, aw, ah, ow, oh);
	HOST_KEEPS;
}
static NOINLINE void k_measure_v(uint32_t id, const float *in, float *out) {
	given.measure_v(id, in, out);
	HOST_KEEPS;
}
static NOINLINE void k_post(void) {
	given.post();
	HOST_KEEPS;
}
static NOINLINE size_t k_native_new(const uint8_t *cls, size_t len, uint32_t iface, uint8_t *out, size_t cap, uint8_t **big) {
	size_t r = given.native_new(cls, len, iface, out, cap, big);
	HOST_KEEPS;
	return r;
}
static NOINLINE size_t k_native_call(uint64_t h, uint32_t m, const uint8_t *in, size_t len, uint8_t *out, size_t cap, uint8_t **big) {
	size_t r = given.native_call(h, m, in, len, out, cap, big);
	HOST_KEEPS;
	return r;
}
static NOINLINE void k_native_start(uint64_t h, uint32_t m, const uint8_t *in, size_t len, uint64_t call) {
	given.native_start(h, m, in, len, call);
	HOST_KEEPS;
}
static NOINLINE void k_native_cancel(uint64_t call) {
	given.native_cancel(call);
	HOST_KEEPS;
}
static NOINLINE void k_native_release(uint64_t h) {
	given.native_release(h);
	HOST_KEEPS;
}
static NOINLINE void k_native_free(void *p) {
	given.native_free(p);
	HOST_KEEPS;
}

static asuka_host_callbacks kept(const asuka_host_callbacks *c) {
	given = *c;
	asuka_host_callbacks k = {0};
	if (c->apply) k.apply = k_apply;
	if (c->measure) k.measure = k_measure;
	if (c->measure_v) k.measure_v = k_measure_v;
	if (c->post) k.post = k_post;
	if (c->native_new) k.native_new = k_native_new;
	if (c->native_call) k.native_call = k_native_call;
	if (c->native_start) k.native_start = k_native_start;
	if (c->native_cancel) k.native_cancel = k_native_cancel;
	if (c->native_release) k.native_release = k_native_release;
	if (c->native_free) k.native_free = k_native_free;
	return k;
}
#else
static asuka_host_callbacks kept(const asuka_host_callbacks *c) { return *c; }
#endif

void asuka_start(asuka_host_callbacks *callbacks) {
	host = kept(callbacks);
	if (started) return;
	started = true;
	ak_signal_bind_thread();
	ak_vm_init();
	ak_ui_builtins_init();
	ak_signal_set_error_handler(on_error, NULL);
	ak_sched_set_wake(wake);
	ak_native_set_host(&host);
	ak_ui_init(&(ak_host){NULL, host_apply, host_measure});
	dispatch_early();
}

static char *dup_string(const char *s) {
	size_t n = strlen(s);
	char *p = malloc(n + 1);
	if (p) memcpy(p, s, n + 1);
	return p;
}

static void fail(char **err, const char *msg) {
	if (err) *err = dup_string(msg);
}

static void run_init(void *ctx) {
	ak_val v = ak_vm_call(ctx, NULL, 0);
	AK_UNUSED(v);
}

static bool keeping;  // the dev loop keeps state across loads

static const ak_shape *shape_named(void *ctx, const char *name) { return ak_module_shape_named(ctx, name); }

// start_module makes m (owned) the running app: the old one goes (its
// state saved first when the dev loop keeps it, unless fresh: a
// restart's, whose state all starts again), m's init runs and, when
// mount, its app mounts.
static int start_module(ak_module *m, bool mount, bool fresh, char **err) {
	char msg[512];
	int32_t app = ak_module_app(m);
	if (mount && app < 0) {
		ak_module_release(m);
		fail(err, "asuka: the module has no app");
		return -1;
	}
	// The old app goes first: its state (saved, in development), its views.
	if (keeping) {
		if (fresh)
			ak_keep_discard();
		else
			ak_keep_save();
	}
	ak_ui_unmount();
	if (module_owner) ak_owner_dispose(module_owner);
	module_owner = NULL;
	ak_trace_forget();  // the last error thrown may be of the old module's types
	if (module) ak_module_release(module);
	module = m;
	if (keeping) ak_keep_use(shape_named, m);
	if (ak_module_nfuncs(m)) {
		ak_owner *o = module_owner = ak_root_new();
		ak_with_owner(o, run_init, (void *)ak_module_func(m, ak_module_init(m)));
		if (ak_error_pending()) {
			void *e = ak_error_take();
			snprintf(msg, sizeof msg, "asuka: init: %s", ak_error_message(e)->data);
			if (dev_player && !ak_is_cancel(e)) {
				// The overlay shows it, over a root of no app's: the old
				// app went.
				dev_report(e, true);
				if (mount) ak_ui_mount(NULL);
			}
			ak_release(e);
			fail(err, msg);
			return -1;
		}
	}
	if (!mount) return 0;
	ak_closure *build = ak_vm_closure(ak_module_func(m, (uint32_t)app));
	ak_ui_mount(build);
	ak_release(build);
	return 0;
}

static int run_module(ak_module *m, char **err) { return start_module(m, true, false, err); }

// ak_abi_init_only makes m (owned; NULL: the app compiled into the
// build) the module that runs, without its app: its init runs and nothing
// mounts. For the web's workers, which run spawn's closures (abi/web.c).
int ak_abi_init_only(ak_module *m, char **err) {
	if (err) *err = NULL;
	if (!m) {
		if (!ak_app_loader) {
			fail(err, "asuka: no app is compiled into this build");
			return -1;
		}
		m = ak_app_loader();
		ak_module_retain(m);
	}
	return start_module(m, false, false, err);
}

// ak_abi_module is the module that runs (NULL: none).
ak_module *ak_abi_module(void) { return module; }

void asuka_unload(void) {
	if (!started) return;
	ak_ui_unmount();
	if (module_owner) ak_owner_dispose(module_owner);
	module_owner = NULL;
	ak_trace_forget();
	if (module) ak_module_release(module);
	module = NULL;
}

// load_module loads a module's bytes and runs its app (fresh: none of
// the old app's state kept).
static int load_module(const uint8_t *p, size_t n, bool fresh, char **err) {
	if (err) *err = NULL;
	if (!started) {
		fail(err, "asuka: asuka_load before asuka_start");
		return -1;
	}
	char msg[512];
	ak_module *m = ak_module_load(p, n, msg, sizeof msg);
	if (!m) {
		if (dev_player) {
			// The app that ran (if any) runs on, under the overlay.
			ak_dev_overlay_error(AK_STR("The app didn't load"), (ak_span){msg, (uint32_t)strlen(msg)}, AK_STR(""), false);
			if (!ak_ui_mounted()) ak_ui_mount(NULL);
		}
		fail(err, msg);
		return -1;
	}
	if (dev_player) ak_dev_overlay_clear_app();  // the errors were the old module's
	return start_module(m, true, fresh, err);
}

int asuka_load(const uint8_t *p, size_t n, char **err) { return load_module(p, n, false, err); }

// The app compiled into this build (lang/cgen), named by core/link's
// ak_link_app, so that a static library's linker keeps its object (which
// an unreferenced constructor wouldn't).
static ak_module *(*app_loader(void))(void) { return ak_app_loader ? ak_app_loader : ak_link_app; }

int asuka_run(char **err) {
	if (err) *err = NULL;
	if (!started) {
		fail(err, "asuka: asuka_run before asuka_start");
		return -1;
	}
	if (!app_loader()) {
		fail(err, "asuka: no app is compiled into this build (a player loads one with asuka_dev_connect)");
		return -1;
	}
	ak_module *m = app_loader()();
	ak_module_retain(m);  // the build's own stays with it
	return run_module(m, err);
}

int asuka_has_app(void) { return app_loader() != NULL; }

// ---- asuka dev ----

bool ak_dev_player(void) { return dev_player; }

static bool init_failed;  // the last load's init threw: dev_report said so

static void reloaded(int64_t t0, bool fresh) {
	uint32_t kept = ak_keep_restored(), us = (uint32_t)((ak_now_ns() - t0) / 1000);
	uint8_t b[9] = {(uint8_t)kept, (uint8_t)(kept >> 8), (uint8_t)(kept >> 16), (uint8_t)(kept >> 24), (uint8_t)us, (uint8_t)(us >> 8), (uint8_t)(us >> 16), (uint8_t)(us >> 24), fresh ? AK_DEV_RESTARTED : 0};
	ak_dev_send(AK_DEV_RELOADED, b, sizeof b);
}

static void reload(const uint8_t *p, size_t n, bool fresh) {
	int64_t t0 = ak_now_ns();
	char *err = NULL;
	init_failed = false;
	if (load_module(p, n, fresh, &err) != 0) {
		// An init that threw went to the CLI as uncaught, with its stack.
		if (!init_failed) {
			ak_dev_send(AK_DEV_ERROR, err, strlen(err));
			fprintf(stderr, "%s\n", err);
		}
		free(err);
		return;
	}
	reloaded(t0, fresh);
}

// dev_report shows err, which nothing in the app caught, in the overlay
// with where it was thrown, and sends it to asuka dev, which prints it
// and answers with the lines of the source where it went wrong (source,
// dev_frame). didnt_start: the module's init threw it.
static void dev_report(void *err, bool didnt_start) {
	ak_str *msg = ak_vm_error_text(err);
	const char *c, *vm;
	ak_trace_of(err, &c, &vm);
	ak_buf trace = {0};
	ak_buf_put(&trace, c, strlen(c));
	ak_buf_put(&trace, vm, strlen(vm));
	while (trace.len && trace.data[trace.len - 1] == '\n') trace.len--;
	ak_span m = {msg->data, msg->len}, t = {(const char *)trace.data, (uint32_t)trace.len};
	if (didnt_start) init_failed = true;
	uint32_t seq = ak_dev_overlay_error(didnt_start ? AK_STR("The app didn't start") : AK_STR("Uncaught error"), m, t, true);
	ak_buf f = {0};
	ak_buf_u32(&f, seq);
	ak_buf_u8(&f, didnt_start ? AK_DEV_DIDNT_START : 0);
	ak_buf_str(&f, m.p, m.len);
	ak_buf_str(&f, t.p, t.len);
	if (!ak_dev_send(AK_DEV_UNCAUGHT, f.data, f.len)) {
		// No asuka dev to print it: the player's log has it.
		fprintf(stderr, "asuka: %s%s\n", didnt_start ? "init: " : "", msg->data);
		if (t.len) fprintf(stderr, "%.*s\n", (int)t.len, t.p);
	}
	ak_buf_free(&f);
	ak_buf_free(&trace);
	ak_release(msg);
}

// dev_restart is the overlay's Reload: the app starts again from its
// module, keeping none of its state, as asuka dev's restart does.
static void dev_restart(void) {
	if (!module) return;
	int64_t t0 = ak_now_ns();
	ak_module *m = module;
	ak_module_retain(m);  // start_module lets the old one (this one) go
	char *err = NULL;
	init_failed = false;
	int rc = start_module(m, true, true, &err);
	free(err);  // an init that threw: the overlay shows it
	if (rc == 0) reloaded(t0, true);
}

// dev_frame takes asuka dev's frames for the overlay (dev.h).
static void dev_frame(uint8_t kind, const uint8_t *p, size_t n) {
	ak_reader r = {p, n, 0, false};
	switch (kind) {
	case AK_DEV_SOURCE: {
		uint32_t seq = ak_read_u32(&r);
		ak_span source = ak_read_str(&r), details = ak_read_str(&r);
		if (!r.err) ak_dev_overlay_source(seq, source, details);
		break;
	}
	case AK_DEV_FAILED: {
		ak_span message = ak_read_str(&r), source = ak_read_str(&r), details = ak_read_str(&r);
		if (!r.err) ak_dev_overlay_build(message, source, details);
		break;
	}
	case AK_DEV_FIXED: ak_dev_overlay_fixed(); break;
	}
}

static void keep_states(void);

// dev_begin makes this run asuka dev's player: states kept across
// reloads, the run-time diagnostics, errors traced for the overlay.
static void dev_begin(void) {
	keep_states();
	ak_diag_enabled = true;  // a development build: the run-time diagnostics (rfcs/0015)
	if (dev_player) return;
	dev_player = true;
	ak_trace_start();
	ak_dev_overlay_set_reload(dev_restart);
	ak_dev_set_reload(reload);
	ak_dev_set_frames(dev_frame);
}

void asuka_dev_frame(int kind, const uint8_t *p, size_t len) {
	if (!started) return;
	dev_begin();
	dev_frame((uint8_t)kind, p, len);
}

static void keep_states(void) {
	if (keeping) return;
	keeping = true;
	ak_keep_start();
	ak_keep_qualify = ak_ui_path_key;
}

static int dev_load(const uint8_t *p, size_t n, bool fresh, uint32_t *micros, char **err) {
	if (err) *err = NULL;
	dev_begin();
	int64_t t0 = ak_now_ns();
	if (load_module(p, n, fresh, err) != 0) return -1;
	if (micros) *micros = (uint32_t)((ak_now_ns() - t0) / 1000);
	return (int)ak_keep_restored();
}

int asuka_dev_reload(const uint8_t *p, size_t n, uint32_t *micros, char **err) { return dev_load(p, n, false, micros, err); }

int asuka_dev_restart(const uint8_t *p, size_t n, uint32_t *micros, char **err) { return dev_load(p, n, true, micros, err) < 0 ? -1 : 0; }

int asuka_dev_connect(const char *host_name, int port, const char *hello, char **err) {
	if (err) *err = NULL;
	if (!started) {
		fail(err, "asuka: asuka_dev_connect before asuka_start");
		return -1;
	}
	dev_begin();
	char msg[256];
	if (!ak_dev_connect(host_name, port, hello, true, msg, sizeof msg)) {
		fail(err, msg);
		return -1;
	}
	return 0;
}

int asuka_dev_listen(int port, const char *hello, char **err) {
	if (err) *err = NULL;
	if (!started) {
		fail(err, "asuka: asuka_dev_listen before asuka_start");
		return -1;
	}
	dev_begin();
	char msg[256];
	if (!ak_dev_listen(port, hello, true, msg, sizeof msg)) {
		fail(err, msg);
		return -1;
	}
	return 0;
}

void asuka_free_string(char *s) { free(s); }

// The core's allocator's (base/alloc.c): what the core hands out
// (asuka_native_invoke's answer, an ak_buf's) is its memory, not the C
// library's.
void *asuka_alloc(size_t n) { return ak_alloc(n); }
void asuka_free(void *p) { ak_free(p); }

// asuka_attach tells the host again which view is the root, and resends
// every frame (an Android activity recreated took over the views).
void asuka_attach(void) {
	if (started) ak_ui_reattach();
}

// Events a host sends before it starts the core (Android reports its
// environment, the screen's size and insets, first) wait for it.
static ak_buf early;

void asuka_event(const uint8_t *buf, size_t len) {
	if (started) {
		ak_ui_dispatch(buf, len);
		return;
	}
	ak_buf_u32(&early, (uint32_t)len);
	ak_buf_put(&early, buf, len);
}

int asuka_quit_request(int ending) { return started && ak_ui_quit_request(ending != 0) ? 1 : 0; }

static void dispatch_early(void) {
	ak_reader r = {early.data, early.len, 0, false};
	while (r.off < r.len) {
		uint32_t n;
		const uint8_t *p = ak_read_bytes(&r, &n);
		if (r.err) break;
		ak_ui_dispatch(p, n);
	}
	ak_buf_free(&early);
}

int64_t asuka_list_count(uint32_t list) { return started ? ak_ui_list_count(list) : 0; }

void asuka_list_bind(uint32_t list, int64_t index, uint32_t cell) {
	if (started) ak_ui_list_bind(list, index, cell);
}

void asuka_list_unbind(uint32_t list, uint32_t cell) {
	if (started) ak_ui_list_unbind(list, cell);
}

void asuka_run_posted(void) {
	if (started) {
		ak_ui_run_posted();
	} else {
		ak_sched_run_posted();  // a program's (abi/program.c): no UI
		ak_stdio_flush_all();   // the host's loop goes idle next
	}
}

void asuka_setenv(const char *key, const char *value) {
#if defined(_WIN32)
	_putenv_s(key, value);
#else
	setenv(key, value, 1);
#endif
}

void asuka_set_app_dir(const char *path) { ak_fs_set_app_dir(path); }

void asuka_set_start(const char *href) { ak_router_set_start(href ? ak_cstr(href) : ak_cstr("")); }

void asuka_native_done(uint64_t call, const uint8_t *p, size_t len) { ak_native_done(call, p, len); }

size_t asuka_native_invoke(uint64_t id, const uint8_t *p, size_t len, uint8_t **out) { return ak_native_invoke(id, p, len, out); }
