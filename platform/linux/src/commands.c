// commands.c: the core's commands to a view (focus, present, animate,
// a module's call…) and to the root (the menu bar, the window's options,
// the accent, the locale, quitting, asuka dev's error overlay).
#include "host.h"

#include <stdlib.h>

void lx_copy_text(const char *text);
void lx_share(node *n, ak_span text);

void lx_copy_text(const char *text) { gdk_clipboard_set_text(gdk_display_get_clipboard(gdk_display_get_default()), text); }

void lx_command(node *n, ak_cmd c, ak_span args) {
	ak_reader r = {(const uint8_t *)args.p, args.len, 0, false};
	switch (c) {
	case AK_CMD_FOCUS:
	case AK_CMD_BLUR:
		lx_focus_command(n, c == AK_CMD_FOCUS);
		return;
	case AK_CMD_INSERT_TEXT:
		if (n->kind == AK_KIND_TEXT_FIELD) lx_insert_text(n, args);
		return;
	case AK_CMD_PRESENT:
		if (n->kind == AK_KIND_OVERLAY)
			lx_command_more(n, c, args);  // over its root's others (overlay.c)
		else
			lx_present(n);
		return;
	case AK_CMD_HAPTIC:
		return;  // desktops have no haptics
	case AK_CMD_ANIMATE_BEGIN:
		lx_begin_animation(args);
		return;
	case AK_CMD_ANIMATE_END:
		lx_end_animation();
		return;
	case AK_CMD_OPEN_URL: {
		char *u = str_dup(args);
		lx_open_url(u);
		free(u);
		return;
	}
	case AK_CMD_CALL:
		lx_call(n, args);
		return;
	case AK_CMD_COPY: {
		char *t = str_dup(args);
		lx_copy_text(t);
		free(t);
		return;
	}
	case AK_CMD_FRAMES:
		lx_set_frames(args.len > 0 && args.p[0] != 0);
		return;
	case AK_CMD_ANNOUNCE:
		lx_announce(args);
		return;
	case AK_CMD_DEV_ERROR:
		lx_dev_error(n, args);
		return;
	case AK_CMD_DEV_ERROR_CLEAR:
		lx_dev_error_clear();
		return;
	case AK_CMD_DEV_HIGHLIGHT:
		lx_dev_highlight(n, args);
		return;
	case AK_CMD_DEV_HIGHLIGHT_CLEAR:
		lx_dev_highlight_clear();
		return;
	case AK_CMD_REPLACE:
		if (n->nav) n->nav->pending = -2;
		return;
	case AK_CMD_A11Y_FOCUS:
		if (n->widget) gtk_widget_grab_focus(n->widget);
		return;
	case AK_CMD_SHARE:
		lx_share(n, args);
		return;
	case AK_CMD_SCROLL_TO_OFFSET: {
		double x = ak_read_f32(&r), y = ak_read_f32(&r);
		bool animated = ak_read_u8(&r) != 0;
		if (n->scroll)
			lx_scroll_to(n, x, y, animated);
		else
			lx_list_scroll_to_offset(n, x, y, animated);
		return;
	}
	case AK_CMD_ACCENT:
		H.accent = ak_read_u64(&r);
		ak_read_u8(&r);  // Android's wallpaper colors
		H.accent_dark = ak_read_u64(&r);  // 0 past the end: accent's
		lx_update_css();
		lx_theme_changed();
		return;
	case AK_CMD_MENU_BAR:
		lx_set_menu_bar(args);
		return;
	case AK_CMD_WINDOW_OPTIONS:
		lx_set_window_options(args);
		return;
	case AK_CMD_WINDOW_ACTION: {
		win *w = lx_win_for(n);
		if (w) lx_window_action(w, args);
		return;
	}
	case AK_CMD_LOCATION:
		lx_route_location(n, args);
		return;
	case AK_CMD_WINDOW_TITLE:
		if (H.main) {
			if (args.len)
				str_set(&H.main->name, args);
			else
				str_setc(&H.main->name, H.app_name);
			lx_update_title(H.main);
		}
		return;
	case AK_CMD_LOCALE:
		lx_set_locale(args);
		return;
	case AK_CMD_SAVE_LOCALE:
		lx_save_locale(args);
		return;
	case AK_CMD_COLOR_SCHEME:
		if (args.len) {
			AdwColorScheme scheme = ADW_COLOR_SCHEME_DEFAULT;
			if ((uint8_t)args.p[0] == AK_COLOR_SCHEME_LIGHT) scheme = ADW_COLOR_SCHEME_FORCE_LIGHT;
			if ((uint8_t)args.p[0] == AK_COLOR_SCHEME_DARK) scheme = ADW_COLOR_SCHEME_FORCE_DARK;
			adw_style_manager_set_color_scheme(adw_style_manager_get_default(), scheme);
		}
		return;
	case AK_CMD_QUIT:
		lx_quit_command(args);
		return;
	}
	if (n->kind == AK_KIND_TEXT_FIELD) {
		lx_field_command(n, c, args);
		return;
	}
	lx_command_more(n, c, args);
}
