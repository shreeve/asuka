// The C core's host for tests. See fake.h.
#include "fake/fake.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

// Misuse.

// fail reports a misuse: the fail function hears of it (and the caller
// drops what it was doing), else it panics.
AK_PRINTF(2, 3) static void fail(ak_fake *f, const char *fmt, ...) {
	char msg[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);
	ak_buf_reset(&f->error);
	ak_buf_put(&f->error, msg, strlen(msg) + 1);
	f->error.len--;
	if (!f->fail_fn) ak_panic("fake: %s", msg);
	f->fail_fn(f->fail_ctx, msg);
}

static void location(ak_fake *f, ak_fake_view *v, ak_span args);
static bool listening(ak_fake *f, const ak_fake_view *v, ak_event_kind e);
static void focus_to(ak_fake *f, ak_fake_view *v, bool keyboard, bool posted);
static void report_anchors(ak_fake *f);

static const char *kind_str(ak_view_kind k, char buf[16]) {
	if (k == AK_FAKE_KIND_CELL) return "Cell";
	const char *n = ak_kind_name(k);
	if (n) return n;
	snprintf(buf, 16, "Kind(%u)", k);
	return buf;
}

// UTF-8.

// next_rune returns the length of the rune at p (n > 0 bytes left), as
// Go's range over a string steps: an invalid byte is one rune.
static uint32_t rune_len(const uint8_t *p, uint32_t n) {
	uint8_t c = p[0];
	uint32_t l;
	uint32_t min;
	if (c < 0x80) return 1;
	if (c >= 0xc2 && c < 0xe0) l = 2, min = 0x80;
	else if (c >= 0xe0 && c < 0xf0) l = 3, min = 0x800;
	else if (c >= 0xf0 && c < 0xf5) l = 4, min = 0x10000;
	else return 1;
	if (l > n) return 1;
	uint32_t r = c & (0x7f >> l);
	for (uint32_t i = 1; i < l; i++) {
		if ((p[i] & 0xc0) != 0x80) return 1;
		r = r << 6 | (p[i] & 0x3f);
	}
	if (r < min || r > 0x10ffff || (r >= 0xd800 && r < 0xe000)) return 1;
	return l;
}

// runes counts s's runes as Go's len([]rune(s)) does.
static int64_t runes(ak_span s) {
	const uint8_t *p = (const uint8_t *)s.p;
	int64_t n = 0;
	for (uint32_t i = 0; i < s.len; i += rune_len(p + i, s.len - i)) n++;
	return n;
}

// Views by ID: open addressing, linear probing.

static uint32_t slot_of(ak_id id, uint32_t cap) { return (uint32_t)(id * UINT32_C(2654435761)) & (cap - 1); }

ak_fake_view *ak_fake_view_of(const ak_fake *f, ak_id id) {
	if (!f->slots_cap) return NULL;
	for (uint32_t i = slot_of(id, f->slots_cap);; i = (i + 1) & (f->slots_cap - 1)) {
		ak_fake_view *v = f->slots[i];
		if (!v) return NULL;
		if (v->id == id) return v;
	}
}

static void slots_put(ak_fake *f, ak_fake_view *v) {
	if ((f->nviews + 1) * 4 > f->slots_cap * 3) {
		uint32_t old = f->slots_cap;
		ak_fake_view **olds = f->slots;
		f->slots_cap = old ? old * 2 : 64;
		f->slots = ak_alloc(sizeof(ak_fake_view *) * f->slots_cap);
		for (uint32_t i = 0; i < old; i++) {
			ak_fake_view *w = olds[i];
			if (!w) continue;
			uint32_t j = slot_of(w->id, f->slots_cap);
			while (f->slots[j]) j = (j + 1) & (f->slots_cap - 1);
			f->slots[j] = w;
		}
		ak_free(olds);
	}
	uint32_t j = slot_of(v->id, f->slots_cap);
	while (f->slots[j]) j = (j + 1) & (f->slots_cap - 1);
	f->slots[j] = v;
	f->nviews++;
}

static void slots_delete(ak_fake *f, ak_id id) {
	uint32_t mask = f->slots_cap - 1;
	uint32_t i = slot_of(id, f->slots_cap);
	while (f->slots[i]->id != id) i = (i + 1) & mask;
	f->slots[i] = NULL;
	f->nviews--;
	// Shift back what probed past the hole.
	for (uint32_t j = (i + 1) & mask; f->slots[j]; j = (j + 1) & mask) {
		uint32_t home = slot_of(f->slots[j]->id, f->slots_cap);
		if (((j - home) & mask) >= ((j - i) & mask)) {
			f->slots[i] = f->slots[j];
			f->slots[j] = NULL;
			i = j;
		}
	}
}

static int by_id(const void *a, const void *b) {
	ak_id x = (*(ak_fake_view *const *)a)->id, y = (*(ak_fake_view *const *)b)->id;
	return x < y ? -1 : x > y;
}

// collect appends the views pred takes, in ID order.
static void collect(const ak_fake *f, bool (*pred)(const ak_fake_view *, const void *), const void *arg, ak_fake_view_vec *out) {
	uint32_t start = out->len;
	for (uint32_t i = 0; i < f->slots_cap; i++) {
		ak_fake_view *v = f->slots[i];
		if (v && pred(v, arg)) ak_vec_push(out, v);
	}
	if (out->len > start + 1) qsort(out->data + start, out->len - start, sizeof(ak_fake_view *), by_id);
}

// Props.

static ak_fake_prop *prop_slot(const ak_fake_view *v, ak_prop p) {
	for (uint32_t i = 0; i < v->props.len; i++)
		if (v->props.data[i].prop == p) return &v->props.data[i];
	return NULL;
}

const ak_value *ak_fake_prop_of(const ak_fake_view *v, ak_prop p) {
	ak_fake_prop *s = prop_slot(v, p);
	return s ? &s->value : NULL;
}

ak_span ak_fake_string(const ak_fake_view *v, ak_prop p) {
	const ak_value *x = ak_fake_prop_of(v, p);
	return x ? x->s : (ak_span){"", 0};
}

bool ak_fake_bool(const ak_fake_view *v, ak_prop p) {
	const ak_value *x = ak_fake_prop_of(v, p);
	return x && x->i != 0;
}

int64_t ak_fake_int(const ak_fake_view *v, ak_prop p) {
	const ak_value *x = ak_fake_prop_of(v, p);
	return x ? x->i : 0;
}

double ak_fake_float(const ak_fake_view *v, ak_prop p) {
	const ak_value *x = ak_fake_prop_of(v, p);
	return x ? x->f : 0;
}

static ak_span dup_str(ak_span s) {
	if (!s.len) return (ak_span){"", 0};
	char *p = ak_alloc(s.len);
	memcpy(p, s.p, s.len);
	return (ak_span){p, s.len};
}

static void free_str(ak_span s) {
	if (s.len) ak_free((void *)s.p);
}

// set_prop sets p to a copy of x.
static void set_prop(ak_fake_view *v, ak_prop p, const ak_value *x) {
	ak_fake_prop *s = prop_slot(v, p);
	if (!s) {
		ak_vec_push(&v->props, ((ak_fake_prop){.prop = p}));
		s = &v->props.data[v->props.len - 1];
	} else {
		free_str(s->value.s);
	}
	s->value = *x;
	s->value.s = dup_str(x->s);
}

static void set_int(ak_fake_view *v, ak_prop p, uint8_t type, int64_t i) { set_prop(v, p, &(ak_value){.type = type, .i = i}); }

bool ak_fake_listens(const ak_fake_view *v, ak_event_kind e) {
	for (uint32_t i = 0; i < v->listens.len; i++)
		if (v->listens.data[i] == e) return true;
	return false;
}

const ak_fake_native *ak_fake_native_value(const ak_fake_view *v, const char *name) {
	ak_span n = ak_cstr(name);
	for (uint32_t i = 0; i < v->native.len; i++)
		if (ak_span_eq(v->native.data[i].name, n)) return &v->native.data[i];
	return NULL;
}

// set_native keeps a NativeProp's value (ui.Native's wire: a u32-length
// name, u8 type, the value) in v->native; it ignores one it can't read,
// as Go's does.
static void set_native(ak_fake_view *v, ak_span b) {
	ak_reader r = {(const uint8_t *)b.p, b.len, 0, false};
	ak_fake_native n = {.name = ak_read_str(&r)};
	n.type = ak_read_u8(&r);
	switch (n.type) {
	case 1: n.i = ak_read_u8(&r) != 0; break;
	case 2: n.i = (int64_t)ak_read_u64(&r); break;
	case 3: n.f = ak_read_f64(&r); break;
	case 4:
	case 5: n.s = ak_read_str(&r); break;
	default: return;
	}
	if (r.err) return;
	ak_fake_native *slot = NULL;
	for (uint32_t i = 0; i < v->native.len; i++)
		if (ak_span_eq(v->native.data[i].name, n.name)) slot = &v->native.data[i];
	if (slot) {
		free_str(slot->s);
		n.name = slot->name;
	} else {
		n.name = dup_str(n.name);
		ak_vec_push(&v->native, n);
		slot = &v->native.data[v->native.len - 1];
	}
	n.s = dup_str(n.s);
	*slot = n;
}

static void free_view(ak_fake_view *v) {
	for (uint32_t i = 0; i < v->props.len; i++) free_str(v->props.data[i].value.s);
	for (uint32_t i = 0; i < v->native.len; i++) {
		free_str(v->native.data[i].name);
		free_str(v->native.data[i].s);
	}
	ak_vec_free(&v->children);
	ak_vec_free(&v->props);
	ak_vec_free(&v->listens);
	ak_vec_free(&v->items);
	ak_vec_free(&v->sections);
	ak_vec_free(&v->native);
	ak_free(v);
}

static void remove_child(ak_fake_view *p, ak_fake_view *c) {
	for (uint32_t i = 0; i < p->children.len; i++) {
		if (p->children.data[i] != c) continue;
		memmove(&p->children.data[i], &p->children.data[i + 1], (p->children.len - i - 1) * sizeof(ak_fake_view *));
		p->children.len--;
		break;
	}
	c->parent = NULL;
}

// The host.

static const ak_environment default_env = {
	.width = 390, .height = 844, .safe_top = 47, .safe_bottom = 34,
	.scale = 3, .font_scale = 1,
	.locale = {"en_US", 5}, .languages = {"en-US", 5},
};

ak_fake *ak_fake_new(void) {
	ak_fake *f = ak_alloc(sizeof *f);
	f->env = default_env;
	return f;
}

void ak_fake_free(ak_fake *f) {
	if (!f) return;
	for (uint32_t i = 0; i < f->slots_cap; i++)
		if (f->slots[i]) free_view(f->slots[i]);
	ak_free(f->slots);
	ak_vec_free(&f->ops);
	ak_buf_free(&f->arena);
	ak_buf_free(&f->error);
	ak_buf_free(&f->pending);
	ak_buf_free(&f->ev);
	ak_buf_free(&f->dev_error);
	for (uint32_t i = 0; i < f->histories.len; i++) {
		ak_fake_history *h = &f->histories.data[i];
		for (uint32_t j = 0; j < h->entries.len; j++) {
			ak_buf_free(&h->entries.data[j].href);
			ak_buf_free(&h->entries.data[j].state);
		}
		ak_vec_free(&h->entries);
	}
	ak_vec_free(&f->histories);
	ak_fake_desk_free(f);
	ak_fake_update_free(f);
	ak_free(f);
}

// The vtable has no context: it drives the host most recently given.
static ak_fake *current;

static void cb_apply(const uint8_t *buf, size_t len) { ak_fake_apply(current, buf, len); }
static void cb_measure(uint32_t id, float w, float h, float aw, float ah, float *ow, float *oh) { ak_fake_measure(current, id, w, h, aw, ah, ow, oh); }
static void cb_post(void) { ak_fake_post(current); }
static size_t cb_native_new(const uint8_t *cls, size_t n, uint32_t iface, uint8_t *out, size_t cap, uint8_t **big) {
	AK_UNUSED(cls), AK_UNUSED(n), AK_UNUSED(iface), AK_UNUSED(out), AK_UNUSED(cap);
	*big = NULL;
	return 0;
}
static size_t cb_native_call(uint64_t h, uint32_t m, const uint8_t *in, size_t len, uint8_t *out, size_t cap, uint8_t **big) {
	AK_UNUSED(h), AK_UNUSED(m), AK_UNUSED(in), AK_UNUSED(len), AK_UNUSED(out), AK_UNUSED(cap);
	*big = NULL;
	return 0;
}
static void cb_native_start(uint64_t h, uint32_t m, const uint8_t *in, size_t len, uint64_t call) {
	AK_UNUSED(h), AK_UNUSED(m), AK_UNUSED(in), AK_UNUSED(len), AK_UNUSED(call);
}
static void cb_native_cancel(uint64_t call) { AK_UNUSED(call); }
static void cb_native_release(uint64_t h) { AK_UNUSED(h); }
static void cb_native_free(void *p) { free(p); }

asuka_host_callbacks ak_fake_callbacks(ak_fake *f) {
	current = f;
	return (asuka_host_callbacks){
		.apply = cb_apply,
		.measure = cb_measure,
		.post = cb_post,
		.native_new = cb_native_new,
		.native_call = cb_native_call,
		.native_start = cb_native_start,
		.native_cancel = cb_native_cancel,
		.native_release = cb_native_release,
		.native_free = cb_native_free,
	};
}

// Posting.

void ak_fake_post(ak_fake *f) { atomic_fetch_add(&f->posted, 1); }

static uint32_t pending_events(const ak_fake *f) {
	uint32_t n = 0;
	for (size_t off = 0; off + 4 <= f->pending.len; n++) {
		uint32_t len;
		memcpy(&len, f->pending.data + off, 4);
		off += 4 + len;
	}
	return n;
}

uint32_t ak_fake_pending(ak_fake *f) { return atomic_load(&f->posted) + pending_events(f); }

static void send(ak_fake *f, const uint8_t *buf, size_t len) {
	if (!f->core.event) {
		fail(f, "not connected: no core to send events to");
		return;
	}
	f->core.event(f->core.ctx, buf, len);
}

// post_event queues e for ak_fake_run_posted (as Go's Post of a
// dispatch).
void ak_fake_post_event(ak_fake *f, const ak_event *e) {
	size_t at = f->pending.len;
	ak_buf_u32(&f->pending, 0);
	ak_event_append(&f->pending, e);
	uint32_t n = (uint32_t)(f->pending.len - at - 4);
	memcpy(f->pending.data + at, &n, 4);
}

void ak_fake_run_posted(ak_fake *f) {
	for (;;) {
		uint32_t posted = atomic_exchange(&f->posted, 0);
		if (posted && f->core.run_posted) f->core.run_posted(f->core.ctx);
		if (!f->pending.len) {
			if (!posted) return;
			continue;
		}
		// Events posted while these run wait for the next round.
		ak_buf q = f->pending;
		f->pending = (ak_buf){0};
		for (size_t off = 0; off < q.len;) {
			uint32_t n;
			memcpy(&n, q.data + off, 4);
			send(f, q.data + off + 4, n);
			off += 4 + n;
		}
		if (!f->pending.data) {
			f->pending = q;
			ak_buf_reset(&f->pending);
		} else {
			ak_buf_free(&q);
		}
	}
}

// Recorded ops.

void ak_fake_reset(ak_fake *f) {
	f->ops.len = 0;
	ak_buf_reset(&f->arena);
	f->applies = f->measures = 0;
}

ak_op ak_fake_op_at(const ak_fake *f, uint32_t i) {
	AK_ASSERT(i < f->ops.len);
	const ak_fake_record *r = &f->ops.data[i];
	ak_op op = r->op;
	if (op.value.s.len) op.value.s.p = (const char *)f->arena.data + r->off;
	else op.value.s.p = "";
	return op;
}

uint32_t ak_fake_count(const ak_fake *f, uint8_t code) {
	uint32_t n = 0;
	for (uint32_t i = 0; i < f->ops.len; i++) n += f->ops.data[i].op.code == code;
	return n;
}

void ak_fake_commands(const ak_fake *f, ak_cmd c, ak_fake_index_vec *out) {
	for (uint32_t i = 0; i < f->ops.len; i++)
		if (f->ops.data[i].op.code == AK_OP_COMMAND && f->ops.data[i].op.cmd == c) ak_vec_push(out, i);
}

void ak_fake_log(const ak_fake *f, ak_buf *out) {
	for (uint32_t i = 0; i < f->ops.len; i++) {
		ak_op op = ak_fake_op_at(f, i);
		if (op.code == AK_OP_SET_PROP) {
			const ak_fake_view *v = ak_fake_view_of(f, op.id);
			ak_op_format_kind(out, &op, v ? v->kind : 0);
		} else {
			ak_op_format(out, &op);
		}
		ak_buf_u8(out, '\n');
	}
}

void ak_fake_dump(const ak_fake *f, ak_buf *out) {
	for (uint32_t i = 0; i < f->ops.len; i++) {
		ak_op op = ak_fake_op_at(f, i);
		ak_op_dump(out, &op);
		ak_buf_u8(out, '\n');
	}
}

// Applying.

static ak_fake_view *view(ak_fake *f, ak_id id, const char *op) {
	ak_fake_view *v = ak_fake_view_of(f, id);
	if (!v) fail(f, "%s: no view %u", op, id);
	return v;
}

static const char *const type_names[] = {"?", "bool", "int", "float", "string", "color", "edges", "enum", "bytes"};

// list_update keeps ListUpdate's args: u32 count, the item IDs (i64
// each), then, if there are any, u32 count and the sections (u32 start,
// i64 ID each).
static void list_update(ak_fake *f, ak_fake_view *v, ak_span args) {
	ak_reader r = {(const uint8_t *)args.p, args.len, 0, false};
	v->items.len = v->sections.len = 0;
	uint32_t n = ak_read_u32(&r);
	for (uint32_t i = 0; i < n && !r.err; i++) {
		int64_t id = (int64_t)ak_read_u64(&r);
		if (!r.err) ak_vec_push(&v->items, id);
	}
	if (!r.err && r.len - r.off >= 4) {
		uint32_t ns = ak_read_u32(&r);
		for (uint32_t i = 0; i < ns && !r.err; i++) {
			ak_fake_section s;
			s.start = ak_read_u32(&r);
			s.id = (int64_t)ak_read_u64(&r);
			if (!r.err) ak_vec_push(&v->sections, s);
		}
	}
	if (r.err) {
		fail(f, "ListUpdate on %u: args cut short", v->id);
		return;
	}
	if (v->sections.len && v->sections.data[0].start != 0) fail(f, "a list's first section doesn't start at 0");
}

// call runs a Call (u32 call ID, a u32-length module, method, then the
// args) with the call function and posts its Result: u32 call ID, u8 1
// and the answer, or u8 0 and the error's message.
static void call(ak_fake *f, ak_id root, ak_span b) {
	ak_reader r = {(const uint8_t *)b.p, b.len, 0, false};
	uint32_t id = ak_read_u32(&r);
	ak_span module = ak_read_str(&r);
	ak_span method = ak_read_str(&r);
	if (r.err) {
		fail(f, "Call: args cut short");
		return;
	}
	ak_span args = {b.p + r.off, (uint32_t)(r.len - r.off)};
	if (ak_fake_desk_call(f, root, id, module, method, args)) return;  // the tray's
	if (ak_fake_update_call(f, root, id, module, method, args)) return;
	ak_buf ans = {0};
	bool ok = false;
	if (f->call_fn) {
		ok = f->call_fn(f->call_ctx, module, method, args, &ans);
	} else {
		ak_buf_printf(&ans, "no module \"%.*s\"", (int)module.len, module.p);
	}
	ak_buf res = {0};
	ak_buf_u32(&res, id);
	ak_buf_u8(&res, ok ? 1 : 0);
	ak_buf_put(&res, ans.data, ans.len);
	ak_fake_post_event(f, &(ak_event){.id = root, .kind = AK_EVENT_RESULT, .s = {(const char *)res.data, (uint32_t)res.len}});
	ak_buf_free(&ans);
	ak_buf_free(&res);
}

// window_command keeps what of a window's commands the fake host follows:
// which window is in use (Present of a Window that isn't hidden,
// WindowAction Focus), and checks they're sent where they can be.
static void window_command(ak_fake *f, ak_fake_view *v, ak_cmd c, ak_span args) {
	switch (c) {
	case AK_CMD_WINDOW_OPTIONS:
		if (v->kind != AK_KIND_VIEW) fail(f, "WindowOptions not on the root");
		return;
	case AK_CMD_PRESENT: {
		if (v->kind != AK_KIND_WINDOW) return;
		ak_span cfg = ak_fake_string(v, AK_PROP_WINDOW_CONFIG);
		ak_window_config wc = ak_decode_window_config((const uint8_t *)cfg.p, cfg.len);
		if (!(wc.flags & AK_WINDOW_HIDDEN)) f->key = v->id;
		return;
	}
	case AK_CMD_WINDOW_ACTION: {
		uint8_t a;
		float x, y;
		bool ok = ak_decode_window_action((const uint8_t *)args.p, args.len, &a, &x, &y);
		if (v->kind == AK_KIND_SHEET) {
			// A sheet as tall as its content is sized with Fit.
			if (!ok || a != AK_WINDOW_ACTION_FIT) fail(f, "a WindowAction on a Sheet other than Fit");
			return;
		}
		char kb[16];
		if (v->kind != AK_KIND_WINDOW && v->kind != AK_KIND_VIEW) {
			fail(f, "WindowAction on a %s", kind_str(v->kind, kb));
			return;
		}
		if (!ok) {
			fail(f, "WindowAction: bad args");
			return;
		}
		if (a == AK_WINDOW_ACTION_FOCUS) f->key = v->kind == AK_KIND_WINDOW ? v->id : 0;
		return;
	}
	}
}

static void apply_op(ak_fake *f, const ak_op *op) {
	char kb[16], kb2[16];
	switch (op->code) {
	case AK_OP_CREATE: {
		if (ak_fake_view_of(f, op->id)) { fail(f, "Create: view %u exists", op->id); return; }
		if (op->id & AK_HOST_ID_BIT) { fail(f, "Create: %u is a host ID", op->id); return; }
		if (!ak_kind_name(op->kind)) { fail(f, "Create: unknown kind %u", op->kind); return; }
		ak_fake_view *v = ak_alloc(sizeof *v);
		v->id = op->id;
		v->kind = op->kind;
		if (v->kind == AK_KIND_LIST) f->nlists++;
		slots_put(f, v);
		return;
	}
	case AK_OP_DESTROY: {
		ak_fake_view *v = view(f, op->id, "Destroy");
		if (!v) return;
		if (v->parent) remove_child(v->parent, v);
		for (uint32_t i = 0; i < v->children.len; i++) v->children.data[i]->parent = NULL;
		ak_fake_desk_destroyed(f, v->id);
		if (f->focus == v->id) f->focus = 0;  // its focus goes with it, unreported
		if (f->dev_error_on == v->id) {
			fail(f, "Destroy: root %u went with its error overlay showing (DevErrorClear first)", v->id);
			f->dev_error_on = 0;
		}
		if (v->kind == AK_KIND_LIST) f->nlists--;
		if (v->presented) f->npresented--;
		if (v->id == f->root_id) f->root_id = 0;
		slots_delete(f, v->id);
		free_view(v);
		return;
	}
	case AK_OP_INSERT: {
		ak_fake_view *p = view(f, op->id, "Insert"), *c = p ? view(f, op->child, "Insert") : NULL;
		if (!c) return;
		if (c->parent) { fail(f, "Insert: %u already has parent %u", c->id, c->parent->id); return; }
		if (op->index > p->children.len) { fail(f, "Insert: index %u of %u into %u", op->index, p->children.len, p->id); return; }
		if ((p->kind == AK_KIND_SCROLL || p->kind == AK_KIND_SHEET || p->kind == AK_KIND_WINDOW) && p->children.len > 0)
			{ fail(f, "Insert: a %s has one child", kind_str(p->kind, kb)); return; }
		if ((p->kind == AK_KIND_STACK || p->kind == AK_KIND_TABS) && c->kind != AK_KIND_SCREEN)
			{ fail(f, "Insert: a %s holds Screens, not a %s", kind_str(p->kind, kb), kind_str(c->kind, kb2)); return; }
		ak_vec_reserve(&p->children, 1);
		memmove(&p->children.data[op->index + 1], &p->children.data[op->index], (p->children.len - op->index) * sizeof(ak_fake_view *));
		p->children.data[op->index] = c;
		p->children.len++;
		c->parent = p;
		return;
	}
	case AK_OP_REMOVE: {
		ak_fake_view *p = view(f, op->id, "Remove"), *c = p ? view(f, op->child, "Remove") : NULL;
		if (!c) return;
		if (c->parent != p) { fail(f, "Remove: %u is not a child of %u", c->id, p->id); return; }
		remove_child(p, c);
		return;
	}
	case AK_OP_SET_PROP: {
		ak_fake_view *v = view(f, op->id, "SetProp");
		if (!v) return;
		const ak_prop_info *info = ak_prop_of(v->kind, op->prop);
		if (!info || info->type != op->value.type)
			{ fail(f, "SetProp: %s has no %s prop %u", kind_str(v->kind, kb), op->value.type < AK_T_COUNT ? type_names[op->value.type] : "?", op->prop); return; }
		if (v->kind == AK_KIND_SHEET && (op->prop == AK_PROP_SHEET_FULL_SCREEN || op->prop == AK_PROP_SHEET_MOVABLE || op->prop == AK_PROP_SHEET_RESIZABLE ||
		                                     op->prop == AK_PROP_SHEET_DISMISS_OUTSIDE) && v->presented)
			{ fail(f, "SetProp: a sheet's %s after its Present: a host presents it as it was", op->prop == AK_PROP_SHEET_MOVABLE ? "Movable" : op->prop == AK_PROP_SHEET_RESIZABLE ? "Resizable" : op->prop == AK_PROP_SHEET_DISMISS_OUTSIDE ? "DismissOutside" : "FullScreen"); return; }
		set_prop(v, op->prop, &op->value);
		if (v->kind == AK_KIND_NATIVE && op->prop == AK_PROP_NATIVE_PROP) set_native(v, op->value.s);
		if (v->kind == AK_KIND_OVERLAY && op->prop == AK_PROP_OVERLAY_ANCHOR) v->anchor_known = false;  // reported again
		return;
	}
	case AK_OP_SET_FRAME: {
		ak_fake_view *v = view(f, op->id, "SetFrame");
		if (v) memcpy(v->frame, op->frame, sizeof v->frame);
		return;
	}
	case AK_OP_LISTEN: {
		ak_fake_view *v = view(f, op->id, "Listen");
		if (v && !ak_fake_listens(v, op->event)) {
			ak_vec_push(&v->listens, op->event);
			ak_fake_desk_listen(f, v, op->event);
		}
		return;
	}
	case AK_OP_UNLISTEN: {
		ak_fake_view *v = view(f, op->id, "Unlisten");
		if (!v) return;
		for (uint32_t i = 0; i < v->listens.len; i++)
			if (v->listens.data[i] == op->event) v->listens.data[i--] = v->listens.data[--v->listens.len];
		return;
	}
	case AK_OP_COMMAND: {
		ak_fake_view *v = view(f, op->id, "Command");
		if (!v) return;
		switch (op->cmd) {
		case AK_CMD_CALL: call(f, op->id, op->value.s); return;
		case AK_CMD_LOCATION: location(f, v, op->value.s); return;
		case AK_CMD_LIST_UPDATE: list_update(f, v, op->value.s); break;
		case AK_CMD_PRESENT:
			if (v->kind != AK_KIND_SHEET && v->kind != AK_KIND_POPOVER && v->kind != AK_KIND_ALERT && v->kind != AK_KIND_WINDOW && v->kind != AK_KIND_SNACKBAR &&
			    v->kind != AK_KIND_OVERLAY)
				{ fail(f, "Present on a %s", kind_str(v->kind, kb)); return; }
			if (v->kind == AK_KIND_OVERLAY && !v->presented) {
				// Its root's size and insets: the root's, as a host's layer over it.
				ak_fake_post_event(f, &(ak_event){.id = v->id, .kind = AK_EVENT_ENVIRONMENT, .env = f->env});
			}
			if (!v->presented) f->npresented++;
			v->presented = true;
			v->present_seq = ++f->present_seq;
			break;
		case AK_CMD_DEV_ERROR: {
			ak_dev_error e;
			if (!v->root) { fail(f, "DevError to %u, not the root", v->id); return; }
			if (!ak_decode_dev_error((const uint8_t *)op->value.s.p, op->value.s.len, &e)) { fail(f, "DevError: bad arguments"); return; }
			ak_buf_reset(&f->dev_error);
			ak_buf_put(&f->dev_error, op->value.s.p, op->value.s.len);
			f->dev_error_on = v->id;
			return;
		}
		case AK_CMD_DEV_ERROR_CLEAR:
			if (f->dev_error_on != v->id) { fail(f, "DevErrorClear to %u, which shows no error overlay", v->id); return; }
			f->dev_error_on = 0;
			return;
		case AK_CMD_FOCUS:
			if (ak_fake_takes_focus(v)) focus_to(f, v, f->keyboard, true);
			break;
		case AK_CMD_BLUR:
			if (f->focus == v->id) focus_to(f, NULL, false, true);
			break;
		}
		window_command(f, v, op->cmd, op->value.s);
		ak_fake_desk_command(f, v, op->cmd, op->value.s);
		return;
	}
	case AK_OP_SET_ROOT: {
		ak_fake_view *r = view(f, op->id, "SetRoot");
		if (!r) return;
		for (uint32_t i = 0; i < f->slots_cap; i++)
			if (f->slots[i]) f->slots[i]->root = false;
		r->root = true;
		f->root_id = r->id;
		return;
	}
	}
	fail(f, "unknown op %u", op->code);
}

// ---- Histories ----

ak_fake_history *ak_fake_history_of(ak_fake *f, const ak_fake_view *win) {
	if (!win) win = ak_fake_root(f);
	if (!win) return NULL;
	for (uint32_t i = 0; i < f->histories.len; i++)
		if (f->histories.data[i].win == win->id) return &f->histories.data[i];
	return NULL;
}

void ak_fake_hrefs(const ak_fake_history *h, ak_buf *out) {
	for (uint32_t i = 0; h && i < h->entries.len; i++) {
		if (i) ak_buf_u8(out, ' ');
		ak_buf_put(out, h->entries.data[i].href.data, h->entries.data[i].href.len);
	}
}

static void set_entry(ak_fake_entry *e, const ak_location *l) {
	e->href.len = e->state.len = 0;
	ak_buf_put(&e->href, l->href.p, l->href.len);
	ak_buf_put(&e->state, l->state.p, l->state.len);
}

static void append_entry(ak_fake_history *h, const ak_location *l) {
	ak_vec_push(&h->entries, ((ak_fake_entry){0}));
	set_entry(&h->entries.data[h->entries.len - 1], l);
}

static void truncate_entries(ak_fake_history *h, uint32_t n) {
	while (h->entries.len > n) {
		ak_fake_entry *e = &h->entries.data[--h->entries.len];
		ak_buf_free(&e->href);
		ak_buf_free(&e->state);
	}
}

// location applies a Location command to window v's history, as a
// browser's pushState, replaceState and history.go do.
static void location(ak_fake *f, ak_fake_view *v, ak_span args) {
	ak_location l;
	if (!ak_decode_location((const uint8_t *)args.p, args.len, &l)) {
		fail(f, "Location: its args don't decode");
		return;
	}
	if (v->kind != AK_KIND_VIEW && v->kind != AK_KIND_WINDOW) {
		fail(f, "Location sent to a view that isn't the root or a Window");
		return;
	}
	ak_fake_history *h = ak_fake_history_of(f, v);
	if (!h) {
		ak_vec_push(&f->histories, ((ak_fake_history){.win = v->id, .index = -1}));
		h = &f->histories.data[f->histories.len - 1];
	}
	h->commands++;
	if (f->popping) {
		// The browser went there already: the router's state is that
		// entry's, or the app stays where it was (Forward: back again).
		if (l.action == AK_LOCATION_ACTION_FORWARD) h->index = f->pop_from;
		if (h->index >= 0 && (uint32_t)h->index < h->entries.len) set_entry(&h->entries.data[h->index], &l);
		return;
	}
	switch (l.action) {
	case AK_LOCATION_ACTION_PUSH:
		truncate_entries(h, (uint32_t)(h->index + 1));
		append_entry(h, &l);
		h->index++;
		break;
	case AK_LOCATION_ACTION_BACK: {
		// The latest entry back with the same href, as the web host goes
		// back; none: this entry is replaced.
		int32_t i = -1;
		for (int32_t j = h->index - 1; j >= 0; j--) {
			ak_buf *b = &h->entries.data[j].href;
			if (ak_span_eq(ak_strn(b->data, b->len), l.href)) {
				i = j;
				break;
			}
		}
		if (i >= 0) h->index = i;
		if (h->index < 0) {
			append_entry(h, &l);
			h->index = 0;
		}
		set_entry(&h->entries.data[h->index], &l);
		break;
	}
	default:
		if (h->index < 0) {
			append_entry(h, &l);
			h->index = 0;
		} else {
			set_entry(&h->entries.data[h->index], &l);
		}
	}
}

bool ak_fake_go(ak_fake *f, ak_fake_view *win, int32_t n) {
	if (!win) win = ak_fake_root(f);
	ak_fake_history *h = ak_fake_history_of(f, win);
	if (!h) return false;
	int32_t i = h->index + n;
	if (i < 0 || (uint32_t)i >= h->entries.len) return false;
	f->pop_from = h->index;
	h->index = i;
	ak_buf b = {0};
	ak_fake_entry *e = &h->entries.data[i];
	ak_append_location_event(&b, ak_strn(e->href.data, e->href.len), ak_strn(e->state.data, e->state.len));
	f->popping = true;
	if (listening(f, win, AK_EVENT_LOCATION))
		ak_fake_dispatch(f, &(ak_event){.id = win->id, .kind = AK_EVENT_LOCATION, .s = ak_strn(b.data, b.len)});
	f->popping = false;
	ak_buf_free(&b);
	return true;
}

ak_fake_work ak_fake_totals;

void ak_fake_apply(ak_fake *f, const uint8_t *buf, size_t len) {
	f->applies++;
	ak_fake_totals.applies++;
	ak_fake_totals.bytes += len;
	ak_batch_reader br;
	ak_batch_reader_init(&br, buf, len);
	ak_op op;
	while (ak_batch_next(&br, &op)) {
		// The batch's bytes die with it.
		if (!f->unlogged) {
			ak_fake_record r = {op, f->arena.len};
			ak_buf_put(&f->arena, op.value.s.p, op.value.s.len);
			r.op.value.s.p = NULL;
			ak_vec_push(&f->ops, r);
		}
		ak_fake_totals.ops++;
		apply_op(f, &op);
	}
	if (br.err != AK_OK) fail(f, "a bad batch: %s", ak_err_string(br.err));
	report_anchors(f);
}

// Measuring.

static bool fin(float x) { return x == x && x < AK_INF && x > -AK_INF; }

// trunc_i64 is Go's int(x) for the sizes here, saturating.
static int64_t trunc_i64(float x) {
	if (x != x) return 0;
	if (x >= 9.2e18f) return INT64_MAX;
	if (x <= -9.2e18f) return INT64_MIN;
	return (int64_t)x;
}

// measure_text lays out 8pt characters on 20pt lines, wrapping anywhere
// (min-content is one character wide).
static void measure_text(ak_span text, float width, float avail_w, float *w, float *h) {
	int64_t n = runes(text);
	if (n == 0) {
		*w = *h = 0;
		return;
	}
	float tw = width;
	if (tw != tw) {
		if (avail_w == -AK_INF) tw = 8;
		else if (fin(avail_w)) tw = ak_minf((float)(n * 8), ak_maxf(8, truncf(avail_w / 8) * 8));
		else tw = (float)(n * 8);
	}
	int64_t per = trunc_i64(tw / 8);
	if (per < 1) per = 1;
	int64_t lines = n / per + (n % per != 0);
	*w = tw;
	*h = (float)(lines * 20);
}

// rich_size measures a RichText as Go's fake does: the paragraphs' text,
// a line each at least, as tall as its inline components.
static void rich_size(const ak_fake_view *v, float width, float avail_w, float *ow, float *oh) {
	ak_span in = ak_fake_string(v, AK_PROP_RICH_TEXT_INLINES);
	ak_reader ir = {(const uint8_t *)in.p, in.len, 0, false};
	uint32_t ninl = in.len >= 4 ? ak_read_u32(&ir) : 0;
	uint32_t next = 0;
	ak_span c = ak_fake_string(v, AK_PROP_RICH_TEXT_CONTENT);
	ak_reader r = {(const uint8_t *)c.p, c.len, 0, false};
	float w = 0, h = 0;
	if (c.len) {
		uint32_t nstyles = ak_read_u32(&r);
		AK_VEC(bool) inline_style = {0};
		for (uint32_t i = 0; i < nstyles && !r.err; i++) {
			ak_read_u8(&r);
			ak_read_f32(&r);
			ak_read_u8(&r);
			uint8_t fl = ak_read_u8(&r);
			ak_read_u64(&r);
			ak_read_u64(&r);
			ak_read_str(&r);
			ak_vec_push(&inline_style, (fl & AK_RICH_INLINE) != 0);
		}
		uint32_t np = ak_read_u32(&r);
		ak_buf text = {0};
		for (uint32_t i = 0; i < np && !r.err; i++) {
			uint32_t nr = ak_read_u32(&r);
			ak_buf_reset(&text);
			uint32_t first_inline = next;
			uint32_t inl = 0;
			for (uint32_t j = 0; j < nr && !r.err; j++) {
				uint32_t st = ak_read_u32(&r);
				ak_span t = ak_read_str(&r);
				if (r.err) break;
				if (st >= inline_style.len) {
					r.err = true;
					break;
				}
				ak_buf_put(&text, t.p, t.len);
				if (inline_style.data[st]) inl++;
			}
			float tw, th;
			measure_text((ak_span){(const char *)text.data, (uint32_t)text.len}, width, avail_w, &tw, &th);
			w = ak_maxf(w, tw);
			for (uint32_t k = 0; k < inl && first_inline + k < ninl; k++) {
				ak_reader e = {(const uint8_t *)in.p, in.len, 4 + (size_t)(first_inline + k) * 12 + 8, false};
				th = ak_maxf(th, ak_read_f32(&e));
				next++;
			}
			h += ak_maxf(th, 20);
		}
		ak_buf_free(&text);
		ak_vec_free(&inline_style);
		if (r.err) ak_panic("fake: short RichText content");
	}
	*ow = w;
	*oh = h;
}

void ak_fake_measure(ak_fake *f, ak_id id, float width, float height, float avail_w, float avail_h, float *out_w, float *out_h) {
	f->measures++;
	ak_fake_totals.measures++;
	*out_w = *out_h = 0;
	ak_fake_view *v = view(f, id, "Measure");
	if (!v) return;
	if (f->measure_fn) {
		f->measure_fn(f->measure_ctx, v, width, height, avail_w, avail_h, out_w, out_h);
		return;
	}
	float w = 0, h = 0;
	switch (v->kind) {
	case AK_KIND_TEXT: measure_text(ak_fake_string(v, AK_PROP_TEXT_TEXT), width, avail_w, &w, &h); break;
	case AK_KIND_BUTTON:
		w = (float)runes(ak_fake_string(v, AK_PROP_BUTTON_TITLE)) * 8 + 24;
		h = 44;
		break;
	case AK_KIND_TEXT_FIELD:
		w = 200, h = 36;
		if (ak_fake_bool(v, AK_PROP_TEXT_FIELD_MULTILINE)) {
			// 20pt lines, as many as its text has, up to its line limit.
			ak_span s = ak_fake_string(v, AK_PROP_TEXT_FIELD_VALUE);
			int64_t lines = 1;
			for (uint32_t i = 0; i < s.len; i++) lines += s.p[i] == '\n';
			int64_t l = ak_fake_int(v, AK_PROP_LINE_LIMIT);
			if (l > 0 && l < lines) lines = l;
			h = (float)(16 + 20 * lines);
		}
		break;
	case AK_KIND_TOGGLE: w = 51, h = 31; break;
	case AK_KIND_PAGE_CONTROL:
		w = (float)ak_fake_int(v, AK_PROP_PAGE_CONTROL_COUNT) * 16 + 16;
		h = 26;
		break;
	case AK_KIND_SLIDER:
		w = fin(avail_w) ? avail_w : 200;
		h = 30;
		break;
	case AK_KIND_ACTIVITY: w = h = 20; break;
	case AK_KIND_SYMBOL: w = h = 22; break;
	case AK_KIND_WHEEL: w = 320, h = 216; break;  // UIPickerView's
	case AK_KIND_PICKER: {
		// A pop-up button: its title (the chosen item, else its label),
		// 8pt characters, and the indicator.
		ak_span title = ak_fake_string(v, AK_PROP_PICKER_LABEL);
		ak_span items = ak_fake_string(v, AK_PROP_PICKER_ITEMS);
		ak_str_vec its = {0};
		ak_decode_strings((const uint8_t *)items.p, items.len, &its);
		int64_t i = ak_fake_int(v, AK_PROP_PICKER_SELECTED);
		if (i >= 0 && i < its.len) title = its.data[i];
		w = (float)runes(title) * 8 + 32;
		h = 34;
		ak_vec_free(&its);
		break;
	}
	case AK_KIND_STEPPER: w = 94, h = 32; break;      // UIStepper's
	case AK_KIND_COLOR_PICKER: w = h = 28; break;     // UIColorWell's
	case AK_KIND_SEGMENTED: {
		ak_span b = ak_fake_string(v, AK_PROP_SEGMENTED_SEGMENTS);
		uint32_t n = 0;
		if (b.len >= 4) memcpy(&n, b.p, 4);
		w = (float)(70 * (int64_t)n);
		h = 32;
		break;
	}
	case AK_KIND_DATE_PICKER:
		w = ak_fake_int(v, AK_PROP_DATE_PICKER_MODE) == AK_DATE_MODE_DATE_TIME ? 200 : 120;
		h = 34;
		break;
	case AK_KIND_PROGRESS:
		w = fin(avail_w) ? avail_w : 100;
		h = 4;
		break;
	case AK_KIND_PROGRESS_RING:
		// Square: a style that sizes one side sizes the other.
		w = h = 20;
		if (width == width) h = width;
		else if (height == height) w = height;
		break;
	case AK_KIND_RICH_TEXT: rich_size(v, width, avail_w, &w, &h); break;
	}
	AK_UNUSED(avail_h);
	if (width == width) w = width;
	if (height == height) h = height;
	*out_w = w;
	*out_h = h;
}

// Finding views.

ak_fake_view *ak_fake_root(const ak_fake *f) {
	ak_fake_view *v = f->root_id ? ak_fake_view_of(f, f->root_id) : NULL;
	return v && v->root ? v : NULL;
}

static ak_id root_id(const ak_fake *f) {
	ak_fake_view *r = ak_fake_root(f);
	return r ? r->id : 0;
}

ak_span ak_fake_label(const ak_fake_view *v) {
	switch (v->kind) {
	case AK_KIND_TEXT: return ak_fake_string(v, AK_PROP_TEXT_TEXT);
	case AK_KIND_BUTTON: return ak_fake_string(v, AK_PROP_BUTTON_TITLE);
	case AK_KIND_NATIVE: {
		// A native package's view: its component's name (its Type is
		// "<class>#<name>").
		ak_span t = ak_fake_string(v, AK_PROP_NATIVE_TYPE);
		for (uint32_t i = t.len; i > 0; i--)
			if (t.p[i - 1] == '#') return (ak_span){t.p + i, t.len - i};
		return t;
	}
	}
	return (ak_span){"", 0};
}

typedef struct {
	ak_view_kind kind;
	ak_span s;
} find_arg;

static bool find_pred(const ak_fake_view *v, const void *arg) {
	const find_arg *a = arg;
	if (v->kind != a->kind) return false;
	return ak_span_eq(ak_fake_label(v), a->s) || (v->kind == AK_KIND_TEXT_FIELD && ak_span_eq(ak_fake_string(v, AK_PROP_TEXT_FIELD_VALUE), a->s));
}

void ak_fake_find(const ak_fake *f, ak_view_kind k, ak_span s, ak_fake_view_vec *out) {
	find_arg a = {k, s};
	collect(f, find_pred, &a, out);
}

ak_fake_view *ak_fake_one(ak_fake *f, ak_view_kind k, const char *s) {
	ak_fake_view_vec vs = {0};
	ak_fake_find(f, k, ak_cstr(s), &vs);
	ak_fake_view *v = vs.len == 1 ? vs.data[0] : NULL;
	if (!v) {
		char kb[16];
		fail(f, "%u %s views with \"%s\"", vs.len, kind_str(k, kb), s);
	}
	ak_vec_free(&vs);
	return v;
}

static bool kind_pred(const ak_fake_view *v, const void *arg) { return v->kind == *(const ak_view_kind *)arg; }

void ak_fake_of_kind(const ak_fake *f, ak_view_kind k, ak_fake_view_vec *out) { collect(f, kind_pred, &k, out); }

static bool presented_pred(const ak_fake_view *v, const void *arg) {
	AK_UNUSED(arg);
	return v->presented;
}

static int by_present(const void *a, const void *b) {
	uint32_t x = (*(ak_fake_view *const *)a)->present_seq, y = (*(ak_fake_view *const *)b)->present_seq;
	return x < y ? -1 : x > y;
}

// ak_fake_presented is what's presented, in the order it last was: what
// came last is over the rest.
void ak_fake_presented(const ak_fake *f, ak_fake_view_vec *out) {
	if (f->npresented == 0) return;
	uint32_t start = out->len;
	collect(f, presented_pred, NULL, out);
	if (out->len > start + 1) qsort(out->data + start, out->len - start, sizeof(ak_fake_view *), by_present);
}

ak_fake_view *ak_fake_snackbar(const ak_fake *f) {
	ak_fake_view *top = NULL;
	for (uint32_t i = 0; i < f->slots_cap; i++) {
		ak_fake_view *v = f->slots[i];
		if (v && v->kind == AK_KIND_SNACKBAR && v->presented && (!top || v->id > top->id)) top = v;
	}
	return top;
}

ak_fake_view *ak_fake_presented_on(const ak_fake *f, const ak_fake_view *v) {
	ak_prop p = AK_PROP_SHEET_WINDOW;
	if (v->kind == AK_KIND_ALERT) p = AK_PROP_ALERT_WINDOW;
	else if (v->kind == AK_KIND_SNACKBAR) p = AK_PROP_SNACKBAR_WINDOW;
	const ak_value *w = ak_fake_prop_of(v, p);
	return w ? ak_fake_view_of(f, (ak_id)w->i) : NULL;
}

ak_fake_view *ak_fake_key_window(const ak_fake *f) {
	ak_fake_view *v = f->key ? ak_fake_view_of(f, f->key) : NULL;
	if (v && v->kind == AK_KIND_WINDOW) return v;
	return ak_fake_root(f);
}

void ak_fake_set_key_window(ak_fake *f, const ak_fake_view *v) { f->key = v->kind == AK_KIND_WINDOW ? v->id : 0; }

ak_fake_view *ak_fake_key_out(const ak_fake *f, const ak_fake_view *v) {
	if (v->parent) return v->parent;
	if (v->list) {
		ak_fake_view *l = ak_fake_view_of(f, v->list);
		if (l) return l;
	}
	switch (v->kind) {
	case AK_KIND_OVERLAY: {
		// On to the root it's over (a sheet's: on from there).
		ak_fake_view *r = ak_fake_view_of(f, (ak_id)ak_fake_int(v, AK_PROP_OVERLAY_ROOT));
		if (r && r != v) return r;
		return ak_fake_root(f) != v ? ak_fake_root(f) : NULL;
	}
	case AK_KIND_SHEET: {
		ak_fake_view *w = ak_fake_presented_on(f, v);
		if (w && w != v) return w;
		return ak_fake_root(f);
	}
	case AK_KIND_POPOVER: {
		ak_fake_view *a = ak_fake_view_of(f, (ak_id)ak_fake_int(v, AK_PROP_POPOVER_ANCHOR));
		if (a) {
			for (;;) {
				if (a->parent) {
					a = a->parent;
					continue;
				}
				ak_fake_view *l = a->list ? ak_fake_view_of(f, a->list) : NULL;
				if (!l) break;
				a = l;
			}
			if (a != v) return a;
		}
		return ak_fake_root(f);
	}
	}
	return NULL;
}

void ak_fake_text(const ak_fake_view *v, ak_buf *out) {
	// Depth first with an explicit stack of (view, next child).
	AK_VEC(const ak_fake_view *) stack = {0};
	size_t start = out->len;
	ak_vec_push(&stack, v);
	while (stack.len) {
		const ak_fake_view *x = ak_vec_pop(&stack);
		ak_span s = ak_fake_label(x);
		if (s.len) {
			if (out->len > start) ak_buf_u8(out, '|');
			ak_buf_put(out, s.p, s.len);
		}
		for (uint32_t i = x->children.len; i > 0; i--) ak_vec_push(&stack, x->children.data[i - 1]);
	}
	ak_vec_free(&stack);
}

void ak_fake_kind_name(ak_buf *out, ak_view_kind k) {
	char kb[16];
	const char *n = kind_str(k, kb);
	ak_buf_put(out, n, strlen(n));
}

static void tree(const ak_fake_view *v, ak_buf *out, int depth) {
	for (int i = 0; i < depth; i++) ak_buf_put(out, "  ", 2);
	ak_fake_kind_name(out, v->kind);
	ak_buf_u8(out, ' ');
	ak_fmt_float(out, v->frame[0], 32);
	ak_buf_u8(out, ',');
	ak_fmt_float(out, v->frame[1], 32);
	ak_buf_u8(out, ' ');
	ak_fmt_float(out, v->frame[2], 32);
	ak_buf_u8(out, 'x');
	ak_fmt_float(out, v->frame[3], 32);
	ak_span s = ak_fake_label(v);
	if (s.len) {
		ak_buf_u8(out, ' ');
		ak_fmt_quote(out, s);
	}
	ak_buf_u8(out, '\n');
	for (uint32_t i = 0; i < v->children.len; i++) tree(v->children.data[i], out, depth + 1);
}

void ak_fake_tree(const ak_fake_view *v, ak_buf *out) { tree(v, out, 0); }

// Payload props.

void ak_fake_action_items(const ak_fake_view *v, ak_prop p, ak_action_vec *out) {
	ak_span b = ak_fake_string(v, p);
	ak_decode_actions((const uint8_t *)b.p, b.len, out);
}

void ak_fake_alert_fields(const ak_fake_view *v, ak_alert_field_vec *out) {
	ak_span b = ak_fake_string(v, AK_PROP_ALERT_FIELDS);
	ak_decode_alert_fields((const uint8_t *)b.p, b.len, out);
}

void ak_fake_sidebar(const ak_fake_view *v, ak_sidebar_heading_vec *sections, ak_sidebar_item_vec *items) {
	ak_span b = ak_fake_string(v, AK_PROP_SPLIT_ITEMS);
	ak_decode_sidebar((const uint8_t *)b.p, b.len, sections, items);
}

void ak_fake_strings(const ak_fake_view *v, ak_prop p, ak_str_vec *out) {
	ak_span b = ak_fake_string(v, p);
	ak_decode_strings((const uint8_t *)b.p, b.len, out);
}

int64_t ak_fake_action_named(const ak_fake_view *v, ak_prop p, const char *title) {
	ak_action_vec items = {0};
	ak_fake_action_items(v, p, &items);
	int64_t i = ak_action_named(items.data, items.len, ak_cstr(title));
	ak_vec_free(&items);
	return i;
}

// Events.

static bool listening(ak_fake *f, const ak_fake_view *v, ak_event_kind e) {
	if (ak_fake_listens(v, e)) return true;
	char kb[16];
	const char *en = ak_event_name(e);
	fail(f, "view %u (%s) doesn't listen for %s", v->id, kind_str(v->kind, kb), en ? en : "?");
	return false;
}

void ak_fake_dispatch(ak_fake *f, const ak_event *e) {
	// An event sent while one is handled gets its own buffer.
	ak_buf b = f->ev;
	f->ev = (ak_buf){0};
	ak_buf_reset(&b);
	ak_event_append(&b, e);
	send(f, b.data, b.len);
	if (!f->ev.data) f->ev = b;
	else ak_buf_free(&b);
}

static void emit(ak_fake *f, ak_fake_view *v, ak_event_kind k) {
	if (listening(f, v, k)) ak_fake_dispatch(f, &(ak_event){.id = v->id, .kind = k});
}

static void emit_int(ak_fake *f, ak_fake_view *v, ak_event_kind k, int64_t i) {
	if (listening(f, v, k)) ak_fake_dispatch(f, &(ak_event){.id = v->id, .kind = k, .i = i});
}

void ak_fake_tap(ak_fake *f, ak_fake_view *v) { emit(f, v, AK_EVENT_PRESS); }
void ak_fake_long_press(ak_fake *f, ak_fake_view *v) { emit(f, v, AK_EVENT_LONG_PRESS); }

void ak_fake_type(ak_fake *f, ak_fake_view *v, const char *s) {
	ak_span str = ak_cstr(s);
	set_prop(v, AK_PROP_TEXT_FIELD_VALUE, &(ak_value){.type = AK_T_STRING, .s = str});
	if (ak_fake_listens(v, AK_EVENT_TEXT_CHANGED)) ak_fake_dispatch(f, &(ak_event){.id = v->id, .kind = AK_EVENT_TEXT_CHANGED, .s = str});
}

void ak_fake_submit(ak_fake *f, ak_fake_view *v) { emit(f, v, AK_EVENT_SUBMIT); }

void ak_fake_flip(ak_fake *f, ak_fake_view *v) {
	bool on = !ak_fake_bool(v, AK_PROP_TOGGLE_ON);
	set_int(v, AK_PROP_TOGGLE_ON, AK_T_BOOL, on);
	if (listening(f, v, AK_EVENT_TOGGLED)) ak_fake_dispatch(f, &(ak_event){.id = v->id, .kind = AK_EVENT_TOGGLED, .b = on});
}

void ak_fake_pick_color(ak_fake *f, ak_fake_view *v, ak_color c) {
	set_int(v, AK_PROP_COLOR_PICKER_VALUE, AK_T_COLOR, (int64_t)c);
	emit_int(f, v, AK_EVENT_COLOR_CHANGED, (int64_t)c);
}

void ak_fake_pick_date(ak_fake *f, ak_fake_view *v, int64_t ms) {
	set_int(v, AK_PROP_DATE_PICKER_VALUE, AK_T_INT, ms);
	if (listening(f, v, AK_EVENT_VALUE_CHANGED)) ak_fake_dispatch(f, &(ak_event){.id = v->id, .kind = AK_EVENT_VALUE_CHANGED, .f = (double)ms});
}

void ak_fake_slide(ak_fake *f, ak_fake_view *v, double value) {
	set_prop(v, AK_PROP_SLIDER_VALUE, &(ak_value){.type = AK_T_FLOAT, .f = value});
	if (listening(f, v, AK_EVENT_VALUE_CHANGED)) ak_fake_dispatch(f, &(ak_event){.id = v->id, .kind = AK_EVENT_VALUE_CHANGED, .f = value});
}

void ak_fake_select(ak_fake *f, ak_fake_view *v, int64_t index) {
	ak_prop p = AK_PROP_TABS_SELECTED;
	switch (v->kind) {
	case AK_KIND_SEGMENTED: p = AK_PROP_SEGMENTED_SELECTED; break;
	case AK_KIND_SPLIT: p = AK_PROP_SPLIT_SELECTED; break;
	case AK_KIND_PAGE_CONTROL: p = AK_PROP_PAGE_CONTROL_PAGE; break;
	case AK_KIND_WHEEL: p = AK_PROP_WHEEL_SELECTED; break;
	case AK_KIND_PICKER: p = AK_PROP_PICKER_SELECTED; break;
	}
	set_int(v, p, AK_T_INT, index);
	emit_int(f, v, AK_EVENT_SELECTED, index);
}

void ak_fake_scroll(ak_fake *f, ak_fake_view *v, float x, float y) {
	if (listening(f, v, AK_EVENT_SCROLLED)) ak_fake_dispatch(f, &(ak_event){.id = v->id, .kind = AK_EVENT_SCROLLED, .x = x, .y = y});
}

void ak_fake_pull_to_refresh(ak_fake *f, ak_fake_view *v) { emit(f, v, AK_EVENT_REFRESH); }

void ak_fake_dismiss(ak_fake *f, ak_fake_view *v) {
	if (ak_fake_bool(v, AK_PROP_INTERCEPT_BACK)) emit(f, v, AK_EVENT_BACK);
	else emit(f, v, AK_EVENT_DISMISSED);
}

void ak_fake_back(ak_fake *f, ak_fake_view *stack) {
	if (stack->children.len < 2) { fail(f, "Back: nothing to pop"); return; }
	ak_fake_view *top = stack->children.data[stack->children.len - 1];
	if (ak_fake_bool(top, AK_PROP_INTERCEPT_BACK)) { emit(f, top, AK_EVENT_BACK); return; }
	ak_id id = top->id;
	remove_child(stack, top);
	emit_int(f, stack, AK_EVENT_POPPED, id);
}

bool ak_fake_back_from_root(ak_fake *f) {
	ak_fake_view *r = ak_fake_root(f);
	if (!r || !ak_fake_bool(r, AK_PROP_INTERCEPT_BACK)) return false;
	emit(f, r, AK_EVENT_BACK);
	return true;
}

void ak_fake_choose(ak_fake *f, ak_fake_view *v, int64_t index) { emit_int(f, v, AK_EVENT_ACTION, index); }

bool ak_fake_press_toolbar(ak_fake *f, ak_fake_view *screen, const char *title) {
	int64_t i = ak_fake_action_named(screen, AK_PROP_SCREEN_TOOLBAR, title);
	if (i < 0) return false;
	ak_fake_choose(f, screen, AK_SCREEN_TOOLBAR_ACTIONS + i);
	return true;
}

bool ak_fake_press_primary(ak_fake *f, ak_fake_view *screen) {
	ak_action_vec items = {0};
	ak_fake_action_items(screen, AK_PROP_SCREEN_PRIMARY, &items);
	bool has = items.len > 0;
	ak_vec_free(&items);
	if (!has) return false;
	ak_fake_choose(f, screen, AK_SCREEN_PRIMARY_ACTION);
	return true;
}

bool ak_fake_press_primary_item(ak_fake *f, ak_fake_view *screen, const char *title) {
	int64_t i = ak_fake_action_named(screen, AK_PROP_SCREEN_PRIMARY, title);
	if (i <= 0) return false;
	ak_fake_choose(f, screen, AK_SCREEN_PRIMARY_ACTION + i);
	return true;
}

void ak_fake_type_in_alert(ak_fake *f, ak_fake_view *alert, uint32_t field, const char *s) {
	ak_alert_field_vec fs = {0};
	ak_fake_alert_fields(alert, &fs);
	if (field >= fs.len) {
		ak_vec_free(&fs);
		{ fail(f, "alert %u has no field %u", alert->id, field); return; }
	}
	ak_span text = ak_cstr(s);
	fs.data[field].text = text;
	ak_buf b = {0};
	ak_append_alert_fields(&b, fs.data, fs.len);
	ak_vec_free(&fs);
	set_prop(alert, AK_PROP_ALERT_FIELDS, &(ak_value){.type = AK_T_BYTES, .s = {(const char *)b.data, (uint32_t)b.len}});
	if (listening(f, alert, AK_EVENT_FIELD_CHANGED)) {
		ak_buf_reset(&b);
		ak_append_field_changed(&b, field, text);
		ak_fake_dispatch(f, &(ak_event){.id = alert->id, .kind = AK_EVENT_FIELD_CHANGED, .s = {(const char *)b.data, (uint32_t)b.len}});
	}
	ak_buf_free(&b);
}

bool ak_fake_return_in_alert(ak_fake *f, ak_fake_view *alert) {
	ak_alert_field_vec fs = {0};
	ak_fake_alert_fields(alert, &fs);
	uint32_t nf = fs.len;
	ak_vec_free(&fs);
	if (!nf) {
		fail(f, "alert %u has no fields", alert->id);
		return false;
	}
	ak_action_vec items = {0};
	ak_fake_action_items(alert, AK_PROP_ALERT_BUTTONS, &items);
	int64_t i = ak_default_alert_action(items.data, items.len);
	bool ok = i >= 0 && !(items.data[i].flags & AK_MENU_DISABLED);
	ak_vec_free(&items);
	if (ok) ak_fake_choose(f, alert, i);
	return ok;
}

void ak_fake_press_snackbar_action(ak_fake *f, ak_fake_view *v) {
	if (!ak_fake_string(v, AK_PROP_SNACKBAR_ACTION).len) { fail(f, "snackbar %u has no action", v->id); return; }
	ak_fake_choose(f, v, 0);
}

void ak_fake_time_out(ak_fake *f, ak_fake_view *snackbar) {
	if (snackbar->kind != AK_KIND_SNACKBAR || !snackbar->presented) {
		char kb[16];
		{ fail(f, "TimeOut on %s %u, not a snackbar showing", kind_str(snackbar->kind, kb), snackbar->id); return; }
	}
	ak_fake_dismiss(f, snackbar);
}

bool ak_fake_key(ak_fake *f, ak_fake_view *focused, const char *name, uint8_t mods) {
	f->keyboard = true;
	ak_fake_view *had = focused ? focused : ak_fake_focused(f);
	if (!focused) focused = had ? had : ak_fake_key_window(f);
	ak_key_vec keys = {0};
	for (ak_fake_view *v = focused; v; v = ak_fake_key_out(f, v)) {
		const ak_value *b = ak_fake_prop_of(v, AK_PROP_KEYS);
		if (!b) continue;
		keys.len = 0;
		ak_decode_keys((const uint8_t *)b->s.p, b->s.len, &keys);
		int i = ak_match_key(keys.data, keys.len, ak_cstr(name), mods);
		if (i >= 0) {
			ak_vec_free(&keys);
			emit_int(f, v, AK_EVENT_KEY, i);
			return true;
		}
	}
	ak_vec_free(&keys);
	// What the platform does with a key no view took (not the app's keys
	// taking it: false).
	ak_span k = ak_cstr(name);
	if (ak_span_eq(k, ak_cstr("tab")) && !(mods & ~2)) {
		ak_fake_tab(f, (mods & 2) != 0);
		return false;
	}
	if (!had || mods) return false;
	if ((ak_span_eq(k, ak_cstr("enter")) || ak_span_eq(k, ak_cstr("space"))) && had->kind != AK_KIND_TEXT_FIELD && listening(f, had, AK_EVENT_PRESS)) {
		emit(f, had, AK_EVENT_PRESS);
		return false;
	}
	ak_span text = ak_span_eq(k, ak_cstr("space")) ? ak_cstr(" ") : k;
	if (runes(text) == 1 && listening(f, had, AK_EVENT_KEY_TEXT)) ak_fake_dispatch(f, &(ak_event){.id = had->id, .kind = AK_EVENT_KEY_TEXT, .s = text});
	return false;
}

// ---- The keyboard's focus ----

bool ak_fake_takes_focus(const ak_fake_view *v) {
	if (!v || ak_fake_bool(v, AK_PROP_DISABLED) || ak_fake_bool(v, AK_PROP_HIDDEN)) return false;
	switch (v->kind) {
	case AK_KIND_TEXT_FIELD:
	case AK_KIND_BUTTON:
	case AK_KIND_TOGGLE:
	case AK_KIND_SLIDER:
	case AK_KIND_STEPPER:
	case AK_KIND_SEGMENTED:
	case AK_KIND_PICKER:
	case AK_KIND_WHEEL:
	case AK_KIND_COLOR_PICKER:
	case AK_KIND_DATE_PICKER:
		return true;
	}
	return ak_fake_bool(v, AK_PROP_FOCUSABLE) || ak_fake_listens(v, AK_EVENT_PRESS) || ak_fake_bool(v, AK_PROP_A11Y_ADJUSTABLE);
}

ak_fake_view *ak_fake_focused(const ak_fake *f) { return f->focus ? ak_fake_view_of(f, f->focus) : NULL; }

// focus_send sends e now, or posted (while a batch is applied).
static void focus_send(ak_fake *f, const ak_event *e, bool posted) {
	if (posted) ak_fake_post_event(f, e);
	else ak_fake_dispatch(f, e);
}

static void focus_to(ak_fake *f, ak_fake_view *v, bool keyboard, bool posted) {
	ak_fake_view *was = ak_fake_focused(f);
	if (was == v) return;
	f->focus = v ? v->id : 0;
	if (was) {
		if (was->kind == AK_KIND_TEXT_FIELD) {
			if (ak_fake_listens(was, AK_EVENT_BLUR)) focus_send(f, &(ak_event){.id = was->id, .kind = AK_EVENT_BLUR}, posted);
		} else {
			focus_send(f, &(ak_event){.id = was->id, .kind = AK_EVENT_FOCUS_CHANGE, .i = 0}, posted);
		}
	}
	if (v) {
		if (v->kind == AK_KIND_TEXT_FIELD) {
			if (ak_fake_listens(v, AK_EVENT_FOCUS)) focus_send(f, &(ak_event){.id = v->id, .kind = AK_EVENT_FOCUS}, posted);
		} else {
			focus_send(f, &(ak_event){.id = v->id, .kind = AK_EVENT_FOCUS_CHANGE, .i = keyboard ? 2 : 1}, posted);
		}
	}
}

void ak_fake_focus(ak_fake *f, ak_fake_view *v, bool keyboard) {
	f->keyboard = keyboard;
	focus_to(f, v, keyboard, false);
}

// tab_order adds the views under v that take the focus, in the tree's
// order (what shows: not hidden; a stack's top screen).
static void tab_order(ak_fake_view *v, ak_fake_view_vec *out) {
	if (ak_fake_bool(v, AK_PROP_HIDDEN)) return;
	if (ak_fake_takes_focus(v)) ak_vec_push(out, v);
	if (v->kind == AK_KIND_STACK) {
		if (v->children.len) tab_order(v->children.data[v->children.len - 1], out);
		return;
	}
	for (uint32_t i = 0; i < v->children.len; i++) tab_order(v->children.data[i], out);
}

void ak_fake_tab(ak_fake *f, bool backward) {
	ak_fake_view *cur = ak_fake_focused(f);
	// Within the focus trap the focus is in (the innermost), else the
	// root and what's presented over it.
	ak_fake_view *trap = NULL;
	for (ak_fake_view *v = cur; v && !trap; v = v->parent)
		if (ak_fake_bool(v, AK_PROP_FOCUS_TRAP)) trap = v;
	ak_fake_view_vec order = {0};
	if (trap) {
		tab_order(trap, &order);
	} else {
		ak_fake_view *r = ak_fake_root(f);
		if (r) tab_order(r, &order);
		ak_fake_view_vec ps = {0};
		ak_fake_presented(f, &ps);
		for (uint32_t i = 0; i < ps.len; i++) tab_order(ps.data[i], &order);
		ak_vec_free(&ps);
	}
	if (order.len) {
		int64_t at = -1;
		for (uint32_t i = 0; i < order.len; i++)
			if (order.data[i] == cur) at = i;
		int64_t n = order.len;
		int64_t next = at < 0 ? (backward ? n - 1 : 0) : ((at + (backward ? -1 : 1)) % n + n) % n;
		ak_fake_focus(f, order.data[next], true);
	}
	ak_vec_free(&order);
}

void ak_fake_abs_frame(const ak_fake *f, const ak_fake_view *v, float out[4]) {
	AK_UNUSED(f);
	out[0] = out[1] = 0;
	out[2] = v->frame[2], out[3] = v->frame[3];
	for (const ak_fake_view *x = v; x; x = x->parent) {
		if (!x->parent) break;  // a root's own frame is where it is, not in it
		out[0] += x->frame[0], out[1] += x->frame[1];
	}
}

// report_anchors reports where each shown Overlay's anchor is, when
// that changed, as a host does after it lays its views out.
static void report_anchors(ak_fake *f) {
	for (uint32_t i = 0; i < f->slots_cap; i++) {
		ak_fake_view *o = f->slots[i];
		if (!o || o->kind != AK_KIND_OVERLAY || !o->presented || !ak_fake_listens(o, AK_EVENT_ANCHOR_FRAME)) continue;
		ak_fake_view *a = ak_fake_view_of(f, (ak_id)ak_fake_int(o, AK_PROP_OVERLAY_ANCHOR));
		if (!a) continue;
		float fr[5];
		ak_fake_abs_frame(f, a, fr);
		bool vis = true;
		for (ak_fake_view *x = a; x; x = x->parent)
			if (ak_fake_bool(x, AK_PROP_HIDDEN)) vis = false;
		fr[4] = vis ? 1 : 0;
		if (o->anchor_known && memcmp(fr, o->anchor_sent, sizeof fr) == 0) continue;
		o->anchor_known = true;
		memcpy(o->anchor_sent, fr, sizeof fr);
		uint8_t b[17];
		memcpy(b, fr, 16);
		b[16] = vis ? 1 : 0;
		ak_fake_post_event(f, &(ak_event){.id = o->id, .kind = AK_EVENT_ANCHOR_FRAME, .s = {(const char *)b, 17}});
	}
}

void ak_fake_outside_press(ak_fake *f, ak_fake_view *overlay, double x, double y) {
	if (listening(f, overlay, AK_EVENT_OUTSIDE_PRESS)) ak_fake_dispatch(f, &(ak_event){.id = overlay->id, .kind = AK_EVENT_OUTSIDE_PRESS, .x = x, .y = y});
}

void ak_fake_set_environment(ak_fake *f, const ak_environment *e) {
	f->env = *e;
	ak_fake_dispatch(f, &(ak_event){.id = root_id(f), .kind = AK_EVENT_ENVIRONMENT, .env = *e});
}

void ak_fake_report_sheet(ak_fake *f, ak_fake_view *sheet) {
	if (sheet->kind != AK_KIND_SHEET || !sheet->presented) {
		char kb[16];
		{ fail(f, "ReportSheet on %u (%s), not a presented sheet", sheet->id, kind_str(sheet->kind, kb)); return; }
	}
	ak_environment e = f->env;
	if (!ak_fake_bool(sheet, AK_PROP_SHEET_FULL_SCREEN)) {
		e.height -= e.safe_top + 10;
		e.safe_top = 0;
	}
	ak_fake_dispatch(f, &(ak_event){.id = sheet->id, .kind = AK_EVENT_ENVIRONMENT, .env = e});
}

void ak_fake_open_url(ak_fake *f, const char *url) {
	ak_fake_dispatch(f, &(ak_event){.id = root_id(f), .kind = AK_EVENT_INCOMING_URL, .s = ak_cstr(url)});
}

void ak_fake_link(ak_fake *f, ak_fake_view *v, ak_span url) {
	if (!listening(f, v, AK_EVENT_LINK)) return;
	ak_fake_dispatch(f, &(ak_event){.id = v->id, .kind = AK_EVENT_LINK, .s = url});
}

void ak_fake_lifecycle(ak_fake *f, int64_t state) {
	ak_fake_dispatch(f, &(ak_event){.kind = AK_EVENT_LIFECYCLE, .i = state});
}

bool ak_fake_dev_error(const ak_fake *f, ak_dev_error *e) {
	if (!f->dev_error_on) return false;
	return ak_decode_dev_error(f->dev_error.data, f->dev_error.len, e);
}

bool ak_fake_dev_error_press(ak_fake *f, int64_t action) {
	ak_dev_error e;
	if (!ak_fake_dev_error(f, &e) || (action == AK_DEV_ERROR_ACTION_RELOAD && !(e.flags & AK_DEV_ERROR_RELOAD))) return false;
	ak_fake_dispatch(f, &(ak_event){.id = f->dev_error_on, .kind = AK_EVENT_DEV_ERROR_ACTION, .i = action});
	return true;
}

void ak_fake_emit_native(ak_fake *f, ak_fake_view *v, const char *name, const ak_fake_native *values, uint32_t n) {
	if (!listening(f, v, AK_EVENT_NATIVE)) return;
	ak_buf b = {0};
	ak_buf_wstr(&b, ak_cstr(name));
	ak_buf_u32(&b, n);
	for (uint32_t i = 0; i < n; i++) {
		const ak_fake_native *x = &values[i];
		ak_buf_wstr(&b, x->name);
		ak_buf_u8(&b, x->type);
		switch (x->type) {
		case 1: ak_buf_u8(&b, x->i ? 1 : 0); break;
		case 2: ak_buf_u64(&b, (uint64_t)x->i); break;
		case 3: ak_buf_f64(&b, x->f); break;
		case 4:
		case 5: ak_buf_wstr(&b, x->s); break;
		default:
			ak_buf_free(&b);
			{ fail(f, "EmitNative: a value of an unknown type %u", x->type); return; }
		}
	}
	ak_fake_dispatch(f, &(ak_event){.id = v->id, .kind = AK_EVENT_NATIVE, .s = {(const char *)b.data, (uint32_t)b.len}});
	ak_buf_free(&b);
}

// Lists.

ak_fake_view *ak_fake_new_cell(ak_fake *f) {
	ak_fake_view *v = ak_alloc(sizeof *v);
	v->id = AK_HOST_ID_BIT | ++f->next_cell;
	v->kind = AK_FAKE_KIND_CELL;
	slots_put(f, v);
	return v;
}

void ak_fake_bind(ak_fake *f, ak_fake_view *list, int64_t index, ak_fake_view *cell) {
	cell->list = list->id;
	if (!f->core.list_bind) { fail(f, "not connected: no core to bind cells"); return; }
	f->core.list_bind(f->core.ctx, list->id, index, cell->id);
}

void ak_fake_unbind(ak_fake *f, ak_fake_view *list, ak_fake_view *cell) {
	cell->list = 0;
	if (!f->core.list_unbind) { fail(f, "not connected: no core to unbind cells"); return; }
	f->core.list_unbind(f->core.ctx, list->id, cell->id);
}

int64_t ak_fake_list_count(ak_fake *f, ak_fake_view *list) {
	if (!f->core.list_count) {
		fail(f, "not connected: no core to count a list");
		return 0;
	}
	return f->core.list_count(f->core.ctx, list->id);
}

void ak_fake_select_row(ak_fake *f, ak_fake_view *cell, bool selected) {
	if (!listening(f, cell, AK_EVENT_ROW_SELECTED)) return;
	set_int(cell, AK_PROP_SELECTED, AK_T_BOOL, selected);
	ak_fake_dispatch(f, &(ak_event){.id = cell->id, .kind = AK_EVENT_ROW_SELECTED, .b = selected});
}
