//go:build darwin

package mactest

import (
	"encoding/json"
	"strings"
	"testing"
)

const styleResizable = 1 << 3

// sheetOf is the tree's one Sheet node with its window, nil if none.
func sheetOf(tr *Tree) *Node {
	for _, n := range tr.Kind("Sheet") {
		if _, ok := n.Get("sheetWindow").(map[string]any); ok {
			return n
		}
	}
	return nil
}

func sheetWindow(n *Node) map[string]any { w, _ := n.Get("sheetWindow").(map[string]any); return w }

func pair(v any) (float64, float64) {
	a, _ := v.([]any)
	if len(a) < 2 {
		return -1, -1
	}
	x, _ := a[0].(float64)
	y, _ := a[1].(float64)
	return x, y
}

func size(v any) (float64, float64) {
	a, _ := v.([]any)
	if len(a) < 4 {
		return -1, -1
	}
	w, _ := a[2].(float64)
	h, _ := a[3].(float64)
	return w, h
}

// A full-screen sheet is a sheet of its window as big as all of that
// window's content under its title bar and toolbar, which the user
// doesn't resize; it follows the window as it resizes, its environment
// with it; Escape dismisses it as it does a sheet (TestFullScreenSheetDarwin).
func TestNavFullScreenSheet(t *testing.T) {
	a := Run(t, `
		app { Root() }
		component Root {
		  state open = false
		  Column {
		    padding: 20
		    Text("open {open}")
		    Button("Show") { on press { open = true } }
		    Sheet {
		      bind open: open
		      fullScreen: true
		      Column {
		        padding: 20
		        Text("sheet {environment().width}x{environment().height}")
		        TextField { placeholder: "Name" }
		      }
		    }
		  }
		}
	`)
	a.UntilText("open false")
	a.Do("tap Show")
	covers := func(what string) *Tree {
		t.Helper()
		return a.Until(func(tr *Tree) bool {
			s := sheetOf(tr)
			if s == nil {
				return false
			}
			w := sheetWindow(s)
			cw, ch := pair(s.Get("cover"))
			sw, sh := size(w["content"])
			ew, eh := pair(s.Get("env"))
			fw, fh := frameSize(s)
			return w["isSheet"] == true && w["visible"] == true && cw > 0 && sw == cw && sh == ch && ew == cw && eh == ch && fw == cw && fh == ch
		})
	}
	tr := covers("opened")
	s := sheetOf(tr)
	if m := sheetWindow(s)["styleMask"].(float64); int(m)&styleResizable != 0 {
		t.Fatal("a full-screen sheet the user resizes")
	}
	if p := sheetWindow(s)["parent"]; p != tr.Main().Title {
		t.Fatalf("a sheet of %q, not of the main window %q", p, tr.Main().Title)
	}
	// The window resizes: the sheet follows.
	a.Do("resize 920,700")
	covers("the window grown")
	a.Do("resize 700,520")
	covers("the window shrunk")
	// Escape in its content dismisses it.
	a.Do("cancel")
	a.UntilText("open false")
	a.Until(func(tr *Tree) bool { return sheetOf(tr) == nil })
}

// A sheet as tall as its content (DetentFit) opens at its content's
// height, at a sheet's width, follows its content as it grows and shrinks,
// and goes no taller than a sheet may, beyond which its content scrolls;
// the user doesn't resize it (TestFitSheetDarwin).
func TestNavFitSheet(t *testing.T) {
	a := Run(t, `
		app { Root() }
		component Root {
		  state open = false
		  state more = false
		  state lots = false
		  Column {
		    padding: 20
		    Text("open {open}")
		    Button("Show") { on press { open = true } }
		    Button("Hide") { on press { open = false } }
		    Button("More") { on press { more = !more } }
		    Button("Lots") { on press { lots = !lots } }
		    Sheet {
		      bind open: open
		      detents: [.fit]
		      Column {
		        padding: 20
		        gap: 10
		        View { height: 100; background: .blue }
		        if more { View { height: 50; background: .green } }
		        if lots { Scroll { shrink: 1; View { height: 5000 } } }
		        Button("Less") { on press { more = false; lots = false } }
		      }
		    }
		  }
		}
	`)
	a.UntilText("open false")
	a.Do("tap Show")
	var roomW, roomH float64
	is := func(what string, height float64) {
		t.Helper()
		a.Until(func(tr *Tree) bool {
			s := sheetOf(tr)
			if s == nil {
				return false
			}
			w := sheetWindow(s)
			roomW, roomH = pair(s.Get("sheetRoom"))
			want := height
			if want < 0 {
				want = roomH
			}
			cw, ch := size(w["content"])
			fw, fh := frameSize(s)
			return w["isSheet"] == true && w["visible"] == true && cw == roomW && ch == want && fw == cw && fh == ch
		})
	}
	// Its content: 20 + 100 + 10 + a button + 20.
	tr := a.Until(func(tr *Tree) bool { return sheetOf(tr) != nil && tr.Find("Button", "Less") != nil })
	bh := tr.Find("Button", "Less").Frame[3]
	base := 20 + 100 + 10 + bh + 20
	is("opened", base)
	if m := sheetWindow(sheetOf(a.Tree()))["styleMask"].(float64); int(m)&styleResizable != 0 {
		t.Fatal("a sheet as tall as its content the user resizes")
	}
	a.Do("tap More")
	is("grown", base+60)
	a.Do("tap More")
	is("shrunk", base)
	a.Do("tap Lots")
	is("too tall for a sheet", -1)
	// Opened again, it shows, at its content's size from the first.
	a.Do("tap Less", "tap Hide")
	a.UntilText("open false")
	a.Do("tap Show")
	is("opened again", base)
}

// frameSize is a node's view's size, -1 by -1 if it has no view.
func frameSize(n *Node) (float64, float64) {
	if len(n.Frame) < 4 {
		return -1, -1
	}
	return n.Frame[2], n.Frame[3]
}

// A movable sheet is a window of its own, not a sheet, nor a child window
// (which the window server doesn't drag smoothly): centered over its
// window and in front of it, the key window, faded in where it opens (no
// zoom or bounce of AppKit's own), which the user can move and resize. Its window takes no presses
// meanwhile. A fit one opens at its content's size, the least the user
// can size it to; after that its size is its window's, which its content
// is laid out at, whatever the content does. Escape and its close button
// dismiss it.
func TestNavMovableSheet(t *testing.T) {
	a := Run(t, `
		app { Root() }
		component Root {
		  state open = false
		  state taps = 0
		  state more = false
		  Column {
		    padding: 20
		    Text("open {open} taps {taps}")
		    Button("Show") { on press { open = true } }
		    Button("Count") { on press { taps += 1 } }
		    Sheet {
		      bind open: open
		      detents: [.fit]
		      movable: true
		      Column {
		        padding: 20
		        View { height: 100; background: .blue }
		        if more { View { height: 50; background: .green } }
		        Button("More") { on press { more = !more } }
		        Text("sheet {environment().width}x{environment().height}")
		      }
		    }
		  }
		}
	`)
	a.UntilText("open false taps 0")
	a.Do("tap Show")
	shown := func(what string, ok func(w map[string]any, s *Node) bool) *Tree {
		t.Helper()
		return a.Until(func(tr *Tree) bool {
			s := sheetOf(tr)
			if s == nil {
				return false
			}
			w := sheetWindow(s)
			return w["visible"] == true && w["isSheet"] == false && ok(w, s)
		})
	}
	// The key window, while the app is active (macOS doesn't activate a
	// test's player while the user works in another app).
	keyed := func(w map[string]any) bool { return w["key"] == true || !appActive(a.Tree()) }
	tr := shown("opened", func(w map[string]any, s *Node) bool {
		return w["over"] != "" && keyed(w) && w["alpha"] == 1.0 && w["animation"] == 2.0 && w["blocks"] == true
	})
	main := tr.Main()
	s := sheetOf(tr)
	w := sheetWindow(s)
	if p := w["over"]; p != tr.Main().Title {
		t.Fatalf("over %q, not the main window %q", p, tr.Main().Title)
	}
	if p := w["childOf"]; p != "" {
		t.Fatalf("a child window of %q: the window server doesn't drag it smoothly", p)
	}
	if m := int(w["styleMask"].(float64)); m&styleResizable == 0 || m&styleClosable == 0 {
		t.Fatalf("style mask %#x: a movable sheet the user can't resize or close", m)
	}
	// Centered over the main window.
	mf := main.Frame
	if len(mf) < 4 {
		t.Fatalf("the main window has no frame: %v", mf)
	}
	sx, sy := pair(w["frame"])
	sw, sh := size(w["frame"])
	if d := (mf[0] + mf[2]/2) - (sx + sw/2); d > 1 || d < -1 {
		t.Fatalf("not centered across: main %v, sheet %v", mf, w["frame"])
	}
	if d := (mf[1] + mf[3]/2) - (sy + sh/2); d > 1 || d < -1 {
		t.Fatalf("not centered down: main %v, sheet %v", mf, w["frame"])
	}
	// At its content's height, and following it.
	bh := tr.Find("Button", "More").Frame[3]
	th := -1.0
	for _, n := range tr.All() {
		if n.Kind == "Text" && strings.HasPrefix(n.Text, "sheet ") {
			th = n.Frame[3]
		}
	}
	base := 20 + 100 + bh + th + 20
	fits := func(what string, h float64) {
		t.Helper()
		shown(what, func(w map[string]any, s *Node) bool { _, ch := size(w["content"]); return ch == h })
	}
	fits("opened at its content's height", base)
	if min, ok := sheetWindow(sheetOf(a.Tree()))["minSize"].([]any); !ok || len(min) < 2 || min[1].(float64) != base {
		t.Fatalf("its least size %v, not its content's height %v", sheetWindow(sheetOf(a.Tree()))["minSize"], base)
	}
	// Its content grows: the window keeps its size, its environment that size.
	a.Do("tap More")
	shown("its size kept", func(w map[string]any, s *Node) bool {
		cw, ch := size(w["content"])
		ew, eh := pair(s.Get("env"))
		return ch == base && ew == cw && eh == ch
	})
	// A click on its window goes nowhere.
	a.Do("click Count")
	// The user resizes it: its environment is that size, as its content
	// changes too.
	a.Do("sheetresize 500,420")
	shown("resized", func(w map[string]any, s *Node) bool {
		cw, ch := size(w["content"])
		ew, eh := pair(s.Get("env"))
		return cw == 500 && ch == 420 && ew == 500 && eh == 420
	})
	a.Do("tap More")
	a.UntilText("sheet 500x420")
	shown("the user's size kept", func(w map[string]any, s *Node) bool { cw, ch := size(w["content"]); return cw == 500 && ch == 420 })
	// Its close button dismisses it; the click before it never counted.
	a.Do("sheetclose")
	a.UntilText("open false taps 0")
	a.Until(func(tr *Tree) bool { return sheetOf(tr) == nil })
	// Its window takes clicks again.
	a.Do("click Count")
	a.UntilText("open false taps 1")
	// Opened again, Escape dismisses it.
	a.Do("tap Show")
	shown("opened again", func(w map[string]any, s *Node) bool { return keyed(w) })
	a.Do("cancel")
	a.UntilText("open false taps 1")
}

// appActive is whether the player is the active app.
func appActive(tr *Tree) bool {
	var d struct {
		App struct {
			Active bool `json:"active"`
		} `json:"app"`
	}
	json.Unmarshal([]byte(tr.Raw()), &d)
	return d.App.Active
}
