// layout_test.c: the layout's table tests and the Chrome fixtures
// (testdata/css: Taffy's, and the left-to-right and right-to-left cases
// beyond them), read from testdata/layout_fixtures.txt (go run
// core/test/testdata/css/gen_fixtures.go). testdata/css/known_failures.txt
// lists the fixtures the layout doesn't match yet; go test ./core -run
// TestC/Layout -update-known rewrites it.
#define _POSIX_C_SOURCE 200809L  // strdup under -std=c17
#include "t.h"
#include "base/base.h"
#include "layout/layout.h"

static bool isdef(float f) { return f == f; }
static bool definite(float f) { return f == f && !isinf(f); }

static ak_lnode *box(ak_lstyle s, ak_lnode *const *kids, uint32_t n) {
	ak_lnode *b = ak_lnode_new();
	*ak_lnode_style(b) = s;
	ak_lnode_set_children(b, kids, n);
	return b;
}

#define KIDS(...) (ak_lnode *[]){__VA_ARGS__}, (uint32_t)(sizeof((ak_lnode *[]){__VA_ARGS__}) / sizeof(ak_lnode *))

static ak_lnode *colv(ak_lnode *const *kids, uint32_t n) {
	ak_lstyle s = ak_lstyle_default();
	s.flex_direction = AK_LCOLUMN;
	return box(s, kids, n);
}

static ak_lnode *rowv(ak_lnode *const *kids, uint32_t n) { return box(ak_lstyle_default(), kids, n); }

#define col(...) colv(KIDS(__VA_ARGS__))
#define row(...) rowv(KIDS(__VA_ARGS__))

// label measures like text: 8pt per character, 20pt lines, wrapping.
static ak_lsize label_measure(void *ctx, ak_lconstraints c) {
	int chars = *(int *)ctx;
	float w = (float)(chars * 8);
	if (isdef(c.width)) w = c.width;
	else if (definite(c.avail_w)) w = w < c.avail_w ? w : c.avail_w;
	else if (c.avail_w == AK_LMIN_CONTENT) w = 8;
	int lines = 1;
	if (w > 0) {
		int per = (int)(w / 8);
		int q = (chars + per - 1) / (per > 1 ? per : 1);
		lines = q > 1 ? q : 1;
	}
	return (ak_lsize){w, (float)(lines * 20)};
}

static ak_lnode *label(int *chars) {
	ak_lnode *n = ak_lnode_new();
	ak_lnode_set_measure(n, label_measure, chars);
	return n;
}

static ak_lnode *sized(float w, float h) {
	ak_lnode *n = ak_lnode_new();
	ak_lnode_style(n)->width = ak_lpt(w);
	ak_lnode_style(n)->height = ak_lpt(h);
	return n;
}

static char frames_buf[1024];

static const char *frames(ak_lnode *const *ns, uint32_t count) {
	size_t off = 0;
	frames_buf[0] = 0;
	for (uint32_t i = 0; i < count; i++) {
		ak_lframe f = ak_lnode_frame(ns[i]);
		off += (size_t)snprintf(frames_buf + off, sizeof frames_buf - off, "%s%g,%g %gx%g", i ? " | " : "", f.x, f.y, f.w, f.h);
	}
	return frames_buf;
}

#define FRAMES(...) frames(KIDS(__VA_ARGS__))

static void screen(ak_lnode *n, float w, float h, ak_lpass *p) {
	ak_lnode_style(n)->width = ak_lpct(100);
	ak_lnode_style(n)->height = ak_lpct(100);
	ak_lnode_layout(n, (ak_lsize){w, h}, p);
}

static bool same_frame(ak_lframe a, ak_lframe b) {
	return fabsf(a.x - b.x) < 1e-3f && fabsf(a.y - b.y) < 1e-3f && fabsf(a.w - b.w) < 1e-3f && fabsf(a.h - b.h) < 1e-3f;
}

TEST(screen_column) {
	int a = 5, b = 50;
	ak_lnode *ta = label(&a), *tb = label(&b);
	ak_lnode *spacer = ak_lnode_new();
	ak_lnode_style(spacer)->flex_grow = 1;
	ak_lnode *root = col(ta, spacer, tb);
	ak_lnode_style(root)->padding = ak_lrect_all(ak_lpt(10));
	ak_lpass p = {0};
	screen(root, 200, 400, &p);
	CHECK_EQ_STR(FRAMES(root, ta, spacer, tb), "0,0 200x400 | 10,10 180x20 | 10,30 180x300 | 10,330 180x60");
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

// A column sized to its content, one of its texts with a least width
// wider than its content's narrowest: the column is as wide as that,
// and its texts' heights are the ones at that width (the text in 2
// lines, the title in 1), not at their own narrowest; laid out at that
// width after, it answers the same.
TEST(a_column_at_its_min_content_width_lays_its_texts_out_at_it) {
	int a = 9, b = 50;  // b: 400pt on one line, 2 lines at 200pt, 50 at its narrowest
	ak_lnode *title = label(&a), *t = label(&b);
	ak_lnode_style(t)->min_width = ak_lpt(220);
	ak_lnode_style(t)->padding = ak_lrect_all(ak_lpt(10));
	ak_lnode *root = col(title, t);
	ak_lpass p = {0};
	ak_lsize s = ak_lnode_layout(root, (ak_lsize){AK_LMIN_CONTENT, AK_LMAX_CONTENT}, &p);
	CHECK_EQ_STR(FRAMES(root, title, t), "0,0 220x80 | 0,0 220x20 | 0,20 220x60");
	CHECK(s.w == 220 && s.h == 80);
	ak_lstyle st = *ak_lnode_style(root);
	st.width = ak_lpt(220);
	ak_lnode_set_style(root, &st);
	s = ak_lnode_layout(root, (ak_lsize){220, AK_LMAX_CONTENT}, &p);
	CHECK_EQ_STR(FRAMES(root, title, t), "0,0 220x80 | 0,0 220x20 | 0,20 220x60");
	CHECK(s.h == 80);
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

TEST(relayout_without_changes_does_nothing) {
	int n[20];
	ak_lnode *kids[20];
	for (int i = 0; i < 20; i++) {
		n[i] = 5 + i;
		kids[i] = row(label(&n[i]), label(&n[i]));
	}
	ak_lnode *root = colv(kids, 20);
	ak_lpass p = {0};
	screen(root, 320, 640, &p);
	REQUIRE(p.measures != 0 && p.nchanged != 0);
	ak_lpass_reset(&p);
	screen(root, 320, 640, &p);
	CHECK_EQ_INT(p.measures, 0);
	CHECK_EQ_INT(p.nchanged, 0);
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

TEST(changing_one_text_measures_it) {
	int n[100];
	ak_lnode *kids[100];
	for (int i = 0; i < 100; i++) {
		n[i] = 3;
		kids[i] = label(&n[i]);
	}
	ak_lnode *root = colv(kids, 100);
	ak_lpass p = {0};
	screen(root, 320, 4000, &p);

	n[50] = 6;  // same height
	ak_lnode_mark_dirty(kids[50]);
	ak_lpass_reset(&p);
	screen(root, 320, 4000, &p);
	CHECK(p.measures <= 2);
	CHECK_EQ_INT(p.nchanged, 0);  // stretched: its frame didn't change

	n[50] = 100;  // two lines: it and what follows move
	ak_lnode_mark_dirty(kids[50]);
	ak_lpass_reset(&p);
	screen(root, 320, 4000, &p);
	CHECK_EQ_INT(p.nchanged, 50);
	CHECK(p.nchanged > 0 && p.changed[0] == kids[50]);
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

static const ak_ltrack_size fr1 = {{AK_LBREADTH_AUTO, 0}, {AK_LBREADTH_FR, 1}};
static const ak_lgrid_track repeat3fr = {3, &fr1, 1};

TEST(steady_state_layout_allocates_nothing) {
	int chars[50];
	ak_lnode *kids[52];
	for (int i = 0; i < 50; i++) {
		chars[i] = i % 7 + 1;
		ak_lnode *g = ak_lnode_new();
		ak_lnode_style(g)->flex_grow = 1;
		kids[i] = row(label(&chars[i]), g, label(&chars[i]));
	}
	ak_lnode *grid = ak_lnode_new();
	ak_lnode_style(grid)->display = AK_LDISPLAY_GRID;
	ak_lnode_style(grid)->grid_template_columns = &repeat3fr;
	ak_lnode_style(grid)->ngrid_template_columns = 1;
	for (int i = 0; i < 9; i++) ak_lnode_append_child(grid, label(&chars[i]));
	ak_lnode *blk = ak_lnode_new();
	ak_lnode_style(blk)->display = AK_LDISPLAY_BLOCK;
	ak_lnode_append_child(blk, label(&chars[0]));
	kids[50] = grid;
	kids[51] = blk;
	ak_lnode *root = colv(kids, 52);
	ak_lpass p = {0};
	screen(root, 320, 2000, &p);
	screen(root, 680, 2000, &p);

	// The same layout again.
	uint64_t before = ak_alloc_count;
	ak_lpass_reset(&p);
	screen(root, 680, 2000, &p);
	CHECK_EQ_INT(ak_alloc_count - before, 0);
	CHECK_EQ_INT(p.nchanged, 0);

	// One leaf changed.
	chars[10] = 40;
	ak_lnode_mark_dirty(ak_lnode_child(kids[10], 0));
	before = ak_alloc_count;
	ak_lpass_reset(&p);
	screen(root, 680, 2000, &p);
	CHECK_EQ_INT(ak_alloc_count - before, 0);
	CHECK(p.measures > 0);

	// Alternating widths, so everything recomputes.
	float w = 680;
	before = ak_alloc_count;
	for (int i = 0; i < 20; i++) {
		w = 1000 - w;
		ak_lpass_reset(&p);
		screen(root, w, 2000, &p);
	}
	CHECK_EQ_INT(ak_alloc_count - before, 0);
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

// direction_tree has a flex row, a column, a grid, a block and an
// absolute child, symmetric from side to side but for what runs from
// start to end.
static const ak_ltrack_size px20 = {{AK_LBREADTH_POINT, 20}, {AK_LBREADTH_POINT, 20}};
static const ak_ltrack_size px40 = {{AK_LBREADTH_POINT, 40}, {AK_LBREADTH_POINT, 40}};
static const ak_lgrid_track dir_cols[2] = {{0, &px20, 1}, {0, &px40, 1}};

static ak_lnode *direction_tree(ak_lnode *nodes[12]) {
	ak_lnode *a = sized(30, 10), *b = sized(50, 10);
	ak_lnode *r = row(a, b);
	ak_lnode_style(r)->column_gap = ak_lpt(4);

	ak_lnode *c0 = sized(20, 10);
	ak_lnode_style(c0)->align_self = AK_LALIGN_FLEX_END;
	ak_lnode *c1 = sized(20, 10);
	ak_lnode *c = col(c0, c1);
	ak_lnode_style(c)->padding = ak_lrect_all(ak_lpt(3));

	ak_lnode *g0 = ak_lnode_new(), *g1 = sized(10, 10), *g2 = ak_lnode_new();
	ak_lnode_style(g0)->height = ak_lpt(10);
	ak_lnode_style(g2)->height = ak_lpt(10);
	ak_lnode_style(g1)->justify_self = AK_LALIGN_END;
	ak_lnode *g = box(ak_lstyle_default(), KIDS(g0, g1, g2));
	ak_lnode_style(g)->display = AK_LDISPLAY_GRID;
	ak_lnode_style(g)->grid_template_columns = dir_cols;
	ak_lnode_style(g)->ngrid_template_columns = 2;

	ak_lnode *b0 = sized(40, 10);
	ak_lnode_style(b0)->display = AK_LDISPLAY_BLOCK;
	ak_lnode *bl = box(ak_lstyle_default(), KIDS(b0));
	ak_lnode_style(bl)->display = AK_LDISPLAY_BLOCK;

	ak_lnode *abs = sized(16, 16);
	ak_lnode_style(abs)->position = AK_LABSOLUTE;
	ak_lnode_style(abs)->inset.left = ak_lpt(5);

	ak_lnode *root = col(r, c, g, bl, abs);
	ak_lnode_style(root)->width = ak_lpt(300);
	ak_lnode_style(root)->height = ak_lpt(400);
	ak_lnode_style(root)->padding = ak_lrect_all(ak_lpt(8));
	ak_lnode *all[12] = {a, b, r, c0, c1, c, g0, g1, g2, g, b0, bl};
	memcpy(nodes, all, sizeof all);
	return root;
}

static void set_direction(ak_lnode *n, uint8_t d) {
	ak_lstyle s = *ak_lnode_style(n);
	s.direction = d;
	ak_lnode_set_style(n, &s);
}

TEST(flipping_direction_mirrors_from_clean_caches) {
	ak_lnode *nodes[12];
	ak_lnode *root = direction_tree(nodes);
	ak_lpass p = {0};
	ak_lnode_layout(root, (ak_lsize){300, 400}, &p);
	ak_lframe ltr[12];
	for (int i = 0; i < 12; i++) ltr[i] = ak_lnode_frame(nodes[i]);

	set_direction(root, AK_LDIR_RTL);
	REQUIRE(ak_lnode_dirty(nodes[0]) && ak_lnode_dirty(nodes[5]) && ak_lnode_dirty(nodes[10]));
	ak_lpass_reset(&p);
	ak_lnode_layout(root, (ak_lsize){300, 400}, &p);
	CHECK(p.nchanged > 0);
	for (int i = 0; i < 12; i++) {
		ak_lframe l = ltr[i];
		ak_lframe want = {ak_lnode_frame(ak_lnode_parent(nodes[i])).w - l.x - l.w, l.y, l.w, l.h};
		ak_lframe got = ak_lnode_frame(nodes[i]);
		if (!same_frame(got, want)) {
			T_FAIL("node %d: got %g,%g %gx%g, want %g,%g %gx%g", i, got.x, got.y, got.w, got.h, want.x, want.y, want.w, want.h);
		}
	}

	// As a tree laid out only right to left has it: nothing stale.
	ak_lnode *fresh_nodes[12];
	ak_lnode *fresh = direction_tree(fresh_nodes);
	ak_lnode_style(fresh)->direction = AK_LDIR_RTL;
	ak_lpass q = {0};
	ak_lnode_layout(fresh, (ak_lsize){300, 400}, &q);
	for (int i = 0; i < 12; i++) {
		if (!same_frame(ak_lnode_frame(nodes[i]), ak_lnode_frame(fresh_nodes[i]))) T_FAIL("node %d: flipped and fresh differ", i);
	}

	set_direction(root, AK_LDIR_LTR);
	ak_lnode_layout(root, (ak_lsize){300, 400}, &p);
	for (int i = 0; i < 12; i++) {
		if (!same_frame(ak_lnode_frame(nodes[i]), ltr[i])) T_FAIL("node %d back to left to right", i);
	}
	ak_lnode_free_tree(root);
	ak_lnode_free_tree(fresh);
	ak_lpass_free(&p);
	ak_lpass_free(&q);
}

TEST(explicit_direction_inside_another) {
	ak_lnode *a = sized(10, 10), *b = sized(10, 10);
	ak_lnode *inner = row(a, b);
	ak_lnode_style(inner)->direction = AK_LDIR_LTR;
	ak_lnode_style(inner)->width = ak_lpt(150);
	ak_lnode *deep_a = sized(10, 10), *deep_b = sized(10, 10);
	ak_lnode *deep = row(deep_a, deep_b);
	ak_lnode_style(deep)->direction = AK_LDIR_RTL;
	ak_lnode_style(deep)->width = ak_lpt(50);
	ak_lnode *inherits = sized(10, 10);
	ak_lnode *mid = row(inherits, deep);
	ak_lnode_style(mid)->width = ak_lpt(100);
	ak_lnode_append_child(inner, mid);
	ak_lnode *root = row(inner);
	ak_lnode_style(root)->direction = AK_LDIR_RTL;
	ak_lnode_style(root)->width = ak_lpt(200);
	ak_lpass p = {0};
	ak_lnode_layout(root, (ak_lsize){200, 100}, &p);
	CHECK_EQ_F(ak_lnode_frame(inner).x, 50);  // placed by its right-to-left parent
	CHECK_EQ_F(ak_lnode_frame(a).x, 0);       // its own children run left to right
	CHECK_EQ_F(ak_lnode_frame(b).x, 10);
	CHECK_EQ_F(ak_lnode_frame(inherits).x, 0);  // inherited from inner
	CHECK_EQ_F(ak_lnode_frame(deep).x, 10);
	CHECK_EQ_F(ak_lnode_frame(deep_a).x, 40);  // right to left again
	CHECK_EQ_F(ak_lnode_frame(deep_b).x, 30);
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

TEST(direction_follows_a_new_parent) {
	ak_lnode *a = sized(10, 10), *b = sized(10, 10);
	ak_lnode *moved = row(a, b);  // inherits
	ak_lnode_style(moved)->width = ak_lpt(50);
	ak_lnode_style(moved)->height = ak_lpt(10);
	ak_lnode *rtl = row(moved);
	ak_lnode_style(rtl)->direction = AK_LDIR_RTL;
	ak_lnode *ltr = rowv(NULL, 0);
	ak_lnode_style(ltr)->direction = AK_LDIR_LTR;
	ak_lpass pr = {0}, pl = {0};
	ak_lnode_layout(rtl, (ak_lsize){100, 100}, &pr);
	CHECK_EQ_F(ak_lnode_frame(a).x, 40);
	CHECK_EQ_F(ak_lnode_frame(b).x, 30);
	ak_lnode_layout(ltr, (ak_lsize){100, 100}, &pl);

	ak_lnode_append_child(ltr, moved);
	ak_lpass_reset(&pr);
	ak_lpass_reset(&pl);
	ak_lnode_layout(ltr, (ak_lsize){100, 100}, &pl);
	CHECK_EQ_F(ak_lnode_frame(a).x, 0);  // moved left to right
	CHECK_EQ_F(ak_lnode_frame(b).x, 10);
	ak_lnode_append_child(rtl, moved);
	ak_lpass_reset(&pl);
	ak_lnode_layout(rtl, (ak_lsize){100, 100}, &pl);
	CHECK_EQ_F(ak_lnode_frame(a).x, 40);  // moved back
	CHECK_EQ_F(ak_lnode_frame(b).x, 30);

	// Laid out by itself, with no parent's direction to read.
	ak_lnode_remove_child(rtl, moved);
	ak_lnode_layout(moved, (ak_lsize){50, 10}, &pl);
	CHECK_EQ_F(ak_lnode_frame(a).x, 0);
	CHECK_EQ_F(ak_lnode_frame(b).x, 10);
	ak_lnode_free_tree(rtl);
	ak_lnode_free_tree(ltr);
	ak_lnode_free_tree(moved);
	ak_lpass_free(&pr);
	ak_lpass_free(&pl);
}

TEST(layout_root_with_parent_resolves_direction) {
	ak_lnode *c = sized(10, 10);
	ak_lnode *inner = row(c);
	ak_lnode_style(inner)->width = ak_lpt(100);
	ak_lnode *mid = col(inner);
	ak_lnode *top = col(mid);
	ak_lnode_style(top)->direction = AK_LDIR_RTL;  // never laid out itself
	ak_lpass p = {0};
	ak_lnode_layout(inner, (ak_lsize){100, 100}, &p);
	CHECK_EQ_F(ak_lnode_frame(c).x, 90);
	ak_lnode_free_tree(top);
	ak_lpass_free(&p);
}

TEST(style_write_of_direction_does_not_answer_from_cache) {
	ak_lnode *c = sized(10, 10);
	ak_lnode *inner = row(c);
	ak_lnode *root = col(inner);
	ak_lnode_style(root)->width = ak_lpt(100);
	ak_lpass p = {0};
	ak_lnode_layout(root, (ak_lsize){100, 100}, &p);
	ak_lnode_style(root)->direction = AK_LDIR_RTL;
	ak_lnode_mark_dirty(root);  // inner and c stay clean: their caches say nothing of it
	ak_lnode_layout(root, (ak_lsize){100, 100}, &p);
	CHECK_EQ_F(ak_lnode_frame(c).x, 90);
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

TEST(same_direction_keeps_caches) {
	int n = 5;
	ak_lnode *leaf = label(&n);
	ak_lnode *root = row(leaf);
	ak_lnode_style(root)->direction = AK_LDIR_RTL;
	ak_lpass p = {0};
	ak_lnode_layout(root, (ak_lsize){200, 100}, &p);
	REQUIRE(p.measures != 0);
	ak_lstyle s = *ak_lnode_style(root);
	s.padding.left = ak_lpt(0);  // a change, but not of direction
	ak_lnode_set_style(root, &s);
	CHECK(!ak_lnode_dirty(leaf));
	ak_lpass_reset(&p);
	ak_lnode_layout(root, (ak_lsize){200, 100}, &p);
	CHECK_EQ_INT(p.measures, 0);
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

TEST(steady_state_rtl_layout_allocates_nothing) {
	int chars[50];
	ak_lnode *kids[53];
	for (int i = 0; i < 50; i++) {
		chars[i] = i % 7 + 1;
		ak_lnode *g = ak_lnode_new();
		ak_lnode_style(g)->flex_grow = 1;
		ak_lnode_style(g)->margin.left = AK_LAUTO;
		kids[i] = row(label(&chars[i]), g, label(&chars[i]));
	}
	ak_lnode *grid = ak_lnode_new();
	ak_lnode_style(grid)->display = AK_LDISPLAY_GRID;
	ak_lnode_style(grid)->grid_template_columns = &repeat3fr;
	ak_lnode_style(grid)->ngrid_template_columns = 1;
	ak_lnode_style(grid)->column_gap = ak_lpt(4);
	for (int i = 0; i < 9; i++) {
		ak_lnode *c = label(&chars[i]);
		ak_lnode_style(c)->justify_self = AK_LALIGN_SELF_END;
		ak_lnode_style(c)->direction = AK_LDIR_RTL;
		ak_lnode_append_child(grid, c);
	}
	ak_lnode *absolute = sized(10, 10);
	ak_lnode_style(absolute)->position = AK_LABSOLUTE;
	ak_lnode_style(absolute)->grid_column = (ak_lplacement){ak_lline(1), ak_lline(2)};
	ak_lnode_append_child(grid, absolute);
	ak_lnode *blk = ak_lnode_new();
	ak_lnode_style(blk)->display = AK_LDISPLAY_BLOCK;
	ak_lnode_style(blk)->overflow_y = AK_LOVERFLOW_SCROLL;
	ak_lnode_style(blk)->scrollbar_width = 15;
	ak_lnode_append_child(blk, label(&chars[0]));
	ak_lnode *column = col(label(&chars[1]), label(&chars[2]));
	ak_lnode_style(column)->align_items = AK_LALIGN_FLEX_END;
	ak_lnode_style(column)->flex_wrap = AK_LWRAP_REVERSE;
	kids[50] = grid;
	kids[51] = blk;
	kids[52] = column;
	ak_lnode *root = colv(kids, 53);
	ak_lnode_style(root)->direction = AK_LDIR_RTL;
	ak_lpass p = {0};
	screen(root, 320, 2000, &p);
	REQUIRE(ak_lnode_frame(ak_lnode_child(ak_lnode_child(root, 0), 0)).x > 0);  // laid out right to left
	screen(root, 680, 2000, &p);
	float w = 680;
	uint64_t before = ak_alloc_count;
	for (int i = 0; i < 20; i++) {
		w = 1000 - w;  // alternate widths so everything recomputes
		ak_lpass_reset(&p);
		screen(root, w, 2000, &p);
	}
	CHECK_EQ_INT(ak_alloc_count - before, 0);
	// Nor does changing direction.
	before = ak_alloc_count;
	for (int i = 0; i < 10; i++) {
		set_direction(root, ak_lnode_style(root)->direction == AK_LDIR_RTL ? AK_LDIR_LTR : AK_LDIR_RTL);
		ak_lpass_reset(&p);
		screen(root, w, 2000, &p);
	}
	CHECK_EQ_INT(ak_alloc_count - before, 0);
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

TEST(display_none_hides) {
	int c = 4;
	ak_lnode *a = label(&c), *b = label(&c);
	ak_lnode_style(b)->display = AK_LDISPLAY_NONE;
	ak_lnode *root = col(a, b);
	ak_lpass p = {0};
	screen(root, 100, 100, &p);
	CHECK_EQ_STR(FRAMES(a, b), "0,0 100x20 | 0,0 0x0");
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

TEST(tree_edits) {
	ak_lnode *a = ak_lnode_new(), *b = ak_lnode_new(), *c = ak_lnode_new();
	ak_lnode *p1 = row(a, b), *p2 = rowv(NULL, 0);
	ak_lnode_insert_child(p1, c, 1);
	CHECK(ak_lnode_child(p1, 0) == a && ak_lnode_child(p1, 1) == c && ak_lnode_child(p1, 2) == b);
	ak_lnode_insert_child(p1, a, 99);  // moves a to the end
	CHECK(ak_lnode_child(p1, 0) == c && ak_lnode_child(p1, 1) == b && ak_lnode_child(p1, 2) == a);
	ak_lnode_append_child(p2, b);  // taken from p1
	CHECK_EQ_INT(ak_lnode_child_count(p1), 2);
	CHECK(ak_lnode_parent(b) == p2);
	ak_lnode_set_children(p2, KIDS(a, c, b));
	CHECK_EQ_INT(ak_lnode_child_count(p1), 0);
	CHECK_EQ_INT(ak_lnode_child_count(p2), 3);
	CHECK(ak_lnode_parent(a) == p2 && ak_lnode_parent(c) == p2);
	uint32_t count;
	ak_lnode *const *kids = ak_lnode_children(p2, &count);
	ak_lnode_set_children(p2, kids + 1, count - 1);  // from its own array
	CHECK_EQ_INT(ak_lnode_child_count(p2), 2);
	CHECK(ak_lnode_child(p2, 0) == c && ak_lnode_child(p2, 1) == b);
	CHECK(ak_lnode_parent(a) == NULL);
	ak_lnode_free(c);  // leaves p2
	CHECK_EQ_INT(ak_lnode_child_count(p2), 1);
	ak_lnode_free(p2);  // orphans b
	CHECK(ak_lnode_parent(b) == NULL);
	ak_lnode_free(a);
	ak_lnode_free(b);
	ak_lnode_free(p1);
}

TEST(content_insets) {
	ak_lnode *c = ak_lnode_new();
	ak_lnode_style(c)->padding = (ak_lrect){ak_lpt(1), ak_lpct(10), ak_lpt(3), ak_lpt(4)};
	ak_lnode_style(c)->border.left = ak_lpt(2);
	ak_lnode *plain = ak_lnode_new();
	ak_lnode *root = row(c, plain);
	ak_lnode_style(root)->padding = ak_lrect_all(ak_lpt(10));
	ak_lnode_style(root)->border = ak_lrect_all(ak_lpt(5));
	ak_lnode_style(root)->width = ak_lpt(230);
	ak_lpass p = {0};
	ak_lnode_layout(root, (ak_lsize){230, 100}, &p);
	ak_ledges r = ak_lnode_content_insets(root);
	CHECK(r.top == 15 && r.right == 15 && r.bottom == 15 && r.left == 15);
	ak_ledges e = ak_lnode_content_insets(c);  // 10% of the root's 200pt content width
	CHECK(e.top == 1 && e.right == 20 && e.bottom == 3 && e.left == 6);
	ak_ledges z = ak_lnode_content_insets(plain);
	CHECK(z.top == 0 && z.right == 0 && z.bottom == 0 && z.left == 0);
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

TEST(force_report) {
	ak_lnode *a = sized(10, 10), *b = sized(10, 10);
	ak_lnode *root = row(a, b);
	ak_lpass p = {0};
	ak_lnode_layout(root, (ak_lsize){100, 100}, &p);
	CHECK_EQ_INT(p.nchanged, 3);
	ak_lnode_force_report(b);
	ak_lnode_mark_dirty(root);
	ak_lpass_reset(&p);
	ak_lnode_layout(root, (ak_lsize){100, 100}, &p);
	REQUIRE(p.nchanged == 1);  // its frame is the same, but reported again
	CHECK(p.changed[0] == b);
	ak_lpass_reset(&p);
	ak_lnode_mark_dirty(root);
	ak_lnode_layout(root, (ak_lsize){100, 100}, &p);
	CHECK_EQ_INT(p.nchanged, 0);
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

TEST(set_style_allocates_nothing) {
	ak_lnode *g = ak_lnode_new();
	ak_lstyle s = *ak_lnode_style(g);
	s.display = AK_LDISPLAY_GRID;
	s.grid_template_columns = dir_cols;
	s.ngrid_template_columns = 2;
	uint64_t before = ak_alloc_count;
	for (int i = 0; i < 10; i++) {
		s.column_gap = ak_lpt((float)i);
		s.direction = i % 2 ? AK_LDIR_RTL : AK_LDIR_LTR;
		ak_lnode_set_style(g, &s);
	}
	CHECK_EQ_INT(ak_alloc_count - before, 0);
	ak_lnode_free(g);
}

// The Chrome fixtures.

typedef struct {
	char *p;
} scanner;

#define BAD(...) do { fprintf(stderr, "layout fixtures: " __VA_ARGS__); fputc('\n', stderr); abort(); } while (0)

static char *next_token(scanner *s) {
	for (;;) {
		while (*s->p == ' ' || *s->p == '\n' || *s->p == '\r' || *s->p == '\t') s->p++;
		if (*s->p != '#') break;
		while (*s->p && *s->p != '\n') s->p++;
	}
	if (!*s->p) return NULL;
	char *t = s->p;
	while (*s->p && *s->p != ' ' && *s->p != '\n' && *s->p != '\r' && *s->p != '\t') s->p++;
	if (*s->p) *s->p++ = 0;
	return t;
}

static float tok_f(scanner *s) {
	char *t = next_token(s);
	if (!t) {
		fprintf(stderr, "layout fixtures: unexpected end\n");
		abort();
	}
	return strtof(t, NULL);
}

static int tok_i(scanner *s) {
	char *t = next_token(s);
	if (!t) {
		fprintf(stderr, "layout fixtures: unexpected end\n");
		abort();
	}
	return (int)strtol(t, NULL, 10);
}

static void expect_tag(scanner *s, const char *tag) {
	char *t = next_token(s);
	if (!t || strcmp(t, tag) != 0) {
		fprintf(stderr, "layout fixtures: want %s, got %s\n", tag, t ? t : "(end)");
		abort();
	}
}

#define NFIXED 77

// What a fixture's nodes own: their grid arrays and words.
typedef struct {
	int nwords;
	int *words;
	int minw, maxw;
	ak_lgrid_track *tpl[2];
	ak_ltrack_size *sizes;
	ak_ltrack_size *autos[2];
	float shadow[4];  // incremental_relayout's: its frame, as layouts reported it
} fnode;

// ahem measures text in the Ahem font: 10×10 glyphs, breaking only at
// zero-width spaces.
static ak_lsize ahem(void *ctx, ak_lconstraints c) {
	const fnode *f = ctx;
	float inl = c.width;
	if (inl != inl) {
		if (c.avail_w == AK_LMIN_CONTENT) inl = (float)(f->minw * 10);
		else if (c.avail_w == AK_LMAX_CONTENT) inl = (float)(f->maxw * 10);
		else inl = c.avail_w < (float)(f->maxw * 10) ? c.avail_w : (float)(f->maxw * 10);
	}
	if (!(inl > (float)(f->minw * 10))) inl = (float)(f->minw * 10);
	float block = c.height;
	if (block != block) {
		int per_line = (int)floor((double)(inl / 10));
		int lines = 1, cur = 0;
		for (int i = 0; i < f->nwords; i++) {
			int n = f->words[i];
			if (cur + n > per_line) {
				if (cur > 0) lines++;
				cur = n;
			} else {
				cur += n;
			}
		}
		block = (float)(lines * 10);
	}
	return (ak_lsize){inl, block};
}

static float resolve0_at0(ak_llength l) {
	if (l.u == AK_LUNIT_POINT) return l.v;
	if (l.u == AK_LUNIT_PERCENT) {
		float v = l.v * 0 / 100;
		return v != v ? 0 : v;
	}
	return 0;
}

static float ahem_baseline(void *ctx, ak_lsize s) {
	ak_lnode *n = ctx;
	return resolve0_at0(ak_lnode_style(n)->padding.top) + resolve0_at0(ak_lnode_style(n)->border.top) + 8;
}

static ak_llength flen(const float *v, int i) { return (ak_llength){v[i], (uint8_t)v[i + 1]}; }

static ak_lrect frect(const float *v, int i) { return (ak_lrect){flen(v, i), flen(v, i + 2), flen(v, i + 4), flen(v, i + 6)}; }

static ak_lgrid_line fline(const float *v, int i) { return (ak_lgrid_line){(uint8_t)v[i], (int16_t)v[i + 1]}; }

static ak_ltrack_size read_track_size(scanner *s) {
	ak_ltrack_size t;
	t.min.kind = (uint8_t)tok_i(s);
	t.min.v = tok_f(s);
	t.max.kind = (uint8_t)tok_i(s);
	t.max.v = tok_f(s);
	return t;
}

static ak_lnode *read_node(scanner *s, const float *defaults) {
	expect_tag(s, "N");
	int nkids = tok_i(s);
	float v[NFIXED];
	memcpy(v, defaults, sizeof v);
	int ndiff = tok_i(s);
	for (int i = 0; i < ndiff; i++) {
		int k = tok_i(s);
		float x = tok_f(s);
		if (k < 0 || k >= NFIXED) BAD("field %d", k);
		v[k] = x;
	}
	ak_lstyle st = ak_lstyle_default();
	st.display = (uint8_t)v[0];
	st.position = (uint8_t)v[1];
	st.box_sizing = (uint8_t)v[2];
	st.overflow_x = (uint8_t)v[3];
	st.overflow_y = (uint8_t)v[4];
	st.scrollbar_width = v[5];
	st.width = flen(v, 6);
	st.height = flen(v, 8);
	st.min_width = flen(v, 10);
	st.min_height = flen(v, 12);
	st.max_width = flen(v, 14);
	st.max_height = flen(v, 16);
	st.aspect_ratio = v[18];
	st.margin = frect(v, 19);
	st.padding = frect(v, 27);
	st.border = frect(v, 35);
	st.inset = frect(v, 43);
	st.column_gap = flen(v, 51);
	st.row_gap = flen(v, 53);
	st.align_items = (uint8_t)v[55];
	st.align_self = (uint8_t)v[56];
	st.justify_items = (uint8_t)v[57];
	st.justify_self = (uint8_t)v[58];
	st.align_content = (uint8_t)v[59];
	st.justify_content = (uint8_t)v[60];
	st.direction = (uint8_t)v[61];
	st.flex_direction = (uint8_t)v[62];
	st.flex_wrap = (uint8_t)v[63];
	st.flex_grow = v[64];
	st.flex_shrink = v[65];
	st.flex_basis = flen(v, 66);
	st.grid_auto_flow = (uint8_t)v[68];
	st.grid_column = (ak_lplacement){fline(v, 69), fline(v, 71)};
	st.grid_row = (ak_lplacement){fline(v, 73), fline(v, 75)};

	fnode *f = calloc(1, sizeof *f);
	// The templates: their sizes in one array, pointed into once read.
	int cap = 16, nsizes = 0;
	f->sizes = malloc(sizeof(ak_ltrack_size) * (size_t)cap);
	int ntpl[2];
	for (int a = 0; a < 2; a++) {
		ntpl[a] = tok_i(s);
		f->tpl[a] = calloc((size_t)ntpl[a] + 1, sizeof(ak_lgrid_track));
		for (int i = 0; i < ntpl[a]; i++) {
			f->tpl[a][i].count = tok_i(s);
			f->tpl[a][i].nsizes = (uint32_t)tok_i(s);
			f->tpl[a][i].sizes = (const ak_ltrack_size *)(intptr_t)nsizes;  // an index until the array stops moving
			for (uint32_t j = 0; j < f->tpl[a][i].nsizes; j++) {
				if (nsizes == cap) {
					cap *= 2;
					f->sizes = realloc(f->sizes, sizeof(ak_ltrack_size) * (size_t)cap);
				}
				f->sizes[nsizes++] = read_track_size(s);
			}
		}
	}
	for (int a = 0; a < 2; a++) {
		for (int i = 0; i < ntpl[a]; i++) f->tpl[a][i].sizes = f->sizes + (intptr_t)f->tpl[a][i].sizes;
	}
	st.grid_template_columns = f->tpl[0];
	st.ngrid_template_columns = (uint32_t)ntpl[0];
	st.grid_template_rows = f->tpl[1];
	st.ngrid_template_rows = (uint32_t)ntpl[1];
	for (int a = 0; a < 2; a++) {
		int n = tok_i(s);
		f->autos[a] = calloc((size_t)n + 1, sizeof(ak_ltrack_size));
		for (int i = 0; i < n; i++) f->autos[a][i] = read_track_size(s);
		if (a == 0) {
			st.grid_auto_columns = f->autos[0];
			st.ngrid_auto_columns = (uint32_t)n;
		} else {
			st.grid_auto_rows = f->autos[1];
			st.ngrid_auto_rows = (uint32_t)n;
		}
	}
	f->nwords = tok_i(s);
	f->words = calloc((size_t)f->nwords + 1, sizeof(int));
	for (int i = 0; i < f->nwords; i++) {
		f->words[i] = tok_i(s);
		if (f->words[i] > f->minw) f->minw = f->words[i];
		f->maxw += f->words[i];
	}

	ak_lnode *n = ak_lnode_new();
	*ak_lnode_style(n) = st;
	ak_lnode_set_context(n, f);
	if (nkids > 0) {
		ak_lnode **kids = calloc((size_t)nkids, sizeof(ak_lnode *));
		for (int i = 0; i < nkids; i++) kids[i] = read_node(s, defaults);
		ak_lnode_set_children(n, kids, (uint32_t)nkids);
		free(kids);
	} else if (f->nwords > 0) {
		ak_lnode_set_measure(n, ahem, f);
		ak_lnode_set_baseline(n, ahem_baseline, n);
	}
	return n;
}

static void free_fixture(ak_lnode *n) {
	for (uint32_t i = 0; i < ak_lnode_child_count(n); i++) free_fixture(ak_lnode_child(n, i));
	fnode *f = ak_lnode_context(n);
	free(f->words);
	free(f->sizes);
	free(f->tpl[0]);
	free(f->tpl[1]);
	free(f->autos[0]);
	free(f->autos[1]);
	free(f);
}

typedef struct {
	int nkids;
	float f[4];
} frame_rec;

static int read_frames(scanner *s, const char *tag, frame_rec *out, int cap, int k) {
	expect_tag(s, tag);
	if (k >= cap) BAD("too many frames");
	int i = k++;
	out[i].nkids = tok_i(s);
	for (int j = 0; j < 4; j++) out[i].f[j] = tok_f(s);
	for (int c = 0; c < out[i].nkids; c++) k = read_frames(s, tag, out, cap, k);
	return k;
}

// matches compares n's subtree to Chrome's frames from *k on: with a
// tolerance of 0.1, over the nodes both have.
static bool matches(ak_lnode *n, const frame_rec *e, int *k, char *diff, size_t ndiff) {
	const frame_rec *r = &e[(*k)++];
	ak_lframe g = ak_lnode_frame(n);
	float got[4] = {g.x, g.y, g.w, g.h};
	bool ok = true;
	for (int j = 0; j < 4; j++) {
		if (!(fabsf(r->f[j] - got[j]) < 0.1f)) {
			if (diff[0] == 0) {
				snprintf(diff, ndiff, "got %g,%g %gx%g, want %g,%g %gx%g", got[0], got[1], got[2], got[3], r->f[0], r->f[1], r->f[2], r->f[3]);
			}
			ok = false;
			break;
		}
	}
	for (int c = 0; c < r->nkids; c++) {
		if ((uint32_t)c < ak_lnode_child_count(n)) {
			if (!matches(ak_lnode_child(n, (uint32_t)c), e, k, diff, ndiff)) ok = false;
		} else {
			// Skip the subtree the node hasn't.
			int skip = 1;
			while (skip > 0) skip += e[(*k)++].nkids - 1;
		}
	}
	return ok;
}

// record writes n's subtree's frames, in preorder, from *k on.
static void record(ak_lnode *n, frame_rec *out, int *k) {
	frame_rec *r = &out[(*k)++];
	ak_lframe f = ak_lnode_frame(n);
	r->nkids = (int)ak_lnode_child_count(n);
	r->f[0] = f.x, r->f[1] = f.y, r->f[2] = f.w, r->f[3] = f.h;
	for (uint32_t c = 0; c < ak_lnode_child_count(n); c++) record(ak_lnode_child(n, c), out, k);
}

// same_frames compares every frame of n's subtree to those record wrote.
static bool same_frames(ak_lnode *n, const frame_rec *g, int *k, char *diff, size_t ndiff) {
	const frame_rec *r = &g[(*k)++];
	ak_lframe f = ak_lnode_frame(n);
	float got[4] = {f.x, f.y, f.w, f.h};
	bool ok = (uint32_t)r->nkids == ak_lnode_child_count(n);
	for (int j = 0; j < 4 && ok; j++) {
		if (!(fabsf(r->f[j] - got[j]) < 0.01f || (r->f[j] != r->f[j] && got[j] != got[j]))) {
			snprintf(diff, ndiff, "got %g,%g %gx%g, the first layout %g,%g %gx%g", got[0], got[1], got[2], got[3], r->f[0], r->f[1], r->f[2], r->f[3]);
			ok = false;
		}
	}
	if (!ok) return false;
	for (uint32_t c = 0; c < ak_lnode_child_count(n); c++) {
		if (!same_frames(ak_lnode_child(n, c), g, k, diff, ndiff)) return false;
	}
	return true;
}

static ak_lnode *last_node(ak_lnode *n) {
	while (ak_lnode_child_count(n) > 0) n = ak_lnode_child(n, ak_lnode_child_count(n) - 1);
	return n;
}

static char *read_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = malloc((size_t)n + 1);
	size_t got = fread(buf, 1, (size_t)n, f);
	buf[got] = 0;
	fclose(f);
	return buf;
}

// testdata reads a file of core/test/testdata, from the test's directory
// or, failing that, ASUKA_ROOT's.
static char *testdata(const char *name) {
	char path[4096];
	snprintf(path, sizeof path, "testdata/%s", name);
	char *data = read_file(path);
	if (!data && getenv("ASUKA_ROOT")) {
		snprintf(path, sizeof path, "%s/core/test/testdata/%s", getenv("ASUKA_ROOT"), name);
		data = read_file(path);
	}
	return data;
}

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

#define KNOWN_FAILURES "css/known_failures.txt"
#define MAX_FIXTURES 4096
#define MAX_FRAMES 4096

TEST(css_fixtures) {
	char *data = testdata("layout_fixtures.txt");
	REQUIRE(data != NULL);
	// The known failures: a name a line, sorted for bsearch; # comments.
	char *known_data = testdata(KNOWN_FAILURES);
	REQUIRE(known_data != NULL);
	char **known = calloc(MAX_FIXTURES, sizeof(char *));
	int nknown = 0;
	for (char *line = strtok(known_data, "\r\n"); line; line = strtok(NULL, "\r\n")) {
		if (*line && *line != '#' && nknown < MAX_FIXTURES) known[nknown++] = line;
	}
	qsort(known, (size_t)nknown, sizeof(char *), cmp_str);
	bool update = getenv("ASUKA_UPDATE_KNOWN") != NULL;
	char **failing = calloc(MAX_FIXTURES, sizeof(char *));
	int nfailing = 0;

	scanner s = {data};
	expect_tag(&s, "D");
	REQUIRE(tok_i(&s) == NFIXED);
	float defaults[NFIXED];
	for (int i = 0; i < NFIXED; i++) defaults[i] = tok_f(&s);

	frame_rec *want = calloc(MAX_FRAMES, sizeof(frame_rec));
	frame_rec *first = calloc(MAX_FRAMES, sizeof(frame_rec));
	struct {
		char name[32];
		int pass, total;
	} groups[16];
	int ngroups = 0;
	int total = 0, pass = 0, regressions = 0, fixed = 0, cache_fails = 0;
	ak_lpass p = {0};
	char *t;
	while ((t = next_token(&s)) != NULL) {
		if (strcmp(t, "F") != 0) {
			T_FAIL("layout fixtures: want F, got %s", t);
			break;
		}
		const char *name = next_token(&s);
		bool rounding = tok_i(&s) != 0;
		float vw = tok_f(&s), vh = tok_f(&s);
		ak_lnode *root = read_node(&s, defaults);
		read_frames(&s, "E", want, MAX_FRAMES, 0);
		expect_tag(&s, ".");

		ak_lpass_reset(&p);
		ak_lnode_layout(root, (ak_lsize){vw, vh}, &p);
		if (rounding) ak_lround(root, 1);
		char diff[256] = "";
		int k = 0;
		bool ok = matches(root, want, &k, diff, sizeof diff);
		k = 0;
		record(root, first, &k);

		total++;
		pass += ok;
		const char *slash = strchr(name, '/');
		size_t glen = slash ? (size_t)(slash - name) : strlen(name);
		int gi = 0;
		while (gi < ngroups && (strlen(groups[gi].name) != glen || strncmp(groups[gi].name, name, glen) != 0)) gi++;
		if (gi == ngroups && ngroups < 16) {
			memset(&groups[gi], 0, sizeof groups[gi]);
			memcpy(groups[gi].name, name, glen < 31 ? glen : 31);
			ngroups++;
		}
		if (gi < ngroups) {
			groups[gi].total++;
			groups[gi].pass += ok;
		}
		bool is_known = bsearch(&name, known, (size_t)nknown, sizeof(char *), cmp_str) != NULL;
		if (!ok && nfailing < MAX_FIXTURES) failing[nfailing++] = (char *)name;
		if (!update) {
			if (!ok && !is_known) {
				regressions++;
				T_FAIL("%s: doesn't match Chrome (not in " KNOWN_FAILURES "): %s", name, diff);
			} else if (ok && is_known) {
				fixed++;
				T_FAIL("%s: matches Chrome now: remove it from " KNOWN_FAILURES " (-update-known)", name);
			} else if (!ok && getenv("T_VERBOSE")) {
				fprintf(stderr, "  known failure: %s: %s\n", name, diff);
			}
		}

		// Again, unchanged: answered from the final caches, reporting
		// nothing and allocating nothing.
		uint64_t before = ak_alloc_count;
		ak_lpass_reset(&p);
		ak_lnode_layout(root, (ak_lsize){vw, vh}, &p);
		if (p.nchanged != 0 || p.measures != 0 || ak_alloc_count != before) {
			cache_fails++;
			T_FAIL("%s: unchanged relayout: %u changed, %llu measures, %llu allocations", name, p.nchanged,
				(unsigned long long)p.measures, (unsigned long long)(ak_alloc_count - before));
		}
		// The last node invalidated: its ancestors recompute, the rest
		// answers from its caches; then everything from clean caches. Both
		// give the first layout's frames, from the pass's warm scratch
		// memory.
		for (int round = 0; round < 2; round++) {
			if (round == 0) ak_lnode_mark_dirty(last_node(root));
			else ak_lnode_mark_subtree_dirty(root);
			before = ak_alloc_count;
			ak_lpass_reset(&p);
			ak_lnode_layout(root, (ak_lsize){vw, vh}, &p);
			uint64_t allocs = ak_alloc_count - before;
			if (rounding) ak_lround(root, 1);
			k = 0;
			char rdiff[256] = "";
			if (!same_frames(root, first, &k, rdiff, sizeof rdiff) || allocs != 0) {
				cache_fails++;
				T_FAIL("%s: relayout %s: %llu allocations; %s", name, round == 0 ? "of the last node" : "from clean caches",
					(unsigned long long)allocs, rdiff);
				break;
			}
		}
		free_fixture(root);
		ak_lnode_free_tree(root);
	}
	fprintf(stderr, "CSS fixtures: %d/%d match Chrome (", pass, total);
	for (int i = 0; i < ngroups; i++) fprintf(stderr, "%s%s %d/%d", i ? ", " : "", groups[i].name, groups[i].pass, groups[i].total);
	fprintf(stderr, "); %d known failures\n", nfailing);
	if (update) {
		// The fixture names live in data, which is still allocated.
		qsort(failing, (size_t)nfailing, sizeof(char *), cmp_str);
		const char *path = "testdata/" KNOWN_FAILURES;
		FILE *f = fopen(path, "wb");
		REQUIRE(f != NULL);
		fputs("# Fixtures the core's layout doesn't match yet. go test ./core -run TestC/Layout -update-known rewrites this.\n", f);
		for (int i = 0; i < nfailing; i++) fprintf(f, "%s\n", failing[i]);
		fclose(f);
		fprintf(stderr, "wrote %s: %d fixtures\n", path, nfailing);
	}
	CHECK(total > 1900);
	CHECK_EQ_INT(regressions, 0);
	CHECK_EQ_INT(fixed, 0);
	CHECK_EQ_INT(cache_fails, 0);
	ak_lpass_free(&p);
	free(want);
	free(first);
	free(known);
	free(failing);
	free(known_data);
	free(data);
}

// A repeat() of more tracks than a grid has lines makes as many as it
// has (browsers' limit), however many it asks for, at once.
TEST(grid_repeat_beyond_its_lines) {
	static const ak_ltrack_size px10 = {{AK_LBREADTH_POINT, 10}, {AK_LBREADTH_POINT, 10}};
	static const ak_ltrack_size three[3] = {px10, px10, px10};
	const ak_lgrid_track tpls[2][2] = {
		{{800000000, three, 3}},
		{{INT32_MAX, three, 3}, {INT32_MAX, three, 3}},
	};
	const uint32_t ntpls[2] = {1, 2};
	for (int i = 0; i < 2; i++) {
		ak_lnode *grid = ak_lnode_new();
		ak_lnode_style(grid)->display = AK_LDISPLAY_GRID;
		ak_lnode_style(grid)->grid_template_columns = tpls[i];
		ak_lnode_style(grid)->ngrid_template_columns = ntpls[i];
		ak_lnode *last = sized(5, 5);
		ak_lnode_style(last)->grid_column = (ak_lplacement){ak_lline(-2), ak_lline(-1)};
		ak_lnode_append_child(grid, last);
		ak_lnode *root = col(grid);
		ak_lpass p = {0};
		screen(root, 320, 480, &p);
		CHECK_EQ_F(ak_lnode_frame(last).x, 99990);  // the 10000th track's
		ak_lnode_free_tree(root);
		ak_lpass_free(&p);
	}
}


// Relayout boundaries (ak_lnode_mark_content_dirty): every fixture's tree,
// each leaf showing text, changed at random (its text, a node's style, a
// child moved, the space it's laid out in) and laid out three ways: with
// relayout boundaries; with each change dropping its ancestors' caches
// (ak_lnode_mark_dirty, what every change did before); and from clean
// caches. Where dropping the ancestors' caches gives clean caches'
// frames, relayout boundaries do too (the caches' looser matches, which
// both keep, can differ from clean caches' in a few corners); and every
// layout reports each frame that changed.

static uint64_t rng_state;
static uint32_t rnd(uint32_t n) {
	rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
	return n ? (uint32_t)((rng_state >> 33) % n) : 0;
}

static void preorder(ak_lnode *n, ak_lnode **out, int *k, int cap) {
	if (*k < cap) out[(*k)++] = n;
	for (uint32_t i = 0; i < ak_lnode_child_count(n); i++) preorder(ak_lnode_child(n, i), out, k, cap);
}

// set_words gives f's text count words of the given lengths.
static void set_words(fnode *f, const int *lens, int count) {
	free(f->words);
	f->words = calloc((size_t)count + 1, sizeof(int));
	f->nwords = count;
	f->minw = f->maxw = 0;
	for (int i = 0; i < count; i++) {
		f->words[i] = lens[i];
		if (lens[i] > f->minw) f->minw = lens[i];
		f->maxw += lens[i];
	}
}

static void record_reported(const ak_lpass *p) {
	for (uint32_t i = 0; i < p->nchanged; i++) {
		ak_lframe fr = ak_lnode_frame(p->changed[i]);
		fnode *f = ak_lnode_context(p->changed[i]);
		f->shadow[0] = fr.x, f->shadow[1] = fr.y, f->shadow[2] = fr.w, f->shadow[3] = fr.h;
	}
}

static bool same_or_nan(ak_lframe x, ak_lframe y) { return same_frame(x, y) || (x.w != x.w && y.w != y.w); }

// all_reported: each node's frame is what the layouts reported of it.
static bool all_reported(ak_lnode **ns, int n, int *bad) {
	for (int k = 0; k < n; k++) {
		const fnode *f = ak_lnode_context(ns[k]);
		if (!same_or_nan(ak_lnode_frame(ns[k]), (ak_lframe){f->shadow[0], f->shadow[1], f->shadow[2], f->shadow[3]})) {
			*bad = k;
			return false;
		}
	}
	return true;
}

#define DIFF_NODES 512

TEST(incremental_relayout_matches_a_fresh_one) {
	char *data = testdata("layout_fixtures.txt");
	REQUIRE(data != NULL);
	char *texts[3] = {data, strdup(data), strdup(data)};  // the scanner writes into its text
	scanner s[3] = {{texts[0]}, {texts[1]}, {texts[2]}};
	float defaults[NFIXED];
	for (int t = 0; t < 3; t++) {
		expect_tag(&s[t], "D");
		REQUIRE(tok_i(&s[t]) == NFIXED);
		for (int i = 0; i < NFIXED; i++) defaults[i] = tok_f(&s[t]);
	}
	frame_rec *skip = calloc(MAX_FRAMES, sizeof(frame_rec));
	ak_lnode **ns[3];
	for (int t = 0; t < 3; t++) ns[t] = calloc(DIFF_NODES, sizeof(ak_lnode *));
	ak_lpass ps[3] = {{0}, {0}, {0}};
	int trees = 0, steps = 0, fails = 0, boundaries = 0, old_off = 0;
	while (next_token(&s[0]) != NULL && fails < 5) {
		const char *name = next_token(&s[0]);
		tok_i(&s[0]);
		float vw = tok_f(&s[0]), vh = tok_f(&s[0]);
		ak_lnode *root[3];
		root[0] = read_node(&s[0], defaults);
		for (int t = 1; t < 3; t++) {
			next_token(&s[t]), next_token(&s[t]), tok_i(&s[t]), tok_f(&s[t]), tok_f(&s[t]);
			root[t] = read_node(&s[t], defaults);
		}
		int n = 0;
		for (int t = 0; t < 3; t++) {
			read_frames(&s[t], "E", skip, MAX_FRAMES, 0);
			expect_tag(&s[t], ".");
			n = 0;
			preorder(root[t], ns[t], &n, DIFF_NODES);
		}
		trees++;
		rng_state = (uint64_t)trees * 0x9e3779b97f4a7c15ULL;
		// Every leaf shows text.
		for (int i = 0; i < n; i++) {
			if (ak_lnode_child_count(ns[0][i]) > 0) continue;
			for (int t = 0; t < 3; t++) {
				ak_lnode_set_measure(ns[t][i], ahem, ak_lnode_context(ns[t][i]));
				ak_lnode_set_baseline(ns[t][i], ahem_baseline, ns[t][i]);
			}
		}
		for (int t = 0; t < 3; t++) {
			ak_lpass_reset(&ps[t]);
			ak_lnode_layout(root[t], (ak_lsize){vw, vh}, &ps[t]);
			record_reported(&ps[t]);
		}
		for (int step = 0; step < 5 && fails < 5; step++) {
			steps++;
			uint32_t what = rnd(10);
			int i = (int)rnd((uint32_t)n);
			if (what < 6) {
				// A leaf's text.
				int lens[6], count = (int)rnd(6);
				for (int w = 0; w < count; w++) lens[w] = 1 + (int)rnd(8);
				int leaf = i;
				while (leaf < n && ak_lnode_child_count(ns[0][leaf]) > 0) leaf++;
				if (leaf == n) continue;
				for (int t = 0; t < 3; t++) set_words(ak_lnode_context(ns[t][leaf]), lens, count);
				ak_lnode_mark_content_dirty(ns[0][leaf]);
				ak_lnode_mark_dirty(ns[1][leaf]);
			} else if (what == 6) {
				// A node's style.
				float w = rnd(3) == 0 ? -1 : (float)(10 * rnd(30));
				float grow = (float)rnd(2);
				for (int t = 0; t < 3; t++) {
					ak_lnode_style(ns[t][i])->width = w < 0 ? AK_LAUTO : ak_lpt(w);
					ak_lnode_style(ns[t][i])->flex_grow = grow;
				}
				ak_lnode_mark_dirty(ns[0][i]);
				ak_lnode_mark_dirty(ns[1][i]);
			} else if (what == 7) {
				// A child moved within its parent.
				uint32_t nk = ak_lnode_child_count(ns[0][i]);
				if (nk == 0) continue;
				uint32_t from = rnd(nk), to = rnd(nk);
				for (int t = 0; t < 3; t++) {
					ak_lnode *x = ns[t][i], *c = ak_lnode_child(x, from);
					ak_lnode_remove_child(x, c);
					ak_lnode_insert_child(x, c, to);
					if (t == 1) ak_lnode_mark_dirty(x);
				}
				for (int t = 0; t < 3; t++) {
					n = 0;
					preorder(root[t], ns[t], &n, DIFF_NODES);
				}
			} else {
				// The space it's laid out in.
				vw = vw + (float)rnd(41) - 20;
				if (vw < 0) vw = 0;
			}
			ak_lnode_mark_subtree_dirty(root[2]);
			uint64_t before = 0;
			for (int t = 0; t < 3; t++) {
				ak_lpass_reset(&ps[t]);
				if (t == 0) before = ak_lnode_computed;
				ak_lnode_layout(root[t], (ak_lsize){vw, vh}, &ps[t]);
				if (t == 0 && what < 6 && ak_lnode_computed - before < (uint64_t)n) boundaries++;
				record_reported(&ps[t]);
			}
			int bad = -1;
			if (!all_reported(ns[0], n, &bad)) {
				ak_lframe x = ak_lnode_frame(ns[0][bad]);
				T_FAIL("%s, step %d (change %u at node %d): node %d's frame %g,%g %gx%g wasn't reported", name, step, what, i, bad, x.x, x.y, x.w, x.h);
				fails++;
				break;
			}
			bool old_right = true;
			for (int k = 0; k < n && old_right; k++) old_right = same_or_nan(ak_lnode_frame(ns[1][k]), ak_lnode_frame(ns[2][k]));
			if (!old_right) {
				// The caches' looser matches: start again from clean caches.
				old_off++;
				for (int t = 0; t < 2; t++) {
					ak_lnode_mark_subtree_dirty(root[t]);
					ak_lpass_reset(&ps[t]);
					ak_lnode_layout(root[t], (ak_lsize){vw, vh}, &ps[t]);
					record_reported(&ps[t]);
				}
				continue;
			}
			for (int k = 0; k < n; k++) {
				ak_lframe x = ak_lnode_frame(ns[0][k]), y = ak_lnode_frame(ns[2][k]);
				if (!same_or_nan(x, y)) {
					T_FAIL("%s, step %d (change %u at node %d): node %d: %g,%g %gx%g; from clean caches %g,%g %gx%g", name, step, what, i, k, x.x, x.y, x.w,
						x.h, y.x, y.y, y.w, y.h);
					fails++;
					break;
				}
			}
		}
		for (int t = 0; t < 3; t++) {
			free_fixture(root[t]);
			ak_lnode_free_tree(root[t]);
		}
	}
	fprintf(stderr, "incremental relayout: %d trees, %d changes, %d text changes stopped below the root, %d where the old way differs from clean caches\n",
		trees, steps, boundaries, old_off);
	CHECK(trees > 1900);
	CHECK(boundaries > 500);
	for (int t = 0; t < 3; t++) {
		ak_lpass_free(&ps[t]);
		free(ns[t]);
		free(texts[t]);
	}
	free(skip);
}

// A text that changes in a long list re-lays out its row, not the list:
// what the list's other rows answered stands.
TEST(a_text_in_a_long_list_relays_out_its_row) {
	int n[1000];
	ak_lnode *rows[1000], *texts[1000];
	for (int i = 0; i < 1000; i++) {
		n[i] = 5;
		texts[i] = label(&n[i]);
		ak_lnode_style(texts[i])->flex_grow = 1;
		ak_lnode *icon = sized(40, 20);
		ak_lnode_style(icon)->flex_shrink = 0;
		rows[i] = row(icon, texts[i]);
	}
	ak_lnode *root = colv(rows, 1000);
	ak_lpass p = {0};
	screen(root, 320, 20000, &p);
	n[500] = 7;  // wider, the same height
	ak_lnode_mark_content_dirty(texts[500]);
	CHECK(ak_lnode_dirty(root));
	ak_lpass_reset(&p);
	uint64_t before = ak_lnode_computed;
	screen(root, 320, 20000, &p);
	uint64_t computed = ak_lnode_computed - before;
	CHECK(computed <= 8);  // the text's answers, its row's: none of the other rows'
	CHECK_EQ_INT(p.nchanged, 0);
	CHECK(!ak_lnode_dirty(root));
	n[500] = 100;  // taller: the rows below move
	ak_lnode_mark_content_dirty(texts[500]);
	ak_lpass_reset(&p);
	screen(root, 320, 20000, &p);
	CHECK_EQ_INT(p.nchanged, 1 + 1 + 499);  // the text, its row, the rows below
	for (uint32_t i = 0; i < p.nchanged; i++) {
		ak_lnode *c = p.changed[i];
		bool known = c == texts[500] || c == rows[500];
		for (int r = 501; r < 1000 && !known; r++) known = c == rows[r];
		if (!known) T_FAIL("reported %p (%g wide)", (void *)c, (double)ak_lnode_frame(c).w);
	}
	ak_lnode_free_tree(root);
	ak_lpass_free(&p);
}

T_MAIN()
