# 0018: Blocks by indentation: a deeper line opens a block, braces stay for one line

- **Status**: Draft
- **Created**: 2026-11-18
- **Related**: lang/syntax/scanner.go (significant newlines, the bracket
  frames), lang/syntax/parser.go (rule 6: `}` newline `else`),
  lang/syntax/GRAMMAR.md (line continuation, decision 26),
  lang/syntax/printer.go (`asuka fmt`'s one form), 0015 (diagnostics
  with codes); Python's and Haskell's offside rule, CoffeeScript's
  INDENT and OUTDENT tokens

## Summary

A block can be written by indentation as well as by braces. A line that
ends where a `{` could go, followed by a line indented deeper, opens a
block; the block ends where the indentation comes back. Both of these
are the same program, with the same tree:

```asuka
component Greeting(name: string) {
  state shown = true
  Column {
    gap: 8
    if shown {
      Text("Hello, {name}") { font: .title }
      Button("Hide") { on press { shown = false } }
    } else {
      Button("Show") { on press { shown = true } }
    }
  }
}
```

```asuka
component Greeting(name: string)
  state shown = true
  Column
    gap: 8
    if shown
      Text("Hello, {name}") { font: .title }
      Button("Hide") { on press { shown = false } }
    else
      Button("Show") { on press { shown = true } }
```

Braces stay for what fits on one line (`Text(t) { font: .title }`,
`on press { shown = false }`) and for anyone who prefers them: a line
that ends with `{` opens a block as it does today. The scanner does it
alone. Where indentation opens and closes a block it emits the `{` and
`}` that braces would have, so the parser, the checker and everything
after them see exactly the tokens they see now: the two forms can't
mean different things.

## Motivation

A line that holds only `}` says what the indentation already shows. In
Asuka's own code (examples, std and sdk: 159 files, 26,778 lines of
code without comments, blank lines or test blocks), 5,245 lines are a
lone `}`, about one line in five:

| | Files | Code lines | Lines that only close a block | Share |
|---|---:|---:|---:|---:|
| examples | 48 | 8,651 | 1,849 | 21% |
| std | 46 | 7,000 | 1,378 | 19% |
| sdk | 65 | 11,127 | 2,055 | 18% |

The share is the same in an app of about 3,400 lines written outside
this repository (20%). UI code is where it's highest: an element's
block holds its props and its children, a component's its body, an
`if` and a `match` theirs, and each ends in a line of its own. Without
those lines a screen reads as its tree, which is what indentation
already shows a reader.

Asuka is most of the way there. The scanner already decides where a
line ends (it returns a NEWLINE where a line ends an item, a member or
a statement, and none where it can't: after an operator, a comma or an
opening bracket, inside parentheses and brackets, before a line that
starts with a binary operator), and keeps a stack of the brackets open.
Indentation needs one more decision at the same place: whether the next
line is deeper.

## Decisions

1. **The scanner alone.** Indentation produces `{` and `}` tokens; the
   grammar doesn't change, and neither does anything after the parser.
2. **Both forms, mixed freely.** A block may be braced or indented, a
   file may hold both, and existing code is unchanged. A one-line block
   keeps its braces.
3. **Spaces only for indentation.** A line indented with a tab never
   opens a block: tabs stay braced code's, as in a program inside a Go
   string. A tab in the indentation of a line inside an indented block
   is an error. (No `.asuka` file in this repository indents with a
   tab.)
4. **One form from `asuka fmt`.** The printer writes one canonical form
   today and keeps writing one. Which form is the question this RFC
   most needs an answer to (Open questions).

## The language

### Where indentation opens a block

At the end of a line, where the scanner would return a NEWLINE, it
looks at the next line that isn't blank or only a comment. If that line
is indented deeper than the line that opens the block, and the line
doesn't already end with `{`, the scanner returns `{` instead of the
NEWLINE and pushes the deeper indentation. The lines it opens on are the
lines that end where a block's `{` would go: a component's or a
function's header, an element call (`Column`, `Text(t)`), `if`, `else`,
`for`, `while`, `match` and its arms' heads, `task`, `effect`,
`cleanup`, `do`, `catch`, a handler (`on press`), a style, a store, a
struct or enum body.

A deeper line that the scanner already treats as the line before's
continuation stays one: after an operator, a comma, `=>`, an opening
bracket, and before a line that starts with a binary operator.
Indentation never opens a block inside parentheses or brackets, where
newlines don't count.

A deeper line opens nothing where no block can go, so existing braced
code under uneven indentation keeps its meaning:

- a line that starts with `}`: it closes a braced block;
- a line after a `}` that starts its line: the end of a braced block
  written over lines;
- a line after an element's or a declaration's one-line block
  (`Text(t) { font: .title }`): that was its block.

A statement whose head takes an expression (`if`, `for`, `while`,
`match`) can still take its block after a one-line closure in it:
`if let p = try? await spawn { load() }` and a deeper line.

A deeper line that starts with `.name` continues the line before, as
decision 26 has it (`let names = users ⏎ .filter(…)`), with one
exception: after a line that starts with `match`, it's the first arm,
and the deeper line opens the arms' block. A block whose first line is
an implicit member for any other reason (rare: a value-only block such
as `if c ⏎ .red`) keeps its braces (Open questions).

### Where it closes one

When the next line is indented less, the scanner returns one `}` for
each indentation it pops, then the NEWLINE. The line must come back to
an indentation that was opened; anything else is an error (below). The
end of the file closes every indented block still open.

### else, catch and match arms

Rule 6 of the parser already reads `}` newline `else` as `} else`, and
the same for `catch`, so an `else` at the closing indentation needs
nothing new:

```asuka
if items.isEmpty()
  Text("Nothing yet")
else
  for i in items key i.id
    Row(i)
```

A `match` written by indentation has its arms one indentation deeper,
each arm's body either on its line or indented once more:

```asuka
match load
  .pending => Spinner()
  .failed(e) => Text(e.message) { color: .red }
  .ready(list) =>
    for x in list key x.id
      Row(x)
```

Here a line that ends with `=>` and is followed by a deeper line opens
a block for the arm, as it would end with `=> {` today.

### Closures

A closure's body inside a call's parentheses (`rows.map(r => {`, 180
lines in this repository) keeps its braces: newlines inside
parentheses don't count, and this RFC doesn't change that. A closure
that ends a line outside any bracket (`let f = x =>` and a deeper line)
opens a block as a match arm does.

### Strings and comments

The scanner already tracks a string's interpolation as a frame, so a
`"""` string's lines and its `{…}` parts are never read as
indentation. Blank lines and lines holding only a comment don't count:
a comment at any indentation neither opens nor closes a block.

## Checking

Indentation errors are the scanner's, reported where they are, each
with a code (0015) and its zh and ja text:

- `indent-mismatch`: a line comes back to an indentation no open block
  has ("this line's indentation, 3 spaces, matches no open block; the
  blocks open here are at 0, 2 and 4").
- `indent-tab`: a tab in the indentation of a line inside an indented
  block.
- `indent-unexpected`: a line indented deeper after a line that can't
  open a block or continue onto it (for example after a complete
  `let`): today a NEWLINE and an indented statement, which may be what
  was meant, so this may be a warning rather than an error (Open
  questions).

## The printer and asuka fmt

The printer keeps writing one form. In the indented form it writes a
block of more than one line without braces, a block written on one line
with them (as it keeps one-line blocks today), `else` and `catch` on
their own lines at the closing indentation, and a closure's body inside
parentheses with braces. Printing a file it wrote gives the same bytes
again, as today, and printing it in the other form and back does too:
the round trip is a test.

## Tools

- The language server's ranges come from the tokens, so a block opened
  by indentation spans from its header to its last line.
- Syntax highlighting has nothing new to colour.
- `asuka explain` gets the three codes' pages.

## Migration

None needed: braced code stays valid. If `asuka fmt` writes the
indented form, running it once converts a package; the repository's
own code would be converted in one commit, checked by the golden tests
and by comparing each file's tokens before and after.

## Alternatives

- **Indentation only, braces removed.** Simpler to describe, but a
  one-line block (`on press { x += 1 }`) has no indented spelling that
  isn't longer, and every existing file would change.
- **A keyword to end a block (`end`).** Saves nothing: it replaces a
  `}` line with an `end` line.
- **Leaving closing braces on the last line of their block** (Lisp's
  style, `Text(x) }}`). Saves the lines and keeps braces, but every
  edit to a block's last line moves the braces, and diffs show it.
- **Only in UI blocks** (elements and components). Two rules for one
  thing; the scanner's rule is the same for every block.

## Open questions

1. Which form does `asuka fmt` write: braces (today's, so nothing
   changes unless an author asks), or indentation (one form, the
   shorter one, across the repository and the docs)? A setting in
   `asuka.toml` would make it two canonical forms, which `asuka fmt`
   has so far avoided.
2. Is `indent-unexpected` an error or a warning?
3. Should a closure's body inside parentheses be able to open by
   indentation too (`rows.map(r =>` and a deeper line), for the 180
   lines that keep their braces under this RFC?
4. A deeper line starting with `.name` continues the line before
   except after `match`. Should another head (an `if` used as a value,
   a closure) also take an implicit member as its block's first line,
   or is braces for those enough?
5. The docs: show both forms everywhere, or the one `asuka fmt` writes
   with a note on the other?

## Milestones

1. The scanner: open and close by indentation, the three diagnostics,
   and tests that every file in the repository scans to the same tokens
   in both forms.
2. The printer: the indented form, its round trip, and the golden
   tests.
3. The docs in en, zh and ja, the grammar, `asuka explain` pages.
4. If accepted as the canonical form: the repository converted in one
   commit.
