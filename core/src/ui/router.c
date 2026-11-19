// router.c: routing: every screen has an
// address, an href (a path and a query, as on the web). A Router's
// Routes map path patterns to screens, under navigators that show them
// (a Stack, Tabs, a slot) and sheets presented over them.
//
// Its navigation state is a tree: each navigator's screens (a stack's,
// bottom first) or chosen one (a tab's, a slot's), and the sheets over
// it. Navigating is done on a description of the state (ndesc), made
// from the live one (inst, entry); the live state is then reconciled to
// it, as a view diff: entries kept by key, the others popped, pushed,
// presented or dismissed. Whatever changed the state (navigate, the
// user's back, a tab chosen, push and pop in a routed stack), the new
// state's address goes to the host (the Location command) with how its
// history moved: a push, a back, or a replace.
//
// In Asuka (lang/LOWERING.md's "Routes"): Router { Route("/inbox",
// layout: .stack) { Route("") { (p) in Inbox() } ... } }, a Route's
// content a builder of its Params (an accessor: a kept screen follows its
// query); a screen declared with its path (screen Thread(id: int) at
// "/inbox/:id") is such a Route whose builder parses its parameters.
//
// Not ported yet: a split view's routes (LayoutSplit shows its chosen
// destination as a slot does), the state saved in the history (a
// browser's back rebuilds from the href).
#include "ui/ui_int.h"

enum { L_GROUP, L_STACK, L_TABS, L_SPLIT, L_SLOT };
enum { MODE_NAVIGATE, MODE_PUSH, MODE_REPLACE };
enum { SEG_STATIC, SEG_PARAM, SEG_REST };

// ---- Strings, hrefs ----

static ak_str *sb_str(ak_buf *b) {
	ak_str *s = ak_str_new((const char *)b->data, b->len);
	ak_buf_free(b);
	return s;
}

static bool unreserved(uint8_t c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' ||
	       c == '$' || c == '&' || c == '+' || c == ':' || c == '=' || c == '@';
}

// path_escape escapes a path segment, as Go's url.PathEscape.
static void path_escape(ak_buf *b, const char *s, size_t n) {
	static const char hex[] = "0123456789ABCDEF";
	for (size_t i = 0; i < n; i++) {
		uint8_t c = (uint8_t)s[i];
		if (unreserved(c)) {
			ak_buf_u8(b, c);
		} else {
			ak_buf_u8(b, '%');
			ak_buf_u8(b, (uint8_t)hex[c >> 4]);
			ak_buf_u8(b, (uint8_t)hex[c & 15]);
		}
	}
}

static int unhex(uint8_t c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// unescape decodes %XX (and, in a query, + as a space); an invalid
// escape stays as it is.
static ak_str *unescape(const char *s, size_t n, bool query) {
	ak_buf b = {0};
	for (size_t i = 0; i < n; i++) {
		uint8_t c = (uint8_t)s[i];
		if (c == '%' && i + 2 < n && unhex((uint8_t)s[i + 1]) >= 0 && unhex((uint8_t)s[i + 2]) >= 0) {
			ak_buf_u8(&b, (uint8_t)(unhex((uint8_t)s[i + 1]) << 4 | unhex((uint8_t)s[i + 2])));
			i += 2;
		} else if (c == '+' && query) {
			ak_buf_u8(&b, ' ');
		} else {
			ak_buf_u8(&b, c);
		}
	}
	return sb_str(&b);
}

// Parameters: name, value pairs.
typedef struct {
	ak_str *k, *v;
} kv;
typedef AK_VEC(kv) kvs;

static void kvs_free(kvs *p) {
	for (uint32_t i = 0; i < p->len; i++) ak_release(p->data[i].k), ak_release(p->data[i].v);
	ak_vec_free(p);
}

static ak_str *kvs_get(const kvs *p, const ak_str *k) {
	for (uint32_t i = 0; i < p->len; i++)
		if (ak_str_eq(p->data[i].k, k)) return p->data[i].v;
	return NULL;
}

static void kvs_set(kvs *p, ak_str *k, ak_str *v) {  // takes both
	for (uint32_t i = 0; i < p->len; i++) {
		if (ak_str_eq(p->data[i].k, k)) {
			ak_release(k);
			ak_release(p->data[i].v);
			p->data[i].v = v;
			return;
		}
	}
	ak_vec_push(p, ((kv){k, v}));
}

static void kvs_copy(kvs *dst, const kvs *src) {
	for (uint32_t i = 0; i < src->len; i++) ak_vec_push(dst, ((kv){ak_retain(src->data[i].k), ak_retain(src->data[i].v)}));
}

static bool kvs_eq(const kvs *a, const kvs *b) {
	if (a->len != b->len) return false;
	for (uint32_t i = 0; i < a->len; i++) {
		ak_str *v = kvs_get(b, a->data[i].k);
		if (!v || !ak_str_eq(v, a->data[i].v)) return false;
	}
	return true;
}

static bool str_eq(const ak_str *a, const ak_str *b) {
	if (!a || !b) return (!a || !a->len) && (!b || !b->len);
	return ak_str_eq(a, b);
}

typedef struct {
	uint8_t kind;
	ak_str *text;  // the static text, or the parameter's name
} seg;
typedef AK_VEC(seg) segs;

static void segs_free(segs *s) {
	for (uint32_t i = 0; i < s->len; i++) ak_release(s->data[i].text);
	ak_vec_free(s);
}

// split_href splits an href into its path and its query (no fragment).
static void split_href(const char *h, size_t n, ak_span *path, ak_span *query) {
	size_t end = n;
	for (size_t i = 0; i < n; i++)
		if (h[i] == '#') {
			end = i;
			break;
		}
	size_t q = end;
	for (size_t i = 0; i < end; i++)
		if (h[i] == '?') {
			q = i;
			break;
		}
	*path = ak_strn(h, q);
	*query = q < end ? ak_strn(h + q + 1, end - q - 1) : ak_strn("", 0);
}

// parse_pattern appends a pattern's segments: "chat" (static), ":id" (a
// parameter), "*path" (a catch-all); empty ones are dropped.
static void parse_pattern(segs *out, const char *p, size_t n) {
	ak_span path, q;
	split_href(p, n, &path, &q);
	size_t i = 0;
	while (i <= path.len) {
		size_t j = i;
		while (j < path.len && path.p[j] != '/') j++;
		const char *part = path.p + i;
		size_t len = j - i;
		if (len > 0) {
			if (part[0] == ':' && len > 1) ak_vec_push(out, ((seg){SEG_PARAM, ak_str_new(part + 1, len - 1)}));
			else if (part[0] == '*') ak_vec_push(out, ((seg){SEG_REST, len > 1 ? ak_str_new(part + 1, len - 1) : ak_str_c("rest")}));
			else ak_vec_push(out, ((seg){SEG_STATIC, ak_str_new(part, len)}));
		}
		i = j + 1;
	}
}

// split_path appends a path's segments, unescaped.
typedef AK_VEC(ak_str *) strs;

static void strs_free(strs *s) {
	for (uint32_t i = 0; i < s->len; i++) ak_release(s->data[i]);
	ak_vec_free(s);
}

static void split_path(strs *out, ak_span path) {
	size_t i = 0;
	while (i <= path.len) {
		size_t j = i;
		while (j < path.len && path.p[j] != '/') j++;
		if (j > i) ak_vec_push(out, unescape(path.p + i, j - i, false));
		i = j + 1;
	}
}

static bool match(const segs *ss, const strs *path, kvs *params) {
	uint32_t i = 0;
	for (uint32_t k = 0; k < ss->len; k++) {
		const seg *s = &ss->data[k];
		switch (s->kind) {
		case SEG_STATIC:
			if (i >= path->len || !ak_str_eq(path->data[i], s->text)) goto no;
			i++;
			break;
		case SEG_PARAM:
			if (i >= path->len) goto no;
			kvs_set(params, ak_retain(s->text), ak_retain(path->data[i]));
			i++;
			break;
		case SEG_REST: {
			ak_buf b = {0};
			for (uint32_t j = i; j < path->len; j++) {
				if (j > i) ak_buf_u8(&b, '/');
				ak_buf_put(&b, path->data[j]->data, path->data[j]->len);
			}
			kvs_set(params, ak_retain(s->text), sb_str(&b));
			i = path->len;
			break;
		}
		}
	}
	if (i == path->len) return true;
no:
	kvs_free(params);
	return false;
}

static void escape_rest(ak_buf *b, const ak_str *v) {
	// Segment by segment, keeping its slashes.
	uint32_t i = 0, n = v->len;
	while (i < n && v->data[i] == '/') i++;
	while (n > i && v->data[n - 1] == '/') n--;
	uint32_t start = i;
	for (; i <= n; i++) {
		if (i == n || v->data[i] == '/') {
			if (start > 0 && start <= n && i > 0 && b->len && v->data[start - 1] == '/') ak_buf_u8(b, '/');
			path_escape(b, v->data + start, i - start);
			start = i + 1;
		}
	}
}

// fill_pattern is segs' path with params filled in (NULL: one is
// missing).
static ak_str *fill_pattern(const segs *ss, const kvs *params) {
	ak_buf b = {0};
	for (uint32_t k = 0; k < ss->len; k++) {
		const seg *s = &ss->data[k];
		ak_str *v = s->kind == SEG_STATIC ? s->text : kvs_get(params, s->text);
		if (s->kind != SEG_STATIC && (!v || (s->kind == SEG_PARAM && !v->len))) {
			ak_buf_free(&b);
			return NULL;
		}
		if (s->kind == SEG_REST) {
			if (!v->len) continue;
			ak_buf_u8(&b, '/');
			escape_rest(&b, v);
			continue;
		}
		ak_buf_u8(&b, '/');
		path_escape(&b, v->data, v->len);
	}
	if (b.len == 0) ak_buf_u8(&b, '/');
	return sb_str(&b);
}

static ak_str *join_href(ak_str *path, const ak_str *query) {  // takes path
	if (!query || !query->len) return path;
	ak_buf b = {0};
	ak_buf_put(&b, path->data, path->len);
	ak_buf_u8(&b, '?');
	ak_buf_put(&b, query->data, query->len);
	ak_release(path);
	return sb_str(&b);
}

// ak_ui_href_of fills a route's pattern with values, in order: each
// parameter takes one, path-escaped, and a catch-all the rest of the path
// (its segments escaped one by one). It throws when the number of values
// isn't the pattern's number of parameters.
ak_str *ak_ui_href_of(ak_str *pattern, ak_arr *values) {
	segs ss = {0};
	parse_pattern(&ss, pattern->data, pattern->len);
	ak_buf b = {0};
	uint32_t n = 0, nv = values ? values->len : 0, np = 0;
	for (uint32_t k = 0; k < ss.len; k++)
		if (ss.data[k].kind != SEG_STATIC) np++;
	if (np != nv) {
		ak_throw_msg("hrefOf(\"%.*s\"): %u values for %u parameters", (int)pattern->len, pattern->data, nv, np);
		segs_free(&ss);
		return NULL;
	}
	for (uint32_t k = 0; k < ss.len; k++) {
		seg *s = &ss.data[k];
		ak_buf_u8(&b, '/');
		if (s->kind == SEG_STATIC) {
			path_escape(&b, s->text->data, s->text->len);
			continue;
		}
		ak_str *v = ak_arr_vals(values)[n++].p;
		if (!v) continue;
		if (s->kind == SEG_PARAM) path_escape(&b, v->data, v->len);
		else escape_rest(&b, v);
	}
	while (b.len > 1 && b.data[b.len - 1] == '/') b.len--;
	if (b.len == 0) ak_buf_u8(&b, '/');
	segs_free(&ss);
	return sb_str(&b);
}

// rank_less reports whether pattern a is more specific than b, for paths
// both match, as expo-router ranks its routes: a static segment before a
// parameter before a catch-all; where one ends, before the other's
// catch-all.
static bool rank_less(const segs *a, const segs *b) {
	for (uint32_t i = 0; i < a->len && i < b->len; i++)
		if (a->data[i].kind != b->data[i].kind) return a->data[i].kind < b->data[i].kind;
	if (a->len < b->len) return b->data[a->len].kind == SEG_REST;
	if (a->len > b->len) return a->data[b->len].kind != SEG_REST;
	return false;
}

static bool segs_eq(const segs *a, const segs *b) {
	if (a->len != b->len) return false;
	for (uint32_t i = 0; i < a->len; i++)
		if (a->data[i].kind != b->data[i].kind || !ak_str_eq(a->data[i].text, b->data[i].text)) return false;
	return true;
}

// is_prefix reports whether a is a proper prefix of b.
static bool is_prefix(const segs *a, const segs *b) {
	if (a->len == 0 || a->len >= b->len) return false;
	for (uint32_t i = 0; i < a->len; i++)
		if (a->data[i].kind != b->data[i].kind || !ak_str_eq(a->data[i].text, b->data[i].text)) return false;
	return true;
}

// to_href is the href a URL the app was opened with stands for: a rlink
// of its scheme (myapp://chat/7: /chat/7, the host the first segment), a
// universal rlink's path and query, or a path itself.
static ak_str *to_href(ak_span raw) {
	if (raw.len && raw.p[0] == '/') return ak_str_new(raw.p, raw.len);
	size_t c = 0;
	while (c < raw.len && raw.p[c] != ':') c++;
	if (c == 0 || c >= raw.len) return NULL;
	ak_span scheme = ak_strn(raw.p, c);
	if (ak_span_eqc(scheme, "file")) return NULL;
	bool web = ak_span_eqc(scheme, "http") || ak_span_eqc(scheme, "https");
	const char *rest = raw.p + c + 1;
	size_t n = raw.len - c - 1;
	ak_buf b = {0};
	if (n >= 2 && rest[0] == '/' && rest[1] == '/') {
		rest += 2, n -= 2;
		size_t h = 0;
		while (h < n && rest[h] != '/' && rest[h] != '?' && rest[h] != '#') h++;
		if (!web && h) {
			ak_buf_u8(&b, '/');
			ak_buf_put(&b, rest, h);
		}
		rest += h, n -= h;
	} else if (!web && n && rest[0] != '/') {
		ak_buf_u8(&b, '/');
	}
	ak_buf_put(&b, rest, n);
	if (b.len == 0 || b.data[0] == '?') {
		ak_buf t = {0};
		ak_buf_u8(&t, '/');
		ak_buf_put(&t, b.data, b.len);
		ak_buf_free(&b);
		b = t;
	}
	return sb_str(&b);
}

// ---- The routes ----

typedef struct rnode rnode;
typedef AK_VEC(rnode *) rnodes;

struct rnode {
	ak_view *route;  // its Route (the router holds it); NULL: the tree, the not found screen
	uint8_t layout;
	bool leaf, sheet;
	rnode *nav;       // the navigator it's a member of
	rnodes members;   // a navigator's: its children, groups flattened
	rnodes slots;     // its members that aren't sheets
	int32_t index, slot;
	segs segs;        // its full pattern
	AK_VEC(ak_closure *) guards;  // its own and its groups' (the Routes')
	rnode *initial;   // a navigator's first slot
};

typedef struct {
	ak_closure *build;
	AK_VEC(ak_view *) kids;  // its Routes (retained)
	AK_VEC(ak_view *) tabs;  // the Tabs made of it (retained; a gone one dropped as it's next seen)
} route_t;

static const ak_val *key_of(const ak_view *e, uint16_t key) { return ak_style_get(e, key); }

static ak_str *str_key(const ak_view *e, uint16_t key) {
	const ak_val *v = e ? key_of(e, key) : NULL;
	return v ? v->p : NULL;
}

static int64_t int_key(const ak_view *e, uint16_t key) {
	const ak_val *v = e ? key_of(e, key) : NULL;
	return v ? v->i : 0;
}

static bool has_str(const ak_view *e, uint16_t key) {
	ak_str *s = str_key(e, key);
	return s && s->len;
}

typedef struct inst inst;
typedef struct entry entry;

typedef struct {
	ak_view *e;           // the Router (not retained)
	AK_VEC(ak_view *) routes;  // its Routes (retained)
	rnode *tree, *not_found;
	rnodes leaves;        // most specific first
	rnodes all;
	inst *root;
	ak_owner *owner;      // its views' (a root under what made it)
	ak_view *shown;       // its kid: the tree's view
	uint64_t next_key;
	int applying;
	bool dead, started, addressed;
	ak_view *win;  // the window it's in (ak_window_of): the root's view, or a Window's
	struct ndesc *last;
	ak_str *last_href;
	ak_node href_node;
} router_t;

static AK_VEC(ak_view *) routers;  // the Routers mounted, the first first

static void member(rnode *nav, rnode *n) {
	n->index = (int32_t)nav->members.len;
	ak_vec_push(&nav->members, n);
	n->slot = -1;
	if (!n->sheet) {
		n->slot = (int32_t)nav->slots.len;
		ak_vec_push(&nav->slots, n);
	}
}

static rnode *rnode_new(router_t *rt) {
	rnode *n = ak_alloc(sizeof *n);
	ak_vec_push(&rt->all, n);
	return n;
}

static void pattern_text(ak_buf *b, const segs *ss) {
	if (!ss->len) ak_buf_u8(b, '/');
	for (uint32_t i = 0; i < ss->len; i++) {
		ak_buf_u8(b, '/');
		if (ss->data[i].kind == SEG_PARAM) ak_buf_u8(b, ':');
		else if (ss->data[i].kind == SEG_REST) ak_buf_u8(b, '*');
		ak_buf_put(b, ss->data[i].text->data, ss->data[i].text->len);
	}
}

static void walk(router_t *rt, rnode *nav, const segs *prefix, ak_closure *const *guards, uint32_t ng, ak_view *const *rs, uint32_t n) {
	for (uint32_t i = 0; i < n; i++) {
		ak_view *rv = rs[i];
		route_t *rr = rv->impl;
		ak_str *path = str_key(rv, AK_KEY_ROUTE_PATH);
		rnode *x = rnode_new(rt);
		x->route = rv;
		x->layout = (uint8_t)int_key(rv, AK_KEY_ROUTE_LAYOUT);
		x->sheet = int_key(rv, AK_KEY_ROUTE_SHEET) != 0;
		x->nav = nav;
		if (!(path && path->len && path->data[0] == '/'))
			for (uint32_t k = 0; k < prefix->len; k++) ak_vec_push(&x->segs, ((seg){prefix->data[k].kind, ak_retain(prefix->data[k].text)}));
		if (path) parse_pattern(&x->segs, path->data, path->len);
		for (uint32_t k = 0; k + 1 < x->segs.len; k++)
			if (x->segs.data[k].kind == SEG_REST) ak_panic("ui: Router: route %.*s: a catch-all must be last", path ? (int)path->len : 0, path ? path->data : "");
		for (uint32_t k = 0; k < ng; k++) ak_vec_push(&x->guards, guards[k]);
		const ak_val *g = key_of(rv, AK_KEY_ROUTE_GUARD);
		if (g && g->p) ak_vec_push(&x->guards, (ak_closure *)g->p);
		x->leaf = rr->build || has_str(rv, AK_KEY_ROUTE_REDIRECT);
		if (x->leaf) {
			if (x->layout != L_GROUP || rr->kids.len) ak_panic("ui: Router: a route with a screen (or a redirect) has no child routes");
			member(nav, x);
			ak_vec_push(&rt->leaves, x);
		} else if (x->layout == L_GROUP) {
			if (x->sheet) ak_panic("ui: Router: a group is a sheet: give it a layout (.stack) to make it one");
			walk(rt, nav, &x->segs, x->guards.data, x->guards.len, rr->kids.data, rr->kids.len);
		} else {
			member(nav, x);
			walk(rt, x, &x->segs, x->guards.data, x->guards.len, rr->kids.data, rr->kids.len);
		}
	}
}

// check finds each navigator's first slot and panics at shapes the
// navigators can't show.
static void check(router_t *rt, rnode *n) {
	if (n->leaf) return;
	if (!n->slots.len && n != rt->tree) ak_panic("ui: Router: a navigator has no routes to show (only sheets, or none)");
	n->initial = n->slots.len ? n->slots.data[0] : NULL;
	ak_str *ini = str_key(n->route, AK_KEY_ROUTE_INITIAL);
	if (ini && ini->len) {
		segs want = {0};
		if (ini->data[0] != '/')
			for (uint32_t k = 0; k < n->segs.len; k++) ak_vec_push(&want, ((seg){n->segs.data[k].kind, ak_retain(n->segs.data[k].text)}));
		parse_pattern(&want, ini->data, ini->len);
		n->initial = NULL;
		for (uint32_t i = 0; i < n->slots.len; i++)
			if (segs_eq(&n->slots.data[i]->segs, &want)) n->initial = n->slots.data[i];
		segs_free(&want);
		if (!n->initial) ak_panic("ui: Router: a navigator's initial %.*s is none of its routes", (int)ini->len, ini->data);
	}
	for (uint32_t i = 0; i < n->members.len; i++) {
		rnode *m = n->members.data[i];
		if (!m->sheet) {
			if (n->layout == L_STACK && !m->leaf) ak_panic("ui: Router: a stack's routes are screens, or sheets of any kind");
			if ((n->layout == L_TABS || n->layout == L_SPLIT) && !m->leaf && m->layout != L_STACK)
				ak_panic("ui: Router: a tab or a destination is a screen or a stack");
		}
		check(rt, m);
	}
}

static ak_val not_found_screen(ak_closure *c, ak_val *a, uint32_t n);

static void compile(router_t *rt) {
	rt->tree = rnode_new(rt);
	rt->tree->layout = L_SLOT;
	segs none = {0};
	walk(rt, rt->tree, &none, NULL, 0, rt->routes.data, rt->routes.len);
	rnode *nf = rnode_new(rt);
	nf->leaf = true;
	nf->nav = rt->tree;
	ak_vec_push(&nf->segs, ((seg){SEG_REST, ak_str_c("path")}));
	member(rt->tree, nf);
	ak_vec_push(&rt->leaves, nf);
	rt->not_found = nf;
	// Most specific first (stable).
	for (uint32_t i = 1; i < rt->leaves.len; i++) {
		rnode *x = rt->leaves.data[i];
		uint32_t j = i;
		while (j > 0 && rank_less(&x->segs, &rt->leaves.data[j - 1]->segs)) {
			rt->leaves.data[j] = rt->leaves.data[j - 1];
			j--;
		}
		rt->leaves.data[j] = x;
	}
	check(rt, rt->tree);
}

static void rnode_free(rnode *n) {
	ak_vec_free(&n->members);
	ak_vec_free(&n->slots);
	segs_free(&n->segs);
	ak_vec_free(&n->guards);
	ak_free(n);
}

// ---- Targets ----

typedef struct {
	rnode *leaf;
	kvs params;
	ak_str *query;
} target;

static void target_free(target *t) {
	kvs_free(&t->params);
	ak_release(t->query);
}

static ak_str *leaf_href(const rnode *l, const kvs *params, const ak_str *query) {
	ak_str *p = fill_pattern(&l->segs, params);
	if (!p) p = ak_str_c("/");
	return join_href(p, query);
}

static void match_href(router_t *rt, const ak_str *h, target *t) {
	ak_span path, q;
	split_href(h->data, h->len, &path, &q);
	strs ps = {0};
	split_path(&ps, path);
	t->query = ak_str_new(q.p, q.len);
	t->params = (kvs){0};
	for (uint32_t i = 0; i < rt->leaves.len; i++) {
		rnode *l = rt->leaves.data[i];
		if (l == rt->not_found) continue;
		kvs p = {0};
		if (match(&l->segs, &ps, &p)) {
			t->leaf = l;
			t->params = p;
			strs_free(&ps);
			return;
		}
	}
	t->leaf = rt->not_found;
	ak_buf b = {0};
	for (uint32_t i = 0; i < ps.len; i++) {
		if (i) ak_buf_u8(&b, '/');
		ak_buf_put(&b, ps.data[i]->data, ps.data[i]->len);
	}
	kvs_set(&t->params, ak_str_c("path"), sb_str(&b));
	strs_free(&ps);
}

// denied is where l's guards send the user instead of h (NULL lets them
// in), asked untracked.
static ak_str *denied(const rnode *l, ak_str *h) {
	for (uint32_t i = 0; i < l->guards.len; i++) {
		ak_closure *g = l->guards.data[i];
		ak_val arg = {.p = h};
		void *tok = ak_untrack_begin();
		ak_val to = ak_call(g, &arg, 1);
		ak_untrack_end(tok);
		if (ak_error_pending()) {
			ak_ui_raise_pending(NULL);
			return NULL;
		}
		ak_str *s = to.p;
		if (s && s->len && !ak_str_eq(s, h)) return s;
		ak_release(s);
	}
	return NULL;
}

// resolve is the screen h names after redirects and guards.
static bool resolve(router_t *rt, ak_str *h, target *t) {
	ak_retain(h);
	for (int i = 0; i < 32; i++) {
		match_href(rt, h, t);
		ak_str *redirect = t->leaf->route ? str_key(t->leaf->route, AK_KEY_ROUTE_REDIRECT) : NULL;
		if (redirect && redirect->len) {
			segs rs = {0};
			parse_pattern(&rs, redirect->data, redirect->len);
			ak_str *to = fill_pattern(&rs, &t->params);
			segs_free(&rs);
			ak_span rp, rq;
			split_href(redirect->data, redirect->len, &rp, &rq);
			if (!to) to = ak_str_new(rp.p, rp.len);
			ak_str *q = rq.len ? ak_str_new(rq.p, rq.len) : ak_retain(t->query);
			ak_release(h);
			h = join_href(to, q);
			ak_release(q);
			target_free(t);
			continue;
		}
		ak_str *href = leaf_href(t->leaf, &t->params, t->query);
		ak_str *to = denied(t->leaf, href);
		ak_release(href);
		if (to) {
			ak_release(h);
			h = to;
			target_free(t);
			continue;
		}
		ak_release(h);
		return true;
	}
	ak_release(h);
	ak_throw_msg("Router: redirects and guards go round in a loop");
	return false;
}

// ---- The state, described ----

typedef struct edesc edesc;
typedef AK_VEC(edesc *) edescs;

typedef struct ndesc {
	rnode *node;
	int32_t sel;
	edescs entries;  // a stack's, bottom first; tabs': one per slot (NULL: not made); a slot's: the shown one
	edescs sheets;   // over it, the oldest first
} ndesc;

struct edesc {
	uint64_t key;    // the live entry it describes (0: one to make)
	rnode *member;   // NULL: a screen pushed with push(), which has no route
	kvs params;
	ak_str *query;
	ndesc *child;    // a navigator's
};

static void ndesc_free(ndesc *d);

static void edesc_free(edesc *e) {
	if (!e) return;
	kvs_free(&e->params);
	ak_release(e->query);
	ndesc_free(e->child);
	ak_free(e);
}

static void ndesc_free(ndesc *d) {
	if (!d) return;
	for (uint32_t i = 0; i < d->entries.len; i++) edesc_free(d->entries.data[i]);
	for (uint32_t i = 0; i < d->sheets.len; i++) edesc_free(d->sheets.data[i]);
	ak_vec_free(&d->entries);
	ak_vec_free(&d->sheets);
	ak_free(d);
}

static void cut_to(edescs *v, uint32_t n) {
	while (v->len > n) edesc_free(v->data[--v->len]);
}

static edesc *content_of(const ndesc *d) {
	if (d->node->layout == L_TABS) return d->sel >= 0 && (uint32_t)d->sel < d->entries.len ? d->entries.data[d->sel] : NULL;
	return d->entries.len ? d->entries.data[d->entries.len - 1] : NULL;
}

static edesc *focus_of(const ndesc *d) { return d->sheets.len ? d->sheets.data[d->sheets.len - 1] : content_of(d); }

static edesc *routed_focus(const ndesc *d) {
	if (d->sheets.len) return d->sheets.data[d->sheets.len - 1];
	if (d->node->layout == L_TABS) return content_of(d);
	for (uint32_t i = d->entries.len; i-- > 0;)
		if (d->entries.data[i] && d->entries.data[i]->member) return d->entries.data[i];
	return NULL;
}

static int hist_len(const ndesc *d) {
	int n = (int)d->sheets.len;
	if (d->node->layout == L_STACK) return n + (int)d->entries.len;
	n++;
	if (d->node->initial && d->sel != d->node->initial->slot) n++;
	return n;
}

static bool same_entry(const edesc *a, const edesc *b) {
	if (!a || !b) return a == b;
	if (a->key && b->key) return a->key == b->key;
	return a->member && a->member == b->member && kvs_eq(&a->params, &b->params);
}

// history_delta is how the history moved from o to n: a push (> 0), a
// back by as many (< 0), or a replace, decided where the focused screens
// part, as react-navigation's linking does.
static int history_delta(const ndesc *o, const ndesc *n) {
	while (o && n && o->node == n->node) {
		int ol = hist_len(o), nl = hist_len(n);
		if (ol != nl) return nl - ol;
		edesc *of = focus_of(o), *nf = focus_of(n);
		if (!of || !nf || !same_entry(of, nf)) return 0;
		o = of->child, n = nf->child;
	}
	return 0;
}

static ak_str *desc_href(const ndesc *d) {
	while (d) {
		edesc *f = routed_focus(d);
		if (!f || !f->member) {
			ak_buf b = {0};
			pattern_text(&b, &d->node->segs);
			return sb_str(&b);
		}
		if (f->member->leaf) return leaf_href(f->member, &f->params, f->query);
		d = f->child;
	}
	return ak_str_c("/");
}

typedef struct {
	rnode *nav, *member;
} rlink;
typedef AK_VEC(rlink) rlinks;

static void chain_of(rlinks *c, rnode *l) {
	for (rnode *m = l; m->nav; m = m->nav) ak_vec_push(c, ((rlink){m->nav, m}));
	for (uint32_t i = 0; i < c->len / 2; i++) {
		rlink t = c->data[i];
		c->data[i] = c->data[c->len - 1 - i];
		c->data[c->len - 1 - i] = t;
	}
}

static ndesc *fresh(router_t *rt, rnode *nav, const rlink *chain, uint32_t nchain, const target *t);

// fresh_entry describes member m as a rlink to the rest of the chain
// builds it; a screen under the one linked to takes its parameters from
// it, by name, and isn't made if it needs one it hasn't or a guard keeps
// the user out.
static edesc *fresh_entry(router_t *rt, rnode *m, const rlink *rest, uint32_t nrest, const target *t) {
	edesc *e = ak_alloc(sizeof *e);
	e->member = m;
	if (!m->leaf) {
		e->child = fresh(rt, m, rest, nrest, t);
		return e;
	}
	if (t && t->leaf == m) {
		kvs_copy(&e->params, &t->params);
		e->query = ak_retain(t->query);
		return e;
	}
	if (m->route && has_str(m->route, AK_KEY_ROUTE_REDIRECT)) {
		ak_free(e);
		return NULL;
	}
	for (uint32_t i = 0; i < m->segs.len; i++) {
		seg *s = &m->segs.data[i];
		if (s->kind == SEG_STATIC) continue;
		ak_str *v = t ? kvs_get(&t->params, s->text) : NULL;
		if (!v && s->kind == SEG_PARAM) {
			edesc_free(e);
			return NULL;
		}
		kvs_set(&e->params, ak_retain(s->text), v ? ak_retain(v) : ak_str_empty());
	}
	ak_str *h = leaf_href(m, &e->params, NULL);
	ak_str *to = denied(m, h);
	ak_release(h);
	if (to) {
		ak_release(to);
		edesc_free(e);
		return NULL;
	}
	return e;
}

static void push_desc(edescs *v, edesc *e) {
	if (e) ak_vec_push(v, e);
}

// fresh describes navigator nav as a rlink to the chain's screen builds
// it: a stack's initial screen under the one linked to, and the screens
// up its path; a tab's or a slot's, the one linked to (else its initial).
static ndesc *fresh(router_t *rt, rnode *nav, const rlink *chain, uint32_t nchain, const target *t) {
	ndesc *d = ak_alloc(sizeof *d);
	d->node = nav;
	rnode *m = nchain ? chain[0].member : NULL;
	const rlink *rest = nchain ? chain + 1 : NULL;
	uint32_t nrest = nchain ? nchain - 1 : 0;
	switch (nav->layout) {
	case L_STACK:
		if ((!m || m->sheet || m != nav->initial) && nav->initial) push_desc(&d->entries, fresh_entry(rt, nav->initial, NULL, 0, t));
		if (m) {
			// The screens up its path: /inbox/:id under /inbox/:id/reply.
			rnodes up = {0};
			for (uint32_t i = 0; i < nav->slots.len; i++) {
				rnode *s = nav->slots.data[i];
				if (s != nav->initial && s != m && s->leaf && is_prefix(&s->segs, &m->segs)) ak_vec_push(&up, s);
			}
			for (uint32_t i = 1; i < up.len; i++) {
				rnode *x = up.data[i];
				uint32_t j = i;
				while (j > 0 && up.data[j - 1]->segs.len > x->segs.len) {
					up.data[j] = up.data[j - 1];
					j--;
				}
				up.data[j] = x;
			}
			for (uint32_t i = 0; i < up.len; i++) push_desc(&d->entries, fresh_entry(rt, up.data[i], NULL, 0, t));
			ak_vec_free(&up);
			if (!m->sheet) push_desc(&d->entries, fresh_entry(rt, m, rest, nrest, t));
		}
		break;
	case L_TABS:
		// Each tab's entry is made as it's first chosen: the one linked to
		// (else the initial) now.
		d->sel = nav->initial ? nav->initial->slot : 0;
		if (m && !m->sheet) d->sel = m->slot;
		for (uint32_t i = 0; i < nav->slots.len; i++) {
			rnode *s = nav->slots.data[i];
			edesc *e = NULL;
			if (s == m) e = fresh_entry(rt, s, rest, nrest, t);
			else if ((int32_t)i == d->sel) e = fresh_entry(rt, s, NULL, 0, NULL);
			ak_vec_push(&d->entries, e);
		}
		break;
	default: {
		rnode *s = nav->initial;
		const rlink *r = NULL;
		uint32_t nr = 0;
		const target *tt = NULL;
		if (m && !m->sheet) s = m, r = rest, nr = nrest, tt = t;
		if (s) {
			d->sel = s->slot;
			push_desc(&d->entries, fresh_entry(rt, s, r, nr, tt));
		}
	}
	}
	if (m && m->sheet) push_desc(&d->sheets, fresh_entry(rt, m, rest, nrest, t));
	return d;
}

static void step(router_t *rt, ndesc *d, const rlink *chain, uint32_t nchain, const target *t, int mode);

static void step_into(router_t *rt, edesc **p, rnode *m, const rlink *rest, uint32_t nrest, const target *t, int mode) {
	edesc *e = *p;
	if (!e || e->member != m || (m->leaf && (mode == MODE_REPLACE || !kvs_eq(&e->params, &t->params)))) {
		edesc_free(e);
		*p = fresh_entry(rt, m, rest, nrest, t);
		return;
	}
	if (m->leaf) {
		ak_retain(t->query);
		ak_release(e->query);
		e->query = t->query;
		return;
	}
	step(rt, e->child, rest, nrest, t, mode);
}

// step changes d, the state of the chain's first navigator, to show t.
static void step(router_t *rt, ndesc *d, const rlink *chain, uint32_t nchain, const target *t, int mode) {
	rnode *m = chain[0].member;
	const rlink *rest = chain + 1;
	uint32_t nrest = nchain - 1;
	if (m->sheet) {
		for (uint32_t i = 0; i < d->sheets.len; i++) {
			edesc *s = d->sheets.data[i];
			if (s->member != m || (m->leaf && (mode == MODE_PUSH || !kvs_eq(&s->params, &t->params)))) continue;
			cut_to(&d->sheets, i + 1);
			if (!m->leaf) {
				step(rt, s->child, rest, nrest, t, mode);
			} else if (mode == MODE_REPLACE) {
				d->sheets.data[i] = fresh_entry(rt, m, NULL, 0, t);
				edesc_free(s);
			} else {
				ak_retain(t->query);
				ak_release(s->query);
				s->query = t->query;
			}
			return;
		}
		edesc *e = fresh_entry(rt, m, rest, nrest, t);
		if (mode == MODE_REPLACE && nrest == 0 && d->sheets.len) {
			edesc_free(d->sheets.data[d->sheets.len - 1]);
			d->sheets.data[d->sheets.len - 1] = e;
		} else {
			push_desc(&d->sheets, e);
		}
		return;
	}
	if (d->sheets.len) {
		cut_to(&d->sheets, 0);  // going to the navigator's own screens closes what's over them
		if (mode == MODE_REPLACE) mode = MODE_NAVIGATE;
	}
	switch (d->node->layout) {
	case L_STACK: {
		uint32_t n = d->entries.len;
		if (mode == MODE_NAVIGATE) {
			for (uint32_t i = n; i-- > 0;) {
				edesc *e = d->entries.data[i];
				if (e && e->member == m && kvs_eq(&e->params, &t->params)) {
					cut_to(&d->entries, i + 1);
					ak_retain(t->query);
					ak_release(e->query);
					e->query = t->query;
					return;
				}
			}
			push_desc(&d->entries, fresh_entry(rt, m, NULL, 0, t));
		} else if (mode == MODE_PUSH || n == 0) {
			push_desc(&d->entries, fresh_entry(rt, m, NULL, 0, t));
		} else {
			edesc *e = fresh_entry(rt, m, NULL, 0, t);
			if (e) {
				edesc_free(d->entries.data[n - 1]);
				d->entries.data[n - 1] = e;
			}
		}
		break;
	}
	case L_TABS:
		d->sel = m->slot;
		while (d->entries.len <= (uint32_t)m->slot) ak_vec_push(&d->entries, NULL);
		step_into(rt, &d->entries.data[m->slot], m, rest, nrest, t, mode);
		break;
	default:
		if (d->sel != m->slot || !d->entries.len) {
			d->sel = m->slot;
			cut_to(&d->entries, 0);
			push_desc(&d->entries, fresh_entry(rt, m, rest, nrest, t));
			return;
		}
		cut_to(&d->entries, 1);
		step_into(rt, &d->entries.data[0], m, rest, nrest, t, mode);
	}
}

// back_desc goes back in d: the innermost first, the newest sheet, else
// the stack's top screen; false when there's nowhere to go.
static bool back_desc(ndesc *d) {
	edesc *f = focus_of(d);
	if (f && f->child && back_desc(f->child)) return true;
	if (d->sheets.len) {
		cut_to(&d->sheets, d->sheets.len - 1);
		return true;
	}
	if (d->node->layout == L_STACK && d->entries.len > 1) {
		cut_to(&d->entries, d->entries.len - 1);
		return true;
	}
	return false;
}

// ---- The live state ----

// A place whose one child the router sets: a tab's page, a slot.
typedef struct {
	ak_view *holder;  // a Tab, or a fragment (retained)
	ak_owner *owner;  // what it shows', disposed as it changes
	ak_view *shown;
} slotv;

struct inst {
	router_t *rt;
	rnode *node;
	ak_owner *owner;  // what its entries are made under
	AK_VEC(entry *) entries;
	int32_t sel;
	AK_VEC(entry *) sheets;
	ak_view *view;     // its view (retained): a Stack, Tabs, or a slot's fragment
	AK_VEC(slotv) pages;  // tabs'
	slotv slot;        // a slot's
	bool dying;
};

struct entry {
	uint64_t key;
	rnode *member;
	kvs params;
	ak_str *query;
	void *cell;       // its Params, which its screen reads (an accessor of it)
	inst *nav, *child;
	ak_view *screen;  // a stack's screen (the stack holds it)
	ak_view *sheet;   // a sheet's (retained)
};

static void inst_free(inst *in);

static void entry_free(entry *e) {
	if (!e) return;
	if (e->child) inst_free(e->child);
	kvs_free(&e->params);
	ak_release(e->query);
	ak_release(e->cell);
	ak_release(e->sheet);
	ak_free(e);
}

static void slot_clear(slotv *sv) {
	if (sv->shown) {
		ak_view_remove(sv->holder, sv->shown, true);
		sv->shown = NULL;
	}
	if (sv->owner) {
		ak_owner *o = sv->owner;
		sv->owner = NULL;
		ak_owner_dispose(o);
	}
}

static void inst_free(inst *in) {
	in->dying = true;
	for (uint32_t i = 0; i < in->sheets.len; i++) {
		entry *e = in->sheets.data[i];
		if (e->sheet) ak_sheet_close(e->sheet);
		entry_free(e);
	}
	ak_vec_free(&in->sheets);
	for (uint32_t i = 0; i < in->pages.len; i++) {
		slot_clear(&in->pages.data[i]);
		ak_release(in->pages.data[i].holder);
	}
	ak_vec_free(&in->pages);
	slot_clear(&in->slot);
	ak_release(in->slot.holder);
	for (uint32_t i = 0; i < in->entries.len; i++) entry_free(in->entries.data[i]);
	ak_vec_free(&in->entries);
	ak_release(in->view);
	ak_free(in);
}

// Params, as types.asuka has them: href, path, query.
static const uint8_t params_kinds[] = {AK_K_REF, AK_K_REF, AK_K_REF};
static const char *const params_names[] = {"href", "path", "query"};
static const ak_shape *const no_shapes[3];
const ak_shape ak_ui_shape_params = {"Params", 3, params_kinds, no_shapes, params_names, 0, NULL, false, NULL};

static ak_map *map_of(const kvs *p) {
	ak_map *m = ak_map_new(AK_K_REF, true, AK_K_REF, NULL);
	for (uint32_t i = 0; i < p->len; i++) ak_map_set(&m, (ak_val){.p = ak_retain(p->data[i].k)}, (ak_val){.p = ak_retain(p->data[i].v)});
	return m;
}

static ak_val params_value(const entry *e) {
	ak_struct *s = ak_struct_new(&ak_ui_shape_params);
	s->f[0].p = e->member && e->member->leaf ? leaf_href(e->member, &e->params, e->query) : ak_str_empty();
	s->f[1].p = map_of(&e->params);
	kvs q = {0};
	const ak_str *qs = e->query;
	for (uint32_t i = 0, start = 0; qs && i <= qs->len; i++) {
		if (i == qs->len || qs->data[i] == '&') {
			if (i > start) {
				uint32_t eq = start;
				while (eq < i && qs->data[eq] != '=') eq++;
				ak_str *k = unescape(qs->data + start, eq - start, true);
				ak_str *v = eq < i ? unescape(qs->data + eq + 1, i - eq - 1, true) : ak_str_empty();
				if (kvs_get(&q, k)) ak_release(k), ak_release(v);  // its first value
				else kvs_set(&q, k, v);
			}
			start = i + 1;
		}
	}
	s->f[2].p = map_of(&q);
	kvs_free(&q);
	return (ak_val){.p = s};
}

static entry *new_entry(router_t *rt, inst *nav, const edesc *d) {
	entry *e = ak_alloc(sizeof *e);
	e->key = d->key ? d->key : ++rt->next_key;
	e->member = d->member;
	kvs_copy(&e->params, &d->params);
	e->query = d->query ? ak_retain(d->query) : NULL;
	e->nav = nav;
	e->cell = ak_vm_cell_new(params_value(e), AK_K_REF, NULL).p;
	return e;
}

static bool same_live(const entry *e, const edesc *d) {
	if (!e || !d) return !e && !d;
	if (d->key) return e->key == d->key;
	return e->member && e->member == d->member && kvs_eq(&e->params, &d->params);
}

static ndesc *snapshot(const inst *n);

static edesc *snapshot_entry(const entry *e) {
	if (!e) return NULL;
	edesc *d = ak_alloc(sizeof *d);
	d->key = e->key;
	d->member = e->member;
	kvs_copy(&d->params, &e->params);
	d->query = e->query ? ak_retain(e->query) : NULL;
	if (e->child) d->child = snapshot(e->child);
	return d;
}

static ndesc *snapshot(const inst *n) {
	ndesc *d = ak_alloc(sizeof *d);
	d->node = n->node;
	d->sel = n->sel;
	for (uint32_t i = 0; i < n->entries.len; i++) ak_vec_push(&d->entries, snapshot_entry(n->entries.data[i]));
	for (uint32_t i = 0; i < n->sheets.len; i++) ak_vec_push(&d->sheets, snapshot_entry(n->sheets.data[i]));
	return d;
}

static inst *new_inst(router_t *rt, rnode *node, ak_owner *owner) {
	inst *in = ak_alloc(sizeof *in);
	in->rt = rt;
	in->node = node;
	in->owner = owner;
	return in;
}

static void changed(router_t *rt, int action);
static ak_view *nav_view(router_t *rt, inst *in, const ndesc *d);
static void reconcile(router_t *rt, inst *in, const ndesc *d);

// The screen of a leaf entry: its Route's builder, with its Params.
static ak_closure *builder_of(const rnode *m) {
	if (!m->route) return NULL;
	return ((route_t *)m->route->impl)->build;
}

static ak_view *build_leaf(router_t *rt, entry *e, ak_owner *o) {
	ak_val acc = ak_vm_acc_cell(e->cell);
	ak_view *v;
	if (!e->member->route) {
		ak_closure *c = ak_closure_new(not_found_screen, NULL, 0, NULL);
		v = ak_build_under(rt->e->r, o, c, &acc, 1);
		ak_release(c);
	} else {
		v = ak_build_under(rt->e->r, o, builder_of(e->member), &acc, 1);
	}
	ak_release(acc.p);
	return v;
}

static void title_screen(ak_view *screen, const rnode *m) {
	ak_str *t = m && m->route ? str_key(m->route, AK_KEY_ROUTE_TITLE) : NULL;
	if (screen && t) ak_screen_set_title(screen, t);
}

static void push_screen(inst *in, entry *e) {
	e->nav = in;
	ak_val acc = ak_vm_acc_cell(e->cell);
	ak_closure *build = builder_of(e->member);
	ak_closure *nf = NULL;
	if (!build) build = nf = ak_closure_new(not_found_screen, NULL, 0, NULL);
	e->screen = ak_stack_push(in->view, build, &acc, 1, in->owner);
	ak_release(nf);
	ak_release(acc.p);
	title_screen(e->screen, e->member);
}

typedef struct {
	router_t *rt;
	inst *in;
	entry *e;
	const edesc *d;
	ak_view *out;
} content_ctx;

// content builds what an entry shows in a tab's page, a slot or a sheet:
// a screen, or its navigator's views (under the current owner).
static void content_fn(void *ctx) {
	content_ctx *c = ctx;
	if (c->e->member->leaf) {
		c->out = build_leaf(c->rt, c->e, ak_current_owner());
		return;
	}
	c->e->child = new_inst(c->rt, c->e->member, ak_current_owner());
	c->out = nav_view(c->rt, c->e->child, c->d->child);
}

static void slot_set(router_t *rt, inst *in, slotv *sv, entry *e, const edesc *d) {
	slot_clear(sv);
	sv->owner = ak_root_under(in->owner);
	if (sv->holder->prov) ak_owner_set_context(sv->owner, ak_env_key, sv->holder->prov);  // a tab's page's environment
	content_ctx c = {rt, in, e, d, NULL};
	ak_ui_root *pr = ak_ui.cur;
	ak_ui.cur = rt->e->r;
	ak_with_owner(sv->owner, content_fn, &c);
	ak_ui.cur = pr;
	if (c.out) {
		c.out->fill = true;
		if (!c.out->frag) ak_style_relayout(c.out);
		sv->shown = c.out;
		ak_view_append(sv->holder, c.out);
		ak_release(c.out);
	}
}

static void update(router_t *rt, entry *e, const edesc *d) {
	if (!str_eq(e->query, d->query)) {
		ak_release(e->query);
		e->query = d->query ? ak_retain(d->query) : NULL;
		ak_vm_cell_set(e->cell, params_value(e));  // the screen follows its query
	}
	if (e->child && d->child) reconcile(rt, e->child, d->child);
}

static void reconcile_slot(router_t *rt, inst *in, slotv *sv, entry **have, const edesc *want) {
	if (*have && same_live(*have, want)) {
		update(rt, *have, want);
		return;
	}
	if (*have) {
		slot_clear(sv);
		entry_free(*have);
		*have = NULL;
	}
	if (!want) return;
	entry *e = new_entry(rt, in, want);
	*have = e;
	slot_set(rt, in, sv, e, want);
}

static void reconcile_stack(router_t *rt, inst *in, const edescs *want) {
	uint32_t k = 0;
	while (k < in->entries.len && k < want->len && same_live(in->entries.data[k], want->data[k])) k++;
	for (uint32_t i = 0; i < k; i++) update(rt, in->entries.data[i], want->data[i]);
	uint32_t nold = in->entries.len - k;
	entry **old = ak_alloc((nold ? nold : 1) * sizeof *old);
	if (nold) memcpy(old, in->entries.data + k, nold * sizeof *old);
	in->entries.len = k;
	for (uint32_t i = k; i < want->len; i++) {
		const edesc *d = want->data[i];
		if (!d || !d->member) continue;  // a screen push() pushed: it can't be made again
		entry *e = new_entry(rt, in, d);
		push_screen(in, e);
		ak_vec_push(&in->entries, e);
	}
	// The old ones go, the lower ones first, the top last, so one pop
	// animates.
	for (uint32_t i = 0; i < nold; i++) {
		entry *e = old[i];
		ak_view *stack = in->view;
		for (uint32_t j = 0; j < ak_stack_depth(stack); j++) {
			if (ak_stack_screen(stack, j) == e->screen) {
				// Pop it alone: the screens above it are old ones, popped next.
				uint32_t depth = ak_stack_depth(stack);
				if (j == depth - 1) ak_stack_pop_to(stack, j);
				else ak_stack_remove_at(stack, j);  // a lower one: out of the middle
				break;
			}
		}
		entry_free(e);
	}
	ak_free(old);
}

static void dismissed(void *ctx);

static ak_val sheet_content(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(a), AK_UNUSED(n);
	content_ctx *cc = (content_ctx *)(uintptr_t)c->caps[0].i;
	content_fn(cc);
	ak_view *v = cc->out;
	cc->out = NULL;
	return (ak_val){.p = v};
}

static const uint8_t int_cap[] = {AK_K_INT};

static void present(router_t *rt, inst *in, entry *e, const edesc *d) {
	const ak_val *dv = key_of(e->member->route, AK_KEY_ROUTE_DETENTS);
	int64_t detents = 0;
	const ak_arr *da = dv ? dv->p : NULL;
	for (uint32_t i = 0; da && i < da->len; i++) detents |= INT64_C(1) << ak_arr_vals(da)[i].i;
	content_ctx cc = {rt, in, e, d, NULL};
	ak_closure *c = ak_closure_new(sheet_content, NULL, 1, int_cap);
	c->caps[0].i = (int64_t)(uintptr_t)&cc;
	bool grabber = int_key(e->member->route, AK_KEY_ROUTE_GRABBER) != 0, full = int_key(e->member->route, AK_KEY_ROUTE_FULL_SCREEN) != 0;
	ak_view *s = ak_sheet_present(rt->e->r, in->owner, c, NULL, 0, detents, grabber, full, false, true, dismissed, e);
	ak_release(c);
	e->sheet = ak_retain(s);
	if (e->member->leaf) title_screen(s->kids.len ? s->kids.data[0] : NULL, e->member);
}

static void reconcile(router_t *rt, inst *in, const ndesc *d) {
	rt->applying++;
	// The sheets that go, first: the newest first.
	uint32_t k = 0;
	while (k < in->sheets.len && k < d->sheets.len && same_live(in->sheets.data[k], d->sheets.data[k])) k++;
	for (uint32_t i = in->sheets.len; i-- > k;) {
		entry *e = in->sheets.data[i];
		if (e->sheet) ak_sheet_close(e->sheet);
		entry_free(e);
	}
	in->sheets.len = k;
	switch (in->node->layout) {
	case L_STACK: reconcile_stack(rt, in, &d->entries); break;
	case L_TABS:
		for (uint32_t i = 0; i < in->node->slots.len; i++) {
			const edesc *want = i < d->entries.len ? d->entries.data[i] : NULL;
			if (!want && in->entries.data[i]) continue;  // a tab's page, made once, stays
			reconcile_slot(rt, in, &in->pages.data[i], &in->entries.data[i], want);
		}
		in->sel = d->sel;
		ak_ui_set(in->view, AK_KEY_TABS_SELECTED, (ak_val){.i = d->sel});
		break;
	default: {
		entry *have = in->entries.len ? in->entries.data[0] : NULL;
		const edesc *want = d->entries.len ? d->entries.data[0] : NULL;
		reconcile_slot(rt, in, &in->slot, &have, want);
		in->entries.len = 0;
		if (have) ak_vec_push(&in->entries, have);
		in->sel = d->sel;
	}
	}
	for (uint32_t i = 0; i < k; i++) update(rt, in->sheets.data[i], d->sheets.data[i]);
	for (uint32_t i = k; i < d->sheets.len; i++) {
		entry *e = new_entry(rt, in, d->sheets.data[i]);
		ak_vec_push(&in->sheets, e);
		present(rt, in, e, d->sheets.data[i]);
	}
	rt->applying--;
}

// dismissed is the user's dismissal of a sheet entry (or going back from
// it): it and those over it go.
static void dismissed(void *ctx) {
	entry *e = ctx;
	inst *in = e->nav;
	router_t *rt = in->rt;
	if (rt->applying || rt->dead || in->dying) return;
	uint32_t i = 0;
	while (i < in->sheets.len && in->sheets.data[i] != e) i++;
	if (i == in->sheets.len) return;
	rt->applying++;  // what closes with them is the router's doing
	for (uint32_t j = in->sheets.len; j-- > i;) {
		entry *s = in->sheets.data[j];
		if (s->sheet) ak_sheet_close(s->sheet);
		entry_free(s);
	}
	in->sheets.len = i;
	rt->applying--;
	changed(rt, -1);
}

// A routed stack's screens pushed and popped by push(), pop() and the
// user are the router's to know.
static void stack_pushed(void *ctx, ak_view *screen) {
	inst *in = ctx;
	router_t *rt = in->rt;
	if (rt->applying || rt->dead || in->dying) return;
	entry *e = ak_alloc(sizeof *e);
	e->key = ++rt->next_key;
	e->nav = in;
	e->screen = screen;
	e->cell = ak_vm_cell_new((ak_val){.p = NULL}, AK_K_REF, NULL).p;
	ak_vec_push(&in->entries, e);
	changed(rt, -1);
}

static void stack_popped(void *ctx, ak_view *screen) {
	inst *in = ctx;
	router_t *rt = in->rt;
	if (rt->applying || rt->dead || in->dying) return;
	for (uint32_t i = 0; i < in->entries.len; i++) {
		entry *e = in->entries.data[i];
		if (e->screen == screen) {
			memmove(in->entries.data + i, in->entries.data + i + 1, (in->entries.len - i - 1) * sizeof(entry *));
			in->entries.len--;
			entry_free(e);
			changed(rt, -1);
			return;
		}
	}
}

static void chose(router_t *rt, inst *in, int64_t i);

static ak_val tab_chosen(ak_closure *c, ak_val *a, uint32_t n) {
	inst *in = (inst *)(uintptr_t)c->caps[0].i;
	if (n > 0) chose(in->rt, in, a[0].i);
	return (ak_val){0};
}

// nav_view makes in's views, showing d.
static ak_view *nav_view(router_t *rt, inst *in, const ndesc *d) {
	ak_ui_root *r = rt->e->r;
	rnode *n = in->node;
	switch (n->layout) {
	case L_STACK: {
		ak_view *s = ak_stack_new(r);
		ak_view_finalize(s);
		in->view = ak_retain(s);
		ak_batch_set_bool(r->batch, s->id, AK_PROP_STACK_ROUTED, true);
		ak_stack_on_pop(s, stack_popped, in);
		ak_stack_on_push(s, stack_pushed, in);
		reconcile(rt, in, d);
		return s;
	}
	case L_TABS: {
		ak_view *tabs = ak_ui_new(AK_EL_TABS);
		in->view = ak_retain(tabs);
		for (uint32_t i = 0; i < n->slots.len; i++) {
			rnode *s = n->slots.data[i];
			ak_view *tab = ak_ui_new(AK_EL_TAB);
			const uint16_t from[] = {AK_KEY_ROUTE_TITLE, AK_KEY_ROUTE_ICON, AK_KEY_ROUTE_BADGE};
			const uint16_t to[] = {AK_KEY_TAB_TITLE, AK_KEY_TAB_ICON, AK_KEY_TAB_BADGE};
			for (int k = 0; k < 3; k++) {
				const ak_val *v = s->route ? key_of(s->route, from[k]) : NULL;
				if (v) ak_ui_set(tab, to[k], *v);
			}
			// It follows the Route's title, icon and badge (route_set).
			if (s->route) {
				route_t *rr = s->route->impl;
				ak_vec_push(&rr->tabs, (ak_view *)ak_retain(tab));
			}
			ak_ui_add(tabs, tab);
			ak_vec_push(&in->pages, ((slotv){tab, NULL, NULL}));  // the tabs hold it, and the page
			ak_vec_push(&in->entries, NULL);
		}
		ak_closure *c = ak_closure_new(tab_chosen, NULL, 1, int_cap);
		c->caps[0].i = (int64_t)(uintptr_t)in;
		ak_ui_on(tabs, AK_EV_TABS_CHANGE, c);
		ak_release(c);
		ak_ui_set(tabs, AK_KEY_TABS_SELECTED, (ak_val){.i = d->sel});
		reconcile(rt, in, d);
		return tabs;
	}
	default: {
		ak_view *f = ak_frag_new();
		in->slot.holder = ak_retain(f);
		in->view = ak_retain(f);
		reconcile(rt, in, d);
		return f;
	}
	}
}

// chose is the user's choice of a tab.
static void chose(router_t *rt, inst *in, int64_t i) {
	if (rt->applying || rt->dead || i < 0 || i >= in->node->slots.len) return;
	ndesc *d = snapshot(in);
	d->sel = (int32_t)i;
	cut_to(&d->sheets, 0);
	if (!d->entries.data[i]) d->entries.data[i] = fresh_entry(rt, in->node->slots.data[i], NULL, 0, NULL);
	reconcile(rt, in, d);
	ndesc_free(d);
	changed(rt, -1);
}

// ---- Navigating ----

typedef struct {
	router_t *rt;
	const ndesc *d;
} apply_ctx;

static void apply_fn(void *ctx) {
	apply_ctx *a = ctx;
	ak_ui_root *pr = ak_ui.cur;
	ak_ui.cur = a->rt->e->r;
	reconcile(a->rt, a->rt->root, a->d);
	ak_ui.cur = pr;
}

static void apply(router_t *rt, const ndesc *d) {
	apply_ctx a = {rt, d};
	ak_untrack(apply_fn, &a);
}

static void navigate(router_t *rt, ak_str *h, int mode) {
	if (rt->dead || !rt->started) return;
	target t = {0};
	if (!resolve(rt, h, &t)) return;
	ndesc *d = snapshot(rt->root);
	rlinks c = {0};
	chain_of(&c, t.leaf);
	step(rt, d, c.data, c.len, &t, mode);
	ak_vec_free(&c);
	apply(rt, d);
	ndesc_free(d);
	target_free(&t);
	changed(rt, -1);
}

static void send(router_t *rt, int action, int32_t delta) {
	if (!rt->addressed) return;
	ak_view *w = rt->win;
	if (!w || w->dead) return;
	ak_location l = {(uint8_t)action, delta, ak_strn(rt->last_href->data, rt->last_href->len), ak_cstr(""), ak_cstr("")};
	ak_buf *b = ak_batch_command_begin(w->r->batch, w->id, AK_CMD_LOCATION);
	ak_append_location(b, &l);
	ak_batch_command_end(w->r->batch);
}

// changed tells the host of the state's new address, if it changed, with
// how the history moved (action -1: as the state did).
static void changed(router_t *rt, int action) {
	if (rt->applying || rt->dead || !rt->last) return;
	ndesc *now = snapshot(rt->root);
	ak_str *href = desc_href(now);
	int delta = history_delta(rt->last, now);
	ndesc_free(rt->last);
	rt->last = now;
	if (ak_str_eq(href, rt->last_href) && delta == 0 && action < 0) {
		ak_release(href);
		return;
	}
	ak_release(rt->last_href);
	rt->last_href = href;
	ak_signal_write(&rt->href_node);
	if (action >= 0) send(rt, action, 0);
	else if (delta > 0) send(rt, AK_LOCATION_ACTION_PUSH, 0);
	else if (delta < 0) send(rt, AK_LOCATION_ACTION_BACK, delta);
	else send(rt, AK_LOCATION_ACTION_REPLACE, 0);
}

static bool go_back(router_t *rt) {
	ndesc *d = snapshot(rt->root);
	bool ok = back_desc(d);
	if (ok) {
		apply(rt, d);
		changed(rt, -1);
	}
	ndesc_free(d);
	return ok;
}

static bool dismiss_sheet(router_t *rt) {
	ndesc *d = snapshot(rt->root), *deepest = NULL;
	for (ndesc *n = d; n;) {
		if (n->sheets.len) deepest = n;
		edesc *f = focus_of(n);
		n = f ? f->child : NULL;
	}
	if (deepest) {
		cut_to(&deepest->sheets, deepest->sheets.len - 1);
		apply(rt, d);
		changed(rt, -1);
	}
	ndesc_free(d);
	return deepest != NULL;
}

static void set_query(router_t *rt, ak_str *q) {
	ndesc *d = snapshot(rt->root);
	for (ndesc *n = d; n;) {
		edesc *f = routed_focus(n);
		if (!f || !f->member) break;
		if (f->member->leaf) {
			ak_retain(q);
			ak_release(f->query);
			f->query = q;
			break;
		}
		n = f->child;
	}
	apply(rt, d);
	ndesc_free(d);
	changed(rt, AK_LOCATION_ACTION_REPLACE);
}

// ---- The Router and Route elements ----

static ak_view *not_found_text(const char *s, size_t n, ak_color color) {
	ak_view *t = ak_ui_new(AK_EL_TEXT);
	ak_str *str = ak_str_new(s, n);
	ak_ui_set(t, AK_KEY_TEXT_TEXT, (ak_val){.p = str});
	ak_release(str);
	if (color) ak_ui_set(t, AK_KEY_COLOR, (ak_val){.i = (int64_t)color});
	return t;
}

// not_found_screen is the screen for an href no route takes: saying so,
// with a way home.
static ak_val not_found_screen(ak_closure *c, ak_val *a, uint32_t n) {
	AK_UNUSED(c);
	ak_view *col = ak_ui_new(AK_EL_SAFE_AREA);
	ak_ui_set(col, AK_KEY_GAP, (ak_val){.f = 12});
	ak_ui_set(col, AK_KEY_GROW, (ak_val){.f = 1});
	ak_ui_set(col, AK_KEY_ALIGN, (ak_val){.i = 2});    // center
	ak_ui_set(col, AK_KEY_JUSTIFY, (ak_val){.i = 1});  // center
	ak_view *t = not_found_text("This screen doesn't exist.", 26, 0);
	ak_ui_add(col, t);
	ak_release(t);
	if (n > 0 && a[0].p) {
		ak_val pv = ak_vm_acc_get(a[0].p);
		const ak_struct *p = pv.p;
		ak_val path = {0};
		ak_str *k = ak_str_c("path");
		if (p) ak_map_get(p->f[1].p, (ak_val){.p = k}, &path);
		ak_release(k);
		const ak_str *ps = path.p;
		ak_buf b = {0};
		ak_buf_u8(&b, '/');
		if (ps) ak_buf_put(&b, ps->data, ps->len);
		t = not_found_text((const char *)b.data, b.len, ak_semantic_color(AK_SEMANTIC_COLOR_SECONDARY_LABEL));
		ak_buf_free(&b);
		ak_ui_add(col, t);
		ak_release(t);
		ak_release(pv.p);
	}
	ak_view *home = ak_ui_new(AK_EL_BUTTON);
	ak_str *s = ak_str_c("Go home");
	ak_ui_set(home, AK_KEY_BUTTON_TITLE, (ak_val){.p = s});
	ak_release(s);
	s = ak_str_c("/");
	ak_ui_set(home, AK_KEY_BUTTON_HREF, (ak_val){.p = s});
	ak_release(s);
	ak_ui_add(col, home);
	ak_release(home);
	return (ak_val){.p = col};
}

static void route_init(ak_view *e) { e->impl = ak_alloc(sizeof(route_t)); }

// route_set passes a change of the Route's title, icon or badge (a
// binding's: an unread count) on to the tabs made of it.
static void route_set(ak_view *e, uint16_t key, ak_val v, const ak_val *prev) {
	AK_UNUSED(prev);
	uint16_t to;
	switch (key) {
	case AK_KEY_ROUTE_TITLE: to = AK_KEY_TAB_TITLE; break;
	case AK_KEY_ROUTE_ICON: to = AK_KEY_TAB_ICON; break;
	case AK_KEY_ROUTE_BADGE: to = AK_KEY_TAB_BADGE; break;
	default: return;
	}
	route_t *r = e->impl;
	uint32_t kept = 0;
	for (uint32_t i = 0; i < r->tabs.len; i++) {
		ak_view *t = r->tabs.data[i];
		if (t->dead) {
			ak_release(t);
			continue;
		}
		ak_ui_set(t, to, v);
		r->tabs.data[kept++] = t;
	}
	r->tabs.len = kept;
}

static void route_drop(ak_view *e) {
	route_t *r = e->impl;
	if (!r) return;
	for (uint32_t i = 0; i < r->kids.len; i++) ak_release(r->kids.data[i]);
	ak_vec_free(&r->kids);
	for (uint32_t i = 0; i < r->tabs.len; i++) ak_release(r->tabs.data[i]);
	ak_vec_free(&r->tabs);
	ak_release(r->build);
	ak_free(r);
}

static void route_add(ak_view *e, ak_view *child) {
	route_t *r = e->impl;
	if (child->el != AK_EL_ROUTE) ak_panic("ui: a Route's children are Routes (its screen is its content's builder)");
	ak_view_finalize(child);
	ak_vec_push(&r->kids, (ak_view *)ak_retain(child));
}

static void route_lazy(ak_view *e, ak_closure *build) {
	route_t *r = e->impl;
	ak_retain(build);
	ak_release(r->build);
	r->build = build;
}

const ak_el_class ak_class_route = {.name = "Route", .kind = AK_KIND_VIEW, .flags = AK_ELF_FRAGMENT | AK_ELF_FLATTEN | AK_ELF_CHILDREN,
                                    .init = route_init, .set = route_set, .add = route_add, .lazy = route_lazy, .drop = route_drop};

static void router_dispose(void *ctx) {
	ak_view *e = ctx;
	router_t *rt = e->impl;
	rt->dead = true;
	for (uint32_t i = 0; i < routers.len; i++)
		if (routers.data[i] == e) {
			memmove(routers.data + i, routers.data + i + 1, (routers.len - i - 1) * sizeof(ak_view *));
			routers.len--;
			break;
		}
	if (!routers.len) ak_vec_free(&routers);
	if (rt->root) {
		inst *in = rt->root;
		rt->root = NULL;
		in->dying = true;
		ak_owner *o = rt->owner;
		rt->owner = NULL;
		if (o) ak_owner_dispose(o);
		inst_free(in);
	}
	ak_release(e);
}

static void router_init(ak_view *e) {
	router_t *rt = ak_alloc(sizeof *rt);
	e->impl = rt;
	rt->e = e;
	ak_on_cleanup(router_dispose, ak_retain(e));
}

static void router_drop(ak_view *e) {
	router_t *rt = e->impl;
	if (!rt) return;
	if (rt->root) inst_free(rt->root);
	if (rt->owner) ak_owner_dispose(rt->owner);
	for (uint32_t i = 0; i < rt->routes.len; i++) ak_release(rt->routes.data[i]);
	ak_vec_free(&rt->routes);
	for (uint32_t i = 0; i < rt->all.len; i++) rnode_free(rt->all.data[i]);
	ak_vec_free(&rt->all);
	ak_vec_free(&rt->leaves);
	ndesc_free(rt->last);
	ak_release(rt->last_href);
	ak_signal_drop(&rt->href_node);
	ak_free(rt);
}

static void router_add(ak_view *e, ak_view *child) {
	router_t *rt = e->impl;
	if (child->el != AK_EL_ROUTE) ak_panic("ui: a Router's children are Routes");
	ak_view_finalize(child);
	ak_vec_push(&rt->routes, (ak_view *)ak_retain(child));
}

static ak_str *start_href;  // ak_router_set_start's

void ak_router_set_start(ak_span href) {
	ak_release(start_href);
	start_href = href.len ? ak_str_new(href.p, href.len) : NULL;
}

static void start_fn(void *ctx) {
	router_t *rt = ctx;
	ak_view *e = rt->e;
	const ak_val *iv = key_of(e, AK_KEY_ROUTER_INITIAL);
	ak_str *h = iv && iv->p && ((ak_str *)iv->p)->len ? ak_retain(iv->p) : ak_str_c("/");
	if (rt->addressed && rt->win == e->r->el && start_href) {  // the host's start is the main window's
		ak_release(h);
		h = ak_retain(start_href);
	}
	target t = {0};
	if (!resolve(rt, h, &t)) {
		ak_release(h);
		return;
	}
	ak_release(h);
	rlinks c = {0};
	chain_of(&c, t.leaf);
	ndesc *d = fresh(rt, rt->tree, c.data, c.len, &t);
	ak_vec_free(&c);
	target_free(&t);
	rt->root = new_inst(rt, rt->tree, rt->owner);
	ak_view *v = nav_view(rt, rt->root, d);
	ndesc_free(d);
	rt->shown = v;
	ak_view_append(e, v);
	ak_release(v);
}

// guard_run asks the guards of the screen shown again, tracked: as what
// they read changes (the user signs out), one that sends the user away
// replaces the screen.
static bool go_back(router_t *rt);

static void guard_run(void *ctx) {
	router_t *rt = ctx;
	if (rt->dead || !rt->started || !rt->last_href) return;
	ak_signal_read(&rt->href_node);
	ak_str *h = ak_retain(rt->last_href);
	target t = {0};
	match_href(rt, h, &t);
	ak_str *to = NULL;
	for (uint32_t i = 0; t.leaf && i < t.leaf->guards.len && !to; i++) {
		ak_val arg = {.p = h};
		ak_val v = ak_call(t.leaf->guards.data[i], &arg, 1);
		if (ak_error_pending()) {
			ak_ui_raise_pending(NULL);
			break;
		}
		ak_str *s = v.p;
		if (s && s->len && !ak_str_eq(s, h)) to = s;
		else ak_release(s);
	}
	target_free(&t);
	if (to) {
		void *tok = ak_untrack_begin();
		// The screen it guards goes, as back would take it; then where the
		// guard sends the user.
		go_back(rt);
		navigate(rt, to, MODE_NAVIGATE);
		ak_untrack_end(tok);
		ak_release(to);
	}
	ak_release(h);
}

static const ak_comp_info guard_info = {.label = "Router.guards"};

static void guard_effect(void *ctx) { ak_effect_new(guard_run, ctx, AK_EFFECT_USER, &guard_info); }

static void router_finalize(ak_view *e) {
	router_t *rt = e->impl;
	compile(rt);
	ak_ui_root *r = e->r;
	// Each window's first Router has its address (the main window's: the
	// root's view; a Window's: its own, which the host keeps by its key).
	rt->win = ak_window_under(e->owner);  // by its context: it isn't in its window's views yet
	rt->addressed = rt->win != NULL;
	for (uint32_t i = 0; i < routers.len; i++) {
		router_t *o = routers.data[i]->impl;
		if (!o->dead && o->addressed && o->win == rt->win) rt->addressed = false;
	}
	ak_vec_push(&routers, e);
	ak_ui_route_url = ak_router_incoming;  // the URLs the app is opened with (share.c)
	rt->owner = ak_root_under(e->owner);
	ak_owner_set_context(rt->owner, ak_router_key, e);
	ak_ui_root *pr = ak_ui.cur;
	ak_ui.cur = r;
	ak_with_owner(rt->owner, start_fn, rt);
	ak_ui.cur = pr;
	rt->started = true;
	rt->last = snapshot(rt->root);
	rt->last_href = desc_href(rt->last);
	ak_signal_write(&rt->href_node);  // what its screens read as they were built
	if (rt->addressed) ak_view_listen(rt->win, AK_EVENT_LOCATION);
	send(rt, AK_LOCATION_ACTION_REPLACE, 0);
	ak_with_owner(rt->owner, guard_effect, rt);
}

const ak_el_class ak_class_router = {.name = "Router", .kind = AK_KIND_VIEW, .flags = AK_ELF_FRAGMENT | AK_ELF_FLATTEN | AK_ELF_CHILDREN,
                                     .init = router_init, .finalize = router_finalize, .add = router_add, .drop = router_drop};

// ---- The Router of the calling code ----

static void lookup_fn(void *ctx) {
	void **out = ctx;
	void *v = NULL;
	if (ak_context_lookup(ak_router_key, &v)) *out = v;
}

// router_for is the Router code belongs to: of the owner given (a view's),
// else of the current owner, else the main window's first.
static router_t *router_for(ak_owner *o) {
	void *v = NULL;
	if (o && !ak_owner_disposed(o)) ak_with_owner(o, lookup_fn, &v);
	else if (!ak_context_lookup(ak_router_key, &v)) v = NULL;
	if (!v) {
		for (uint32_t i = 0; i < routers.len && !v; i++) {
			router_t *o = routers.data[i]->impl;
			if (o->addressed && o->win == routers.data[i]->r->el) v = routers.data[i];  // the main window's
		}
		if (!v && routers.len) v = routers.data[0];
	}
	if (!v) return NULL;
	router_t *rt = ((ak_view *)v)->impl;
	return rt->dead ? NULL : rt;
}

void ak_router_href(ak_view *from, ak_str *href) {
	router_t *rt = router_for(from ? from->owner : NULL);
	if (!rt) {
		ak_throw_msg("href %.*s: no Router", (int)href->len, href->data);
		ak_ui_raise_pending(from ? from->owner : NULL);
		return;
	}
	navigate(rt, href, MODE_NAVIGATE);
}

bool ak_router_incoming(ak_span url) {
	ak_str *h = to_href(url);
	if (!h) return false;
	// The first Router that takes it, the main window's first.
	for (uint32_t i = 0; i < routers.len; i++) {
		router_t *rt = routers.data[i]->impl;
		if (rt->dead) continue;
		target t = {0};
		match_href(rt, h, &t);
		bool takes = t.leaf != rt->not_found;
		target_free(&t);
		if (takes) {
			navigate(rt, h, MODE_NAVIGATE);
			ak_release(h);
			return true;
		}
	}
	ak_release(h);
	return false;
}

// ak_router_location is the user going back or forward in a browser's
// history, to an entry a Router made, or a host restoring a window's
// route: win is the window's view (the root's, or a Window's).
void ak_router_location(ak_view *win, const ak_event *ev) {
	ak_span href, state;
	if (!ak_decode_location_event((const uint8_t *)ev->s.p, ev->s.len, &href, &state)) return;
	for (uint32_t i = 0; i < routers.len; i++) {
		router_t *rt = routers.data[i]->impl;
		if (rt->dead || !rt->addressed || rt->win != win) continue;
		ak_str *h = ak_str_new(href.p, href.len);
		navigate(rt, h, MODE_NAVIGATE);
		ak_release(h);
		return;
	}
}

static router_t *need(const char *what) {
	router_t *rt = router_for(NULL);
	if (!rt) ak_throw_msg("%s: no Router", what);
	return rt;
}

// The builtins' (ui.navigate and friends).
void ak_ui_navigate(ak_str *href, int64_t mode) {
	router_t *rt = need("navigate");
	if (rt) navigate(rt, href, (int)mode);
}

bool ak_ui_router_back(void) {
	router_t *rt = need("back");
	return rt && go_back(rt);
}

bool ak_ui_router_dismiss(void) {
	router_t *rt = need("dismiss");
	return rt && dismiss_sheet(rt);
}

// ak_ui_can_go_back is read as the address is: a binding that reads it
// follows each change of the history.
bool ak_ui_can_go_back(void) {
	router_t *rt = need("canGoBack");
	if (!rt) return false;
	ak_signal_read(&rt->href_node);
	ndesc *d = snapshot(rt->root);
	bool ok = back_desc(d);
	ndesc_free(d);
	return ok;
}

void ak_ui_set_query(ak_str *query) {
	router_t *rt = need("setQuery");
	if (rt) set_query(rt, query);
}

// ak_ui_router_href is the address shown, read: a binding that reads it
// follows it.
ak_str *ak_ui_router_href(void) {
	router_t *rt = need("href");
	if (!rt) return NULL;
	ak_signal_read(&rt->href_node);
	return ak_retain(rt->last_href);
}
