//go:build e2e

package e2e

import (
	"encoding/json"
	"fmt"
	"strings"
	"testing"
)

// asuka.toml's [web.colors] is the web host's palette
// (e2e/testdata/web-colors): a Text that names no color is in its label,
// a field's text in its label and its placeholder in its tertiary label,
// a stepper's divider in its separator; each its dark color when the
// browser prefers dark and when the app shows itself dark, and the light
// one again when the app follows a light system. A color with no dark one
// (tertiaryLabel) is the same in both.
func TestWebColorsWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	url := webServer(t, "e2e/testdata/web-colors")
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
	js := `(() => {
	  const text = [...document.querySelectorAll(".asuka-text")].find((e) => e.textContent === "Plain");
	  const field = document.querySelector("input.asuka-field");
	  const step = document.querySelector(".asuka-stepper button + button");
	  return JSON.stringify({
	    text: text ? getComputedStyle(text).color : "",
	    field: field ? getComputedStyle(field).color : "",
	    placeholder: field ? getComputedStyle(field, "::placeholder").color : "",
	    separator: step ? getComputedStyle(step).boxShadow : "",
	  });
	})()`
	colors := func(label, separator string) {
		t.Helper()
		want := map[string]string{"text": label, "field": label, "placeholder": "rgb(162, 165, 170)", "separator": separator}
		var got map[string]string
		until(t, func() bool {
			got = nil
			if err := json.Unmarshal([]byte(c.eval(t, page, js)), &got); err != nil {
				t.Fatal(err)
			}
			for k, w := range want {
				if !strings.Contains(got[k], w) {
					return false
				}
			}
			return true
		}, func() string { return fmt.Sprintf("got %v, want %v", got, want) })
	}
	const label, separator = "rgb(22, 24, 29)", "rgb(223, 225, 229)"
	const labelDark, separatorDark = "rgb(232, 233, 235)", "rgb(58, 61, 66)"
	colors(label, separator)
	scheme("dark")
	colors(labelDark, separatorDark)
	scheme("light")
	colors(label, separator)
	s.tap("Dark")
	colors(labelDark, separatorDark)
	s.tap("System")
	colors(label, separator)
}
