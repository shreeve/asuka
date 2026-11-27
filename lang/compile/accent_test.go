package compile

import (
	"github.com/egoist/asuka/lang/diag"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"testing"
)

// An app's asuka.toml accent goes into its init (ui.set_accent), as the
// wire's Color, so it travels with the module; an app with none sets 0,
// and a package with no app block sets nothing.
func TestAccent(t *testing.T) {
	app := func(t *testing.T, toml, src string) *Result {
		dir := t.TempDir()
		if toml != "" {
			os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(toml), 0o644)
		}
		os.WriteFile(filepath.Join(dir, "app.asuka"), []byte(src), 0o644)
		res, err := Dir(dir, nil)
		if err != nil {
			t.Fatal(err)
		}
		return res
	}
	const appSrc = "app { Text(\"Hi\") }\n"
	m := app(t, "name = \"a\"\naccent = \"#7c3aed\"\n[android]\ndynamicColor = true\n", appSrc).Module
	if m == nil || !slices.Contains(m.Builtins, "ui.set_accent") || !slices.Contains(m.Ints, int64(2<<32|0x7c3aedff)) {
		t.Fatalf("no accent in %+v", m)
	}
	if m := app(t, "", appSrc).Module; m == nil || !slices.Contains(m.Builtins, "ui.set_accent") {
		t.Fatal("an app with no accent doesn't set the platform's")
	}
	if r := app(t, "", "pub fn f() -> int { return 1 }\n"); r.Module == nil || slices.Contains(r.Module.Builtins, "ui.set_accent") {
		t.Log(r.Err())
		t.Fatal("a package with no app sets an accent")
	}
	res := app(t, "accent = \"purple\"\n", appSrc)
	if err := res.Err(); err == nil || !strings.Contains(err.Error(), "asuka.toml") || !strings.Contains(err.Error(), "purple") {
		t.Fatalf("a bad accent: %v", err)
	}
	out, err := runBoth(t, app(t, "accent = \"#ff000080\"\n", appSrc+"test \"runs\" { let a = mount(Text(\"x\")); a.expect(\"x\") }\n").Module)
	if err != nil || strings.Contains(out, "FAIL") {
		t.Fatalf("%v\n%s", err, out)
	}
}

// asuka.toml's accentDark goes into the init beside the accent, as the
// wire's Color the core sends for dark mode; one in the wrong form is the
// project's error, named by its key.
func TestAccentDark(t *testing.T) {
	app := func(t *testing.T, toml, src string) *Result {
		dir := t.TempDir()
		os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(toml), 0o644)
		os.WriteFile(filepath.Join(dir, "app.asuka"), []byte(src), 0o644)
		res, err := Dir(dir, nil)
		if err != nil {
			t.Fatal(err)
		}
		return res
	}
	const appSrc = "app { Text(\"Hi\") }\n"
	m := app(t, "accent = \"#2563eb\"\naccentDark = \"#93c5fd\"\n", appSrc).Module
	if m == nil {
		t.Fatal("no module")
	}
	if !slices.Contains(m.Ints, int64(2<<32|0x2563ebff)) || !slices.Contains(m.Ints, int64(2<<32|0x93c5fdff)) {
		t.Fatalf("no dark accent in the init's ints %v", m.Ints)
	}
	res := app(t, "accent = \"#2563eb\"\naccentDark = \"navy\"\n", appSrc)
	if err := res.Err(); err == nil || !strings.Contains(err.Error(), `accentDark "navy"`) {
		t.Fatalf("a bad dark accent: %v", err)
	}
	out, err := runBoth(t, app(t, "accent = \"#2563eb\"\naccentDark = \"#93c5fd\"\n", appSrc+"test \"runs\" { let a = mount(Text(\"x\")); a.expect(\"x\") }\n").Module)
	if err != nil || strings.Contains(out, "FAIL") {
		t.Fatalf("%v\n%s", err, out)
	}
}

// asuka.toml's [web.colors] in the wrong form is the project's error
// (invalid-project), naming the key; in the right form it compiles.
func TestWebColorsChecked(t *testing.T) {
	app := func(t *testing.T, toml string) *Result {
		dir := t.TempDir()
		os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(toml), 0o644)
		os.WriteFile(filepath.Join(dir, "app.asuka"), []byte("app { Text(\"Hi\") }\n"), 0o644)
		res, err := Dir(dir, nil)
		if err != nil {
			t.Fatal(err)
		}
		return res
	}
	res := app(t, "[web.colors]\nlabel = \"#16181d\"\nseparatorDark = \"#3a3d4\"\n")
	if len(res.Diags) != 1 || res.Diags[0].Code != diag.InvalidProject.Name || !strings.Contains(res.Diags[0].Msg, `web.colors.separatorDark "#3a3d4"`) {
		t.Fatalf("a bad web color: %+v", res.Diags)
	}
	if err := app(t, "[web.colors]\nlabel = \"#16181d\"\nlabelDark = \"#e8e9eb80\"\n").Err(); err != nil {
		t.Fatal(err)
	}
}
