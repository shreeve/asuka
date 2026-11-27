// parts.h: what the host's parts ask of each other, by the file that
// answers (host.h has the core of it: nodes, painting, the theme).
#ifndef ASUKA_WIN_PARTS_H
#define ASUKA_WIN_PARTS_H

#include "host.h"

// ---- sys.c ----
bool wh_reg_dword(HKEY root, const wchar_t *path, const wchar_t *name, DWORD *out);
bool wh_reg_binary(HKEY root, const wchar_t *path, const wchar_t *name, void *out, DWORD *size);
wchar_t *wh_reg_string(HKEY root, const wchar_t *path, const wchar_t *name);
bool wh_apps_use_light_theme(void);
bool wh_accent_palette(uint32_t out[7]);
float wh_text_scale(void);
void wh_set_app_dark(bool dark);
void wh_allow_dark(HWND hwnd, bool dark);
void wh_set_dark_title_bar(HWND hwnd, bool dark);
DWORD wh_build(void);
void wh_dpi_aware(void);
int wh_dpi(HWND hwnd);
void wh_adjust_window_rect(RECT *r, DWORD style, DWORD ex, int dpi);
int wh_system_metric(int i, int dpi);
void wh_activate_common_controls(void);
void wh_locale_name(char *out, size_t cap);
void wh_preferred_languages(char *out, size_t cap);
wchar_t *wh_locale_info(LCTYPE t);
uint8_t wh_first_weekday(void);
uint8_t wh_hour_cycle(void);
void wh_time_zone(char *out, size_t cap);
bool wh_set_thread_language(const char *tag);
bool wh_key_down(int vk);
int64_t wh_now_ns(void);
double wh_seconds(void);
HTHEME wh_open_theme(HWND hwnd, const wchar_t *cls, int dpi);
HRESULT wh_task_dialog(const TASKDIALOGCONFIG *c, int *button);

// ---- host.c ----
uint32_t wh_children(HWND hwnd, HWND **out);
void wh_free_node(node *n);

// ---- views.c ----
void wh_subclass(HWND hwnd, LRESULT (*fn)(HWND, UINT, WPARAM, LPARAM));
void wh_unsubclass(HWND hwnd);
LRESULT wh_def_subclass(HWND hwnd, UINT m, WPARAM wp, LPARAM lp);

// ---- paint.c ----
RECT wh_rect_in(HWND hwnd, HWND parent);
void wh_invalidate_shadow(node *n);

// ---- theme.c ----
bool wh_font_installed(const wchar_t *family);

// ---- controls.c ----
void wh_check_selection(node *n);
void wh_insert_text(node *n, const char *s, size_t len);
void wh_field_command(node *n, HWND ctl, UINT code);

// ---- commands.c: commands, menus of actions, modules' calls ----
void wh_command(node *n, uint16_t cmd, const uint8_t *args, size_t len);
void wh_pop_up(node *n, HWND anchor, bool below);
void wh_pop_up_actions(node *target, HWND anchor, node *at, const actions *a, int64_t base);
bool wh_animate_frame(node *n, const float f[4]);
void wh_set_accent(ak_color c, ak_color dark);
void wh_set_color_scheme(int s);
void wh_set_dev_font_scale(float s);

// ---- window.c / winctl.c / titlebar.c / placement.c ----
void wh_listen_window(node *n);
bool wh_under_caption(HWND hwnd);
void wh_set_scale(HWND hwnd, float k);
void wh_close_main(void);

// ---- interaction.c: hover, drag and drop ----
void wh_hover_views(int delta);
void wh_refresh_hover(void);
void wh_forget_interaction(node *n);
void wh_start_drag(node *n, HWND hwnd);
bool wh_paste_content(node *n);
void wh_accept_drops(HWND win);
void wh_pointer_moved(HWND hwnd, POINT screen, bool track);
void wh_mouse_left(HWND hwnd);
// wh_interaction_step runs ASUKA_SCRIPT's drop, drag, paste and pinch
// steps (automation.c); false: not one of them.
bool wh_interaction_step(const char *verb, const wchar_t *arg);

// ---- overlay.c: Overlay (RFC 0010) ----
void wh_make_overlay(node *n);
void wh_overlay_gone(node *n);
void wh_set_overlay_prop(node *n, uint16_t p, const ak_value *v);
void wh_present_overlay(node *n);
bool wh_overlay_modal(const node *n);
node *wh_overlay_root(const node *n);
uint32_t wh_overlay_seq(const node *n);
void wh_overlays_dirty(void);
void wh_overlays_follow(node *r);
void wh_overlays_environment(void);
void wh_overlay_pointer_down(HWND target, POINT screen);
bool wh_overlay_back(node *root);
node *wh_modal_overlay(HWND top);
HWND wh_hit(HWND top, POINT screen);
void wh_overlays_dump(ak_buf *b, void (*walk)(ak_buf *, node *, int));

// ---- deverror.c: asuka dev's error overlay (DevError, DevErrorClear) ----
void wh_dev_error(node *root, const uint8_t *args, size_t len);
void wh_dev_error_clear(void);
void wh_dev_error_follow(void);
void wh_dev_error_restyle(void);
bool wh_dev_error_key(MSG *m);
bool wh_dev_error_primary(HWND ctl);
bool wh_dev_error_showing(void);
bool wh_dev_error_tap(const wchar_t *text);
void wh_dev_highlight(node *n, const uint8_t *args, size_t len);  // asuka dev's inspector's
void wh_dev_highlight_clear(void);
void wh_dev_error_dump(ak_buf *b);
void wh_dev_error_print(HDC dc, RECT win);

// ---- devperf.c: asuka dev's performance overlay (DevPerf) ----
void wh_dev_perf(const uint8_t *args, size_t len);
void wh_dev_perf_follow(void);
void wh_dev_perf_dump(ak_buf *b);
void wh_dev_perf_print(HDC dc, RECT win);
// wh_packaged reports whether the app runs from a package (an MSIX), with
// a package identity: Windows installs and updates it, and its manifest
// declares what an unpackaged app registers as it runs (dock.c).
bool wh_packaged(void);

// ---- focus.c: the keyboard's focus, Tab (RFC 0010) ----
void wh_focus_start(void);
void wh_input_seen(const MSG *m);
bool wh_is_press_message(UINT m);
void wh_pressing(bool on);
void wh_key_pressed(void);
node *wh_focused_node(void);
void wh_focus_forget(node *n);
void wh_focus_flush(void);
bool wh_tab_key(MSG *m);

// ---- keys.c ----
void wh_set_keys(node *n, const uint8_t *b, size_t len);
bool wh_route_key(MSG *m);

// ---- gesture.c ----
void wh_allow_gestures(node *n);
bool wh_touch_gesture(node *n, HWND hwnd, LPARAM lp);
bool wh_wheel_pinch(node *n, HWND hwnd, int delta, LPARAM lp);
void wh_end_wheel_pinch(node *n, HWND hwnd);

// ---- a11y.c: screen readers (UI Automation) ----
void wh_set_a11y(node *n, uint16_t p, const ak_value *v);
bool wh_adjust_key(node *n, WPARAM vk);
bool wh_actions_menu(node *n, HWND hwnd, LPARAM lp);
LRESULT wh_get_object(node *n, HWND hwnd, WPARAM wp, LPARAM lp, bool *handled);
void wh_drop_provider(node *n, HWND hwnd);
void wh_set_accessible_name(HWND hwnd, const wchar_t *name);
void wh_clear_accessible_name(HWND hwnd);
void wh_uia_range_changed(node *n, double old, double now);
void wh_a11y_modal_changed(node *n);
void wh_a11y_forget(node *n);
void wh_announce(const char *s, size_t len);
void wh_set_tooltip(node *n, const wchar_t *tip);
void wh_tooltip_on(HWND hwnd, const wchar_t *tip);  // any window's tooltip (a snackbar's close button)

// ---- scroll.c: Scroll views and the scroll bars over them ----
void wh_update_scroll(node *n);
void wh_restart_scroll(node *n);
void wh_scroll_by(node *n, float dx, float dy);
bool wh_wheel(node *n, bool horizontal, int delta);
bool wh_page_wheel(node *n, bool horizontal, int delta);
void wh_scroll_bar_message(node *n, bool horizontal, int code);
void wh_hide_bars(node *n);
void wh_bars_reset(node *n);
void wh_update_bars(node *n);
void wh_scroll_to_offset(node *n, float x, float y);
void wh_set_refreshable(node *n, bool on);
void wh_set_refreshing(node *n, bool on);
bool wh_refresh_key(MSG *m);
void wh_show_bars(node *n);
void wh_mouse_over(HWND hwnd);

// ---- list.c ----
void wh_list_new(node *n);
void wh_list_release(node *n);
void wh_list_resized(node *n);
void wh_list_set_prop(node *n, uint16_t p, const ak_value *v);
void wh_list_set_direction(node *n, uint8_t d);
void wh_list_rebind(node *n);
void wh_list_update(node *n, const uint8_t *args, size_t len);
void wh_list_scroll_to(node *n, uint32_t index);
void wh_list_scroll(node *n, float y);
float wh_list_offset(node *n);
float wh_list_content(node *n);
void wh_cell_set_height(node *n, float h);
void wh_cell_set_prop(node *n, uint16_t p, const ak_value *v);
void wh_paint_row(node *n, gfx g, float w, float h);
bool wh_select_click(node *n);
bool wh_row_menu(node *n);

// ---- nav.c: stacks, tabs, sheets, alerts, windows (and cmdbar.c,
// search.c, split.c, popover.c, snackbar.c, alertdialog.c) ----
bool wh_create_nav(node *n);
bool wh_destroy_nav(node *n);
void wh_nav_free(node *n);
bool wh_insert_nav(node *p, node *c, uint32_t index);
bool wh_remove_nav(node *p, node *c);
void wh_set_nav_prop(node *n, uint16_t p, const ak_value *v);
bool wh_nav_sized_by_host(node *n);
void wh_fill_screens(node *p);
HWND wh_nav_bar_window(node *p);
void wh_nav_set_direction(node *n, uint8_t d);
void wh_layout_screens(node *n);
void wh_layout_nav(void);
void wh_present(node *n);
void wh_back(node *root);
node *wh_root_of(HWND top);
void wh_paint_header(node *n, HDC dc, gfx g, RECT r);
void wh_paint_tab_bar(node *n, HDC dc, gfx g, RECT r);
uint16_t wh_header_button(node *n, HWND ctl, bool rtl, wchar_t *title, int cap);
void wh_header_command(node *n, HWND ctl);
void wh_paint_command_bar(node *n, gfx g, RECT r);
uint16_t wh_command_bar_button(node *n, HWND ctl, bool rtl, wchar_t *title, int cap);
bool wh_command_bar_is_primary(node *n, HWND ctl);
void wh_command_bar_command(node *n, HWND ctl);
void wh_search_event(node *n, ak_event *e);
void wh_paint_search_icon(node *n, HDC dc, RECT r);
void wh_make_split(node *n);
void wh_set_split_prop(node *n, uint16_t p, const ak_value *v);
void wh_layout_split(node *n);
void wh_split_release(node *n);  // its pane, with it
void wh_paint_pane(node *n, HDC dc, gfx g, RECT r);
// wh_pane_chosen is a SplitView pane's chosen item's title and badge
// (split.c), which screen readers read (a11y.c); false: none.
bool wh_pane_chosen(node *n, const wchar_t **title, const wchar_t **badge);
void wh_paint_snack(node *n, gfx g, float w, float h);
void wh_paint_snack_text(node *n, HDC dc);
bool wh_snack_is_close(node *n, HWND ctl);
void wh_snack_clicked(node *n, HWND ctl);
void wh_alert_field_event(node *n, ak_event *e);
void wh_alert_dialog_command(node *n, HWND ctl, UINT code);
bool wh_alert_dialog_default_button(node *n, HWND ctl);
HBRUSH wh_alert_dialog_colors(HDC dc);
void wh_paint_alert_dialog(node *n, gfx g, RECT r);

// ---- combo.c: Wheel and Picker ----
void wh_make_combo(node *n);
void wh_remake_combo(node *n);
void wh_set_combo_prop(node *n, uint16_t p, const ak_value *v);
void wh_combo_command(node *n, HWND ctl, UINT code);
void wh_layout_combo(node *n, RECT r);
void wh_measure_combo(node *n, float *w, float *h);
void wh_style_combo_list(node *n);
rgba wh_combo_list_bg(void);
rgba wh_combo_list_text(void);
bool wh_combo_draw_item(node *n, DRAWITEMSTRUCT *d);

// ---- datepicker.c ----
void wh_make_date_controls(node *n);
void wh_set_date_prop(node *n, uint16_t p, const ak_value *v);
void wh_layout_date(node *n, RECT r);
void wh_measure_date(node *n, float *w, float *h);
void wh_theme_date(node *n);
LRESULT wh_date_notify(node *n, NMHDR *hdr, LPARAM lp, bool *handled);

// ---- pager.c: PageControl ----
void wh_measure_pips(node *n, float *w, float *h);
void wh_paint_pips(node *n, gfx g, float w, float h);
int wh_pip_at(node *n, int x);
void wh_select_pip(node *n, int i);

// ---- image.c ----
void wh_image_new(node *n);
void wh_image_close(node *n);
void wh_set_image_prop(node *n, uint16_t p, const ak_value *v);
void wh_image_size(node *n, float *w, float *h);
void wh_paint_image(node *n, gfx g, float w, float h, float radius);
void wh_image_tint_changed(node *n);

// ---- shape.c, canvas.c, shader.c ----
void wh_shape_new(node *n);
void wh_shape_set_prop(node *n, uint16_t p, const ak_value *v);
void wh_shape_free(node *n);
void wh_paint_shape(node *n, gfx g);
void wh_paint_canvas(node *n, gfx g);
void wh_shader_new(node *n);
void wh_shader_release(node *n);
void wh_set_shader_prop(node *n, uint16_t p, const ak_value *v);
void wh_render_shader(node *n);
void wh_paint_shader(node *n, gfx g, float w, float h, float radius);

// ---- rich.c (RichText), richfield.c (a Rich TextField) ----
void wh_make_rich(node *n);
void wh_rich_release(node *n);
void wh_rich_set_prop(node *n, uint16_t p, const ak_value *v);
void wh_rich_measure(node *n, float cw, float ch, float aw, float ah, float *w, float *h);
void wh_rich_place(node *n);
void wh_rich_place_inlines(node *n);
void wh_rich_resized(node *n);
void wh_rich_invalidate(node *n);
LRESULT wh_rich_notify(node *n, NMHDR *hdr, LPARAM lp, bool *handled);
const wchar_t *wh_rich_field_class(node *n);
void wh_setup_rich_edit(node *n);
void wh_set_rich_text(node *n, const uint8_t *b, size_t len);
void wh_rich_edited(node *n);
int wh_visible_text(node *n, wchar_t *out, int cap);
LRESULT wh_field_notify(node *n, NMHDR *hdr, LPARAM lp, bool *handled);
void wh_format_rich(node *n, const uint8_t *args, size_t len);
void wh_set_rich_link(node *n, const char *url, size_t len);
void wh_field_free(node *n);
void wh_load_msftedit(void);
// wh_twips is a size in points as a rich edit's font size: twips at its
// window's scale k (0: the main window's), as the control converts them
// at the system's DPI.
int wh_twips(float size, float k);
void wh_open_link(node *n, const char *url, size_t len);

// A web method's answer as a callback (web.c's; modules.h's wh_reply is
// the modules' own): once, on the main
// thread; err NULL, else a message ("unsupported: ..." for what isn't).
typedef void (*wh_answer)(void *ctx, const uint8_t *b, size_t n, const char *err);

// ---- web.c: WebView2, Video ----
void wh_make_web(node *n);
void wh_web_set_prop(node *n, uint16_t p, const ak_value *v);
void wh_web_close(node *n, HWND hwnd);
void wh_web_resized(node *n);
// wh_web_view is a WebView's ICoreWebView2 (the host's), NULL until it's made.
void *wh_web_view(node *n);
void wh_web_command(node *n, uint16_t cmd, const uint8_t *args, size_t len);
void wh_web_color_scheme(node *n, bool dark);
// wh_web_environment calls fn with the app's WebView2 environment (an
// ICoreWebView2Environment *; NULL without the runtime), made once: what
// web views and the modules' hidden pages share.
void wh_web_environment(void (*fn)(void *ctx, void *env), void *ctx);
// wh_web_evaluate runs script in view's page (an ICoreWebView2 *: a hidden
// page's, the js module's) as a script of its own, answering its value as
// JSON (a promise's awaited) or what it threw, on a later turn.
void wh_web_evaluate(void *view, const char *script, size_t len, wh_answer reply, void *ctx);
// wh_web_module runs the web module's method (eval, cookies): false if it
// has no such method.
bool wh_web_module(const char *method, size_t method_len, const uint8_t *args, size_t len, wh_answer reply, void *ctx);

// ---- native.c: Native views ----
void wh_set_native_prop(node *n, uint16_t p, const ak_value *v);
void wh_destroy_native(node *n);
void wh_measure_native(node *n, float w, float h, float *ow, float *oh);

// ---- clip.c, picture.c, material.c, transform.c ----
void wh_shape_clip(node *n);
void wh_clip_tree(node *n, HDC dc);
void wh_paint_clip_corners(node *n, HWND hwnd, HDC dc);
void wh_corners_painted(HWND hwnd);
void wh_update_picture(node *n);
void wh_place_picture(node *n);
void wh_drop_picture(node *n);
void wh_paint_picture(node *n, HDC dc, bool own);
void wh_pictures_stale(HWND hwnd);
bool wh_tree_in_picture(node *n);
void wh_set_material(node *n, int32_t m);
void wh_paint_material(node *n, HDC dc, int w, int h);
void wh_materials_behind(HWND hwnd);
void wh_set_transform(node *n, const uint8_t *b, size_t len);
void wh_transformed(node *n, float x, float y, float w, float h, int *x0, int *y0, int *x1, int *y1);
float wh_k_for(float k, float zoom);
float wh_own_zoom(const node *n);

// ---- transition.c, match.c: views coming and going ----
bool wh_enter_transition(node *n, const float f[4]);
bool wh_exit_transition(node *n);
bool wh_in_exiting(HWND hwnd);
void wh_exiting_forget(HWND hwnd);
void wh_gone_later(node *n);
void wh_set_match(node *n, ak_span key);
bool wh_match_out(node *n);
void wh_match_in(node *n);
void wh_match_batch_done(void);
void wh_run_matches(void);

#endif
