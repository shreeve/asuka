package syntax

// The printer: a tree back to source, in the one canonical form asuka
// fmt writes. Like gofmt, it keeps the line breaks the author chose
// where the form allows more than one, and fixes the rest.
//
//   - Two spaces of indentation; prop sets as name: value; spaces
//     around binary operators, none around .. and ..< (0..<n), none
//     inside brackets.
//   - One member, item, field, case or arm per line, except those the
//     source had on one line, which stay there separated by ; (an
//     enum's cases and a match's arms by ,): gap: 8; padding: 16.
//   - A block, a struct's or an enum's body, a match's arms stay on one
//     line when they were written on one line and hold no comment (an
//     enum: only cases): Column { height: 1; background: x }. Otherwise
//     each item goes on a line of its own, and an empty block is {}.
//   - Arguments, parameters (a closure's and a handler's too), type
//     parameters and arguments, arrays, maps, tuples (and tuple and
//     function types, tuple and case patterns), a task's dependencies
//     and an import's names go a line each (those the source had on one
//     line together), each with a trailing comma, when the first was
//     written on the line after the opening bracket or a comment sits
//     between them; otherwise on one line, but for the line breaks the
//     source had after their commas, the next lines indented once more.
//     A blank line between two of them is kept.
//   - A binary expression broken before or after its operator, a chain
//     broken before a . or a ?., and a value broken after its =, its :
//     or its =>, stay broken there, the next lines indented once more.
//   - } else and } catch go on the }'s line. Imports' .{…} follow the
//     path directly: import ./components.{Row, Card}. An optional
//     function type is (fn(…))?, its parentheses kept whether they're
//     needed (fn() -> int?, a function that returns an optional, isn't
//     one) or not.
//   - Comments are kept: above the node that follows them, after the
//     node on its line, or at the end of a block. The comments after the
//     code of consecutive lines start in one column, as gofmt aligns
//     them. A comment inside an expression that can't stay where it was
//     (a // in a binary expression joined on one line) moves above the
//     line it was on. Blank lines between items are kept, at most one in
//     a row; none after an opening brace or before a closing one.
//
// Printing a file the printer wrote gives the same bytes again.
//
// FormatIndented writes the same program with its blocks by indentation
// (rfcs/0018): a block of more than one line without its { and }, where
// the scanner reads the indentation back as the same block. A block
// keeps its braces inside ( ) or [ ] (where newlines don't count), after
// =>, when it's empty, when a comment follows its {, when its first line
// starts with .name outside a match (the scanner would join it to the
// line before), and when anything but else or catch follows its } on its
// line (a trailing block's handlers, rule 5). else and catch go on a
// line of their own.

import (
	"bytes"
	"io"
	"slices"
	"sort"
	"strings"
	"unicode/utf8"
)

// Format parses src as the file filename and returns it in canonical
// form. A file with syntax errors isn't formatted: Format returns its
// ErrorList.
func Format(filename string, src []byte) ([]byte, error) {
	return format(filename, src, false)
}

// FormatIndented is Format with the blocks written by indentation where
// they can be (rfcs/0018).
func FormatIndented(filename string, src []byte) ([]byte, error) {
	return format(filename, src, true)
}

func format(filename string, src []byte, indented bool) ([]byte, error) {
	fset := NewFileSet()
	f, errs := ParseFile(fset, filename, src)
	if len(errs) > 0 {
		return nil, errs
	}
	var b bytes.Buffer
	if err := fprint(&b, f.File, f, indented); err != nil {
		return nil, err
	}
	return b.Bytes(), nil
}

// Fprint writes node to w in canonical form. file has node's positions,
// which say where the source broke lists across lines and left blank
// lines; with a nil file (a tree made in code) lists go on one line.
// A *SourceFile prints with all its comments, any other node with the
// comments its Trivia hold.
func Fprint(w io.Writer, file *File, node Node) error {
	return fprint(w, file, node, false)
}

// fprint prints node, its blocks by indentation if indented. A block
// written so and then followed on its line by more than else or catch
// keeps its braces, and the node is printed again.
func fprint(w io.Writer, file *File, node Node, indented bool) error {
	keep := map[Pos]bool{}
	for {
		p := &printer{file: file, printed: map[*Comment]bool{}, indented: indented, keep: keep}
		if f, ok := node.(*SourceFile); ok {
			if p.file == nil {
				p.file = f.File
			}
			p.comments = f.Comments
			p.sourceFile(f)
		} else {
			p.node(node)
		}
		if !p.retry {
			_, err := w.Write(p.out)
			return err
		}
	}
}

type printer struct {
	out      []byte
	file     *File
	comments []*Comment // the file's, in source order
	printed  map[*Comment]bool
	broken   map[Pos]bool // bodies, by their {, written on one line that don't fit on one
	indent   int
	bol      bool // at the start of a line, its indentation not written yet

	// Blocks by indentation: whether to write them, the ( and [ open,
	// the { of the block just written without braces (until its line
	// ends), the blocks that keep their braces, and whether one more was
	// found to (the file is printed again).
	indented bool
	depth    int
	pending  Pos
	keep     map[Pos]bool
	retry    bool
}

// ---------------------------------------------------------------------
// Output

func (p *printer) write(s string) {
	if p.pending != NoPos && s != "" {
		if strings.HasPrefix(s, " else") || strings.HasPrefix(s, " catch") {
			p.pending = NoPos
			p.newline()
			s = s[1:]
		} else {
			// Something follows the block on its line: it keeps its braces.
			p.keep[p.pending], p.retry, p.pending = true, true, NoPos
		}
	}
	for i := 0; i < len(s); i++ {
		switch s[i] {
		case '(', '[':
			p.depth++
		case ')', ']':
			p.depth--
		}
	}
	p.raw(s)
}

// raw writes s, after the line's indentation, counting no brackets in
// it (a comment's text).
func (p *printer) raw(s string) {
	if p.bol {
		for range p.indent {
			p.out = append(p.out, "  "...)
		}
		p.bol = false
	}
	p.out = append(p.out, s...)
}

func (p *printer) newline() {
	p.out = append(p.out, '\n')
	p.bol = true
	p.pending = NoPos
}

func (p *printer) comment(c *Comment) {
	text := c.Text
	if strings.HasPrefix(text, "//") {
		text = strings.TrimRight(text, " \t\r")
	} else if strings.Contains(text, "\r\n") {
		text = strings.ReplaceAll(text, "\r\n", "\n") // as every line printed
	}
	if p.pending != NoPos {
		p.write(" ") // a comment on the block's last line: it keeps its braces
	}
	p.raw(text)
	p.printed[c] = true
}

// ---------------------------------------------------------------------
// Positions and comments

func (p *printer) line(pos Pos) int {
	if p.file == nil || !pos.IsValid() || int(pos) < p.file.Base() || int(pos) > p.file.Base()+p.file.Size() {
		return 0
	}
	return p.file.Line(pos)
}

// sameLine reports whether a and b were on one line of the source (no
// when there's no source).
func (p *printer) sameLine(a, b Pos) bool {
	la, lb := p.line(a), p.line(b)
	return la > 0 && la == lb
}

// endLine is the line of a node's last character.
func (p *printer) endLine(n Node) int {
	end := n.Range().End
	if !end.IsValid() {
		return 0
	}
	return p.line(end - 1)
}

// firstComment is the index of the first comment at or after pos.
func (p *printer) firstComment(pos Pos) int {
	return sort.Search(len(p.comments), func(i int) bool { return p.comments[i].Pos >= pos })
}

// commentsIn reports whether a comment, printed or not, is in [lo, hi).
func (p *printer) commentsIn(lo, hi Pos) bool {
	i := p.firstComment(lo)
	return i < len(p.comments) && p.comments[i].Pos < hi
}

// unprinted are the comments in [lo, hi) not printed yet.
func (p *printer) unprinted(lo, hi Pos) []*Comment {
	var list []*Comment
	for i := p.firstComment(lo); i < len(p.comments) && p.comments[i].Pos < hi; i++ {
		if c := p.comments[i]; !p.printed[c] {
			list = append(list, c)
		}
	}
	return list
}

// lineNode prints a node that starts a line with print, then moves any
// comment inside it that print didn't place above it. It returns where
// the node's code starts in the output, below those comments.
func (p *printer) lineNode(n Node, print func()) int {
	p.write("")
	mark := len(p.out)
	print()
	r := n.Range()
	left := p.unprinted(r.Start, r.End)
	if len(left) == 0 {
		return mark
	}
	tail := append([]byte(nil), p.out[mark:]...)
	p.out = p.out[:mark]
	for _, c := range left {
		p.comment(c)
		p.newline()
		p.write("")
	}
	p.out = append(p.out, tail...)
	return len(p.out) - len(tail)
}

// ownLines prints the unprinted comments in [lo, hi), each on its own
// line, before what comes next on the current line.
func (p *printer) ownLines(lo, hi Pos) {
	for _, c := range p.unprinted(lo, hi) {
		p.comment(c)
		p.newline()
	}
}

// leading prints the comments above a node, and the blank lines
// between them.
func (p *printer) leading(t *Trivia) {
	if t == nil {
		return
	}
	for gi, g := range t.Leading {
		if gi > 0 && g.BlankBefore {
			p.newline()
		}
		for _, c := range g.List {
			p.comment(c)
			p.newline()
		}
	}
	if len(t.Leading) > 0 && t.BlankBefore {
		p.newline()
	}
}

// blankBefore reports whether a blank line goes above a node and its
// comments.
func blankBefore(t *Trivia) bool {
	if t == nil {
		return false
	}
	if len(t.Leading) > 0 {
		return t.Leading[0].BlankBefore
	}
	return t.BlankBefore
}

// trailing prints the comment after a node on its line: its Trivia's, or
// one the tree keeps nowhere, before limit.
func (p *printer) trailing(t *Trivia, n Node, limit Pos) {
	if t != nil && t.Trailing != nil {
		if !p.printed[t.Trailing] {
			p.write(" ")
			p.comment(t.Trailing)
		}
		return
	}
	end := n.Range().End
	if !end.IsValid() {
		return
	}
	if !limit.IsValid() {
		limit = Pos(1 << 62)
	}
	l := p.line(end - 1)
	for _, c := range p.unprinted(end, limit) {
		if p.line(c.Pos) != l {
			break
		}
		p.write(" ")
		p.comment(c)
	}
}

// ---------------------------------------------------------------------
// Lists

// lines prints nodes one per line after an opening bracket at open and
// before its closer at close (NoPos when unknown, as for a file), with
// the comments around them; sep follows each node (, in lists). In a
// bracket (inside), the first line starts on a line of its own. Nodes
// the source had on one line stay so, join between them (; between
// items, a space after a list's comma). The comments after the nodes of
// consecutive lines are aligned, as gofmt's are.
func (p *printer) lines(inside bool, open Pos, nodes []Node, dangling []*CommentGroup, close Pos, sep, join string) {
	prev := open
	var notes []note
	for i, n := range nodes {
		t := TriviaOf(n)
		if i > 0 && join != "" && p.joined(nodes[i-1], n) {
			p.write(join)
			p.lineNode(n, func() { p.node(n) })
			p.write(sep)
			limit := close
			if i+1 < len(nodes) {
				limit = nodes[i+1].Range().Start
			}
			notes = p.note(notes, -1, func() { p.trailing(t, n, limit) })
			prev = n.Range().End
			continue
		}
		if i > 0 || inside {
			p.newline()
		}
		if i > 0 && (blankBefore(t) || t == nil && p.blankBetween(prev, n.Range().Start)) {
			p.newline()
		}
		if prev.IsValid() {
			if t != nil {
				for _, g := range t.Leading {
					for _, c := range g.List {
						p.printed[c] = true // printed next, by leading
					}
				}
			}
			p.ownLines(prev, n.Range().Start)
		}
		p.leading(t)
		start := p.lineNode(n, func() { p.node(n) })
		p.write(sep)
		limit := close
		if i+1 < len(nodes) {
			limit = nodes[i+1].Range().Start
		}
		notes = p.note(notes, start, func() { p.trailing(t, n, limit) })
		prev = n.Range().End
	}
	p.align(notes)
	for gi, g := range dangling {
		if len(nodes) > 0 || gi > 0 || inside {
			p.newline()
		}
		if (len(nodes) > 0 || gi > 0) && g.BlankBefore {
			p.newline()
		}
		for ci, c := range g.List {
			if ci > 0 {
				p.newline()
			}
			p.comment(c)
		}
	}
	if prev.IsValid() && close.IsValid() {
		for _, c := range p.unprinted(prev, close) {
			p.newline()
			p.comment(c)
		}
	}
}

// joined reports whether the source had b on a's line, nothing between
// them, both on one line: a; b in a block, a row of an array's elements.
// An enum's case and a method aren't joined.
func (p *printer) joined(a, b Node) bool {
	ra, rb := a.Range(), b.Range()
	if !ra.End.IsValid() || !rb.Start.IsValid() || p.commentsIn(ra.End, rb.End) {
		return false // a comment in b would move above its line, a's
	}
	if t := TriviaOf(b); t != nil && (t.HasComments() || t.BlankBefore) {
		return false
	}
	if _, ca := a.(*EnumCase); ca != isEnumCase(b) {
		return false
	}
	la := p.line(ra.Start)
	return la > 0 && la == p.endLine(a) && la == p.line(rb.Start) && la == p.endLine(b)
}

func isEnumCase(n Node) bool {
	_, ok := n.(*EnumCase)
	return ok
}

// A note is a comment printed after a line's code: where the code ends,
// and where its line starts, in the output.
type note struct {
	code, line int
}

// note runs comment, which may print a comment after the code just
// printed, and keeps where, when the line's code (from start, -1 for the
// line's start) is on one line.
func (p *printer) note(notes []note, start int, comment func()) []note {
	end := len(p.out)
	comment()
	if len(p.out) == end {
		return notes
	}
	line := bytes.LastIndexByte(p.out[:end], '\n') + 1
	if start >= 0 && line > start {
		return notes // the node took more than one line
	}
	return append(notes, note{code: end, line: line})
}

// align pads the code of consecutive lines with comments after it, so
// the comments start in one column, as a tabwriter's cells do in gofmt:
// a line without one, a blank line or a node of several lines ends a
// run.
func (p *printer) align(notes []note) {
	for i := len(notes) - 1; i >= 0; {
		j := i // the run is notes[j..i]
		for j > 0 && p.nextLine(notes[j-1].code) == notes[j].line {
			j--
		}
		if i > j {
			width := 0
			for _, n := range notes[j : i+1] {
				width = max(width, utf8.RuneCount(p.out[n.line:n.code]))
			}
			for k := i; k >= j; k-- {
				n := notes[k]
				if pad := width - utf8.RuneCount(p.out[n.line:n.code]); pad > 0 {
					p.out = slices.Insert(p.out, n.code, bytes.Repeat([]byte{' '}, pad)...)
				}
			}
		}
		i = j - 1
	}
}

// nextLine is where the output's line after the one at off starts.
func (p *printer) nextLine(off int) int {
	if k := bytes.IndexByte(p.out[off:], '\n'); k >= 0 {
		return off + k + 1
	}
	return -1
}

// blankBetween reports whether a blank line separates end and start,
// nothing between them, for a node without Trivia (an array's element).
func (p *printer) blankBetween(end, start Pos) bool {
	if !end.IsValid() || p.commentsIn(end, start) {
		return false
	}
	l := p.line(end - 1)
	return l > 0 && p.line(start) > l+1
}

// multiline reports whether a bracketed list goes one element per line:
// its first element was on a line after the opening bracket, or a
// comment sits between its elements.
func (p *printer) multiline(open, close Pos, elems []Node) bool {
	if len(elems) == 0 {
		return p.commentsIn(open, close)
	}
	if l := p.line(open); l > 0 && l != p.line(elems[0].Range().Start) {
		return true
	}
	for _, e := range elems {
		if t := TriviaOf(e); t != nil && t.HasComments() {
			return true
		}
	}
	// A comment between the elements, not inside one.
	lo := open
	for _, e := range elems {
		if p.commentsIn(lo, e.Range().Start) {
			return true
		}
		lo = e.Range().End
	}
	return p.commentsIn(lo, close)
}

// list prints elements between the brackets openTok and closeTok,
// separated by commas, on one line or one per line.
func (p *printer) list(openTok, closeTok string, open, close Pos, elems []Node) {
	p.write(openTok)
	if p.multiline(open, close, elems) {
		p.indent++
		p.lines(true, open, elems, nil, close, ",", " ")
		p.indent--
		p.newline()
		p.write(closeTok)
		return
	}
	broke := false
	for i, e := range elems {
		p.sep(elems, i, &broke)
		p.node(e)
	}
	if broke {
		p.indent--
	}
	p.write(closeTok)
}

// sep writes what goes before a list's element i on its first line: a
// comma, and a line break where the source had one after it (gofmt keeps
// those), the next lines indented once more (broke says they are).
func (p *printer) sep(elems []Node, i int, broke *bool) {
	if i == 0 {
		return
	}
	p.write(",")
	if l := p.endLine(elems[i-1]); l > 0 && p.line(elems[i].Range().Start) > l {
		if !*broke {
			*broke = true
			p.indent++
		}
		p.newline()
		return
	}
	p.write(" ")
}

// body prints { nodes } as a block's, a struct's or an enum's body.
// oneLine says it may stay on one line, sep how to separate the nodes
// there and those the source had on one line.
func (p *printer) body(lbrace Pos, comment *Comment, nodes []Node, dangling []*CommentGroup, rbrace Pos, oneLine bool, sep string) {
	if len(nodes) == 0 && len(dangling) == 0 && comment == nil && !p.commentsIn(lbrace, rbrace) {
		p.write("{}")
		return
	}
	if oneLine && len(nodes) > 0 && comment == nil && len(dangling) == 0 && !p.broken[lbrace] &&
		p.sameLine(lbrace, rbrace) && !p.commentsIn(lbrace, rbrace+1) {
		ok := true
		for _, n := range nodes {
			if t := TriviaOf(n); t != nil && t.HasComments() {
				ok = false
			}
		}
		if ok {
			// What spans lines inside (a nested block of two items)
			// breaks this one too: { on press {⏎…⏎} } would print
			// differently the next time.
			mark, bol := len(p.out), p.bol
			p.write("{ ")
			for i, n := range nodes {
				if i > 0 {
					p.write(sep)
				}
				p.node(n)
			}
			if bytes.IndexByte(p.out[mark:], '\n') < 0 {
				p.write(" }")
				return
			}
			// Printed again below, and maybe again as part of an
			// enclosing body that breaks too: don't try once more.
			p.out, p.bol = p.out[:mark], bol
			if p.broken == nil {
				p.broken = map[Pos]bool{}
			}
			p.broken[lbrace] = true
		}
	}
	if p.canIndent(lbrace, comment, nodes) {
		if n := len(p.out); n > 0 && p.out[n-1] == ' ' {
			p.out = p.out[:n-1] // the space before the { it doesn't write
		}
		p.indent++
		p.lines(true, lbrace, nodes, dangling, rbrace, "", sep)
		p.indent--
		p.pending = lbrace
		return
	}
	p.write("{")
	if comment != nil {
		p.write(" ")
		p.comment(comment)
	}
	p.indent++
	p.lines(true, lbrace, nodes, dangling, rbrace, "", sep)
	p.indent--
	p.newline()
	p.write("}")
}

// canIndent reports whether the block at lbrace, about to be written on
// lines of its own, is written by indentation: in indented mode, outside
// ( ) and [ ], not after =>, not empty, no comment after its {, its first
// line not a .name outside a match, and not found to keep its braces.
func (p *printer) canIndent(lbrace Pos, comment *Comment, nodes []Node) bool {
	if !p.indented || p.depth > 0 || comment != nil || len(nodes) == 0 || p.keep[lbrace] {
		return false
	}
	line := p.out[bytes.LastIndexByte(p.out, '\n')+1:]
	if bytes.HasSuffix(bytes.TrimRight(line, " "), []byte("=>")) {
		return false
	}
	if leadsWithImplicit(nodes[0]) && !bytes.Contains(line, []byte("match ")) {
		return false
	}
	return true
}

// leadsWithImplicit reports whether n's source starts with .name.
func leadsWithImplicit(n Node) bool {
	for {
		switch x := n.(type) {
		case *ExprStmt:
			n = x.X
		case *CallExpr:
			n = x.Fun
		case *MemberExpr:
			n = x.X
		case *IndexExpr:
			n = x.X
		case *UnwrapExpr:
			n = x.X
		case *ImplicitMember:
			return true
		default:
			return false
		}
	}
}

func (p *printer) block(b *Block) {
	if b == nil {
		p.write("{}")
		return
	}
	items := styleUses(b.Items)
	p.body(b.Lbrace, b.Comment, itemNodes(items), b.Dangling, b.Rbrace, true, "; ")
}

// styleUses rewrites the form style: replaced, style name, as style:
// name, and a run of them as one list (style a; style b is style: [a,
// b]). A use with comments of its own starts another run.
func styleUses(items []Item) []Item {
	has := false
	for _, it := range items {
		if _, ok := it.(*StyleUse); ok {
			has = true
		}
	}
	if !has {
		return items
	}
	var out []Item
	for i := 0; i < len(items); i++ {
		u, ok := items[i].(*StyleUse)
		if !ok {
			out = append(out, items[i])
			continue
		}
		run := []Expr{u.Name}
		j := i + 1
		for ; j < len(items); j++ {
			v, ok := items[j].(*StyleUse)
			if !ok || v.Trivia.HasComments() {
				break
			}
			run = append(run, v.Name)
		}
		var value Expr = u.Name
		end := items[j-1].(*StyleUse).End
		if len(run) > 1 {
			value = &ArrayExpr{Span: Span{run[0].Range().Start, run[len(run)-1].Range().End}, Elems: run}
		}
		out = append(out, &PropSet{Span: Span{u.Start, end}, Trivia: u.Trivia, Name: &Ident{Span: Span{u.Start, u.Start + 5}, Name: "style"}, Value: value})
		i = j - 1
	}
	return out
}

func itemNodes(items []Item) []Node {
	nodes := make([]Node, len(items))
	for i, it := range items {
		nodes[i] = it
	}
	return nodes
}

func exprNodes(xs []Expr) []Node {
	nodes := make([]Node, len(xs))
	for i, x := range xs {
		nodes[i] = x
	}
	return nodes
}

func typeNodes(ts []Type) []Node {
	nodes := make([]Node, len(ts))
	for i, t := range ts {
		nodes[i] = t
	}
	return nodes
}

func patternNodes(ps []Pattern) []Node {
	nodes := make([]Node, len(ps))
	for i, pt := range ps {
		nodes[i] = pt
	}
	return nodes
}

// An elem is a list's element that's no node of its own (a case
// pattern's labeled argument): its span, and how it prints.
type elem struct {
	span  Span
	print func()
}

func (e *elem) Range() Span { return e.span }

// ---------------------------------------------------------------------
// Files and declarations

func (p *printer) sourceFile(f *SourceFile) {
	var nodes []Node
	for _, d := range f.Imports {
		nodes = append(nodes, d)
	}
	for _, d := range f.Decls {
		nodes = append(nodes, d)
	}
	p.lines(false, NoPos, nodes, f.Dangling, NoPos, "", "; ")
	// Anything left (there shouldn't be): at the end, never lost.
	for _, c := range p.comments {
		if !p.printed[c] {
			p.newline()
			p.comment(c)
		}
	}
	if len(p.out) > 0 {
		p.newline()
	}
}

func (p *printer) pub(pos Pos) {
	if pos.IsValid() {
		p.write("pub ")
	}
}

func (p *printer) private(pos Pos) {
	if pos.IsValid() {
		p.write("private ")
	}
}

func (p *printer) typeParams(list []*TypeParam, a Angles) {
	if len(list) == 0 {
		return
	}
	nodes := make([]Node, len(list))
	for i, tp := range list {
		nodes[i] = tp
	}
	p.list("<", ">", a.Lt, a.Gt, nodes)
}

func (p *printer) typeParam(tp *TypeParam) {
	p.write(tp.Name.Name)
	for j, t := range tp.Constraints {
		if j == 0 {
			p.write(": ")
		} else {
			p.write(" & ")
		}
		p.typ(t)
	}
}

func (p *printer) params(l *ParamList) {
	if l == nil {
		return
	}
	nodes := make([]Node, len(l.List))
	for i, x := range l.List {
		nodes[i] = x
	}
	p.list("(", ")", l.Start, l.End-1, nodes)
}

func (p *printer) conforms(list []Type) {
	for i, t := range list {
		if i == 0 {
			p.write(": ")
		} else {
			p.write(", ")
		}
		p.typ(t)
	}
}

func (p *printer) funcDecl(d *FuncDecl) {
	p.pub(d.Pub)
	p.private(d.Private)
	if d.Static.IsValid() {
		p.write("static ")
	}
	if d.Native.IsValid() {
		if d.NativePrefix != nil {
			p.write("native(" + d.NativePrefix.Name + ") ")
		} else {
			p.write("native ")
		}
	}
	if d.Extern.IsValid() {
		p.write("extern ")
	}
	if d.Async.IsValid() {
		p.write("async ")
	}
	if d.Mut.IsValid() {
		p.write("mut ")
	}
	p.write("fn " + d.Name.Name)
	p.typeParams(d.TypeParams, d.TypeAngles)
	if d.Params != nil {
		p.params(d.Params)
	} else {
		p.write("()")
	}
	if d.Throws.IsValid() {
		p.write(" throws")
	}
	if d.Result != nil {
		p.write(" -> ")
		p.typ(d.Result)
	}
	switch {
	case d.Body != nil:
		p.write(" ")
		p.block(d.Body)
	case d.Value != nil:
		after := d.Params.End
		if d.Throws.IsValid() {
			after = d.Throws + Pos(len("throws"))
		}
		if d.Result != nil {
			after = d.Result.Range().End
		}
		p.value(" =", after, d.Value)
	}
}

// value prints op and x after it, on op's line or, where the source
// broke the line after op, on the next one, indented once more. after
// is where what's before x ends: op is on its line.
func (p *printer) value(op string, after Pos, x Expr) {
	p.write(op)
	if l := p.line(after - 1); l > 0 && p.line(x.Range().Start) > l {
		p.trailing(nil, &BadExpr{Span: Span{after, after}}, x.Range().Start)
		p.indent++
		p.newline()
		p.expr(x)
		p.indent--
		return
	}
	p.write(" ")
	p.expr(x)
}

// end is where n ends, NoPos for nil.
func end(n Node) Pos {
	if n == nil {
		return NoPos
	}
	return n.Range().End
}

// node prints any node.
func (p *printer) node(n Node) {
	switch n := n.(type) {
	case nil:
	case *SourceFile:
		p.sourceFile(n)
	case *ImportDecl:
		p.write("import ")
		if n.Up > 0 {
			p.write(strings.Repeat("../", n.Up))
		} else if n.Relative {
			p.write("./")
		}
		for i, s := range n.Path {
			if i > 0 {
				p.write("/")
			}
			p.write(s.Name)
		}
		if n.Names != nil {
			nodes := make([]Node, len(n.Names))
			for i, id := range n.Names {
				nodes[i] = id
			}
			p.list(".{", "}", n.Lbrace, n.Rbrace, nodes)
		}
		if n.Alias != nil {
			p.write(" as " + n.Alias.Name)
		}

	// Declarations.
	case *ComponentDecl:
		p.pub(n.Pub)
		if n.Native.IsValid() {
			p.write("native ")
		}
		p.write("component " + n.Name.Name)
		p.typeParams(n.TypeParams, n.TypeAngles)
		p.params(n.Params)
		p.write(" ")
		p.block(n.Body)
	case *ShaderDecl:
		p.pub(n.Pub)
		p.write("shader " + n.Name.Name)
		p.params(n.Params)
		p.write(" ")
		p.body(n.Lbrace, nil, n.Members, n.Dangling, n.Rbrace, false, "; ")
	case *ScreenDecl:
		p.pub(n.Pub)
		p.write("screen " + n.Name.Name)
		p.params(n.Params)
		if n.At != nil {
			p.write(" at ")
			p.expr(n.At)
		}
		p.write(" ")
		p.block(n.Body)
	case *StoreDecl:
		p.pub(n.Pub)
		p.write("store " + n.Name.Name)
		p.typeParams(n.TypeParams, n.TypeAngles)
		p.params(n.Params)
		p.write(" ")
		p.block(n.Body)
	case *WorkerDecl:
		p.pub(n.Pub)
		p.write("worker " + n.Name.Name)
		p.params(n.Params)
		p.write(" ")
		p.block(n.Body)
	case *StructDecl:
		p.pub(n.Pub)
		p.private(n.Private)
		if n.Extern.IsValid() {
			p.write("extern ")
		}
		p.write("struct " + n.Name.Name)
		p.typeParams(n.TypeParams, n.TypeAngles)
		p.conforms(n.Conforms)
		p.write(" ")
		p.body(n.Lbrace, nil, n.Members, n.Dangling, n.Rbrace, true, "; ")
	case *Field:
		p.pub(n.Pub)
		p.private(n.Private)
		p.write(n.Name.Name + ": ")
		p.typ(n.Type)
		if n.Default != nil {
			p.value(" =", end(n.Type), n.Default)
		}
	case *EnumDecl:
		p.pub(n.Pub)
		p.private(n.Private)
		p.write("enum " + n.Name.Name)
		p.typeParams(n.TypeParams, n.TypeAngles)
		p.conforms(n.Conforms)
		p.write(" ")
		var nodes []Node
		for _, c := range n.Cases {
			nodes = append(nodes, c)
		}
		nodes = append(nodes, n.Members...)
		p.body(n.Lbrace, nil, nodes, n.Dangling, n.Rbrace, len(n.Members) == 0, ", ")
	case *EnumCase:
		p.write(n.Name.Name)
		if n.Params != nil {
			nodes := make([]Node, len(n.Params.List))
			for i, x := range n.Params.List {
				nodes[i] = x
			}
			p.list("(", ")", n.Params.Start, n.Params.End-1, nodes)
		}
	case *CaseParam:
		if n.Label != nil {
			p.write(n.Label.Name + ": ")
		}
		p.typ(n.Type)
	case *ExtendDecl:
		p.pub(n.Pub)
		p.write("extend")
		p.typeParams(n.TypeParams, n.TypeAngles)
		p.write(" ")
		p.typ(n.Type)
		p.conforms(n.Conforms)
		p.write(" ")
		p.body(n.Lbrace, nil, n.Members, n.Dangling, n.Rbrace, true, "; ")
	case *InterfaceDecl:
		p.pub(n.Pub)
		p.private(n.Private)
		if n.Native.IsValid() {
			p.write("native ")
		}
		p.write("interface " + n.Name.Name)
		p.typeParams(n.TypeParams, n.TypeAngles)
		p.write(" ")
		p.body(n.Lbrace, nil, n.Members, n.Dangling, n.Rbrace, true, "; ")
	case *FuncDecl:
		p.funcDecl(n)
	case *ExternTypeDecl:
		p.pub(n.Pub)
		p.write("extern type " + n.Name.Name)
		if n.Release != nil {
			p.write(" with release " + n.Release.Name)
		}
	case *TypeAliasDecl:
		p.pub(n.Pub)
		p.private(n.Private)
		p.write("type " + n.Name.Name)
		p.typeParams(n.TypeParams, n.TypeAngles)
		p.write(" = ")
		p.typ(n.Type)
	case *StyleDecl:
		p.pub(n.Pub)
		p.write("style " + n.Name.Name)
		if n.Params != nil {
			p.params(n.Params)
		}
		p.write(" ")
		p.block(n.Body)
	case *StateDecl:
		p.pub(n.Pub)
		p.write("state " + n.Name.Name)
		after := n.Name.End
		if n.Lparen.IsValid() || len(n.Deps) > 0 {
			p.list("(", ")", n.Lparen, n.Rparen, exprNodes(n.Deps))
			after = n.Rparen + 1
		}
		if n.Type != nil {
			p.write(": ")
			p.typ(n.Type)
			after = end(n.Type)
		}
		if n.Value != nil {
			p.value(" =", after, n.Value)
		}
	case *LetDecl:
		p.pub(n.Pub)
		p.private(n.Private)
		p.write(n.Keyword.String() + " ")
		p.pattern(n.Pattern)
		after := end(n.Pattern)
		if n.Type != nil {
			p.write(": ")
			p.typ(n.Type)
			after = end(n.Type)
		}
		if n.Value != nil {
			p.value(" =", after, n.Value)
		}
		if n.Else != nil {
			p.write(" else ")
			p.block(n.Else)
		}
	case *AppDecl:
		p.pub(n.Pub)
		p.write("app ")
		p.block(n.Body)
	case *TestDecl:
		p.write("test ")
		p.expr(n.Name)
		p.write(" ")
		if n.Params != nil {
			p.params(n.Params)
			p.write(" ")
		}
		p.block(n.Body)
	case *BenchDecl:
		p.write("bench ")
		p.expr(n.Name)
		p.write(" ")
		p.block(n.Body)
	case *PreviewDecl:
		p.write("preview ")
		p.expr(n.Name)
		p.write(" ")
		if n.Settings != nil {
			p.args(n.Settings)
			p.write(" ")
		}
		p.block(n.Body)
	case *TypeParam:
		p.typeParam(n)
	case *Param:
		p.write(n.Name.Name)
		if n.Type != nil {
			p.write(": ")
			p.typ(n.Type)
		}
		if n.Default != nil {
			p.write(" = ")
			p.expr(n.Default)
		}
	case *ParamList:
		p.params(n)

	// Items.
	case *Block:
		p.block(n)
	case *PropDecl:
		p.pub(n.Pub)
		p.write("prop " + n.Name.Name)
		after := n.Name.End
		if n.Type != nil {
			p.write(": ")
			p.typ(n.Type)
			after = end(n.Type)
		}
		if n.Default != nil {
			p.value(" =", after, n.Default)
		}
	case *EventDecl:
		p.pub(n.Pub)
		p.write("event " + n.Name.Name)
		p.params(n.Params)
	case *VariantDecl:
		p.pub(n.Pub)
		p.write("variant " + n.Name.Name)
		p.value(" =", n.Name.End, n.Value)
	case *UseDecl:
		p.write("use " + n.Name.Name + ": ")
		p.typ(n.Type)
	case *ProvideDecl:
		p.write("provide ")
		p.expr(n.Value)
	case *TaskDecl:
		p.write("task")
		if n.Lparen.IsValid() || len(n.Deps) > 0 {
			p.list("(", ")", n.Lparen, n.Rparen, exprNodes(n.Deps))
		}
		p.write(" ")
		p.block(n.Body)
	case *EffectDecl:
		p.write("effect")
		if n.Lparen.IsValid() || len(n.Deps) > 0 {
			p.list("(", ")", n.Lparen, n.Rparen, exprNodes(n.Deps))
		}
		p.write(" ")
		p.block(n.Body)
	case *PropSet:
		p.write(n.Name.Name)
		p.value(":", n.Name.End, n.Value)
	case *OnHandler:
		p.onHandler(n)
	case *BindItem:
		p.write("bind " + n.Name.Name + ": ")
		p.expr(n.Target)
	case *StyleUse:
		p.write("style: ")
		p.expr(n.Name)
	case *SlotItem:
		p.write(n.Name.Name + " ")
		p.block(n.Body)
	case *IfStmt:
		p.write("if ")
		p.cond(n.Cond)
		p.write(" ")
		p.block(n.Then)
		if n.Else != nil {
			p.write(" else ")
			p.node(n.Else)
		}
	case *ForStmt:
		p.write("for ")
		if n.Await.IsValid() {
			p.write("await ")
		}
		p.pattern(n.Pattern)
		p.write(" in ")
		p.expr(n.Seq)
		if n.Key != nil {
			p.write(" key ")
			p.expr(n.Key)
		}
		if n.Label != nil {
			p.write(" as " + n.Label.Name)
		}
		p.write(" ")
		p.block(n.Body)
	case *WhileStmt:
		p.write("while ")
		p.cond(n.Cond)
		if n.Label != nil {
			p.write(" as " + n.Label.Name)
		}
		p.write(" ")
		p.block(n.Body)
	case *MatchStmt:
		p.write("match ")
		p.expr(n.Subject)
		p.write(" ")
		p.arms(n.Lbrace, n.Arms, n.Rbrace)
	case *MatchArm:
		p.pattern(n.Pattern)
		if n.Guard != nil {
			p.write(" if ")
			p.expr(n.Guard)
		}
		if x, ok := n.Body.(Expr); ok {
			after := end(n.Pattern)
			if n.Guard != nil {
				after = end(n.Guard)
			}
			p.value(" =>", after, x)
		} else {
			p.write(" => ")
			p.node(n.Body)
		}
	case *MeasureStmt:
		p.write("measure ")
		p.block(n.Body)
	case *DoStmt:
		p.write("do ")
		p.block(n.Body)
		for _, c := range n.Catches {
			p.write(" ")
			p.node(c)
		}
	case *CatchClause:
		p.write("catch ")
		if n.Pattern != nil {
			p.pattern(n.Pattern)
			if n.Type != nil {
				p.write(": ")
				p.typ(n.Type)
			}
			p.write(" ")
		}
		p.block(n.Body)
	case *ReturnStmt:
		p.write("return")
		if n.Value != nil {
			p.write(" ")
			p.expr(n.Value)
		}
	case *ThrowStmt:
		p.write("throw ")
		p.expr(n.Value)
	case *BranchStmt:
		p.write(n.Tok.String())
		if n.Label != nil {
			p.write(" " + n.Label.Name)
		}
	case *DeferStmt:
		p.write("defer ")
		p.block(n.Body)
	case *CleanupStmt:
		p.write("cleanup ")
		p.block(n.Body)
	case *EmitStmt:
		p.write("emit " + n.Name.Name)
		if n.Args != nil {
			p.args(n.Args)
		}
	case *AssignStmt:
		p.expr(n.Target)
		p.value(" "+n.Op.String(), n.OpPos+Pos(len(n.Op.String())), n.Value)
	case *ExprStmt:
		p.expr(n.X)
	case *BadDecl, *BadItem:
		p.write("/* bad syntax */")

	case *Arg:
		if n.Label != nil {
			p.write(n.Label.Name + ": ")
		}
		p.expr(n.Value)
	case *ArgList:
		p.args(n)
	case *MapEntry:
		p.expr(n.Key)
		p.write(": ")
		p.expr(n.Value)
	case *CaseParams:
		nodes := make([]Node, len(n.List))
		for i, x := range n.List {
			nodes[i] = x
		}
		p.list("(", ")", n.Start, n.End-1, nodes)

	case *elem:
		n.print()
	case Expr:
		p.expr(n)
	case Type:
		p.typ(n)
	case Pattern:
		p.pattern(n)
	}
}

func (p *printer) onHandler(n *OnHandler) {
	p.write("on " + n.Name.Name)
	if n.Lparen.IsValid() || n.Params != nil {
		nodes := make([]Node, len(n.Params))
		for i, id := range n.Params {
			nodes[i] = id
		}
		p.list("(", ")", n.Lparen, n.Rparen, nodes)
	}
	p.write(" ")
	p.block(n.Body)
}

// cond prints an if's or a while's condition.
func (p *printer) cond(x Expr) {
	if l, ok := x.(*CondList); ok {
		broke := false
		for i, c := range l.List {
			p.sep(exprNodes(l.List), i, &broke)
			p.cond(c)
		}
		if broke {
			p.indent--
		}
		return
	}
	if c, ok := x.(*LetCond); ok {
		if c.Var {
			p.write("var ")
		} else {
			p.write("let ")
		}
		p.pattern(c.Pattern)
		p.write(" = ")
		p.expr(c.Value)
		return
	}
	p.expr(x)
}

// arms prints a match's { arms }: on one line, separated by commas,
// when they were written so.
func (p *printer) arms(lbrace Pos, arms []*MatchArm, rbrace Pos) {
	nodes := make([]Node, len(arms))
	oneLine := true
	for i, a := range arms {
		nodes[i] = a
		if b, ok := a.Body.(*Block); ok && !p.sameLine(b.Lbrace, b.Rbrace) {
			oneLine = false
		}
	}
	p.body(lbrace, nil, nodes, nil, rbrace, oneLine, ", ")
}

func (p *printer) args(l *ArgList) {
	nodes := make([]Node, len(l.List))
	for i, a := range l.List {
		nodes[i] = a
	}
	p.list("(", ")", l.Start, l.End-1, nodes)
}

// ---------------------------------------------------------------------
// Expressions

// Operands' precedences: what binds tighter than every binary operator.
const (
	precLambda  = 0
	precUnary   = PrecUnary
	precPostfix = PrecPow + 1
)

func exprPrec(x Expr) int {
	switch x := x.(type) {
	case *LambdaExpr:
		return precLambda
	case *BinaryExpr:
		return x.Op.Precedence()
	case *UnaryExpr, *SpawnExpr, *JumpExpr:
		// A jump takes the rest of the expression, so the parser makes
		// one an operand only on the right, where it needs no
		// parentheses.
		return precUnary
	case *CatchExpr:
		return precLambda
	}
	return precPostfix
}

func (p *printer) expr(x Expr) { p.exprPrec(x, precLambda) }

// exprPrec prints x where an operand of precedence prec or tighter
// goes, in parentheses when x binds looser.
func (p *printer) exprPrec(x Expr, prec int) {
	if exprPrec(x) < prec {
		p.write("(")
		p.expr1(x)
		p.write(")")
		return
	}
	p.expr1(x)
}

func (p *printer) expr1(x Expr) {
	switch x := x.(type) {
	case nil:
	case *Ident:
		p.write(x.Name)
	case *BasicLit:
		p.write(x.Value)
	case *StringLit:
		p.stringLit(x)
	case *BoolLit:
		if x.Value {
			p.write("true")
		} else {
			p.write("false")
		}
	case *NoneLit:
		p.write("none")
	case *SelfExpr:
		p.write("self")
	case *AtExpr:
		p.write("@" + x.Name)
	case *ImplicitMember:
		p.write("." + x.Name.Name)
	case *MemberExpr, *CallExpr, *IndexExpr, *UnwrapExpr, *TypeApp:
		p.postfix(x)
	case *UnaryExpr:
		switch x.Op {
		case NOT, SUB, TILDE:
			p.write(x.Op.String())
			if u, ok := x.X.(*UnaryExpr); ok && x.Op == SUB && u.Op == SUB {
				p.write(" ") // - -a, not --a
			}
		case TRY:
			if x.Optional {
				p.write("try? ")
			} else {
				p.write("try ")
			}
		default:
			p.write(x.Op.String() + " ")
		}
		if x.Op == LOAD || x.Op == RELOAD {
			p.exprPrec(x.X, precPostfix)
		} else {
			p.exprPrec(x.X, precUnary)
		}
	case *SpawnExpr:
		p.write("spawn ")
		p.block(x.Body)
	case *BinaryExpr:
		cont := false
		p.binary(x, &cont)
		if cont {
			p.indent--
		}
	case *LambdaExpr:
		if x.Async.IsValid() {
			p.write("async ")
		}
		if !x.Parens && len(x.Params) == 1 && x.Params[0].Type == nil && x.Params[0].Default == nil {
			p.write(x.Params[0].Name.Name)
		} else {
			nodes := make([]Node, len(x.Params))
			for i, prm := range x.Params {
				nodes[i] = prm
			}
			p.list("(", ")", x.Lparen, x.Rparen, nodes)
		}
		switch b := x.Body.(type) {
		case *Block:
			p.write(" => ")
			p.block(b)
		case Expr:
			after := x.Rparen + 1
			if !x.Parens && len(x.Params) > 0 {
				after = x.Params[len(x.Params)-1].End
			}
			p.value(" =>", after, b)
		}
	case *ParenExpr:
		p.write("(")
		p.expr(x.X)
		p.write(")")
	case *TupleExpr:
		if len(x.Elems) > 0 && p.multiline(x.Start, x.End-1, exprNodes(x.Elems)) {
			p.list("(", ")", x.Start, x.End-1, exprNodes(x.Elems))
			return
		}
		p.write("(")
		broke := false
		for i, e := range x.Elems {
			p.sep(exprNodes(x.Elems), i, &broke)
			p.expr(e)
		}
		if len(x.Elems) == 1 {
			p.write(",")
		}
		if broke {
			p.indent--
		}
		p.write(")")
	case *ArrayExpr:
		p.list("[", "]", x.Start, x.End-1, exprNodes(x.Elems))
	case *MapExpr:
		if len(x.Entries) == 0 {
			// A comment inside goes above the line: [ // … :] can't
			// hold one on lines of its own as an array can.
			p.write("[:]")
			return
		}
		nodes := make([]Node, len(x.Entries))
		for i, e := range x.Entries {
			nodes[i] = e
		}
		p.list("[", "]", x.Start, x.End-1, nodes)
	case *IfExpr:
		p.write("if ")
		p.cond(x.Cond)
		p.write(" ")
		p.block(x.Then)
		if x.Else != nil {
			p.write(" else ")
			switch e := x.Else.(type) {
			case *Block:
				p.block(e)
			case Expr:
				p.expr(e)
			}
		}
	case *MatchExpr:
		p.write("match ")
		p.expr(x.Subject)
		p.write(" ")
		p.arms(x.Lbrace, x.Arms, x.Rbrace)
	case *LetCond, *CondList:
		p.cond(x)
	case *JumpExpr:
		p.write(x.Tok.String())
		if x.Value != nil {
			p.write(" ")
			p.expr(x.Value)
		}
		if x.Label != nil {
			p.write(" " + x.Label.Name)
		}
	case *CatchExpr:
		p.expr(x.X)
		for _, c := range x.Catches {
			p.write(" ")
			p.node(c)
		}
	case *BadExpr:
		p.write("/* bad syntax */")
	}
}

// startsWithDot reports whether x is printed starting with a . (an
// implicit member, .end, or what applies to one).
func startsWithDot(x Expr) bool {
	for {
		switch e := x.(type) {
		case *ImplicitMember:
			return true
		case *MemberExpr:
			x = e.X
		case *CallExpr:
			x = e.Fun
		case *IndexExpr:
			x = e.X
		case *UnwrapExpr:
			x = e.X
		case *BinaryExpr:
			x = e.X
		case *CatchExpr:
			x = e.X
		default:
			return false
		}
	}
}

// postfixOperand is what a member, a call, an index or an unwrap applies
// to, or nil when x is none of them.
func postfixOperand(x Expr) Expr {
	switch e := x.(type) {
	case *MemberExpr:
		return e.X
	case *TypeApp:
		return e.X
	case *CallExpr:
		return e.Fun
	case *IndexExpr:
		return e.X
	case *UnwrapExpr:
		return e.X
	}
	return nil
}

// postfix prints a chain of members, calls, indexes and unwraps from its
// first operand on. A line break the source had before a . or a ?. stays
// (a line that starts with .name continues the one before), the chain's
// next lines indented once more.
func (p *printer) postfix(x Expr) {
	var chain []Expr
	for e := x; e != nil; e = postfixOperand(e) {
		chain = append(chain, e)
	}
	base := chain[len(chain)-1]
	chain = chain[:len(chain)-1]
	p.exprPrec(base, precPostfix)
	// A % length before (, [ or . keeps a space: 50%(…) would be a
	// remainder.
	if l, ok := base.(*BasicLit); ok && l.Kind == LENGTH && strings.HasSuffix(l.Value, "%") {
		switch e := chain[len(chain)-1].(type) {
		case *MemberExpr:
			if !e.Optional {
				p.write(" ")
			}
		case *CallExpr:
			if len(e.TypeArgs) == 0 && e.Args != nil {
				p.write(" ")
			}
		case *IndexExpr:
			p.write(" ")
		}
	}
	cont := false
	for i := len(chain) - 1; i >= 0; i-- {
		switch e := chain[i].(type) {
		case *MemberExpr:
			dot, op := e.Dot, "."
			if e.Optional {
				op = "?."
			}
			if !dot.IsValid() { // a tree made in code
				dot = e.Name.Start - Pos(len(op))
			}
			// A break before the dot stays before it (.name continues the
			// line before); one after it stays after it, as gofmt keeps
			// x.⏎Name: a line that ends in . always goes on, where one
			// that starts with .name may be a block's value, or a tuple's
			// .0 a number.
			l := p.endLine(e.X)
			before := l > 0 && p.line(dot) > l
			after := !before && p.line(e.Name.Start) > p.line(dot)
			if (before || after) && !cont {
				cont = true
				p.indent++
			}
			if before {
				p.trailing(nil, e.X, dot)
				p.newline()
			}
			p.write(op)
			if after {
				p.newline()
			}
			p.write(e.Name.Name)
		case *TypeApp:
			p.list("<", ">", e.Angles.Lt, e.Angles.Gt, typeNodes(e.TypeArgs))
		case *CallExpr:
			if len(e.TypeArgs) > 0 {
				p.list("<", ">", e.Angles.Lt, e.Angles.Gt, typeNodes(e.TypeArgs))
			}
			if e.Args != nil {
				p.args(e.Args)
			}
			if e.As != nil {
				p.write(" as " + e.As.Name)
			}
			if e.Block != nil {
				p.write(" ")
				p.block(e.Block)
			}
			for _, h := range e.Handlers {
				p.write(" ")
				p.onHandler(h)
			}
		case *IndexExpr:
			p.write("[")
			p.expr(e.Index)
			if e.Default != nil {
				p.write(", default: ")
				p.expr(e.Default)
			}
			p.write("]")
		case *UnwrapExpr:
			p.write("!")
		}
	}
	if cont {
		p.indent--
	}
}

// binary prints x op y, keeping a line break before or after op where
// the source had one (a line that starts with a binary operator
// continues the one before); cont says the chain's next lines are
// indented already.
func (p *printer) binary(b *BinaryExpr, cont *bool) {
	prec := b.Op.Precedence()
	lp, rp := prec, prec+1
	switch {
	case b.Op == COALESCE: // right-associative
		lp, rp = prec+1, prec
	case b.Op == POW: // right-associative, and a prefix operator on its right: 2 ** -1
		lp, rp = prec+1, precUnary
	case prec == 4 || prec == 5: // comparisons and ranges don't chain
		lp = prec + 1
	}
	p.operand(b.X, lp, cont)
	sp := " "
	if (b.Op == RANGE || b.Op == RANGELT) && !startsWithDot(b.Y) {
		sp = "" // but a .. .end: a...end isn't a range
	}
	l := p.endLine(b.X)
	switch {
	case l > 0 && p.line(b.OpPos) > l:
		if !*cont {
			*cont = true
			p.indent++
		}
		p.trailing(nil, b.X, b.OpPos)
		p.newline()
		p.write(b.Op.String() + sp)
	case l > 0 && l < p.line(b.Y.Range().Start):
		p.write(sp + b.Op.String())
		if !*cont {
			*cont = true
			p.indent++
		}
		p.trailing(nil, &BadExpr{Span: Span{b.OpPos, b.OpPos + Pos(len(b.Op.String()))}}, b.Y.Range().Start)
		p.newline()
	default:
		p.write(sp + b.Op.String() + sp)
	}
	p.operand(b.Y, rp, cont)
}

func (p *printer) operand(x Expr, prec int, cont *bool) {
	if b, ok := x.(*BinaryExpr); ok && b.Op.Precedence() >= prec {
		p.binary(b, cont)
		return
	}
	p.exprPrec(x, prec)
}

func (p *printer) stringLit(s *StringLit) {
	q := `"`
	if s.Multiline {
		q = `"""`
	}
	hashes := ""
	if s.Raw {
		hashes = strings.Repeat("#", s.Hashes)
		p.write("r" + hashes)
	}
	p.write(q)
	for i, seg := range s.Segments {
		if i > 0 {
			p.write("{")
			p.expr(s.Interps[i-1])
			if i-1 < len(s.Specs) && s.Specs[i-1] != nil {
				p.write(":" + s.Specs[i-1].Text)
			}
			p.write("}")
		}
		p.out = append(p.out, seg...)
	}
	p.write(q + hashes)
}

// ---------------------------------------------------------------------
// Types and patterns

func (p *printer) typ(t Type) {
	switch t := t.(type) {
	case *NamedType:
		for i, id := range t.Path {
			if i > 0 {
				p.write(".")
			}
			p.write(id.Name)
		}
		if len(t.Args) > 0 {
			p.list("<", ">", t.Angles.Lt, t.Angles.Gt, typeNodes(t.Args))
		}
	case *ArrayType:
		p.write("[")
		p.typ(t.Elem)
		p.write("]")
	case *MapType:
		p.write("[")
		p.typ(t.Key)
		p.write(": ")
		p.typ(t.Value)
		p.write("]")
	case *TupleType:
		if len(t.Elems) > 0 && p.multiline(t.Start, t.End-1, typeNodes(t.Elems)) {
			p.list("(", ")", t.Start, t.End-1, typeNodes(t.Elems))
			return
		}
		p.write("(")
		broke := false
		for i, e := range t.Elems {
			p.sep(typeNodes(t.Elems), i, &broke)
			p.typ(e)
		}
		if len(t.Elems) == 1 {
			p.write(",")
		}
		if broke {
			p.indent--
		}
		p.write(")")
	case *FuncType:
		if t.Async.IsValid() {
			p.write("async ")
		}
		p.write("fn")
		p.list("(", ")", t.Lparen, t.Rparen, typeNodes(t.Params))
		if t.Throws.IsValid() {
			p.write(" throws")
		}
		if t.Result != nil {
			p.write(" -> ")
			p.typ(t.Result)
		}
	case *OptionalType:
		if _, fn := t.Elem.(*FuncType); fn {
			p.write("(")
			p.typ(t.Elem)
			p.write(")")
		} else {
			p.typ(t.Elem)
		}
		p.write("?")
	case *BadType:
		p.write("/* bad syntax */")
	}
}

func (p *printer) pattern(pt Pattern) {
	switch pt := pt.(type) {
	case *WildcardPat:
		p.write("_")
	case *BindPat:
		p.write(pt.Name.Name)
	case *TypedPat:
		p.pattern(pt.Pat)
		p.write(": ")
		p.typ(pt.Type)
	case *LitPat:
		p.expr(pt.Value)
	case *CasePat:
		if pt.Implicit {
			p.write(".")
		}
		for i, id := range pt.Path {
			if i > 0 {
				p.write(".")
			}
			p.write(id.Name)
		}
		if pt.Lparen.IsValid() || pt.Args != nil {
			nodes := make([]Node, len(pt.Args))
			for i, a := range pt.Args {
				nodes[i] = a
				if l := pt.Label(i); l != nil {
					nodes[i] = &elem{span: Span{l.Start, a.Range().End}, print: func() {
						p.write(l.Name + ": ")
						p.pattern(a)
					}}
				}
			}
			p.list("(", ")", pt.Lparen, pt.Rparen, nodes)
		}
	case *TuplePat:
		if len(pt.Elems) > 0 && p.multiline(pt.Start, pt.End-1, patternNodes(pt.Elems)) {
			p.list("(", ")", pt.Start, pt.End-1, patternNodes(pt.Elems))
			return
		}
		p.write("(")
		broke := false
		for i, e := range pt.Elems {
			p.sep(patternNodes(pt.Elems), i, &broke)
			p.pattern(e)
		}
		if len(pt.Elems) == 1 {
			p.write(",")
		}
		if broke {
			p.indent--
		}
		p.write(")")
	case *OrPat:
		for i, a := range pt.Alts {
			if i > 0 {
				p.write(" | ")
			}
			p.pattern(a)
		}
	case *BadPat:
		p.write("/* bad syntax */")
	}
}
