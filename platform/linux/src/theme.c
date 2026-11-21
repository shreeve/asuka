// theme.c: libadwaita's look. Colors are its named colors (window_bg_color,
// card_bg_color, accent_color…), read from the theme for the current
// style; asuka.toml's accent is the theme's accent, through a style sheet
// of the host's. The text styles are libadwaita's type classes relative
// to the interface font (gtk-font-name). The app's fonts are fontconfig's
// application fonts.
#include "host.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

static GdkRGBA hex(uint32_t v) {
	return (GdkRGBA){(float)(v >> 16 & 0xff) / 255, (float)(v >> 8 & 0xff) / 255, (float)(v & 0xff) / 255, 1};
}

// over is c composited over an opaque bg.
static GdkRGBA over(GdkRGBA c, GdkRGBA bg) {
	float a = c.alpha;
	return (GdkRGBA){c.red * a + bg.red * (1 - a), c.green * a + bg.green * (1 - a), c.blue * a + bg.blue * (1 - a), 1};
}

static GdkRGBA with_alpha(GdkRGBA c, float a) {
	c.alpha *= a;
	return c;
}

// default_palette is libadwaita 1.5's own, for when the theme names none.
static palette default_palette(bool dark) {
	palette p;
	if (dark) {
		p.fg = hex(0xffffff);
		p.bg = hex(0x242424);
		p.view = hex(0x1e1e1e);
		p.card = over((GdkRGBA){1, 1, 1, 0.08f}, p.bg);
		p.header = hex(0x303030);
		p.accent = hex(0x78aeed);
		p.red = hex(0xff7b63);
		p.orange = hex(0xffa348);
		p.yellow = hex(0xf8e45c);
		p.green = hex(0x8ff0a4);
		p.blue = hex(0x99c1f1);
		p.purple = hex(0xdc8add);
		p.pink = hex(0xf5a3cf);
		p.gray = hex(0x9a9996);
	} else {
		p.fg = (GdkRGBA){0, 0, 0, 0.8f};
		p.bg = hex(0xfafafa);
		p.view = hex(0xffffff);
		p.card = hex(0xffffff);
		p.header = hex(0xebebeb);
		p.accent = hex(0x1c71d8);
		p.red = hex(0xc01c28);
		p.orange = hex(0xe66100);
		p.yellow = hex(0xc88800);
		p.green = hex(0x26a269);
		p.blue = hex(0x1c71d8);
		p.purple = hex(0x813d9c);
		p.pink = hex(0xc4317c);
		p.gray = hex(0x77767b);
	}
	p.accent_bg = hex(0x3584e4);
	p.on_accent = hex(0xffffff);
	return p;
}

// app_accent is the app's accent in the current style (0: the theme's).
static ak_color app_accent(void) { return H.dark && H.accent_dark ? H.accent_dark : H.accent; }

// apply_accent puts the app's accent in the palette (asuka.toml's, as on
// every platform).
static void apply_accent(palette *p) {
	uint8_t r, g, b, a;
	if (!ak_color_channels(app_accent(), &r, &g, &b, &a)) return;
	GdkRGBA c = {(float)r / 255, (float)g / 255, (float)b / 255, 1};
	p->accent = p->accent_bg = c;
	if (0.299f * r + 0.587f * g + 0.114f * b > 160)
		p->on_accent = (GdkRGBA){0, 0, 0, 0.8f};
	else
		p->on_accent = hex(0xffffff);
}

static void look(GtkStyleContext *ctx, const char *name, GdkRGBA *dst) {
	GdkRGBA c;
	if (gtk_style_context_lookup_color(ctx, name, &c)) *dst = c;
}

// lx_read_palette reads the theme's named colors through a widget's
// style.
void lx_read_palette(GtkWidget *w) {
	palette p = default_palette(H.dark);
	if (w && gtk_style_context_lookup_color) {
		GtkStyleContext *ctx = gtk_widget_get_style_context(w);
		look(ctx, "window_fg_color", &p.fg);
		look(ctx, "window_bg_color", &p.bg);
		look(ctx, "view_bg_color", &p.view);
		look(ctx, "card_bg_color", &p.card);
		if (p.card.alpha < 1) p.card = over(p.card, p.bg);
		look(ctx, "headerbar_bg_color", &p.header);
		look(ctx, "accent_color", &p.accent);
		look(ctx, "accent_bg_color", &p.accent_bg);
		look(ctx, "accent_fg_color", &p.on_accent);
		look(ctx, "destructive_color", &p.red);
		look(ctx, "success_color", &p.green);
		look(ctx, "warning_color", &p.yellow);
	}
	apply_accent(&p);
	H.theme = p;
}

static bool semantic(uint16_t s, GdkRGBA *out) {
	palette *p = &H.theme;
	switch (s) {
	case AK_SEMANTIC_COLOR_LABEL: *out = p->fg; return true;
	case AK_SEMANTIC_COLOR_SECONDARY_LABEL: *out = with_alpha(p->fg, 0.55f); return true;  // .dim-label
	case AK_SEMANTIC_COLOR_TERTIARY_LABEL: *out = with_alpha(p->fg, 0.35f); return true;
	case AK_SEMANTIC_COLOR_BACKGROUND:
	case AK_SEMANTIC_COLOR_GROUPED_BACKGROUND: *out = p->bg; return true;
	case AK_SEMANTIC_COLOR_SECONDARY_BACKGROUND:
	case AK_SEMANTIC_COLOR_SECONDARY_GROUPED_BACKGROUND: *out = p->card; return true;
	case AK_SEMANTIC_COLOR_SEPARATOR: *out = with_alpha(p->fg, 0.15f); return true;
	case AK_SEMANTIC_COLOR_ACCENT: *out = p->accent; return true;
	case AK_SEMANTIC_COLOR_RED: *out = p->red; return true;
	case AK_SEMANTIC_COLOR_ORANGE: *out = p->orange; return true;
	case AK_SEMANTIC_COLOR_YELLOW: *out = p->yellow; return true;
	case AK_SEMANTIC_COLOR_GREEN: *out = p->green; return true;
	case AK_SEMANTIC_COLOR_BLUE: *out = p->blue; return true;
	case AK_SEMANTIC_COLOR_PURPLE: *out = p->purple; return true;
	case AK_SEMANTIC_COLOR_PINK: *out = p->pink; return true;
	case AK_SEMANTIC_COLOR_GRAY: *out = p->gray; return true;
	case AK_SEMANTIC_COLOR_CLEAR: *out = (GdkRGBA){0}; return true;
	case AK_SEMANTIC_COLOR_ON_ACCENT: *out = p->on_accent; return true;
	}
	return false;
}

// lx_resolve is a color of Asuka's: semantic colors are the theme's;
// false for the platform's default (0).
bool lx_resolve(ak_color c, GdkRGBA *out) {
	uint16_t s;
	if (ak_color_semantic(c, &s)) {
		if (!semantic(s, out)) return false;
		uint8_t a = ak_color_alpha(c);
		if (a != 255) out->alpha *= (float)a / 255;
		return true;
	}
	uint8_t r, g, b, a;
	if (ak_color_channels(c, &r, &g, &b, &a)) {
		*out = (GdkRGBA){(float)r / 255, (float)g / 255, (float)b / 255, (float)a / 255};
		return true;
	}
	return false;
}

// The host's style sheet: the app's accent, and classes for what
// libadwaita has no style of (a tinted button).
static const char host_css[] =
	"button.asuka-tinted { background-color: alpha(@accent_bg_color, 0.15); color: @accent_color; }\n"
	"button.asuka-tinted:hover { background-color: alpha(@accent_bg_color, 0.2); }\n"
	"button.asuka-tinted:active { background-color: alpha(@accent_bg_color, 0.3); }\n"
	"button.asuka-plain { color: @accent_color; }\n"
	"entry.asuka-plain, text.asuka-plain { background: none; box-shadow: none; outline: none; padding: 0; min-height: 0; }\n"
	"asuka-view.asuka-focus-ring:focus-visible { outline: 2px solid alpha(@accent_color, 0.5); outline-offset: -2px; }\n"
	".asuka-no-focus-ring:focus-visible, .asuka-no-focus-ring :focus-visible { outline: none; }\n"
	"scrolledwindow.asuka-field { background-color: alpha(currentColor, 0.1); border-radius: 6px; outline: 0 solid transparent; outline-offset: 2px; transition: outline-color 200ms ease-out, outline-width 200ms ease-out, outline-offset 200ms ease-out; }\n"
	"scrolledwindow.asuka-field:focus-within { outline-color: alpha(@accent_color, 0.5); outline-width: 2px; outline-offset: -2px; }\n"
	"scrolledwindow.asuka-field:disabled { filter: opacity(0.5); }\n"
	"textview.asuka-clear, textview.asuka-clear > text { background: none; }\n";

// lx_update_css sets the app's style sheet: the host's classes, and the
// accent (1.5's named colors, 1.6's variables).
void lx_update_css(void) {
	ak_buf css = {0};
	ak_buf_put(&css, host_css, sizeof host_css - 1);
	uint8_t r, g, b, a;
	if (ak_color_channels(app_accent(), &r, &g, &b, &a)) {
		ak_buf_printf(&css, "@define-color accent_bg_color rgb(%d,%d,%d);\n@define-color accent_color rgb(%d,%d,%d);\n", r, g, b, r, g, b);
		ak_buf_printf(&css, ":root { --accent-bg-color: rgb(%d,%d,%d); --accent-color: rgb(%d,%d,%d); }\n", r, g, b, r, g, b);
	}
	GdkDisplay *d = gdk_display_get_default();
	if (d) {
		if (!H.css) {
			H.css = gtk_css_provider_new();
			gtk_style_context_add_provider_for_display(d, (GtkStyleProvider *)H.css, 700);  // above the theme (600), below the user's
		}
		gtk_css_provider_load_from_string(H.css, lx_buf_cstr(&css));
	}
	ak_buf_free(&css);
}

ak_color lx_parse_accent(const char *s) {
	if (!s) return 0;
	if (*s == '#') s++;
	size_t n = strlen(s);
	char *end;
	unsigned long v = strtoul(s, &end, 16);
	if (*end) return 0;
	if (n == 6) return ak_rgba((uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v, 255);
	if (n == 8) return ak_rgba((uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v);
	return 0;
}

static char *string_prop(gpointer o, const char *name) {
	char *s = NULL;
	g_object_get(o, name, &s, NULL);
	return s;
}

base_font lx_system_font(void) {
	base_font f = {strdup("Adwaita Sans"), 11 * 96 / 72.0f};
	GtkSettings *s = gtk_settings_get_default();
	if (!s) return f;
	char *name = string_prop(s, "gtk-font-name");  // "Cantarell 11"
	if (name) {
		char *sp = strrchr(name, ' ');
		if (sp && sp > name) {
			char *end;
			double pt = strtod(sp + 1, &end);
			if (!*end && pt > 0) {
				*sp = 0;
				free(f.family);
				f.family = strdup(name);
				f.px = (float)(pt * 96 / 72);
			}
		}
		g_free(name);
	}
	return f;
}

// lx_text_scale is the desktop's text scaling (GNOME's Large Text, KDE's
// font DPI): GTK's Xft DPI over 96.
float lx_text_scale(void) {
	GtkSettings *s = gtk_settings_get_default();
	if (!s) return 1;
	int dpi = 0;
	g_object_get(s, "gtk-xft-dpi", &dpi, NULL);
	if (dpi <= 0) return 1;
	return (float)dpi / 1024 / 96;
}

void lx_load_theme(void) {
	if (!str_empty(H.cfg.accent)) H.accent = lx_parse_accent(H.cfg.accent);
	if (!str_empty(H.cfg.accent_dark)) H.accent_dark = lx_parse_accent(H.cfg.accent_dark);
	H.dark = adw_style_manager_get_dark(adw_style_manager_get_default());
	lx_update_css();
	H.base = lx_system_font();
	H.font_scale = lx_text_scale();
}

// The text styles as libadwaita's type classes, relative to the
// interface font (11 pt, 14.67 points): LargeTitle is .title-1 (181%,
// heavy), Title .title-2 (136%, heavy), Title2 .title-3 (136%, bold),
// Title3 .title-4 (118%, bold), Headline .heading (bold), Subheadline
// 91%, Body and Callout the body, Footnote and Caption .caption (82%),
// Caption2 75%.
static const struct {
	float scale;
	int weight;
} style_sizes[] = {
	[AK_TEXT_STYLE_DEFAULT] = {1, 400},
	[AK_TEXT_STYLE_LARGE_TITLE] = {1.81f, 800},
	[AK_TEXT_STYLE_TITLE] = {1.36f, 800},
	[AK_TEXT_STYLE_TITLE2] = {1.36f, 700},
	[AK_TEXT_STYLE_TITLE3] = {1.18f, 700},
	[AK_TEXT_STYLE_HEADLINE] = {1, 700},
	[AK_TEXT_STYLE_SUBHEADLINE] = {0.91f, 400},
	[AK_TEXT_STYLE_BODY] = {1, 400},
	[AK_TEXT_STYLE_CALLOUT] = {1, 400},
	[AK_TEXT_STYLE_FOOTNOTE] = {0.82f, 400},
	[AK_TEXT_STYLE_CAPTION] = {0.82f, 400},
	[AK_TEXT_STYLE_CAPTION2] = {0.75f, 400},
};

static const int weights[] = {
	[AK_FONT_WEIGHT_ULTRALIGHT] = 200,
	[AK_FONT_WEIGHT_THIN] = 100,
	[AK_FONT_WEIGHT_LIGHT] = 300,
	[AK_FONT_WEIGHT_REGULAR] = 400,
	[AK_FONT_WEIGHT_MEDIUM] = 500,
	[AK_FONT_WEIGHT_SEMIBOLD] = 600,
	[AK_FONT_WEIGHT_BOLD] = 700,
	[AK_FONT_WEIGHT_HEAVY] = 800,
	[AK_FONT_WEIGHT_BLACK] = 900,
};

// lx_font is a node's text style resolved: its size in points (scaled by
// the desktop's text scaling) and weight.
void lx_font(node *n, float *size, int *weight) {
	int st = n->text_style;
	if (st < 0 || (size_t)st >= AK_LEN(style_sizes)) st = 0;
	*size = H.base.px * style_sizes[st].scale;
	*weight = style_sizes[st].weight;
	if (n->font_size > 0) *size = n->font_size;
	if (n->font_weight > 0 && (size_t)n->font_weight < AK_LEN(weights) && weights[n->font_weight]) *weight = weights[n->font_weight];
	if (H.font_scale > 0) *size *= H.font_scale;
}

// ---- The app's fonts ----

typedef struct {
	char *key, *family;
} font_name;

static AK_VEC(font_name) fonts;
static bool fonts_loaded;

static void add_family(const char *key, const char *fam) {
	char *k = strdup(key);
	for (char *p = k; *p; p++) *p = (char)tolower((unsigned char)*p);
	font_name f = {k, strdup(fam)};
	ak_vec_push(&fonts, f);
}

// FcFontSet: int nfont, int sfont, FcPattern **fonts.
typedef struct {
	int nfont, sfont;
	void **fonts;
} fc_font_set;

// add_font_file adds a font file for the process and returns its family
// (malloc'd), or NULL.
static char *add_font_file(const char *path) {
	if (!lx_lib_load(ASUKA_LIB_FC) || !FcConfigAppFontAddFile || !FcConfigAppFontAddFile(NULL, (const unsigned char *)path)) return NULL;
	fc_font_set *set = FcConfigGetFonts(NULL, 1);  // FcSetApplication
	if (!set) return NULL;
	for (int i = 0; i < set->nfont; i++) {
		unsigned char *file = NULL, *fam = NULL;
		if (FcPatternGetString(set->fonts[i], "file", 0, &file) == 0 && file && strcmp((const char *)file, path) == 0 &&
			FcPatternGetString(set->fonts[i], "family", 0, &fam) == 0 && fam)
			return strdup((const char *)fam);
	}
	return NULL;
}

// load_app_fonts adds the app's font files (assets/fonts) to fontconfig's
// fonts for the process, once.
static void load_app_fonts(void) {
	if (fonts_loaded) return;
	fonts_loaded = true;
	char *dir = dsk_join(S(H.assets), "fonts");
	DIR *d = opendir(dir);
	if (!d) {
		free(dir);
		return;
	}
	struct dirent *e;
	while ((e = readdir(d))) {
		const char *dot = strrchr(e->d_name, '.');
		if (!dot || (strcasecmp(dot, ".ttf") && strcasecmp(dot, ".otf") && strcasecmp(dot, ".ttc"))) continue;
		char *path = dsk_join(dir, e->d_name);
		char *fam = add_font_file(path);
		free(path);
		if (!fam) continue;
		char base[256];
		size_t bn = (size_t)(dot - e->d_name) < sizeof base - 1 ? (size_t)(dot - e->d_name) : sizeof base - 1;
		memcpy(base, e->d_name, bn);
		base[bn] = 0;
		add_family(base, fam);
		add_family(fam, fam);
		char *dash = strchr(base, '-');
		if (dash && dash > base) {
			*dash = 0;
			add_family(base, fam);
		}
		free(fam);
	}
	closedir(d);
	free(dir);
	if (fonts.len && pango_fc_font_map_config_changed) pango_fc_font_map_config_changed((PangoFcFontMap *)pango_cairo_font_map_get_default());
}

// lx_custom_family is the family of a font of the app's (assets/fonts),
// loaded for the process: Inter-Bold names Inter's file, whose family is
// Inter.
const char *lx_custom_family(const char *name) {
	load_app_fonts();
	for (uint32_t i = 0; i < fonts.len; i++)
		if (strcasecmp(fonts.data[i].key, name) == 0) return fonts.data[i].family;
	return name;  // a system family
}

// lx_theme_changed reads the palette again, redraws every view and tells
// the core.
static void redraw(node *n, void *ctx) {
	(void)ctx;
	lx_queue_draw(n);
	if (n->kind == AK_KIND_TEXT || n->kind == AK_KIND_TEXT_FIELD || n->kind == AK_KIND_BUTTON || n->kind == AK_KIND_RICH_TEXT) lx_apply_text(n);
}

void lx_each_node(void (*f)(node *n, void *ctx), void *ctx);

void lx_theme_changed(void) {
	lx_read_palette(H.main->content);
	for (uint32_t i = 0; i < H.wins.len; i++)
		if (H.wins.data[i]->cfg.background) lx_window_css(H.wins.data[i]);  // a semantic color, the style's
	lx_each_node(redraw, NULL);
	lx_env_changed();
}
