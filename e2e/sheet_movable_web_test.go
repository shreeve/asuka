//go:build e2e

package e2e

import (
	"encoding/json"
	"fmt"
	"strings"
	"testing"
)

// TestSheetMovableWeb runs e2e/testdata/sheetmovable in headless Chrome,
// on a wide page, with DevTools' trusted mouse events: a movable sheet is
// a modal dialog centered over the page with no bar, its content all of
// it, and a resize corner; dragging its background moves it, dragging the
// corner resizes it (its environment its size); a click on the page
// behind it neither reaches the page nor dismisses it; its own button
// does.
func TestSheetMovableWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	pageURL := webServer(t, "e2e/testdata/sheetmovable")
	c := launchCDP(t, browser)
	var created struct{ TargetID string }
	if err := c.call("", "Target.createTarget", map[string]any{"url": "about:blank"}, &created, ""); err != nil {
		t.Fatal(err)
	}
	page := c.attach(t, created.TargetID)
	var res json.RawMessage
	metrics := map[string]any{"width": 1000, "height": 700, "deviceScaleFactor": 1, "mobile": false}
	if err := c.call(page, "Emulation.setDeviceMetricsOverride", metrics, &res, ""); err != nil {
		t.Fatal(err)
	}
	if err := c.call(page, "Page.navigate", map[string]any{"url": pageURL}, &res, ""); err != nil {
		t.Fatal(err)
	}
	s := &webScreen{t: t, url: pageURL}
	showsLine(t, s, "status: ready")

	mouse := func(typ string, x, y float64) {
		t.Helper()
		p := map[string]any{"type": typ, "x": x, "y": y, "button": "left", "clickCount": 1}
		if typ == "mouseMoved" {
			p["buttons"] = 1
		}
		if err := c.call(page, "Input.dispatchMouseEvent", p, &res, ""); err != nil {
			t.Fatal(err)
		}
	}
	click := func(x, y float64) { t.Helper(); mouse("mouseMoved", x, y); mouse("mousePressed", x, y); mouse("mouseReleased", x, y) }
	drag := func(x, y, dx, dy float64) {
		t.Helper()
		mouse("mousePressed", x, y)
		for i := 1; i <= 4; i++ {
			mouse("mouseMoved", x+dx*float64(i)/4, y+dy*float64(i)/4)
		}
		mouse("mouseReleased", x+dx, y+dy)
	}
	// rect is an element's rectangle (a CSS selector) once nothing on the
	// page animates (the sheet scales in).
	rect := func(sel string) (x, y, w, h float64) {
		t.Helper()
		var r string
		until(t, func() bool {
			r = c.eval(t, page, `(() => { const e = document.querySelector(`+fmt.Sprintf("%q", sel)+`); if (!e || !e.getClientRects().length || document.getAnimations().length) return ""; const r = e.getBoundingClientRect(); return [r.left, r.top, r.width, r.height].map(Math.round).join(" "); })()`)
			return r != ""
		}, func() string { return "nothing shows " + sel })
		fmt.Sscan(r, &x, &y, &w, &h)
		return
	}
	center := func(text string) (float64, float64) {
		t.Helper()
		var r string
		until(t, func() bool {
			r = c.eval(t, page, `(() => { for (const e of document.querySelectorAll("button")) { if (e.textContent.trim() === `+fmt.Sprintf("%q", text)+` && e.getClientRects().length) { const r = e.getBoundingClientRect(); return (r.left + r.width / 2) + " " + (r.top + r.height / 2); } } return ""; })()`)
			return r != ""
		}, func() string { return "no button " + text })
		var x, y float64
		fmt.Sscan(r, &x, &y)
		return x, y
	}

	click(center("Show"))
	// The page behind it is inert while it shows: what's read is the sheet's.
	x, y, w, h := rect("dialog.asuka-movable")
	if d := (x + w/2) - 500; d > 1 || d < -1 {
		t.Fatalf("not centered across the page: %v %v %v %v", x, y, w, h)
	}
	if d := (y + h/2) - 350; d > 1 || d < -1 {
		t.Fatalf("not centered down the page: %v %v %v %v", x, y, w, h)
	}
	// No bar: its content is all of it, its environment its size.
	if r := c.eval(t, page, `String(document.querySelectorAll("dialog.asuka-movable .asuka-window-bar").length)`); !strings.Contains(r, "0") {
		t.Fatalf("a bar over its content: %s", r)
	}
	showsLine(t, s, fmt.Sprintf("sheet %dx%d", int(w), int(h)))

	// Dragged by its background, it moves.
	drag(x+w-12, y+12, -120, 40)
	until(t, func() bool { nx, ny, _, _ := rect("dialog.asuka-movable"); return nx == x-120 && ny == y+40 },
		func() string { nx, ny, nw, nh := rect("dialog.asuka-movable"); return fmt.Sprint(nx, ny, nw, nh) })
	x, y = x-120, y+40
	// Dragged by its corner, it resizes, and its content with it.
	drag(x+w-6, y+h-6, 60, -100)
	showsLine(t, s, fmt.Sprintf("sheet %dx%d", int(w)+60, int(h)-100))

	// A click on the page behind it reaches nothing and leaves it open
	// (the count, read once it's closed, says the first).
	cx, cy := center("Count")
	if cx >= x {
		// Where the page shows beside it: Count stretches across the page.
		cx = 20
	}
	click(cx, cy)
	if _, _, w2, _ := rect("dialog.asuka-movable"); w2 != w+60 {
		t.Fatalf("a click outside changed it: width %v", w2)
	}
	// Its own button dismisses it; the page takes clicks again.
	click(center("Done"))
	showsLine(t, s, "open false taps 0")
	click(cx, cy)
	showsLine(t, s, "open false taps 1")
	if r := c.eval(t, page, `String(!!document.querySelector("dialog.asuka-movable"))`); !strings.Contains(r, "false") {
		t.Fatalf("its dialog is still in the page: %s", r)
	}
}
