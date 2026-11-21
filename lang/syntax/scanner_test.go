package syntax

import (
	"strings"
	"testing"

	"github.com/egoist/asuka/lang/diag"
)

// scanAll scans src and returns its tokens, one string each: a
// literal's kind and text (int:42), an operator or a keyword as
// itself, and NL for a newline.
func scanAll(src string) (toks []string, errs []string, s *Scanner) {
	fset := NewFileSet()
	file := fset.AddFile("x.asuka", len(src))
	s = &Scanner{}
	s.Init(file, []byte(src), func(pos Pos, _ *diag.Code, msg string) {
		errs = append(errs, file.Position(pos).String()+": "+msg)
	})
	for {
		_, tok, lit := s.Scan()
		switch {
		case tok == EOF:
			return toks, errs, s
		case tok == NEWLINE:
			toks = append(toks, "NL")
		case tok.IsLiteral():
			name := map[Token]string{
				IDENT: "ident", INT: "int", FLOAT: "float", DURATION: "duration", LENGTH: "length",
				COLOR: "color", STRING: "string", STRING_HEAD: "head", STRING_MID: "mid", STRING_TAIL: "tail", FORMAT_SPEC: "spec",
			}[tok]
			toks = append(toks, name+":"+lit)
		default:
			toks = append(toks, tok.String())
		}
	}
}

func TestScanTokens(t *testing.T) {
	tests := []struct {
		src  string
		want string // tokens joined by spaces
	}{
		// Numbers.
		{"42 1_000 0xff 0XFF_FF 0", "int:42 int:1_000 int:0xff int:0XFF_FF int:0"},
		{"0b1010 0B1_0 0o17 0O7_7", "int:0b1010 int:0B1_0 int:0o17 int:0O7_7"},
		{"3.14 1e-3 2.5E+10 1e3 0.5", "float:3.14 float:1e-3 float:2.5E+10 float:1e3 float:0.5"},
		{"1.foo", "int:1 . ident:foo"},
		{"x.0", "ident:x . int:0"},
		// A tuple's elements: after a . an index alone, as Rust scans it.
		{"t.0.1 t?.1 t.10.2", "ident:t . int:0 . int:1 ident:t ?. int:1 ident:t . int:10 . int:2"},
		{"f().1.0", "ident:f ( ) . int:1 . int:0"},
		// Durations and lengths.
		{"300ms 5s 2min 1h 1.5s", "duration:300ms duration:5s duration:2min duration:1h duration:1.5s"},
		{"50% 1fr 12.5% 100%", "length:50% length:1fr length:12.5% length:100%"},
		{"10%3 10 % 3 a%b", "int:10 % int:3 int:10 % int:3 ident:a % ident:b"},
		{"[50%, 1fr]", "[ length:50% , length:1fr ]"},
		{"width: 50%\n", "ident:width : length:50% NL"},
		// Colors.
		{"#fff #ffffff #ffffff80 #FFFA", "color:#fff color:#ffffff color:#ffffff80 color:#FFFA"},
		// Strings.
		{`"a\n\"b\\"`, `string:"a\n\"b\\"`},
		{`r"C:\x\{y}"`, `string:r"C:\x\{y}"`},
		{"\"\"\"\nhi \"there\"\n\"\"\"", "string:\"\"\"\nhi \"there\"\n\"\"\""},
		{`"a{b}c{d + 1}e"`, `head:"a{ ident:b mid:}c{ ident:d + int:1 tail:}e"`},
		{`"{f("x")}"`, `head:"{ ident:f ( string:"x" ) tail:}"`},
		{`"{ [1: 2] }"`, `head:"{ [ int:1 : int:2 ] tail:}"`},
		{`"{items.map(x => { x })}!"`, `head:"{ ident:items . ident:map ( ident:x => { ident:x } ) tail:}!"`},
		{`"\{not} \u{1F600}"`, `string:"\{not} \u{1F600}"`},
		{"\"\"\"\n{a}\n\"\"\"", "head:\"\"\"\n{ ident:a tail:}\n\"\"\""},
		{`"{"inner {x}"}"`, `head:"{ head:"inner { ident:x tail:}" tail:}"`},
		// A format: what follows a colon the interpolation's expression
		// doesn't hold, up to the }.
		{`"{x:>8.2f} {n:#x}!"`, `head:"{ ident:x spec:>8.2f mid:} { ident:n spec:#x tail:}!"`},
		{`"{x:*^9}{y:}"`, `head:"{ ident:x spec:*^9 mid:}{ ident:y spec: tail:}"`},
		{`"{f(a: 1):<4} {m["k"]:>3}"`, `head:"{ ident:f ( ident:a : int:1 ) spec:<4 mid:} { ident:m [ string:"k" ] spec:>3 tail:}"`},
		// Identifiers and keywords.
		{"count Text _ _x ünï", "ident:count ident:Text ident:_ ident:_x ident:ünï"},
		{"x.type .default a?.match", "ident:x . ident:type . ident:default ident:a ?. ident:match"},
		// Operators.
		{"?. ?? ??= => -> .. ..< += -= *= /= %=", "?. ?? ??= => -> .. ..< += -= *= /= %="},
		{"== != < <= > >= && || ! ? | : ; ,", "== != < <= > >= && || ! ? | : ; ,"},
		{"& | ^ ~ << &= |= ^= <<=", "& | ^ ~ << &= |= ^= <<="},
		{"a >> b >>= c", "ident:a > > ident:b > >= ident:c"},
		{"&+ &- &* &+= &-= &*= a&-b a & -b", "&+ &- &* &+= &-= &*= ident:a &- ident:b ident:a & - ident:b"},
		{"** **= * *= a**b", "** **= * *= ident:a ** ident:b"},
		{"a &\nb", "ident:a & ident:b"},
		{"a..<b a..b", "ident:a ..< ident:b ident:a .. ident:b"},
		{"import ./ui.{A, B}", "import . / ident:ui .{ ident:A , ident:B }"},
		{"x! y?", "ident:x ! ident:y ?"},
		{"Map<K, Loading<V>>", "ident:Map < ident:K , ident:Loading < ident:V > >"},
		// Comments.
		{"a // c\nb", "ident:a NL ident:b"},
		{"a /* c */ b", "ident:a ident:b"},
		{"a /* c\n */ b", "ident:a NL ident:b"},
		{"/// doc\nfn", "fn"},
	}
	for _, tt := range tests {
		toks, errs, _ := scanAll(tt.src)
		if len(errs) > 0 {
			t.Errorf("%q: errors %v", tt.src, errs)
		}
		if got := strings.Join(toks, " "); got != tt.want {
			t.Errorf("%q:\n got %s\nwant %s", tt.src, got, tt.want)
		}
	}
}

func TestScanKeywords(t *testing.T) {
	for kw := keywordBeg + 1; kw < keywordEnd; kw++ {
		toks, errs, _ := scanAll(kw.String())
		if len(errs) > 0 || len(toks) != 1 || toks[0] != kw.String() {
			t.Errorf("%s: %v %v", kw, toks, errs)
		}
		if Lookup(kw.String()) != kw {
			t.Errorf("Lookup(%q) = %v", kw.String(), Lookup(kw.String()))
		}
	}
	// RFC 0007's, and RFC 0011's mut and extend.
	const rfc = `app as async await bind break catch component continue
  defer do effect else emit enum event extend extern false fn for if
  import in interface key let load match mut native none on prop provide
  pub reload return screen self spawn state store struct style task test
  throw throws true try type use var while worker`
	n := 0
	for _, w := range strings.Fields(rfc) {
		n++
		if !Lookup(w).IsKeyword() {
			t.Errorf("%q isn't a keyword", w)
		}
	}
	if n != int(keywordEnd-keywordBeg-1) {
		t.Errorf("%d keywords, the RFC lists %d", keywordEnd-keywordBeg-1, n)
	}
}

func TestScanNewlines(t *testing.T) {
	tests := []struct{ src, want string }{
		{"a\nb", "ident:a NL ident:b"},
		{"\n\na\n\n\nb\n\n", "ident:a NL ident:b NL"},
		{"a; b", "ident:a ; ident:b"},
		// After an operator, a comma, an opening bracket, =>, ->: no newline.
		{"a +\nb", "ident:a + ident:b"},
		{"a ==\nb", "ident:a == ident:b"},
		{"a &&\nb", "ident:a && ident:b"},
		{"a ??\nb", "ident:a ?? ident:b"},
		{"x =\n1", "ident:x = int:1"},
		{"x +=\n1", "ident:x += int:1"},
		{"x =>\ny", "ident:x => ident:y"},
		{"fn f() ->\nT", "fn ident:f ( ) -> ident:T"},
		{"a.\nb", "ident:a . ident:b"},
		{"a,\nb", "ident:a , ident:b"},
		{"a:\nb", "ident:a : ident:b"},
		{"0..\n9", "int:0 .. int:9"},
		{"} else\n{", "} else {"},
		// Inside ( ) and [ ]: none; inside { }: again.
		{"f(\na,\nb\n)\nc", "ident:f ( ident:a , ident:b ) NL ident:c"},
		{"[\n1,\n2\n]", "[ int:1 , int:2 ]"},
		{"{\na\nb\n}", "{ ident:a NL ident:b NL }"},
		{"f(x => {\na\nb\n})", "ident:f ( ident:x => { ident:a NL ident:b NL } )"},
		{"\"\"\"{a +\nb}\"\"\"", "head:\"\"\"{ ident:a + ident:b tail:}\"\"\""},
		// Not after ?, ! or >: a line may end with them.
		{"x: T?\ny", "ident:x : ident:T ? NL ident:y"},
		{"x!\ny", "ident:x ! NL ident:y"},
		{"x: Task<T>\ny", "ident:x : ident:Task < ident:T > NL ident:y"},
		// Nor before a line that starts with a binary operator; a leading
		// .name is the parser's to join (an implicit member may start a
		// line).
		{"a\n?? b", "ident:a ?? ident:b"},
		{"a\n-b", "ident:a NL - ident:b"},
		{"a\n.b", "ident:a NL . ident:b"},
	}
	for _, tt := range tests {
		toks, errs, _ := scanAll(tt.src)
		if len(errs) > 0 {
			t.Errorf("%q: errors %v", tt.src, errs)
		}
		if got := strings.Join(toks, " "); got != tt.want {
			t.Errorf("%q:\n got %s\nwant %s", tt.src, got, tt.want)
		}
	}
}

func TestScanComments(t *testing.T) {
	src := "/// Doc.\n/// More.\n//// not doc\n// plain\n/* block */ x /* two\nlines */\n"
	_, errs, s := scanAll(src)
	if len(errs) > 0 {
		t.Fatal(errs)
	}
	want := []struct {
		text string
		doc  bool
	}{
		{"/// Doc.", true}, {"/// More.", true}, {"//// not doc", false}, {"// plain", false},
		{"/* block */", false}, {"/* two\nlines */", false},
	}
	cs := s.Comments()
	if len(cs) != len(want) {
		t.Fatalf("%d comments, want %d", len(cs), len(want))
	}
	for i, w := range want {
		if cs[i].Text != w.text || cs[i].IsDoc() != w.doc {
			t.Errorf("comment %d: %q doc=%v, want %q doc=%v", i, cs[i].Text, cs[i].IsDoc(), w.text, w.doc)
		}
	}
}

func TestScanPositions(t *testing.T) {
	src := "a\n  bc \"d{e}\"\n\t#fff"
	fset := NewFileSet()
	file := fset.AddFile("p.asuka", len(src))
	var s Scanner
	s.Init(file, []byte(src), nil)
	// The deeper second line is a's block (rfcs/0018): its { at the end
	// of line 1, its } and the NEWLINEs at the end of line 2.
	want := []string{"p.asuka:1:1", "p.asuka:1:2", "p.asuka:2:3", "p.asuka:2:6", "p.asuka:2:9", "p.asuka:2:10", "p.asuka:2:12", "p.asuka:2:12", "p.asuka:2:12", "p.asuka:3:2"}
	for i := 0; ; i++ {
		pos, tok, lit := s.Scan()
		if tok == EOF {
			if i != len(want) {
				t.Errorf("%d tokens, want %d", i, len(want))
			}
			break
		}
		if i >= len(want) {
			t.Fatalf("extra token %v %q", tok, lit)
		}
		if got := fset.Position(pos).String(); got != want[i] {
			t.Errorf("token %d (%v %q) at %s, want %s", i, tok, lit, got, want[i])
		}
	}
}

func TestScanErrors(t *testing.T) {
	tests := []struct {
		src  string
		err  string // the first error, with its position
		want string // the tokens: scanning goes on
	}{
		{`"abc` + "\nx", `1:1: string not terminated`, `string:"abc NL ident:x`},
		{"#12 x", "1:1: a color has 3, 4, 6 or 8 hexadecimal digits, not 2", "color:#12 ident:x"},
		{"#zz x", "1:1: a color is #rgb", "color:#zz ident:x"},
		{"0x x", "1:1: a hexadecimal number needs digits after 0x", "int:0x ident:x"},
		{"1_ x", "1:2: '_' must separate digits", "int:1_ ident:x"},
		{"5kg x", `1:2: unknown unit "kg" after a number`, "int:5kg ident:x"},
		{`"\q" x`, `1:2: unknown escape \q`, `string:"\q" ident:x`},
		{`"\u{D800}" x`, `1:2: \u{D800} isn't a Unicode code point`, `string:"\u{D800}" ident:x`},
		{`"\u{}" x`, `1:2: a \u escape is \u{1 to 6 hex digits}`, `string:"\u{}" ident:x`},
		{"0b12 x", "1:4: unexpected '2' in a binary number", "int:0b12 ident:x"},
		{"0o8 x", "1:1: an octal number needs digits after 0o", "int:0o8 ident:x"},
		{"a $ b", "1:3: unexpected character '$'", "ident:a ILLEGAL ident:b"},
		{"a ... b", "1:3: unexpected '...'", "ident:a .. ident:b"},
		{"x /* open", "1:3: comment not terminated", "ident:x"},
		{`"{a`, "1:1: string not terminated", `head:"{ ident:a`},
		{"x = \"{a\ny", "1:5: string not terminated", `ident:x = head:"{ ident:a NL ident:y`},
	}
	for _, tt := range tests {
		toks, errs, _ := scanAll(tt.src)
		if len(errs) == 0 {
			t.Errorf("%q: no error, want %q", tt.src, tt.err)
			continue
		}
		if !strings.HasPrefix(errs[0], "x.asuka:"+tt.err) {
			t.Errorf("%q: error %q, want %q", tt.src, errs[0], tt.err)
		}
		if got := strings.Join(toks, " "); got != tt.want {
			t.Errorf("%q:\n got %s\nwant %s", tt.src, got, tt.want)
		}
	}
}
