package diag

import (
	"fmt"
	"strings"
	"sync"
)

// The compiler's codes, by what they're about. Each one's Wrong is
// reported with it, and only it, on every platform; its Right compiles
// clean (go test ./lang/diag). The run time's are in codes_run.go.

// Syntax.
var (
	SyntaxError = def(Code{
		Name:    "syntax-error",
		Summary: "the parser expected something else here",
		Why: `The parser reads a file token by token, and the token it found can't
come next: a value is missing after an operator, a parenthesis or a brace
isn't closed, two statements share a line without a ` + "`;`" + `.`,
		Fix: `Read the message's "expected …, found …": write what it expected, or
close what's open where it says. A mistake early on a line can hide the
rest of the line's, so fix the first one first.`,
		Wrong: "fn total(a: int, b: int) -> int {\n  return a +\n}\n",
		Right: "fn total(a: int, b: int) -> int {\n  return a + b\n}\n",
	})

	BracePlacement = def(Code{
		Name:    "brace-placement",
		Summary: "a block's `{` goes at the end of the line before",
		Why: `A line ends a statement where it can, so ` + "`Row`" + ` on a line of its own is
already a whole call, and a ` + "`{`" + ` on the next line would start a block of
its own. Asuka reads one way only, so it asks for the brace where the
block begins.`,
		Fix:   "Move the `{` to the end of the line before: `fn f() {`, `Row {`, `if x {`.",
		Wrong: "fn greet() -> string\n{\n  return \"hi\"\n}\n",
		Right: "fn greet() -> string {\n  return \"hi\"\n}\n",
	})

	Indentation = def(Code{
		Name:    "indentation",
		Summary: "a line's indentation matches no open block, or holds a tab inside an indented block",
		Why: `A block may be written by indentation instead of braces: a deeper line
opens it and the block ends where the indentation comes back. So a line
must come back to the indentation of a block that's open, or the scanner
can't tell which block it belongs to. Indentation is spaces: a tab is a
different width in every editor. A line indented with a tab never opens
a block (braced code may be indented with tabs, as a program inside a Go
string is), and one inside an indented block is this error.`,
		Fix: `Line the line up with the block it belongs to (the same indentation as
the lines before it in that block), or write the block with braces.
Indent with spaces.`,
		Wrong: "fn sign(a: int) -> int\n  if a < 0\n      return -1\n    return 1\n",
		Right: "fn sign(a: int) -> int\n  if a < 0\n    return -1\n  return 1\n",
	})

	NoSuchOperator = def(Code{
		Name:    "no-such-operator",
		Summary: "an operator other languages have and Asuka doesn't",
		Why: `Asuka leaves out operators that say the same as others: ` + "`++`" + ` and
` + "`--`" + ` are ` + "`+= 1`" + ` and ` + "`-= 1`" + `, the ternary ` + "`?:`" + ` is an ` + "`if`" + ` used as a value,
and comparisons don't chain.`,
		Fix:   "Write `x += 1`, `if a { b } else { c }`, or `a < b && b < c`.",
		Wrong: "fn count() -> int {\n  var n = 0\n  n++\n  return n\n}\n",
		Right: "fn count() -> int {\n  var n = 0\n  n += 1\n  return n\n}\n",
	})

	Unterminated = def(Code{
		Name:    "unterminated",
		Summary: "a string, a comment or an escape isn't closed",
		Why: `A string ends with its ` + "`\"`" + ` on the same line, a block comment with
` + "`*/`" + `, and ` + "`\\u{…}`" + ` with its brace. Without the end, the rest of the file
would be read as part of it.`,
		Fix:   "Close it. A string that spans lines is written `\"\"\"…\"\"\"`.",
		Wrong: "let greeting = \"Hello\n",
		Right: "let greeting = \"Hello\"\n",
	})

	InvalidLiteral = def(Code{
		Name:    "invalid-literal",
		Summary: "a number, a color, a duration or an escape that isn't written right",
		Why: `Literals have fixed forms: a color is ` + "`#rgb`" + `, ` + "`#rgba`" + `, ` + "`#rrggbb`" + ` or
` + "`#rrggbbaa`" + ` in hexadecimal; a duration takes ` + "`ms`" + `, ` + "`s`" + `, ` + "`min`" + ` or ` + "`h`" + `; an
escape is one Asuka knows (` + "`\\n`" + `, ` + "`\\t`" + `, ` + "`\\u{…}`" + `…). A value made when the app
compiles (an icon's markup, a pattern) must parse too.`,
		Fix:   "Write the literal in one of its forms, as the message says.",
		Wrong: "let brand = #12345\n",
		Right: "let brand = #123456\n",
	})

	FormatSpec = def(Code{
		Name:    "format-spec",
		Summary: "an interpolation's format that doesn't fit its value",
		Why: `A format after a colon in an interpolation (` + "`\"{price:>8.2f}\"`" + `) says how its
value is written: ` + "`[[fill]align][sign][#][0][width][,|_][.precision][type]`" + `, as
Python's and Rust's say. A sign, ` + "`#`" + `, ` + "`0`" + `, grouping and a type are a number's
(` + "`x`" + `, ` + "`X`" + `, ` + "`o`" + `, ` + "`b`" + ` an int's digits, ` + "`f`" + `, ` + "`e`" + `, ` + "`%`" + ` a float's); other values
are written as their text, which takes a fill, an alignment, a width and
a precision (the most characters). The compiler reads the format, so a
mistake shows where it's written.`,
		Fix:   "Write the format for the value's type, as the message says: `{n:>6}`, `{x:.2f}`, `{n:#x}`, `{name:<12}`.",
		Wrong: "fn price(cents: int) -> string = \"{cents:.2}\"\n",
		Right: "fn price(cents: int) -> string = \"{float(cents) / 100:.2f}\"\n",
	})

	InvalidCharacter = def(Code{
		Name:    "invalid-character",
		Summary: "a character that isn't part of the language here",
		Why: `Outside strings and comments, a file holds names, numbers and Asuka's
operators, in UTF-8. Anything else is a typo or text pasted from elsewhere.`,
		Fix:   "Remove the character, or put the text in a string or a comment.",
		Wrong: "let total = 1 $ 2\n",
		Right: "let total = 1 + 2\n",
	})
)

// Names.
var (
	UndefinedName = def(Code{
		Name:    "undefined-name",
		Summary: "a name nothing declares where it's used",
		Why: `A name means what a declaration in scope says: a local, a parameter, a
member of the type it's in, a declaration of the package, or one an
import brings. A misspelling, a name of another package that isn't
imported, or ` + "`self`" + ` where there is no value is none of these.`,
		Fix: `Fix the spelling (the message suggests a name close to it), import
the package that declares it, or declare it.`,
		Wrong: "fn double(count: int) -> int = cuont * 2\n",
		Right: "fn double(count: int) -> int = count * 2\n",
	})

	UnknownMember = def(Code{
		Name:    "unknown-member",
		Summary: "a type, an enum or a package has no member of that name",
		Why: `After a ` + "`.`" + `, a name is a field, a method, a case or a constant of what's
before it, or a declaration of a package. The type decides what it has;
extensions add to it only where they're visible.`,
		Fix:   "Use one of its members (the message suggests one), or add it to the type.",
		Wrong: "struct Person {\n  name: string\n}\n\nfn label(p: Person) -> string = p.nmae\n",
		Right: "struct Person {\n  name: string\n}\n\nfn label(p: Person) -> string = p.name\n",
	})

	UnknownPackage = def(Code{
		Name:    "unknown-package",
		Summary: "an import names no package",
		Why: `An import is the standard library's (` + "`std/…`" + `), the platform SDK's
(` + "`platform/…`" + `), a directory beside the file (` + "`./util`" + `), or a dependency
asuka.toml names. A path that's none of them, or a package that was
renamed, can't be read.`,
		Fix:   "Fix the path, add the dependency (`asuka add`), or use the package's new name.",
		Wrong: "import std/clock\n\nlet started = clock.now()\n",
		Right: "import std/time\n\nlet started = time.now()\n",
	})

	NotPub = def(Code{
		Name:    "not-pub",
		Summary: "a declaration another package, another file (private), a store or a worker keeps to itself",
		Why: `What a package declares is its own unless it says ` + "`pub`" + `; a store's and a
worker's members likewise; and what's ` + "`private`" + ` (a declaration, a type's
field or method) is its file's, and its package's tests'. That keeps a
package free to change what others don't use, and a type's invariants
its own.`,
		Fix:   "Mark it `pub` (or drop `private`) where it's declared, or use what's public: a type whose private fields have no default is made by its static fns.",
		Wrong: "-- app.asuka --\nimport ./util\n\nlet answer = util.secret()\n-- util/util.asuka --\nfn secret() -> int = 42\n",
		Right: "-- app.asuka --\nimport ./util\n\nlet answer = util.secret()\n-- util/util.asuka --\npub fn secret() -> int = 42\n",
	})

	DuplicateDeclaration = def(Code{
		Name:    "duplicate-declaration",
		Summary: "a name declared twice in one place",
		Why: `A name means one thing where it's declared: a package's top level, a
type's members, a function's parameters, an extension, a pattern. Two
declarations of it would make every use ambiguous. A file for one
platform (` + "`tray_linux.asuka`" + `) and a file every platform has can't both
declare it either.`,
		Fix:   "Rename one, or remove the one you don't need.",
		Wrong: "fn label() -> string = \"a\"\nfn label() -> string = \"b\"\n",
		Right: "fn label() -> string = \"a\"\nfn otherLabel() -> string = \"b\"\n",
	})

	ReservedName = def(Code{
		Name:    "reserved-name",
		Summary: "a name the language uses itself",
		Why: `Some names mean something already where this one would go: ` + "`peek`" + `
reads without following, ` + "`style`" + ` is every element's style list,
` + "`pressed`" + ` and the other interaction states are every view's variants.
Declaring one would hide what the language means by it.`,
		Fix:   "Name it otherwise.",
		Wrong: "fn peek() -> int = 1\n",
		Right: "fn first() -> int = 1\n",
	})

	NotAValue = def(Code{
		Name:    "not-a-value",
		Summary: "a type, a package or a method used where a value goes",
		Why: `A type names a kind of value, a package a set of declarations, and a
method needs the value it's called on: none of them is a value to pass
or store.`,
		Fix:   "Make a value of the type (`Point(x: 1, y: 2)`), use a package's member, or call the method.",
		Wrong: "struct Point {\n  x: int\n  y: int\n}\n\nlet origin = Point\n",
		Right: "struct Point {\n  x: int\n  y: int\n}\n\nlet origin = Point(x: 0, y: 0)\n",
	})

	NotAType = def(Code{
		Name:    "not-a-type",
		Summary: "something used as a type that isn't one",
		Why: `Where a type goes (a parameter's, a field's, after ` + "`:`" + `), the name must be
a type: a built-in one, a struct, an enum, an interface or an alias.
` + "`void?`" + ` and ` + "`T??`" + ` aren't types either.`,
		Fix:   "Write the type you mean: `bool` for `void?`, one `?` for an optional.",
		Wrong: "fn shown(flag: void?) -> bool = flag != none\n",
		Right: "fn shown(flag: bool) -> bool = flag\n",
	})

	NotCallable = def(Code{
		Name:    "not-callable",
		Summary: "a call of something that isn't a function",
		Why: `Only functions, closures and methods are called. A value isn't, a
field isn't, and an interface, an enum or a C type isn't made by calling
its name.`,
		Fix:   "Call a function, use the value as it is, or make one of the enum's cases (`Kind.name`).",
		Wrong: "fn twice(n: int) -> int = n(2)\n",
		Right: "fn twice(n: int) -> int = n * 2\n",
	})
)

// Types.
var (
	TypeMismatch = def(Code{
		Name:    "type-mismatch",
		Summary: "a value of another type than the one expected",
		Why: `Every value has one type, and what takes it (a parameter, a field, a
` + "`let`" + ` with its type written, a return, the branches of an ` + "`if`" + `) says which.
Asuka converts nothing on its own: an ` + "`int`" + ` isn't a ` + "`float`" + `, and a number
isn't a string.`,
		Fix:   "Give a value of the type expected, or convert it: `float(n)`, `int(x)`, `\"{n}\"`.",
		Wrong: "let count: int = \"three\"\n",
		Right: "let count: int = 3\n",
	})

	CannotInferType = def(Code{
		Name:    "cannot-infer-type",
		Summary: "the compiler can't tell a value's type from what's written",
		Why: `Types are inferred from values, but ` + "`[]`" + `, ` + "`[:]`" + `, ` + "`none`" + ` and ` + "`.case`" + ` say
nothing of theirs, and a generic call whose arguments don't name its
type parameters can't either.`,
		Fix:   "Write the type where the value goes: `let ids: [int] = []`, `Type.case`, `f<T>(…)`.",
		Wrong: "let ids = []\n",
		Right: "let ids: [int] = []\n",
	})

	NotOptional = def(Code{
		Name:    "not-optional",
		Summary: "an optional's operator on a value that's always there",
		Why:     "`??`, `?.`, `!`, `if let` and `??=` are for values that may be `none`. On one that can't be, they do nothing, which usually means the type isn't what the code thinks.",
		Fix:     "Drop the operator, or make the value optional where it's declared (`int?`).",
		Wrong:   "fn shown(count: int) -> int = count ?? 0\n",
		Right:   "fn shown(count: int?) -> int = count ?? 0\n",
	})

	MaybeNone = def(Code{
		Name:    "maybe-none",
		Summary: "a value that may be `none` used as if it were there",
		Why: `An optional (` + "`T?`" + `) is a ` + "`T`" + ` or ` + "`none`" + `. Its members, and a map's entry
` + "`m[k]`" + `, may not be there, so the compiler asks what to do when they
aren't.`,
		Fix:   "Use `?.` (`p?.name`), give a default with `??`, or unwrap it first: `if let p = p { … }`.",
		Wrong: "struct Person {\n  name: string\n}\n\nfn label(p: Person?) -> string = p.name\n",
		Right: "struct Person {\n  name: string\n}\n\nfn label(p: Person?) -> string = p?.name ?? \"nobody\"\n",
	})

	BadOperand = def(Code{
		Name:    "bad-operand",
		Summary: "an operator on values it doesn't take",
		Why: `Each operator takes certain types: arithmetic numbers of one kind
(` + "`int`" + ` with ` + "`int`" + `, ` + "`float`" + ` with ` + "`float`" + `), bit operators ints, ` + "`[]`" + ` arrays,
maps and bytes, ` + "`==`" + ` values of one type. A struct takes an operator from
a method (` + "`fn add`" + ` is ` + "`+`" + `).`,
		Fix:   "Convert one side (`float(n)`), use a method (`a.concat(b)`), or give the type the operator's method.",
		Wrong: "let rest = \"hello\" - \"h\"\n",
		Right: "let rest = \"hello\".slice(1, 5)\n",
	})

	ConstraintNotMet = def(Code{
		Name:    "constraint-not-met",
		Summary: "a type that lacks what a generic or a method needs of it",
		Why: `A set's elements and a map's keys are ` + "`Hashable`" + `, ` + "`==`" + ` takes
` + "`Equatable`" + ` values, ` + "`sorted()`" + ` ` + "`Comparable`" + ` ones, and a generic's type
parameter says what it needs (` + "`T: Comparable`" + `). A struct has them when
its fields do; a closure has none.`,
		Fix:   "Use a type that has it (an id instead of the whole value), or give the type what's missing.",
		Wrong: "struct Handler {\n  run: fn()\n}\n\nlet seen = Set<Handler>()\n",
		Right: "struct Handler {\n  id: int\n}\n\nlet seen = Set<Handler>()\n",
	})

	IntOverflow = def(Code{
		Name:    "int-overflow",
		Summary: "an integer literal that doesn't fit in its type",
		Why: `Each integer type holds a range: an ` + "`int`" + ` is 64 bits, from
−9223372036854775808 to 9223372036854775807, a ` + "`u8`" + ` 0 to 255, an ` + "`i8`" + `
−128 to 127, a ` + "`u64`" + ` 0 to 18446744073709551615. A literal outside its
type's range can't be one, and an unsigned type has no negatives.`,
		Fix:   "Use a number in the range, a wider type (`u16`, `int`, `u64`), a `float`, or, for an exact integer past 64 bits, `std/bigint`'s `BigInt` (`BigInt.parse(\"9223372036854775808\")!`).",
		Wrong: "let big = 9223372036854775808\nlet level: u8 = 300\n",
		Right: "let big: u64 = 9223372036854775808\nlet level: u16 = 300\n",
	})

	MixedNumbers = def(Code{
		Name:    "mixed-numbers",
		Summary: "an operator on numbers of two types",
		Why: `Asuka converts no number by itself: an ` + "`int`" + ` and a ` + "`float`" + `, or a
` + "`u8`" + ` and an ` + "`int`" + `, don't meet in ` + "`+`" + `, ` + "`<`" + ` or ` + "`&`" + `. Widening one quietly
would hide where a value can be truncated, wrap or lose its fraction.
An int literal takes the other side's type, so ` + "`level + 1`" + ` is fine.`,
		Fix: `Convert one side to the other's type: ` + "`int(level) + offset`" + `, ` + "`float(n) * 0.5`" + `,
` + "`u8(n)`" + ` (which throws when n doesn't fit; ` + "`u8(truncating: n)`" + ` keeps its low bits).`,
		Wrong: "fn total(level: u8, bonus: int) -> int = level + bonus\n",
		Right: "fn total(level: u8, bonus: int) -> int = int(level) + bonus\n",
	})

	TypeArguments = def(Code{
		Name:    "type-arguments",
		Summary: "the wrong number of type arguments",
		Why: `A generic type or function says how many types it takes: ` + "`Set<T>`" + `
one, ` + "`[K: V]`" + ` two, a struct its own. A type that isn't generic takes
none.`,
		Fix:   "Give as many types as it declares: `Set<int>`, `Page<Item>`.",
		Wrong: "struct Box<T> {\n  value: T\n}\n\nfn unbox(b: Box<int, string>) -> int = 0\n",
		Right: "struct Box<T> {\n  value: T\n}\n\nfn unbox(b: Box<int>) -> int = b.value\n",
	})

	NoValue = def(Code{
		Name:    "no-value",
		Summary: "something used as a value that gives none",
		Why: `An ` + "`if`" + ` without an ` + "`else`" + `, a block whose last line isn't an
expression, a call of a function that returns nothing, ` + "`()`" + ` and ` + "`_`" + ` have
no value to give.`,
		Fix:   "Give the `if` an `else`, end the block with the value, or use a function that returns one.",
		Wrong: "fn sign(n: int) -> int = if n < 0 { -1 }\n",
		Right: "fn sign(n: int) -> int = if n < 0 { -1 } else { 1 }\n",
	})

	RecursiveType = def(Code{
		Name:    "recursive-type",
		Summary: "a type that holds a whole copy of itself",
		Why: `A struct's fields are inside it, so a struct whose field is itself
would be infinitely large, and no value of it could be made. An alias
can't name itself; a C struct can't hold itself either.`,
		Fix:   "Make the field on the way optional (`next: Node?`) or an array (`children: [Node]`).",
		Wrong: "struct Node {\n  value: int\n  next: Node\n}\n",
		Right: "struct Node {\n  value: int\n  next: Node? = none\n}\n",
	})

	RecursiveDefinition = def(Code{
		Name:    "recursive-definition",
		Summary: "a value or a style defined in terms of itself",
		Why: `A constant, a ` + "`let`" + ` or a state is computed from what it reads; one
that reads itself, through others or directly, has no first value to
start from. A style that applies itself would never end.`,
		Fix:   "Break the cycle: compute one from values that don't read it back.",
		Wrong: "let width = height * 2\nlet height = width / 2\n",
		Right: "let width = 200\nlet height = width / 2\n",
	})

	MissingType = def(Code{
		Name:    "missing-type",
		Summary: "a declaration whose type must be written",
		Why: `Parameters say their types, and a prop says its type or a default to
infer it from. A state that reads itself (its current value, ` + "`none`" + ` at
first) can't be inferred from its own value.`,
		Fix:   "Write the type: `prop size: int`, `prop size = 12`, `state n: int = …`.",
		Wrong: "component Label {\n  prop size\n  Text(\"{size}\")\n}\n",
		Right: "component Label {\n  prop size = 12\n  Text(\"{size}\")\n}\n",
	})

	NotAnError = def(Code{
		Name:    "not-an-error",
		Summary: "a value thrown or caught that isn't an error type",
		Why: `What ` + "`throw`" + ` throws and ` + "`catch e: T`" + ` catches is a type declared an
error (` + "`struct T: Error`" + `, ` + "`enum T: Error`" + `), so every catch can read its
` + "`message`" + ` and every throw is one a caller can name.`,
		Fix:   "Declare the type `: Error`, or throw `Error(\"…\")`.",
		Wrong: "struct Oops {\n  why: string\n}\n\nfn check(ok: bool) throws {\n  if !ok { throw Oops(why: \"no\") }\n}\n",
		Right: "struct Oops: Error {\n  why: string\n}\n\nfn check(ok: bool) throws {\n  if !ok { throw Oops(why: \"no\") }\n}\n",
	})

	PatternType = def(Code{
		Name:    "pattern-type",
		Summary: "a pattern that can't match the value's type",
		Why: `A pattern in ` + "`match`" + `, ` + "`if let`" + ` or ` + "`catch`" + ` describes values of the
matched type: a literal of that type, one of its cases, a tuple of its
size. One of another type would never match.`,
		Fix:   "Write a pattern of the value's type: `1`, not `\"one\"`, for an int.",
		Wrong: "fn name(n: int) -> string = match n {\n  \"one\" => \"1\"\n  _ => \"?\"\n}\n",
		Right: "fn name(n: int) -> string = match n {\n  1 => \"one\"\n  _ => \"?\"\n}\n",
	})

	InvalidPattern = def(Code{
		Name:    "invalid-pattern",
		Summary: "a pattern written in a form the language doesn't take",
		Why: `Patterns have their forms: a case is ` + "`.name(…)`" + ` or ` + "`Type.name(…)`" + `, a
struct pattern names its fields (` + "`Point(x: 0)`" + `), ` + "`|`" + ` joins patterns that bind
nothing, ` + "`let`" + ` binds names and doesn't test, and a ` + "`for`" + ` binds a name or a
tuple of names.`,
		Fix:   "Write the pattern in its form, as the message shows: `.case(x)`, `if let`, `(k, v)`.",
		Wrong: "enum Shape {\n  circle(radius: float)\n  square(side: float)\n}\n\nfn size(s: Shape) -> float = match s {\n  .circle(x) | .square(x) => x\n}\n",
		Right: "enum Shape {\n  circle(radius: float)\n  square(side: float)\n}\n\nfn size(s: Shape) -> float = match s {\n  .circle(r) => r\n  .square(side) => side\n}\n",
	})

	NonExhaustiveMatch = def(Code{
		Name:    "non-exhaustive-match",
		Summary: "a `match` that doesn't cover every case",
		Why: `A ` + "`match`" + ` gives a value (or runs an arm) for every value it's given, so it
covers every case of an enum, every value of a bool, and the rest with
` + "`_`" + `. The compiler says what's missing, as patterns. Adding a case to an
enum then shows every ` + "`match`" + ` that needs it.`,
		Fix:   "Add arms for what's missing, or `_ => …` for the rest.",
		Wrong: "enum Light {\n  red\n  green\n}\n\nfn wait(l: Light) -> bool = match l {\n  .red => true\n}\n",
		Right: "enum Light {\n  red\n  green\n}\n\nfn wait(l: Light) -> bool = match l {\n  .red => true\n  .green => false\n}\n",
	})
)

// Calls.
var (
	ArgumentCount = def(Code{
		Name:    "argument-count",
		Summary: "a call with too many or too few arguments",
		Why: `A call gives each parameter without a default its value, and no more
than it has. A case's payload, an event's values and a handler's
parameters are counted the same way.`,
		Fix:   "Pass what the function declares; leave out only parameters with defaults.",
		Wrong: "fn add(a: int, b: int) -> int = a + b\n\nlet sum = add(1)\n",
		Right: "fn add(a: int, b: int) -> int = a + b\n\nlet sum = add(1, 2)\n",
	})

	ArgumentLabel = def(Code{
		Name:    "argument-label",
		Summary: "a named argument the function has no parameter for",
		Why: `Arguments come in order, then by name: a name is one of the
parameters', given once, after the ones in order. A closure's
arguments have no names, nor have most built-ins' (print's
` + "`separator:`" + ` and ` + "`terminator:`" + ` are named).`,
		Fix:   "Use the parameter's name (the message suggests it), or pass the argument in order.",
		Wrong: "fn area(width: int, height: int) -> int = width * height\n\nlet a = area(width: 2, hieght: 3)\n",
		Right: "fn area(width: int, height: int) -> int = width * height\n\nlet a = area(width: 2, height: 3)\n",
	})

	UnexpectedBlock = def(Code{
		Name:    "unexpected-block",
		Summary: "a block after a call that takes none",
		Why: `A block after a call is its last argument, a function, or an element's
props and children. A function whose last parameter isn't a function, a
struct, a store or a worker takes its values in parentheses only.`,
		Fix:   "Pass the values in the parentheses, and drop the block.",
		Wrong: "fn twice(n: int) -> int = n * 2\n\nlet four = twice(2) {\n  3\n}\n",
		Right: "fn twice(n: int) -> int = n * 2\n\nlet four = twice(2)\n",
	})

	MissingAwait = def(Code{
		Name:    "missing-await",
		Summary: "a call of an async function without `await`",
		Why: `An async function's result comes later; ` + "`await`" + ` says where the code
waits for it, so every place another handler may run in between is
written down.`,
		Fix:   "Write `await f(…)` (`try await` if it throws), or start it on its own with `spawn`.",
		Wrong: "async fn fetchCount() -> int {\n  await sleep(1s)\n  return 1\n}\n\nasync fn total() -> int {\n  return fetchCount()\n}\n",
		Right: "async fn fetchCount() -> int {\n  await sleep(1s)\n  return 1\n}\n\nasync fn total() -> int {\n  return await fetchCount()\n}\n",
	})

	MissingTry = def(Code{
		Name:    "missing-try",
		Summary: "a call of a function that throws without `try`",
		Why: `A function that can fail says ` + "`throws`" + `, and each call says ` + "`try`" + `, so
every place an error can leave from is written down.`,
		Fix:   "Write `try f(…)`, `try? f(…)` for `none` on an error, or `try f(…) catch { fallback }`.",
		Wrong: "fn parse(s: string) throws -> int {\n  if s.isEmpty() { throw Error(\"empty\") }\n  return s.count()\n}\n\nfn size(s: string) throws -> int = parse(s)\n",
		Right: "fn parse(s: string) throws -> int {\n  if s.isEmpty() { throw Error(\"empty\") }\n  return s.count()\n}\n\nfn size(s: string) throws -> int = try parse(s)\n",
	})

	AwaitOutsideAsync = def(Code{
		Name:    "await-outside-async",
		Summary: "an `await` where nothing may wait",
		Why: `Only async functions, handlers, tasks and tests wait. A plain function
runs to its end at once, and a component's body is declarations, not
code that runs.`,
		Fix:   "Mark the function `async`, or load data with `let x = load f()` or a `task { }` in a component.",
		Wrong: "async fn fetchCount() -> int {\n  await sleep(1s)\n  return 1\n}\n\nfn total() -> int {\n  return await fetchCount()\n}\n",
		Right: "async fn fetchCount() -> int {\n  await sleep(1s)\n  return 1\n}\n\nasync fn total() -> int {\n  return await fetchCount()\n}\n",
	})

	AsyncInEffect = def(Code{
		Name:    "async-in-effect",
		Summary: "an `await` in an effect",
		Why: `An effect runs at once, and again whenever what it read changes; a run
that waited would still be running when the next one starts. Async work
belongs to a task, which restarts by cancelling the run before.`,
		Fix:   "Use `task(what) { … }`: it runs when the component appears and again, cancelling the last run, when `what` changes.",
		Wrong: "async fn save(n: int) {\n  await sleep(1s)\n}\n\ncomponent Counter {\n  state n = 0\n  effect { await save(n) }\n  Button(\"{n}\") { on press { n += 1 } }\n}\n",
		Right: "async fn save(n: int) {\n  await sleep(1s)\n}\n\ncomponent Counter {\n  state n = 0\n  task(n) { await save(n) }\n  Button(\"{n}\") { on press { n += 1 } }\n}\n",
	})

	UnhandledError = def(Code{
		Name:    "unhandled-error",
		Summary: "an error that can leave where nothing takes it",
		Why: `An error leaves a function only when it says ` + "`throws`" + `; a closure, a
handler, a component's ` + "`let`" + ` or a value at the top of a file has no
caller to give it to.`,
		Fix:   "Mark the function `throws`, or catch the error: `try … catch { fallback }`, `do { } catch { }`, `try?`.",
		Wrong: "fn parse(s: string) throws -> int {\n  if s.isEmpty() { throw Error(\"empty\") }\n  return s.count()\n}\n\nfn size(s: string) -> int = try parse(s)\n",
		Right: "fn parse(s: string) throws -> int {\n  if s.isEmpty() { throw Error(\"empty\") }\n  return s.count()\n}\n\nfn size(s: string) -> int = try parse(s) catch { 0 }\n",
	})

	InvalidCatch = def(Code{
		Name:    "invalid-catch",
		Summary: "a `catch` that can't be reached, or has nothing to catch",
		Why: `Catches are tried in order, so one after a catch of everything is
never reached, and one after code that can't throw has nothing to take.
` + "`do`" + ` needs a catch; a catch names an error type or one of its cases.`,
		Fix:   "Put the specific catches first, the catch of everything last, and catch only what can throw.",
		Wrong: "enum ParseError: Error {\n  empty\n}\n\nfn parse(s: string) throws -> int {\n  if s.isEmpty() { throw ParseError.empty }\n  return 1\n}\n\nfn size(s: string) -> int {\n  do {\n    return try parse(s)\n  } catch e {\n    return -1\n  } catch ParseError.empty {\n    return 0\n  }\n}\n",
		Right: "enum ParseError: Error {\n  empty\n}\n\nfn parse(s: string) throws -> int {\n  if s.isEmpty() { throw ParseError.empty }\n  return 1\n}\n\nfn size(s: string) -> int {\n  do {\n    return try parse(s)\n  } catch ParseError.empty {\n    return 0\n  } catch e {\n    return -1\n  }\n}\n",
	})

	NotAsyncCall = def(Code{
		Name:    "not-async-call",
		Summary: "`load`, `all` or `race` given something that isn't an async call",
		Why:     "`load f(…)` keeps an async result, and `all` and `race` run async calls at once. A plain value has nothing to wait for.",
		Fix:     "Pass an async call (`load api.user(id)`), or use the value as it is.",
		Wrong:   "fn one() -> int = 1\n\nasync fn both() -> int {\n  let (a, b) = await all(one(), one())\n  return a + b\n}\n",
		Right:   "async fn one() -> int {\n  await sleep(1ms)\n  return 1\n}\n\nasync fn both() -> int {\n  let (a, b) = await all(one(), one())\n  return a + b\n}\n",
	})
)

// Statements.
var (
	MissingReturn = def(Code{
		Name:    "missing-return",
		Summary: "a function with a result that can reach its end without one",
		Why: `A function that says ` + "`-> T`" + ` gives a ` + "`T`" + ` on every path. The compiler
follows the paths, so an ` + "`if`" + ` without an ` + "`else`" + ` or a ` + "`match`" + ` arm that
returns nothing is a path without a value.`,
		Fix:   "Return a value on every path, or end the function with a `return`.",
		Wrong: "fn sign(n: int) -> int {\n  if n < 0 { return -1 }\n}\n",
		Right: "fn sign(n: int) -> int {\n  if n < 0 { return -1 }\n  return 1\n}\n",
	})

	ReturnValue = def(Code{
		Name:    "return-value",
		Summary: "a `return` with a value the function doesn't give, or without one it does",
		Why:     "A function without `-> T` returns nothing, and one with it returns a `T` from each `return`.",
		Fix:     "Drop the value, or declare the result (`-> T`) and return one everywhere.",
		Wrong:   "fn log(s: string) {\n  print(s)\n  return s\n}\n",
		Right:   "fn log(s: string) {\n  print(s)\n}\n",
	})

	MisplacedStatement = def(Code{
		Name:    "misplaced-statement",
		Summary: "a statement where it can't do what it says",
		Why:     "`break` and `continue` leave a loop, so they're in one; `return` leaves a function, not a `defer` or a component's body; `emit` raises a component's event, so it's in a component; `let … else` is a statement of a function or a handler.",
		Fix:     "Move the statement where it means something, or write what you meant there.",
		Wrong:   "fn first(xs: [int]) -> int {\n  if xs.isEmpty() { break }\n  return xs[0]\n}\n",
		Right:   "fn first(xs: [int]) -> int {\n  if xs.isEmpty() { return 0 }\n  return xs[0]\n}\n",
	})

	ViewInStatement = def(Code{
		Name:    "view-in-statement",
		Summary: "a view made in code that runs",
		Why: `Views are declared in a component's body, where they stay as long as
what decides them holds. A view made in a handler or a function would be
made and dropped, shown nowhere.`,
		Fix:   "Declare the view in the body, under an `if` on state the handler sets.",
		Wrong: "component Hello {\n  Button(\"Greet\") {\n    on press { Text(\"Hi!\") }\n  }\n}\n",
		Right: "component Hello {\n  state greeted = false\n  Column {\n    Button(\"Greet\") { on press { greeted = true } }\n    if greeted { Text(\"Hi!\") }\n  }\n}\n",
	})

	NotAssignable = def(Code{
		Name:    "not-assignable",
		Summary: "a change of something that can't change",
		Why: `What changes is a ` + "`var`" + `, state, or a field or an element of one. A ` + "`let`" + `
is a constant (or, in a component, derived from what it reads), a
parameter is the caller's, and a method changes its value only when it's
a ` + "`mut fn`" + `.`,
		Fix:   "Make it a `var` (or `state`), change what a `let` reads, or declare the method `mut fn`.",
		Wrong: "fn countdown() -> int {\n  let n = 3\n  n -= 1\n  return n\n}\n",
		Right: "fn countdown() -> int {\n  var n = 3\n  n -= 1\n  return n\n}\n",
	})

	PropNotAssignable = def(Code{
		Name:    "prop-not-assignable",
		Summary: "a component assigns a prop it can't",
		Why: `A prop is its caller's. A component assigns one only when it reports
the change, with ` + "`event <prop>Change`" + `, so that a caller that binds it
(` + "`bind open: shown`" + `) gets the new value; and a prop changes whole.`,
		Fix:   "Declare `event openChange(open: bool)` to assign it, or keep state of the component's own.",
		Wrong: "component Disclosure {\n  prop open = false\n  Button(\"More\") { on press { open = !open } }\n}\n",
		Right: "component Disclosure {\n  prop open = false\n  event openChange(open: bool)\n  Button(\"More\") { on press { open = !open } }\n}\n",
	})

	InvalidFor = def(Code{
		Name:    "invalid-for",
		Summary: "a `for` over something it doesn't go over",
		Why: `A ` + "`for`" + ` goes over an array, a set, a map, a range, a string's chars or
an iterator; a stream's values come over time (` + "`for await`" + `); a number
isn't a list (` + "`0..<n`" + ` is its range). ` + "`key`" + ` is a ` + "`for`" + `'s in a view.`,
		Fix:   "Go over what the message says: `0..<n`, `m.keys()`, `for await x in stream`.",
		Wrong: "fn total(n: int) -> int {\n  var sum = 0\n  for i in n { sum += i }\n  return sum\n}\n",
		Right: "fn total(n: int) -> int {\n  var sum = 0\n  for i in 0..<n { sum += i }\n  return sum\n}\n",
	})

	LoopLabel = def(Code{
		Name:    "loop-label",
		Summary: "a loop's name that isn't used, or isn't there",
		Why: `` + "`for … as outer`" + ` names a loop for a ` + "`break outer`" + ` or ` + "`continue outer`" + `
inside another one. A name nothing uses, two loops of one name, or a
` + "`break`" + ` of a loop it isn't in says something that isn't so.`,
		Fix:   "Drop the name, or use it: `continue outer`.",
		Wrong: "fn total(xs: [int]) -> int {\n  var sum = 0\n  for x in xs as each {\n    sum += x\n  }\n  return sum\n}\n",
		Right: "fn total(xs: [int]) -> int {\n  var sum = 0\n  for x in xs {\n    sum += x\n  }\n  return sum\n}\n",
	})

	LetElse = def(Code{
		Name:    "let-else",
		Summary: "a `let … else` whose else goes on, or has nothing to unwrap",
		Why: `` + "`let x = optional else { … }`" + ` binds ` + "`x`" + ` for the rest of the block, so
its ` + "`else`" + ` can't go on to it: it leaves (` + "`return`" + `, ` + "`throw`" + `, ` + "`break`" + `,
` + "`continue`" + `). A value that's always there needs no ` + "`else`" + `.`,
		Fix:   "End the `else` with `return` (or `throw`, `break`, `continue`), or drop it.",
		Wrong: "fn shown(name: string?) -> string {\n  let n = name else { print(\"no name\") }\n  return n\n}\n",
		Right: "fn shown(name: string?) -> string {\n  let n = name else { return \"\" }\n  return n\n}\n",
	})

	LocalFn = def(Code{
		Name:    "local-fn",
		Summary: "a function in a block declared with what only a file's functions have",
		Why: `A function declared in a block is a helper of that block: it isn't
` + "`pub`" + `, generic, native or extern, and it takes its arguments in order,
without defaults.`,
		Fix:   "Declare it at the top of the file for any of those.",
		Wrong: "fn pair() -> int {\n  fn both<T>(x: T) -> [T] = [x, x]\n  return both(1).count()\n}\n",
		Right: "fn both<T>(x: T) -> [T] = [x, x]\n\nfn pair() -> int {\n  return both(1).count()\n}\n",
	})
)

// Declarations.
var (
	MisplacedDeclaration = def(Code{
		Name:    "misplaced-declaration",
		Summary: "a declaration where its kind doesn't go",
		Why: `Each place holds certain declarations: a file holds imports first, then
types, functions and state; a struct holds fields and methods; a store
state, lets, fns, tasks and effects; a worker vars, lets and fns; an
enum its cases before its constants and methods; an extension methods
and constants.`,
		Fix:   "Move it where its kind goes, or use the kind this place holds (a worker's `var` for `state`).",
		Wrong: "worker Index {\n  state count = 0\n}\n",
		Right: "worker Index {\n  var count = 0\n}\n",
	})

	InvalidDeclaration = def(Code{
		Name:    "invalid-declaration",
		Summary: "a declaration written in a form its place doesn't take",
		Why: `A type's constant is ` + "`let name = value`" + `, which doesn't change, and a
generic type's is one for all its types, so its type can't name their
parameters; a component's ` + "`let`" + ` names one value, and a default can't read
fields that aren't made yet.`,
		Fix:   "Write it in the place's form: `let name = value` for a type's constant, a `static fn` for one that needs the type's parameters.",
		Wrong: "struct Money {\n  cents: int\n\n  var zero = 0\n}\n",
		Right: "struct Money {\n  cents: int\n\n  let zero = 0\n}\n",
	})

	InvalidModifier = def(Code{
		Name:    "invalid-modifier",
		Summary: "`pub`, `mut`, `async`, `throws` or type parameters where they don't apply",
		Why: `A ` + "`mut fn`" + ` changes the value it's called on, so a store's fns (which
change its state as they are) and a static fn (which has no value) don't
take it, and it can't be ` + "`async`" + `. A worker's fns don't await, a C
function isn't generic, async or throwing, and a test or a preview isn't
` + "`pub`" + `.`,
		Fix:   "Drop the modifier the message names.",
		Wrong: "store Counter {\n  state n = 0\n  pub mut fn bump() { n += 1 }\n}\n",
		Right: "store Counter {\n  state n = 0\n  pub fn bump() { n += 1 }\n}\n",
	})

	FnBody = def(Code{
		Name:    "fn-body",
		Summary: "a function without its body, or with one it can't have",
		Why: `A function has a body, unless the platform implements it: a native
interface's methods, a native fn and an extern fn (C's) have none.`,
		Fix:   "Write the body (`{ … }` or `= expression`), or drop it from a native or extern declaration.",
		Wrong: "fn greeting() -> string\n",
		Right: "fn greeting() -> string = \"Hello\"\n",
	})

	SpecialMethod = def(Code{
		Name:    "special-method",
		Summary: "a method the language calls (equals, hash, description, toJSON, fromJSON) with a signature it can't call",
		Why: `A struct or an enum is equal, hashes, prints and is written as JSON
by its fields unless it declares its own: ` + "`fn equals(other: T) -> bool`" + `,
` + "`fn hash() -> int`" + `, ` + "`fn description() -> string`" + `, ` + "`fn toJSON() -> J`" + ` and
` + "`static fn fromJSON(json: J) throws -> T`" + `. The language calls them from
==, maps and sets, interpolation and JSON, so each takes and gives what
those pass and expect, and can't be async, mut or private. A type that
hashes its own way compares its own way too (equal values hash alike).`,
		Fix:   "Give the method the signature the message says, or another name if it isn't meant for that.",
		Wrong: "struct Money {\n  cents: int\n\n  fn equals(other: int) -> bool = cents == other\n}\n",
		Right: "struct Money {\n  cents: int\n\n  fn equals(other: Money) -> bool = cents == other.cents\n}\n",
	})

	InterfaceNotSatisfied = def(Code{
		Name:    "interface-not-satisfied",
		Summary: "a type that declares an interface it doesn't implement",
		Why: `` + "`struct T: Shape`" + ` promises every method ` + "`Shape`" + ` lists, with its
signature, so a ` + "`Shape`" + ` can be called through without knowing the type.`,
		Fix:   "Add the missing methods (the message lists them), with the interface's signatures.",
		Wrong: "interface Shape {\n  fn area() -> float\n}\n\nstruct Square: Shape {\n  side: float\n}\n",
		Right: "interface Shape {\n  fn area() -> float\n}\n\nstruct Square: Shape {\n  side: float\n  fn area() -> float = side * side\n}\n",
	})

	InvalidConformance = def(Code{
		Name:    "invalid-conformance",
		Summary: "a type declares, or a type parameter needs, what isn't an interface",
		Why: `After ` + "`:`" + `, a type names interfaces: ` + "`Error`" + `, ` + "`Equatable`" + `,
` + "`Comparable`" + `, ` + "`Hashable`" + ` or one the program declares. A native interface
is the platform's to implement, not a type's.`,
		Fix:   "Name an interface there, or drop it.",
		Wrong: "struct Score: int {\n  points: int\n}\n",
		Right: "struct Score: Equatable {\n  points: int\n}\n",
	})

	InvalidExtension = def(Code{
		Name:    "invalid-extension",
		Summary: "an `extend` of what can't be extended, or in a form it doesn't take",
		Why: `An extension adds to a named type, built in or declared: not to a
function type, a tuple or an interface. A generic extension's type
parameters are the type's, and a constant or a static fn needs a type
with a name to be called on.`,
		Fix:   "Extend a named type, or declare a function instead.",
		Wrong: "extend fn() -> int {\n  fn twice() -> int = self() * 2\n}\n",
		Right: "fn twice(f: fn() -> int) -> int = f() * 2\n",
	})

	ExtensionConflict = def(Code{
		Name:    "extension-conflict",
		Summary: "an extension that adds a name the type already has, or another extension adds",
		Why: `An extension adds names; it doesn't replace them, so a method's
meaning never depends on what's imported. Two extensions that add the
same name to a type would make every call of it ambiguous.`,
		Fix:   "Name the method otherwise, or keep one of the extensions.",
		Wrong: "extend string {\n  fn count() -> int = 0\n}\n",
		Right: "extend string {\n  fn wordCount() -> int = fields().count()\n}\n",
	})

	NotSupported = def(Code{
		Name:    "not-supported",
		Summary: "something the language doesn't do yet",
		Why: `A few forms parse but aren't implemented: a generic component, a screen
without the element it needs, a few expressions the compiler can't lower
yet. The message says what to use instead.`,
		Fix:   "Use the alternative the message names: a `View` or a closure prop for a generic component.",
		Wrong: "component Box<T>(value: T) {\n  Text(\"box\")\n}\n",
		Right: "component Box(value: string) {\n  Text(value)\n}\n",
	})

	DeprecatedSyntax = def(Code{
		Name:    "deprecated-syntax",
		Summary: "a form the language replaced, which `asuka fmt` rewrites",
		Why: `An element applies styles with ` + "`style:`" + `, as a style applies others; a
style's name alone in a block was the old form.`,
		Fix:   "Run `asuka fmt -w`, or write `style: name`.",
		Wrong: "style card {\n  padding: 16\n}\n\ncomponent Note {\n  Column {\n    style card\n    Text(\"hi\")\n  }\n}\n",
		Right: "style card {\n  padding: 16\n}\n\ncomponent Note {\n  Column {\n    style: card\n    Text(\"hi\")\n  }\n}\n",
	})

	InvalidEntry = def(Code{
		Name:    "invalid-entry",
		Summary: "a program's `fn main` declared otherwise than it runs",
		Why: `A program starts at ` + "`fn main()`" + `: it takes no parameters (the
arguments are ` + "`os.args()`" + `), returns nothing (` + "`os.exit(code)`" + ` ends with a
code), isn't generic, and a package is an app or a program, not both.`,
		Fix:   "Declare `fn main() { … }`, and read the arguments with std/os.",
		Wrong: "fn main(args: [string]) {\n  print(args)\n}\n",
		Right: "import std/os\n\nfn main() {\n  print(os.args())\n}\n",
	})
)

// Components and views.
var (
	WriteInBinding = def(Code{
		Name:    "write-in-binding",
		Summary: "state written where the UI follows it",
		Why: `A binding, a ` + "`let`" + ` and a component's body say what stays true; they
run again whenever what they read changes. A write there would run on
every change it causes.`,
		Fix: `Write state in a handler (` + "`on press { … }`" + `), a fn, a task or an effect. A
value computed from state is a ` + "`let`" + `.`,
		Wrong: "component Counter {\n  state n = 0\n  let shown = if n > 9 {\n    n = 0\n    0\n  } else {\n    n\n  }\n  Button(\"{shown}\") { on press { n += 1 } }\n}\n",
		Right: "component Counter {\n  state n = 0\n  Button(\"{n}\") { on press { n = if n >= 9 { 0 } else { n + 1 } } }\n}\n",
	})

	StatementInView = def(Code{
		Name:    "statement-in-view",
		Summary: "a statement among a component's views",
		Why: `A component's body is declarations: state, lets, and views that stay
true. A statement there would run once, or at no clear time; a match in a
view takes no guards.`,
		Fix:   "Put statements in a handler (`on press { … }`), a fn, a task or an effect; a derived value is a `let`.",
		Wrong: "component Counter {\n  state n = 0\n  Column {\n    n += 1\n    Text(\"{n}\")\n  }\n}\n",
		Right: "component Counter {\n  state n = 0\n  Column {\n    Button(\"Add\") { on press { n += 1 } }\n    Text(\"{n}\")\n  }\n}\n",
	})

	NotAView = def(Code{
		Name:    "not-a-view",
		Summary: "a value among views that isn't one",
		Why: `A body and an element's block show elements and components. A number
or a string is shown by an element that shows text.`,
		Fix:   "Show it in an element: `Text(\"{n}\")`.",
		Wrong: "component Answer {\n  Column {\n    42\n  }\n}\n",
		Right: "component Answer {\n  Column {\n    Text(\"42\")\n  }\n}\n",
	})

	UnknownProp = def(Code{
		Name:    "unknown-prop",
		Summary: "a prop, a slot or a variant the element doesn't have",
		Why: `An element takes the props it declares, and the style props when it
shows a view that takes them; a style holds style props only. A misspelt
prop would otherwise do nothing, silently.`,
		Fix:   "Use one of its props (the message suggests one).",
		Wrong: "component Title {\n  Text(\"Hello\") { colour: .red }\n}\n",
		Right: "component Title {\n  Text(\"Hello\") { color: .red }\n}\n",
	})

	UnknownEvent = def(Code{
		Name:    "unknown-event",
		Summary: "a handler or an `emit` of an event the component doesn't declare",
		Why: `` + "`on name { }`" + ` handles an event the element reports, and ` + "`emit name`" + ` raises
one its component declares (` + "`event name`" + `). One that isn't there would
never run.`,
		Fix:   "Use one of its events (the message suggests one), or declare it: `event name(…)`.",
		Wrong: "component Go {\n  Button(\"Go\") { on tap { print(\"go\") } }\n}\n",
		Right: "component Go {\n  Button(\"Go\") { on press { print(\"go\") } }\n}\n",
	})

	MissingProp = def(Code{
		Name:    "missing-prop",
		Summary: "an element without a prop it needs",
		Why:     "A prop without a default has no value until its caller gives one.",
		Fix:     "Set it in the element's block: `Badge { count: 3 }`, or give the prop a default.",
		Wrong:   "component Badge {\n  prop count: int\n  Text(\"{count}\")\n}\n\ncomponent Inbox {\n  Badge()\n}\n",
		Right:   "component Badge {\n  prop count: int\n  Text(\"{count}\")\n}\n\ncomponent Inbox {\n  Badge { count: 3 }\n}\n",
	})

	PropOrArgument = def(Code{
		Name:    "prop-or-argument",
		Summary: "a parameter, a prop, an event or children given in the wrong place",
		Why: `Each thing has one place: a header parameter in the parentheses, a prop
as ` + "`name: value`" + ` in the block, an event as ` + "`on name { }`" + `, children as the
block's views.`,
		Fix:   "Move it to its place: `Badge(3) { compact: true; on tap { … } }`.",
		Wrong: "component Badge {\n  prop count = 0\n  Text(\"{count}\")\n}\n\ncomponent Inbox {\n  Badge(count: 3)\n}\n",
		Right: "component Badge {\n  prop count = 0\n  Text(\"{count}\")\n}\n\ncomponent Inbox {\n  Badge { count: 3 }\n}\n",
	})

	NoChildren = def(Code{
		Name:    "no-children",
		Summary: "children given to an element that takes none, or not those",
		Why: `An element takes children when it declares ` + "`prop children: View`" + ` (or is a
container); an ` + "`Alert`" + `'s block holds its buttons and fields only, and a
` + "`List`" + `'s one ` + "`for`" + `, whose items are its rows.`,
		Fix:   "Put the views in a container (`Column`, `Row`), or beside the element. A row of another kind in a `List` (a header, a status) is an item of its `for`: an enum of the kinds of row.",
		Wrong: "component Note {\n  Text(\"Title\") {\n    Text(\"Body\")\n  }\n}\n",
		Right: "component Note {\n  Column {\n    Text(\"Title\")\n    Text(\"Body\")\n  }\n}\n",
	})

	NotBindable = def(Code{
		Name:    "not-bindable",
		Summary: "`bind` of a prop that doesn't report its changes",
		Why: `` + "`bind`" + ` passes state in and writes it when the element reports a change:
a control's own value (` + "`TextField`" + `'s ` + "`text`" + `, ` + "`Toggle`" + `'s ` + "`on`" + `), or a prop
whose component declares ` + "`event <prop>Change`" + `.`,
		Fix:   "Bind a control's value, or pass the value (`text: s`) and handle its event.",
		Wrong: "component Name {\n  state s = \"\"\n  Text(s) { bind text: s }\n}\n",
		Right: "component Name {\n  state s = \"\"\n  TextField { bind text: s }\n}\n",
	})

	InvalidVariant = def(Code{
		Name:    "invalid-variant",
		Summary: "a variant declared or used in a way it can't work",
		Why: `A variant is laid over the one element a component shows, while its
condition holds; it holds style props that change with it (not layout
that would move the views around it), and no variants inside it.`,
		Fix:   "Show one element (put the views in a `Row` or a `Column`), and set in a variant only what changes with it.",
		Wrong: "component Chip {\n  prop selected = false\n  variant selected = selected\n  Text(\"a\")\n  Text(\"b\")\n}\n",
		Right: "component Chip {\n  prop selected = false\n  variant selected = selected\n  Row {\n    Text(\"a\")\n    Text(\"b\")\n  }\n}\n",
	})

	InvalidStyle = def(Code{
		Name:    "invalid-style",
		Summary: "a style declared or applied in a form it doesn't take",
		Why: `A style holds style props, ` + "`style:`" + `, ` + "`use`" + ` and variants; ` + "`style:`" + ` is set
once, to a style or a list of them (` + "`style: [card, pink]`" + `), and a list
holds styles, an ` + "`if`" + ` or a ` + "`match`" + ` choosing them.`,
		Fix:   "Put the styles in one list: `style: [a, b]`.",
		Wrong: "style big {\n  font: .title\n}\n\nstyle red {\n  color: .red\n}\n\ncomponent Title {\n  Text(\"Hi\") {\n    style: big\n    style: red\n  }\n}\n",
		Right: "style big {\n  font: .title\n}\n\nstyle red {\n  color: .red\n}\n\ncomponent Title {\n  Text(\"Hi\") {\n    style: [big, red]\n  }\n}\n",
	})

	InvalidRef = def(Code{
		Name:    "invalid-ref",
		Summary: "`ref` or `as` naming what isn't one element's view",
		Why: `` + "`ref: x`" + ` and ` + "`as x`" + ` name an element's platform view, for what reads it
(its state, a scroll). A component that shows several views has no one
view to name.`,
		Fix:   "Name an element inside the component, or give the component one element to show.",
		Wrong: "component Pair {\n  Text(\"a\")\n  Text(\"b\")\n}\n\ncomponent Page {\n  Pair() as p\n}\n",
		Right: "component Pair {\n  Row {\n    Text(\"a\")\n    Text(\"b\")\n  }\n}\n\ncomponent Page {\n  Pair() as p\n}\n",
	})

	FollowsNothing = def(Code{
		Name:    "follows-nothing",
		Summary: "`state x()` with nothing to follow",
		Why: `The parentheses of ` + "`state x(a, b)`" + ` say what the state follows. Empty, it
would follow nothing, which is what a value read once (` + "`peek`" + `) already says.`,
		Fix:   "Name what to follow, `state x(a) = …`, or write `state x = peek(…)`.",
		Wrong: "component Counter {\n  state n() = 0\n  Text(\"{n}\")\n}\n",
		Right: "component Counter {\n  state n = 0\n  Text(\"{n}\")\n}\n",
	})
)

// Reactivity and threads.
var (
	EffectWritesWhatItFollows = def(Code{
		Name:    "effect-writes-what-it-follows",
		Summary: "an effect that writes state it reads",
		Why: `An effect runs again when what it read changes. Writing that itself, it
runs again after its own write, a pass late: a handler has already seen
the value it corrects, and the screen may show it for a frame.`,
		Fix: `Keep it in range where it's declared (` + "`state n: int = min(n ?? 0, 3)`" + `), follow
something else (` + "`effect(other) { … }`" + `, ` + "`state x(other) = …`" + `), or write it in
the handler that changes what it reads.`,
		Wrong: "component Counter {\n  state n = 0\n  effect {\n    if n > 3 { n = 3 }\n  }\n  Button(\"{n}\") { on press { n += 1 } }\n}\n",
		Right: "component Counter {\n  state n = 0\n  Button(\"{n}\") { on press { n = min(n + 1, 3) } }\n}\n",
	})

	CleanupReadsWhatItFollows = def(Code{
		Name:     "cleanup-reads-what-it-follows",
		Severity: Warning,
		Summary:  "an effect's cleanup that reads what the effect follows",
		Why: `An effect's ` + "`cleanup { }`" + ` runs before the effect's next run, which
happens because what it follows changed. Reading that in the cleanup
reads the new value: ` + "`cleanup { leave(room) }`" + ` leaves the room being
joined, not the one the run joined.`,
		Fix: `Keep the run's value in a ` + "`let`" + ` and read that in the cleanup:
` + "`let r = room`" + `, then ` + "`cleanup { leave(r) }`" + `.`,
		Wrong: "component Room {\n  prop room = \"lobby\"\n  effect(room) {\n    print(\"join {room}\")\n    cleanup { print(\"leave {room}\") }\n  }\n  Text(room)\n}\n",
		Right: "component Room {\n  prop room = \"lobby\"\n  effect(room) {\n    let r = room\n    print(\"join {r}\")\n    cleanup { print(\"leave {r}\") }\n  }\n  Text(room)\n}\n",
	})

	RowWithoutKey = def(Code{
		Name:    "row-without-key",
		Summary: "a `for` in a view over items that change, without a `key`",
		Why: `Without a key, rows are kept by position: when an item moves, the row
at its old place shows the next item, with the state, tasks and views of
the row that was there.`,
		Fix:   "Say what identifies an item: `for t in todos key t.id`. `key i` keeps the position on purpose.",
		Wrong: "struct Todo {\n  id: int\n  title: string\n}\n\ncomponent Todos {\n  state todos: [Todo] = []\n  Column {\n    for t in todos { Text(t.title) }\n  }\n}\n",
		Right: "struct Todo {\n  id: int\n  title: string\n}\n\ncomponent Todos {\n  state todos: [Todo] = []\n  Column {\n    for t in todos key t.id { Text(t.title) }\n  }\n}\n",
	})

	PeekUntracked = def(Code{
		Name:    "peek-untracked",
		Summary: "`peek` where nothing is followed",
		Why: `` + "`peek(x)`" + ` reads ` + "`x`" + ` without following it, in what's followed: a
binding, a ` + "`let`" + `, a state's value. A handler, a task, a test and an
` + "`effect(…)`" + `'s body follow nothing, so ` + "`peek`" + ` there says nothing.`,
		Fix:   "Read the value itself.",
		Wrong: "component Counter {\n  state n = 0\n  Button(\"{n}\") { on press { print(peek(n)) } }\n}\n",
		Right: "component Counter {\n  state n = 0\n  Button(\"{n}\") { on press { print(n) } }\n}\n",
	})

	ClosureKeepsItself = def(Code{
		Name:     "closure-keeps-itself",
		Severity: Warning,
		Summary:  "a closure that calls itself through the variable it's stored in",
		Why: `The closure holds the variable, and the variable holds the closure: a
cycle of references, which is never freed.`,
		Fix:   "Declare a function in the block instead, `fn name(…) { … }`: it can call itself.",
		Wrong: "fn countdown(n: int) -> int {\n  var step: fn(int) -> int = x => x\n  step = x => if x <= 0 { 0 } else { step(x - 1) }\n  return step(n)\n}\n",
		Right: "fn countdown(n: int) -> int {\n  fn step(x: int) -> int = if x <= 0 { 0 } else { step(x - 1) }\n  return step(n)\n}\n",
	})

	SpawnCapturesState = def(Code{
		Name:    "spawn-captures-state",
		Summary: "`spawn` that reads or changes state, or what isn't a value",
		Why: `` + "`spawn`" + ` runs its block on another thread, while the main thread goes on
changing state and a file's vars; it takes values only, copied as it
starts, and gives a value back.`,
		Fix:   "Read the state into a `let` first, and pass that; assign what `spawn` returns.",
		Wrong: "component Total {\n  state items: [int] = []\n  state sum = 0\n  task {\n    sum = await spawn { items.count() }\n  }\n  Text(\"{sum}\")\n}\n",
		Right: "component Total {\n  state items: [int] = []\n  state sum = 0\n  task {\n    let xs = items\n    sum = await spawn { xs.count() }\n  }\n  Text(\"{sum}\")\n}\n",
	})

	WorkerCapturesState = def(Code{
		Name:    "worker-captures-state",
		Summary: "a worker that reads the main thread's state",
		Why: `A worker runs on a thread of its own; state and a file's vars live on
the main thread and change there. Its fns take what they need as
values.`,
		Fix:   "Read the state (or the var) on the main thread and pass its value to a `pub fn`.",
		Wrong: "state limit = 10\n\nworker Clamp {\n  pub fn cap(n: int) -> int = min(n, limit)\n}\n",
		Right: "worker Clamp {\n  pub fn cap(n: int, limit: int) -> int = min(n, limit)\n}\n",
	})

	NotSendable = def(Code{
		Name:    "not-sendable",
		Summary: "a value that can't cross to another thread",
		Why: `What goes to a worker or comes back from ` + "`spawn`" + ` is copied as a value:
numbers, strings, arrays, structs of them. A closure, a store, a view or
a native object belongs to the thread that made it.`,
		Fix:   "Pass the data the other thread needs, not the object that has it.",
		Wrong: "worker Runner {\n  pub fn run(f: fn() -> int) -> int = f()\n}\n",
		Right: "worker Runner {\n  pub fn twice(n: int) -> int = n * 2\n}\n",
	})

	WorkerMisuse = def(Code{
		Name:    "worker-misuse",
		Summary: "a worker's own values read from outside, or a worker made or called where it can't be",
		Why: `A worker's vars are read by its fns, on its thread; its ` + "`pub fn`" + `s are
called from the main thread, and it's made there, by what owns it.`,
		Fix:   "Return the value from a `pub fn`, and call it with `await`.",
		Wrong: "worker Tally {\n  var n = 0\n  pub fn add() { n += 1 }\n}\n\nfn count(t: Tally) -> int = t.n\n",
		Right: "worker Tally {\n  var n = 0\n  pub fn add() { n += 1 }\n  pub fn count() -> int = n\n}\n\nasync fn count(t: Tally) -> int = await t.count()\n",
	})

	LoadOutsideComponent = def(Code{
		Name:    "load-outside-component",
		Summary: "`load` outside a component's or a store's `let`",
		Why: `` + "`load`" + ` keeps an async result for as long as what declares it is shown,
running again when its arguments change: a component's or a store's
` + "`let`" + `. A function has no such lifetime.`,
		Fix:   "Declare `let x = load f(…)` in the component, or `await` the call in the function.",
		Wrong: "async fn fetchName() -> string {\n  await sleep(1ms)\n  return \"Ana\"\n}\n\nfn name() {\n  let n = load fetchName()\n}\n",
		Right: "async fn fetchName() -> string {\n  await sleep(1ms)\n  return \"Ana\"\n}\n\ncomponent Name {\n  let n = load fetchName()\n  Text(n.value ?? \"…\")\n}\n",
	})

	OptimisticNotLoading = def(Code{
		Name:    "optimistic-not-loading",
		Summary: "`optimistic` on what isn't a load",
		Why: `` + "`x.optimistic(f)`" + ` shows a change of what a load shows until the load's next
answer replaces it: it's a layer over the source of truth. A state is
its own source of truth, with nothing coming to replace a change.`,
		Fix:   "Change the state itself (`notes.push(n)`), or call `optimistic` on the load's `let`.",
		Wrong: "struct Note {\n  title: string\n}\n\ncomponent Notes {\n  state notes: [Note] = []\n  Column {\n    for n in notes key n.title {\n      Text(n.title)\n    }\n    Button(\"Add\") {\n      on press { notes.optimistic(ns => ns + [Note(title: \"new\")]) }\n    }\n  }\n}\n",
		Right: "struct Note {\n  title: string\n}\n\ncomponent Notes {\n  state notes: [Note] = []\n  Column {\n    for n in notes key n.title {\n      Text(n.title)\n    }\n    Button(\"Add\") {\n      on press { notes.push(Note(title: \"new\")) }\n    }\n  }\n}\n",
	})

	ReloadNotLoaded = def(Code{
		Name:    "reload-not-loaded",
		Summary: "`reload` of what no load calls",
		Why: `` + "`reload f(…)`" + ` asks again the loads whose operand is a call of ` + "`f`" + `: the
call is the key, by the function it calls. A function no ` + "`load`" + ` in the
program calls, or what isn't a function (a closure, a value's method),
reaches nothing.`,
		Fix:   "Reload the function a `load` calls (`reload api.notes`), or a load by its handle (`x.reload()`).",
		Wrong: "async fn fetchNotes() -> [string] {\n  await sleep(1ms)\n  return []\n}\n\nasync fn fetchTags() -> [string] {\n  await sleep(1ms)\n  return []\n}\n\ncomponent Notes {\n  let notes = load fetchNotes()\n  Button(\"Refresh\") {\n    on press { await reload fetchTags() }\n  }\n}\n",
		Right: "async fn fetchNotes() -> [string] {\n  await sleep(1ms)\n  return []\n}\n\ncomponent Notes {\n  let notes = load fetchNotes()\n  Button(\"Refresh\") {\n    on press { await reload fetchNotes() }\n  }\n}\n",
	})

	ReloadArguments = def(Code{
		Name:    "reload-arguments",
		Summary: "`reload f(…)` with arguments `f` doesn't take",
		Why: `` + "`reload f(a)`" + ` reaches the loads of ` + "`f`" + ` whose arguments equal ` + "`a`" + `: they're
compared as ` + "`f`" + `'s parameters, so they're checked as a call of ` + "`f`" + ` is.`,
		Fix:   "Give the arguments `f` takes, as its loads call it, or none: `reload f` reaches every load of `f`.",
		Wrong: "async fn fetchNote(id: int) -> string {\n  await sleep(1ms)\n  return \"note {id}\"\n}\n\ncomponent Note(id: int) {\n  let note = load fetchNote(id)\n  Button(\"Refresh\") {\n    on press { await reload fetchNote(\"{id}\") }\n  }\n}\n",
		Right: "async fn fetchNote(id: int) -> string {\n  await sleep(1ms)\n  return \"note {id}\"\n}\n\ncomponent Note(id: int) {\n  let note = load fetchNote(id)\n  Button(\"Refresh\") {\n    on press { await reload fetchNote(id) }\n  }\n}\n",
	})

	GroupNotCalls = def(Code{
		Name:    "group-not-calls",
		Summary: "`load (…)` of what isn't each a call",
		Why: `` + "`load (a(), b())`" + ` is one load of several calls, each run again only when
what its arguments read changes, their answers shown together. A value
in it has nothing to run or to wait for.`,
		Fix:   "Group the calls only (`load (api.user(id), api.posts(id))`), and pass the values to them, or beside the load.",
		Wrong: "async fn fetchUser(id: int) -> string {\n  await sleep(1ms)\n  return \"user {id}\"\n}\n\ncomponent Profile(id: int) {\n  let page = load (fetchUser(id), id)\n  Text(\"{page.loading}\")\n}\n",
		Right: "async fn fetchUser(id: int) -> string {\n  await sleep(1ms)\n  return \"user {id}\"\n}\n\nasync fn fetchPosts(id: int) -> [string] {\n  await sleep(1ms)\n  return []\n}\n\ncomponent Profile(id: int) {\n  let page = load (fetchUser(id), fetchPosts(id))\n  Text(\"{page.loading}\")\n}\n",
	})

	OptimisticImpure = def(Code{
		Name:    "optimistic-impure",
		Summary: "an optimistic change that reads state, awaits or writes",
		Why: `An optimistic change runs again whenever what's under it changes: the
load's next answer, a change below it that goes. It must give the same
change each time, from what it's given: a function of the value shown,
as a binding is. What it reads of state may be different by then, and
what it writes would be written again.`,
		Fix: "Read what it needs before, into a `let`, and use that in the closure; write in the handler, not in the change.",
		Wrong: `struct Todo {
  id: int
  done: bool
}

async fn fetchTodos() -> [Todo] {
  await sleep(1ms)
  return [Todo(id: 1, done: false)]
}

fn marked(t: Todo) -> Todo {
  var c = t
  c.done = true
  return c
}

component Todos {
  state picked = 1
  let todos = load fetchTodos()
  Button("Done") {
    on press {
      todos.optimistic(ts => ts.map(t => if t.id == picked { marked(t) } else { t }))
      todos.reload()
    }
  }
}
`,
		Right: `struct Todo {
  id: int
  done: bool
}

async fn fetchTodos() -> [Todo] {
  await sleep(1ms)
  return [Todo(id: 1, done: false)]
}

fn marked(t: Todo) -> Todo {
  var c = t
  c.done = true
  return c
}

component Todos {
  state picked = 1
  let todos = load fetchTodos()
  Button("Done") {
    on press {
      let id = picked
      todos.optimistic(ts => ts.map(t => if t.id == id { marked(t) } else { t }))
      todos.reload()
    }
  }
}
`,
	})
)

// Data, platforms and packages.
var (
	NotJSON = def(Code{
		Name:    "not-json",
		Summary: "a type JSON can't read or write",
		Why: `JSON holds numbers, strings, bools, arrays, objects and null; a struct
of those, an optional, an enum without payloads map to them. A closure,
a view or a store has no JSON.`,
		Fix:   "Decode into a struct of data, and make the rest from it.",
		Wrong: "import std/json\n\nstruct Settings {\n  onChange: fn()\n}\n\nfn read(text: string) throws -> Settings = try json.decode<Settings>(text)\n",
		Right: "import std/json\n\nstruct Settings {\n  theme: string\n}\n\nfn read(text: string) throws -> Settings = try json.decode<Settings>(text)\n",
	})

	TestParameter = def(Code{
		Name:    "test-parameter",
		Summary: "a test's parameter of a type asuka test can't generate",
		Why: `A test with parameters is a property test: ` + "`asuka test`" + ` runs it with values it
generates for them, shrinks a failing one to the simplest it can find, and
` + "`--fuzz`" + ` saves it as JSON, which every later run reads back. It generates
ints, floats, bools, strings, bytes and durations, and the arrays, maps
from strings, sets, optionals, structs and enums made of them. A view, a
closure or a store has no values to generate, and a parameter takes no
default: it gets each generated value.`,
		Fix:   "Take what the test is about as data (an id, a struct of fields), and make the rest from it in the test's body.",
		Wrong: "test \"runs anything\" (f: fn() -> int) {\n  expect(f() == f())\n}\n",
		Right: "test \"reversing twice\" (xs: [int]) {\n  expect(xs.reversed().reversed() == xs)\n}\n",
	})

	InlineSnapshot = def(Code{
		Name:    "inline-snapshot",
		Summary: "an inline snapshot that isn't a string written in the call",
		Why:     "`expectInlineSnapshot(value, \"…\")` compares what the value prints as with the string in the call, and `asuka test --update` rewrites that string in the source when the value changes. A string the program computes, or one with interpolations, isn't text the update can rewrite.",
		Fix:     "Write the snapshot as a string literal, or leave it out (`expectInlineSnapshot(value)`) and let `asuka test` write it on its first run.",
		Wrong:   "fn total() -> int = 42\n\ntest \"total\" {\n  let want = \"42\"\n  expectInlineSnapshot(total(), want)\n}\n",
		Right:   "fn total() -> int = 42\n\ntest \"total\" {\n  expectInlineSnapshot(total(), \"42\")\n}\n",
	})

	PreviewSetting = def(Code{
		Name:    "preview-setting",
		Summary: "a preview's setting it doesn't take, or not a value written as it is",
		Why: `A preview's settings say how ` + "`asuka dev`" + ` shows it: ` + "`dark`" + ` (a bool), ` + "`fontScale`" + `
(the text size, a multiple of the default, from 0.5 to 4), ` + "`lang`" + ` (a
language tag), ` + "`rtl`" + ` (a bool) and ` + "`width`" + ` and ` + "`height`" + ` (a frame's size, in
points). The player applies them before the preview's code runs, so each
is a value written as it is, not one the program computes; and a preview
is listed by its name, a string without interpolations.`,
		Fix:   "Name a setting the preview takes, and write its value as a literal: `(dark: true, fontScale: 1.5, lang: \"ar\")`.",
		Wrong: "component Card {\n  Text(\"Hi\")\n}\n\nlet big = 1.5\n\npreview \"Large text\" (fontScale: big, theme: .dark) {\n  Card()\n}\n",
		Right: "component Card {\n  Text(\"Hi\")\n}\n\npreview \"Large text, dark\" (fontScale: 1.5, dark: true) {\n  Card()\n}\n",
	})

	NotCType = def(Code{
		Name:    "not-c-type",
		Summary: "a type that can't cross to C",
		Why: `An extern declaration's types are C's: ` + "`Int8`" + `…` + "`Int64`" + `, ` + "`UInt8`" + `…
` + "`UInt64`" + `, ` + "`int`" + `, ` + "`Float32`" + `, ` + "`float`" + `, ` + "`bool`" + `, ` + "`CString`" + `, ` + "`bytes`" + `,
` + "`Pointer`" + `, extern structs and types. Asuka's arrays, maps, tuples and
structs have no C layout.`,
		Fix:   "Pass `bytes` or a `Pointer` and a length, or declare an `extern struct`.",
		Wrong: "extern fn sum(xs: [int]) -> int\n",
		Right: "extern fn sum(xs: bytes) -> int\n",
	})

	InvalidExtern = def(Code{
		Name:    "invalid-extern",
		Summary: "an extern declaration C can't stand behind",
		Why: `An extern fn is a C function's name; an extern type's ` + "`release`" + ` is an
extern fn of the package that takes one of it; an extern struct is C's
layout, with nothing to declare.`,
		Fix:   "Declare the release function `extern fn close(x: T)`, and the rest as C has it.",
		Wrong: "extern type Handle with release close\n\nfn close(h: Handle) {}\n",
		Right: "extern type Handle with release close\n\nextern fn close(h: Handle)\n",
	})

	NativeType = def(Code{
		Name:    "native-type",
		Summary: "a native component's prop or event of a type the platform's view can't take",
		Why: `A native component's props and events cross to Swift, Kotlin, C and
JavaScript as values. A closure or a store doesn't cross.`,
		Fix:   "Use data the platform's code can read; report what happens with an event.",
		Wrong: "native component Dial {\n  prop format: fn(int) -> string\n}\n",
		Right: "native component Dial {\n  prop value: int\n  event change(value: int)\n}\n",
	})

	EmbedFile = def(Code{
		Name:    "embed-file",
		Summary: "`embed` of a file the package doesn't have",
		Why: `` + "`embed.text(path)`" + ` and its kin read a file into the app as it compiles:
the path is a string literal, relative to the package's directory, inside
it.`,
		Fix:   "Fix the path, or add the file to the package.",
		Wrong: "-- app.asuka --\nimport std/embed\n\nlet terms = embed.text(\"terms.md\")\n",
		Right: "-- app.asuka --\nimport std/embed\n\nlet terms = embed.text(\"terms.md\")\n-- terms.md --\n# Terms\n",
	})

	InvalidRoute = def(Code{
		Name:    "invalid-route",
		Summary: "a screen's address or a navigation the routes can't make",
		Why: `A routed screen's pattern is a string literal whose ` + "`:name`" + `s are its
parameters, of types an address says (ints, floats, bools, strings,
enums' cases). Navigating to a screen needs exactly one route to it.`,
		Fix:   "Name the screen's parameters in its pattern, as it declares them.",
		Wrong: "screen Profile(id: int) at \"/users/:name\" {\n  Text(\"{id}\")\n}\n",
		Right: "screen Profile(id: int) at \"/users/:id\" {\n  Text(\"{id}\")\n}\n",
	})

	InvalidShader = def(Code{
		Name:    "invalid-shader",
		Summary: "a shader written with what the GPU's language doesn't have",
		Why: `A shader runs on the GPU: its values are numbers, vectors and matrices,
its parameters uniforms (a float, an int, a bool or a Color), its code
statements over them, and its entry ` + "`fn fragment(pos: vec2) -> vec4`" + `.`,
		Fix:   "Write it with numbers, vectors and GLSL's functions.",
		Wrong: "shader Glow(label: string) {\n  fn fragment(pos: vec2) -> vec4 {\n    return vec4(1.0, 1.0, 1.0, 1.0)\n  }\n}\n",
		Right: "shader Glow(strength: float) {\n  fn fragment(pos: vec2) -> vec4 {\n    return vec4(strength, strength, strength, 1.0)\n  }\n}\n",
	})

	UnknownMessage = def(Code{
		Name:    "unknown-message",
		Summary: "`tr.key` of a message the catalogs don't have",
		Why: `` + "`tr`" + ` says the app's messages, one per key of the source language's
catalog (` + "`locales/en.json`" + `). A key that isn't there has no text in any
language.`,
		Fix:   "Use a key of the catalog (the message suggests one), or add it there.",
		Wrong: "-- app.asuka --\ncomponent Title {\n  Text(tr.helo())\n}\n-- locales/en.json --\n{\"hello\": \"Hello\"}\n",
		Right: "-- app.asuka --\ncomponent Title {\n  Text(tr.hello())\n}\n-- locales/en.json --\n{\"hello\": \"Hello\"}\n",
	})

	MessageArguments = def(Code{
		Name:    "message-arguments",
		Summary: "a message called with other arguments than its placeholders",
		Why: `A message's arguments are named after its placeholders
(` + "`\"Hello, {name}\"`" + ` takes ` + "`name:`" + `), each given once, of the type the
catalog says.`,
		Fix:   "Pass each placeholder by its name: `tr.greeting(name: n)`.",
		Wrong: "-- app.asuka --\ncomponent Hi {\n  Text(tr.greeting())\n}\n-- locales/en.json --\n{\"greeting\": \"Hello, {name}\"}\n",
		Right: "-- app.asuka --\ncomponent Hi {\n  Text(tr.greeting(name: \"Ana\"))\n}\n-- locales/en.json --\n{\"greeting\": \"Hello, {name}\"}\n",
	})

	TrInPackage = def(Code{
		Name:    "tr-in-package",
		Summary: "`tr` in a package that isn't the app",
		Why: `The catalogs are the app's: a library can't know what messages the apps
that use it have.`,
		Fix:   "Take the text as a parameter, and let the app pass `tr.key()`.",
		Wrong: "-- asuka.toml --\nname = \"home\"\n\n[dependencies]\n\"@acme/ui\" = \"file:./ui\"\n-- app.asuka --\nimport @acme/ui\n\ncomponent Home {\n  ui.Header()\n}\n-- ui/asuka.toml --\nname = \"@acme/ui\"\nversion = \"1.0.0\"\n-- ui/ui.asuka --\npub component Header {\n  Text(tr.title())\n}\n-- locales/en.json --\n{\"title\": \"Home\"}\n",
		Right: "-- asuka.toml --\nname = \"home\"\n\n[dependencies]\n\"@acme/ui\" = \"file:./ui\"\n-- app.asuka --\nimport @acme/ui\n\ncomponent Home {\n  ui.Header(tr.title())\n}\n-- ui/asuka.toml --\nname = \"@acme/ui\"\nversion = \"1.0.0\"\n-- ui/ui.asuka --\npub component Header(title: string) {\n  Text(title)\n}\n-- locales/en.json --\n{\"title\": \"Home\"}\n",
	})

	ImportCycle = def(Code{
		Name:    "import-cycle",
		Summary: "packages that import each other",
		Why: `A package is checked after what it imports, so two that import each
other, directly or through others, have no order to be checked in.`,
		Fix:   "Move what both need into a third package they both import.",
		Wrong: "-- app.asuka --\nimport ./a\n\nlet n = a.one()\n-- a/a.asuka --\nimport ../b\n\npub fn one() -> int = 1\npub fn two() -> int = b.two()\n-- b/b.asuka --\nimport ../a\n\npub fn two() -> int = a.one() + 1\n",
		Right: "-- app.asuka --\nimport ./a\n\nlet n = a.one()\n-- a/a.asuka --\nimport ../b\n\npub fn one() -> int = 1\npub fn two() -> int = b.two()\n-- b/b.asuka --\npub fn two() -> int = 2\n",
	})

	InvalidCatalog = def(Code{
		Name:    "invalid-catalog",
		Summary: "a catalog that isn't a valid file of messages",
		Why: `A catalog (` + "`locales/<language>.json`" + `) is a JSON object of messages:
each key a name, each value an ICU message whose arguments are the
source language's.`,
		Fix:   "Fix the file where the message points: a string for each key, its placeholders as the source's.",
		Wrong: "-- app.asuka --\ncomponent Title {\n  Text(tr.hello())\n}\n-- locales/en.json --\n{\"hello\": 1}\n",
		Right: "-- app.asuka --\ncomponent Title {\n  Text(tr.hello())\n}\n-- locales/en.json --\n{\"hello\": \"Hello\"}\n",
	})

	MissingTranslation = def(Code{
		Name:     "missing-translation",
		Severity: Warning,
		Summary:  "a language's catalog without some of the source's messages",
		Why: `A message a language lacks is shown in the language it falls back to.
That works, and is worth knowing.`,
		Fix:   "Translate the messages it lists, in that language's catalog.",
		Wrong: "-- app.asuka --\ncomponent Title {\n  Text(tr.hello())\n}\n-- locales/en.json --\n{\"hello\": \"Hello\", \"bye\": \"Bye\"}\n-- locales/de.json --\n{\"hello\": \"Hallo\"}\n",
		Right: "-- app.asuka --\ncomponent Title {\n  Text(tr.hello())\n}\n-- locales/en.json --\n{\"hello\": \"Hello\", \"bye\": \"Bye\"}\n-- locales/de.json --\n{\"hello\": \"Hallo\", \"bye\": \"Tschüss\"}\n",
	})

	UnusedCatalogEntry = def(Code{
		Name:     "unused-catalog-entry",
		Severity: Warning,
		Summary:  "a catalog's description or argument that nothing uses",
		Why: `A description (` + "`@key`" + `) of a message that isn't there, a placeholder a
message doesn't use, or a translation that leaves out the source's
argument is a leftover or a mistake.`,
		Fix:   "Remove it, or use the argument in the message.",
		Wrong: "-- app.asuka --\ncomponent Title {\n  Text(tr.hello())\n}\n-- locales/en.json --\n{\"hello\": \"Hello\", \"@bye\": {\"description\": \"Leaving\"}}\n",
		Right: "-- app.asuka --\ncomponent Title {\n  Text(tr.hello())\n}\n-- locales/en.json --\n{\"hello\": \"Hello\", \"@hello\": {\"description\": \"A greeting\"}}\n",
	})

	PluralCategories = def(Code{
		Name:     "plural-categories",
		Severity: Warning,
		Summary:  "a plural or select message whose categories don't match its language's",
		Why: `Each language has its plural categories (English ` + "`one`" + ` and ` + "`other`" + `,
Arabic six). A category the language doesn't have is never chosen, and
one it lacks falls back to ` + "`other`" + `.`,
		Fix:   "Write the language's categories, as the message lists them.",
		Wrong: "-- app.asuka --\ncomponent Count {\n  Text(tr.items(count: 2))\n}\n-- locales/en.json --\n{\"items\": \"{count, plural, one {# item} few {# items} other {# items}}\"}\n",
		Right: "-- app.asuka --\ncomponent Count {\n  Text(tr.items(count: 2))\n}\n-- locales/en.json --\n{\"items\": \"{count, plural, one {# item} other {# items}}\"}\n",
	})

	InvalidFlagSpec = def(Code{
		Name:    "invalid-flag-spec",
		Summary: "a type `flag.parse` can't read a command line into",
		Why: `std/flag reads a command line from a struct's fields: one with a default
is an option, one without an argument; a ` + "`bool`" + ` is a switch, so it has a
default.`,
		Fix:   "Give each switch a default (`verbose: bool = false`), and options the types the message lists.",
		Wrong: "import std/flag\nimport std/os\n\nstruct Options {\n  verbose: bool\n}\n\nfn main() {\n  let o = flag.parse<Options>(os.args())\n  print(o.verbose)\n}\n",
		Right: "import std/flag\nimport std/os\n\nstruct Options {\n  verbose: bool = false\n}\n\nfn main() {\n  let o = flag.parse<Options>(os.args())\n  print(o.verbose)\n}\n",
	})

	InvalidConstant = def(Code{
		Name:    "invalid-constant",
		Summary: "a value made when the app compiles that isn't a literal, or doesn't parse",
		Why: `Some values are made when the app compiles, from a string literal: an
SVG icon from its markup, a shader's program from its source. Text made
at run time can't be, and markup that doesn't parse has nothing to make.`,
		Fix:   "Pass the literal itself, and fix what the message says doesn't parse.",
		Wrong: "import std/svg\n\nlet icon = svg.Icon(\"<svg\")\n",
		Right: "import std/svg\n\nlet icon = svg.Icon(r\"\"\"<svg viewBox=\"0 0 24 24\"><path d=\"M12 3v12\"/></svg>\"\"\")\n",
	})

	TooLarge = def(Code{
		Name:    "too-large",
		Summary: "a function too large for the bytecode",
		Why: `An instruction names at most 65,535 values, and a function holds at most
65,534 at once. A literal or a function past that can't be encoded.`,
		Fix: "Split the function, or keep the data in a file (`embed.json`) instead of a literal.",
		// Made when it's read, not as the CLI starts: 70,000 lines.
		wrong: sync.OnceValue(func() string { return locals(70000) }),
		Right: locals(3),
	})

	InvalidProject = def(Code{
		Name:    "invalid-project",
		Summary: "the project's asuka.toml or directory can't be read as it says",
		Why: `asuka.toml says what the app is (its accent color, its catalogs, its
dependencies); a value in the wrong form there can't be used.`,
		Fix:   "Fix the value in asuka.toml as the message says.",
		Wrong: "-- asuka.toml --\nname = \"Notes\"\naccent = \"purple\"\n-- app.asuka --\ncomponent Home {\n  Text(\"Notes\")\n}\n\napp { Home() }\n",
		Right: "-- asuka.toml --\nname = \"Notes\"\naccent = \"#7c3aed\"\n-- app.asuka --\ncomponent Home {\n  Text(\"Notes\")\n}\n\napp { Home() }\n",
	})

	InternalError = def(Code{
		Name:     "internal-error",
		Internal: true,
		Summary:  "a mistake of the compiler's own",
		Why: `The compiler reached a state it doesn't expect: the program is fine, or
its mistake was reported in a way the compiler then tripped over.`,
		Fix: "Report it (with the program, or the smallest one that shows it) at https://github.com/egoist/asuka/issues.",
	})
)

// locals is a function that holds n values at once.
func locals(n int) string {
	var b strings.Builder
	b.WriteString("fn sum() -> int {\n")
	for i := range n {
		fmt.Fprintf(&b, "  let v%d = %d\n", i, i)
	}
	fmt.Fprintf(&b, "  return v0 + v%d\n}\n\nlet total = sum()\n", n-1)
	return b.String()
}
