// app.c: the app's settings (asuka-app.json, which the CLI writes beside
// the executable) and its directories (each platform's place for them)
// (desktop.h).
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L  // strdup, readlink under -std=c17
#endif
#include "desktop.h"

#include <stdlib.h>
#include <string.h>

static char *dup_or_null(const char *s) { return s && *s ? strdup(s) : NULL; }

void dsk_config_load(dsk_config *c) {
	memset(c, 0, sizeof *c);
	char *path = dsk_getenv("ASUKA_APP_CONFIG");
	if (!path) {
		char *exe = dsk_exe_path();
		char *dir = exe ? dsk_dir(exe) : NULL;
		path = dir ? dsk_join(dir, "asuka-app.json") : NULL;
		if (path && !dsk_exists(path) && exe) {
			// An install prefix (Linux): bin/<exe>, share/<exe>/asuka-app.json.
			char *share = dsk_join(dir, "../share");
			char *app = dsk_join(share, dsk_base(exe));
			char *p = dsk_join(app, "asuka-app.json");
			if (dsk_exists(p)) {
				free(path);
				path = p;
			} else {
				free(p);
			}
			free(share);
			free(app);
		}
		free(exe);
		free(dir);
	}
	size_t n;
	char *text = dsk_read_file(path, &n);
	free(path);
	dsk_json *j = dsk_json_parse(text, n);
	free(text);
	if (!j) return;
	c->name = dup_or_null(dsk_json_str(j, "name", NULL));
	c->id = dup_or_null(dsk_json_str(j, "id", NULL));
	c->version = dup_or_null(dsk_json_str(j, "version", NULL));
	c->build = dup_or_null(dsk_json_str(j, "build", NULL));
	c->accent = dup_or_null(dsk_json_str(j, "accent", NULL));
	c->accent_dark = dup_or_null(dsk_json_str(j, "accentDark", NULL));
	c->scheme = dup_or_null(dsk_json_str(j, "scheme", NULL));
	c->file_types = dup_or_null(dsk_json_str(j, "fileTypes", NULL));
	c->lang = dup_or_null(dsk_json_str(j, "lang", NULL));
	c->direction = dup_or_null(dsk_json_str(j, "direction", NULL));
	c->background = dsk_json_bool(j, "background", false);
	c->keep_running = dsk_json_bool(j, "keepRunning", false);
	c->multiple_instances = dsk_json_bool(j, "multipleInstances", false);
	c->update_feed = dup_or_null(dsk_json_str(j, "updateFeed", NULL));
	c->update_key = dup_or_null(dsk_json_str(j, "updateKey", NULL));
	c->update_target = dup_or_null(dsk_json_str(j, "updateTarget", NULL));
	c->alternate_icons = dup_or_null(dsk_json_str(j, "alternateIcons", NULL));
	dsk_json_free(j);
}

// xdg is an XDG base directory: the variable's if it's absolute (the spec
// ignores a relative one), else home's fallback.
static char *xdg(const char *env, const char *fallback) {
	char *d = dsk_getenv(env);
	if (d && d[0] == '/') return d;
	free(d);
	char *home = dsk_getenv("HOME");
	if (!home) return NULL;
	char *p = dsk_join(home, fallback);
	free(home);
	return p;
}

void dsk_dirs_find(dsk_dirs *d, int platform, const char *data) {
	memset(d, 0, sizeof *d);
	d->data = dsk_getenv("ASUKA_DATA");
	if (!d->data) d->data = strdup(data);
	const char *name = dsk_base(d->data);
	d->config = dsk_getenv("ASUKA_CONFIG");
	if (!d->config) {
		if (platform == DSK_WINDOWS) {
			char *roaming = dsk_getenv("APPDATA");
			if (roaming) d->config = dsk_join(roaming, name);
			free(roaming);
		} else {
			char *x = xdg("XDG_CONFIG_HOME", ".config");
			if (x) d->config = dsk_join(x, name);
			free(x);
		}
		if (!d->config) d->config = strdup(d->data);
	}
	d->cache = dsk_getenv("ASUKA_CACHE");
	if (!d->cache) {
		if (platform == DSK_WINDOWS) {
			d->cache = dsk_join(d->data, "Cache");
		} else {
			char *x = xdg("XDG_CACHE_HOME", ".cache");
			d->cache = x ? dsk_join(x, name) : dsk_join(d->data, "cache");
			free(x);
		}
	}
	d->logs = dsk_getenv("ASUKA_LOGS");
	if (!d->logs) {
		if (platform == DSK_WINDOWS) {
			d->logs = dsk_join(d->data, "Logs");
		} else {
			char *x = xdg("XDG_STATE_HOME", ".local/state");
			d->logs = x ? dsk_join(x, name) : dsk_join(d->data, "logs");
			free(x);
		}
	}
}

void dsk_config_free(dsk_config *c) {
	char **s[] = {&c->name, &c->id, &c->version, &c->build, &c->accent, &c->accent_dark, &c->scheme, &c->file_types, &c->lang,
		&c->direction, &c->update_feed, &c->update_key, &c->update_target, &c->alternate_icons};
	for (size_t i = 0; i < sizeof s / sizeof *s; i++) free(*s[i]);
	memset(c, 0, sizeof *c);
}

void dsk_dirs_free(dsk_dirs *d) {
	free(d->data);
	free(d->config);
	free(d->cache);
	free(d->logs);
	memset(d, 0, sizeof *d);
}
