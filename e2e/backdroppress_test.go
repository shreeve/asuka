//go:build e2e

package e2e

import (
	"encoding/json"
	"fmt"
	"strings"
	"testing"
	"time"
)

// A sheet's backdrop dismisses it only when pressed there
// (e2e/testdata/backdroppress). A tap on a pressable view that opens a
// sheet as it ends is followed in iOS Safari by its click, hit-tested at
// the finger once the sheet shows: on the backdrop, though the press
// began on the view. A click on the dialog with no press there before
// it, as that one, leaves the sheet open (a press inside it then counts);
// a press on the backdrop still dismisses it.

// pageEval runs js in the page and decodes its value into v.
func pageEval(d *devtools, js string, v any) error {
	var r struct {
		Result struct{ Value json.RawMessage }
	}
	if err := d.call("Runtime.evaluate", map[string]any{"expression": js, "returnByValue": true}, &r); err != nil {
		return err
	}
	return json.Unmarshal(r.Result.Value, v)
}

// pageClick presses the mouse at x, y and lets it go.
func pageClick(d *devtools, x, y float64) error {
	for _, typ := range []string{"mousePressed", "mouseReleased"} {
		var r struct{}
		if err := d.call("Input.dispatchMouseEvent", map[string]any{"type": typ, "x": x, "y": y, "button": "left", "clickCount": 1}, &r); err != nil {
			return err
		}
	}
	return nil
}

// textCenter is where the visible text t is in the page; false while it
// isn't.
func textCenter(d *devtools, t string) (float64, float64, bool) {
	var p []float64
	js := fmt.Sprintf(`(() => { const e = [...document.querySelectorAll("*")].filter(e => e.childElementCount === 0 && e.textContent.trim() === %q && e.getClientRects().length).pop(); if (!e) return []; const r = e.getBoundingClientRect(); return [r.x + r.width / 2, r.y + r.height / 2]; })()`, t)
	if pageEval(d, js, &p) != nil || len(p) != 2 {
		return 0, 0, false
	}
	return p[0], p[1], true
}

// waitText waits (bounded) until the page shows one of texts, and says
// which.
func waitText(t *testing.T, d *devtools, texts ...string) string {
	t.Helper()
	deadline := time.Now().Add(within)
	for time.Now().Before(deadline) {
		var body string
		if pageEval(d, "document.body.innerText", &body) == nil {
			for _, s := range texts {
				if strings.Contains(body, s) {
					return s
				}
			}
		}
		time.Sleep(100 * time.Millisecond)
	}
	t.Fatalf("the page never showed any of %q", texts)
	return ""
}

// settle waits (bounded) until nothing in the page is moving: a sheet
// sliding in or out.
func settle(t *testing.T, d *devtools) {
	t.Helper()
	deadline := time.Now().Add(within)
	for time.Now().Before(deadline) {
		var moving int
		if pageEval(d, `document.getAnimations().filter(a => a.playState === "running").length`, &moving) == nil && moving == 0 {
			return
		}
		time.Sleep(50 * time.Millisecond)
	}
	t.Fatal("the page never stopped moving")
}

// pressText presses the visible text once nothing in the page is moving,
// so it's pressed where it rests.
func pressText(t *testing.T, d *devtools, text string) {
	t.Helper()
	settle(t, d)
	deadline := time.Now().Add(within)
	for time.Now().Before(deadline) {
		if x, y, ok := textCenter(d, text); ok {
			if err := pageClick(d, x, y); err != nil {
				t.Fatal(err)
			}
			return
		}
		time.Sleep(100 * time.Millisecond)
	}
	t.Fatalf("%q never showed", text)
}

func TestBackdropPressWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	url := webServer(t, "e2e/testdata/backdroppress")
	d := devtoolsPage(t, browser, url, "1")
	pressText(t, d, "Open")
	waitText(t, d, "Inside")
	// iOS Safari's click after the tap: on the dialog, no press before it.
	var ok bool
	if err := pageEval(d, `(() => { const d = document.querySelector("dialog[open]"); if (!d) return false; d.dispatchEvent(new MouseEvent("click", { bubbles: true, cancelable: true })); return true; })()`, &ok); err != nil || !ok {
		t.Fatalf("no open dialog to click: %v", err)
	}
	// Once a closing sheet would have closed, its press inside the sheet
	// counts, or it was dismissed.
	settle(t, d)
	var shown string
	pageEval(d, "document.body.innerText", &shown)
	if strings.Contains(shown, "dismissed 1") {
		t.Fatalf("a click on the backdrop with no press there dismissed the sheet")
	}
	pressText(t, d, "Inside")
	if got := waitText(t, d, "inside 1", "dismissed 1"); got != "inside 1" {
		t.Fatalf("a click on the backdrop with no press there dismissed the sheet")
	}
	var body string
	pageEval(d, "document.body.innerText", &body)
	if !strings.Contains(body, "dismissed 0") {
		t.Fatalf("the sheet was dismissed by a click with no press on its backdrop:\n%s", body)
	}
	// A press on the backdrop, at the page's top left corner, dismisses it.
	if err := pageClick(d, 4, 4); err != nil {
		t.Fatal(err)
	}
	waitText(t, d, "dismissed 1")
}
