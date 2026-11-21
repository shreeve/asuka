//go:build e2e

package e2e

import (
	"bytes"
	"fmt"
	"image"
	"image/png"
	"testing"
	"time"
)

// A picker's background is the app's (e2e/testdata/pickerstyle): a red
// picker 200 by 40 points is red across nearly all of it, its text and
// chevron aside, not the platform's own field over it. It's measured on
// the screen, against a magenta square 48 points high.

// redShare is how much of the 200 by 40 point picker is red, from the
// red pixels' count and the magenta square's scale; false while either
// isn't on screen.
func redShare(img image.Image) (float64, bool) {
	b := img.Bounds()
	red, magTop, magBottom := 0, -1, -1
	for y := b.Min.Y; y < b.Max.Y; y++ {
		for x := b.Min.X; x < b.Max.X; x++ {
			r, g, bl, _ := img.At(x, y).RGBA()
			r, g, bl = r>>8, g>>8, bl>>8
			switch {
			case r > 200 && g < 60 && bl < 60:
				red++
			case r > 200 && g < 70 && bl > 200:
				if magTop < 0 {
					magTop = y
				}
				magBottom = y
			}
		}
	}
	if magTop < 0 || red == 0 {
		return 0, false
	}
	k := float64(magBottom-magTop+1) / 48
	return float64(red) / (200 * 40 * k * k), true
}

// checkPickerStyle captures the screen with shot until the picker is on
// it (bounded) and checks it's red across at least 80% of it.
func checkPickerStyle(t *testing.T, shot func() ([]byte, error)) {
	t.Helper()
	deadline := time.Now().Add(within)
	var share float64
	var seen bool
	var last error
	for time.Now().Before(deadline) {
		b, err := shot()
		if err == nil {
			var img image.Image
			if img, err = png.Decode(bytes.NewReader(b)); err == nil {
				if share, seen = redShare(img); seen {
					break
				}
				err = fmt.Errorf("the picker or the square isn't on screen")
			}
		}
		last = err
		time.Sleep(300 * time.Millisecond)
	}
	if !seen {
		t.Fatalf("the picker never showed: %v", last)
	}
	t.Logf("the picker is %.0f%% red", share*100)
	if share < 0.8 {
		t.Errorf("the picker is %.0f%% red, want 80%% or more: the platform's field covers the app's background", share*100)
	}
}

func TestPickerStyleWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	url := webServer(t, "e2e/testdata/pickerstyle")
	for _, scale := range []string{"1", "2"} {
		t.Run(scale+"x", func(t *testing.T) {
			page := devtoolsPage(t, browser, url, scale)
			checkPickerStyle(t, func() ([]byte, error) { return page.screenshot() })
		})
	}
}
