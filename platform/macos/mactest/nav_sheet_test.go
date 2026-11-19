//go:build darwin

package mactest

import (
	"encoding/json"
	"strings"
	"testing"
	"time"
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

// A sheet as tall as its content whose content can't be as narrow as a
// sheet (a fixed width) is as wide as its content, centred on its window
// as a sheet is; its content shows whole. While it shows, its window can't
// be made too narrow for it, and a window that already is widens.
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

// A fit sheet whose content can't be as narrow as its room but is no wider
// than a sheet may be (600 here, in a window 650 wide): the window widens
// to the sheet and its margin, and stays held there when the sheet, its
// content changed, is laid out again at that width, where it fills its
// room exactly.
func TestNavFitSheetHeldAtItsWidth(t *testing.T) {
	a := Run(t, `
		app { Root() }
		component Root {
		  state open = false
		  state more = false
		  Column {
		    padding: 20
		    Text("open {open}")
		    Button("Show") { on press { open = true } }
		    Sheet {
		      bind open: open
		      detents: [.fit]
		      Column {
		        width: 600
		        Button("More") { on press { more = true } }
		        if more { Text("more shown") }
		        Button("Close") { on press { open = false } }
		      }
		    }
		  }
		}
	`)
	a.UntilText("open false")
	before := winRect(a.Tree().Main(), "minSize")
	if len(before) != 2 || before[0] >= 600 {
		t.Fatalf("the window's own least size %v", before)
	}
	a.Do("resize 650,900", "tap Show")
	a.Until(func(tr *Tree) bool {
		s := sheetOf(tr)
		if s == nil || sheetWindow(s)["visible"] != true {
			return false
		}
		cw, _ := size(sheetWindow(s)["content"])
		mw, _ := size(tr.Main().Props["content"])
		return cw == 600 && mw == 680
	})
	// The content grows, so the sheet is laid out again at a room of 600,
	// as wide as it is: the hold stays
	a.Do("tap More")
	a.UntilText("more shown")
	time.Sleep(time.Second)
	if m := winRect(a.Tree().Main(), "minSize"); len(m) != 2 || m[0] != 680 {
		t.Errorf("shown: the window's least size is %v, want 680 wide", m)
	}
	a.Do("tap Close")
	a.UntilText("open false")
	if m := winRect(a.Tree().Main(), "minSize"); len(m) != 2 || m[0] != before[0] {
		t.Errorf("closed: the window's least size is %v, want its own %v", m, before)
	}
}

// A sheet as tall as its content that can be as narrow as a sheet leaves
// its window's least size the window's own while it shows: the window can
// still be made narrower, and the sheet narrows with it.
func TestNavFitSheetNarrowWindow(t *testing.T) {
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
		        padding: 20
		        View { height: 100; background: .blue }
		        Button("Close") { on press { open = false } }
		      }
		    }
		  }
		}
	`)
	a.UntilText("open false")
	before := winRect(a.Tree().Main(), "minSize")
	if len(before) != 2 || before[0] >= 600 {
		t.Fatalf("the window's own least size %v", before)
	}
	// shows waits for the sheet at want wide, its room's width, in a
	// window winW wide, then checks the window's least size is its own.
	shows := func(what string, want, winW float64) {
		t.Helper()
		var tr *Tree
		a.Until(func(x *Tree) bool {
			tr = x
			s := sheetOf(x)
			if s == nil || sheetWindow(s)["visible"] != true {
				return false
			}
			roomW, _ := pair(s.Get("sheetRoom"))
			cw, _ := size(sheetWindow(s)["content"])
			mw, _ := size(x.Main().Props["content"])
			return cw == want && roomW == want && mw == winW
		})
		if m := winRect(tr.Main(), "minSize"); len(m) != 2 || m[0] != before[0] {
			t.Errorf("%s: the window's least size is %v, want its own %v", what, m, before)
		}
	}
	a.Do("resize 1400,900", "tap Show")
	shows("shown", 640, 1400)
	a.Do("resize 600,900")
	shows("narrowed", 520, 600)
	a.Do("tap Close")
	a.UntilText("open false")
	if m := winRect(a.Tree().Main(), "minSize"); len(m) != 2 || m[0] != before[0] {
		t.Errorf("closed: the window's least size is %v, want its own %v", m, before)
	}
}

// A wide fit sheet on a Window widens that window and holds its width,
// not the main window's; the Window closed with the sheet up, the main
// window still has its own least size.
func TestNavFitSheetWideOnWindow(t *testing.T) {
	a := Run(t, `
		app { Root() }
		component Root {
		  state open = false
		  state sheet = false
		  Column {
		    padding: 20
		    Text("open {open} sheet {sheet}")
		    Button("Open") { on press { open = true } }
		    Button("Close") { on press { open = false } }
		    Window {
		      bind open: open
		      title: "Inspector"
		      width: 400
		      height: 400
		      Column {
		        Button("Show") { on press { sheet = true } }
		        Sheet {
		          bind open: sheet
		          detents: [.fit]
		          View { width: 820; height: 100 }
		        }
		      }
		    }
		  }
		}
	`)
	a.UntilText("open false sheet false")
	before := winRect(a.Tree().Main(), "minSize")
	a.Do("resize 1400,900", "tap Open")
	a.Until(func(tr *Tree) bool { w := tr.Window("Inspector"); return w != nil && w.Visible })
	a.Do("tap Show")
	a.Until(func(tr *Tree) bool {
		w := tr.Window("Inspector")
		s := sheetOf(tr)
		if w == nil || s == nil || sheetWindow(s)["visible"] != true {
			return false
		}
		cw, _ := size(sheetWindow(s)["content"])
		m := winRect(w, "minSize")
		return cw == 820 && len(m) == 2 && m[0] == 900 && w.Content[2] == 900
	})
	if m := winRect(a.Tree().Main(), "minSize"); len(m) != 2 || m[0] != before[0] || m[1] != before[1] {
		t.Errorf("the sheet on the Window shown, the main window's least size is %v, want %v", m, before)
	}
	a.Do("tap Close")
	a.UntilText("open false sheet true")
	a.Until(func(tr *Tree) bool { return tr.Window("Inspector") == nil })
	if m := winRect(a.Tree().Main(), "minSize"); len(m) != 2 || m[0] != before[0] || m[1] != before[1] {
		t.Errorf("the Window closed, the main window's least size is %v, want its own %v", m, before)
	}
}

// frameSize is a node's view's size, -1 by -1 if it has no view.
func frameSize(n *Node) (float64, float64) {
	if len(n.Frame) < 4 {
		return -1, -1
	}
	return n.Frame[2], n.Frame[3]
}

// A screen with zoomFrom grows out of the view on the screen below that
// shares its key, and goes back into it: the view hides while the screen
// stands for it, and shows again once it's done, pushed or popped; the
// pushed screen shows, and the one below hides as after a slide.
func TestNavZoomFrom(t *testing.T) {
	a := Run(t, `
		app { Stack { Home() } }
		screen Home {
		  title: "Tiles"
		  Column {
		    padding: 16
		    gap: 12
		    for i in 1..<4 key i {
		      View {
		        label: "Tile {i}"
		        height: 80
		        radius: 12
		        background: .blue
		        shared: "tile-{i}"
		        on press { push(Tile(i)) }
		        Text("Tile {i}")
		      }
		    }
		  }
		}
		screen Tile(n: int) {
		  title: "Tile {n}"
		  zoomFrom: "tile-{n}"
		  Text("Detail {n}")
		}
	`)
	a.UntilText("Tile 2")
	shown := func(tr *Tree, text string) bool {
		n := tr.FindText(text)
		return n != nil && n.Get("hidden") != true
	}
	a.Do("tap Tile 2")
	a.Until(func(tr *Tree) bool { return shown(tr, "Detail 2") && !shown(tr, "Tile 1") })
	a.Do("back")
	tr := a.Until(func(tr *Tree) bool { return tr.FindText("Detail 2") == nil && shown(tr, "Tile 1") })
	if !shown(tr, "Tile 2") {
		t.Fatalf("the tile the screen zoomed back into stayed hidden:\n%s", tr.Raw())
	}
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
