// theme.c: Windows 11's colors and type ramp.
#include "host.h"
#include "parts.h"

rgba rgba_alpha(rgba c, float a) {
	c.a = (uint8_t)((float)c.a * wh_clampf(a, 0, 1) + 0.5f);
	return c;
}

rgba rgba_over(rgba c, rgba bg) {
	if (c.a == 255) return c;
	float a = (float)c.a / 255;
#define MIX(x, y) (uint8_t)((float)(x) * a + (float)(y) * (1 - a) + 0.5f)
	return (rgba){MIX(c.r, bg.r), MIX(c.g, bg.g), MIX(c.b, bg.b), 255};
#undef MIX
}

// wh_accent is the app's accent in light or dark mode (0: the system's).
ak_color wh_accent(bool dark) { return dark && H.accent_dark ? H.accent_dark : H.accent; }

void wh_palette(palette *p, bool dark, ak_color accent) {
	// The system accent and its shades: Settings, Personalization, Colors.
	uint32_t shades[7];
	static const uint32_t def[7] = {0x99EBFF, 0x4CC2FF, 0x0091F8, 0x0078D4, 0x0067C0, 0x003E92, 0x001A68};
	bool sys = wh_accent_palette(shades);
	rgba ac[7];
	for (int i = 0; i < 7; i++)
		ac[i] = sys ? (rgba){(uint8_t)shades[i], (uint8_t)(shades[i] >> 8), (uint8_t)(shades[i] >> 16), 255} : rgba_hex(def[i]);
	memset(p, 0, sizeof *p);
	p->dark = dark;
	if (dark) {
		p->label = rgba_hex(0xFFFFFF), p->secondary = rgba_hex(0xCCCCCC), p->tertiary = rgba_hex(0x969696);
		p->disabled = rgba_hex(0x787878), p->bg = rgba_hex(0x202020), p->card = rgba_hex(0x2B2B2B);
		p->layer = rgba_hex(0x2C2C2C), p->separator = rgba_hex(0x383838), p->stroke = rgba_hex(0x303030);
		p->card_stroke = rgba_hex(0x1D1D1D);
		p->control = rgba_hex(0x2D2D2D), p->control_hot = rgba_hex(0x323232), p->control_pressed = rgba_hex(0x272727);
		p->control_stroke = rgba_hex(0x3A3A3A);
		p->subtle_hot = (rgba){255, 255, 255, 0x0F}, p->subtle_pressed = (rgba){255, 255, 255, 0x0A};
		p->accent = ac[1];  // Light2
		p->accent_hot = rgba_alpha(ac[1], 0.9f), p->accent_pressed = rgba_alpha(ac[1], 0.8f);
		p->on_accent = rgba_hex(0x000000), p->accent_text = ac[0];  // Light3
		p->red = rgba_hex(0xFF99A4), p->orange = rgba_hex(0xFFA552), p->yellow = rgba_hex(0xFCE100);
		p->green = rgba_hex(0x6CCB5F), p->blue = rgba_hex(0x60CDFF), p->purple = rgba_hex(0xB4A0FF), p->pink = rgba_hex(0xF7A1D1);
		p->gray = rgba_hex(0x9A9A9A), p->knob = rgba_hex(0x454545), p->track = (rgba){255, 255, 255, 0x8B};
		p->field_bg = rgba_hex(0x2D2D2D), p->field_focus_bg = rgba_hex(0x1F1F1F);
	} else {
		p->label = rgba_hex(0x1A1A1A), p->secondary = rgba_hex(0x5D5D5D), p->tertiary = rgba_hex(0x8A8A8A);
		p->disabled = rgba_hex(0xA0A0A0), p->bg = rgba_hex(0xF3F3F3), p->card = rgba_hex(0xFBFBFB);
		p->layer = rgba_hex(0xF9F9F9), p->separator = rgba_hex(0xE5E5E5), p->stroke = rgba_hex(0xE5E5E5);
		p->card_stroke = rgba_hex(0xE5E5E5);
		p->control = rgba_hex(0xFEFEFE), p->control_hot = rgba_hex(0xF9F9F9), p->control_pressed = rgba_hex(0xF5F5F5);
		p->control_stroke = rgba_hex(0xD0D0D0);
		p->subtle_hot = (rgba){0, 0, 0, 0x09}, p->subtle_pressed = (rgba){0, 0, 0, 0x06};
		p->accent = ac[4];  // Dark1
		p->accent_hot = rgba_alpha(ac[4], 0.9f), p->accent_pressed = rgba_alpha(ac[4], 0.8f);
		p->on_accent = rgba_hex(0xFFFFFF), p->accent_text = ac[5];  // Dark2
		p->red = rgba_hex(0xC42B1C), p->orange = rgba_hex(0xF7630C), p->yellow = rgba_hex(0xC19C00);
		p->green = rgba_hex(0x0F7B0F), p->blue = rgba_hex(0x0067C0), p->purple = rgba_hex(0x8764B8), p->pink = rgba_hex(0xE3008C);
		p->gray = rgba_hex(0x8A8A8A), p->knob = rgba_hex(0xFFFFFF), p->track = (rgba){0, 0, 0, 0x72};
		p->field_bg = rgba_hex(0xFDFDFD), p->field_focus_bg = rgba_hex(0xFFFFFF);
	}
	uint8_t r, g, b, a;
	if (ak_color_channels(accent, &r, &g, &b, &a)) {
		// The app's accent replaces the system's, as asuka.toml's does on
		// iOS and Android.
		rgba c = {r, g, b, 255};
		p->accent = c, p->accent_hot = rgba_alpha(c, 0.9f), p->accent_pressed = rgba_alpha(c, 0.8f);
		p->accent_text = c;
		double lum = 0.299 * r + 0.587 * g + 0.114 * b;
		p->on_accent = lum > 160 ? rgba_hex(0x000000) : rgba_hex(0xFFFFFF);
	}
}

static bool semantic(uint16_t s, rgba *out) {
	const palette *p = &H.theme;
	switch (s) {
	case AK_SEMANTIC_COLOR_LABEL: *out = p->label; return true;
	case AK_SEMANTIC_COLOR_SECONDARY_LABEL: *out = p->secondary; return true;
	case AK_SEMANTIC_COLOR_TERTIARY_LABEL: *out = p->tertiary; return true;
	case AK_SEMANTIC_COLOR_BACKGROUND:
	case AK_SEMANTIC_COLOR_GROUPED_BACKGROUND: *out = p->bg; return true;
	case AK_SEMANTIC_COLOR_SECONDARY_BACKGROUND:
	case AK_SEMANTIC_COLOR_SECONDARY_GROUPED_BACKGROUND: *out = p->card; return true;
	case AK_SEMANTIC_COLOR_SEPARATOR: *out = p->separator; return true;
	case AK_SEMANTIC_COLOR_ACCENT: *out = p->accent_text; return true;
	case AK_SEMANTIC_COLOR_RED: *out = p->red; return true;
	case AK_SEMANTIC_COLOR_ORANGE: *out = p->orange; return true;
	case AK_SEMANTIC_COLOR_YELLOW: *out = p->yellow; return true;
	case AK_SEMANTIC_COLOR_GREEN: *out = p->green; return true;
	case AK_SEMANTIC_COLOR_BLUE: *out = p->blue; return true;
	case AK_SEMANTIC_COLOR_PURPLE: *out = p->purple; return true;
	case AK_SEMANTIC_COLOR_PINK: *out = p->pink; return true;
	case AK_SEMANTIC_COLOR_GRAY: *out = p->gray; return true;
	case AK_SEMANTIC_COLOR_CLEAR: *out = (rgba){0}; return true;
	case AK_SEMANTIC_COLOR_ON_ACCENT: *out = (rgba){255, 255, 255, 255}; return true;
	}
	return false;
}

bool wh_resolve(ak_color c, rgba *out) {
	uint16_t s;
	if (ak_color_semantic(c, &s)) {
		if (!semantic(s, out)) return false;
		uint8_t a = ak_color_alpha(c);
		if (a != 255) out->a = (uint8_t)((uint32_t)out->a * a / 255);
		return true;
	}
	uint8_t r, g, b, a;
	if (ak_color_channels(c, &r, &g, &b, &a)) {
		*out = (rgba){r, g, b, a};
		return true;
	}
	return false;
}

// wh_brush is a cached solid brush (WM_CTLCOLOR* return one).
static wh_map brushes;

HBRUSH wh_brush(rgba c) {
	uint64_t k = (uint64_t)rgba_ref(c) + 1;
	HBRUSH b = wh_map_get(&brushes, k);
	if (!b) {
		b = CreateSolidBrush(rgba_ref(c));
		wh_map_put(&brushes, k, b);
	}
	return b;
}

void wh_clear_brushes(void) {
	uint32_t i = 0;
	void *v;
	while (wh_map_next(&brushes, &i, NULL, &v)) DeleteObject(v);
	free(brushes.keys);
	free(brushes.vals);
	memset(&brushes, 0, sizeof brushes);
}

// wh_backdrop is the opaque color behind a view: its nearest ancestor's
// background, or the window's.
rgba wh_backdrop(const node *n) {
	rgba bg = wh_bg_in(n->hwnd);
	rgba chain[64];
	int k = 0;
	for (const node *p = n; p && k < 64; p = p->parent) {
		rgba c;
		if (wh_resolve(p->background, &c) && c.a > 0) {
			chain[k++] = c;
			if (c.a == 255) break;
		}
		if (p->kind == AK_KIND_SCREEN || p->kind == AK_KIND_SHEET || p->kind == AK_KIND_WINDOW) {
			if (k == 0) chain[k++] = bg;
			break;
		}
	}
	for (int i = k - 1; i >= 0; i--) bg = rgba_over(chain[i], bg);
	return bg;
}

// wh_text_color is n's text color over what's behind it (GDI text is
// opaque).
rgba wh_text_color(const node *n, rgba def) {
	rgba c;
	if (!wh_resolve(n->color, &c)) c = def;
	if (n->disabled) c = H.theme.disabled;
	float a = wh_own_alpha(n);
	if (a < 1) c = rgba_alpha(c, a);
	return rgba_over(c, wh_backdrop(n));
}

// ---- Fonts ----

// The text styles on Windows 11's type ramp (WinUI's): Caption 12, Body
// 14, Body Strong 14 semibold, Body Large 18, Subtitle 20 semibold, Title
// 28 semibold, Title Large 40 semibold, in Segoe UI Variable. iOS's
// styles map to the nearest.
static const struct {
	float size;
	int32_t weight;
} style_sizes[] = {
	[AK_TEXT_STYLE_DEFAULT] = {14, 400},
	[AK_TEXT_STYLE_LARGE_TITLE] = {40, 600},
	[AK_TEXT_STYLE_TITLE] = {28, 600},
	[AK_TEXT_STYLE_TITLE2] = {20, 600},
	[AK_TEXT_STYLE_TITLE3] = {18, 400},
	[AK_TEXT_STYLE_HEADLINE] = {14, 600},
	[AK_TEXT_STYLE_SUBHEADLINE] = {13, 400},
	[AK_TEXT_STYLE_BODY] = {14, 400},
	[AK_TEXT_STYLE_CALLOUT] = {14, 400},
	[AK_TEXT_STYLE_FOOTNOTE] = {12, 400},
	[AK_TEXT_STYLE_CAPTION] = {12, 400},
	[AK_TEXT_STYLE_CAPTION2] = {11, 400},
};

static const int32_t weights[] = {
	[AK_FONT_WEIGHT_ULTRALIGHT] = 200, [AK_FONT_WEIGHT_THIN] = 250, [AK_FONT_WEIGHT_LIGHT] = 300,
	[AK_FONT_WEIGHT_REGULAR] = 400, [AK_FONT_WEIGHT_MEDIUM] = 500, [AK_FONT_WEIGHT_SEMIBOLD] = 600,
	[AK_FONT_WEIGHT_BOLD] = 700, [AK_FONT_WEIGHT_HEAVY] = 800, [AK_FONT_WEIGHT_BLACK] = 900,
};

float wh_font_scale(void) { return H.env.env.font_scale > 0 ? H.env.env.font_scale : 1; }

font_spec wh_spec(int32_t style, int32_t weight, float size, const char *family) {
	if (style < 0 || (size_t)style >= WH_LEN(style_sizes) || style_sizes[style].size == 0) style = 0;
	font_spec s = {.size = style_sizes[style].size, .weight = style_sizes[style].weight, .family = family};
	if (size > 0) s.size = size;
	if (weight > 0 && (size_t)weight < WH_LEN(weights) && weights[weight]) s.weight = weights[weight];
	s.size *= wh_font_scale();
	return s;
}

font_spec wh_node_spec(const node *n) {
	font_spec s = wh_spec(n->text_style, n->font_weight, n->font_size, n->font_family);
	s.k = wh_scale_of(n);
	return s;
}

static int CALLBACK found_font(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM l) {
	(void)lf, (void)tm, (void)type;
	*(bool *)l = true;
	return 0;
}

bool wh_font_installed(const wchar_t *family) {
	LOGFONTW lf = {.lfCharSet = DEFAULT_CHARSET};
	wcsncpy(lf.lfFaceName, family, LF_FACESIZE - 1);
	bool found = false;
	EnumFontFamiliesExW(H.measure_dc, &lf, found_font, (LPARAM)&found, 0);
	return found;
}

static int has_variable = -1;

// The app's fonts: the files in its assets/fonts, added for the process
// (private) the first time a FontFamily names one: a file's name
// (Inter-Bold) or its family (Inter) is GDI's family name.
static struct {
	char **names;
	wchar_t **families;
	uint32_t n;
	bool read;
} app_fonts;

static void add_app_font(const char *name, const wchar_t *family) {
	app_fonts.names = wh_realloc(app_fonts.names, (app_fonts.n + 1) * sizeof *app_fonts.names);
	app_fonts.families = wh_realloc(app_fonts.families, (app_fonts.n + 1) * sizeof *app_fonts.families);
	app_fonts.names[app_fonts.n] = wh_strdup(name);
	app_fonts.families[app_fonts.n] = _wcsdup(family);
	app_fonts.n++;
}

static const wchar_t *custom_family(const char *name) {
	if (!app_fonts.read) {
		app_fonts.read = true;
		char *dir = dsk_join(H.assets, "fonts");
		char *pat = dsk_join(dir, "*");
		wchar_t *wpat = wh_wide(pat, (int)strlen(pat));
		WIN32_FIND_DATAW fd;
		HANDLE f = FindFirstFileW(wpat, &fd);
		if (f != INVALID_HANDLE_VALUE) {
			do {
				wchar_t *dot = wcsrchr(fd.cFileName, '.');
				if (!dot || (_wcsicmp(dot, L".ttf") != 0 && _wcsicmp(dot, L".otf") != 0)) continue;
				char *file = wh_utf8(fd.cFileName, -1, NULL);
				char *path = dsk_join(dir, file);
				wchar_t *wpath = wh_wide(path, (int)strlen(path));
				if (AddFontResourceExW(wpath, FR_PRIVATE, NULL) > 0) {
					*strrchr(file, '.') = 0;
					wchar_t *family = wh_wide(file, (int)strlen(file));
					wchar_t *dash = wcschr(family, '-');
					if (dash) *dash = 0;
					// GDI knows a font by its family; the weight comes from
					// the view's FontWeight (a file named Inter-Bold is Inter).
					add_app_font(file, family);
					char *fam = wh_utf8(family, -1, NULL);
					add_app_font(fam, family);
					free(fam);
					free(family);
				}
				free(wpath);
				free(path);
				free(file);
			} while (FindNextFileW(f, &fd));
			FindClose(f);
		}
		free(wpat);
		free(pat);
		free(dir);
	}
	for (uint32_t i = 0; i < app_fonts.n; i++)
		if (strcmp(app_fonts.names[i], name) == 0) return app_fonts.families[i];
	return NULL;
}

// face picks the font family for a size: Segoe UI Variable's optical
// sizes (Small, Text, Display) where it's installed, else Segoe UI.
static const wchar_t *face(const font_spec *s, wchar_t *buf, size_t cap) {
	const char *f = s->family;
	if (f && *f) {
		if (_stricmp(f, "monospace") == 0 || _stricmp(f, "mono") == 0) return L"Cascadia Mono";
		if (_stricmp(f, "serif") == 0) return L"Georgia";
		if (_stricmp(f, "rounded") != 0) {
			const wchar_t *c = custom_family(f);
			if (c) return c;
			MultiByteToWideChar(CP_UTF8, 0, f, -1, buf, (int)cap);
			buf[cap - 1] = 0;
			return buf;
		}
	}
	if (s->mono) return L"Cascadia Mono";
	if (has_variable < 0) has_variable = wh_font_installed(L"Segoe UI Variable Text");
	if (!has_variable) return L"Segoe UI";
	if (s->size < 12.5f) return L"Segoe UI Variable Small";
	if (s->size < 20) return L"Segoe UI Variable Text";
	return L"Segoe UI Variable Display";
}

// wh_face is the font family a spec draws in (buf: room for one of the app's).
const wchar_t *wh_face(const font_spec *s, wchar_t *buf, size_t cap) { return face(s, buf, cap); }

typedef struct {
	wchar_t family[LF_FACESIZE];
	int px, weight;
	uint8_t flags;
	HFONT font;
} font_entry;

static font_entry *fonts;
static uint32_t nfonts;

// wh_font is the HFONT for a spec at its window's DPI, cached (by its size
// in pixels: windows at two DPIs have fonts of their own).
HFONT wh_font(font_spec s) {
	float scale = s.k > 0 ? s.k : H.scale;
	wchar_t buf[LF_FACESIZE];
	const wchar_t *fam = face(&s, buf, LF_FACESIZE);
	int px = (int)lroundf(s.size * scale);
	uint8_t flags = (uint8_t)(s.italic | s.underline << 1 | s.strike << 2 | s.gray << 3);
	for (uint32_t i = 0; i < nfonts; i++) {
		font_entry *e = &fonts[i];
		if (e->px == px && e->weight == s.weight && e->flags == flags && wcscmp(e->family, fam) == 0) return e->font;
	}
	LOGFONTW lf = {.lfHeight = -px, .lfWeight = s.weight, .lfCharSet = DEFAULT_CHARSET, .lfQuality = CLEARTYPE_QUALITY};
	if (s.gray) lf.lfQuality = ANTIALIASED_QUALITY;
	lf.lfItalic = s.italic, lf.lfUnderline = s.underline, lf.lfStrikeOut = s.strike;
	wcsncpy(lf.lfFaceName, fam, LF_FACESIZE - 1);
	HFONT f = CreateFontIndirectW(&lf);
	fonts = wh_realloc(fonts, (nfonts + 1) * sizeof *fonts);
	font_entry *e = &fonts[nfonts++];
	memset(e, 0, sizeof *e);
	wcsncpy(e->family, fam, LF_FACESIZE - 1);
	e->px = px, e->weight = s.weight, e->flags = flags, e->font = f;
	return f;
}

HFONT wh_node_font(const node *n) { return wh_font(wh_node_spec(n)); }

// wh_body_font is the controls' font: Body, 14, in n's window.
HFONT wh_body_font(const node *n) {
	font_spec s = wh_spec(0, 0, 0, NULL);
	s.k = wh_scale_of(n);
	return wh_font(s);
}

void wh_clear_fonts(void) {
	for (uint32_t i = 0; i < nfonts; i++) DeleteObject(fonts[i].font);
	free(fonts);
	fonts = NULL;
	nfonts = 0;
}

// wh_icon_face is Segoe Fluent Icons (Windows 11; Segoe MDL2 Assets
// before it).
const wchar_t *wh_icon_face(void) { return wh_windows11() ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets"; }

HFONT wh_icon_font(const node *n, float size) {
	static char face8[64];
	if (!face8[0]) WideCharToMultiByte(CP_UTF8, 0, wh_icon_face(), -1, face8, sizeof face8, NULL, NULL);
	font_spec s = {.size = size, .weight = 400, .family = face8, .k = wh_scale_of(n)};
	return wh_font(s);
}
