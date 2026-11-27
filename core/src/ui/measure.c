// measure.c: the shared measure cache. Nodes showing the
// same thing (recycled list rows, repeated labels) ask the host once: a
// measurement is keyed by the kind, the text or title, the variant, the
// font, the line limit, the font scale, the scale, whether it's inset
// and the constraints. Two generations bound it: when the current one is
// full it becomes the old one, and hits in the old one move back. Its
// tables are made once: looking up and storing allocate nothing.
#include "ui/ui_int.h"

static uint32_t mix(uint32_t h, uint32_t v) {
	h ^= v;
	h *= 0x01000193u;
	return h;
}

static uint32_t str_hash(ak_str *s) { return s && s->len ? ak_str_hash(s) : 0; }

uint32_t ak_mkey_hash(const ak_mkey *k) {
	uint32_t h = 0x811c9dc5u;
	h = mix(h, k->kind);
	h = mix(h, (uint32_t)k->variant);
	h = mix(h, str_hash(k->text));
	h = mix(h, str_hash(k->text2));
	h = mix(h, str_hash(k->family));
	h = mix(h, (uint32_t)k->font_style);
	h = mix(h, (uint32_t)k->font_weight);
	uint32_t u;
	memcpy(&u, &k->font_size, 4);
	h = mix(h, u);
	memcpy(&u, &k->line_height, 4);
	h = mix(h, u);
	h = mix(h, (uint32_t)k->lines);
	memcpy(&u, &k->font_scale, 4);
	h = mix(h, u);
	memcpy(&u, &k->scale, 4);
	h = mix(h, u);
	h = mix(h, k->inset);
	for (int i = 0; i < 4; i++) h = mix(h, k->c[i]);
	// mix's multiply leaves each low bit of h to the same low bits of
	// what it mixed: the table's index (its low bits) would ignore the
	// high bits of the sizes (whole points have none low), and keys would
	// cluster. Murmur3's finalizer spreads every bit over all of them.
	h ^= h >> 16;
	h *= 0x85ebca6bu;
	h ^= h >> 13;
	h *= 0xc2b2ae35u;
	h ^= h >> 16;
	return h ? h : 1;
}

static bool str_same(const ak_str *a, const ak_str *b) {
	if (a == b) return true;
	if (!a || !b) return (!a || a->len == 0) && (!b || b->len == 0);
	return ak_str_eq(a, b);
}

static bool key_eq(const ak_mkey *a, const ak_mkey *b) {
	return a->kind == b->kind && a->variant == b->variant && a->font_style == b->font_style && a->font_weight == b->font_weight &&
	       memcmp(&a->font_size, &b->font_size, 4) == 0 && memcmp(&a->line_height, &b->line_height, 4) == 0 && a->lines == b->lines && memcmp(&a->font_scale, &b->font_scale, 4) == 0 &&
	       memcmp(&a->scale, &b->scale, 4) == 0 && a->inset == b->inset && memcmp(a->c, b->c, sizeof a->c) == 0 &&
	       str_same(a->text, b->text) && str_same(a->text2, b->text2) && str_same(a->family, b->family);
}

void ak_mcache_init(ak_mcache *m) {
	m->cur = ak_alloc(AK_MCACHE_SLOTS * sizeof(ak_mentry));
	m->old = ak_alloc(AK_MCACHE_SLOTS * sizeof(ak_mentry));
	m->ncur = m->nold = 0;
}

static void clear_table(ak_mentry *t) {
	for (uint32_t i = 0; i < AK_MCACHE_SLOTS; i++) {
		if (!t[i].hash) continue;
		ak_release(t[i].k.text);
		ak_release(t[i].k.text2);
		ak_release(t[i].k.family);
		t[i].hash = 0;
	}
}

void ak_mcache_clear(ak_mcache *m) {
	if (!m->cur) return;
	clear_table(m->cur);
	clear_table(m->old);
	m->ncur = m->nold = 0;
}

void ak_mcache_free(ak_mcache *m) {
	ak_mcache_clear(m);
	ak_free(m->cur);
	ak_free(m->old);
	m->cur = m->old = NULL;
}

static ak_mentry *find(ak_mentry *t, const ak_mkey *k, uint32_t hash) {
	for (uint32_t i = hash & (AK_MCACHE_SLOTS - 1);; i = (i + 1) & (AK_MCACHE_SLOTS - 1)) {
		if (!t[i].hash) return &t[i];  // where it would go
		if (t[i].hash == hash && key_eq(&t[i].k, k)) return &t[i];
	}
}

bool ak_mcache_get(ak_mcache *m, const ak_mkey *k, uint32_t hash, float *w, float *h) {
	ak_mentry *e = find(m->cur, k, hash);
	if (e->hash) {
		*w = e->w, *h = e->h;
		m->hits++;
		return true;
	}
	e = find(m->old, k, hash);
	if (e->hash) {
		*w = e->w, *h = e->h;
		m->hits++;
		ak_mcache_put(m, k, hash, e->w, e->h);
		return true;
	}
	return false;
}

void ak_mcache_put(ak_mcache *m, const ak_mkey *k, uint32_t hash, float w, float h) {
	if (m->ncur >= AK_MCACHE_MAX) {
		// The current generation is full: it becomes the old one.
		clear_table(m->old);
		ak_mentry *t = m->old;
		m->old = m->cur;
		m->cur = t;
		m->nold = m->ncur;
		m->ncur = 0;
	}
	ak_mentry *e = find(m->cur, k, hash);
	if (e->hash) {
		e->w = w, e->h = h;
		return;
	}
	e->k = *k;
	ak_retain(e->k.text);
	ak_retain(e->k.text2);
	ak_retain(e->k.family);
	e->hash = hash;
	e->w = w, e->h = h;
	m->ncur++;
}
