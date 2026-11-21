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

// A sheet with the .fit detent is as tall as its content
// (e2e/testdata/fitsheet): its content ends with a red band, and right
// under the band is what's behind the sheet, not more of the sheet.

// fitSheetEnd finds the red band in img and says whether what's just
// under its middle is the sheet's white (the sheet goes on past its
// content); false while the band isn't on screen.
func fitSheetEnd(img image.Image) (white bool, below image.Point, ok bool) {
	b := img.Bounds()
	minX, maxX, bottom := -1, -1, -1
	for y := b.Min.Y; y < b.Max.Y; y++ {
		for x := b.Min.X; x < b.Max.X; x++ {
			r, g, bl, _ := img.At(x, y).RGBA()
			if r>>8 > 200 && g>>8 < 60 && bl>>8 < 60 {
				if minX < 0 || x < minX {
					minX = x
				}
				if x > maxX {
					maxX = x
				}
				bottom = y
			}
		}
	}
	if bottom < 0 {
		return false, image.Point{}, false
	}
	// A few pixels under the band, past any rounding of the sheet's corner.
	p := image.Pt((minX+maxX)/2, bottom+6)
	if p.Y >= b.Max.Y {
		return false, p, true
	}
	r, g, bl, _ := img.At(p.X, p.Y).RGBA()
	return r>>8 > 240 && g>>8 > 240 && bl>>8 > 240, p, true
}

// checkFitSheet captures the screen with shot until the sheet shows
// (bounded) and checks it ends where its content does.
func checkFitSheet(t *testing.T, shot func() ([]byte, error)) {
	t.Helper()
	deadline := time.Now().Add(within)
	var white, seen bool
	var at image.Point
	var last error
	for time.Now().Before(deadline) {
		b, err := shot()
		if err == nil {
			var img image.Image
			if img, err = png.Decode(bytes.NewReader(b)); err == nil {
				if white, at, seen = fitSheetEnd(img); seen {
					// past its opening animation
					time.Sleep(500 * time.Millisecond)
					if b, err = shot(); err == nil {
						if img, err = png.Decode(bytes.NewReader(b)); err == nil {
							white, at, seen = fitSheetEnd(img)
						}
					}
					if seen {
						break
					}
				}
				err = fmt.Errorf("the sheet's content isn't on screen")
			}
		}
		last = err
		time.Sleep(300 * time.Millisecond)
	}
	if !seen {
		t.Fatalf("the sheet never showed: %v", last)
	}
	if white {
		t.Errorf("under the sheet's content, at %v, the sheet goes on: a .fit sheet isn't as tall as its content", at)
	}
}

func TestFitSheetWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	url := webServer(t, "e2e/testdata/fitsheet")
	for _, scale := range []string{"1", "2"} {
		t.Run(scale+"x", func(t *testing.T) {
			page := devtoolsPage(t, browser, url, scale)
			checkFitSheet(t, func() ([]byte, error) { return page.screenshot() })
		})
	}
}
