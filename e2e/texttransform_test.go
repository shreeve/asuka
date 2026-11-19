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

// A text in a view drawn smaller by a transform is laid out at its own
// size, which layout doesn't see the transform change
// (e2e/testdata/texttransform): drawn at half its size, it's half as
// wide and half as high as the same text drawn as it is, on one line.

// boundsOf is the bounds of the pixels in img that match, in pixels;
// false while there are none.
func boundsOf(img image.Image, match func(r, g, b uint32) bool) (image.Rectangle, bool) {
	b := img.Bounds()
	out := image.Rectangle{Min: b.Max, Max: b.Min}
	found := false
	for y := b.Min.Y; y < b.Max.Y; y++ {
		for x := b.Min.X; x < b.Max.X; x++ {
			r, g, bl, _ := img.At(x, y).RGBA()
			if match(r>>8, g>>8, bl>>8) {
				found = true
				out = out.Union(image.Rect(x, y, x+1, y+1))
			}
		}
	}
	return out, found
}

// transformedSize is the red text's size and the blue one's, as drawn,
// in pixels; false while either isn't on screen.
func transformedSize(img image.Image) (image.Point, image.Point, bool) {
	red, ok1 := boundsOf(img, func(r, g, b uint32) bool { return r > 200 && g < 60 && b < 60 })
	blue, ok2 := boundsOf(img, func(r, g, b uint32) bool { return r < 60 && g < 60 && b > 200 })
	if !ok1 || !ok2 {
		return image.Point{}, image.Point{}, false
	}
	return red.Size(), blue.Size(), true
}

// checkTextTransform captures the screen with shot until the texts are on
// it (bounded) and checks the red one is drawn at half the blue one's
// size, to a pixel and a half.
func checkTextTransform(t *testing.T, shot func() ([]byte, error)) {
	t.Helper()
	deadline := time.Now().Add(within)
	var red, blue image.Point
	var seen bool
	var last error
	for time.Now().Before(deadline) {
		b, err := shot()
		if err == nil {
			var img image.Image
			if img, err = png.Decode(bytes.NewReader(b)); err == nil {
				if red, blue, seen = transformedSize(img); seen {
					break
				}
				err = fmt.Errorf("the texts aren't on screen")
			}
		}
		last = err
		time.Sleep(300 * time.Millisecond)
	}
	if !seen {
		t.Fatalf("the texts never showed: %v", last)
	}
	t.Logf("drawn at half its size the text is %v pixels, as it is %v", red, blue)
	if math.Abs(float64(red.X)-float64(blue.X)/2) > 1.5 || math.Abs(float64(red.Y)-float64(blue.Y)/2) > 1.5 {
		t.Errorf("drawn at half its size the text is %v pixels, as it is %v: it was measured as drawn, transform and all", red, blue)
	}
}

func TestTextTransformWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	url := webServer(t, "e2e/testdata/texttransform")
	for _, scale := range []string{"1", "2"} {
		t.Run(scale+"x", func(t *testing.T) {
			page := devtoolsPage(t, browser, url, scale)
			checkTextTransform(t, func() ([]byte, error) { return page.screenshot() })
		})
	}
}
