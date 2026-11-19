package syntax

import (
	"fmt"
	"strconv"
	"strings"
	"unicode"
	"unicode/utf8"

	"github.com/egoist/asuka/lang/diag"
)

// An ErrorHandler is told about each error the scanner finds. The
// scanner keeps going after one.
type ErrorHandler func(pos Pos, code *diag.Code, msg string)

// A Comment is a // line comment (/// for documentation) or a /* block
// */ comment, with its delimiters.
type Comment struct {
	Pos  Pos
	Text string
}

// End is the position just after the comment.
func (c *Comment) End() Pos { return c.Pos + Pos(len(c.Text)) }

// IsDoc reports whether the comment is a /// documentation comment.
func (c *Comment) IsDoc() bool {
	return len(c.Text) >= 3 && c.Text[:3] == "///" && (len(c.Text) == 3 || c.Text[3] != '/')
}

// A Scanner turns a file's source into tokens. Comments don't come out
// of Scan; they're kept, in order, for Comments.
//
// Newlines are significant: Scan returns a NEWLINE where a line ends an
// item, a member or a statement. It doesn't when the line can't end
// there, as Go and Swift do:
//
//   - inside ( ), [ ] and an import's .{ }; inside { } newlines are
//     significant again;
//   - inside a """ string's {interpolation}; a one-line string's
//     interpolation ends with its line (the string isn't terminated);
//   - after an operator that needs an operand on its right (binary
//     operators, =, the assignment operators, =>, ->, ., ?., .., ..<, |),
//     after a , or a :, and after an opening bracket;
//   - after another newline, a ; or the start of the file: runs of
//     newlines are one;
//   - before a line (blank lines and comments aside) that starts with a
//     binary operator that can't start an expression: + * / % == != <
//     <= > >= && || ?? & | ^ << >> .. ..< and ?. (a ⏎ ?? b). Not -, !
//     or ~, which can (a ⏎ -b is two lines), and not an assignment's
//     operator (+=). A line that starts with .name is the parser's to
//     join (decision 26): it may be an implicit member of its own.
//
// Not after ? (a line may end with an optional type, String?), ! (one
// may end with a force unwrap, x!) or > (one may end with a generic
// type, Loading<User>): a comparison broken across lines puts its > at
// the start of the next line instead.
//
// Blocks may be written by indentation (rfcs/0018). Where a line ends a
// statement (Scan would return a NEWLINE) or ends with else, and the
// next line that isn't blank or only a comment is indented deeper than
// the statement's first line, Scan returns the { a brace would have
// opened instead; where the indentation comes back, it returns the
// NEWLINE, then a } and a NEWLINE for each block it closes, as braces
// would have. A deeper line that starts with .name continues the line
// before (decision 26), except after a statement that holds a match,
// whose arms it opens. Inside ( ), [ ] and an explicit { } block's
// first line nothing changes; an explicit { at a line's end opens its
// block as before, and a block of either kind may hold the other.
type Scanner struct {
	file *File
	src  string
	err  ErrorHandler

	ch       rune // the current character, -1 at the end
	offset   int  // ch's offset
	rdOffset int  // the offset after ch

	last     Token // the last token returned, for newlines
	frames   []frame
	comments []*Comment

	// Blocks by indentation: the tokens queued to return before
	// scanning on, the indentation of the current statement's first
	// line, its first token, and whether the next token starts one.
	queue      []queued
	stmtIndent int
	stmtFirst  bool // the statement's first token is its line's first
	stmtMatch  bool // the statement holds a match (its arms may start .name)
	headTok    Token
	startStmt  bool
	stmts      []stmt // the statements a { left, back at its }

	ErrorCount int
}

type frameKind uint8

const (
	paren    frameKind = iota // ( or [: newlines don't count
	brace                     // {: newlines count
	dotbrace                  // an import's .{: newlines don't count
	interp                    // a string's {…}: newlines don't count
)

type frame struct {
	kind      frameKind
	multiline bool // interp: in a """ string
	start     int  // interp: the string's first byte
	indented  bool // brace: opened by indentation, not a {
	indent    int  // indented: its lines' indentation
	outer     int  // indented: the indentation of the line that opened it
}

// A statement's state, kept while a { inside it is open (a one-line
// block in a call: Button(if x { "a" } else { "b" })).
type stmt struct {
	indent        int
	first, match_ bool
	head          Token
}

// A token Scan returns without scanning: an indented block's { or }, or
// the NEWLINE that goes with it.
type queued struct {
	pos Pos
	tok Token
}

const eof = -1

// Init prepares s to scan src, the contents of file. err, if not nil, is
// told about each error.
func (s *Scanner) Init(file *File, src []byte, err ErrorHandler) {
	if file.Size() != len(src) {
		panic("syntax: file size doesn't match the source's length")
	}
	s.file = file
	s.src = string(src)
	s.err = err
	s.ch = ' '
	s.offset, s.rdOffset = 0, 0
	s.last = NEWLINE // so a file's leading newlines don't count
	s.frames = s.frames[:0]
	s.comments = nil
	s.ErrorCount = 0
	s.queue = s.queue[:0]
	s.stmts = s.stmts[:0]
	s.stmtIndent, s.headTok, s.startStmt = 0, ILLEGAL, true
	s.next()
	if s.ch == '\uFEFF' {
		s.next() // a byte order mark
	}
}

// Comments are the comments scanned so far, in source order.
func (s *Scanner) Comments() []*Comment { return s.comments }

func (s *Scanner) error(offset int, code *diag.Code, msg string) {
	if s.err != nil {
		s.err(s.file.Pos(offset), code, msg)
	}
	s.ErrorCount++
}

// next reads the next character into s.ch.
func (s *Scanner) next() {
	if s.rdOffset >= len(s.src) {
		s.offset = len(s.src)
		if s.ch == '\n' {
			s.file.AddLine(s.offset)
		}
		s.ch = eof
		return
	}
	s.offset = s.rdOffset
	if s.ch == '\n' {
		s.file.AddLine(s.offset)
	}
	r, w := rune(s.src[s.rdOffset]), 1
	if r >= utf8.RuneSelf {
		r, w = utf8.DecodeRuneInString(s.src[s.rdOffset:])
		if r == utf8.RuneError && w == 1 {
			s.error(s.offset, diag.InvalidCharacter, "invalid UTF-8")
		} else if r == '\uFEFF' && s.offset > 0 {
			s.error(s.offset, diag.InvalidCharacter, "a byte order mark in the middle of the file")
		}
	}
	s.rdOffset += w
	s.ch = r
}

func (s *Scanner) peek() byte {
	if s.rdOffset < len(s.src) {
		return s.src[s.rdOffset]
	}
	return 0
}

func (s *Scanner) peek2() byte {
	if s.rdOffset+1 < len(s.src) {
		return s.src[s.rdOffset+1]
	}
	return 0
}

// newlineCounts reports whether a newline here ends something.
func (s *Scanner) newlineCounts() bool {
	if n := len(s.frames); n > 0 && s.frames[n-1].kind != brace {
		return false
	}
	switch s.last {
	case NEWLINE, SEMICOLON, LBRACE, LPAREN, LBRACK, DOTBRACE, COMMA, COLON,
		ADD, SUB, MUL, QUO, REM, AMP, CARET, TILDE, SHL, WADD, WSUB, WMUL, POW,
		ASSIGN, ADD_ASSIGN, SUB_ASSIGN, MUL_ASSIGN, QUO_ASSIGN, REM_ASSIGN, COALESCE_ASSIGN,
		AND_ASSIGN, OR_ASSIGN, XOR_ASSIGN, SHL_ASSIGN, WADD_ASSIGN, WSUB_ASSIGN, WMUL_ASSIGN, POW_ASSIGN,
		EQL, NEQ, LSS, LEQ, GEQ, LAND, LOR, COALESCE,
		OPTDOT, FATARROW, ARROW, RANGE, RANGELT, PERIOD, PIPE, ELSE:
		return false
	}
	return true
}

// continued reports whether the next line, after the source at from
// (blank lines and comments aside), starts with a binary operator that
// can't start an expression, which continues this line's: a ⏎ ?? b,
// items.count() ⏎ * 2. Not -, which can (a ⏎ -b is two lines), nor
// one's assignment form (a ⏎ += 1). A leading . is the parser's to
// decide (an implicit member may start a line).
func (s *Scanner) continued(from int) bool {
	src, i := s.src, from
	for i < len(src) {
		switch c := src[i]; {
		case c == ' ' || c == '\t' || c == '\r' || c == '\n':
			i++
			continue
		case c == '/' && i+1 < len(src) && src[i+1] == '/':
			for i < len(src) && src[i] != '\n' {
				i++
			}
			continue
		case c == '/' && i+1 < len(src) && src[i+1] == '*':
			end := strings.Index(src[i+2:], "*/")
			if end < 0 {
				return false
			}
			i += 2 + end + 2
			continue
		}
		break
	}
	at := func(k int) byte {
		if i+k < len(src) {
			return src[i+k]
		}
		return 0
	}
	switch at(0) {
	case '.':
		return at(1) == '.' // .. and ..<
	case '?':
		return at(1) == '.' || at(1) == '?' && at(2) != '=' // ?. and ??
	case '*':
		if at(1) == '*' {
			return at(2) != '=' // **
		}
		return at(1) != '='
	case '+', '/', '%', '^':
		return at(1) != '='
	case '&', '|':
		if at(0) == '&' && (at(1) == '+' || at(1) == '-' || at(1) == '*') {
			return at(2) != '=' // &+ &- &*
		}
		return at(1) == at(0) || at(1) != '=' // && ||, & |
	case '=', '!':
		return at(1) == '=' // == !=
	case '<':
		return !(at(1) == '<' && at(2) == '=') // < <= <<
	case '>':
		return !(at(1) == '>' && at(2) == '=') // > >= >>
	}
	return false
}

// endLineInterp ends, at a line's end, the interpolation of a one-line
// string it's in: the string wasn't terminated.
func (s *Scanner) endLineInterp() {
	if n := len(s.frames); n > 0 && s.frames[n-1].kind == interp && !s.frames[n-1].multiline {
		s.error(s.frames[n-1].start, diag.Unterminated, "string not terminated")
		s.frames = s.frames[:n-1]
	}
}

func (s *Scanner) push(k frameKind) { s.frames = append(s.frames, frame{kind: k}) }

// pop closes the innermost bracket of kind k, if that's what's open.
func (s *Scanner) pop(k frameKind) {
	if n := len(s.frames); n > 0 && s.frames[n-1].kind == k {
		s.frames = s.frames[:n-1]
	}
}

// Scan returns the next token: its position, its kind and its source
// text (for every token, so its end is pos+len(lit); "" at the end).
func (s *Scanner) Scan() (pos Pos, tok Token, lit string) {
	if len(s.queue) > 0 {
		q := s.queue[0]
		s.queue = s.queue[1:]
		pos, tok, lit = q.pos, q.tok, ""
	} else {
		pos, tok, lit = s.scan()
		if tok == EOF && s.closeIndented(pos) {
			return s.Scan()
		}
	}
	if s.startStmt && tok != NEWLINE && tok != EOF {
		off := s.file.Offset(pos)
		s.stmtIndent, s.headTok, s.startStmt, s.stmtMatch = s.lineIndent(off), tok, false, false
		s.stmtFirst = strings.TrimLeft(s.src[strings.LastIndexByte(s.src[:off], '\n')+1:off], " \t") == ""
	}
	switch tok {
	case MATCH:
		s.stmtMatch = true
	case LBRACE:
		s.stmts = append(s.stmts, stmt{s.stmtIndent, s.stmtFirst, s.stmtMatch, s.headTok})
		s.startStmt = true
	case RBRACE:
		if n := len(s.stmts); n > 0 {
			st := s.stmts[n-1]
			s.stmts = s.stmts[:n-1]
			s.stmtIndent, s.stmtFirst, s.stmtMatch, s.headTok, s.startStmt = st.indent, st.first, st.match_, st.head, false
		}
	case NEWLINE, SEMICOLON:
		s.startStmt = true
	}
	s.last = tok
	return
}

// lineIndent is the indentation, in columns, of the line holding offset.
func (s *Scanner) lineIndent(offset int) int {
	start := strings.LastIndexByte(s.src[:offset], '\n') + 1
	n := 0
	for i := start; i < len(s.src) && (s.src[i] == ' ' || s.src[i] == '\t'); i++ {
		n++
	}
	return n
}

// nextLine finds the next line after from that isn't blank or only a
// comment: its indentation, the offset of its first character, and
// whether there is one. A tab in that indentation is an error.
func (s *Scanner) nextLine(from int) (indent, first int, ok bool) {
	src, i := s.src, from
	for i < len(src) {
		start, n, tab := i, 0, -1
		for i < len(src) && (src[i] == ' ' || src[i] == '\t') {
			if src[i] == '\t' && tab < 0 {
				tab = i
			}
			n++
			i++
		}
		switch {
		case i >= len(src):
			return 0, i, false
		case src[i] == '\n' || src[i] == '\r':
			i++
			continue
		case src[i] == '/' && i+1 < len(src) && src[i+1] == '/':
			for i < len(src) && src[i] != '\n' {
				i++
			}
			continue
		}
		_ = start
		if tab >= 0 {
			s.error(tab, diag.Indentation, "a tab in a line's indentation: indent with spaces")
		}
		return n, i, true
	}
	return 0, i, false
}

// indentedTop reports whether the innermost frame is a block opened by
// indentation.
func (s *Scanner) indentedTop() bool {
	n := len(s.frames)
	return n > 0 && s.frames[n-1].kind == brace && s.frames[n-1].indented
}

// inBlock reports whether newlines count here for a block: at the top
// of the file or inside a { } of either kind.
func (s *Scanner) inBlock() bool {
	n := len(s.frames)
	return n == 0 || s.frames[n-1].kind == brace
}

// lineEnd decides, at the newline at off where a NEWLINE would end the
// line, what Scan returns: an indented block's {, or the NEWLINE and
// the } of each indented block the next line closes (queued after it).
func (s *Scanner) lineEnd(off int) (Pos, Token, string) {
	pos := s.file.Pos(off)
	indent, first, ok := s.nextLine(off + 1)
	if !ok {
		return pos, NEWLINE, "\n" // the end of the file closes them (Scan)
	}
	if s.stmtFirst && indent > s.stmtIndent && s.opens(first) {
		s.frames = append(s.frames, frame{kind: brace, indented: true, indent: indent, outer: s.stmtIndent})
		return pos, LBRACE, ""
	}
	s.dedent(pos, indent, first)
	return pos, NEWLINE, "\n"
}

// opens reports whether a deeper line starting at first opens a block:
// not one that starts with .name (it continues the line before), but
// after a statement that holds a match (it's the first arm: match x,
// let y = match x).
func (s *Scanner) opens(first int) bool {
	if s.src[first] == '.' && first+1 < len(s.src) && isLetter(rune(s.src[first+1])) {
		return s.stmtMatch
	}
	return true
}

// dedent queues a } and a NEWLINE for each indented block the next line,
// indented by indent, closes: those whose lines are deeper. A block's }
// goes after the comments at its indentation that end it, so they stay
// in it.
func (s *Scanner) dedent(pos Pos, indent, first int) {
	closed := -1
	from := s.file.Offset(pos)
	for s.indentedTop() && s.frames[len(s.frames)-1].indent > indent {
		f := s.frames[len(s.frames)-1]
		closed = f.outer
		s.frames = s.frames[:len(s.frames)-1]
		at := s.file.Pos(s.commentsEnd(from, first, f.indent))
		s.queue = append(s.queue, queued{at, RBRACE}, queued{at, NEWLINE})
	}
	if closed >= 0 && indent > closed && !(s.indentedTop() && s.frames[len(s.frames)-1].indent == indent) {
		s.error(first, diag.Indentation, fmt.Sprintf("this line is indented %d, which no open block is: line it up with the block it belongs to", indent))
	}
}

// commentsEnd is where the last comment-only line between from and to
// indented by at least min ends (its newline), or from if there's none.
func (s *Scanner) commentsEnd(from, to, min int) int {
	end, i := from, from
	for i < to {
		j := strings.IndexByte(s.src[i:to], '\n')
		if j < 0 {
			break
		}
		line := s.src[i : i+j]
		t := strings.TrimLeft(line, " \t")
		if strings.HasPrefix(t, "//") && len(line)-len(t) >= min {
			end = i + j
		}
		i += j + 1
	}
	return end
}

// closeIndented queues, at the end of the file, a } and a NEWLINE for
// each indented block still open, then the end itself; it reports
// whether there were any.
func (s *Scanner) closeIndented(pos Pos) bool {
	if !s.indentedTop() {
		return false
	}
	if s.last != NEWLINE {
		s.queue = append(s.queue, queued{pos, NEWLINE})
	}
	for s.indentedTop() {
		s.frames = s.frames[:len(s.frames)-1]
		s.queue = append(s.queue, queued{pos, RBRACE}, queued{pos, NEWLINE})
	}
	s.queue = append(s.queue, queued{pos, EOF})
	return true
}

func (s *Scanner) scan() (Pos, Token, string) {
redo:
	// Whitespace, newlines and comments.
	for {
		switch s.ch {
		case ' ', '\t', '\r':
			s.next()
			continue
		case '\n':
			s.endLineInterp()
			if s.newlineCounts() && !s.continued(s.offset+1) {
				off := s.offset
				s.next()
				return s.lineEnd(off)
			}
			if s.last == ELSE && s.inBlock() {
				// else at a line's end: a deeper line is its block
				if indent, first, ok := s.nextLine(s.offset + 1); ok && s.stmtFirst && indent > s.stmtIndent && s.opens(first) {
					off := s.offset
					s.next()
					s.frames = append(s.frames, frame{kind: brace, indented: true, indent: indent, outer: s.stmtIndent})
					return s.file.Pos(off), LBRACE, ""
				}
			}
			s.next()
			continue
		case '/':
			if c := s.peek(); c == '/' || c == '*' {
				if nl := s.comment(); nl >= 0 {
					// A block comment spanning lines ends a line.
					s.endLineInterp()
					if s.newlineCounts() && !s.continued(s.offset) {
						return s.file.Pos(nl), NEWLINE, "\n"
					}
				}
				continue
			}
		}
		break
	}

	start := s.offset
	pos := s.file.Pos(start)
	ch := s.ch
	switch {
	case ch == eof:
		// A string whose interpolation is open, maybe inside a bracket
		// left open in it ("{f(), isn't terminated.
		for n := len(s.frames); n > 0; n-- {
			if f := s.frames[n-1]; f.kind == interp {
				s.error(f.start, diag.Unterminated, "string not terminated")
				s.frames = s.frames[:0]
				break
			}
		}
		return pos, EOF, ""
	case isLetter(ch):
		if ch == 'r' {
			if n, ok := s.rawHashes(); ok {
				s.next() // past r
				for range n {
					s.next()
				}
				return s.string(start, true, n)
			}
		}
		s.ident()
		lit := s.src[start:s.offset]
		// After . or ?., a keyword is a member's name: x.type, .default.
		if s.last != PERIOD && s.last != OPTDOT {
			if t, ok := keywords[lit]; ok {
				return pos, t, lit
			}
		}
		return pos, IDENT, lit
	case isDigit(ch):
		if (s.last == PERIOD || s.last == OPTDOT) && !unitAfterDigits(s.src[start:]) {
			// A tuple's element, t.0: its index alone, so that t.0.1 is
			// t . 0 . 1, not t . 0.1 (as Rust reads it). (.5s, a number
			// without its 0, is scanned whole for the parser to say so.)
			s.digits(isDigit)
			return pos, INT, s.src[start:s.offset]
		}
		tok := s.number()
		return pos, tok, s.src[start:s.offset]
	case ch == '"':
		return s.string(start, false, 0)
	case ch == '\'':
		return s.char(start)
	}

	s.next() // past ch
	tok := ILLEGAL
	switch ch {
	case '#':
		tok = s.color(start)
	case '(':
		s.push(paren)
		tok = LPAREN
	case '[':
		s.push(paren)
		tok = LBRACK
	case '{':
		s.push(brace)
		tok = LBRACE
	case ')':
		s.pop(paren)
		tok = RPAREN
	case ']':
		s.pop(paren)
		tok = RBRACK
	case '}':
		if n := len(s.frames); n > 0 && s.frames[n-1].kind == interp {
			// The end of an interpolation: the string goes on.
			multiline, open := s.frames[n-1].multiline, s.frames[n-1].start
			s.frames = s.frames[:n-1]
			return s.stringRest(start, open, false, multiline, true, 0)
		}
		// A } closes its { and any ( or [ left open inside it, so an
		// unclosed bracket doesn't swallow the newlines of the rest of
		// the file; the parser reports it.
		for n := len(s.frames); n > 0; n-- {
			k := s.frames[n-1].kind
			if k == interp {
				break
			}
			s.frames = s.frames[:n-1]
			if k == brace || k == dotbrace {
				break
			}
		}
		tok = RBRACE
	case ',':
		tok = COMMA
	case ':':
		if n := len(s.frames); n > 0 && s.frames[n-1].kind == interp {
			// A colon an interpolation's expression doesn't hold (no
			// bracket of its is open): its format, up to the }.
			for s.ch != '}' && s.ch != eof && s.ch != '\n' && s.ch != '"' {
				s.next()
			}
			return pos, FORMAT_SPEC, s.src[start+1 : s.offset]
		}
		tok = COLON
	case ';':
		tok = SEMICOLON
	case '+':
		tok = s.alt('=', ADD_ASSIGN, ADD)
	case '-':
		switch s.ch {
		case '>':
			s.next()
			tok = ARROW
		case '=':
			s.next()
			tok = SUB_ASSIGN
		default:
			tok = SUB
		}
	case '*':
		if s.ch == '*' {
			s.next()
			tok = s.alt('=', POW_ASSIGN, POW)
		} else {
			tok = s.alt('=', MUL_ASSIGN, MUL)
		}
	case '/':
		tok = s.alt('=', QUO_ASSIGN, QUO)
	case '%':
		tok = s.alt('=', REM_ASSIGN, REM)
	case '=':
		switch s.ch {
		case '=':
			s.next()
			tok = EQL
		case '>':
			s.next()
			tok = FATARROW
		default:
			tok = ASSIGN
		}
	case '!':
		tok = s.alt('=', NEQ, NOT)
	case '<':
		switch s.ch {
		case '=':
			s.next()
			tok = LEQ
		case '<':
			s.next()
			tok = s.alt('=', SHL_ASSIGN, SHL)
		default:
			tok = LSS
		}
	case '>':
		tok = s.alt('=', GEQ, GTR)
	case '&':
		switch s.ch {
		case '&':
			s.next()
			tok = LAND
		case '=':
			s.next()
			tok = AND_ASSIGN
		case '+', '-', '*':
			// &+ &- &*: wrapping arithmetic, and their assignments.
			op, assign := WADD, WADD_ASSIGN
			if s.ch == '-' {
				op, assign = WSUB, WSUB_ASSIGN
			} else if s.ch == '*' {
				op, assign = WMUL, WMUL_ASSIGN
			}
			s.next()
			tok = op
			if s.ch == '=' {
				s.next()
				tok = assign
			}
		default:
			tok = AMP
		}
	case '|':
		switch s.ch {
		case '|':
			s.next()
			tok = LOR
		case '=':
			s.next()
			tok = OR_ASSIGN
		default:
			tok = PIPE
		}
	case '^':
		tok = s.alt('=', XOR_ASSIGN, CARET)
	case '~':
		tok = TILDE
	case '@':
		tok = AT
	case '?':
		switch s.ch {
		case '?':
			s.next()
			tok = s.alt('=', COALESCE_ASSIGN, COALESCE)
		case '.':
			s.next()
			tok = OPTDOT
		default:
			tok = QUESTION
		}
	case '.':
		switch s.ch {
		case '.':
			s.next()
			tok = s.alt('<', RANGELT, RANGE)
			if tok == RANGE && s.ch == '.' {
				s.error(start, diag.InvalidCharacter, "unexpected '...' (a range is a..b or a..<b)")
				s.next()
			}
		case '{':
			s.next()
			s.push(dotbrace)
			tok = DOTBRACE
		default:
			tok = PERIOD
		}
	default:
		if ch == '\uFEFF' {
			goto redo
		}
		s.error(start, diag.InvalidCharacter, "unexpected character "+strconv.QuoteRune(ch))
	}
	return pos, tok, s.src[start:s.offset]
}

func (s *Scanner) alt(c rune, yes, no Token) Token {
	if s.ch == c {
		s.next()
		return yes
	}
	return no
}

// comment scans a comment at s.ch == '/' and keeps it. For a block
// comment that spans lines it returns the offset of its first newline,
// otherwise -1.
func (s *Scanner) comment() int {
	start := s.offset
	nl := -1
	s.next()
	if s.ch == '/' {
		for s.ch != '\n' && s.ch != eof {
			s.next()
		}
	} else {
		s.next() // past *
		for {
			if s.ch == eof {
				s.error(start, diag.Unterminated, "comment not terminated")
				break
			}
			if s.ch == '\n' && nl < 0 {
				nl = s.offset
			}
			if s.ch == '*' && s.peek() == '/' {
				s.next()
				s.next()
				break
			}
			s.next()
		}
	}
	text := s.src[start:s.offset]
	if n := len(text); n > 0 && text[n-1] == '\r' && text[1] == '/' {
		text = text[:n-1]
	}
	s.comments = append(s.comments, &Comment{Pos: s.file.Pos(start), Text: text})
	return nl
}

// isLetter reports whether ch starts a name (see IsIdentStart).
func isLetter(ch rune) bool { return ch >= 0 && IsIdentStart(ch) }

func isDigit(ch rune) bool { return '0' <= ch && ch <= '9' }

func isHex(ch rune) bool {
	return '0' <= ch && ch <= '9' || 'a' <= ch && ch <= 'f' || 'A' <= ch && ch <= 'F'
}

func isBinary(ch rune) bool { return ch == '0' || ch == '1' }

func isOctal(ch rune) bool { return '0' <= ch && ch <= '7' }

func (s *Scanner) ident() {
	for s.ch >= 0 && IsIdentContinue(s.ch) {
		s.next()
	}
}

// digits scans digits and underscores (an underscore only between
// digits), as isDigit or isHex says.
func (s *Scanner) digits(ok func(rune) bool) {
	for ok(s.ch) || s.ch == '_' {
		if s.ch == '_' {
			if r := rune(s.peek()); !ok(r) {
				s.error(s.offset, diag.InvalidLiteral, "'_' must separate digits")
			}
		}
		s.next()
	}
}

// char scans a char literal at s.ch == '\'': one code point, or an
// escape (\n, \', \u{1F600}), between single quotes. What's more is an
// error, as is an empty one: a string holds more (or none).
func (s *Scanner) char(start int) (Pos, Token, string) {
	pos := s.file.Pos(start)
	s.next() // past '
	n := 0
	for s.ch != '\'' {
		switch s.ch {
		case eof, '\n':
			s.error(start, diag.Unterminated, "char not terminated: it's one code point between single quotes, 'a'")
			return pos, CHAR, s.src[start:s.offset]
		case '\\':
			s.escape()
		default:
			s.next()
		}
		n++
	}
	s.next() // past '
	lit := s.src[start:s.offset]
	switch {
	case n == 0:
		s.error(start, diag.InvalidLiteral, "'' holds no char: a char is one code point ('a'), an empty text a string (\"\")")
	case n > 1:
		s.error(start, diag.InvalidLiteral, lit+" holds "+strconv.Itoa(n)+" code points, and a char is one: a string holds more (\""+lit[1:len(lit)-1]+"\"), its graphemes() what a reader takes for one character")
	}
	return pos, CHAR, lit
}

// unitAfterDigits reports a unit (a letter or %) right after src's
// leading digits.
func unitAfterDigits(src string) bool {
	i := 0
	for i < len(src) && src[i] >= '0' && src[i] <= '9' {
		i++
	}
	return i < len(src) && (src[i] == '%' || isLetter(rune(src[i])))
}

// number scans an integer or a float and its unit: 300ms, 5s, 2min, 1h
// (durations), 50% and 1fr (lengths).
//
// A % straight after a number is a percentage unless it's followed by
// what starts an operand (a letter, a digit, an opening bracket, a
// string or a .), which makes it the remainder: 50% is a length, 10%3
// and 10 % 3 are remainders.
func (s *Scanner) number() Token {
	start := s.offset
	tok := INT
	if s.ch == '0' {
		var ok func(rune) bool
		var base, prefix string
		switch s.peek() {
		case 'x', 'X':
			ok, base, prefix = isHex, "a hexadecimal", "0x"
		case 'b', 'B':
			ok, base, prefix = isBinary, "a binary", "0b"
		case 'o', 'O':
			ok, base, prefix = isOctal, "an octal", "0o"
		}
		if ok != nil {
			s.next()
			s.next()
			if !ok(s.ch) {
				s.error(start, diag.InvalidLiteral, base+" number needs digits after "+prefix)
			}
			s.digits(ok)
			if isLetter(s.ch) || isDigit(s.ch) {
				s.error(s.offset, diag.InvalidLiteral, "unexpected "+strconv.QuoteRune(s.ch)+" in "+base+" number")
				s.ident()
			}
			return INT
		}
	}
	s.digits(isDigit)
	if s.ch == '.' && isDigit(rune(s.peek())) {
		tok = FLOAT
		s.next()
		s.digits(isDigit)
	}
	if s.ch == 'e' || s.ch == 'E' {
		p, p2 := rune(s.peek()), rune(s.peek2())
		if isDigit(p) || (p == '+' || p == '-') && isDigit(p2) {
			tok = FLOAT
			s.next()
			if s.ch == '+' || s.ch == '-' {
				s.next()
			}
			s.digits(isDigit)
		}
	}
	switch {
	case s.ch == '%':
		switch c := rune(s.peek()); {
		case isLetter(c), isDigit(c), c == '(', c == '[', c == '"', c == '.', c == '#':
			// a remainder: 10%3
		default:
			s.next()
			return LENGTH
		}
	case isLetter(s.ch):
		u := s.offset
		s.ident()
		switch unit := s.src[u:s.offset]; unit {
		case "ns", "us", "ms", "s", "min", "h":
			return DURATION
		case "fr":
			return LENGTH
		default:
			s.error(u, diag.InvalidLiteral, "unknown unit "+strconv.Quote(unit)+" after a number (a duration takes ns, us, ms, s, min or h; a length % or fr)")
		}
	}
	return tok
}

// color scans a color's hex digits after its #.
func (s *Scanner) color(start int) Token {
	d := s.offset
	for isHex(s.ch) {
		s.next()
	}
	n := s.offset - d
	if isLetter(s.ch) || isDigit(s.ch) {
		s.ident()
		s.error(start, diag.InvalidLiteral, "a color is #rgb, #rgba, #rrggbb or #rrggbbaa, in hexadecimal digits")
		return COLOR
	}
	switch n {
	case 3, 4, 6, 8:
	case 0:
		s.error(start, diag.InvalidLiteral, "unexpected '#' (a color is #rgb, #rrggbb or #rrggbbaa)")
	default:
		s.error(start, diag.InvalidLiteral, "a color has 3, 4, 6 or 8 hexadecimal digits, not "+strconv.Itoa(n))
	}
	return COLOR
}

// rawHashes reports, at the r of r"…", r#"…"#, r##"…"## (s.ch == 'r'),
// that a raw string starts there, and how many #s it has.
func (s *Scanner) rawHashes() (int, bool) {
	i := s.rdOffset
	for i < len(s.src) && s.src[i] == '#' {
		i++
	}
	return i - s.rdOffset, i < len(s.src) && s.src[i] == '"'
}

// string scans a string starting at its opening quote (s.ch == '"';
// start is the r of a raw string, hashes the #s between its r and its
// quote). A string with interpolations comes out as STRING_HEAD, the
// interpolation's tokens, then STRING_MID or STRING_TAIL from the } that
// ends it.
func (s *Scanner) string(start int, raw bool, hashes int) (Pos, Token, string) {
	multiline := false
	if s.peek() == '"' && s.peek2() == '"' {
		multiline = true
		s.next()
		s.next()
	}
	s.next() // past the (last) opening quote
	return s.stringRest(start, start, raw, multiline, false, hashes)
}

// closes reports whether n #s follow the closing quote at s.ch, which
// close a raw string opened with them, and moves past them.
func (s *Scanner) closes(quotes, n int) bool {
	at := s.offset + quotes
	if at+n > len(s.src) || strings.Count(s.src[at:at+n], "#") != n {
		return false
	}
	for range quotes + n {
		s.next()
	}
	return true
}

// stringRest scans a string's text from s.ch to its closing quote or
// its next interpolation; resumed says the text follows an
// interpolation's }. start is the token's first byte, open the string's.
// A raw string with hashes ends only at its quote followed by as many
// #s: r#"say "hi""#.
func (s *Scanner) stringRest(start, open int, raw, multiline, resumed bool, hashes int) (Pos, Token, string) {
	pos := s.file.Pos(start)
	done := func(t Token) (Pos, Token, string) { return pos, t, s.src[start:s.offset] }
	for {
		switch s.ch {
		case eof:
			s.error(open, diag.Unterminated, "string not terminated")
			return done(tailOr(resumed))
		case '\n':
			if !multiline {
				s.error(open, diag.Unterminated, "string not terminated (a string that spans lines is \"\"\"…\"\"\")")
				return done(tailOr(resumed))
			}
		case '"':
			if !multiline {
				if s.closes(1, hashes) {
					return done(tailOr(resumed))
				}
			} else if s.peek() == '"' && s.peek2() == '"' && s.closes(3, hashes) {
				return done(tailOr(resumed))
			}
		case '\\':
			if !raw {
				s.escape()
				continue
			}
		case '{':
			if !raw {
				s.next()
				s.frames = append(s.frames, frame{kind: interp, multiline: multiline, start: open})
				if resumed {
					return done(STRING_MID)
				}
				return done(STRING_HEAD)
			}
		}
		s.next()
	}
}

func tailOr(resumed bool) Token {
	if resumed {
		return STRING_TAIL
	}
	return STRING
}

// escape scans an escape at s.ch == '\\': \n \t \r \0 \" \' \\ \{ \}
// \u{1F600}.
func (s *Scanner) escape() {
	start := s.offset
	s.next()
	switch s.ch {
	case 'n', 't', 'r', '0', '"', '\'', '\\', '{', '}':
		s.next()
	case 'u':
		s.next()
		if s.ch != '{' {
			s.error(start, diag.InvalidLiteral, "a \\u escape is \\u{hex digits}")
			return
		}
		s.next()
		d := s.offset
		for isHex(s.ch) {
			s.next()
		}
		n, err := strconv.ParseUint(s.src[d:s.offset], 16, 32)
		if s.ch != '}' || s.offset == d || s.offset-d > 6 {
			s.error(start, diag.InvalidLiteral, "a \\u escape is \\u{1 to 6 hex digits}")
			return
		}
		s.next()
		if err != nil || n > unicode.MaxRune || 0xD800 <= n && n < 0xE000 {
			s.error(start, diag.InvalidLiteral, "\\u{"+s.src[d:s.offset-1]+"} isn't a Unicode code point")
		}
	case eof, '\n':
		s.error(start, diag.Unterminated, "escape not terminated")
	default:
		s.error(start, diag.InvalidLiteral, "unknown escape \\"+string(s.ch))
		s.next()
	}
}
