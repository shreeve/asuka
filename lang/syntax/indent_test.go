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
		{"braced code indented with tabs, as in a Go string",
			"\tcomponent A {\n\t\tColumn {\n\t\t\tText(\"a\")\n\t\t}\n\t}\n",
			"component A {\n  Column {\n    Text(\"a\")\n  }\n}\n"},
		{"a deeper line after a closing brace opens nothing",
			"\tcomponent A {\n\t  Text(\"a\")\n\t}\n\n\t\tcomponent B {\n\t\t  Text(\"b\")\n\t\t}\n",
			"component A {\n  Text(\"a\")\n}\n\ncomponent B {\n  Text(\"b\")\n}\n"},
		{"tab-indented lines open nothing: after an import, after a one-line block",
			"import std/math\n\t\tcomponent R {\n\t\t  Column {\n\t\t    gap: 4\nText(\"a\") { width: 16 }\n\t\t    Text(\"b\")\n\t\t  }\n\t\t}\n",
			"import std/math\ncomponent R {\n  Column {\n    gap: 4\n    Text(\"a\") { width: 16 }\n    Text(\"b\")\n  }\n}\n"},
		{"a one-line block then a deeper line: braced code under uneven indentation",
			"component A {\n  Column {\nText(\"a\") { width: 16 }\n    Text(\"b\")\nButton(\"c\") { on press { n = 1 } }\n  }\n}\n",
			"component A {\n  Column {\n    Text(\"a\") { width: 16 }\n    Text(\"b\")\n    Button(\"c\") { on press { n = 1 } }\n  }\n}\n"},
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

// A line that comes back to an indentation no block has, or a tab in the
// indentation of a line inside an indented block, is an indentation
// error, where it is.
func TestIndentationErrors(t *testing.T) {
	tests := []struct{ src, where string }{
		{"fn f() -> int\n    let a = 1\n  return a\n", "t.asuka:3:3"},
		{"fn f() -> int\n  let a = 1\n\t return a\n", "t.asuka:3:1"},
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
