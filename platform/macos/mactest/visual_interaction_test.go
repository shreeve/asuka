//go:build darwin

package mactest

import (
	"fmt"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"testing"
)

// A click presses the innermost pressable view: Pressed, Press, then
// Pressed off; moving off and back on, it's released and pressed again;
// released off the view, it doesn't press; a pan that takes the mouse
// cancels it; VoiceOver's press presses.
func TestVisualPressed(t *testing.T) {
	a := Run(t, `
		component Pressed {
		  state log = ""
		  Column {
		    padding: 20; gap: 20; align: .start
		    View {
		      width: 200; height: 100; padding: 10; gap: 10; align: .start
		      on press { log = log + "press;" }
		      on pressedChange(p) { log = log + "outer {p};" }
		      View { width: 50; height: 30; on pressedChange(p) { log = log + "inner {p};" } }
		      Button("B") { on press { log = log + "button;" } }
		    }
		    View {
		      width: 201; height: 100
		      on press { log = log + "pan press;" }
		      on pressedChange(p) { log = log + "pannable {p};" }
		      on pan(g) { log = log + "pan {g.state};" }
		    }
		    Text("log:{log}")
		  }
		}
		app { Pressed() }
	`)
	tr := a.UntilText("log:")
	outer, inner, pannable := viewSized(t, tr, 200, 100), viewSized(t, tr, 50, 30), viewSized(t, tr, 201, 100)
	l := &vlogger{a: a}
	o := vid(outer)

	a.Do("mouse down " + o + " 150 80, up " + o + " 150 80")
	l.expect(t, "a click", "outer true", "press", "outer false")

	a.Do("mouse down " + o + " 150 80, dragged " + o + " 150 90, dragged " + o + " 150 130, dragged " + o + " 150 95, up " + o + " 150 95")
	l.expect(t, "off and back on", "outer true", "outer false", "outer true", "press", "outer false")

	a.Do("mouse down " + o + " 150 80, dragged " + o + " 250 80, up " + o + " 250 80")
	l.expect(t, "released off", "outer true", "outer false")

	// The inner pressable view takes its clicks: its ancestor isn't pressed.
	a.Do("mouse down " + vid(inner) + " 10 10, up " + vid(inner) + " 10 10")
	l.expect(t, "the inner view", "inner true", "inner false")

	// A pan takes the mouse: no Press.
	p := vid(pannable)
	steps := []string{fmt.Sprintf("down %s 20 50 0", p)}
	for i := 1; i <= 8; i++ {
		steps = append(steps, fmt.Sprintf("dragged %s %d 50 %v", p, 20+i*4, float64(i)*0.01))
	}
	steps = append(steps, fmt.Sprintf("up %s 52 50 0.1", p))
	a.Do("mouse " + strings.Join(steps, ", "))
	got := l.next()
	if len(got) < 3 || got[0] != "pannable true" || got[1] != "pannable false" || got[2] != "pan began" || slices.Contains(got, "pan press") {
		t.Errorf("a pan: %q", got)
	}

	// The accessibility press still presses.
	a.Do("a11ypress " + o)
	l.expect(t, "VoiceOver", "press")
	if outer.Str("a11yRole") != "AXButton" || !outer.Bool("a11yElement") {
		t.Errorf("a pressable view's role %q, element %v", outer.Str("a11yRole"), outer.Bool("a11yElement"))
	}
}

// Hover: a tracking area's entered and exited; nested views both stay
// hovered; leaving the app, nothing is; a view that appears under the
// pointer is hovered, and hidden it isn't.
func TestVisualHover(t *testing.T) {
	a := Run(t, `
		component Hover {
		  state log = ""
		  state shown = false
		  Column {
		    padding: 20; gap: 20; align: .start
		    View {
		      width: 200; height: 100; padding: 10
		      on hoverChange(h) { log = log + "outer {h};" }
		      View { width: 50; height: 30; on hoverChange(h) { log = log + "inner {h};" } }
		    }
		    if shown {
		      View { width: 60; height: 60; on hoverChange(h) { log = log + "new {h};" } }
		    }
		    Button("Show") { on press { shown = true } }
		    Text("log:{log}")
		  }
		}
		app { Hover() }
	`)
	tr := a.UntilText("log:")
	outer, inner := viewSized(t, tr, 200, 100), viewSized(t, tr, 50, 30)
	ix := vobj(outer, "interaction")
	if vnum(ix, "trackingAreas") != 1 || vnum(ix, "hoverOptions") != 0x641 {
		t.Fatalf("tracking areas %v, options %#x", ix["trackingAreas"], int(vnum(ix, "hoverOptions")))
	}
	l := &vlogger{a: a}
	a.Do("hover enter "+vid(outer), "hover enter "+vid(inner), "hover exit "+vid(inner), "hover enter "+vid(inner))
	l.expect(t, "entered and exited", "outer true", "inner true", "inner false", "inner true")

	a.Do("appactive 0")
	got := l.next()
	slices.Sort(got)
	if !slices.Equal(got, []string{"inner false", "outer false"}) {
		t.Errorf("leaving the app: %q", got)
	}

	// A view that appears under a still pointer: the new view's center,
	// below the outer view.
	a.Do("hoverprobe 50 170", "tap Show")
	l.until(t, func(s string) bool { return strings.HasSuffix(s, "new true;") })
	tr = a.Tree()
	n := viewSized(t, tr, 60, 60)
	a.Do("hide " + vid(n) + " 1")
	l.expect(t, "hidden", "new false")
	a.Do("hoverprobe off")
}

// Hover in a List: of the rows that appear under a still pointer over the
// first, only the first is hovered. The checks wait until the list has
// placed the rows it bound, and a row's visibleRect is clipped to it.
func TestVisualHoverListRows(t *testing.T) {
	a := Run(t, `
		component Rows {
		  state log = ""
		  state shown = false
		  Row {
		    padding: 20; gap: 20; align: .start
		    View {
		      width: 300; height: 200
		      if shown {
		        List {
		          appearance: .bare
		          grow: 1
		          for i in [0, 1, 2, 3] key i {
		            View { height: 30; on hoverChange(h) { log = log + "row{i} {h};" } }
		          }
		        }
		      }
		    }
		    Column {
		      gap: 10
		      Button("Show") { on press { shown = true } }
		      Text("log:{log}")
		    }
		  }
		}
		app { Rows() }
	`)
	a.UntilText("log:")
	l := &vlogger{a: a}
	a.Do("hoverprobe 100 35", "tap Show")
	l.until(t, func(s string) bool { return strings.Contains(s, "row0 true;") })
	var hovered int
	for _, n := range a.Tree().Kind("View") {
		if len(n.Frame) == 4 && vnear(n.Frame[2], 300, 0.5) && vnear(n.Frame[3], 30, 0.5) && vbool(vobj(n, "interaction"), "hovered") {
			hovered++
		}
	}
	if log := vlogOf(a.Tree()); hovered != 1 || log != "row0 true;" {
		t.Errorf("rows appearing under the pointer over the first: %d hovered, log %q", hovered, log)
	}
	a.Do("hoverprobe off")
}

// Drops: AppKit gives a drag to the innermost registered view, which
// hands it to the innermost target above it that takes it; DragOver
// follows it; a text, a file, image data and the app's own drag drop as
// the core expects.
func TestVisualDrop(t *testing.T) {
	a := Run(t, `
		component Drops {
		  state log = ""
		  Column {
		    padding: 20; gap: 20; align: .start
		    View {
		      width: 200; height: 100; padding: 10
		      dropTypes: ["text/plain", "application/x-card"]
		      on dropOver(o) { log = log + "outer over {o};" }
		      on drop(d) { log = log + "outer drop {describe(d)};" }
		      View {
		        width: 50; height: 30
		        dropTypes: ["image/*"]
		        on dropOver(o) { log = log + "inner over {o};" }
		        on drop(d) { log = log + "inner drop {describe(d)};" }
		      }
		    }
		    View {
		      width: 201; height: 50
		      dropTypes: ["*/*"]
		      on dropOver(o) { log = log + "files over {o};" }
		      on drop(d) { log = log + "files drop {describe(d)};" }
		    }
		    View {
		      width: 80; height: 40; background: .accent
		      dragItems: [DragItem(mime: "application/x-card", text: "Card", value: "card-1")]
		      on dragPhase(p) { log = log + "drag {p};" }
		    }
		    Text("log:{log}")
		  }
		}
		fn describe(d: Drop) -> string {
		  let items = d.items.map(it => "{it.kind}|{it.text}|{it.path}|{it.name}|{it.mime}|{it.size}|{it.value}")
		  return "local={d.local} copied={d.copied} at={d.x},{d.y} items={items.joined("+")}"
		}
		app { Drops() }
	`)
	tr := a.UntilText("log:")
	outer, inner, files, source := viewSized(t, tr, 200, 100), viewSized(t, tr, 50, 30), viewSized(t, tr, 201, 50), viewSized(t, tr, 80, 40)
	if n := vnum(vobj(inner, "interaction"), "registeredTypes"); n < 4 {
		t.Fatalf("the inner target registered %v types", n)
	}
	l := &vlogger{a: a}
	drop := func(entries []string, prefix string) string {
		for _, e := range entries {
			if strings.HasPrefix(e, prefix) {
				return strings.TrimPrefix(e, prefix)
			}
		}
		return ""
	}
	results := func() []string { return vstrs(visualState(a.Tree()), "dragResults") }
	seen := 0
	newResults := func() []string {
		r := results()
		d := r[seen:]
		seen = len(r)
		return d
	}

	// A text over the inner image target: the outer takes it. Out of the
	// inner view onto the outer one, the same target: still over.
	a.Do("pasteboard text text hello",
		"dragging text 1001 entered "+vid(inner)+" 10 10",
		"dragging text 1001 updated "+vid(inner)+" 12 10",
		"dragging text 1001 exited "+vid(inner)+" 140 70",
		"dragging text 1001 entered "+vid(outer)+" 150 80",
		"dragging text 1001 prepare "+vid(outer)+" 150 80",
		"dragging text 1001 perform "+vid(outer)+" 150 80",
		"dragging text 1001 ended "+vid(outer)+" 150 80")
	if r := newResults(); !slices.Equal(r, []string{"entered=1", "updated=1", "exited=", "entered=1", "prepare=true", "perform=true", "ended="}) {
		t.Errorf("a text: AppKit's answers %q", r)
	}
	got := l.next()
	if len(got) != 3 || got[0] != "outer over true" || !slices.Contains(got, "outer over false") {
		t.Errorf("a text: %q", got)
	}
	if d := drop(got, "outer drop "); d != "local=false copied=false at=150,80 items=text|hello|||text/plain|-1|" {
		t.Errorf("a text: %q", d)
	}

	// A file from Finder: the file itself, its type and size. Neither the
	// inner nor the outer target takes a PDF.
	path := filepath.Join(t.TempDir(), "notes.pdf")
	os.WriteFile(path, []byte("some notes"), 0o644)
	a.Do("pasteboard file file "+path,
		"dragging file 1002 entered "+vid(inner)+" 10 10",
		"dragging file 1002 exited "+vid(inner)+" 10 10",
		"dragging file 1002 entered "+vid(files)+" 10 10",
		"dragging file 1002 perform "+vid(files)+" 10 10")
	if r := newResults(); len(r) != 4 || r[0] != "entered=0" {
		t.Errorf("a file: AppKit's answers %q", r)
	}
	got = l.next()
	if len(got) != 3 || got[0] != "files over true" || !slices.Contains(got, "files over false") {
		t.Errorf("a file: %q", got)
	}
	d := drop(got, "files drop ")
	// The path as the pasteboard has it (/var is /private/var).
	if !strings.HasPrefix(d, "local=false copied=false at=10,10 items=file||") || !strings.HasSuffix(d, "notes.pdf|notes.pdf|application/pdf|10|") {
		t.Errorf("a file: %q", d)
	}

	// Image data (no file): written to the caches, the app's copy.
	a.Do("pasteboard png png",
		"dragging png 1004 entered "+vid(inner)+" 5 5",
		"dragging png 1004 perform "+vid(inner)+" 5 5")
	newResults()
	var pngSize string
	for _, r := range vstrs(visualState(a.Tree()), "moduleResults") {
		if strings.HasPrefix(r, "png ") {
			pngSize = strings.TrimPrefix(r, "png ")
		}
	}
	got = l.next()
	d = drop(got, "inner drop ")
	parts := strings.Split(strings.TrimPrefix(d, "local=false copied=true at=5,5 items="), "|")
	if len(got) != 3 || got[0] != "inner over true" || len(parts) != 7 || parts[0] != "image" || parts[4] != "image/png" || parts[5] != pngSize {
		t.Fatalf("image data: %q (a png of %s bytes)", got, pngSize)
	}
	if b, err := os.ReadFile(parts[2]); err != nil || fmt.Sprint(len(b)) != pngSize {
		t.Errorf("the image's copy: %v", err)
	}
	os.RemoveAll(filepath.Dir(parts[2]))

	// The app's own drag: a move, dropped at once with its values. The
	// drag begins with the mouse (its session the automation's).
	s := vid(source)
	a.Do("fakedrag on", "mouse down "+s+" 10 10, dragged "+s+" 20 10", "dragbegan")
	l.expect(t, "the drag begins", "drag began")
	a.Do("pasteboard local local "+s+" Card",
		"dragging local 1003 entered "+vid(files)+" 20 20",
		"dragging local 1003 exited "+vid(files)+" 20 20",
		"dragging local 1003 entered "+vid(outer)+" 100 50",
		"dragging local 1003 perform "+vid(outer)+" 100 50",
		"enddrag 16")
	if r := newResults(); len(r) < 1 || r[0] != "entered=16" {
		t.Errorf("a drag within the app: AppKit's answers %q", r)
	}
	got = l.next()
	if len(got) < 5 || got[0] != "files over true" || got[1] != "files over false" || got[2] != "outer over true" {
		t.Errorf("within the app: %q", got)
	}
	if d := drop(got, "outer drop "); !strings.HasPrefix(d, "local=true ") || !strings.Contains(d, "|card-1") {
		t.Errorf("within the app: %q", d)
	}
	if !slices.Contains(got, "drag dropped") {
		t.Errorf("the drag's end: %q", got)
	}
}

// A move with the button down past the drag distance drags a draggable
// view: its press is cancelled, its items go on the pasteboard, and the
// session reports Began, then Dropped or Cancelled.
func TestVisualDragSource(t *testing.T) {
	a := Run(t, `
		component Source {
		  state log = ""
		  Column {
		    padding: 20; align: .start
		    View {
		      width: 80; height: 40; background: .accent
		      on press { log = log + "press;" }
		      on pressedChange(p) { log = log + "pressed {p};" }
		      dragItems: [DragItem(text: "hello"), DragItem(kind: .link, text: "https://example.com"), DragItem(value: "1", mime: "application/x-n")]
		      on dragPhase(p) { log = log + "drag {p};" }
		    }
		    Text("log:{log}")
		  }
		}
		app { Source() }
	`)
	tr := a.UntilText("log:")
	v := viewSized(t, tr, 80, 40)
	l := &vlogger{a: a}
	s := vid(v)
	a.Do("fakedrag on", "mouse down "+s+" 10 10, dragged "+s+" 11 10, dragged "+s+" 15 10")
	l.expect(t, "the drag", "pressed true", "pressed false")
	st := visualState(a.Tree())
	want := []string{"public.utf8-plain-text,dev.asuka.drag", "public.url,public.utf8-plain-text,dev.asuka.drag", "dev.asuka.drag"}
	if got := vstrs(st, "dragLog"); !slices.Equal(got, want) {
		t.Fatalf("the pasteboard: %q, want %q", got, want)
	}
	if dr, _ := st["drag"].(map[string]any); dr == nil || vnum(dr, "id") != float64(v.ID) {
		t.Fatalf("no drag under way: %v", st["drag"])
	}
	a.Do("dragmask outside", "dragmask inside", "dragmask valuesoutside")
	if r := vstrs(visualState(a.Tree()), "dragResults"); !slices.Equal(r, []string{"mask=1", "mask=21", "mask=0"}) {
		t.Errorf("masks: %q (outside, inside, values outside)", r)
	}
	a.Do("dragbegan", "enddrag 1", "enddrag 0")
	l.expect(t, "the session", "drag began", "drag dropped")
	// The mouse's up after the drag presses nothing.
	a.Do("mouse up " + s + " 15 10")
	l.expect(t, "after the drag")
}

// A quick drag with the mouse is a swipe, on a view that takes swipes and
// no pans; a slow one isn't.
func TestVisualSwipeWithMouse(t *testing.T) {
	a := Run(t, `
		component Swipes {
		  state log = ""
		  Column {
		    padding: 20; align: .start
		    View { width: 300; height: 100; on swipe(d) { log = log + "swipe {d};" } }
		    Text("log:{log}")
		  }
		}
		app { Swipes() }
	`)
	tr := a.UntilText("log:")
	v := vid(viewSized(t, tr, 300, 100))
	l := &vlogger{a: a}
	// 160 points in 80 ms.
	steps := []string{"down " + v + " 250 50 0"}
	for i := 1; i <= 8; i++ {
		steps = append(steps, fmt.Sprintf("dragged %s %d 50 %v", v, 250-i*20, float64(i)*0.01))
	}
	steps = append(steps, "up "+v+" 90 50 0.09")
	a.Do("mouse " + strings.Join(steps, ", "))
	l.expect(t, "a quick drag left", "swipe left")
	// Down, but too short to be a swipe.
	steps = []string{"down " + v + " 50 20 0"}
	for i := 1; i <= 4; i++ {
		steps = append(steps, fmt.Sprintf("dragged %s 50 %d %v", v, 20+i*4, float64(i)*0.01))
	}
	steps = append(steps, "up "+v+" 50 36 0.05")
	a.Do("mouse " + strings.Join(steps, ", "))
	l.expect(t, "a short drag")
}

// A long press: a secondary click is a Mac's (its recognizer's), and the
// view's recognizers don't delay its mouse's events; pinch and rotate
// views get AppKit's recognizers.
func TestVisualGestureRecognizers(t *testing.T) {
	a := Run(t, `
		component G {
		  state log = ""
		  Column {
		    padding: 20; gap: 10; align: .start
		    View { width: 100; height: 50; on longPress { log = log + "long;" } }
		    View { width: 101; height: 50; on pinch(g) { log = log + "pinch;" }; on rotate(g) { log = log + "rotate;" } }
		    Text("log:{log}")
		  }
		}
		app { G() }
	`)
	tr := a.UntilText("log:")
	long, two := viewSized(t, tr, 100, 50), viewSized(t, tr, 101, 50)
	rs := vstrs(vobj(long, "interaction"), "recognizers")
	if !slices.Contains(rs, "NSPressGestureRecognizer") || !slices.Contains(rs, "NSClickGestureRecognizer") {
		t.Errorf("a long press's recognizers %q", rs)
	}
	rs = vstrs(vobj(two, "interaction"), "recognizers")
	if !slices.Contains(rs, "NSMagnificationGestureRecognizer") || !slices.Contains(rs, "NSRotationGestureRecognizer") {
		t.Errorf("pinch and rotate's recognizers %q", rs)
	}
	l := &vlogger{a: a}
	a.Do("mouse rightdown " + vid(long) + " 10 10, rightup " + vid(long) + " 10 10")
	l.until(t, func(s string) bool { return s == "long;" })
}

// wheelFlick is a trackpad's two-finger flick over view id: began, five
// moves of dx, dy points 10 ms apart, then the fingers lift.
func wheelFlick(id string, dx, dy int) string {
	steps := []string{fmt.Sprintf("%s began 0 0 0", id)}
	for i := 1; i <= 5; i++ {
		steps = append(steps, fmt.Sprintf("%s changed %d %d %v", id, dx, dy, float64(i)*0.01))
	}
	steps = append(steps, fmt.Sprintf("%s ended 0 0 0.06", id))
	return "wheel " + strings.Join(steps, ", ")
}

// A trackpad's two-finger flick is a swipe: across always, up and down
// only where no scroll view around the view scrolls that way (there it's
// the scroll view's); fingers that stop before they lift aren't one.
func TestVisualSwipeWithTrackpad(t *testing.T) {
	a := Run(t, `
		component Swipes {
		  state log = ""
		  Column {
		    padding: 20; gap: 10; align: .start
		    View { width: 300; height: 100; on swipe(d) { log = log + "plain {d};" } }
		    Scroll {
		      horizontal: true; width: 200; height: 80
		      // Shorter than the scroll view (a legacy scroller takes 15 points
		      // of its height): it scrolls across only.
		      View { width: 600; height: 40; on swipe(d) { log = log + "scrolled {d};" } }
		    }
		    Text("log:{log}")
		  }
		}
		app { Swipes() }
	`)
	tr := a.UntilText("log:")
	plain, inner := vid(viewSized(t, tr, 300, 100)), vid(viewSized(t, tr, 600, 40))
	l := &vlogger{a: a}
	// Without natural scrolling, the content moves against the fingers: a
	// positive x delta is the fingers going left; a positive y, up.
	a.Do(wheelFlick(plain, 20, 0))
	l.expect(t, "a flick left", "plain left")
	a.Do(wheelFlick(plain, 0, -20))
	l.expect(t, "a flick down", "plain down")
	a.Do(wheelFlick(inner, 20, 0))
	l.expect(t, "across, in a horizontal scroll view")
	a.Do(wheelFlick(inner, 0, 20))
	l.expect(t, "up, in a horizontal scroll view", "scrolled up")
	// The fingers stop, then lift.
	a.Do("wheel " + plain + " began 0 0 0, " + plain + " changed 20 0 0.01, " + plain + " changed 20 0 0.02, " + plain + " ended 0 0 0.3")
	l.expect(t, "stopped before lifting")
}
