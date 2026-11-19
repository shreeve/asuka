package syntax

import (
	"strings"
	"testing"

	"github.com/egoist/asuka/lang/diag"
)

// Blocks by indentation (rfcs/0018): each program written by indentation
// is the same program as its braced form, so the printer writes both the
// same.
func TestIndentedBlocks(t *testing.T) {
	tests := []struct{ name, indented, braced string }{
		{"component and elements",
			"component Greeting(name: string)\n  state shown = true\n  Column\n    gap: 8\n    Text(\"Hello, {name}\") { font: .title }\n    Button(\"Hide\") { on press { shown = false } }\n",
			"component Greeting(name: string) {\n  state shown = true\n  Column {\n    gap: 8\n    Text(\"Hello, {name}\") { font: .title }\n    Button(\"Hide\") { on press { shown = false } }\n  }\n}\n"},
		{"if and else",
			"fn f(x: int) -> string\n  if x > 0\n    return \"up\"\n  else if x < 0\n    return \"down\"\n  else\n    return \"flat\"\n",
			"fn f(x: int) -> string {\n  if x > 0 {\n    return \"up\"\n  } else if x < 0 {\n    return \"down\"\n  } else {\n    return \"flat\"\n  }\n}\n"},
		{"match arms",
			"fn f(t: Tone) -> int\n  return match t\n    .ok => 1\n    .bad => 2\n",
			"fn f(t: Tone) -> int {\n  return match t {\n    .ok => 1\n    .bad => 2\n  }\n}\n"},
		{"a chain continues, a block opens",
			"fn f(xs: [int]) -> [int]\n  let ys = xs\n    .filter(x => x > 0)\n    .map(x => x * 2)\n  return ys\n",
			"fn f(xs: [int]) -> [int] {\n  let ys = xs\n    .filter(x => x > 0)\n    .map(x => x * 2)\n  return ys\n}\n"},
		{"closing several at once, and at the end of the file",
			"component A\n  Column\n    Row\n      Text(\"a\")\ncomponent B\n  Column\n    Row\n      Text(\"b\")",
			"component A {\n  Column {\n    Row {\n      Text(\"a\")\n    }\n  }\n}\ncomponent B {\n  Column {\n    Row {\n      Text(\"b\")\n    }\n  }\n}\n"},
		{"braces inside indentation, indentation inside braces",
			"component A {\n  Column\n    for i in [1, 2] key i\n      Text(\"{i}\")\n}\ncomponent B\n  Column {\n    Text(\"b\")\n  }\n",
			"component A {\n  Column {\n    for i in [1, 2] key i {\n      Text(\"{i}\")\n    }\n  }\n}\ncomponent B {\n  Column {\n    Text(\"b\")\n  }\n}\n"},
		{"a closure in parentheses keeps its braces",
			"fn f(xs: [int]) -> [int]\n  return xs.map(x => {\n    let y = x * 2\n    return y\n  })\n",
			"fn f(xs: [int]) -> [int] {\n  return xs.map(x => {\n    let y = x * 2\n    return y\n  })\n}\n"},
		{"blank lines and comments",
			"fn f() -> int\n  // a comment\n\n  let a = 1\n    // deeper comment\n  return a\n",
			"fn f() -> int {\n  // a comment\n\n  let a = 1\n    // deeper comment\n  return a\n}\n"},
		{"struct and store",
			"struct P\n  x: int\n  y: int\nstore S\n  state n = 0\n  pub fn add()\n    n += 1\n",
			"struct P {\n  x: int\n  y: int\n}\nstore S {\n  state n = 0\n  pub fn add() {\n    n += 1\n  }\n}\n"},
	}
	for _, tt := range tests {
		got, err := Format("i.asuka", []byte(tt.indented))
		if err != nil {
			t.Errorf("%s: the indented form: %v", tt.name, err)
			continue
		}
		want, err := Format("b.asuka", []byte(tt.braced))
		if err != nil {
			t.Errorf("%s: the braced form: %v", tt.name, err)
			continue
		}
		if string(got) != string(want) {
			t.Errorf("%s: the two forms differ:\n%s", tt.name, firstDiff(string(got), string(want)))
		}
	}
}

// A line that comes back to an indentation no block has, or indents with
// a tab, is an indentation error, where it is.
func TestIndentationErrors(t *testing.T) {
	tests := []struct{ src, where string }{
		{"fn f() -> int\n    let a = 1\n  return a\n", "t.asuka:3:3"},
		{"fn f() -> int\n\tlet a = 1\n\treturn a\n", "t.asuka:2:1"},
	}
	for _, tt := range tests {
		_, errs := ParseFile(NewFileSet(), "t.asuka", []byte(tt.src))
		found := false
		for _, e := range errs {
			if e.Code == diag.Indentation.Name && strings.HasPrefix(e.Pos.String(), tt.where) {
				found = true
			}
		}
		if !found {
			t.Errorf("%q: no indentation error at %s, got %v", tt.src, tt.where, errs)
		}
	}
}
