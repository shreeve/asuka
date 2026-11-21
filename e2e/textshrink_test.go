//go:build e2e

package e2e

import (
	"bytes"
	"fmt"
	"image"
	"image/png"
	"math"
	"testing"
	"time"
)

// A one-line text shrinks in a row that's too narrow for it, cut short
// with an ellipsis (e2e/testdata/textshrink): in a row 200 points wide
// beside a square 40 points wide, it's 160 points wide. It's measured on
// the screen, against a magenta square 48 points high.

// shrunkWidth is the red text's width in points, measured against the
// magenta square; false while either isn't on screen.
func shrunkWidth(img image.Image) (float64, bool) {
	b := img.Bounds()
	redL, redR, magTop, magBottom := -1, -1, -1, -1
	for y := b.Min.Y; y < b.Max.Y; y++ {
		for x := b.Min.X; x < b.Max.X; x++ {
			r, g, bl, _ := img.At(x, y).RGBA()
			r, g, bl = r>>8, g>>8, bl>>8
			switch {
			case r > 200 && g < 60 && bl < 60:
				if redL < 0 || x < redL {
					redL = x
				}
				if x > redR {
					redR = x
				}
			case r > 200 && g < 70 && bl > 200:
				if magTop < 0 {
					magTop = y
				}
				magBottom = y
			}
		}
	}
	if redL < 0 || magTop < 0 {
		return 0, false
	}
	k := float64(magBottom-magTop+1) / 48
	return float64(redR-redL+1) / k, true
}

// checkTextShrink captures the screen with shot until the text is on it
// (bounded) and checks it's 160 points wide, to a point.
func checkTextShrink(t *testing.T, shot func() ([]byte, error)) {
	t.Helper()
	deadline := time.Now().Add(within)
	var w float64
	var seen bool
	var last error
	for time.Now().Before(deadline) {
		b, err := shot()
		if err == nil {
			var img image.Image
			if img, err = png.Decode(bytes.NewReader(b)); err == nil {
				if w, seen = shrunkWidth(img); seen {
					break
				}
				err = fmt.Errorf("the text or the square isn't on screen")
			}
		}
		last = err
		time.Sleep(300 * time.Millisecond)
	}
	if !seen {
		t.Fatalf("the text never showed: %v", last)
	}
	t.Logf("the text is %.1f points wide", w)
	if math.Abs(w-160) > 1 {
		t.Errorf("the text is %.1f points wide, want 160: a one-line text doesn't shrink in its row", w)
	}
}

func TestTextShrinkWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	url := webServer(t, "e2e/testdata/textshrink")
	for _, scale := range []string{"1", "2"} {
		t.Run(scale+"x", func(t *testing.T) {
			page := devtoolsPage(t, browser, url, scale)
			checkTextShrink(t, func() ([]byte, error) { return page.screenshot() })
		})
	}
}
