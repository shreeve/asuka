package main

import (
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// The page finds its files from any route's path: its <base href> is the
// site's base.
func TestWebIndexBase(t *testing.T) {
	for base, want := range map[string]string{"": "/", "/": "/", "app": "/app/", "/app/": "/app/", "/a/b": "/a/b/"} {
		page := webIndex(webPage{Name: "Notes", Lang: "en", Base: base, Host: "asuka/x", Core: "asuka.wasm"})
		if !strings.Contains(page, `<base href="`+want+`">`) || strings.Index(page, "<base") > strings.Index(page, "<link") && strings.Contains(page, "<link") {
			t.Errorf("base %q: %s", base, page)
		}
	}
}

// The page downloads the app's font files as it starts, beside the core:
// a preload each, which the host's FontFace then finds.
func TestWebIndexPreloadsFonts(t *testing.T) {
	page := webIndex(webPage{Name: "A", Host: "asuka", Core: "asuka.wasm", Options: map[string]any{"fonts": []string{"Inter-Regular.woff2", "Brand.ttf"}}})
	for _, f := range []string{"Inter-Regular.woff2", "Brand.ttf"} {
		if want := `<link rel="preload" href="assets/fonts/` + f + `" as="font" crossorigin>`; !strings.Contains(page, want) {
			t.Errorf("no %s in:\n%s", want, page)
		}
	}
	if page := webIndex(webPage{Name: "A", Host: "asuka", Core: "asuka.wasm"}); strings.Contains(page, `as="font"`) {
		t.Errorf("an app without fonts preloads one:\n%s", page)
	}
}

// A static site serves the page for its routes' paths (Netlify's
// rewrite), and its headers are under its base.
func TestWebSiteRewritesAndHeaders(t *testing.T) {
	if got := webRedirects("/app/"); got != "/app/*  /app/index.html  200\n" {
		t.Fatalf("_redirects %q", got)
	}
	h := webHeaders("/app/")
	if !strings.HasPrefix(h, "/app/index.html\n  Cache-Control: no-cache\n") || !strings.Contains(h, "/app/*.wasm\n  Cache-Control: public, max-age=31536000, immutable\n  Content-Type: application/wasm\n") {
		t.Fatalf("_headers:\n%s", h)
	}
	if strings.Contains(webHeaders("/"), "//") {
		t.Fatalf("_headers:\n%s", webHeaders("/"))
	}
}

// asuka dev serves the page at a route's path too (a reload, a link
// pasted), and still a 404 for a file that isn't there.
func TestDevServerServesRoutes(t *testing.T) {
	s := &devServer{app: &asukaApp{cfg: &appConfig{ID: "com.example.notes", Name: "Notes"}}}
	noSQLite := ""
	s.sqlite.Store(&noSQLite)
	var none []webScript
	s.scripts.Store(&none)
	for _, c := range []struct {
		path, accept string
		code         int
	}{
		{"/chat/7", "text/html,application/xhtml+xml", 200},
		{"/files/docs/a.md", "text/html", 200},
		{"/nothing.js", "*/*", 404},
	} {
		r := httptest.NewRequest(http.MethodGet, c.path, nil)
		r.Header.Set("Accept", c.accept)
		w := httptest.NewRecorder()
		s.ServeHTTP(w, r)
		if w.Code != c.code || (c.code == 200 && !strings.Contains(w.Body.String(), `<base href="/">`)) {
			t.Errorf("%s: %d\n%s", c.path, w.Code, w.Body.String())
		}
	}
}

// The splash's colors are the page's until the app's first frame when
// asuka.toml sets its background: the light one in dark mode too without
// a dark one. A splash with an image alone, or none, leaves the page
// without a launch style.
func TestWebIndexLaunch(t *testing.T) {
	page := func(toml string) string {
		t.Helper()
		dir := t.TempDir()
		if err := os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(toml), 0o644); err != nil {
			t.Fatal(err)
		}
		c, err := loadConfig(dir)
		if err != nil {
			t.Fatal(err)
		}
		return webIndex(webPage{Name: "A", Host: "asuka", Core: "asuka.wasm", Launch: launchColors(c)})
	}
	const sel = "html:not(:has(.asuka-drawn)), .asuka-page:not(.asuka-drawn)"
	for _, c := range []struct {
		toml        string
		light, dark string
	}{
		{"[splash]\nbackground = \"#f5f7f9\"\ndarkBackground = \"#0e0f12\"", "#f5f7f9", "#0e0f12"},
		{"[splash]\nbackground = \"#f5f7f9\"", "#f5f7f9", "#f5f7f9"},
	} {
		p := page(c.toml)
		for _, want := range []string{
			sel + " { background: " + c.light + "; }",
			"@media (prefers-color-scheme: dark) { " + sel + " { background: " + c.dark + "; } }",
		} {
			if !strings.Contains(p, want) {
				t.Errorf("%q: the page lacks %q:\n%s", c.toml, want, p)
			}
		}
	}
	for _, toml := range []string{"[splash]\nimage = \"logo.png\"", `name = "A"`} {
		if strings.Contains(page(toml), "asuka-drawn") {
			t.Errorf("%q: the page has a launch style", toml)
		}
	}
}
