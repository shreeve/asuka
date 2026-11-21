//go:build darwin

package mactest

import (
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

// A sheet as tall as its content whose content can't be as narrow as a
// sheet (a fixed width) is as wide as its content, centred on its window
// as a sheet is; its content shows whole. While it shows, its window can't
// be made too narrow for it, and a window that already is widens.
// A fit sheet wider than a sheet, laid out at its narrowest to widen it,
// with a line of text under its wide content: as tall as its content at
// the width it's widened to, the text on one line, not as tall as the
// text at its narrowest (a word on each line).
func TestNavFitSheetWideText(t *testing.T) {
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
		      detents: [.fit]
		      Column {
		        View { width: 820; height: 40 }
		        Text("one line of text under the wide view, which wraps a word on each line at its narrowest")
		      }
		    }
		  }
		}
	`)
	a.UntilText("open false")
	a.Do("resize 1400,900", "tap Show")
	var cw, ch float64
	tr := a.Until(func(tr *Tree) bool {
		s := sheetOf(tr)
		if s == nil {
			return false
		}
		w := sheetWindow(s)
		cw, ch = size(w["content"])
		return w["isSheet"] == true && w["visible"] == true && cw == 820
	})
	text := tr.Find("Text", "one line of text under the wide view, which wraps a word on each line at its narrowest")
	if text == nil {
		t.Fatal("the text isn't shown")
	}
	th := text.Frame[3]
	if th <= 0 || th > 30 {
		t.Errorf("the text is %v high: not one line", th)
	}
	if want := 40 + th; ch < want-1 || ch > want+1 {
		t.Errorf("the sheet is %vx%v, want %v high: its content's, at its width", cw, ch, want)
	}
}

func TestNavFitSheetWide(t *testing.T) {
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
		      detents: [.fit]
		      Column {
		        width: 820; height: 300
		        Row { Text("wide"); View { grow: 1 }; Button("Close") { on press { open = false } } }
		      }
		    }
		  }
		}
	`)
	a.UntilText("open false")
	before := winRect(a.Tree().Main(), "minSize")
	if len(before) != 2 || before[0] >= 900 {
		t.Fatalf("the window's own least size %v", before)
	}
	a.Do("resize 1400,900", "tap Show")
	var cw, ch float64
	tr := a.Until(func(tr *Tree) bool {
		s := sheetOf(tr)
		if s == nil {
			return false
		}
		w := sheetWindow(s)
		cw, ch = size(w["content"])
		return w["isSheet"] == true && w["visible"] == true && cw == 820 && ch == 300
	})
	close := tr.Find("Button", "Close")
	if close == nil || close.Get("shown") != true {
		t.Fatalf("Close isn't shown: %v", close)
	}
	if x := close.Frame[0] + close.Frame[2]; x > 820 {
		t.Errorf("Close ends at %v, past the sheet's 820", x)
	}
	f := sheetWindow(sheetOf(tr))["frame"].([]any)
	mf := tr.Main().Frame
	mid, sheetMid := mf[0]+mf[2]/2, f[0].(float64)+f[2].(float64)/2
	if d := mid - sheetMid; d > 1 || d < -1 {
		t.Errorf("the sheet's centre %v isn't its window's %v", sheetMid, mid)
	}
	// While it shows, its window can't be made narrower than the sheet
	// and the margin a sheet keeps (40 on each side): the window's least
	// width is 900; closed, the window's own least size is back.
	least := func(what string, want float64) {
		t.Helper()
		a.Until(func(tr *Tree) bool {
			m := winRect(tr.Main(), "minSize")
			return len(m) == 2 && m[0] == want
		})
	}
	least("shown", 900)
	a.Do("tap Close")
	a.UntilText("open false")
	least("closed", before[0])

	// Opened in a window too narrow for it, the window widens to the sheet
	// and its margin, and the sheet shows whole.
	a.Do("resize 700,900", "tap Show")
	a.Until(func(tr *Tree) bool {
		s := sheetOf(tr)
		if s == nil || sheetWindow(s)["visible"] != true {
			return false
		}
		cw, _ := size(sheetWindow(s)["content"])
		mw, _ := size(tr.Main().Props["content"])
		return cw == 820 && mw == 900
	})
	least("shown in a narrow window", 900)
	a.Do("tap Close")
	a.UntilText("open false")
	least("closed again", before[0])
}

// frameSize is a node's view's size, -1 by -1 if it has no view.
func frameSize(n *Node) (float64, float64) {
	if len(n.Frame) < 4 {
		return -1, -1
	}
	return n.Frame[2], n.Frame[3]
}
