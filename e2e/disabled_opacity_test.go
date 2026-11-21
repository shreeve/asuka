//go:build e2e

package e2e

import (
	"encoding/json"
	"testing"
)

// A disabled control is drawn at one opacity on the web, whatever its kind
// (e2e/testdata/disabled-opacity): a button, a field, a switch, a
// segmented control's choices and a stepper's buttons all at the 0.5 a
// disabled view has.
func TestDisabledOpacityWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	url := webServer(t, "e2e/testdata/disabled-opacity")
	c := launchCDP(t, browser)
	var created struct{ TargetID string }
	if err := c.call("", "Target.createTarget", map[string]any{"url": "about:blank"}, &created, ""); err != nil {
		t.Fatal(err)
	}
	page := c.attach(t, created.TargetID)
	var res json.RawMessage
	if err := c.call(page, "Page.navigate", map[string]any{"url": url}, &res, ""); err != nil {
		t.Fatal(err)
	}
	s := &webScreen{t: t, url: url}
	showsLine(t, s, "ready")
	// each disabled control's opacity, by what it is; "" while one isn't
	// drawn yet
	js := `(() => {
	  const o = (el) => el ? getComputedStyle(el).opacity : "";
	  const buttons = [...document.querySelectorAll(".asuka-button")];
	  return JSON.stringify({
	    plain: o(buttons.find((b) => b.textContent === "Plain")),
	    filled: o(buttons.find((b) => b.textContent === "Filled")),
	    field: o(document.querySelector("input.asuka-field")),
	    toggle: o(document.querySelector(".asuka-switch")),
	    segment: o(document.querySelector(".asuka-segmented button")),
	    stepper: o(document.querySelector(".asuka-stepper button")),
	  });
	})()`
	var got map[string]string
	until(t, func() bool {
		got = nil
		if err := json.Unmarshal([]byte(c.eval(t, page, js)), &got); err != nil {
			t.Fatal(err)
		}
		for _, v := range got {
			if v == "" {
				return false
			}
		}
		return true
	}, func() string { b, _ := json.Marshal(got); return string(b) })
	for kind, v := range got {
		if v != "0.5" {
			t.Errorf("a disabled %s is at opacity %s, not 0.5", kind, v)
		}
	}
}
