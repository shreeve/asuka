//go:build e2e

package e2e

import (
	"encoding/json"
	"fmt"
	"runtime"
	"strconv"
	"testing"
	"time"
)

// TestDialogSelectWeb runs e2e/testdata/dialog-select in headless Chrome,
// with DevTools' trusted mouse and key events:
//
//   - Select all (Cmd+A on a Mac, Ctrl+A elsewhere) in a sheet selects one
//     selectable text, the one last pressed, not the sheet's title, its
//     buttons or the page.
//   - A sheet's own text (its title) isn't selectable: a double click on
//     it selects nothing.
//   - A sheet shows no focus ring of its own.
func TestDialogSelectWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	pageURL := webServer(t, "e2e/testdata/dialog-select")
	c := launchCDP(t, browser)
	var created struct{ TargetID string }
	if err := c.call("", "Target.createTarget", map[string]any{"url": "about:blank"}, &created, ""); err != nil {
		t.Fatal(err)
	}
	page := c.attach(t, created.TargetID)
	var res json.RawMessage
	if err := c.call(page, "Page.navigate", map[string]any{"url": pageURL}, &res, ""); err != nil {
		t.Fatal(err)
	}
	s := &webScreen{t: t, url: pageURL}
	showsLine(t, s, "status: ready")

	c.eval(t, page, `window.__rect = (t) => {
		let best = null;
		for (const el of document.querySelectorAll(".asuka-view, button")) {
			if (!el.textContent.trim().startsWith(t) || !el.getClientRects().length) continue;
			if (!best || best.contains(el)) best = el;
		}
		if (!best) return "";
		const r = best.getBoundingClientRect();
		return [r.left, r.top, r.width, r.height].map(Math.round).join(" ");
	}; "ok"`)
	center := func(text string) (x, y float64) {
		t.Helper()
		var r string
		until(t, func() bool { r = c.eval(t, page, `__rect(`+strconv.Quote(text)+`)`); return r != "" }, func() string { return "no view shows " + text })
		var w, h float64
		fmt.Sscan(r, &x, &y, &w, &h)
		return x + w/2, y + h/2
	}
	click := func(x, y float64, count int) {
		t.Helper()
		for i := 1; i <= count; i++ {
			for _, typ := range []string{"mouseMoved", "mousePressed", "mouseReleased"} {
				p := map[string]any{"type": typ, "x": x, "y": y, "button": "left", "clickCount": i}
				if typ == "mouseMoved" {
					p["button"] = "none"
				}
				if err := c.call(page, "Input.dispatchMouseEvent", p, &res, ""); err != nil {
					t.Fatal(err)
				}
			}
		}
	}
	selectAll := func() {
		t.Helper()
		mods := 2 // Ctrl
		if runtime.GOOS == "darwin" {
			mods = 4 // Meta
		}
		for _, typ := range []string{"rawKeyDown", "keyUp"} {
			p := map[string]any{"type": typ, "key": "a", "code": "KeyA", "windowsVirtualKeyCode": 65, "modifiers": mods}
			if err := c.call(page, "Input.dispatchKeyEvent", p, &res, ""); err != nil {
				t.Fatal(err)
			}
		}
	}
	selection := func() string { return c.eval(t, page, `getSelection().toString()`) }

	x, y := center("Open")
	click(x, y, 1)
	center("Sheet title")
	open := func() bool { return c.eval(t, page, `String(!!document.querySelector("dialog[open]"))`) == "true" }
	until(t, open, func() string { return "the sheet didn't open" })
	// It's opened when it has stopped moving.
	last := ""
	until(t, func() bool {
		r := c.eval(t, page, `__rect("Sheet title")`)
		done := r == last
		last = r
		time.Sleep(100 * time.Millisecond)
		return done
	}, func() string { return "the sheet keeps moving" })

	// A double click on the title selects nothing.
	x, y = center("Sheet title")
	click(x, y, 2)
	if got := selection(); got != "" {
		t.Errorf("a double click on the sheet's title selects %q: a sheet's own text is selectable", got)
	}
	if us := c.eval(t, page, `(() => { const d = document.querySelector("dialog[open]"); const e = [...d.querySelectorAll(".asuka-text")].find((e) => e.textContent === "Sheet title"); const s = getComputedStyle(e); return s.userSelect || s.webkitUserSelect; })()`); us != "none" {
		t.Errorf("the sheet's title is user-select %q, want none", us)
	}

	// Select all selects the selectable text last pressed, of the two.
	x, y = center("First line")
	click(x, y, 1)
	selectAll()
	if got, want := selection(), "First line of the body\nSecond line of the body"; got != want {
		t.Errorf("select all after a press in the body selects %q, want %q", got, want)
	}
	x, y = center("Another")
	click(x, y, 1)
	selectAll()
	if got, want := selection(), "Another selectable text"; got != want {
		t.Errorf("select all after a press in the other text selects %q, want %q", got, want)
	}

	// The sheet has the keyboard, shown (focus-visible), and no ring.
	var doc struct{ Root struct{ NodeID int } }
	var q struct{ NodeID int }
	for _, step := range []struct {
		method string
		params map[string]any
		out    any
	}{
		{"DOM.enable", nil, &res},
		{"CSS.enable", nil, &res},
		{"DOM.getDocument", nil, &doc},
		{"DOM.querySelector", nil, &q},
		{"CSS.forcePseudoState", nil, &res},
	} {
		switch step.method {
		case "DOM.querySelector":
			step.params = map[string]any{"nodeId": doc.Root.NodeID, "selector": "dialog[open]"}
		case "CSS.forcePseudoState":
			step.params = map[string]any{"nodeId": q.NodeID, "forcedPseudoClasses": []string{"focus", "focus-visible"}}
		}
		if step.params == nil {
			step.params = map[string]any{}
		}
		if err := c.call(page, step.method, step.params, step.out, ""); err != nil {
			t.Fatal(step.method, err)
		}
	}
	ring := c.eval(t, page, `(() => { const d = document.querySelector("dialog[open]"); return d.matches(":focus-visible") + " " + getComputedStyle(d).outlineStyle; })()`)
	if ring != "true none" {
		t.Errorf("a sheet with the keyboard shown: focus-visible, outline is %q, want \"true none\"", ring)
	}
}
