package syntax

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// toIndented writes a formatted file's braced blocks by indentation where
// rfcs/0018 lets it: a line that ends with " {" whose block closes on a
// line of its own at the same indentation ("}", or "} else", "} catch"),
// holds a line of code, and doesn't open inside ( ) or [ ] (where
// newlines don't count); not a closure's ("=> {"), not one whose first
// line is a .name the scanner would join to the line before. Everything
// else stays braced. It reports how many lines it took out.
func toIndented(src string) (string, int) {
	lines := strings.Split(src, "\n")
	indentOf := func(l string) int { return len(l) - len(strings.TrimLeft(l, " ")) }
	// The ( and [ open at each line's end, strings aside.
	depth := make([]int, len(lines))
	d := 0
	for i, l := range lines {
		inStr := false
		for k := 0; k < len(l); k++ {
			switch c := l[k]; {
			case c == '\\' && inStr:
				k++
			case c == '"':
				inStr = !inStr
			case inStr:
			case c == '(' || c == '[':
				d++
			case c == ')' || c == ']':
				d--
			}
		}
		depth[i] = d
	}
	open := make([]bool, len(lines))
	closer := make([]int, len(lines))
	for i := range closer {
		closer[i] = -1
	}
	for i, l := range lines {
		t := strings.TrimSpace(l)
		if !strings.HasSuffix(t, " {") || strings.HasSuffix(t, "=> {") || strings.Contains(l, "//") || depth[i] > 0 {
			continue
		}
		in := indentOf(l)
		j := i + 1
		for j < len(lines) && (strings.TrimSpace(lines[j]) == "" || indentOf(lines[j]) > in) {
			j++
		}
		if j >= len(lines) || j == i+1 || indentOf(lines[j]) != in {
			continue
		}
		ct := strings.TrimSpace(lines[j])
		if ct != "}" && !strings.HasPrefix(ct, "} else") && !strings.HasPrefix(ct, "} catch") {
			continue
		}
		body, code := strings.TrimSpace(lines[i+1]), false
		for k := i + 1; k < j; k++ {
			if b := strings.TrimSpace(lines[k]); b != "" && !strings.HasPrefix(b, "//") {
				code = true
				break
			}
		}
		if !code || strings.HasPrefix(body, ".") && !strings.Contains(t, "match ") {
			continue
		}
		open[i], closer[j] = true, i
	}
	var out []string
	removed := 0
	for i, l := range lines {
		if closer[i] >= 0 {
			t := strings.TrimSpace(l)
			if t == "}" {
				removed++
				continue
			}
			l = strings.Repeat(" ", indentOf(l)) + strings.TrimPrefix(t, "} ")
		}
		if open[i] {
			l = strings.TrimSuffix(l, " {")
		}
		out = append(out, l)
	}
	return strings.Join(out, "\n"), removed
}

// Every .asuka file of the examples, std and sdk, written by indentation
// where it can be, is the same program: the printer writes it the same.
func TestIndentedRepository(t *testing.T) {
	root := filepath.Join("..", "..")
	files, total, removed := 0, 0, 0
	for _, dir := range []string{"examples", "std", "sdk"} {
		filepath.WalkDir(filepath.Join(root, dir), func(path string, d os.DirEntry, err error) error {
			if err != nil || d.IsDir() || !strings.HasSuffix(path, ".asuka") || strings.Contains(path, "testdata") {
				return nil
			}
			src, err := os.ReadFile(path)
			if err != nil || strings.Contains(string(src), `"""`) {
				return nil
			}
			want, err := Format(path, src)
			if err != nil {
				return nil // not this test's to judge
			}
			ind, n := toIndented(string(want))
			got, err := Format(path, []byte(ind))
			if err != nil {
				t.Errorf("%s: written by indentation: %v", path, err)
				return nil
			}
			if string(got) != string(want) {
				t.Errorf("%s: written by indentation, a different program:\n%s", path, firstDiff(string(got), string(want)))
				return nil
			}
			files++
			total += strings.Count(string(want), "\n")
			removed += n
			return nil
		})
	}
	if files < 100 {
		t.Fatalf("only %d files checked", files)
	}
	t.Logf("%d files, %d lines, %d lines that only closed a block taken out", files, total, removed)
}

// asuka fmt's indented form of every example, std and sdk file is the
// same program (formatted with braces again it's the braced form), and
// formatting it again changes nothing.
func TestFormatIndentedRepository(t *testing.T) {
	root := filepath.Join("..", "..")
	files, braced, indented := 0, 0, 0
	for _, dir := range []string{"examples", "std", "sdk"} {
		filepath.WalkDir(filepath.Join(root, dir), func(path string, d os.DirEntry, err error) error {
			if err != nil || d.IsDir() || !strings.HasSuffix(path, ".asuka") || strings.Contains(path, "testdata") {
				return nil
			}
			src, err := os.ReadFile(path)
			if err != nil {
				return nil
			}
			want, err := Format(path, src)
			if err != nil {
				return nil // not this test's to judge
			}
			ind, err := FormatIndented(path, src)
			if err != nil {
				t.Errorf("%s: FormatIndented: %v", path, err)
				return nil
			}
			back, err := Format(path, ind)
			if err != nil {
				t.Errorf("%s: its indented form doesn't parse: %v\n%s", path, err, ind)
				return nil
			}
			if string(back) != string(want) {
				t.Errorf("%s: its indented form is a different program:\n%s", path, firstDiff(string(back), string(want)))
				return nil
			}
			again, err := FormatIndented(path, ind)
			if err != nil || string(again) != string(ind) {
				t.Errorf("%s: formatting the indented form again changes it:\n%s", path, firstDiff(string(again), string(ind)))
				return nil
			}
			files++
			braced += strings.Count(string(want), "\n")
			indented += strings.Count(string(ind), "\n")
			return nil
		})
	}
	if files < 150 {
		t.Fatalf("only %d files checked", files)
	}
	t.Logf("%d files: %d lines braced, %d indented (%d fewer)", files, braced, indented, braced-indented)
}
