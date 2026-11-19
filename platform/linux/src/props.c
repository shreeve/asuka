// props.c: making a node's view and its control, and setting their props:
// the common ones (every kind's: style, text, accessibility) and each
// control's own.
#include "host.h"

#include <math.h>
#include <stdlib.h>

static void on_clicked(GtkButton *b, node *n) {
	(void)b;
	lx_pressed(n);
}

static void on_slider(GtkRange *r, node *n) {
	if (n->updating) return;
	n->value = gtk_range_get_value(r);
	lx_emit(n, (ak_event){.kind = AK_EVENT_VALUE_CHANGED, .f = n->value});
}

static void on_wheel(GObject *d, GParamSpec *ps, node *n) {
	(void)ps;
	if (n->updating) return;
	n->selected = (int)gtk_drop_down_get_selected((GtkDropDown *)d);
	lx_emit(n, (ak_event){.kind = AK_EVENT_SELECTED, .i = n->selected});
}

// lx_create makes a node's view and, for a control, the control in it.
void lx_create(ak_id id, ak_view_kind kind) {
	node *n = calloc(1, sizeof *n);
	if (!n) abort();
	n->id = id;
	n->kind = kind;
	n->opacity = 1;
	n->selected = -1;
	n->widget = lx_view_new();
	lx_view_set_node(n->widget, n);
	switch (kind) {
	case AK_KIND_TEXT: {
		GtkWidget *l = gtk_label_new("");
		gtk_label_set_wrap((GtkLabel *)l, 1);
		gtk_label_set_wrap_mode((GtkLabel *)l, PANGO_WRAP_WORD_CHAR);
		gtk_label_set_xalign((GtkLabel *)l, 0);
		gtk_label_set_yalign((GtkLabel *)l, 0);
		lx_set_inner(n, l);
		break;
	}
	case AK_KIND_BUTTON: {
		GtkWidget *b = gtk_button_new();
		lx_set_inner(n, b);
		lx_connect(n, b, "clicked", G_CALLBACK(on_clicked));
		lx_style_button(n);
		break;
	}
	case AK_KIND_TEXT_FIELD:
		lx_make_field(n);
		break;
	case AK_KIND_TOGGLE:
		lx_make_toggle(n, false);
		break;
	case AK_KIND_SLIDER: {
		GtkWidget *s = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.001);
		gtk_scale_set_draw_value((GtkScale *)s, 0);
		n->max = 1;
		lx_set_inner(n, s);
		lx_connect(n, s, "value-changed", G_CALLBACK(on_slider));
		break;
	}
	case AK_KIND_SCROLL:
		lx_make_scroll(n, false);
		break;
	case AK_KIND_ACTIVITY:
		lx_set_inner(n, adw_spinner_new ? adw_spinner_new() : gtk_spinner_new());
		break;
	case AK_KIND_PROGRESS: {
		GtkWidget *p = gtk_progress_bar_new();
		gtk_widget_set_valign(p, GTK_ALIGN_CENTER);
		lx_set_inner(n, p);
		break;
	}
	case AK_KIND_IMAGE:
	case AK_KIND_SYMBOL:
		break;  // drawn by the view (image.c, symbols.c)
	case AK_KIND_SEGMENTED: {
		GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_add_css_class(box, "linked");
		lx_set_inner(n, box);
		break;
	}
	case AK_KIND_WHEEL: {
		GtkWidget *d = gtk_drop_down_new_from_strings(NULL);
		lx_set_inner(n, d);
		lx_connect(n, d, "notify::selected", G_CALLBACK(on_wheel));
		break;
	}
	case AK_KIND_STEPPER:
		lx_make_stepper(n);
		break;
	case AK_KIND_COLOR_PICKER:
		lx_make_color_picker(n);
		break;
	case AK_KIND_DATE_PICKER:
		lx_make_date_picker(n);
		break;
	default:
		// A kind this host has no view for shows nothing, but keeps its
		// children.
		if (!lx_create_nav(n)) lx_create_more(n);
	}
	lx_add_node(n);
	if (n->widget && !n->inner && n->kind != AK_KIND_VIEW && n->kind != AK_KIND_OVERLAY) gtk_widget_set_can_target(n->widget, 1);
}

// lx_set_inner makes c the control in n's view.
void lx_set_inner(node *n, GtkWidget *c) {
	if (n->inner) {
		g_signal_handlers_disconnect_matched(n->inner, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, n);
		gtk_widget_unparent(n->inner);
	}
	n->inner = c;
	if (c) {
		gtk_widget_set_parent(c, n->widget);
		gtk_widget_queue_allocate(n->widget);
	}
}

void lx_pressed(node *n) {
	if (n->has_menu && n->menu_trigger == AK_MENU_TRIGGER_TAP) {
		lx_popup_menu(n, -1, -1);
		return;
	}
	lx_emit(n, (ak_event){.kind = AK_EVENT_PRESS});
}

static void set_transform(node *n, transform_t t) {
	if (H.animating && n->placed) {
		lx_animate_transform(n, t);
		return;
	}
	n->transform = t;
	if (n->widget) {
		GtkWidget *p = gtk_widget_get_parent(n->widget);
		if (p) gtk_widget_queue_allocate(p);
	}
}

// style_changed redraws a view whose look changed, and its control's.
static void style_changed(node *n) {
	lx_queue_draw(n);
	if (n->kind == AK_KIND_BUTTON) lx_style_button(n);
}

void lx_apply_direction(node *n);
void lx_set_dialog_can_close(node *n);

void lx_set_common(node *n, ak_prop p, const ak_value *v) {
	switch (p) {
	case AK_PROP_BACKGROUND:
		n->background = (ak_color)v->i;
		style_changed(n);
		break;
	case AK_PROP_CORNER_RADIUS:
		n->corner_radius = (float)v->f;
		lx_queue_draw(n);
		break;
	case AK_PROP_BORDER_WIDTH:
		n->border_width = (float)v->f;
		lx_queue_draw(n);
		break;
	case AK_PROP_BORDER_COLOR:
		n->border_color = (ak_color)v->i;
		lx_queue_draw(n);
		break;
	case AK_PROP_OPACITY:
		n->opacity = (float)v->f;
		if (n->widget) {
			if (H.animating)
				lx_animate_opacity(n);
			else
				gtk_widget_set_opacity(n->widget, v->f);
		}
		break;
	case AK_PROP_HIDDEN:
		n->hidden = v->i != 0;
		if (n->widget) gtk_widget_set_visible(n->widget, !n->hidden);
		break;
	case AK_PROP_CLIP:
		n->clip = v->i != 0;
		lx_queue_draw(n);
		break;
	case AK_PROP_A11Y_LABEL:
		str_set(&n->a11y_label, v->s);
		lx_update_a11y(n);
		break;
	case AK_PROP_A11Y_HINT:
	case AK_PROP_A11Y_ROLE:
	case AK_PROP_A11Y_HIDDEN:
		lx_set_a11y(n, p, v);
		break;
	case AK_PROP_COLOR:
		n->color = (ak_color)v->i;
		lx_apply_text(n);
		break;
	case AK_PROP_TEXT_STYLE:
		n->text_style = (int32_t)v->i;
		lx_apply_text(n);
		break;
	case AK_PROP_FONT_SIZE:
		n->font_size = (float)v->f;
		lx_apply_text(n);
		break;
	case AK_PROP_FONT_WEIGHT:
		n->font_weight = (int32_t)v->i;
		lx_apply_text(n);
		break;
	case AK_PROP_FONT_FAMILY:
		str_set(&n->font_family, v->s);
		lx_apply_text(n);
		break;
	case AK_PROP_TEXT_ALIGN:
		n->text_align = (int32_t)v->i;
		lx_apply_text(n);
		break;
	case AK_PROP_LINE_LIMIT:
		n->line_limit = (int)v->i;
		lx_apply_text(n);
		break;
	case AK_PROP_DISABLED:
		n->disabled = v->i != 0;
		if (n->widget) gtk_widget_set_sensitive(n->widget, !n->disabled);
		break;
	case AK_PROP_MENU:
		n->has_menu = v->s.len > 0;
		actions_decode(&n->menu, v->s, 0);
		if (!n->menu.len) n->has_menu = v->s.len >= 4 && false;
		n->has_menu = n->menu.len > 0;
		lx_menu_changed(n);
		break;
	case AK_PROP_MENU_TRIGGER:
		n->menu_trigger = (int32_t)v->i;
		lx_menu_changed(n);
		break;
	case AK_PROP_TRANSFORM: {
		ak_reader r = {(const uint8_t *)v->s.p, v->s.len, 0, false};
		transform_t t = {.set = v->s.len >= 20};
		t.tx = ak_read_f32(&r);
		t.ty = ak_read_f32(&r);
		t.sx = ak_read_f32(&r);
		t.sy = ak_read_f32(&r);
		t.rotation = ak_read_f32(&r);
		if (t.set && t.tx == 0 && t.ty == 0 && t.sx == 1 && t.sy == 1 && t.rotation == 0) t.set = false;
		set_transform(n, t);
		break;
	}
	case AK_PROP_SHADOW: {
		ak_reader r = {(const uint8_t *)v->s.p, v->s.len, 0, false};
		n->shadow.color = ak_read_u64(&r);
		n->shadow.blur = ak_read_f32(&r);
		n->shadow.dx = ak_read_f32(&r);
		n->shadow.dy = ak_read_f32(&r);
		if (r.err) n->shadow = (shadow_t){0};
		lx_queue_draw(n);
		break;
	}
	case AK_PROP_GRADIENT: {
		ak_reader r = {(const uint8_t *)v->s.p, v->s.len, 0, false};
		uint32_t c = ak_read_u32(&r);
		n->gradient.len = 0;
		for (uint32_t i = 0; i < c && r.off + 12 <= r.len; i++) {
			grad_stop s;
			s.color = ak_read_u64(&r);
			s.location = ak_read_f32(&r);
			ak_vec_push(&n->gradient, s);
		}
		for (int i = 0; i < 4; i++) n->gradient_line[i] = ak_read_f32(&r);
		lx_queue_draw(n);
		break;
	}
	case AK_PROP_TRANSITION:
		lx_set_transition(n, v->s);
		break;
	case AK_PROP_MATERIAL:
		n->material = (int32_t)v->i;
		lx_queue_draw(n);
		break;
	case AK_PROP_LAYOUT_DIRECTION:
		n->dir = (int32_t)v->i;
		lx_apply_direction(n);
		break;
	case AK_PROP_CONTENT_INSET:
		memcpy(n->inset, v->edges, sizeof n->inset);
		if (n->widget) gtk_widget_queue_allocate(n->widget);
		break;
	case AK_PROP_TOOLTIP: {
		if (n->widget) {
			char *t = str_dup(v->s);
			gtk_widget_set_tooltip_text(n->widget, *t ? t : NULL);
			free(t);
		}
		break;
	}
	case AK_PROP_INTERCEPT_BACK:
		n->intercept_back = v->i != 0;
		lx_set_dialog_can_close(n);
		break;
	case AK_PROP_KEYS:
		lx_set_keys(n, v->s);
		break;
	case AK_PROP_A11Y_ACTIONS:
		lx_set_a11y_actions(n, v->s);
		break;
	case AK_PROP_A11Y_ADJUSTABLE:
		lx_set_adjustable(n, v->i != 0);
		break;
	case AK_PROP_FOCUSABLE:
	case AK_PROP_NO_FOCUS_ON_PRESS:
	case AK_PROP_NO_FOCUS_RING:
	case AK_PROP_FOCUS_TRAP:
		lx_set_focus_prop(n, p, v);
		break;
	case AK_PROP_A11Y_CHECKED:
	case AK_PROP_A11Y_EXPANDED:
	case AK_PROP_A11Y_SELECTED:
	case AK_PROP_A11Y_VALUE:
	case AK_PROP_A11Y_RANGE:
	case AK_PROP_A11Y_MODAL:
	case AK_PROP_A11Y_CONTROLS:
	case AK_PROP_A11Y_LABELLED_BY:
	case AK_PROP_A11Y_DESCRIBED_BY:
		lx_set_a11y(n, p, v);
		break;
	default:
		lx_set_common_more(n, p, v);
	}
}

// ---- Text ----

// lx_physical_align is a node's text alignment on screen: 0 left, 1
// center, 2 right, 3 justified (Start and End follow the way it reads).
int lx_physical_align(node *n) {
	switch (n->text_align) {
	case AK_TEXT_ALIGN_CENTER:
		return 1;
	case AK_TEXT_ALIGN_JUSTIFY:
		return 3;
	case AK_TEXT_ALIGN_END:
		return n->rtl ? 0 : 2;
	}
	return n->rtl ? 2 : 0;
}

float lx_xalign(node *n) {
	switch (lx_physical_align(n)) {
	case 1:
		return 0.5f;
	case 2:
		return 1;
	}
	return 0;
}

// lx_family is the font family a node's FontFamily names: the app's font,
// a generic family, or the desktop's ("" for the desktop's own).
const char *lx_family(node *n) {
	const char *f = S(n->font_family);
	if (!*f) return "";
	if (!strcmp(f, "monospace") || !strcmp(f, "mono")) return "monospace";
	if (!strcmp(f, "serif")) return "serif";
	if (!strcmp(f, "rounded")) return "";  // no rounded system face on Linux
	return lx_custom_family(f);
}

// lx_attrs are a node's text props as Pango attributes: its font's
// family, size and weight, and its color.
PangoAttrList *lx_attrs(node *n, bool with_color) {
	PangoAttrList *list = pango_attr_list_new();
	float size;
	int weight;
	lx_font(n, &size, &weight);
	pango_attr_list_insert(list, pango_attr_size_new_absolute((int)(size * 1024 + 0.5f)));
	pango_attr_list_insert(list, pango_attr_weight_new((PangoWeight)weight));
	const char *fam = lx_family(n);
	if (*fam) pango_attr_list_insert(list, pango_attr_family_new(fam));
	GdkRGBA c;
	if (with_color && lx_resolve(n->color, &c)) {
		pango_attr_list_insert(list, pango_attr_foreground_new(u16c(c.red), u16c(c.green), u16c(c.blue)));
		if (c.alpha < 1) pango_attr_list_insert(list, pango_attr_foreground_alpha_new(u16c(c.alpha)));
	}
	return list;
}

void lx_set_label_attrs(node *n, GtkWidget *label) {
	PangoAttrList *list = lx_attrs(n, true);
	gtk_label_set_attributes((GtkLabel *)label, list);
	pango_attr_list_unref(list);
}

static void apply_text_more(node *n) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->text) lx_areas[i]->text(n);
}

// lx_apply_text sets a text-showing control's font, color and alignment
// from its node's text props.
void lx_apply_text(node *n) {
	if (n->kind == AK_KIND_SYMBOL) {
		lx_style_symbol(n);
		return;
	}
	if (!n->inner) {
		apply_text_more(n);
		return;
	}
	switch (n->kind) {
	case AK_KIND_TEXT: {
		GtkLabel *l = (GtkLabel *)n->inner;
		lx_set_label_attrs(n, n->inner);
		gtk_label_set_xalign(l, lx_xalign(n));
		GtkJustification j = GTK_JUSTIFY_LEFT;
		switch (lx_physical_align(n)) {
		case 1: j = GTK_JUSTIFY_CENTER; break;
		case 2: j = GTK_JUSTIFY_RIGHT; break;
		case 3: j = GTK_JUSTIFY_FILL; break;
		}
		gtk_label_set_justify(l, j);
		if (n->line_limit > 0) {
			gtk_label_set_lines(l, n->line_limit);
			gtk_label_set_ellipsize(l, PANGO_ELLIPSIZE_END);
		} else {
			gtk_label_set_lines(l, -1);
			gtk_label_set_ellipsize(l, PANGO_ELLIPSIZE_NONE);
		}
		break;
	}
	case AK_KIND_BUTTON:
		lx_style_button(n);
		break;
	case AK_KIND_TEXT_FIELD:
		lx_style_field(n);
		break;
	case AK_KIND_PICKER:
		lx_style_picker(n);
		break;
	default:
		apply_text_more(n);
	}
}

// ---- Buttons ----

// lx_button_label is the GtkLabel a button shows its title in.
GtkWidget *lx_button_label(GtkWidget *b) {
	for (GtkWidget *c = gtk_widget_get_first_child(b); c; c = gtk_widget_get_next_sibling(c)) {
		if (AK_IS(c, gtk_label_get_type)) return c;
		GtkWidget *l = lx_button_label(c);
		if (l) return l;
	}
	return NULL;
}

// lx_style_button sets a button's content (its title, its icon) and its
// libadwaita style: Filled is .suggested-action, Plain .flat in the
// accent color, Tinted an accent wash, Gray and Bordered the standard
// button.
void lx_style_button(node *n) {
	GtkButton *b = (GtkButton *)n->inner;
	if (!b) return;
	if (!str_empty(n->icon)) {
		GtkWidget *c = adw_button_content_new();
		adw_button_content_set_icon_name((AdwButtonContent *)c, lx_icon_name(n->icon));
		adw_button_content_set_label((AdwButtonContent *)c, S(n->title));
		gtk_button_set_child(b, c);
		if (str_empty(n->title)) gtk_button_set_icon_name(b, lx_icon_name(n->icon));
	} else {
		gtk_button_set_label(b, S(n->title));
	}
	static const char *const classes[] = {"suggested-action", "flat", "asuka-tinted", "asuka-plain", "pill"};
	for (size_t i = 0; i < AK_LEN(classes); i++) gtk_widget_remove_css_class((GtkWidget *)b, classes[i]);
	switch (n->variant) {
	case AK_BUTTON_VARIANT_FILLED:
	case AK_BUTTON_VARIANT_GLASS_PROMINENT:
		gtk_widget_add_css_class((GtkWidget *)b, "suggested-action");
		break;
	case AK_BUTTON_VARIANT_PLAIN:
		gtk_widget_add_css_class((GtkWidget *)b, "flat");
		gtk_widget_add_css_class((GtkWidget *)b, "asuka-plain");
		break;
	case AK_BUTTON_VARIANT_TINTED:
		gtk_widget_add_css_class((GtkWidget *)b, "asuka-tinted");
		break;
	}
	// The button's label takes the node's font and color, when it sets
	// them.
	GtkWidget *l = lx_button_label((GtkWidget *)b);
	if (l && (n->color || n->font_size > 0 || n->text_style || n->font_weight || !str_empty(n->font_family))) lx_set_label_attrs(n, l);
}

// ---- Each kind's own props ----

void lx_set_own(node *n, ak_prop p, const ak_value *v) {
	switch (n->kind) {
	case AK_KIND_TEXT:
		if (p == AK_PROP_TEXT_TEXT) {
			str_set(&n->text, v->s);
			gtk_label_set_text((GtkLabel *)n->inner, n->text);
			lx_update_a11y(n);
		}
		return;
	case AK_KIND_BUTTON:
		switch (p) {
		case AK_PROP_BUTTON_TITLE:
			str_set(&n->title, v->s);
			break;
		case AK_PROP_BUTTON_VARIANT:
			n->variant = (int32_t)v->i;
			break;
		case AK_PROP_BUTTON_ICON:
			str_set(&n->icon, v->s);
			break;
		}
		lx_style_button(n);
		return;
	case AK_KIND_TEXT_FIELD:
		lx_set_field_prop(n, p, v);
		return;
	case AK_KIND_TOGGLE:
		switch (p) {
		case AK_PROP_TOGGLE_ON:
			n->value = (double)v->i;
			lx_set_toggle(n);
			break;
		case AK_PROP_TOGGLE_CHECKBOX:
			lx_make_toggle(n, v->i != 0);
			break;
		}
		return;
	case AK_KIND_SLIDER:
		n->updating = true;
		switch (p) {
		case AK_PROP_SLIDER_VALUE:
			n->value = v->f;
			break;
		case AK_PROP_SLIDER_MIN:
			n->min = v->f;
			break;
		case AK_PROP_SLIDER_MAX:
			n->max = v->f;
			break;
		}
		if (n->max > n->min) {
			gtk_range_set_range((GtkRange *)n->inner, n->min, n->max);
			gtk_range_set_increments((GtkRange *)n->inner, (n->max - n->min) / 100, (n->max - n->min) / 10);
		}
		gtk_range_set_value((GtkRange *)n->inner, n->value);
		n->updating = false;
		return;
	case AK_KIND_SCROLL:
		lx_set_scroll_prop(n, p, v);
		return;
	case AK_KIND_ACTIVITY:
		if (p == AK_PROP_ACTIVITY_ANIMATING) {
			bool on = v->i != 0;
			if (adw_spinner_new)
				gtk_widget_set_visible(n->inner, on);
			else
				gtk_spinner_set_spinning((GtkSpinner *)n->inner, on);
		}
		return;
	case AK_KIND_PROGRESS:
		switch (p) {
		case AK_PROP_PROGRESS_VALUE:
			n->value = v->f;
			if (n->min == 0)  // not indeterminate
				gtk_progress_bar_set_fraction((GtkProgressBar *)n->inner, fmin(fmax(v->f, 0), 1));
			break;
		case AK_PROP_PROGRESS_INDETERMINATE:
			lx_set_indeterminate(n, v->i != 0);
			break;
		}
		return;
	case AK_KIND_IMAGE:
		lx_set_image_prop(n, p, v);
		return;
	case AK_KIND_SYMBOL:
		if (p == AK_PROP_SYMBOL_NAME) {
			str_set(&n->symbol, v->s);
			lx_style_symbol(n);
		}
		return;
	case AK_KIND_SEGMENTED:
		switch (p) {
		case AK_PROP_SEGMENTED_SEGMENTS: {
			for (uint32_t i = 0; i < n->items.len; i++) free(n->items.data[i]);
			n->items.len = 0;
			ak_str_vec ss = {0};
			ak_decode_strings((const uint8_t *)v->s.p, v->s.len, &ss);
			for (uint32_t i = 0; i < ss.len; i++) ak_vec_push(&n->items, str_dup(ss.data[i]));
			ak_vec_free(&ss);
			lx_build_segments(n);
			break;
		}
		case AK_PROP_SEGMENTED_SELECTED:
			n->selected = (int)v->i;
			lx_select_segment(n);
			break;
		}
		return;
	case AK_KIND_WHEEL:
		n->updating = true;
		switch (p) {
		case AK_PROP_WHEEL_ITEMS: {
			for (uint32_t i = 0; i < n->items.len; i++) free(n->items.data[i]);
			n->items.len = 0;
			ak_str_vec ss = {0};
			ak_decode_strings((const uint8_t *)v->s.p, v->s.len, &ss);
			GtkStringList *model = gtk_string_list_new(NULL);
			for (uint32_t i = 0; i < ss.len; i++) {
				char *s = str_dup(ss.data[i]);
				ak_vec_push(&n->items, s);
				gtk_string_list_append(model, s);
			}
			ak_vec_free(&ss);
			gtk_drop_down_set_model((GtkDropDown *)n->inner, (GListModel *)model);
			g_object_unref(model);
			if (n->selected >= 0) gtk_drop_down_set_selected((GtkDropDown *)n->inner, (guint)n->selected);
			break;
		}
		case AK_PROP_WHEEL_SELECTED:
			n->selected = (int)v->i;
			gtk_drop_down_set_selected((GtkDropDown *)n->inner, (guint)(n->selected > 0 ? n->selected : 0));
			break;
		}
		n->updating = false;
		return;
	case AK_KIND_STEPPER:
		lx_set_stepper_prop(n, p, v);
		return;
	case AK_KIND_COLOR_PICKER:
		lx_set_color_picker_prop(n, p, v);
		return;
	case AK_KIND_DATE_PICKER:
		lx_set_date_prop(n, p, v);
		return;
	}
	if (lx_set_nav_prop(n, p, v)) return;
	lx_set_own_more(n, p, v);
}

// ---- Areas ----

bool lx_create_more(node *n) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->create && lx_areas[i]->create(n)) return true;
	return false;
}

bool lx_set_own_more(node *n, ak_prop p, const ak_value *v) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->prop && lx_areas[i]->prop(n, p, v)) return true;
	return false;
}

bool lx_set_common_more(node *n, ak_prop p, const ak_value *v) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->common && lx_areas[i]->common(n, p, v)) return true;
	return false;
}

bool lx_command_more(node *n, ak_cmd c, ak_span args) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->command && lx_areas[i]->command(n, c, args)) return true;
	return false;
}

bool lx_measure_more(node *n, float cw, float ch, float aw, float ah, float *ow, float *oh) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->measure && lx_areas[i]->measure(n, cw, ch, aw, ah, ow, oh)) return true;
	return false;
}

bool lx_draw_more(node *n, GtkSnapshot *snap, const graphene_rect_t *b) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->draw && lx_areas[i]->draw(n, snap, b)) return true;
	return false;
}

void lx_release_more(node *n) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->release) lx_areas[i]->release(n);
	if (n->ext_free) {
		n->ext_free(n);
		n->ext_free = NULL;
	}
}

void lx_scroll_moved(node *n) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->scrolled) lx_areas[i]->scrolled(n);
}

void lx_list_scroll_to_offset(node *n, double x, double y, bool animated) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->scroll_to && lx_areas[i]->scroll_to(n, x, y, animated)) return;
}

bool lx_list_frame(node *n, const float f[4]) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->frame && lx_areas[i]->frame(n, f)) return true;
	return false;
}

bool lx_list_insert(node *p, node *c, uint32_t index) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->insert && lx_areas[i]->insert(p, c, index)) return true;
	return false;
}

bool lx_list_remove(node *p, node *c) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->remove && lx_areas[i]->remove(p, c)) return true;
	return false;
}

bool lx_listen_more(node *n, ak_event_kind e);
bool lx_listen_more(node *n, ak_event_kind e) {
	for (size_t i = 0; i < lx_area_count; i++)
		if (lx_areas[i]->listen && lx_areas[i]->listen(n, e)) return true;
	return false;
}
