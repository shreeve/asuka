//go:build darwin

package mactest

import (
	"slices"
	"testing"
)

// The common props' layers and the controls the visual part's tests
// check: a clipping view with a shadow, a shadow kept in the window, a
// transform held and animated, glass buttons, the Picker, the controls'
// labels.

// A view that clips and casts a shadow clips its children in a view of
// their own, so its layer keeps its shadow; without the shadow they go
// back, and the view clips itself again.
func TestVisualClipShadow(t *testing.T) {
	a := Run(t, `
		component Card {
		  state shadow = true
		  state more = false
		  Column {
		    padding: 20; gap: 20; align: .start
		    View {
		      width: 60; height: 60; clip: true; radius: 12
		      shadow: if shadow { Shadow(color: #000000, radius: 8) } else { none }
		      View { width: 100; height: 100; background: #ff0000 }
		      if more { View { width: 10; height: 10; background: #0000ff } }
		    }
		    Button("More") { on press { more = true } }
		    Button("Flat") { on press { shadow = false } }
		  }
		}
		app { Card() }
	`)
	tr := a.UntilText("Flat")
	card := viewSized(t, tr, 60, 60)
	cv := vobj(card, "clipView")
	if cv == nil {
		t.Fatalf("the children aren't in a clip view:\n%s", tr.Raw())
	}
	if !vbool(cv, "masks") || vnum(cv, "cornerRadius") != 12 || card.Bool("layerMasks") {
		t.Errorf("masks: the view's %v, the clip view's %v (radius %v)", card.Bool("layerMasks"), cv["masks"], cv["cornerRadius"])
	}
	if f := vnums(cv, "frame"); len(f) != 4 || f[2] != 60 || f[3] != 60 {
		t.Errorf("the clip view is %v", f)
	}
	if vnum(cv, "children") != 1 || !card.Bool("shadowPath") {
		t.Errorf("children in the clip view %v, shadow path %v", cv["children"], card.Props["shadowPath"])
	}
	// A child added later goes in the clip view too.
	a.Do("tap More")
	tr = a.Until(func(tr *Tree) bool { return vnum(vobj(viewSized(t, tr, 60, 60), "clipView"), "children") == 2 })
	a.Do("tap Flat")
	tr = a.Until(func(tr *Tree) bool { return vobj(viewSized(t, tr, 60, 60), "clipView") == nil })
	card = viewSized(t, tr, 60, 60)
	if card.Num("childrenInView") != 2 || !card.Bool("layerMasks") {
		t.Errorf("without the shadow: %v children back, masks %v", card.Num("childrenInView"), card.Bool("layerMasks"))
	}
}

// AppKit sets a layer-backed view's layer's shadow from the view's own
// (none) as it moves into a window: the style's shadow holds.
func TestVisualShadowInWindow(t *testing.T) {
	a := Run(t, `app { Column { padding: 20; align: .start; View { width: 60; height: 30; background: #ff0000; shadow: Shadow(color: #000000, radius: 8, y: 4) } } }`)
	tr := a.Until(func(tr *Tree) bool { return len(tr.Kind("View")) >= 2 })
	n := viewSized(t, tr, 60, 30)
	// Radius is a blur radius, 2σ: Core Animation's is σ.
	check := func(what string, n *Node) {
		t.Helper()
		off, _ := n.Props["shadowOffset"].([]any)
		if n.Num("shadowOpacity") != 1 || len(off) != 2 || off[1].(float64) != 4 || n.Num("layerShadowRadius") != 4 {
			t.Errorf("%s: shadow opacity %v, offset %v, radius %v", what, n.Props["shadowOpacity"], off, n.Props["layerShadowRadius"])
		}
	}
	check("in the window", n)
	a.Do("readd " + vid(n))
	check("moved in again", viewSized(t, a.Tree(), 60, 30))
}

// Style.Transform holds on the layer (AppKit sets the layer's own
// transform), and animate { } animates it.
func TestVisualTransformHolds(t *testing.T) {
	a := Run(t, `
		component T {
		  state scale = 0.5
		  Column {
		    padding: 20; gap: 20; align: .start
		    View { width: 60; height: 60; background: #ff0000; transform: Transform(scale: scale) }
		    Button("Grow") { on press { animate { scale = 1 } } }
		  }
		}
		app { T() }
	`)
	tr := a.Until(func(tr *Tree) bool { return len(viewSized(t, tr, 60, 60).Props["held"].([]any)) == 4 })
	n := viewSized(t, tr, 60, 60)
	held := vnums(n.Props, "held")
	if held[0] != 0.5 || held[2] != 15 {
		t.Fatalf("held %v", held)
	}
	a.Do("tap Grow")
	tr = a.Until(func(tr *Tree) bool { return vnums(viewSized(t, tr, 60, 60).Props, "held")[0] == 1 })
	if d := viewSized(t, tr, 60, 60).Num("transformAnimation"); d < 0.2 {
		t.Errorf("animated in %vs", d)
	}
}

// Glass buttons are macOS 26's glass bezel: a capsule, a circle around an
// icon alone, the prominent one tinted as the primary action. A button
// that stops being glass loses its shape and tint.
func TestVisualGlassButtons(t *testing.T) {
	a := Run(t, `
		component Buttons {
		  state glass = true
		  Column {
		    padding: 20; gap: 8; align: .start
		    Button("Add") { variant: .glassProminent }
		    Button("") { icon: "plus"; label: "Add one"; variant: .glass }
		    Button("Cancel") { variant: if glass { .glass } else { .gray } }
		    Button("Plain") { on press { glass = false } }
		  }
		}
		app { Buttons() }
	`)
	tr := a.UntilText("Plain")
	bs := tr.Kind("Button")
	if len(bs) != 4 {
		t.Fatalf("%d buttons", len(bs))
	}
	if bs[0].Props["borderShape"] == nil {
		t.Skip("before macOS 26: glass is the push and default buttons")
	}
	const bezelGlass, shapeAutomatic, shapeCapsule, shapeCircle, prominenceAuto, prominencePrimary = 16, 0, 1, 3, 0, 2
	check := func(what string, b *Node, bezel, shape, prominence float64) {
		t.Helper()
		got := []float64{b.Num("bezelStyle"), b.Num("borderShape"), b.Num("tintProminence")}
		if !slices.Equal(got, []float64{bezel, shape, prominence}) {
			t.Errorf("%s: bezel, shape, prominence %v, want %v", what, got, []float64{bezel, shape, prominence})
		}
	}
	check("prominent", bs[0], bezelGlass, shapeCapsule, prominencePrimary)
	check("an icon alone", bs[1], bezelGlass, shapeCircle, prominenceAuto)
	check("glass", bs[2], bezelGlass, shapeCapsule, prominenceAuto)
	if bs[1].Str("a11yLabel") != "Add one" {
		t.Errorf("the icon's label %q", bs[1].Str("a11yLabel"))
	}
	a.Do("tap Plain")
	tr = a.Until(func(tr *Tree) bool { return tr.Kind("Button")[2].Num("bezelStyle") != bezelGlass })
	check("no longer glass", tr.Kind("Button")[2], 1, shapeAutomatic, prominenceAuto)
}

// A Picker is a pop-up button: its prompt while none is chosen (a hidden
// item, not in its menu), then the chosen item; the user's choice writes

// A disabled filled button keeps its title white: AppKit only lightens its
// accent bezel, and its gray disabled text would all but vanish on it.
// Enabled, the title is AppKit's again; a title color of the app's own
// stays its.
func TestVisualFilledDisabled(t *testing.T) {
	a := Run(t, `
		component Buttons {
		  state off = true
		  Column {
		    padding: 20; gap: 8; align: .start
		    Button("Sign On") { variant: .filled; disabled: off }
		    Button("Red") { variant: .filled; disabled: true; color: .red }
		    Button("Toggle") { on press { off = !off } }
		  }
		}
		app { Buttons() }
	`)
	tr := a.UntilText("Toggle")
	sign, red := tr.Find("Button", "Sign On"), tr.Find("Button", "Red")
	if sign == nil || red == nil {
		t.Fatalf("buttons:\n%s", tr.Raw())
	}
	// Disabled, its cell draws its title white (titleAsGiven), as AppKit
	// would draw it gray.
	white := func(b *Node) bool { return b.Get("titleAsGiven") == true && b.Str("titleColor") == "#ffffffff" }
	if sign.Get("enabled") != false || !white(sign) {
		t.Errorf("disabled: enabled %v, title %q drawn as given %v", sign.Get("enabled"), sign.Str("titleColor"), sign.Get("titleAsGiven"))
	}
	if c := red.Str("titleColor"); c == "#ffffffff" || c == "" {
		t.Errorf("the app's own title color became %q", c)
	}
	a.Do("tap Toggle")
	tr = a.Until(func(tr *Tree) bool { b := tr.Find("Button", "Sign On"); return b != nil && b.Get("enabled") == true })
	if tr.Find("Button", "Sign On").Get("titleAsGiven") != nil {
		t.Error("enabled, its cell still draws its title")
	}
	a.Do("tap Toggle")
	a.Until(func(tr *Tree) bool {
		b := tr.Find("Button", "Sign On")
		return b != nil && b.Get("enabled") == false && white(b)
	})
}

// A Picker is a pop-up button: its prompt while none is chosen (a hidden
// item, not in its menu), then the chosen item; the user's choice writes
// the state, and code's selects the item.
func TestVisualPicker(t *testing.T) {
	a := Run(t, `
		component P {
		  state pick = -1
		  state items = ["Name", "Date", "Size"]
		  Column {
		    padding: 16; gap: 8; align: .start
		    Picker(items) { prompt: "Sort by"; bind selected: pick }
		    Text("picked {pick}")
		    Button("First") { on press { pick = 0 } }
		    Button("New items") { on press { items = ["Title", "Date"] } }
		    Button("None") { on press { pick = -1 } }
		  }
		}
		app { P() }
	`)
	tr := a.UntilText("picked -1")
	p := tr.Kind("Picker")[0]
	if p.Class != "NSPopUpButton" {
		t.Fatalf("a %s", p.Class)
	}
	if s := p.Str("titleOfSelected"); s != "Sort by" {
		t.Errorf("none chosen: %q", s)
	}
	if m := vstrs(p.Props, "menuShown"); !slices.Equal(m, []string{"Name", "Date", "Size"}) {
		t.Errorf("menu %q", m)
	}
	if s := p.Str("a11yLabel"); s != "Sort by" {
		t.Errorf("accessibility label %q", s)
	}
	if p.Frame[2] < 40 || p.Frame[3] < 16 {
		t.Errorf("frame %v", p.Frame)
	}
	title := func(tr *Tree) string { return tr.Kind("Picker")[0].Str("titleOfSelected") }
	// The user chooses Size, as the menu does: its item (after the hidden
	// prompt), then the action.
	a.Do("popupselect " + vid(p) + " 3")
	tr = a.UntilText("picked 2")
	if title(tr) != "Size" {
		t.Errorf("after choosing Size: %q", title(tr))
	}
	// Code's choice, and new items, keep the chosen one.
	a.Do("tap First")
	a.Until(func(tr *Tree) bool { return title(tr) == "Name" })
	a.Do("tap New items")
	a.Until(func(tr *Tree) bool { return title(tr) == "Title" })
	a.Do("tap None")
	a.Until(func(tr *Tree) bool { return title(tr) == "Sort by" })
}

// The controls' labels are what VoiceOver reads of them, followed as they
// change: a text field (one line, multiline, rich), a switch, a checkbox.
func TestVisualControlLabels(t *testing.T) {
	a := Run(t, `
		component L {
		  state name = "Name"
		  state text = "Ada"
		  state on = false
		  Column {
		    padding: 16; gap: 8; align: .start
		    TextField { label: name; bind text: text }
		    TextField { label: name; multiline: true; bind text: text }
		    TextField { label: name; rich: [Span(text: "Ada", italic: true)] }
		    Toggle { label: name; bind on: on }
		    Toggle { label: name; checkbox: true; bind on: on }
		    Button("Change") { on press { text = "Grace"; on = true } }
		    Button("Rename") { on press { name = "Full name" } }
		    Button("Clear") { on press { name = "" } }
		  }
		}
		app { L() }
	`)
	labels := func(tr *Tree) []string {
		var out []string
		for _, kind := range []string{"TextField", "Toggle"} {
			for _, n := range tr.Kind(kind) {
				l := n.Str("a11yLabel")
				if tv := n.Str("textViewA11yLabel"); tv != "" || n.Props["textViewA11yLabel"] != nil {
					l = tv
				}
				out = append(out, l)
			}
		}
		return out
	}
	want := func(s string) []string { return []string{s, s, s, s, s} }
	tr := a.UntilText("Rename")
	if got := labels(tr); !slices.Equal(got, want("Name")) {
		t.Errorf("labels %q", got)
	}
	a.Do("tap Change")
	if got := labels(a.Tree()); !slices.Equal(got, want("Name")) {
		t.Errorf("after the values change: %q", got)
	}
	a.Do("tap Rename")
	if !veventually(a, waitShort, func(tr *Tree) bool { return slices.Equal(labels(tr), want("Full name")) }) {
		t.Errorf("renamed: %q", labels(a.Tree()))
	}
	a.Do("tap Clear")
	if !veventually(a, waitShort, func(tr *Tree) bool { return slices.Equal(labels(tr), want("")) }) {
		t.Errorf("cleared: %q", labels(a.Tree()))
	}
}
