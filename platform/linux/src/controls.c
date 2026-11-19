// controls.c: the controls beyond a label and a button: toggles,
// steppers, color pickers, indeterminate progress, segmented controls,
// date pickers, pickers and progress rings.
#include "host.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void lx_set_a11y_string(gpointer w, int prop, const char *s);
void lx_set_role(gpointer w, const char *nick);
#define A11Y_PROP_LABEL 4

// ---- Toggles ----

static void on_check(GtkCheckButton *t, node *n) {
	if (n->updating) return;
	bool on = gtk_check_button_get_active(t);
	n->value = on;
	lx_emit(n, (ak_event){.kind = AK_EVENT_TOGGLED, .b = on});
}

static void on_switch(GObject *t, GParamSpec *ps, node *n) {
	(void)ps;
	if (n->updating) return;
	bool on = gtk_switch_get_active((GtkSwitch *)t);
	n->value = on;
	lx_emit(n, (ak_event){.kind = AK_EVENT_TOGGLED, .b = on});
}

void lx_make_toggle(node *n, bool checkbox) {
	if (n->inner && checkbox == n->secure) return;
	n->secure = checkbox;  // the kind of control it is
	GtkWidget *t;
	if (checkbox) {
		t = gtk_check_button_new();
		lx_set_inner(n, t);
		lx_connect(n, t, "toggled", G_CALLBACK(on_check));
	} else {
		t = gtk_switch_new();
		gtk_widget_set_halign(t, GTK_ALIGN_START);
		gtk_widget_set_valign(t, GTK_ALIGN_CENTER);
		lx_set_inner(n, t);
		lx_connect(n, t, "notify::active", G_CALLBACK(on_switch));
	}
	lx_set_toggle(n);
	if (!str_empty(n->a11y_label)) lx_update_a11y(n);
}

void lx_set_toggle(node *n) {
	n->updating = true;
	if (n->secure)
		gtk_check_button_set_active((GtkCheckButton *)n->inner, n->value != 0);
	else
		gtk_switch_set_active((GtkSwitch *)n->inner, n->value != 0);
	n->updating = false;
}

// ---- Progress ----

static gboolean pulse_tick(GtkWidget *w, GdkFrameClock *clock, gpointer data) {
	(void)clock;
	node *n = data;
	if (n->dead || n->min == 0 || n->inner != w) return 0;
	gint64 t = g_get_monotonic_time();
	if (t - n->pulsed > 100000) {
		n->pulsed = t;
		gtk_progress_bar_pulse((GtkProgressBar *)w);
	}
	return 1;
}

void lx_set_indeterminate(node *n, bool on) {
	if (on == (n->min != 0)) return;
	if (on) {
		n->min = 1;
		gtk_progress_bar_set_pulse_step((GtkProgressBar *)n->inner, 0.1);
		gtk_widget_add_tick_callback(n->inner, pulse_tick, n, NULL);
	} else {
		n->min = 0;
		gtk_progress_bar_set_fraction((GtkProgressBar *)n->inner, fmin(fmax(n->value, 0), 1));
	}
}

// ---- Segmented controls: linked toggle buttons in a group ----

static void on_segment(GtkToggleButton *b, node *n) {
	if (n->updating || !gtk_toggle_button_get_active(b)) return;
	int i = GPOINTER_TO_INT(g_object_get_data((GObject *)b, "asuka-index"));
	n->selected = i;
	lx_emit(n, (ak_event){.kind = AK_EVENT_SELECTED, .i = i});
}

void lx_build_segments(node *n) {
	GtkWidget *box = n->inner;
	for (GtkWidget *c = gtk_widget_get_first_child(box); c;) {
		GtkWidget *next = gtk_widget_get_next_sibling(c);
		gtk_box_remove((GtkBox *)box, c);
		c = next;
	}
	GtkWidget *first = NULL;
	for (uint32_t i = 0; i < n->items.len; i++) {
		GtkWidget *b = gtk_toggle_button_new();
		gtk_button_set_label((GtkButton *)b, n->items.data[i]);
		gtk_widget_set_hexpand(b, 1);
		if (!first)
			first = b;
		else
			gtk_toggle_button_set_group((GtkToggleButton *)b, (GtkToggleButton *)first);
		g_object_set_data((GObject *)b, "asuka-index", GINT_TO_POINTER((int)i));
		lx_connect(n, b, "toggled", G_CALLBACK(on_segment));
		gtk_box_append((GtkBox *)box, b);
	}
	lx_select_segment(n);
}

void lx_select_segment(node *n) {
	n->updating = true;
	int i = 0;
	for (GtkWidget *c = gtk_widget_get_first_child(n->inner); c; c = gtk_widget_get_next_sibling(c)) {
		gtk_toggle_button_set_active((GtkToggleButton *)c, i == n->selected);
		i++;
	}
	n->updating = false;
}

// ---- Steppers: minus and plus buttons, linked, as GNOME's spin buttons
// show them ----

static void update_stepper(node *n) {
	GtkWidget *minus = gtk_widget_get_first_child(n->inner);
	GtkWidget *plus = gtk_widget_get_next_sibling(minus);
	gtk_widget_set_sensitive(minus, n->value > n->min);
	gtk_widget_set_sensitive(plus, n->value < n->max);
}

static void step(node *n, double d) {
	double v = fmin(fmax(n->value + d * n->step, n->min), n->max);
	if (v == n->value) return;
	n->value = v;
	update_stepper(n);
	lx_emit(n, (ak_event){.kind = AK_EVENT_VALUE_CHANGED, .f = v});
}

static void on_minus(GtkButton *b, node *n) {
	(void)b;
	step(n, -1);
}

static void on_plus(GtkButton *b, node *n) {
	(void)b;
	step(n, 1);
}

void lx_make_stepper(node *n) {
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(box, "linked");
	GtkWidget *minus = gtk_button_new(), *plus = gtk_button_new();
	gtk_button_set_icon_name((GtkButton *)minus, "list-remove-symbolic");
	gtk_button_set_icon_name((GtkButton *)plus, "list-add-symbolic");
	gtk_box_append((GtkBox *)box, minus);
	gtk_box_append((GtkBox *)box, plus);
	n->step = 1;
	n->max = INFINITY;
	n->min = -INFINITY;
	lx_set_inner(n, box);
	lx_connect(n, minus, "clicked", G_CALLBACK(on_minus));
	lx_connect(n, plus, "clicked", G_CALLBACK(on_plus));
}

void lx_set_stepper_prop(node *n, ak_prop p, const ak_value *v) {
	switch (p) {
	case AK_PROP_STEPPER_VALUE:
		n->value = v->f;
		break;
	case AK_PROP_STEPPER_MIN:
		n->min = v->f;
		break;
	case AK_PROP_STEPPER_MAX:
		n->max = v->f;
		break;
	case AK_PROP_STEPPER_STEP:
		n->step = v->f > 0 ? v->f : 1;
		break;
	}
	update_stepper(n);
}

// ---- Color pickers: GTK's color dialog button ----

static void on_rgba(GObject *b, GParamSpec *ps, node *n) {
	(void)ps;
	if (n->updating) return;
	const GdkRGBA *c = gtk_color_dialog_button_get_rgba((GtkColorDialogButton *)b);
	ak_color v = ak_rgba(u8c(c->red), u8c(c->green), u8c(c->blue), u8c(c->alpha));
	lx_emit(n, (ak_event){.kind = AK_EVENT_COLOR_CHANGED, .i = (int64_t)(uint32_t)v});
}

void lx_make_color_picker(node *n) {
	GtkColorDialog *d = gtk_color_dialog_new();
	GtkWidget *b = gtk_color_dialog_button_new(d);  // takes the dialog
	gtk_widget_set_halign(b, GTK_ALIGN_START);
	gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
	lx_set_inner(n, b);
	lx_connect(n, b, "notify::rgba", G_CALLBACK(on_rgba));
}

void lx_set_color_picker_prop(node *n, ak_prop p, const ak_value *v) {
	GtkColorDialogButton *b = (GtkColorDialogButton *)n->inner;
	GtkColorDialog *d = gtk_color_dialog_button_get_dialog(b);
	switch (p) {
	case AK_PROP_COLOR_PICKER_VALUE: {
		GdkRGBA c;
		if (!lx_resolve((ak_color)v->i, &c)) return;
		n->updating = true;
		gtk_color_dialog_button_set_rgba(b, &c);
		n->updating = false;
		break;
	}
	case AK_PROP_COLOR_PICKER_ALPHA:
		if (d) gtk_color_dialog_set_with_alpha(d, v->i != 0);
		break;
	case AK_PROP_COLOR_PICKER_TITLE:
		if (d) {
			char *t = str_dup(v->s);
			gtk_color_dialog_set_title(d, t);
			free(t);
		}
		break;
	}
}

// ---- Date pickers ----
//
// A button showing the date (and time), opening a popover of GTK's
// calendar and hour and minute spin buttons, as GNOME's apps pick dates;
// Inline shows them in place. The value is the instant, in the local
// time zone.

struct date_picker {
	int32_t mode;  // AK_DATE_MODE_*
	bool inline_;
	int64_t ms, min, max;
	GtkWidget *button, *cal, *hour, *minute;
	bool updating;
};

static void show_date(node *n);
static void date_changed(node *n);

static void on_date(GObject *o, node *n) {
	(void)o;
	date_changed(n);
}

static void build_date_picker(node *n) {
	date_picker *d = n->date;
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	d->cal = d->hour = d->minute = NULL;
	if (d->mode != AK_DATE_MODE_TIME) {
		d->cal = gtk_calendar_new();
		gtk_box_append((GtkBox *)box, d->cal);
		lx_connect(n, d->cal, "day-selected", G_CALLBACK(on_date));
	}
	if (d->mode != AK_DATE_MODE_DATE) {
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_halign(row, GTK_ALIGN_CENTER);
		// A 24-hour spin: GTK's spin buttons have no AM/PM.
		d->hour = gtk_spin_button_new_with_range(0, 23, 1);
		d->minute = gtk_spin_button_new_with_range(0, 59, 1);
		gtk_spin_button_set_numeric((GtkSpinButton *)d->hour, 1);
		gtk_spin_button_set_numeric((GtkSpinButton *)d->minute, 1);
		gtk_spin_button_set_wrap((GtkSpinButton *)d->hour, 1);
		gtk_spin_button_set_wrap((GtkSpinButton *)d->minute, 1);
		gtk_box_append((GtkBox *)row, d->hour);
		gtk_box_append((GtkBox *)row, gtk_label_new(":"));
		gtk_box_append((GtkBox *)row, d->minute);
		gtk_box_append((GtkBox *)box, row);
		lx_connect(n, d->hour, "value-changed", G_CALLBACK(on_date));
		lx_connect(n, d->minute, "value-changed", G_CALLBACK(on_date));
	}
	if (d->inline_) {
		d->button = NULL;
		lx_set_inner(n, box);
	} else {
		GtkWidget *pop = gtk_popover_new();
		gtk_popover_set_child((GtkPopover *)pop, box);
		GtkWidget *b = gtk_menu_button_new();
		gtk_menu_button_set_popover((GtkMenuButton *)b, pop);
		gtk_widget_set_halign(b, GTK_ALIGN_START);
		gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
		d->button = b;
		lx_set_inner(n, b);
	}
	show_date(n);
}

void lx_make_date_picker(node *n) {
	n->date = calloc(1, sizeof *n->date);
	if (!n->date) abort();
	n->date->ms = g_get_real_time() / 1000;
	build_date_picker(n);
}

void lx_release_date(node *n) {
	free(n->date);
	n->date = NULL;
}

void lx_set_date_prop(node *n, ak_prop p, const ak_value *v) {
	date_picker *d = n->date;
	switch (p) {
	case AK_PROP_DATE_PICKER_VALUE:
		d->ms = v->i;
		show_date(n);
		break;
	case AK_PROP_DATE_PICKER_MODE:
		if ((int32_t)v->i != d->mode) {
			d->mode = (int32_t)v->i;
			build_date_picker(n);
		}
		break;
	case AK_PROP_DATE_PICKER_MIN:
		d->min = v->i;
		break;
	case AK_PROP_DATE_PICKER_MAX:
		d->max = v->i;
		break;
	case AK_PROP_DATE_PICKER_INLINE:
		if ((v->i != 0) != d->inline_) {
			d->inline_ = v->i != 0;
			build_date_picker(n);
		}
		break;
	}
}

// local_tm is the instant ms (Unix milliseconds) in the local time zone.
static struct tm local_tm(int64_t ms) {
	time_t t = (time_t)(ms >= 0 ? ms / 1000 : (ms - 999) / 1000);
	struct tm tm;
	localtime_r(&t, &tm);
	return tm;
}

// show_date puts the value in the picker's controls and label.
static void show_date(node *n) {
	date_picker *d = n->date;
	struct tm tm = local_tm(d->ms);
	d->updating = true;
	if (d->cal) {
		GDateTime *dt = g_date_time_new_local(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 12, 0, 0);
		gtk_calendar_select_day((GtkCalendar *)d->cal, dt);
		g_date_time_unref(dt);
	}
	if (d->hour) {
		gtk_spin_button_set_value((GtkSpinButton *)d->hour, tm.tm_hour);
		gtk_spin_button_set_value((GtkSpinButton *)d->minute, tm.tm_min);
	}
	d->updating = false;
	if (d->button) {
		// The instant as the user's locale writes it (strftime's %x and
		// %X, through GLib).
		GDateTime *dt = g_date_time_new_from_unix_local(d->ms / 1000);
		const char *time_f = H.hour_cycle == 12 ? "%l:%M %p" : "%H:%M";
		char f[32];
		if (d->mode == AK_DATE_MODE_TIME)
			snprintf(f, sizeof f, "%s", time_f);
		else if (d->mode == AK_DATE_MODE_DATE_TIME)
			snprintf(f, sizeof f, "%%x %s", time_f);
		else
			snprintf(f, sizeof f, "%%x");
		char *s = g_date_time_format(dt, f);
		gtk_menu_button_set_label((GtkMenuButton *)d->button, s ? s : "");
		g_free(s);
		g_date_time_unref(dt);
	}
}

// date_changed reads the controls into the value (within Min and Max)
// and reports it.
static void date_changed(node *n) {
	date_picker *d = n->date;
	if (d->updating) return;
	struct tm tm = local_tm(d->ms);
	if (d->cal) {
		GDateTime *dt = gtk_calendar_get_date((GtkCalendar *)d->cal);
		tm.tm_year = g_date_time_get_year(dt) - 1900;
		tm.tm_mon = g_date_time_get_month(dt) - 1;
		tm.tm_mday = g_date_time_get_day_of_month(dt);
		g_date_time_unref(dt);
	}
	if (d->hour) {
		tm.tm_hour = (int)gtk_spin_button_get_value((GtkSpinButton *)d->hour);
		tm.tm_min = (int)gtk_spin_button_get_value((GtkSpinButton *)d->minute);
	}
	tm.tm_isdst = -1;
	int64_t ms = (int64_t)mktime(&tm) * 1000;
	if (d->min && ms < d->min) ms = d->min;
	if (d->max && ms > d->max) ms = d->max;
	if (ms == d->ms) return;
	d->ms = ms;
	show_date(n);
	lx_emit(n, (ak_event){.kind = AK_EVENT_VALUE_CHANGED, .f = (double)ms});
}

// ---- Pickers ----
//
// A GtkDropDown over a GtkStringList of its items, as GNOME's apps choose
// one of many. GTK shows nothing while none is chosen, so its label is a
// dim label over the drop-down's button where the chosen item would be,
// shown only then; it names the drop-down for screen readers too. The
// button shows the chosen item in a label of the picker's own (its
// factory); it and the dim label are in the picker's font when it sets
// one. The list keeps GTK's rows.

typedef struct {
	GtkWidget *drop, *label;
	GtkWidget *shown;  // the button's label, while it has one
} picker;

#define PICKER_PAD 10  // where Adwaita's button puts its content: its padding
#define INVALID_POSITION 0xffffffffu

static void free_picker(node *n) {
	free(n->ext);
	n->ext = NULL;
}

// style_picker_label sets a picker's label in its font when it sets one,
// in the theme's otherwise; its color stays the theme's.
static void style_picker_label(node *n, GtkWidget *l) {
	if (!(n->font_size > 0 || n->text_style || n->font_weight || !str_empty(n->font_family))) {
		gtk_label_set_attributes((GtkLabel *)l, NULL);
		return;
	}
	PangoAttrList *a = lx_attrs(n, false);
	gtk_label_set_attributes((GtkLabel *)l, a);
	pango_attr_list_unref(a);
}

// lx_style_picker sets a picker's labels in its font: the one its
// drop-down's button shows the chosen item in, and the dim one.
void lx_style_picker(node *n) {
	picker *pk = n->ext;
	if (n->kind != AK_KIND_PICKER || !pk) return;
	if (pk->shown) style_picker_label(n, pk->shown);
	style_picker_label(n, pk->label);
}

// The button's factory: a label, its text the item's, in the picker's
// font.
static void on_picker_setup(GObject *f, GtkListItem *it, node *n) {
	GtkWidget *l = gtk_label_new(NULL);
	gtk_label_set_xalign((GtkLabel *)l, 0);
	gtk_list_item_set_child(it, l);
	((picker *)n->ext)->shown = l;
}

static void on_picker_bind(GObject *f, GtkListItem *it, node *n) {
	GtkWidget *l = gtk_list_item_get_child(it);
	GtkStringObject *s = gtk_list_item_get_item(it);
	gtk_label_set_text((GtkLabel *)l, s ? gtk_string_object_get_string(s) : "");
	style_picker_label(n, l);
}

static void on_picker_teardown(GObject *f, GtkListItem *it, node *n) {
	picker *pk = n->ext;
	if (pk && pk->shown == gtk_list_item_get_child(it)) pk->shown = NULL;
}

static void select_picker(node *n, picker *pk) {
	bool none = n->selected < 0 || (uint32_t)n->selected >= n->items.len;
	n->updating = true;
	gtk_drop_down_set_selected((GtkDropDown *)pk->drop, none ? INVALID_POSITION : (guint)n->selected);
	n->updating = false;
	gtk_widget_set_visible(pk->label, none);
}

static void on_picked(GObject *d, GParamSpec *ps, node *n) {
	(void)ps;
	if (n->updating) return;
	guint i = gtk_drop_down_get_selected((GtkDropDown *)d);
	if (i == INVALID_POSITION) return;
	n->selected = (int)i;
	gtk_widget_set_visible(((picker *)n->ext)->label, 0);
	lx_emit(n, (ak_event){.kind = AK_EVENT_SELECTED, .i = n->selected});
}

bool lx_create_picker(node *n) {
	if (n->kind != AK_KIND_PICKER) return false;
	picker *pk = calloc(1, sizeof *pk);
	if (!pk) abort();
	pk->drop = gtk_drop_down_new_from_strings(NULL);
	pk->label = gtk_label_new("");
	gtk_widget_add_css_class(pk->label, "dim-label");
	gtk_label_set_ellipsize((GtkLabel *)pk->label, PANGO_ELLIPSIZE_END);
	gtk_label_set_xalign((GtkLabel *)pk->label, 0);
	gtk_widget_set_can_target(pk->label, 0);  // clicks go to the drop-down
	gtk_widget_set_halign(pk->label, GTK_ALIGN_START);
	gtk_widget_set_valign(pk->label, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start(pk->label, PICKER_PAD);
	gtk_widget_set_margin_end(pk->label, PICKER_PAD);
	gtk_widget_set_visible(pk->label, 0);
	GtkWidget *ov = gtk_overlay_new();
	gtk_overlay_set_child((GtkOverlay *)ov, pk->drop);
	gtk_overlay_add_overlay((GtkOverlay *)ov, pk->label);
	// The label is part of its size: a short item's drop-down still fits it.
	gtk_overlay_set_measure_overlay((GtkOverlay *)ov, pk->label, 1);
	n->selected = -1;  // none, till the core says
	n->ext = pk;
	n->ext_free = free_picker;
	GtkListItemFactory *f = gtk_signal_list_item_factory_new();
	lx_connect(n, f, "setup", G_CALLBACK(on_picker_setup));
	lx_connect(n, f, "bind", G_CALLBACK(on_picker_bind));
	lx_connect(n, f, "teardown", G_CALLBACK(on_picker_teardown));
	gtk_drop_down_set_list_factory((GtkDropDown *)pk->drop, gtk_drop_down_get_factory((GtkDropDown *)pk->drop));
	gtk_drop_down_set_factory((GtkDropDown *)pk->drop, f);
	g_object_unref(f);
	lx_set_inner(n, ov);
	lx_connect(n, pk->drop, "notify::selected", G_CALLBACK(on_picked));
	return true;
}

bool lx_set_picker_prop(node *n, ak_prop p, const ak_value *v) {
	if (n->kind != AK_KIND_PICKER || !n->ext) return false;
	picker *pk = n->ext;
	switch (p) {
	case AK_PROP_PICKER_ITEMS: {
		for (uint32_t i = 0; i < n->items.len; i++) free(n->items.data[i]);
		n->items.len = 0;
		ak_str_vec ss = {0};
		ak_decode_strings((const uint8_t *)v->s.p, v->s.len, &ss);
		n->updating = true;
		GtkStringList *model = gtk_string_list_new(NULL);
		for (uint32_t i = 0; i < ss.len; i++) {
			char *s = str_dup(ss.data[i]);
			ak_vec_push(&n->items, s);
			gtk_string_list_append(model, s);
		}
		ak_vec_free(&ss);
		gtk_drop_down_set_model((GtkDropDown *)pk->drop, (GListModel *)model);
		g_object_unref(model);
		n->updating = false;
		select_picker(n, pk);
		return true;
	}
	case AK_PROP_PICKER_SELECTED:
		n->selected = (int)v->i;
		select_picker(n, pk);
		return true;
	case AK_PROP_PICKER_LABEL: {
		char *t = str_dup(v->s);
		gtk_label_set_text((GtkLabel *)pk->label, t);
		if (str_empty(n->a11y_label)) lx_set_a11y_string(pk->drop, A11Y_PROP_LABEL, t);
		free(t);
		return true;
	}
	}
	return false;
}

// ---- Progress rings ----
//
// libadwaita has no determinate ring, so the view draws one as AdwSpinner
// draws its circle: a track of the text color at 15% and, over it, an arc
// in the accent color (Color sets it) from the top, clockwise, as far as
// the value, round capped, its width growing with its size as the
// spinner's does (2.5 pixels at 16 across, 14 at 128). The arc moves to a
// new value as a progress bar's fill eases; at once with reduced motion.
// Without a value it's an AdwSpinner. Orca reads it as a progress bar,
// with its percentage.

#define RING_SIZE 32

typedef struct {
	double value, shown;
	bool animating;
	animation a;
	double start, from, to;
	guint tick;
	float w, h, v;
	GskPath *track, *fill;
	GskStroke *stroke;
} ring;

static void ring_paths_free(ring *r) {
	if (r->track) gsk_path_unref(r->track);
	if (r->fill) gsk_path_unref(r->fill);
	if (r->stroke) gsk_stroke_free(r->stroke);
	r->track = r->fill = NULL;
	r->stroke = NULL;
}

static void free_ring(node *n) {
	ring *r = n->ext;
	if (r->tick && n->widget) gtk_widget_remove_tick_callback(n->widget, r->tick);
	ring_paths_free(r);
	free(r);
	n->ext = NULL;
}

static void set_double_prop(gpointer w, int prop, double v) {
	GValue val = {0};
	g_value_init(&val, G_TYPE_DOUBLE);
	g_value_set_double(&val, v);
	gtk_accessible_update_property_value((GtkAccessible *)w, 1, (GtkAccessibleProperty *)&prop, &val);
	g_value_unset(&val);
}

// ring_a11y tells screen readers its value, as GtkProgressBar does: from
// 0 to 1, read as a percentage; an indeterminate one has none.
static void ring_a11y(node *n, ring *r, bool indeterminate) {
	if (!n->widget) return;
	enum { VALUE_MAX = 15, VALUE_MIN = 16, VALUE_NOW = 17, VALUE_TEXT = 18 };
	if (indeterminate) {
		for (int p = VALUE_MAX; p <= VALUE_TEXT; p++) gtk_accessible_reset_property((GtkAccessible *)n->widget, (GtkAccessibleProperty)p);
		return;
	}
	set_double_prop(n->widget, VALUE_MAX, 1);
	set_double_prop(n->widget, VALUE_MIN, 0);
	set_double_prop(n->widget, VALUE_NOW, r->value);
	char pct[16];
	snprintf(pct, sizeof pct, "%d%%", (int)lround(r->value * 100));
	lx_set_a11y_string(n->widget, VALUE_TEXT, pct);
}

bool lx_create_progress_ring(node *n) {
	if (n->kind != AK_KIND_PROGRESS_RING) return false;
	ring *r = calloc(1, sizeof *r);
	if (!r) abort();
	n->ext = r;
	n->ext_free = free_ring;
	lx_set_role(n->widget, "progress-bar");
	ring_a11y(n, r, false);
	return true;
}

static gboolean ring_tick(GtkWidget *w, GdkFrameClock *clock, gpointer data) {
	(void)w;
	(void)clock;
	node *n = data;
	ring *r = n->ext;
	if (n->dead || !r || !r->animating) {
		if (r) r->tick = 0;
		return 0;
	}
	bool done;
	double f = anim_progress(&r->a, lx_now() - r->start, &done);
	r->shown = r->from + (r->to - r->from) * f;
	if (done) {
		r->shown = r->to;
		r->animating = false;
		r->tick = 0;
	}
	lx_queue_draw(n);
	return !done;
}

static void set_ring_value(node *n, ring *r, double v) {
	r->value = v;
	ring_a11y(n, r, n->inner != NULL);
	if (H.env.reduce_motion || !n->placed || !n->widget) {
		r->animating = false;
		r->shown = v;
		lx_queue_draw(n);
		return;
	}
	r->a = H.animating ? H.anim : (animation){.duration = 0.25, .curve = AK_ANIMATION_CURVE_EASE_OUT, .damping = 1};
	r->start = lx_now();
	r->from = r->shown;
	r->to = v;
	r->animating = true;
	if (!r->tick) r->tick = gtk_widget_add_tick_callback(n->widget, ring_tick, n, NULL);
}

static void set_ring_indeterminate(node *n, ring *r, bool on) {
	if (on == (n->inner != NULL)) return;
	if (on) {
		GtkWidget *s;
		if (adw_spinner_new) {
			s = adw_spinner_new();
		} else {
			s = gtk_spinner_new();
			gtk_spinner_set_spinning((GtkSpinner *)s, 1);
		}
		lx_set_inner(n, s);
		// The view is the progress bar; the spinner in it isn't read.
		lx_set_role(s, "presentation");
	} else {
		lx_set_inner(n, NULL);
	}
	ring_a11y(n, r, on);
	lx_queue_draw(n);
}

bool lx_set_progress_ring_prop(node *n, ak_prop p, const ak_value *v) {
	if (n->kind != AK_KIND_PROGRESS_RING || !n->ext) return false;
	ring *r = n->ext;
	switch (p) {
	case AK_PROP_PROGRESS_RING_VALUE:
		set_ring_value(n, r, fmin(fmax(v->f, 0), 1));
		return true;
	case AK_PROP_PROGRESS_RING_INDETERMINATE:
		set_ring_indeterminate(n, r, v->i != 0);
		return true;
	}
	return false;
}

// ring_svg is the circle's outline from its top, clockwise on screen, as
// far as v (1: the whole circle), as SVG path data.
static void ring_svg(char *out, size_t n, float cx, float cy, float r, float v) {
	if (v >= 1) {
		snprintf(out, n, "M %.3f %.3f A %.3f %.3f 0 1 1 %.3f %.3f A %.3f %.3f 0 1 1 %.3f %.3f Z", cx, cy - r, r, r, cx, cy + r, r, r, cx, cy - r);
		return;
	}
	double a = (double)v * 2 * M_PI - M_PI / 2;
	snprintf(out, n, "M %.3f %.3f A %.3f %.3f 0 %d 1 %.3f %.3f", cx, cy - r, r, r, v > 0.5f ? 1 : 0, cx + r * (float)cos(a), cy + r * (float)sin(a));
}

static float ring_width(float d) { return fminf(fmaxf(2.5f + (d - 16) * (14 - 2.5f) / (128 - 16), 2.5f), 14); }

bool lx_draw_progress_ring(node *n, GtkSnapshot *snap, const graphene_rect_t *b) {
	if (n->kind != AK_KIND_PROGRESS_RING || !n->ext) return false;
	if (n->inner) return true;  // the spinner draws itself
	ring *r = n->ext;
	float d = fminf(b->size.width, b->size.height);
	if (d <= 0) return true;
	float v = (float)fmin(fmax(r->shown, 0), 1);
	if (r->w != b->size.width || r->h != b->size.height || r->v != v || !r->stroke) {
		ring_paths_free(r);
		r->w = b->size.width;
		r->h = b->size.height;
		r->v = v;
		float width = fminf(ring_width(d), d / 2);
		float cx = b->size.width / 2, cy = b->size.height / 2, rad = (d - width) / 2;
		char svg[256];
		ring_svg(svg, sizeof svg, cx, cy, rad, 1);
		r->track = gsk_path_parse(svg);
		if (v > 0) {
			ring_svg(svg, sizeof svg, cx, cy, rad, v);
			r->fill = gsk_path_parse(svg);
		}
		r->stroke = gsk_stroke_new(width);
		gsk_stroke_set_line_cap(r->stroke, GSK_LINE_CAP_ROUND);
	}
	GdkRGBA track = H.theme.fg;
	track.alpha *= 0.15f;
	if (r->track) gtk_snapshot_append_stroke(snap, r->track, r->stroke, &track);
	GdkRGBA fill = H.theme.accent, c;
	if (n->color && lx_resolve(n->color, &c)) fill = c;
	if (r->fill) gtk_snapshot_append_stroke(snap, r->fill, r->stroke, &fill);
	return true;
}

static bool measure_ring(node *n, float cw, float ch, float aw, float ah, float *ow, float *oh) {
	(void)aw;
	(void)ah;
	if (n->kind != AK_KIND_PROGRESS_RING) return false;
	// Its own size, or square: a style that sizes one side sizes the other.
	*ow = *oh = RING_SIZE;
	if (cw == cw)
		*oh = cw;
	else if (ch == ch)
		*ow = ch;
	return true;
}

static bool controls_create(node *n) { return lx_create_picker(n) || lx_create_progress_ring(n); }

static bool controls_prop(node *n, ak_prop p, const ak_value *v) { return lx_set_picker_prop(n, p, v) || lx_set_progress_ring_prop(n, p, v); }

static void controls_release(node *n) {
	if (n->date) lx_release_date(n);
}

const area lx_area_controls = {
	.name = "controls",
	.create = controls_create,
	.prop = controls_prop,
	.measure = measure_ring,
	.draw = lx_draw_progress_ring,
	.release = controls_release,
};

// lx_release_controls lets go of a control's own state.
void lx_release_controls(node *n) { (void)n; }
