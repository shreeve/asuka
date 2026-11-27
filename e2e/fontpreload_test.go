//go:build e2e

package e2e

import (
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

// fontPreloadFrames records, every 10 ms from the page's start while the
// line in the app's font is shown, how it's drawn and laid out: its
// glyphs' width, its box's size and where its status beside it starts
// (asukaFrames). A timer, not animation frames: a page in headless Chrome
// draws frames only when something waits for one.
const fontPreloadFrames = `
globalThis.asukaFrames = [];
globalThis.asukaLeaf = (text) => [...document.querySelectorAll(".asuka *")].find((e) => e.childElementCount === 0 && e.textContent === text);
globalThis.asukaShape = () => {
  const el = asukaLeaf("Hamburgefonstiv"), status = asukaLeaf("status: ready");
  if (!el || !status) return "";
  const r = document.createRange();
  r.selectNodeContents(el);
  const b = el.getBoundingClientRect();
  return Math.round(r.getBoundingClientRect().width) + "/" + Math.round(b.width) + "x" + Math.round(b.height) + "/" + Math.round(status.getBoundingClientRect().left);
};
setInterval(() => {
  const el = asukaLeaf("Hamburgefonstiv");
  if (el && el.checkVisibility({ opacityProperty: true, visibilityProperty: true })) asukaFrames.push(asukaShape());
}, 10);
`

// TestFontPreloadWeb loads e2e/testdata/fontpreload, with the gallery's
// font files copied beside it, in headless Chrome with every request 150
// ms away. The page preloads the app's font file beside the core, so its
// first screen is drawn in it: no frame shows the line, or its status
// beside it, drawn and laid out in a fallback font first.
func TestFontPreloadWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	repo := root(t)
	dir := filepath.Join(t.TempDir(), "fontpreload")
	copies := map[string]string{
		"e2e/testdata/fontpreload/app.asuka":                 "app.asuka",
		"e2e/testdata/fontpreload/asuka.toml":                "asuka.toml",
		"examples/gallery/assets/fonts/Pacifico-Regular.ttf": "assets/fonts/Pacifico-Regular.ttf",
		"examples/gallery/assets/fonts/OFL.txt":              "assets/fonts/OFL.txt",
	}
	for from, to := range copies {
		b, err := os.ReadFile(filepath.Join(repo, from))
		if err != nil {
			t.Fatal(err)
		}
		os.MkdirAll(filepath.Dir(filepath.Join(dir, to)), 0o755)
		if err := os.WriteFile(filepath.Join(dir, to), b, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	pageURL := webServer(t, dir)
	c := launchCDP(t, browser)
	var created struct{ TargetID string }
	if err := c.call("", "Target.createTarget", map[string]any{"url": "about:blank"}, &created, ""); err != nil {
		t.Fatal(err)
	}
	page := c.attach(t, created.TargetID)
	var res json.RawMessage
	for _, step := range []struct {
		method string
		params map[string]any
	}{
		{"Page.enable", map[string]any{}},
		{"Page.addScriptToEvaluateOnNewDocument", map[string]any{"source": fontPreloadFrames}},
		{"Network.enable", map[string]any{}},
		// A request's round trip, as on a network: a font asked for only
		// once the first screen is laid out comes after it.
		{"Network.emulateNetworkConditions", map[string]any{"offline": false, "latency": 150, "downloadThroughput": -1, "uploadThroughput": -1}},
		{"Page.navigate", map[string]any{"url": pageURL}},
	} {
		if err := c.call(page, step.method, step.params, &res, ""); err != nil {
			t.Fatal(err)
		}
	}
	shown := `(() => { const el = globalThis.asukaLeaf?.("Hamburgefonstiv"); return el ? String(el.checkVisibility({ opacityProperty: true, visibilityProperty: true })) : ""; })()`
	until(t, func() bool { return c.eval(t, page, shown) == "true" }, func() string { return c.eval(t, page, "document.body.innerText") })
	got := c.eval(t, page, `(async () => {
	  await new Promise((r) => setTimeout(r, 600));
	  const now = asukaShape();
	  return JSON.stringify({ now, before: [...new Set(asukaFrames.filter((s) => s !== now))] });
	})()`)
	var shapes struct {
		Now    string
		Before []string
	}
	if err := json.Unmarshal([]byte(got), &shapes); err != nil {
		t.Fatal(err)
	}
	if len(shapes.Before) > 0 {
		t.Errorf("in the app's font the line is drawn and laid out as %s (glyphs' width/box/where its status starts), but was shown as %v before", shapes.Now, shapes.Before)
	}
}
