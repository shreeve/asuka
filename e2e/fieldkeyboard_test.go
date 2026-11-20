//go:build e2e

package e2e

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"testing"
	"time"
)

// A one-line field low in a scrolling page, tapped, is scrolled above
// the keyboard that opens for it, as a multiline one keeps its caret in
// sight.
func TestFieldAboveKeyboardAndroid(t *testing.T) {
	s := androidApp(t, "e2e/testdata/field-keyboard", "dev.asuka.fieldkeyboard")
	waitFor(t, s, []string{"ready"})
	left, top, right, bottom := s.fieldBounds()
	if _, err := s.adb("shell", "input", "tap", strconv.Itoa((left+right)/2), strconv.Itoa((top+bottom)/2)); err != nil {
		t.Fatal(err)
	}
	var state string
	until(t, func() bool {
		kb := s.keyboardTop()
		_, top, _, bottom := s.fieldBounds()
		state = fmt.Sprintf("field %d to %d, keyboard from %d", top, bottom, kb)
		return kb > 0 && bottom <= kb && top >= 0
	}, func() string { return state + ", want the field above the keyboard" })
}

// On iOS the automation focuses the field (the keyboard UIKit shows for
// it) and says where it and the keyboard are.
func TestFieldAboveKeyboardIOS(t *testing.T) {
	s := iosApp(t, "e2e/testdata/field-keyboard", "dev.asuka.fieldkeyboard")
	waitFor(t, s, []string{"ready"})
	s.command("type 1")
	var line string
	until(t, func() bool {
		file := filepath.Join(s.tmp, "asuka-focused.txt")
		os.Remove(file)
		s.command("focused")
		line = ""
		for range 50 {
			if b, err := os.ReadFile(file); err == nil {
				line = strings.TrimSpace(string(b))
				break
			}
			time.Sleep(100 * time.Millisecond)
		}
		var top, bottom, kb, window int
		if _, err := fmt.Sscanf(line, "field %d %d keyboard %d window %d", &top, &bottom, &kb, &window); err != nil {
			return false
		}
		// With a hardware keyboard connected, UIKit shows none: nothing to test.
		return kb < window && bottom <= kb && top >= 0
	}, func() string { return line + ", want a keyboard and the field above it" })
}

// Focus moving from one field to another, each with its own bar on the
// keyboard (an accessory), leaves the keyboard's height as it is. UIKit
// swaps the bars, and the keyboard it reports shrinks by the old one's,
// then grows by the new one's, a turn or two later: followed, what sits on
// the keyboard drops and comes back up.
func TestFieldAccessoriesIOS(t *testing.T) {
	s := iosApp(t, "e2e/testdata/field-accessories", "dev.asuka.fieldaccessories")
	waitFor(t, s, []string{"ready", "keyboard down"})
	s.tap("Focus first")
	// With a hardware keyboard connected, UIKit shows none: nothing to test.
	deadline := time.Now().Add(within)
	for !hasAll(s.texts(), []string{"keyboard up", "Done first"}) {
		if time.Now().After(deadline) {
			t.Skip("no keyboard shown (a hardware keyboard is connected)")
		}
		time.Sleep(200 * time.Millisecond)
	}
	// The keyboard's coming up over (a move during it has nothing to drop):
	// like the pause below, it can let the bug by, never fail the fix.
	time.Sleep(time.Second)
	// The second field's bar on the keyboard in place of the first's, then
	// the few frames in which UIKit reports the old bar gone and the new one
	// come: nothing is waited for that has to come, so a pass doesn't race.
	s.tap("Focus second")
	waitFor(t, s, []string{"Done second"}, "Done first")
	time.Sleep(300 * time.Millisecond)
	var seen string
	for _, line := range s.texts() {
		if strings.HasPrefix(line, "seen: ") {
			seen = strings.TrimPrefix(line, "seen: ")
		}
	}
	// It may still grow, as the first field's keyboard finishes coming up
	// (UIKit reports the keys, then the keys with the bar); it never drops.
	heights := strings.Fields(seen)
	if len(heights) == 0 {
		t.Fatalf("no keyboard height seen: %q", s.texts())
	}
	first := mustAtoi(t, heights[0])
	for _, h := range heights[1:] {
		if mustAtoi(t, h) < first {
			t.Fatalf("the keyboard's height went %s as focus moved, want it never to drop", strings.Join(heights, " → "))
		}
	}
}

// mustAtoi is s as a number; the test fails if it isn't one.
func mustAtoi(t *testing.T, s string) int {
	t.Helper()
	v, err := strconv.Atoi(s)
	if err != nil {
		t.Fatal(err)
	}
	return v
}

var boundsRE = regexp.MustCompile(`class="android\.widget\.EditText"[^>]*bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"`)

// fieldBounds is the first text field's frame on screen, in pixels, from UiAutomator.
func (s *androidScreen) fieldBounds() (l, t, r, b int) {
	s.adb("shell", "uiautomator", "dump", "/sdcard/asuka-ui.xml")
	out, _ := s.adb("exec-out", "cat", "/sdcard/asuka-ui.xml")
	m := boundsRE.FindStringSubmatch(out)
	if m == nil {
		s.t.Fatalf("no text field in:\n%s", out)
	}
	n := func(i int) int { v, _ := strconv.Atoi(m[i]); return v }
	return n(1), n(2), n(3), n(4)
}

var imeRE = regexp.MustCompile(`touchable region=SkRegion\(\(\d+,(\d+),\d+,\d+\)\)`)

// keyboardTop is where the keyboard's window starts on screen, in pixels (0: none shown).
func (s *androidScreen) keyboardTop() int {
	if out, _ := s.adb("shell", "dumpsys", "input_method"); !regexp.MustCompile(`mInputShown=true`).MatchString(out) {
		return 0
	}
	out, _ := s.adb("shell", "dumpsys", "window", "InputMethod")
	m := imeRE.FindStringSubmatch(out)
	if m == nil {
		return 0
	}
	v, _ := strconv.Atoi(m[1])
	return v
}
