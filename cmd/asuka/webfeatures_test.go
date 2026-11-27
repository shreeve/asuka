package main

import (
	"os"
	"path/filepath"
	"slices"
	"testing"
)

// asuka.toml's web.fontFeatures takes OpenType tags, and refuses anything
// else with what it wants.
func TestWebFontFeatures(t *testing.T) {
	dir := t.TempDir()
	write := func(doc string) {
		if err := os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(doc), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	write("[web]\nfontFeatures = [\"cv11\", \"ss01\", \"tnum\"]\n")
	c, err := loadConfig(dir)
	if err != nil || !slices.Equal(c.Web.FontFeatures, []string{"cv11", "ss01", "tnum"}) {
		t.Fatalf("got %v, %v", c, err)
	}
	write("[web]\nfontFeatures = [\"tnum\", \"tabular\"]\n")
	_, err = loadConfig(dir)
	if want := `asuka.toml: web.fontFeatures "tabular" (want a four-letter OpenType tag, as "tnum")`; err == nil || err.Error() != want {
		t.Fatalf("got %v, want %s", err, want)
	}
}
