//go:build e2e

package e2e

import (
	"encoding/json"
	"testing"
)

// asuka.toml's accentDark is the accent in dark mode on the web
// (e2e/testdata/accent-dark): the page's controls take the accent in light
// mode, the dark one when the browser prefers dark and when the app shows
// itself dark, and the accent again when it follows a light system.
func TestAccentDarkWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	url := webServer(t, "e2e/testdata/accent-dark")
	c := launchCDP(t, browser)
	var created struct{ TargetID string }
	if err := c.call("", "Target.createTarget", map[string]any{"url": "about:blank"}, &created, ""); err != nil {
		t.Fatal(err)
	}
	page := c.attach(t, created.TargetID)
	var res json.RawMessage
	scheme := func(s string) {
		t.Helper()
		features := []map[string]string{{"name": "prefers-color-scheme", "value": s}}
		if err := c.call(page, "Emulation.setEmulatedMedia", map[string]any{"features": features}, &res, ""); err != nil {
			t.Fatal(err)
		}
	}
	scheme("light")
	if err := c.call(page, "Page.navigate", map[string]any{"url": url}, &res, ""); err != nil {
		t.Fatal(err)
	}
	s := &webScreen{t: t, url: url}
	showsLine(t, s, "ready")
	// The controls' accent-color, and the color of a Text in .accent.
	js := `(() => {
	  const root = document.querySelector(".asuka");
	  const text = [...document.querySelectorAll(".asuka-text")].find((e) => e.textContent === "Accent");
	  return JSON.stringify([root ? getComputedStyle(root).accentColor : "", text ? getComputedStyle(text).color : ""]);
	})()`
	accent := func(want string) {
		t.Helper()
		var got []string
		until(t, func() bool {
			got = nil
			if err := json.Unmarshal([]byte(c.eval(t, page, js)), &got); err != nil {
				t.Fatal(err)
			}
			return len(got) == 2 && got[0] == want && got[1] == want
		}, func() string { b, _ := json.Marshal(got); return "accent-color, text: " + string(b) + ", want " + want })
	}
	const light, dark = "rgb(37, 99, 235)", "rgb(147, 197, 253)"
	accent(light)
	scheme("dark")
	accent(dark)
	scheme("light")
	accent(light)
	s.tap("Dark")
	accent(dark)
	s.tap("System")
	accent(light)
}
