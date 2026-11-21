//go:build e2e

package e2e

import (
	"bytes"
	"encoding/json"
	"fmt"
	"image"
	"image/png"
	"strings"
	"testing"
	"time"
)

// TestListRowsWeb runs e2e/testdata/list-rows in headless Chrome at 2x:
// rows 31.5 points high, each a blue band over a black line a point
// high, are drawn where they're laid out, every other one half a point
// into a point too: down the list, each line is 2 device pixels of black,
// not 1 under the next row's band, and nothing but blue and black shows.
func TestListRowsWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	page := devtoolsPage(t, browser, webServer(t, "e2e/testdata/list-rows"), "1")
	// 2x as DevTools' device emulation sets it, as a browser zoomed in or
	// on a 2x screen lays the page out.
	var res json.RawMessage
	metrics := map[string]any{"width": 430, "height": 932, "deviceScaleFactor": 2, "mobile": false}
	if err := page.call("Emulation.setDeviceMetricsOverride", metrics, &res); err != nil {
		t.Fatal(err)
	}
	var runs []string
	var last error
	for deadline := time.Now().Add(30 * time.Second); time.Now().Before(deadline); time.Sleep(300 * time.Millisecond) {
		b, err := page.screenshot()
		if err != nil {
			last = err
			continue
		}
		img, err := png.Decode(bytes.NewReader(b))
		if err != nil {
			last = err
			continue
		}
		if runs = rowRuns(img); len(runs) >= 16 {
			break
		}
		last = fmt.Errorf("the rows aren't on screen: %v", runs)
	}
	if len(runs) < 16 {
		t.Fatalf("no rows: %v", last)
	}
	// The first 8 rows, from the first band's top: band, line, band,
	// line… Each line is whole, 2 pixels, not cut to 1 by the next row's
	// band; a band, 61 pixels, can take a pixel more or less as the
	// browser rounds a row half a point into a point.
	for i, r := range runs[:16] {
		var c string
		var n int
		fmt.Sscan(r, &c, &n)
		ok := c == "blue" && n >= 60 && n <= 62
		if i%2 == 1 {
			ok = r == "black 2"
		}
		if !ok {
			t.Fatalf("down the list: %s; run %d is %q: a row isn't drawn where it's laid out", strings.Join(runs[:16], ", "), i, r)
		}
	}
}

// rowRuns is the colours down the middle of img from its first blue
// pixel, as runs of a colour and its length: "blue 61", "black 2", or
// another colour's RGB.
func rowRuns(img image.Image) []string {
	b := img.Bounds()
	x := b.Min.X + b.Dx()/2
	name := func(y int) string {
		r, g, bl, _ := img.At(x, y).RGBA()
		r, g, bl = r>>8, g>>8, bl>>8
		switch {
		case r == 0 && g == 0 && bl == 255:
			return "blue"
		case r == 0 && g == 0 && bl == 0:
			return "black"
		}
		return fmt.Sprintf("rgb(%d,%d,%d)", r, g, bl)
	}
	y := b.Min.Y
	for y < b.Max.Y && name(y) != "blue" {
		y++
	}
	var runs []string
	for y < b.Max.Y {
		c, n := name(y), 0
		for y < b.Max.Y && name(y) == c {
			y, n = y+1, n+1
		}
		runs = append(runs, fmt.Sprintf("%s %d", c, n))
	}
	if len(runs) > 0 {
		runs = runs[:len(runs)-1] // the last, cut off by the screen's edge
	}
	return runs
}
