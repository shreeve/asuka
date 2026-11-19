package main

import (
	"bytes"
	"errors"
	"io/fs"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestFmt(t *testing.T) {
	dir := t.TempDir()
	messy := "component C {gap:8\nText( \"a\" )}\n"
	clean := "component C {\n  gap: 8\n  Text(\"a\")\n}\n"
	bad := "component D {\n  Text(\"a\"\n}\n"
	files := map[string]string{
		"a.asuka":              messy,
		"ok.asuka":             clean,
		"sub/b.asuka":          messy,
		"sub/bad.asuka":        bad,
		".hidden/c.asuka":      messy,
		"sub/notes.txt":        messy,
		"node_modules/d.asuka": messy,
	}
	for name, src := range files {
		path := filepath.Join(dir, name)
		os.MkdirAll(filepath.Dir(path), 0o755)
		if err := os.WriteFile(path, []byte(src), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	read := func(name string) string {
		b, _ := os.ReadFile(filepath.Join(dir, name))
		return string(b)
	}

	// -l lists what would change and reports the syntax errors.
	var out, errOut bytes.Buffer
	err := runFmt([]string{dir}, false, true, false, nil, &out, &errOut)
	if !errors.Is(err, errFmt) {
		t.Errorf("-l: %v, want errFmt", err)
	}
	want := filepath.Join(dir, "a.asuka") + "\n" + filepath.Join(dir, "sub/b.asuka") + "\n"
	if out.String() != want {
		t.Errorf("-l listed %q, want %q", out.String(), want)
	}
	if e := filepath.Join(dir, "sub/bad.asuka") + ":3:1: expected ')' to close the argument list opened at 2:7"; !strings.HasPrefix(errOut.String(), e) {
		t.Errorf("diagnostics %q, want %q", errOut.String(), e)
	}

	// Printing a file.
	out.Reset()
	if err := runFmt([]string{filepath.Join(dir, "a.asuka")}, false, false, false, nil, &out, &errOut); err != nil {
		t.Fatal(err)
	}
	if out.String() != clean || read("a.asuka") != messy {
		t.Errorf("printed %q", out.String())
	}

	// -w writes the files that change, and only those.
	out.Reset()
	runFmt([]string{dir}, true, false, false, nil, &out, &errOut)
	if read("a.asuka") != clean || read("sub/b.asuka") != clean || read("sub/bad.asuka") != bad ||
		read(".hidden/c.asuka") != messy || read("node_modules/d.asuka") != messy || read("sub/notes.txt") != messy {
		t.Errorf("-w wrote the wrong files")
	}
	if out.Len() != 0 {
		t.Errorf("-w printed %q", out.String())
	}

	// -w writes through a symlink, to the file it names, and leaves no
	// backup behind.
	if err := os.WriteFile(filepath.Join(dir, "real.asuka"), []byte(messy), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink("real.asuka", filepath.Join(dir, "link.asuka")); err != nil {
		t.Fatal(err)
	}
	if err := runFmt([]string{filepath.Join(dir, "link.asuka")}, true, false, false, nil, &out, &errOut); err != nil {
		t.Fatal(err)
	}
	if fi, err := os.Lstat(filepath.Join(dir, "link.asuka")); err != nil || fi.Mode()&fs.ModeSymlink == 0 {
		t.Errorf("-w replaced the symlink: %v", err)
	}
	if read("real.asuka") != clean {
		t.Errorf("-w didn't write the file the symlink names")
	}
	if fi, err := os.Stat(filepath.Join(dir, "real.asuka")); err != nil || fi.Mode().Perm() != 0o600 {
		t.Errorf("-w changed the mode: %v", fi.Mode())
	}
	if baks, _ := filepath.Glob(filepath.Join(dir, "*.bak")); len(baks) != 0 {
		t.Errorf("-w left %v", baks)
	}

	// Standard input.
	out.Reset()
	if err := runFmt(nil, false, false, false, strings.NewReader(messy), &out, &errOut); err != nil || out.String() != clean {
		t.Errorf("stdin: %q, %v", out.String(), err)
	}
	if err := runFmt(nil, true, false, false, strings.NewReader(messy), &out, &errOut); err == nil {
		t.Errorf("-w with standard input")
	}
}

// asuka fmt --indented writes blocks by indentation, and the result is
// the same program: formatted again without it, the braced form.
func TestFmtIndented(t *testing.T) {
	src := "component A {\n  Column {\n    if true {\n      Text(\"a\")\n    } else {\n      Text(\"b\")\n    }\n  }\n}\n"
	want := "component A\n  Column\n    if true\n      Text(\"a\")\n    else\n      Text(\"b\")\n"
	var out, errOut bytes.Buffer
	if err := runFmt(nil, false, false, true, strings.NewReader(src), &out, &errOut); err != nil || out.String() != want {
		t.Fatalf("fmt --indented: %v %s\n%q\nwant\n%q", err, errOut.String(), out.String(), want)
	}
	var back bytes.Buffer
	if err := runFmt(nil, false, false, false, strings.NewReader(out.String()), &back, &errOut); err != nil || back.String() != src {
		t.Fatalf("fmt of the indented form: %v %s\n%q\nwant\n%q", err, errOut.String(), back.String(), src)
	}
}
