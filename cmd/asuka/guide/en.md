# Asuka, on one page

This is the Asuka language as the installed CLI compiles it, and how to work with it. Every error and warning has a code; `asuka explain <code>` prints its page.

## Workflow

- Check: `asuka check --json` (one diagnostic per line, each with a code, its message, its help and its page)
- Look up a code: `asuka explain <code>`
- Test: `asuka test --json` (each test's result, and what the core reports as the app runs); run: `asuka dev`, or `asuka run` for a program
- Format: `asuka fmt -w`
- A component's body is declarations; state is written in handlers, fns, tasks and effects. Lists in views need `key`.

## The language

### A tour of Asuka

Asuka apps are written in Asuka, a small language made for interfaces
that use each platform's own widgets. It's statically typed, compiles
ahead of time for release builds, and reloads in place as you save.
This page shows the language in the order you meet it; the [language
reference](https://asuka.egoist.dev/docs/language) has the whole of each part.

```asuka
component Counter {
  state count = 0

  Column {
    gap: 12; padding: 24
    Text("Count: {count}") { font: .title }
    Button("Add") { on press { count += 1 } }
  }
}

app { Counter() }
```

A file is declarations: components, functions, types, state. `app { }`
is the app's root; the rest is in any order.

#### Values

```asuka
fn values() -> string {
  let name = "Asuka"               // a constant: its type is inferred
  var visits = 0                   // a variable
  visits += 1
  let greeting = "Hello, {name}"   // {…} puts a value in a string
  let ratio: float = 2.5
  let level: u8 = 200              // sized integers: i8 … i32, u8 … u64 (int is 64 bits)
  let area = 2 ** 10               // a power: 1024
  let tags = ["new", "work"]       // an array: [string]
  let ages = ["ana": 31, "bo": 28] // a map: [string: int]
  let nickname: string? = none     // an optional: a string or none
  let shown = nickname ?? name     // none's fallback
  return "{greeting} {visits} {ratio} {level} {area} {tags.count()} {ages["ana"] ?? 0} {shown}"
}
```

`var` is for functions and handlers: what's at a file's top level
doesn't change, unless it's `state`. Numbers of different types don't
mix: `int(level) + visits` converts the `u8` first, and arithmetic past
a type's range is an error, not a wrapped number. Values are copied, never shared:
changing a copy of an array leaves the original as it was. Strings, arrays and maps have the methods you'd
expect (`count()`, `contains`, `map`, `filter`, `sorted`, `split`...):
[Values and types](https://asuka.egoist.dev/docs/language/types).

#### Functions

```asuka
fn area(width: float, height: float = 1) -> float {
  return width * height
}

fn double(x: int) -> int = x * 2     // one expression

let a = area(3, height: 4)           // in order, then by name
let squares = [1, 2, 3].map(x => x * x)
```

Arguments come in order, then by name (`area(3, height: 4)`); a
parameter with a default may be left out. `x => …` is a closure. [Functions and
closures](https://asuka.egoist.dev/docs/language/functions)

#### Structs and enums

```asuka
struct Message {
  subject: string
  read: bool = false

  fn preview() -> string = subject.slice(0, 40)
}

enum Filter {
  all
  unread
  from(sender: string)
}

fn label(f: Filter) -> string = match f {
  .all => "Everything"
  .unread => "Unread"
  .from(sender) => "From {sender}"
}

let m = Message(subject: "Hello")
let n = label(.from(sender: "ana"))

struct Counter {
  n: int = 0

  mut fn bump() { n += 1 }
}

extend string {
  fn shout() -> string = upper() + "!"
}

fn bumped() -> string {
  var c = Counter()
  c.bump()
  return "{c.n}".shout()
}
```

A struct is fields and methods; a `mut fn` changes the value it's
called on (a `var`, or state). `extend` adds methods and constants to
any type, `string` and `Color` included. An enum's cases can carry
values, and `match` covers every case (the compiler says which one is
missing).
`if`, `match` and blocks are values too. [Declarations](https://asuka.egoist.dev/docs/language/declarations),
[Control flow](https://asuka.egoist.dev/docs/language/control-flow)

#### Components and state

```asuka
component Greeting(name: string) {
  prop excited = false                // a prop, with its default
  state draft = ""                    // the component's own state

  Column {
    gap: 8
    Text(if excited { "Hi, {name}!" } else { "Hi, {name}" })
    TextField { bind text: draft; placeholder: "Reply" }
    if !draft.isEmpty() {
      Button("Send") { on press { draft = "" } }
    }
  }
}

app { Greeting("Ana") { excited: true } }
```

A component's header takes its parameters; its block holds its props,
its state and its views. What reads state is updated when it changes,
and only that: no diffing. A state made from what changes
(`state draft = name`) follows it until it's written; `peek(name)`
reads it once. `bind` makes a two-way binding, `on press`
handles an event, and `if` and `for` decide which views there are.
[Components](https://asuka.egoist.dev/docs/language/components), [Stores](https://asuka.egoist.dev/docs/language/stores) for
state several components share.

#### Async and errors

```asuka
enum LoadError: Error {
  offline
  server(code: int)
}

async fn fetchCount(online: bool) throws -> int {
  if !online { throw LoadError.offline }
  await sleep(1s)
  return 42
}

component Badge {
  state count = 0
  state problem = ""

  task {
    do {
      count = try await fetchCount(online: true)
    } catch LoadError.offline {
      problem = "You're offline"
    } catch e {
      problem = e.message
    }
  }

  Text(if problem.isEmpty() { "{count}" } else { problem })
}
```

`async` work runs without holding the screen up, and `task { }` belongs
to the component that started it: it's cancelled when the component
goes. A function that can fail says `throws`, and its callers `try` it
(`try?` for none on an error). [Async](https://asuka.egoist.dev/docs/language/async),
[Errors](https://asuka.egoist.dev/docs/language/errors)

What a component shows from such a call is a `load`: `let n = load
fetchCount(online: true)` is `.pending`, `.ready(count)` or
`.failed(e)`, and keeps what it shows while it asks again.
`n.optimistic(f)` shows a change at once, `reload fetchCount` asks every
load of it again, wherever it is, and `load (a(), b())` lands several
calls together. [Data that loads](https://asuka.egoist.dev/docs/language/async#data-that-loads)

#### Packages

```asuka
import std/time

let today = time.now().format("EEE d MMM")
```

`pub` makes a declaration visible to other packages. The standard
library (`std/…`) and the platform SDK (`platform/…`) come with the
CLI: import a package and use it. A file named `tray_linux.asuka` is
part of its package only on Linux, and `@platform == .linux` is a
constant, so what's for one platform isn't in the others' builds;
`@debug`, true under `asuka dev`, leaves what's for development out of a
release.
[Files, packages and imports](https://asuka.egoist.dev/docs/language/packages), [the standard
library and the platform SDK](https://asuka.egoist.dev/docs/stdlib), [code for one
platform](https://asuka.egoist.dev/docs/language/control-flow#code-for-one-platform), [code for
development builds](https://asuka.egoist.dev/docs/language/control-flow#code-for-development-builds)

#### Tests

```asuka
component Counter {
  state count = 0
  Button("Add {count}") { on press { count += 1 } }
}

test "adding counts" {
  let app = mount(Counter())
  app.tap("Add 0")
  app.expect("Add 1")
}
```

`asuka test` runs a package's `test` blocks with no device: `mount`
shows a view, and the test taps, types and checks what it shows.
[Tests](https://asuka.egoist.dev/docs/language/tests)

#### Next

- [The language reference](https://asuka.egoist.dev/docs/language): each part in full,
  from types and their methods to styles, navigation and native code.
- [Components](https://asuka.egoist.dev/docs/components): the elements apps are made of.
- [Concepts](https://asuka.egoist.dev/docs/concepts): what updates, owners, the main thread.

### Files, packages and imports

A source file ends in `.asuka`. A directory of them is a **package**:
its files see each other's declarations without importing anything.
`pub` makes a declaration visible to other packages, and `private` keeps
one to its file ([What's private](https://asuka.egoist.dev/docs/language/declarations#whats-private)).

```asuka
import std/http                 // the standard library
import platform/camera          // the platform SDK
import @acme/charts               // a dependency (asuka add @acme/charts)
import ./components               // a package beside this one
import ./model.{Message, Folder}  // names used without the package's name
import ./theme as t               // a shorter name
import ../model                   // a package a directory up
```

A package's names are used through it: `http.get(url)`,
`components.Card("Hi")`. A package is named by its path's last segment;
one whose name isn't a name (`@acme/line-charts`) is imported with
`as`. The prelude (the built-in types, elements and functions) needs no
import. [The standard library and the platform SDK](https://asuka.egoist.dev/docs/stdlib) lists
the `std/...` and `platform/...` packages, which come with the CLI; no
other package can be named under `std/` or `platform/`.

Statements and declarations end at the end of a line; `;` puts several
on one. A line that ends with an operator, a comma or an opening bracket
goes on to the next. So does a line followed by one that starts with
`.name` or `?.name`, or with a binary operator that can't start an
expression (any but `-`, which can: `-x`), so a chain may take a line
a step:

```asuka
let names = users
  .filter(u => u.active)
  .map(u => u.name)
let label = user?.nickname
  ?? user?.name
  ?? "Guest"
```

A line that starts with `.name` is something of its own in two places.
In a `match`, it's the next arm (`.ready(v) => …`). And alone on the
last line of a block whose last expression is its value (an `if` or a
`match` used as a value, a closure's block), it's that value:

```asuka
let tint: Color = if urgent {
  log("urgent")
  .red  // the if's value
} else { .gray }
```

There, two `.name` lines or more are still a chain.

Comments are `// …` and `/* … */`. A `///` comment documents the
declaration under it, and editors show it.

A name starts with a letter or `_` and goes on with letters, digits and
`_`. A letter is any script's, with its combining marks: `total`,
`नमस्ते`, `ราคา`, `名前` (Unicode's identifiers, UAX #31's XID_Start and
XID_Continue, as in Swift, Rust and Python). Names that look the same
are the same name: they're compared in Unicode's NFC form, so an `é`
typed as one character and one typed as an `e` and an accent are one.

#### Files for one platform

A file whose name ends in a platform's suffix is part of its package
only when the app is built for that platform: `_ios`, `_android`,
`_macos`, `_windows`, `_linux` and `_web`, and `_desktop` (macOS,
Windows and Linux) and `_mobile` (iOS and Android) for several. The
platforms' files may each declare the same names, which the package's
other files use as they would any:

```asuka
// feedback_mobile.asuka: iOS and Android
import platform/device

pub fn confirm() { device.haptic(.success) }

// feedback_desktop.asuka: macOS, Windows and Linux
import platform/accessibility

pub fn confirm() { accessibility.announce("Done") }

// feedback_web.asuka
pub fn confirm() {}
```

A name is declared once on each platform: in a file every platform has,
or in a file for each platform that has it. One declared in a file
every platform has and in a platform's, or in two files a platform has
both of (`feedback_desktop.asuka` and `feedback_linux.asuka`), is an error,
whichever platform the app is built for. A file whose name is only the
suffix (`linux.asuka`) is a file like any other.

Every package goes by its files' names alike: the app's, its
dependencies', the standard library's. What's built is a platform's:
`asuka dev` and `asuka build` build for the platform they run the app
on, `asuka test` for iOS unless `--platform` says another, and a program
for the computer it runs on (or `--target`'s). `asuka check` checks
each platform's files, and says which platforms an error is on when
it's not every one's: a name only some platforms' files declare is an
error where the others use it. An editor reads a platform's file with
that platform's files, and the others with iOS's, and shows every
platform's errors. For code that differs by platform within a file,
there's [`@platform`](https://asuka.egoist.dev/docs/language/control-flow#code-for-one-platform).

#### Apps, programs and libraries

A package is an entry, or a library. One with an `app { }` block is an
app: the block is what it shows (see [Navigation](https://asuka.egoist.dev/docs/language/navigation#navigation)). One
whose files declare `fn main()` is a **program**, a command-line tool:
`main` takes no parameters (`os.args()` are its arguments), returns
nothing (`os.exit(code)` ends it with a code), and may be `async` and
throw; it ends as `main` does. A package with neither is a library,
which others import. A package can't be an app and a program both, and
a program declares no components: it shows no views. Only the package
an app or a program is made of has an entry; a `main` in a package it
imports is a function like any other. See [Programs](https://asuka.egoist.dev/docs/programs).

```asuka
import std/os

fn main() {
  print("Hello, {os.args().first() ?? "world"}!")
}
```

### Values and types

| Type | Values |
|---|---|
| `int` | 64-bit integers: `42`, `1_000`, `0xff`, `0b1010`, `0o17` |
| `i8`, `i16`, `i32`, `u8`, `u16`, `u32`, `u64` | integers of 8, 16, 32 and 64 bits, signed (`i`) or not (`u`): [Integer types](#integer-types) |
| `float` | 64-bit floating point: `3.14`, `1e-3` |
| `bool` | `true`, `false` |
| `string` | UTF-8 text: `"Hello, {name}"` |
| `char` | one Unicode code point (a scalar value): `'a'`, `'\n'`, `'\u{1F600}'` |
| `bytes` | raw bytes |
| `[T]` | arrays: `[1, 2, 3]`, `[]` |
| `[K: V]` | maps, kept in insertion order: `["a": 1]`, `[:]` |
| `Set<T>` | sets of distinct values, kept in insertion order: `["a", "b"]` where a set is expected |
| `(A, B)` | tuples: `(1, "one")` |
| `T?` | optionals: a `T`, or `none` |
| `Duration` | `300ms`, `5s`, `2min`, `1h` (and `us`, `ns`) |
| `Color` | `#7c3aed`, `#7c3aed80`, and the semantic colors: `.label`, `.accent`, ... |
| `Length` | `100` (points), `50%`, `.auto` |
| `fn(A) -> B` | functions and closures |

Strings take any expression between braces: `"{count} items"`, `"Total:
{price * qty}"`. `\{` is a brace; `r"…"` is a raw string with no escapes
or interpolation, and `r#"…"#` a raw string that may hold a `"`: it
ends at a `"` followed by as many `#`s as it starts with (`r#"say
"hi""#`, `r##"a "# b"##`). `"""…"""` spans lines (without its first
line when that's blank, and without the indentation of its closing
quotes' line) and holds `"` and `""` as they are; `r"""…"""` is a raw
one.
A value that isn't a number, a bool or a string shows as text: its
type's own `description()` when it has one
([Declarations](https://asuka.egoist.dev/docs/language/declarations#equality-hashing-text-and-json)), an enum's
case by its name, an optional as `none` or its value, an error as its
message, a color as it's written (`#0a84ff`, `.accent`), a duration in
milliseconds (`1500ms`), the rest as JSON. What JSON can't hold is
written in the same way: a tuple as its elements in parentheses, each as
an array's are (`(1,"a","red")`), and a map whose keys aren't strings
with each key as it's written (`{1:"a"}`); a function, which has no
text, as its type (`<fn(int) -> int>`).

An interpolation may say how it's written after a colon, as Python
and Rust do: `"{price:>8.2f}"`. The format is
`[[fill]align][sign][#][0][width][,|_][.precision][type]`: `<`, `>` and
`^` align left, right and centered in the width, after the fill
character (a space unless it's given); `+` writes a plus too; `0` fills
with zeros after the sign; `,` or `_` groups an int's digits (and a
float's whole part); `.2` is a float's digits after its point, or the
most characters of a text. The type is `x`, `X`, `o` or `b` for an
int's digits in hexadecimal, octal or binary (`#` writes `0x`, `0o`,
`0b` before them), or `f`, `e` and `%` for a number as a fixed point, an
exponent or a percentage. A number is right-aligned, anything else
left-aligned as its text, which takes a fill, an alignment, a width and
a precision. The compiler reads the format: one that doesn't fit its
value is an error where it's written
([format-spec](https://asuka.egoist.dev/docs/errors/format-spec)).

```asuka
fn row(name: string, count: int, share: float) -> string = "{name:<10}{count:>6,}{share:>8.1%}"

let price = "{1234.5:,.2f}"          // 1,234.50
let hex = "{255:#06x}"               // 0x00ff
let title = "{"Total":*^11}"         // ***Total***
let table = row("asuka", 12500, 0.4) // asuka     12,500   40.0%
```

A width counts characters (code points), not how wide a terminal draws
them: `std/term`'s `textWidth`, `padEnd` and `fit` count its columns
(Chinese, Japanese and Korean characters and emoji take two),
[Programs](https://asuka.egoist.dev/docs/programs#an-aligned-table).

`print(a, b, c)` writes its values as interpolation shows them,
separated by spaces, on a line of the standard output (an app's log);
`eprint` writes them to standard error. After the values, as Swift's
`print`, `separator:` goes between them and `terminator:` after the
last (a line ending unless it says otherwise): `print(x, y, separator:
", ")`, `print("Saving…", terminator: "")`.

A tuple's elements are read by their place, `pair.0` and `pair.1`, and
a `var`'s change in place (`pair.0 += 1`); a place the tuple doesn't
have (`pair.2` of a pair) is a compile error. A tuple is also taken
apart: `let (count, label) = pair`, `for (i, x) in xs.enumerated()`, or
a tuple pattern in a `match` ([Control flow](https://asuka.egoist.dev/docs/language/control-flow#control-flow)).

A map's keys, and a set's elements, are `Hashable`: integers, strings,
bools, bytes, durations, colors, and the enums, structs, tuples,
optionals, arrays, maps and sets made of them (a struct or an enum that
holds itself is when its other parts are; a map's keys and values both).
Floats aren't, since NaN isn't equal to itself, nor are functions:

```asuka
struct Cell {
  row: int
  col: int
}

let marks: [Cell: string] = [Cell(row: 0, col: 2): "x"]
let seen: Set<(int, int)> = [(0, 0), (1, 2)]
```

#### Operators

From the tightest to the loosest; the operators on a line bind alike,
and go left to right (`**` and `??` right to left):

| Operators | What they do |
|---|---|
| `**` | a power: `2 ** 10` is 1024 |
| `!x`, `-x`, `~x` | not, negation, an integer's bits flipped |
| `<<`, `>>` | an integer's bits shifted left, and right (keeping a signed one's sign) |
| `*`, `/`, `%`, `&`, `&*` | multiplication, division, remainder, bitwise and, wrapping multiplication |
| `+`, `-`, <code>&#124;</code>, `^`, `&+`, `&-` | addition (and strings, arrays joined), subtraction, bitwise or, exclusive or, wrapping addition and subtraction |
| `..`, `..<` | ranges: `1..3`, `0..<n` |
| `==`, `!=`, `<`, `<=`, `>`, `>=` | comparisons (they don't chain) |
| `&&` | and |
| <code>&#124;&#124;</code> | or |
| `??` | an optional's value, or the default |

`<`, `<=`, `>` and `>=` order integers, floats, strings, durations, enums
without payloads (by the order their cases are declared) and types
with a `compare` method; a struct or an enum takes the arithmetic
operators from its methods too, `add`, `sub`, `mul`, `div`, `rem`,
`pow` and `neg` ([Declarations](https://asuka.egoist.dev/docs/language/declarations#structs)).

`**` binds tighter than the operators before it, `-` too, and to its
right, as in Python and in mathematics: `-2 ** 2` is `-(2 ** 2)`, -4,
and `2 ** 3 ** 2` is `2 ** 9`; `(-2) ** 2` is 4. An integer's power is of
its own type, its exponent an integer of any type, 0 or more (a
negative one is an error: `2 ** -1` isn't an integer); a float's power
takes a float or an integer (`2.0 ** 0.5`, `x ** 2`). `**=` assigns one.

The bitwise operators take integers, both of one type. A shift's count
is an integer of any type; a shift by the type's width or more shifts
every bit out (`>>` of a negative gives -1), and one by a negative
count is an error. `>>` keeps a signed integer's sign and fills an
unsigned one with zeros. With them and `b[i]` (a byte as an int), a
program reads and writes binary data, a Bluetooth characteristic or an
NFC record ([`std/encoding`](https://asuka.egoist.dev/docs/std/encoding) reads and writes whole
numbers in either byte order):

```asuka
let raw = b[1] | b[2] << 8                         // a little-endian uint16
let celsius = if raw & 0x8000 != 0 { raw - 0x10000 } else { raw }
let out = bytes([flags, value & 0xff, value >> 8])
```

An `int` holds −2⁶³ to 2⁶³ − 1. `+`, `-`, `*` and `**` whose result is past
that range throw an error, "integer overflow", as dividing by zero (`/`,
`%`) does: a `catch` takes it, and when nothing does it goes to the
nearest `Boundary` ([Errors](https://asuka.egoist.dev/docs/language/errors#errors)). So do `-x` and `abs(x)`
of the smallest `int`, its `/ -1`, and an array's `sum()` past the
range. A count, a size or a price that overflows is a bug shown where it
happens, not a wrong number saved. Every integer type is checked the
same way, at its own range: a `u8`'s `255 + 1` throws.

```asuka
let big = 9223372036854775807
let n = try big + 1 catch { 0 }               // 0: it threw
let next = big.addingChecked(1) ?? big          // none on overflow: big
```

For numbers past int's range, `std/bigint`'s `BigInt` is an integer of
any size, with the arithmetic operators and the comparisons
([Standard library](https://asuka.egoist.dev/docs/stdlib)):

```asuka
import std/bigint.{BigInt}

let big = BigInt.of(9223372036854775807)
let past = big * big + BigInt.one           // 85070591730234615847396907784232501250
let digits = "{BigInt.factorial(30)}"       // 265252859812191058636308480000000
```

`&+`, `&-` and `&*` wrap instead: past the largest `int` they go on from
the smallest, as 64-bit two's complement does (`9223372036854775807 &+
1` is `-9223372036854775808`), and a sized type's wrap at its width (a
`u8`'s `255 &+ 1` is 0). That's what hashes, checksums and binary
protocols want; `&+=`, `&-=` and `&*=` assign them:

```asuka
fn fnv1a(data: bytes) -> int {
  var h = 0xcbf2_9ce4_8422_2325
  for i in 0..<data.count() {
    h ^= data[i]
    h &*= 0x100_0000_01b3
  }
  return h
}
```

`a.addingChecked(b)`, `subtractingChecked` and `multipliedChecked` give
an `int?` (a `u8?` of a `u8`...), none on overflow, for code that
handles it as a value. The shifts and the bitwise operators never throw
for a result; `int(x)` of a float past the range gives the nearest `int`
(`0` for NaN).

#### Integer types

Beside `int`, there are integers of a fixed width, signed and not:

| Type | Values |
|---|---|
| `i8`, `i16`, `i32` | −2⁷ to 2⁷ − 1, −2¹⁵ to 2¹⁵ − 1, −2³¹ to 2³¹ − 1 |
| `int` | −2⁶³ to 2⁶³ − 1 (there's no `i64`: it's `int`) |
| `u8`, `u16`, `u32`, `u64` | 0 to 2⁸ − 1 (255), 2¹⁶ − 1, 2³² − 1, 2⁶⁴ − 1 |

`int` is what to use for counts, indexes, sizes and money in cents; the
others are for what has a width of its own: a byte (`u8`), a pixel's
channel, a file format's or a protocol's fields, a hash, an id past
`int`'s range (`u64`), a C function's (`Int32` is an `i32`:
[C](https://asuka.egoist.dev/docs/c)). Each holds its range exactly: its arithmetic throws past
it, as `int`'s does, and it's what JSON, CSV, TOML and SQLite read into
it, checked.

An integer literal is of the type that's expected, and must fit it: `let
level: u8 = 200`, `level + 1`, `let big: u64 = 18446744073709551615`;
`let x: u8 = 256` and `let y: u8 = -1` are errors where they're
written. Without one expected, it's an `int`.

Two integers of different types don't meet in an operator, an `int` and
a `u8` no more than an `int` and a `float`: `level + offset` of a `u8`
and an `int` is an error ([mixed-numbers](https://asuka.egoist.dev/docs/errors/mixed-numbers)),
and so is passing a `u8` where an `int` goes. Converting says which:

```asuka
fn conversions(n: int, level: u8, ratio: float) -> [u8] {
  let total = int(level) + n        // widening: always fits
  let a = u8(n)                     // an error ("integer overflow") when n doesn't fit
  let b = u8(exactly: n) ?? 0       // a u8?: none when it doesn't fit
  let c = u8(truncating: n)         // its low 8 bits: u8(truncating: 300) is 44, of -1 255
  let d = u8(clamping: n)           // the nearest: 0 below, 255 above
  let e = u8(ratio)                 // a float: toward zero, the nearest past the range
  return [a, b, c, d, e, u8(clamping: total)]
}
```

`u8.min` and `u8.max` are a type's range (`int.max` is
9223372036854775807), and `u64.parse(text)` reads one from text, as
`toInt()` does an `int` (`radix:` too), or gives none when it isn't one
or doesn't fit. A `u64` past `int`'s largest prints, parses, compares,
divides and goes into JSON as the number it is (`18446744073709551615`).

`-x` and `abs(x)` take a signed integer: an unsigned one has no
negatives (`0 &- x` wraps). The integer types have `int`'s methods,
`toString(radix:)`, `padded(width)`, `float()`, `abs()` and the checked
ones, take format specs (`"{id:x}"`), order and hash as numbers (keys of
maps and sets, `Comparable` and `Hashable` in generics), and make
ranges: `for i in u16(0)..<count`.

A byte read from `bytes` is an `int` (`b[i]`, 0 to 255), so arithmetic
on it doesn't wrap at 8 bits; `b.toArray()` is them as a `[u8]`, and
`bytes(xs)` makes bytes of a `[u8]` as of an `[int]`.

#### Built-in methods

Strings, bytes, arrays, sets, maps and optionals have methods; those
that change their value (`push`, `pushFirst`, `insert`, `removeAt`,
`removeWhere`, `pop`, `popFirst`, `clear`, `sort`, `remove`) take a
`var` or state:

| Type | Methods |
|---|---|
| `[T]` | `count()`, `count(where)`, `isEmpty()`, `first()`, `last()`, `get(i)` (a `T?`), `push(x)`, `pushFirst(x)`, `insert(x, at: i)`, `removeAt(i)`, `removeWhere(p)`, `pop()`, `popFirst()`, `clear()`, `map(f)`, `flatMap(f)` (the arrays `f` gives, one after another), `compactMap(f)` (what `f` gives, without its nones), `filter(p)`, `reduce(initial, f)`, `any(p)`, `all(p)`, `index(p)` (an `int?`), `find(p)`, `contains(x)`, `sum()`, `min()`, `max()` (a `T?`; `min(by:)`, `max(by:)` with a less-than, `min(key:)`, `max(key:)` by a key), `unique()`, `grouped(by: f)` (a `[K: [T]]`), `chunked(n)`, `zip(other)` (pairs, as long as the shorter), `reversed()`, `slice(from, to)` (`to` left out: the end), `prefix(n)`, `suffix(n)`, `sorted()`, `sorted(by)`, `sorted(key:, descending:)`, `sort()`, `sort(by)`, `sort(key:, descending:)`, `enumerated()`, `joined(separator:)` (of strings), `lazy()` (a `Seq<T>`), `+` |
| `Set<T>` | `count()`, `isEmpty()`, `contains(x)`, `insert(x)` (whether it was new), `remove(x)` (whether it was there), `union(s)`, `intersection(s)`, `subtracting(s)`, `isSubset(of: s)`, `isSuperset(of: s)`, `lazy()`; `Set(xs)` is an array's distinct elements, `Set<T>()` none |
| `[K: V]` | `m[k]` (a `V?`), `m[k] = v`, `count()`, `isEmpty()`, `has(k)`, `get(k)`, `remove(k)`, `keys()`, `values()` (in insertion order), `mapValues(f)`, `filter((k, v) => …)`, `lazy()` (its `(key, value)` pairs) |
| `string` | `count()` (characters), `byteCount()`, `isEmpty()`, `upper()`, `lower()`, `trim()`, `trimStart()`, `trimEnd()`, `trimPrefix(s)`, `trimSuffix(s)`, `contains(s, ignoreCase:)`, `hasPrefix(s, ignoreCase:)`, `hasSuffix(s, ignoreCase:)`, `equals(s, ignoreCase:)`, `compare(s, ignoreCase:)` (negative, zero or positive), `foldCase()`, `replace(old, new, count:)` (every one unless count says), `split(sep, count:)`, `fields()` (the words), `lines()`, `find(s)`, `findLast(s)`, `findAny(chars)` (byte offsets, or none), `slice(from, to)` (characters; `to` left out: the end), `byteSlice(from, to)` (bytes: the offsets find gives; `to` left out: the end), `padStart(width, with:)`, `padEnd(width, with:)`, `toInt()`, `toInt(radix:)`, `toFloat()`, `repeat(n)`, `chars()` (its `[char]`), `graphemes()`, `lazy()` (its chars, a `Seq<char>`), `reversed()`, `bytes()`, `+` |
| `bytes` | `b[i]` (an `int`, 0 to 255; out of range is an error), `count()`, `isEmpty()`, `get(i)` (an `int?`), `slice(from, to)` (`to` left out: the end), `concat(b)`, `string()` (UTF-8; what isn't becomes U+FFFD), `hex()` and `hex(separator)` (`"04 a1"`), `toArray()` (a `[u8]`); `bytes()` is none, `bytes([0x04, 0xa1])` those, `bytes(xs)` a `[u8]`'s, `s.bytes()` a string's UTF-8 |
| `int`, `i8` … `u64` | `float()`, `abs()` (of a signed one), `toString(radix:)` (`255.toString(radix: 16)` is `"ff"`), `padded(width)` (`7.padded(2)` is `"07"`), `addingChecked(o)`, `subtractingChecked(o)`, `multipliedChecked(o)` (an `int?`, a `u8?` of a `u8`: none on overflow); `T.min`, `T.max`, `T.parse(text, radix:)` (a `T?`) |
| `float` | `fixed(digits)`, `rounded()`, `floor()`, `ceil()`, `int()` (toward zero), `abs()` |
| `Color` | `Color.parse(text)` (`#rrggbb`, `#rgb`, `#rrggbbaa`: a `Color?`), `Color.rgb(r, g, b, alpha:)`; `opacity(a)`: drawn at a times its opacity (a semantic color stays semantic); `hex()` (`"#0a84ff"`; a semantic color, its name: `".accent"`) |
| `Duration` | `nanoseconds()`, `milliseconds()` (ints), `seconds()`, `minutes()`, `hours()` (floats), `rounded(to:)`, `truncated(to:)`, `+`, `-`, `* n` (an int or a float: `1.5 * 1h`), `/ n`, `/ d` (another Duration: how many times it goes in, a float) |
| `Length` | `* n` (`width: 50% * 2`, a point length scaled) |
| `T?` | `isNone()`, `isSome()` |

The methods that call a function you give them (`map`, `filter`,
`reduce`, `sorted(by:)`, `grouped(by:)`, a map's `mapValues`...) take
one that throws, and then throw what it throws: the call is a `try`.
Without a throwing function, they don't throw.

```asuka
fn port(text: string) throws -> int = text.toInt() ?? throw Error("not a port: {text}")

fn ports(lines: [string]) throws -> [int] = try lines.map(l => try port(l))
```

A few of them, at work:

```asuka
struct Sale {
  region: string
  amount: int
}

fn report(sales: [Sale]) -> [string] {
  let byRegion = sales.grouped(by: s => s.region)
  let totals = byRegion.mapValues(ss => ss.map(s => s.amount).sum())
  let best = sales.max(by: (a, b) => a.amount < b.amount)
  let codes = ["7", "x", "42"].compactMap(c => c.toInt())    // [7, 42]
  let pages = sales.chunked(50)                               // [[Sale]], 50 each
  var out = ["{best?.region ?? "none"} sold the most"]
  for (region, total) in totals { out.push("{region}: {total}") }
  for (i, page) in pages.enumerated() { out.push("page {i + 1}: {page.count()}") }
  out.push("{codes.reduce(0, (sum, c) => sum + c)}")
  return out
}
```

`unique()` keeps each element once, where it first comes, and
`grouped(by:)` keeps the keys in the order they first come: both need
`Hashable` values (the keys, for `grouped`). `min()` and `max()` order
as `sorted()` does, and give the first of equal elements. `chunked(n)`
of an `n` under 1 is an error, as `xs[i]` out of range is.

`sorted(key:)` orders the elements by what a function gives each, a
`Comparable` computed once per element: `people.sorted(key: p =>
p.age)`. `descending: true` puts the greatest first, and either way the
sort is stable: elements with equal keys keep their order. `sort(key:)`
sorts a `var` in place, and `min(key:)` and `max(key:)` give the element
whose key is least or greatest (the first of equal ones). `sorted(by:)`
takes a less-than instead, for an order no single key says.

```asuka
struct Player {
  name: string
  score: int
  joined: int
}

fn ranking(players: [Player]) -> [string] {
  let best = players.sorted(key: p => p.score, descending: true)
  let newest = players.max(key: p => p.joined)
  return best.map(p => p.name) + [newest?.name ?? ""]
}
```

An array is a double-ended queue too: `pushFirst(x)` and `popFirst()`
change its start as `push(x)` and `pop()` change its end, each in O(1)
(amortized: an array keeps room at its start once it's changed there),
and so do `insert(x, at: 0)` and `removeAt(0)`. An insertion or a
removal elsewhere moves the elements on its shorter side. A queue is
an array: `queue.push(job)`, then `queue.popFirst()`.

`toInt(radix:)` reads a whole string in a radix from 2 to 36, letters
in either case and an optional sign first (`"ff".toInt(radix: 16)` is
255, `"-101".toInt(radix: 2)` is -5), or gives none; `toString(radix:)`
writes one, lowercase. A radix outside 2 to 36 is an error.

`min`, `max`, `abs`, `float(n)`, `int(x)` and the other integer types'
conversions (`u8(x)`) are functions;
`std/math` has the rest (`math.sqrt`, `math.pow`, `math.pi`, ...).

`upper()` and `lower()` map every letter by Unicode's rules, the same
in every language: `"straße".upper()` is `"STRASSE"`, and a word's last
`Σ` is `ς` in lower case. `ignoreCase: true` compares two strings'
case foldings (`foldCase()`: `"Straße"` and `"STRASSE"` both fold to
`"strasse"`), as Go's `strings.EqualFold` and Python's `casefold` do; a
match starts and ends on whole characters, so `"ß"` doesn't start with
`"s"`. Neither follows a language's own rules: the Turkish dotless ı
and dotted İ map as everywhere else (`"İ".lower()` is `i` with a
combining dot), and text isn't normalized (`"é"` and `e` with a
combining accent differ). For sorting names without their case,
`compare` with `sorted(by:)`; `foldCase()` makes a key that ignores it:

```asuka
fn byName(names: [string]) -> [string] = names.sorted(by: (a, b) => a.compare(b, ignoreCase: true) < 0)

fn sameUser(a: string, b: string) -> bool = a.trim().equals(b.trim(), ignoreCase: true)

fn isMarkdown(path: string) -> bool = path.hasSuffix(".md", ignoreCase: true)
```

`count()` and `chars()` count code points; `graphemes()` gives what a
reader takes for one character, as Unicode's text segmentation (UAX
#29) says: `"e\u{301}"`, a flag (`"🇯🇵"`) and a family emoji joined with
ZWJs are one each.

A `char` is one code point, as Rust's is: what `for c in s` goes over
and `s.chars()` gives, in order (a grapheme of several, an `e` and its
accent, is a string: `graphemes()`). It's written between single quotes
(`'a'`, `'\''`, `'\u{301}'`), compared and ordered by its code point,
and is a map's key; it prints as itself and is JSON's one-character
string. `c.isLetter()`, `isDigit()`, `isNumber()`, `isWhitespace()`,
`isUpper()`, `isLower()`, `isPunctuation()` and `isSymbol()` read
Unicode's categories, any script's (`'٣'.isDigit()`); `upper()` and
`lower()` map it where the result is one char (`'ß'.upper()` stays
`'ß'`: `"ß".upper()` is `"SS"`); `code()` is its number, and `char(n)`
the char of one (an error when `n` isn't a code point). `s + c` and `s
+= c` add one to a string.

```asuka
fn initials(name: string) -> string {
  var out = ""
  var start = true
  for c in name {
    if c.isWhitespace() {
      start = true
    } else if start {
      out += c.upper()
      start = false
    }
  }
  return out
}

fn digits(s: string) -> int = s.chars().filter(c => c.isDigit()).count()

fn kind(c: char) -> string = match c {
  'a' | 'e' | 'i' | 'o' | 'u' => "vowel"
  _ => if c.isLetter() { "letter" } else { "other" }
}
```


`toInt()` and `toFloat()` read the whole string, or give none: an
optional sign, then the number, with nothing around it (`" 12"` is
none: `trim()` it first). `toInt()` takes digits (`"+7"`, `"-42"`);
`toFloat()` also a fraction and an exponent (`"2.5"`, `".5"`,
`"1e-3"`) and what a float prints as (`"Infinity"`, `"NaN"`). Both
read `.` as the decimal point in every language.

#### Lazy sequences

`xs.lazy()` is an array's elements as a `Seq<T>`, a lazy sequence. Its
`map`, `filter`, `take` and the rest make another `Seq` and compute
nothing; what reads it (a `for`, `toArray()`, `first()`, `reduce`...)
computes its elements one at a time, each through every step, with no
array in between. A chain over a million elements that keeps three
computes those three and what it took to find them, and a sequence may
have no end:

```asuka
fn squaresUnder(limit: int) -> [int] = Seq(from: 1).map(n => n * n).takeWhile(n => n < limit).toArray()

fn firstLong(lines: [string]) -> string? = lines.lazy().map(l => l.trim()).find(where: l => l.count() > 80)

fn fibonacci() -> Seq<int> = Seq(state: (0, 1), next: s => (s.0, (s.1, s.0 + s.1)))
```

A map's `lazy()` gives its `(key, value)` pairs, a set's its elements, a
string's its characters, and a range's (`(0..<n).lazy()`) its numbers,
without an array of them. `Seq(…)` makes one from a function:

| Made with | Its elements |
|---|---|
| `Seq(from: n, by: step)` | `n`, `n + step`, ... without end; `by` is 1 unless it says |
| `Seq(first: x, next: f)` | `x`, `f(x)`, `f(f(x))`, ... until `f` gives none |
| `Seq(state: s, next: f)` | `f(s)` gives the next element and the next state, `(x, s2)`, or none at the end |
| `Seq(repeating: x, count: n)` | `x`, `n` times; without `count`, without end |

| | Methods |
|---|---|
| A new `Seq` | `map(f)`, `filter(p)`, `compactMap(f)`, `flatMap(f)` (the elements of the `Seq`s `f` gives, one after another), `enumerated()`, `zip(other)`, `take(n)`, `skip(n)`, `takeWhile(p)`, `skipWhile(p)`, `chunked(n)`, `windows(n)` (each `n` in a row: `[1, 2, 3]`'s `windows(2)` is `[1, 2]`, `[2, 3]`), `onEach(f)` (`f` called with each element as it's read), `unique()`, `concat(other)` |
| Reading it | `toArray()`, `toSet()`, `toMap()` (of `(key, value)` pairs), `count()`, `count(where:)`, `first()`, `last()`, `find(where:)`, `any(where:)`, `all(where:)`, `contains(x)`, `reduce(initial, f)`, `sum()`, `min()`, `max()`, `min(by:)`, `max(by:)`, `min(key:)`, `max(key:)`, `joined(separator:)` |

`take`, `find`, `first`, `any` and `all` stop pulling elements once
they have their answer: what comes after is never computed. A `Seq` is
read from its start each time (a `for` over it twice computes it
twice). It holds its source's value as it was when it was made, as any
value would: a change to the array after it doesn't change it.

A step's function runs later, as the sequence is read, so it can't
throw: do that in a `for`, or with an array's `map`. A function that
reading calls at once (`reduce`, `find(where:)`, `any`, `count(where:)`,
`min(key:)`...) may, and then the call throws what it throws, as an
array's methods do.

A priority queue, and maps and sets kept in their keys' order, are
`std/collections`' `Heap`, `SortedMap` and `SortedSet` ([Standard
library](https://asuka.egoist.dev/docs/stdlib)).

#### Value semantics

Strings, arrays, sets, maps, structs and enums are **values**: assigning one
or passing it gives a copy, as far as you can tell. Copies are cheap: they
share memory until one of them changes, and a value only one place holds
is changed in place.

```asuka
var a = [1, 2, 3]
var b = a
b.push(4)        // a is still [1, 2, 3]
```

Stores, closures, tasks and views are **references**: copies are the same
thing.

There is no null. A value that may be missing is an optional:

```asuka
let user: User? = none
if let u = user { Text(u.name) }
let name = user?.name ?? "Guest"
let first = items.first()!        // a crash if it's none: say so only when it can't be
```

After `?.`, the rest of the chain stops at none too: `user?.boss.name`
is a `string?`.

Optionals don't nest: a value is there or it isn't. `int??` is an error
(one `?` is enough), and a generic `T?` whose `T` is already optional
is that `T`.

### Declarations

```asuka
let pi = 3.14159             // a constant
var total = 0                // a variable
state count = 0              // state: a value the UI follows
state shown = count          // count, until it's written: state that follows
```

Types are inferred; write one where it helps: `let ids: [int] = []`.

`type` names a type, at the top of a file: `type Pair<A, B> = (A, B)`,
`type Check = fn(string) -> bool`. It's another name for the same type
(an alias): a `Pair<int, string>` is an `(int, string)`.

#### A file's values

At the top of a file, `let` is a constant, `state` the app's state
([Stores](https://asuka.egoist.dev/docs/language/stores)), and `var` a variable of the package's: any of its
code may read and change it, and other packages too when it's `pub`
(`util.hits += 1`). A var isn't state: what reads it gets its value as
it is then, and a view that shows it doesn't change when it does (that's
what `state` is for). It's for what views don't show: a cache, a
counter, a program's settings.

```asuka
var cache: [int: int] = [:]
var lookups = 0

fn fib(n: int) -> int {
  lookups += 1
  if n < 2 { return n }
  if let v = cache[n] { return v }
  let v = fib(n - 1) + fib(n - 2)
  cache[n] = v
  return v
}
```

A file's values are made before anything else runs, a package's before
those of the packages that import it, and each after the ones its
initializer reads (one that reads itself, directly or not, is an error).
A var is changed in place, as a local is: `cache[n] = v` copies
nothing. It's the main thread's, as state is: `spawn`'s block and a
[worker](https://asuka.egoist.dev/docs/language/async#workers) can't use one, nor call a function that does
(the compiler says which); read it first and pass the value. When
`asuka dev` reloads the app, a file's vars are made again from their
initializers, as its lets are; state keeps its values.

#### Structs

```asuka
struct Message {
  id: string
  subject: string
  read: bool = false

  fn preview() -> string = subject.slice(0, 40)
}

let m = Message(id: "1", subject: "Hello")
let n = Message(id: m.id, subject: m.subject, read: true)
```

A struct is made by calling it with its fields, by name or in order;
fields with a default may be left out. Its methods see its fields by
their names. A `fn` reads the value; a `mut fn` may change it: assign
its fields (and their parts), call its other `mut fn`s, or assign
`self` a new value.

```asuka
struct Item {
  id: int
  qty: int = 1
}

struct Cart {
  items: [Item] = []

  mut fn add(i: Item) {
    if let n = items.index(x => x.id == i.id) { items[n].qty += 1 } else { items.push(i) }
  }
  mut fn clear() { self = Cart() }
  fn total() -> int {
    var sum = 0
    for i in items { sum += i.qty }
    return sum
  }
}

fn twoApples() -> int {
  var cart = Cart()
  cart.add(Item(id: 1))
  cart.add(Item(id: 1))
  return cart.total()
}
```

A `mut fn` is called on what can change: a `var`, state (a component's,
a store's, the app's), or a field, an element or a map's entry of one
(`carts[0].add(i)`, `order.cart.clear()`). Not on a `let`, a parameter,
a prop or what a call returns: the compiler says so. On state, the call
is one change of that state, however many fields it changes: what reads
`todos[i]` updates, not every row. A `mut fn` can't be `async` (a value
half changed across an `await` would be seen by every handler that runs
meanwhile: an async change is a store's `fn`). It may throw: what it
changed before the throw stays. A closure made in one has `self`'s
value as it was then, and can't change it.

`let` in a struct's body is a constant of the
type (`Font.title`), which `.title` names wherever a `Font` is
expected. `static fn` in a struct's or an enum's body is a function of
the type's, with no `self`: called on the type with named arguments and
defaults as any function (`Money.of(dollars: 2)`), or as `.of(…)`
wherever the type is expected.

```asuka
struct Money {
  cents: int

  static fn of(dollars: int, cents: int = 0) -> Money = Money(cents: dollars * 100 + cents)
}

let price: Money = .of(dollars: 4, cents: 50)
```

A struct may take type parameters, inferred where it's made, or
written out after its name, as a generic function's are (a store's
too):

```asuka
struct Page<T> {
  items: [T]
  next: string? = none
}

let page = Page(items: [1, 2, 3])     // a Page<int>
let empty = Page<string>(items: [])   // written out, where nothing else says it
```

A generic struct's `static fn` takes the struct's type parameters, then
its own: they're inferred from its arguments (`Stack.of([1, 2])`), or
from what's expected (`let s: Stack<string> = .empty()`), or written
with the struct's name (`Stack<int>.empty()`) or after the function's
(`Stack.empty<int>()`, the struct's first). Its `let` is
one constant for every type it's made with, so its type can't name
them: a constant that would (an empty `Stack<T>`) is a `static fn`.

```asuka
struct Stack<T> {
  items: [T] = []
  let limit = 1000

  static fn of(items: [T]) -> Stack<T> = Stack(items: items)
  static fn empty() -> Stack<T> = Stack()
}

let numbers = Stack.of([1, 2, 3])       // a Stack<int>
let names: Stack<string> = .empty()     // from what's expected
let flags = Stack.empty<bool>()         // written out
let most = Stack.limit
```

A struct pattern matches a struct's fields by their names, in `match`
and `if let`: `y` alone binds the field `y`, `y: py` binds it as `py`,
`x: 0` matches a value, and the fields it leaves out match anything.
Patterns nest:

```asuka
match line {
  Line(from: Point(x: 0, y: 0), dashed: true) => "dashed, from the origin"
  Line(from: Point(x: 0, y), to) => "from {y} on the y axis to {to.x}"
  Line(dashed) => "a line"
}
```

A struct or an enum takes operators from its methods: `fn add(o: T) ->
R` (or `plus`) is `a + b`, `fn sub(o: T) -> R` (or `minus`) is `a - b`, `fn mul(o:
T) -> R` (or `times`) is `a * b`, `fn div(o: T) -> R` (or `quo`) is `a /
b`, `fn rem(o: T) -> R` is `a % b`, `fn pow(o: T) -> R` is `a ** b`,
`fn neg() -> R` is `-a`, and `fn
compare(o: T) -> int` (negative, zero or positive) is `<`, `<=`, `>`
and `>=`, `sorted()`, and `T: Comparable`
([Functions](https://asuka.egoist.dev/docs/language/functions#interfaces-and-generics)). `T` may be another
type than the value's (`Money * int`), and an operator with two names
takes one `T` each: a `Decimal`'s `mul(o: Decimal)` and `times(n: int)`
are `price * rate` and `price * 3`. The other order (`3 * price`) isn't
derived. `a += b` is `a = a + b` with them (and `-=`, `*=`, `/=`, `%=`, `**=`),
when the method gives `a`'s type back, on a var, state, a field or an
element (state changes once). That's how `std/time`'s `Time` adds a
`Duration` and subtracts another `Time`:

```asuka
struct Money {
  cents: int

  fn add(o: Money) -> Money = Money(cents: cents + o.cents)
  fn times(n: int) -> Money = Money(cents: cents * n)
  fn neg() -> Money = Money(cents: -cents)
  fn compare(o: Money) -> int = cents - o.cents
}

let total = Money(cents: 250) + Money(cents: 99) * 2
let over = total > Money(cents: 300)
let refund = -total

fn withTip(bill: Money) -> Money {
  var paid = bill
  paid += Money(cents: 100)
  return paid
}
```

A struct isn't ordered field by field: what its order means is its
`compare`'s to say.

#### Equality, hashing, text and JSON

A struct is equal to another (`==`), hashes as a map's key, prints and
is written as JSON by its fields, an enum by its case and payload. A
type says otherwise with methods the language calls, as `compare` is
`<`:

| Method | What calls it |
|---|---|
| `fn equals(other: T) -> bool` | `==` and `!=`, a map's keys, a set's elements, `contains`, an array's `==` |
| `fn hash() -> int` | a map's keys and a set's elements: equal values hash alike, so a type with its own `equals` has its own `hash` (`hashOf(x)` is a value's hash, to combine) |
| `fn description() -> string` | interpolation, `print`, a snapshot |
| `fn toJSON() -> J` | `json.encode`, and the text of a value it's inside: `J` is any type JSON can say |
| `static fn fromJSON(json: J) throws -> T` | `json.decode`; what it throws is the decode's error |

```asuka
import std/json

struct Tag {
  name: string
  color: string = "gray"

  fn equals(other: Tag) -> bool = name.lower() == other.name.lower()
  fn hash() -> int = hashOf(name.lower())
  fn description() -> string = "#{name}"
  fn toJSON() -> string = name
  static fn fromJSON(json: string) throws -> Tag = Tag(name: json)
}

let same = Tag(name: "Work") == Tag(name: "work", color: "red")  // true
let label = "{Tag(name: "home")}"                                 // #home
let text = json.encode([Tag(name: "a")])                          // ["a"]
```

They're used wherever the values go, inside arrays, maps and other
structs too. With them, a type equal by what its fields mean (a name
without case, a set in any order) is a map's key, and one that holds a
function (an order) is equal, hashed and JSON by what it holds. A
generic type gives them in an extension of its package, for the types
whose arguments meet its constraints (`Equatable`, `Hashable`,
`Comparable`): std/collections' `Heap<T>` is equal when `T` is
Hashable, and read from JSON when it's Comparable. They can't be
`async`, `mut` or private, and an enum without payloads has only its
own `description` (it's equal and written by its case). The compiler
says when one's signature isn't what the language calls
([special-method](https://asuka.egoist.dev/docs/errors/special-method)).

#### What's private

`private` makes a declaration its file's: a file's `fn`, `let`, `var`,
struct, enum, interface or type, and a struct's or an enum's field,
method, `static fn` or constant. The file uses it as any other (its
types' methods, its functions, its extensions); other files can't name
it, but the package's test files (`name_test.asuka`), which test its
insides. What isn't `pub` is its package's; what's `private`, its
file's.

```asuka
pub struct Counter {
  private hits: [string: int] = [:]

  mut fn add(word: string) { hits[word, default: 0] += 1 }
  fn top() -> string? = best(hits)
}

private fn best(counts: [string: int]) -> string? {
  var found: (string, int)? = none
  for (w, n) in counts {
    if found == none || n > found!.1 { found = (w, n) }
  }
  return found?.0
}
```

Elsewhere, a struct is made without its private fields, so each needs a
default (`Counter()`); one with a private field that has none is made
in its file only, by its static fns, which keep its rules (a
`Decimal`'s coefficient without trailing zeros). A private method is
no operator (`add`, `compare`), nor does it give its type an interface,
since those are used from anywhere.

#### Extensions

`extend` adds methods, `mut fn`s, `static fn`s and constants to a type:
a built-in one (`string`, `int`, `[T]`, `[K: V]`, `Set<T>`, `Color`,
`Duration`), the package's own, or another package's.

```asuka
extend string {
  fn slug() -> string = lower().fields().joined(separator: "-")
}

extend Color {
  let brand = #7c3aed
  let brandSoft = #7c3aed.opacity(0.15)
}

extend<T: Comparable> [T] {
  fn median() -> T? = if isEmpty() { none } else { sorted()[count() / 2] }
}

component Title {
  prop text: string
  Text(text.slug()) { color: .brand }
}
```

An extension's methods see the value as the type's own do: its fields,
and its methods by their bare names (`lower()` is `self.lower()`). A
constant of the type's is `.name` wherever the type is expected, as a
type's own constants are, and `Color.brand` too; a `static fn` likewise.
An extension adds no fields and no state, and a name it adds can't be
the type's own: the type's layout and members stay its declaration's.
A generic extension takes its type parameters after `extend`
(`extend<K: Hashable, V> [K: V]`).

What an extension adds is visible in its package, and its `pub` members
in the packages that import it, and nowhere else: a dependency doesn't
change what `x.trim()` means in your code. Two extensions you see that
add one name to a type are an error where you use it, naming both
packages.

A type has an interface through what extensions give it, where they're
visible: `extend Money { fn area() -> float … }` makes `Money` a
`Shape`. `extend Money: Shape { … }` says so, and the compiler checks
it as it does `struct Money: Shape`; it's how an enum without payloads
from another package gets an interface. A type has one method of a name
in a program: two packages' extensions that give a type the same
method, both of which make it an interface's, are an error.

#### Enums

```asuka
enum Filter {
  all
  unread
  from(Person)

  fn matches(m: Message) -> bool = match self {
    .all => true
    .unread => !m.read
    .from(p) => m.from == p
  }
}

let f: Filter = .unread
```

An enum's `mut fn` changes the value too, often by assigning `self`:

```asuka
enum Light {
  red
  green
  yellow

  mut fn next() {
    self = match self {
      .red => .green
      .green => .yellow
      .yellow => .red
    }
  }
}
```

Where the type is known, a case is written `.name`. `match` must cover
every case (or end with `_ =>`); patterns bind payloads (`.from(p)`, by
their labels too: `.server(code: c)`), match literals (`0`, `"x"`),
several cases (`.a | .b`), nest (`.from(Person(vip: true))`) and take
guards (`.from(p) if p.vip =>`). The compiler follows nested patterns
when it checks that a match covers everything, and says what's missing
as patterns: `match doesn't cover .key(_, false)`. A case's labeled payload with a zero value
(`false`, `0`, `""`, an empty array or map, `none`, an enum's first case)
may be left out where it's made: `.button(title: "Copy", run: copy)`
leaves `Action.button`'s `icon`, `role`, `disabled` and `checked` so.

A case's payload may hold the enum itself, so an enum can be a tree:

```asuka
enum Expr {
  num(int)
  add(Expr, Expr)

  fn eval() -> int = match self {
    .num(n) => n
    .add(a, b) => a.eval() + b.eval()
  }
}
```

### Functions and closures

```asuka
fn area(width: float, height: float = 1) -> float {
  return width * height
}

fn double(x: int) -> int = x * 2      // one expression

area(3, height: 4)
let squares = [1, 2, 3].map(x => x * x)
let sorted = people.sorted((a, b) => a.age < b.age)
```

Arguments are positional, then by name. A closure is `x => expr`, `(a,
b) => expr` or `x => { statements }`. It shares the variables it
captures with the code around it: what one changes, the other sees.

```asuka
var n = 0
let inc = () => { n += 1 }
inc()
inc()                // n is 2
```

State is shared the same way. Only `spawn` copies what it uses (values
only, as they are when it starts), since it runs on another thread.

A closure is async, and throws, as the function type it's passed as
says (`async fn(int) -> int`). Where nothing says, as for a closure kept
in a `let`, its body decides: it's async if it awaits, and it throws if
an error can leave it. `async` before it says so up front, whether it
awaits yet or not, so that adding an `await` later changes nothing for
its callers; such a closure can't be passed where a function that
can't wait is expected.

```asuka
let refresh = async () => {
  await store.reload()
}
await refresh()
```

When a call misses its `await` or its `try` because of what a closure's
body does, the error says where in the body.

The built-in methods that call a closure at once, an array's `map`,
`filter`, `reduce`, `sorted(by:)` and the rest, take one that throws: the
call then throws what the closure throws, and needs its `try`, as
Swift's `rethrows` does. With a closure that doesn't throw, it doesn't.

```asuka
let counts = try lines.map(l => try parse(l))
let sorted = try names.sorted(by: (a, b) => try rank(a) < try rank(b))
```

A `fn` may be declared in a function's, a handler's, a closure's or a
method's body too. It sees what's around it as a closure does, and it
may call itself and the fns declared next to it in its block (and they
it):

```asuka
fn total(root: Node) -> int {
  var sum = 0
  fn walk(n: Node) {
    sum += n.size
    for c in n.children { walk(c) }
  }
  walk(root)
  return sum
}
```

It's a value like any function (`items.map(walk)`). Its arguments go
in order, without labels or defaults, and it isn't generic.

A function that never returns (it always throws, panics or ends the
program) says so with `-> never`, as `os.exit` does; a call of one ends
a block as a `return` would, and goes where any value does: `let port =
text.toInt() ?? fail("not a port")`.

That's how a function calls itself. A closure kept in a var it calls
itself through (`var walk = …; walk = n => { … walk(c) … }`) holds the
var, and the var holds it: neither is ever freed, since nothing
collects such a cycle, and the compiler warns of it. A local fn calls
itself without holding itself.

#### Interfaces and generics

An interface is a set of methods and fields; a type has it if it has
them (nothing to declare).

```asuka
interface Shape { fn area() -> float }

fn total<T>(items: [T], size: fn(T) -> float) -> float {
  var sum = 0.0
  for i in items { sum += size(i) }
  return sum
}
```

An interface is a type too: a value of one is any value that has its
methods, and calling one calls that value's.

```asuka
let shapes: [Shape] = [Circle(radius: 1), Square(side: 2)]
let areas = shapes.map(s => s.area())
```

An interface may ask for a `mut fn` (`mut fn next() -> T?`, as the
prelude's `Iterator<T>` does): a type's `mut fn` has it, and so does a
`fn` of the same signature. A `mut fn` isn't an interface's plain `fn`,
which is called on any value. A type has an interface through what
[extensions](https://asuka.egoist.dev/docs/language/declarations#extensions) give it, too, where they're
visible.

An interface's method may have a body: the interface provides it. A
type has the interface when it has the methods without bodies; a
provided one is the type's own when it declares one of the same
signature, and the interface's otherwise. Inside it, `self` is the
interface's value, with its methods and fields by their bare names:

```asuka
interface Shape {
  fn area() -> float
  fn describe() -> string = "a shape of {area()}"
}

struct Square {
  side: float
  fn area() -> float = side * side
}

struct Circle {
  radius: float
  fn area() -> float = 3.14159 * radius * radius
  fn describe() -> string = "a circle of {radius}"
}

fn describeAll(shapes: [Shape]) -> [string] = shapes.map(s => s.describe())
```

A provided method is called on the interface's values and on type
parameters constrained by it (`T: Shape`, below); a `Square` that isn't
seen as a `Shape` has no `describe()`. A type that two interfaces each
provide a method of one name to, and that has none of its own, can't be
both: the compiler says so, and the type declares its own.

What an interface's value is, its type, is matched with the patterns of
the types that have the interface: a struct's (`Circle(radius)`), a
name with a type (`c: Circle`), or an enum's case written with its type
(`Unit.big(n)`). A `match` over an interface ends with `_`, since any
type may have it:

```asuka
fn label(s: Shape) -> string = match s {
  Circle(radius: 0) => "a point"
  c: Circle => "a circle of {c.radius}"
  Square(side) => "a square of {side}"
  _ => s.describe()
}

if let c: Circle = shape { … }
let sq: Square = shape else { return }
```

The type must have the interface (a `Shape` can't be a `Point` that has
no `area()`), and an enum without payloads has it only when it declares
it (`enum Unit: Shape`). A value of a concrete type needs no test: `c:
Circle` against a `Circle` is an error. Against a type parameter, the
test is decided as the function is compiled for each type, and an arm
that can't match isn't in the program. `catch e: ParseError` is the
same test, of an `Error` ([Errors](https://asuka.egoist.dev/docs/language/errors)).

Generic functions and types take type parameters. A parameter's
constraints are the program's interfaces and `Equatable` (what `==`
compares: the built-in values, and structs, enums, tuples, arrays, maps
and optionals whose parts are, or that have their own `equals`),
`Comparable` (what `<` orders: ints,
floats, strings, durations, enums without payloads by the order of
their cases, and structs and enums with `fn compare(other: T) -> int`;
a struct isn't ordered field by field) and `Hashable` (what a map's key
can be: the built-in values but floats, and the structs, enums, tuples,
optionals, arrays, maps and sets made of them, or that have their own
`hash`; see [Values and types](https://asuka.egoist.dev/docs/language/types#values-and-types) and [Equality,
hashing, text and JSON](https://asuka.egoist.dev/docs/language/declarations#equality-hashing-text-and-json)),
several joined by `&`. What a type argument must have, its values in
the generic code have: the interfaces' methods and fields, called
directly on each type it's compiled for.

```asuka
fn sortByArea<T: Shape>(xs: [T]) -> [T] = xs.sorted((a, b) => a.area() < b.area())

fn largestOnce<T: Shape & Equatable>(xs: [T]) -> [T] { … }

struct Labeled<T: Shape> {
  item: T
  fn label() -> string = "{item.area()}"
}
```

A generic function's type arguments are inferred from its arguments
and from what's expected of its result; `f<T>(…)` writes them out where
nothing says them: `json.decode<[Note]>(text)`, `flag.parse<Options>(args)`.

A generic is compiled for each type it's used with, so it can't come
back to itself with a bigger one: `fn depth<T>(x: T) -> int` calling
`depth([x])` would need `depth<[int]>`, then `depth<[[int]]>`, without
end, and so would `struct Nested<T> { more: Nested<[T]>? }`. The
compiler says where; a type that holds itself takes the same arguments
(`Tree<T>` in `Tree<T>`).

A type argument has an interface when it has its methods and fields: a
struct or an enum (an enum without payloads too), an interface asking
for them, or another type parameter constrained so. A method is the
interface's when it takes the same parameters: one that takes more,
even with defaults (`greet(loud: bool, times: int = 1)` for `greet(loud:
bool)`), isn't, since a call through the interface passes only its own. `T: Error` reads an
error's `message`.

### Control flow

`if` and `match` are values too. As one, an `if` needs an `else`, and a
block's value is its last expression:

```asuka
let label = if unread > 0 { "{unread} new" } else { "Inbox" }
let icon = match (online, unread) {
  (false, _) => "wifi.slash"
  (true, 0) => "tray"
  _ => "tray.full"
}
```

A branch whose value says nothing of its type, an empty `[]` or `[:]`
or an implicit `.case`, takes the other branch's: `if let c = chosen {
[c] } else { [] }` is a `[Story]`.

A tuple pattern (`(true, 0)`) matches each element with a pattern of
its own. Matching an optional tuple, it matches the tuple when there's
one, as a case pattern does (`none` is an arm of its own): `if let
(name, age) = found { … }`, `let (x, y) = cursor else { return }`.

A name with a type (`c: Circle`) tests what a value of an interface
is: it matches when the value is of that type, and binds it as one, as
a struct's pattern (`Circle(radius)`) and an enum's case written with
its type (`Unit.big(n)`) do: `if let c: Circle = shape { … }`,
`let sq: Square = shape else { return }`. A `match` over an interface
ends with `_`; see [Interfaces and
generics](https://asuka.egoist.dev/docs/language/functions#interfaces-and-generics).

`for` goes over an array, a set, a map's `(key, value)` pairs, a
string's chars (`for c in name`: [Values and types](https://asuka.egoist.dev/docs/language/types#values-and-types)), a
lazy sequence (a `Seq`: [Values and types](https://asuka.egoist.dev/docs/language/types#lazy-sequences)), or a
range:
`a..<b` is the ints from `a` up to `b`, without it, and `a..b` with it
(`0..<3` is `[0, 1, 2]`, and a `for` over a range makes no array).
`while cond { … }` loops while cond holds; `break` and `continue` leave
the loop, or go on to its next turn.

`for` also goes over a value of your own types: an iterator, whose type
has `mut fn next() -> T?` (`for` calls it on a copy, until it gives
`none`), or a sequence, whose `fn iter()` gives an iterator or a `Seq`
(`for` calls it once). The prelude's `Iterator<T>` names such a type,
for constraints: `fn sum<I: Iterator<int>>(xs: I) -> int`. An
iterator's `none` is its end, so one whose elements may be none (a
`T?`) gives a `Seq` instead: `fn iter() -> Seq<int?> = slots.lazy()`.

```asuka
struct Node {
  name: string
  children: [Node] = []
}

struct Walk {
  stack: [Node]

  mut fn next() -> Node? {
    let n = stack.pop() else { return none }
    stack = stack + n.children.reversed()
    return n
  }
}

extend Node {
  fn walk() -> Walk = Walk(stack: [self])
}

fn count(root: Node) -> int {
  var n = 0
  for node in root.walk() { if node.name.hasSuffix(".asuka") { n += 1 } }
  return n
}
```

`as name` after a `for`'s or a `while`'s head names the loop: `break
name` and `continue name` leave it, or go on to its next turn, from any
loop inside it (the `defer`s of every loop they leave run). Not from a
closure; and a name that's declared and never used, or used and not
declared, is an error.

```asuka
for row in grid as rows {
  for cell in row {
    if cell == target { found = true; break rows }
    if cell.isEmpty() { continue rows }
  }
}
```

`if let` and `while let` take an optional's value, or match a pattern,
and run their block when there's one: `if let u = user { … }`, `while
let job = queue.pop() { … }`, `if let .ready(v) = state { … }`. Their
condition may be a chain: lets and bools, separated by commas, in any
order. Each sees what the ones before it bound, and the block runs
when all of them hold (they're tried in order, up to the first that
doesn't):

```asuka
if let user = session.user, user.isAdmin, let team = teams[user.teamId] {
  Text("{team.name}'s admin")
} else {
  Text("Not an admin")
}
```

`if var` is `if let` whose name is a copy that may change, for code that
changes several fields and writes them back: `if var m = draft { m.read
= true; m.tags.push("seen"); draft = m }`.

`let … else` binds a value or leaves. Its `else` block runs when the
optional is none (or the pattern doesn't match), and must leave:
`return`, `throw`, `break`, `continue` or `panic(…)` (the compiler says
so when it can go on). After it, the name holds the value, not an
optional:

```asuka
fn summary(id: string) -> string {
  let message = cache[id] else { return "(deleted)" }
  let .ready(body) = message.body else { return message.subject }
  return "{message.subject}: {body}"
}
```

`throw`, `return`, `break` and `continue` may go where a value does:
they leave instead of giving one (their type is `never`, which fits
any). After `??`, they say what to do with none:

```asuka
let user = cache[id] ?? throw MailError.missing(id)
for line in lines {
  let n = line.toInt() ?? continue
  total += n
}
```

They go likewise in an `if`'s or a `match` arm's branch, and as a
closure's body: `x => x ?? throw Error("empty")`.

#### Changing what may not be there

A `?.` chain that starts at a `var` or state, through fields,
subscripts and `?.`, may be assigned (`=`, `+=`, …) and `mut`-called.
When it meets `none`, nothing happens: nothing is written (the value on
the right isn't even computed), and state's readers aren't told.

```asuka
inbox[id]?.read = true          // nothing when there's no such message
session.user?.name = draft
todos.first()?.toggle()          // an error: first() gives a value, not a place
```

`m[k, default: v]` is a map's entry, or `v` when it has none. Read, it's
a `V` (not a `V?`); written (`=`, `+=`, a `mut fn`, `push`), it writes
`m[k]`, and `v` is computed only when the entry is missing. Counting and
grouping are one line each:

```asuka
fn tally(words: [string]) -> ([string: int], [int: [string]]) {
  var counts: [string: int] = [:]
  var byLength: [int: [string]] = [:]
  for w in words {
    counts[w, default: 0] += 1
    byLength[w.count(), default: []].push(w)
  }
  return (counts, byLength)
}
```

#### Code for one platform

`@platform` is the platform the app is built for, a `Platform`:
`.iOS`, `.android`, `.macOS`, `.windows`, `.linux` or `.web`. It's a
constant, known as the app compiles: it isn't tracked, and it goes
wherever a value does, a file's top level and a component's body
included. A branch it decides that a platform doesn't take is no part
of that platform's build: not its code, not its strings, not its
views.

```asuka
import platform/device

let saveKey = if @platform == .macOS { "⌘S" } else { "Ctrl+S" }

component Toolbar {
  prop link: string

  Row {
    if @platform == .iOS || @platform == .android {
      Button("Share") { on press { device.share(link) } }
    }
    match @platform {
      .macOS | .windows | .linux => Text("Save with {saveKey}")
      _ => Text("Saved as you go")
    }
  }
}
```

That's so of an `if` and its `else if`s, of a `match @platform` (which
needn't cover `.unknown`: it's never that), of `&&`, `||` and `!` over
`@platform`'s comparisons, and of an `if` among views, whose other
branch's views never exist. Every branch is still checked, whichever
platform the app is built for: a platform's APIs are the same
declarations on every platform, so a mistake in the Linux branch is an
error when you build for iOS too. For declarations of a platform's
own, a file of its own is often clearer: see [Files for one
platform](https://asuka.egoist.dev/docs/language/packages#files-for-one-platform).

`environment().platform` is the platform a view runs on, as the system
says it (and as a test's `mount(…, platform:)` does): it's tracked, for
views. What to build for a platform is `@platform`'s.

#### Code for development builds

`@debug` is whether the app is a development build, a `bool`: true
under `asuka dev`, `asuka run` and `asuka test`, false in what `asuka
build` and `asuka publish` make. It's a constant too, folded as
`@platform` is: what a release doesn't take, a debug menu, a staging
server's address, logging you need only while you work, is no part of
it.

```asuka
let server = if @debug { "https://staging.example.com" } else { "https://example.com" }

component About {
  prop version: string

  Column {
    Text("Version {version}")
    if @debug && @platform == .iOS {
      Text("Development build, talking to {server}")
    }
  }
}
```

`!@debug` folds as well, and so do `&&` and `||` of it with `@platform`'s
comparisons. What follows a branch that's taken and returns is left out
too: after `if !@debug { return cached }`, the rest of the block isn't
in a release. That's so of `@platform`'s branches as well. Both branches are checked in every build, so the release
branch can't break unseen while you develop in the other one.

Tests are a development build, as `asuka dev`'s is: `asuka test
--release` runs them with `@debug` false, as the app ships, and so does
`asuka run --release` for a program. Benches (`asuka test --bench`)
time what a release runs, with `@debug` false.

### Components

A component is a piece of interface, declared with its parameters in
its header and everything else in its body:

```asuka
component MessageRow(message: Message) {
  prop compact = false            // a named prop, with its default
  prop accessory: View? = none    // a slot: a view the caller passes
  event archive                   // something it reports
  state expanded = false          // its own state

  Menu {
    longPress: true               // a context menu: a long press, a secondary click
    actions: [.button(title: "Archive", role: .destructive, run: () => { emit archive })]
    Row {
      gap: 12; padding: [12, 16]
      Avatar(message.from)
      Column {
        grow: 1
        Text(message.subject) { font: .headline; lines: 1 }
        if !compact { Text(message.preview()) { color: .secondaryLabel } }
      }
      accessory
    }
  }
}
```

Using it looks the same as using a built-in element. Header parameters
go in the parentheses. Everything else goes in the block:

- props: `name: value`
- handlers: `on name { … }`
- two-way bindings: `bind name: target`
- styles: `style: card`, or a list, `style: [card, pink]`
- children

```asuka
MessageRow(m) {
  compact: true
  accessory: Badge(3)
  on archive { mailbox.archive(m.id) }
}
```

Each thing has one place: a header parameter can't be set in the block,
and a prop can't be passed in the parentheses.

A component that takes children declares `prop children: View`; what's
in its caller's block goes there. A named slot is `name { … }` in the
block (`toolbar { … }`).

`emit name(args)` raises an event; the caller's `on name(args) { }`
handles it. An event nobody handles does nothing; its name read as a
value is whether the caller handles it (`if click { … }`: a tray icon
whose click opens its menu unless the caller takes it).

A handler that awaits belongs to the component that declares it, the
one that receives the event. It runs until it finishes or that
component goes, whoever emitted the event and whatever becomes of the
emitter: a sheet's `on done { shown = false; try await save(draft) }`
closes the sheet and goes on saving. It runs where it's declared (a
`push` reaches that place's stack, and an error it lets out goes to the
`Boundary` around it there). An `emit` in an async function waits for
the handler to finish and gets its error; elsewhere (a handler that
doesn't await, a plain `fn`), `emit` starts it and goes on. Cancelling
the code that's waiting stops the wait, not the handler.

A component that shows one element (its body's views are that one
element, or another component that does) takes the style props too:
what its caller gives goes on that element, over what the component
sets there.

```asuka
component Card(title: string) {
  Column {
    padding: 16; radius: 12; background: .secondaryBackground
    Text(title)
  }
}

Card("Plan") { padding: 8; width: 240 }   // padding 8, radius 12
```

#### Variants a component declares

A component that shows one element can name its states for its caller
to style: `variant name = condition`, a bool that's kept up to date like
a `let`. Its caller writes `name { … }` in its block, style props laid
over that element while the condition holds, as `pressed { … }` is laid
over a `View` while it's pressed:

```asuka
pub component Checkbox {
  prop checked = false
  event checkedChange(checked: bool)
  prop disabled = false
  prop children: View

  pub variant checked = checked
  pub variant disabled = disabled

  View {
    role: .button
    on press { if !disabled { checked = !checked } }
    children
  }
}

Checkbox {
  bind checked: agree
  padding: 4; radius: 4; border: 1
  checked { background: .accent; borderColor: .accent }
  disabled { opacity: 0.4 }
  Text("I agree")
}
```

- A variant's name is its own: it may be a prop's or a state's, as
  `checked` is here. It can't be an interaction state's (`pressed`) or
  a keyword's.
- The variants are laid over everything else on the element: its own
  style, its caller's, and the interaction states. Of two that hold, the
  one declared later wins (`disabled` over `checked` here).
- A `pub variant` is for another package's callers to style; one that
  isn't stays its own package's.
- A style may hold a variant's block too (`style box { checked { … }
  }`): applied to an element whose component declares the variant, the
  block follows it; anywhere else it does nothing.
- The caller's block of a component takes the interaction states
  (`pressed { … }`, `hovered { … }`) when the element it shows is a
  container.
- `Checkbox as box` names the view of the element it shows: `box.checked`
  reads the variant, tracked, as `card.hovered` reads a state.

#### A component's body is declarations

A component's body is **not** code that runs once and is forgotten.
Each line declares something that stays true:

- `state x = …` is state the component owns. When its value reads what
  changes, it follows that ([State that follows](#state-that-follows)).
- `let y = …` is derived from what it reads, and kept up to date
  (computed only when read, and only again when what it read changed).
- Every prop value of every element is kept up to date the same way:
  `Text("Count: {count}")` changes when `count` does, and nothing else
  does.
- `if`, `for` and `match` show what their condition says, now and
  later.

When `count` changes, the only thing that runs again is what read it:
that `Text`'s text, and the `if`. No component runs again, and the
platform's views are touched only where something changed.

#### State that follows

A state's value says what it starts as. When that value reads what
changes (a parameter, a prop, other state, a `let`), the state follows
it: it's that value until it's written, and that value again when what
it read changes.

```asuka
component Edit(title: string) {
  state draft = title          // title until it's typed in; title again when it changes
  state first = peek(title)    // title once: a seed

  TextField { bind text: draft }
}
```

`state x(a, b) = e` follows `a` and `b` only: `e` again when one of them
changes, whatever else `e` reads.

```asuka
let results = items.filter(i => i.name.contains(query))
state highlighted(results) = 0                  // back to the top when the results change
```

In its own value, a state's name is its current value, as an optional
(`none` the first time). That keeps a value while it still holds, and
replaces it when it doesn't; the state's type is written:

```asuka
// The selection stays while it's in the results, and moves to the
// first one when it isn't.
state selected: string? = if let s = selected, results.any(r => r.id == s) { s } else { results.first()?.id }
```

In one handler, a write and a change of what the state follows: the
later one wins. Read right after what it follows changed, it's already
the new value.

`peek(e)` is `e`, read without following it, wherever a value is
followed: `Text("{count} (from {peek(count)})")` updates when `count`
changes, and its second read stays as it was. Handlers, tasks, an
`effect(…)`'s body and tests follow nothing, so `peek` there is an
error.

#### Handlers and statements

Code runs in handlers (`on press { … }`), functions, `task` and `effect`
blocks, and tests. There, reading state gives its current value, and
writing it updates what reads it:

```asuka
on press {
  count += 1
  todos.push(Todo(title: draft))
  todos[0].done = true      // writes todos[0].done: only what reads it (or todos[0], or todos whole) updates
  draft = ""
}
```

Writes in a handler show together when it ends (or when it reaches an
`await`).

Statements: `let`, `var`, `let … else`, assignments (`=`, `+= -= *= /=
%=`, `&= |= ^= <<= >>=`, `&+= &-= &*=`, `??=`), `if` and `if let`, `for x in xs` (an
array, a set), `for (k, v) in m`, `for (i, x) in xs.enumerated()`,
`while` and `while let`, `match`, `return`, `break`, `continue`,
`throw`, `do { } catch { }`, `defer { }`, `emit`
([Control flow](https://asuka.egoist.dev/docs/language/control-flow#control-flow)), and `fn` declarations ([Functions and
closures](https://asuka.egoist.dev/docs/language/functions#functions-and-closures)).

#### Two-way bindings

Controls show a value and change it. `bind` passes them state to do both:

```asuka
state query = ""
state notify = true

TextField { bind text: query; placeholder: "Search" }
Toggle { bind on: notify; title: "Notifications" }
```

Without `bind`, a control only reports: `TextField { text: name; on
change(t) { name = t.trim() } }`.

A component's prop can be bound the same way when the component reports
it with `event <prop>Change`: `bind value: when` on a component with
`prop value` and `event valueChange(value: T)` passes the state in, and
writes it as the component emits the change:

```asuka
component Rating {
  prop value = 0
  event valueChange(value: int)

  Stepper {
    value: float(value)
    on change(v) { emit valueChange(int(v)) }
  }
}

component Review {
  state stars = 3

  Rating { bind value: stars }
}
```

A component may also assign such a prop, as if it were its own state,
in its handlers, fns, effects and tasks (and through an element's
`bind`). What the assignment does depends on its caller:

- Bound (`bind open: shown`): the assignment emits `openChange`, which
  writes the caller's state; the prop goes on reading that state. A
  caller that doesn't write it keeps the prop as it was.
- Not bound (a value, or nothing): the value is the component's own
  from then on, and `openChange` is still reported to a caller that
  handles it. When the caller's value changes, it replaces the
  component's.

```asuka
component Disclosure(title: string) {
  prop open = false
  event openChange(open: bool)
  prop children: View

  Column {
    Button(title) { on press { open = !open } }
    if open { children }
  }
}

Disclosure("Details") { Text("…") }                  // its own
Disclosure("Details") { bind open: shown; Text("…") } // the caller's
```

A prop changes whole: `open = …`, or `items = items + [x]`, not
`items.push(x)`. A prop without its change event can't be assigned.

#### Lists of views

```asuka
for todo in todos key todo.id {
  TodoRow(todo) { on delete { todos.removeWhere(t => t.id == todo.id) } }
}
```

`for` keeps a row per key. A row whose item changed updates its own
bindings. A new item makes a row, and a removed one disposes its row. A
row that moved is moved, not rebuilt. Directly under a `List`, rows exist
only while they're on screen, so a list of 100,000 costs what's
visible.

A `for` over items that change says what identifies one, with `key`:
without it, a row's state would stay where it is while the items under
it move. Some need none:

```asuka
for i in 0..<count { Dot(i) }                      // a range: the number is the key
for tag in tags { Chip(tag) }                      // a set: the element is
for day in ["Mon", "Tue", "Wed"] { Text(day) }     // constant: nothing moves
for (i, x) in xs.enumerated() key i { Slot(i, x) } // the position, on purpose
```

#### Effects and tasks

```asuka
effect { storage.set("draft", draft) }        // now, and when draft changes
effect(selected) { log(selected, user.plan) } // now, and when selected changes: only
task { try? await mailbox.refresh() }         // once, while the component is shown
task(query) { results = await search(query) } // and again when query changes
task(user.id) { profile = try await load(user.id) } // when the id changes, not the name
```

An effect runs at once and again whenever what it read changes, through
the functions it calls. What it read is as much as the code says:
`effect { log(user.name) }` runs again when the name changes, not the
avatar. `effect(a, b)` runs again when `a` or `b`
changes, and its body follows nothing it reads; `effect() { }` follows
nothing at all, and runs once, as the component is built. A task is async
([Async](https://asuka.egoist.dev/docs/language/async#async)): it starts when the component appears, restarts
when what's in its parentheses changes (cancelling the last run), and
is cancelled when the component goes. What's in the parentheses of an
effect or a task is compared: a change that leaves it equal does
nothing, so `task(user.id)` goes on when the user's name changes. The
`app` block takes them too: the app's own, for as long as it runs (a
refresh loop, a setting that follows the app's state: `effect {
appearance.setColorScheme(scheme) }`).

An effect may write state, but not state it follows: `effect { if n > 3
{ n = 3 } }` would run again after its own write, a pass late, after a
handler already saw `n` at 4. The compiler says so, and what says it
instead: a state that follows (`state n: int = min(n ?? 0, 3)`,
`state selected(results) = 0`), an `effect(…)` that follows something
else, or the handler that changes what it reads. A task may write what
it follows: it restarts by cancelling, and ends.

##### Cleanups

```asuka
effect() {
  let id = bus.listen("refresh", e => reload())
  cleanup { bus.unlisten(id) }          // once: as the component goes
}
effect(roomId) {
  let room = roomId
  chat.join(room)
  cleanup { chat.leave(room) }          // before the next run, and as it goes
}
cleanup { drafts.save(draft) }          // as the component goes
```

`cleanup { … }` in an effect's body runs before the effect's next run
and when the effect goes: what the run set up, it undoes. What's set up
once, while the component shows, is an `effect() { }`'s: what it calls
may read and write what it registers with (a list of listeners) without
the effect following it. It may be
under an `if` (it undoes only what that branch did) or in a loop (once
for each time it was reached), and several run last first. In a
component's, a store's or the `app`'s body, beside its effects and
tasks, it runs when the instance goes, after its effects' cleanups.

A cleanup is a closure: it sees the run's locals, it doesn't await or
throw (it runs as its owner goes, and the cleanups after it still have
to: catch with `try?` or `do { } catch { }`), and what it reads isn't
followed. It reads state as it is when it runs. Before the effect runs
again, that's after what the effect follows has changed, so
`cleanup { chat.leave(roomId) }` would leave the new room: keep the
run's value in a `let`, and the compiler warns when you don't
([cleanup-reads-what-it-follows](https://asuka.egoist.dev/docs/errors/cleanup-reads-what-it-follows)).
What it writes counts as its effect's writes.

Handlers, functions, tasks and tests don't take one: a task undoes what
it started with `defer`, which runs as it ends or is cancelled, and a
handler or a function has no owner of its own to run it.

### Stores

A store is state with the functions that change it, shared by the views
that use it:

```asuka
pub store Mailbox {
  state messages: [Message] = []
  let unread = messages.count(m => !m.read)

  pub fn markRead(id: string) {
    if let i = messages.index(m => m.id == id) { messages[i].read = true }
  }

  pub async fn refresh() throws {
    messages = try await http.get("https://example.com/inbox").json()
  }
}
```

A store is made like a struct (`Mailbox()`) and belongs to the component
or store that made it. `provide` makes it available to everything below,
and `use` gets it:

```asuka
app {
  provide Mailbox()
  Stack { Inbox() }
}

screen Inbox {
  use mailbox: Mailbox
  …
}
```

What a component provides is for the views it shows: two components
side by side that each provide a `Theme` see their own, not each
other's.

A `use` that nothing above provides is an error, which says the store
to provide (in a test, `mount(…, provide: [Api.fake(), Mailbox()])`,
made in order as `provide`s are). `use` with an optional type is none
when nothing above provides one: a
component that works alone and in a group (a checkbox, in a checkbox
group or not).

```asuka
store Group {
  pub state chosen: [string] = []
}

component Choice(value: string) {
  use group: Group?

  Text(if let g = group, g.chosen.contains(value) { "✓ {value}" } else { value })
}
```

`state` at the top of a file is the app's: one for the whole app.

A store takes type parameters as a struct does, inferred from its
arguments or from what's expected:

```asuka
store Kept<T>(key: string, initial: T) {
  pub state value: T = initial
}

let size = Kept("size", 17.0)                 // a Kept<float>
let tags: Kept<[string]> = Kept("tags", [])
```

In a store's fns, `self` is the store itself, to hand to what it makes.

### Async

Work that waits (the network, a file, a timer) is `async`, and waiting
for it is `await`:

```asuka
async fn loadThread(id: string) throws -> Thread {
  let (messages, people) = try await all(api.messages(id), api.people(id))
  let html = await spawn { renderMarkdown(messages) }
  return Thread(messages, people, html)
}
```

- Calling an async function needs `await`. Handlers, tasks and tests
  may await; a component's body and its `let`s may not. A closure is
  async when it's passed as an async function or when its body awaits,
  and `async () => …` marks it so ([Functions and
  closures](https://asuka.egoist.dev/docs/language/functions)).
- **The main thread.** Everything runs on it, and `await` lets other work
  run while yours waits; state can change there and only there.
- `all(a, b)` runs calls at once and waits for every result; `race(a, b)`
  takes the first and cancels the rest; `sleep(300ms)`;
  `withTimeout(5s) { … }`, whose value is its block's (`let page = try
  await withTimeout(5s) { return try await fetch(url) }`), and which
  throws when the time runs out. They're structured: what they cancel has
  stopped, its `defer`s run, before they return or throw.
  `await never()` waits for nothing but the task's end: a stream that
  ends only when its reader goes, or a task that awaits something before
  it holds it (`try await register(); defer { release() }; await
  never()`). What a component holds without awaiting is a `cleanup { }`
  ([Cleanups](https://asuka.egoist.dev/docs/language/components#cleanups)).
- `all(items, f, limit:)` calls `f`, an async function, with each of
  `items` at once, and gives their results in the items' order: `let
  pages = try await all(urls, u => try await http.get(u))`. `limit: 8`
  runs at most 8 at a time, the next starting as one finishes; left out
  (or 0), all of them start at once. As `all(a, b)`, it's structured: the
  first that throws cancels the others and starts no more, and it throws
  once they've stopped; the task that awaits it going cancels them all.
  Each runs on the main thread as a task of its own, so it can wait on
  the network or a file and change state; work for another thread is
  `spawn`'s, which `f` may await.

  ```asuka
  async fn fetchAll(urls: [string]) throws -> [string] {
    let pages = try await all(urls, u => try await http.get(u), limit: 4)
    return pages.map(p => p.text())
  }
  ```
- `spawn { … }` runs a block on another thread, for work that would hold
  the main thread up (parsing, image processing). It may use values only,
  not state, a file's vars, stores, views, native objects or C pointers,
  nor a file's `let` that holds one (a database): the compiler says so
  if it does. On
  the web it runs on one of the page's Web Workers, which make the
  file's values for themselves, and nothing else. Work that keeps
  something between calls (an index, a database it opens) is a
  [worker](#workers).
- `for await message in socket.messages { … }` reads a stream, a
  `Stream<T>`: values that come over time. `Stream(send => { … })`
  makes one: the block runs as a task when the stream is first read,
  and each `send(x)` is a value; the stream ends when the block
  returns, and fails with what it throws. Leaving the loop (`break`, an
  error, the owner going) cancels the block. A stream has one reader.

  ```asuka
  fn levels() -> Stream<float> = Stream(send => {
    try await battery.watch(changed: level => send(level))
  })
  ```
- `live(initial, source)` is a value a stream keeps current, as a
  function that reads it: read in a binding, it follows `source()`'s
  stream from the first such read until the last reader goes (then the
  stream is cancelled: nothing watches the platform for no one); read
  elsewhere, it's the last value known. It's how a package offers what
  the platform changes, such as the network's state:

  ```asuka
  let level = live(1.0, levels)
  pub fn batteryLevel() -> float = level()
  ```
- **Cancellation.** A task belongs to what started it (a handler of a
  component's event, to the component that declares it:
  [Components](https://asuka.egoist.dev/docs/language/components)). When that goes (the user leaves the
  screen), the task is cancelled: the `await` it's at ends it, its
  `defer`s run, and no error is reported.

#### Workers

`spawn` runs a block once, on another thread. A `worker` keeps a value
there: a type whose instances each run on a thread of their own, with
vars that are theirs alone.

```asuka
worker SearchIndex {
  var words: [string: [int]] = [:]

  pub fn add(id: int, text: string) {
    for w in text.split(" ") {
      let k = w.lower()
      words[k] = (words[k] ?? []) + [id]
    }
  }
  pub fn find(q: string) -> [int] = words[q.lower()] ?? []
}

store Notes {
  let index = SearchIndex()
  state results: [int] = []

  pub async fn add(id: int, text: string) { await index.add(id, text) }
  pub async fn search(q: string) { results = await index.find(q) }
}
```

- A worker's `var`s, `let`s and parameters (`worker Index(language:
  string)`) are its own: only its fns read and change them, on its
  thread. It has no `state` (views don't read it), props, events or
  views.
- Its `pub fn`s, called from outside, are async: `await
  index.find(q)`. Their arguments go over as values, the calls run one
  at a time in the order they were made, and the result, or the error a
  `throws` fn throws, comes back as a value. Inside the worker they're
  ordinary functions that don't await; its other fns are its own.
- What crosses is what `spawn` takes: values, never state, stores,
  views, closures or native objects; a worker's fns don't read the main
  thread's state either. It may make and keep a C handle of its own (an
  [extern type](https://asuka.egoist.dev/docs/c#types-c-owns), a database it opens): it never
  leaves the worker, and it's released there.
- A worker belongs to the component or store that made it. When that
  goes, the worker stops: the call it's running finishes, and those
  waiting are cancelled, which their callers' tasks see as their own
  cancellation, not an error. One that nothing owns stops once nothing
  refers to it and the calls it was given have run.
- On the web, each worker is a Web Worker of its own.

#### Data that loads

`load` keeps an async result in a component:

```asuka
screen Profile(id: string) {
  let user = load api.user(id)   // runs again when id changes

  match user {
    .pending => Spinner()
    .failed(e) => ErrorView(e) { on retry { user.reload() } }
    .ready(u) => ProfileCard(u) { opacity: if user.loading { 0.6 } else { 1 } }
  }
}
```

A `Loading<T>` is `.pending`, `.ready(value)` or `.failed(error)`, and
it's `.pending` only before its first answer. When what its arguments
read changes, or `.reload()` runs it again, it keeps the case it's in
until the new answer comes: `ProfileCard` stays on screen with its state
and its scroll position, and the run that's going stale is cancelled.
An answer equal to the last changes nothing.

- `.value` is what it shows, none before the first answer. A run that
  fails keeps it, so `.failed` can show the old content beside the
  error.
- `.loading` is true while a run is in flight, the first one included:
  dim the content, or show a ring in a corner. Reading it doesn't follow
  the value, and a `match` doesn't follow it.

```asuka
let results = load api.search(query)

TextField { bind text: query; placeholder: "Search" }
if results.loading { ProgressRing() }
match results {
  .pending => Spinner()
  .failed(e) => Text(e.message)
  .ready(items) => List { for r in items key r.id { ResultRow(r) } }
}
```

As the user types, the list keeps the last results, its scroll position
and its rows' state until the new ones land, and the ring says a search
is running. What else reads `query` shows the new text at once.

To start over instead (one person's profile shouldn't show the last
one's while the next loads), read `.loading` in the `match`'s place:

```asuka
if user.loading {
  Spinner()
} else {
  match user {
    .pending => Spinner()
    .failed(e) => ErrorView(e) { on retry { user.reload() } }
    .ready(u) => ProfileCard(u)
  }
}
```

More often the content is its own already: a screen pushed per id, a
row per key, each with a `load` of its own.

##### Changes shown at once

A toggle, a rename, a like: the screen should show the change before
the server says so, and take it back if the save fails.
`x.optimistic(f)` shows what `x` shows, changed by `f`, at once:

```asuka
struct Todo {
  id: int
  title: string
  done: bool = false
}

async fn fetchTodos() -> [Todo] {
  await sleep(100ms)
  return [Todo(id: 1, title: "Milk"), Todo(id: 2, title: "Bread")]
}

async fn saveDone(id: int, done: bool) throws {
  await sleep(100ms)
}

fn marked(t: Todo, done: bool) -> Todo {
  var c = t
  c.done = done
  return c
}

screen Todos {
  let todos = load fetchTodos()

  match todos {
    .pending => ProgressRing()
    .failed(e) => Text(e.message)
    .ready(ts) => List {
      for t in ts key t.id {
        Toggle {
          on: t.done
          title: t.title
          on change(done) {
            let id = t.id
            todos.optimistic(ts => ts.map(x => if x.id == id { marked(x, done) } else { x }))
            try await saveDone(id, done)   // a throw undoes it
            await reload fetchTodos()      // its answer replaces it
          }
        }
      }
    }
  }
}
```

- The change is a layer over the load's answer. `match`, `.value` and
  the rows see it; what reads the paths it changed runs again, nothing
  else: the toggle above runs its own row.
- It goes at the first of: the load's next answer, from a run that was
  in flight or started after it (a `reload()`, a change of what its
  arguments read), shown in the same frame, so the old value never
  flashes back; a throw out of the handler, the `fn` or the `task` that
  made it, undone before the error goes on; and the end of that code
  with no run in flight or asked for. That last one is a mistake, which
  a development build says (optimistic-unconfirmed): ask the load again
  before it ends.
- Several stack in the order they were made, and each goes on its own:
  two toggles in flight, the first one failing, undo only the first.
- `f` is a function of what's shown: it runs again when what's under it
  changes, so it reads no state and writes none
  ([optimistic-impure](https://asuka.egoist.dev/docs/errors/optimistic-impure)). Read what it
  needs into a `let` first, as `id` above.
- On a load that isn't `.ready` there's nothing to change, and it does
  nothing.

##### Reloading what a change touched

A note renamed on its edit screen: the list screen under it has a load
of its own, which `note.reload()` doesn't reach. `reload` names what a
change touched by the call that reads it, and every load of that call
asks again, wherever it is: any component, any screen in any stack,
any window.

```asuka
struct Note {
  id: int
  title: string
}

store Api {
  state notes = [Note(id: 1, title: "Milk")]

  async fn list() -> [Note] {
    await sleep(100ms)
    return notes
  }

  async fn note(id: int) -> Note {
    await sleep(100ms)
    return notes.find(n => n.id == id) ?? Note(id: id, title: "")
  }

  async fn rename(id: int, title: string) {
    await sleep(100ms)
    if let i = notes.index(n => n.id == id) { notes[i].title = title }
  }
}

screen Notes {
  use api: Api
  let notes = load api.list()

  List {
    for n in notes.value ?? [] key n.id {
      Button(n.title) { on press { push(EditNote(n.id)) } }
    }
  }
}

screen EditNote(id: int) {
  use api: Api
  let note = load api.note(id)

  Button("Rename") {
    on press {
      await api.rename(id, "Oat milk")
      await reload api.list       // the list under this screen
      await reload api.note(id)   // this note's loads, not the others'
      pop()
    }
  }
}

app {
  provide Api()
  Stack { Notes() }
}
```

- `reload f(args)` reaches the loads of `f` whose arguments are equal
  to `args` (`==`, as dependencies are compared); `reload f`, every
  load of `f`, whatever its arguments. A store's method reaches the
  loads of every instance's.
- Each one runs as `x.reload()` does: it holds what it shows, and its
  `.loading` is true until it answers.
- `await reload …` waits until every load it reached has answered or
  failed (one that goes meanwhile counts): their failures are each
  load's to show, and it throws only when the waiting is cancelled.
  Without `await`, the loads run and the code goes on.
- The call is the key: a load of a function or a store's method. One
  of a closure's call, or of a value's method, has its `x.reload()`.

##### Loads that land together

A profile and its posts, loaded side by side, swap in their answers one
at a time when `id` changes: for a moment the new person shows over the
last one's posts. `load (a(…), b(…))` is one load of several calls,
whose answers are shown together:

```asuka
struct User {
  name: string
}

async fn fetchUser(id: string) -> User {
  await sleep(100ms)
  return User(name: "Ana")
}

async fn fetchPosts(id: string, page: int) -> [string] {
  await sleep(300ms)
  return ["Hello"]
}

screen Profile(id: string) {
  state n = 1
  let page = load (fetchUser(id), fetchPosts(id, page: n))   // shown together

  Column {
    match page {
      .pending => ProgressRing()
      .failed(e) => Text(e.message)
      .ready((u, posts)) => Column {
        Text(u.name) { font: .title }
        for p in posts key p { Text(p) }
      }
    }
    Button("More") {
      disabled: page.loading
      on press { n += 1 }
    }
  }
}
```

- It's a `Loading<(A, B, …)>`, matched as any other: `.ready((u, posts))`.
- Each call keeps its own dependencies: `n` above runs `fetchPosts`
  alone, and the user stays. A change of `id` runs both, and the new
  user is held until the new posts land.
- `.pending` until every call answered once; `.failed(e)` when one
  failed, the first in source order, `.value` keeping the last whole
  tuple; `.loading` while any call runs.
- `page.reload()` runs every call; `reload fetchPosts(id, page: n)` runs
  that one; `page.optimistic(p => (p.0, p.1 + [draft]))` changes the
  tuple.
- Each element is a call: a value has nothing to wait for
  ([group-not-calls](https://asuka.egoist.dev/docs/errors/group-not-calls)).

### Errors

A function that can fail says `throws`, and calling it needs `try`:

```asuka
enum MailError: Error { offline, server(code: int) }

async fn send(m: Message) throws {
  if !network.online { throw MailError.offline }
  …
}

on press {
  do {
    try await send(draft)
    dismiss()
  } catch MailError.offline {
    showOffline = true
  } catch e {
    alert("Couldn't send", message: e.message)
  }
}

let n = try? parse(text)   // none if it throws
```

`catch` after an expression gives a fallback: the expression's value,
or when what's in it throws, the value of the catch's block (its last
expression, as an `if`'s), unless the block leaves:

```asuka
let port = try parsePort(text) catch e {
  log(e.message)
  80
}
let size = try await fileSize(path) catch { return }
try save(draft) catch { failed = true }   // a statement: no value to give
```

Its clauses are `do`'s: `catch`, `catch e`, `catch e: ParseError`,
`catch MailError.server(code)`, several in a row; the first that takes
the error runs. An error none takes goes on, so the function must throw,
as for `try`. One whose clauses take every error can't throw, so it goes
where a `try` can't: in a component's `let` (`let port = try
parsePort(text) catch { 80 }`, kept up to date as `text` changes), or a
closure that mustn't throw. It works with `try await`, and with `throw`
as a value: `let user = cache[id] ?? throw MailError.missing(id)`.

An error's `message` is its `message` field, its `fn message() ->
string`, or its type's and case's name. `Error("…")` makes an error of a
message. A `catch` never sees a task's cancellation: it goes on to the
task's end, running the `defer`s on its way.

An `Error` is matched by an error type's cases too, as a `catch` takes
them: `match e { MailError.offline => …; MailError.status(code) => …; _
=> e.message }` (an `Error` can be any error, so the match ends with
`_`).

An error a handler or a task doesn't catch goes to the nearest
`Boundary` around the component, and to the app's (shown in
development, logged in release) when there is none:

```asuka
Boundary {
  Feed()
  on error(e) { Text("Something went wrong: {e.message}") }
}
```

### Styles

Every element takes the style props: layout first (`direction`,
`justify`, `align`, `gap`, `padding`, `margin`, `width`, `height`,
`grow`, `shrink`, `absolute`, `top`, `grid`, ...), then look
(`background`, `radius`, `border`, `opacity`, `shadow`, `font`,
`color`, `lines`, ...). See [Layout and style](https://asuka.egoist.dev/docs/layout).

A `style` declares props to reuse, and `style:` applies one, or a list
of them:

```asuka
style card {
  padding: 16; radius: 12; gap: 8
  background: .secondaryBackground
}

style pink { background: #ff2d55 }

View { style: [card, pink]; opacity: 0.9 }
```

A later entry wins a prop both set, and the block's own props win over
the whole list, wherever `style:` is in the block. A style applies
others the same way, and its own props are over theirs:

```asuka
style largeCard { style: card; padding: 24 }
```

A style may take parameters, as a `fn` does (defaults, named
arguments). A parameter is a binding where it's used: `chip(tint)`
follows `tint`, and only the props that read it change.

```asuka
style chip(tint: Color, size: float = 13) {
  padding: [4, 10]; radius: 999
  background: tint.opacity(0.15); color: tint; font: Font(size: size)
}

Text(tag) { style: chip(.green) }
Text(count) { style: [card, chip(tint, size: 15)] }
```

An entry may be an `if` or a `match`: the styles of the branch it takes
apply, and when it takes another, each prop they set goes back to what
the entries under it say (or the platform's default). Here an `if` needs
no `else`; a `match` needs no arm for every case and binds no names, and
an arm's `[]` applies nothing.

```asuka
View {
  style: [
    card,
    if selected { chip(.accent) },
    if m.urgent { urgent } else if m.read { read },
    match m.kind { .note => note, .task => [task, bold], _ => [] },
  ]
}
```

A style holds style props, `style:`, variants (below) and `use`. A
`pub style` is another package's too: `style: theme.card`, its values
read in the package that declares it. A style can't apply itself,
through others either.

An element that sets no style looks like the platform's own.

**Themes.** A style's values read its package's `let`s and `state`, so
a token is one of those (`pub let hairline = 2.0`), and a `state` one
changes everywhere it's read. A style may `use` a store a component
above provides, found where the style is applied: a part of the app
that provides its own has its own tokens.

```asuka
pub store Theme(hairline: float = 2) {
  pub state border = hairline
}

pub style framed { use t: Theme; border: t.border; borderColor: .separator }

app {
  provide Theme()
  Stack { Settings() }
}

component Settings {
  provide Theme(hairline: 1)   // thinner in here
  Column { style: framed }
}
```

**Default styles.** A style named after an element (or a component that
takes the style props) is its default: every one of that kind written in
the package takes it, under its `style:` list. Another package's
elements, and the ones inside its components, keep theirs.

```asuka
style Button { radius: 8 }
style View { borderColor: .separator }
```

A container (`View`, `Row`, `Column`, `SafeArea`) takes **variants**:
blocks of style props named after an interaction state, laid over its
style while it's in that state (CSS's `:hover` and `:active`). They are
`hovered`, `pressed`, `dragging` (while it's dragged) and `dragOver`
(while a drag it takes is over it), laid in that order, so `pressed`
wins over `hovered`. A variant sets only the props it names, and its
values are bindings like any prop's:

```asuka
View {
  padding: 16; radius: 12; background: .secondaryBackground
  hovered { borderColor: .accent }
  pressed { background: .separator; opacity: 0.8 }
  on press { open() }
}
```

A view reports a state only once something asks for it, so one with no
`pressed` variant keeps the platform's own pressed look. A variant may
take a `style:` too (`pressed { style: raised }`), under its own props,
and a style may hold variants, which the containers it's applied to
take (and the other elements leave):

```asuka
style button {
  padding: [8, 16]; radius: 8; background: .accent
  pressed { opacity: 0.7 }
}
```

A component may declare variants of its own (`variant checked =
checked`, see [Components](https://asuka.egoist.dev/docs/language/components#variants-a-component-declares)),
and a style may hold their blocks as it holds the interaction states':
on an element whose component declares the variant, the block is laid
on while it holds, over the states'; on any other element it does
nothing. A misspelled name is an error: a style names only the variants
of its own package's components and other packages' `pub` ones.

```asuka
// For a component that declares variant selected (a listbox's option).
style option {
  padding: [6, 10]; radius: 6
  pressed { opacity: 0.8 }
  selected { background: .accent; color: .onAccent }
}
```

A component's variants are over its caller's props: a caller's
`background` replaces the component's, and its `pressed` still darkens
it. Other views follow a
view's states through its name ([Naming a view](#naming-a-view)):
`View as card { … }` then `card.hovered` (or `card.pressed`,
`card.dragging`, `card.dragOver`) in any binding, a child's (a group's
hover) or a sibling's. What isn't a style uses the handlers: `on
pressedChange(p)`, `on hoverChange(h)`.

#### Naming a view

Some things are done to a view rather than set on it: play a video,
run script in a page, put text in a field, scroll a list. A `Ref` is a
view's handle: `ref: r` on any element puts its view in `r`, and
`Element as name` declares one for the rest of the block it's in (on a
component that shows one element, that element's view).

```asuka
component Player {
  let video = Ref()

  Video("intro.mp4") { ref: video }
  Button("Start over") {
    on press {
      video.seek(0)
      video.play()
    }
  }
}
```

A Ref holds its view while the view is shown, and nothing before or
after: a command then does nothing, and a state is false. Its states
are tracked, so a binding that reads one follows it:

| On | Ref's members |
|---|---|
| any view | `hovered`, `pressed`, `dragging`, `dragOver` (bools, tracked); `focus()`, `blur()` |
| a component's (`Checkbox as box`) | its variants: `box.checked` (bools, tracked) |
| `Scroll`, `List` | `scrollTo(x, y, animated: true)` |
| `Video` | `play()`, `pause()`, `seek(seconds)` |
| `WebView` | `try await eval(script)` (its value, as JSON), `run(script)` (without waiting), `goBack()`, `goForward()`, `reload()`, `stopLoading()` |
| `TextField` | `insert(text)`, `format(.bold, on)`, `toggleFormat(.italic)`, `setLink(url)` (a rich field's selection) |

A Ref can be passed along (`device.shareFiles(paths, from: button)`,
`accessibility.focus(field)`), and kept in a file's `let` for views of several
components.

### Animation

`animate { … }` runs its block at once, and the changes it makes to
frames and to what's drawn (opacity, background, a corner's radius, a
shape's trim) animate natively. `animate(.spring) { … }` takes an
`Animation`: its duration, curve, delay, repetitions, a spring's
damping and velocity. Awaited, it returns once the animation is
complete; it needn't be.

```asuka
on press {
  animate { expanded = !expanded }
}
on press {
  await animate(Animation(duration: 500ms, curve: .spring)) { offset = 0 }
  done = true
}
```

`animate(() => …)` and `animate(anim, () => …)` are the same as
closures. `repeat: Animation.forever` repeats until the props change
again. `animating()` says whether the changes made now animate.
`onFrame(t => …)` calls its closure at each frame the display draws,
with the frame's time, until the closure it returns is called or the
component goes: `let stop = onFrame(t => tick(t))`. `frames()` is the
same as a `Stream<Duration>`, drawn only while it's read: `for await t
in frames() { … }` in a task, which stops as the loop ends or the task
goes.

### Drawing

`Canvas(g => …)` draws with a `Graphics`, in points from its top left,
once it's laid out, and again when its size or what it read changes;
`Shape(fn(width, height) -> Path)` is an outline the platform fills and
strokes (`fill`, `stroke`, `lineWidth`, `trimEnd`, `dash`, ...).

```asuka
Canvas(g => {
  let ring = g.path().arc(g.width / 2, g.height / 2, 40, 0, 6.28318)
  g.stroke(ring, .accent, StrokeStyle(width: 8, cap: .round))
  g.text("{percent}%", g.width / 2, g.height / 2, CanvasText(size: 20, align: .center))
}) { height: 100 }
```

A `Path` is built by its methods, which return it: `Path().move(x,
y)`, `line`, `quad`, `cubic`, `rect`, `roundedRect`, `ellipse`, `arc(cx,
cy, radius, start, end, clockwise: true)` (radians from the positive x
axis), `close()`; `g.path()` is an empty one of the canvas's own,
reused from draw to draw. A Graphics is valid while its draw runs:
`width`, `height`, `fill(path, color)` (or a `LinearGradient`, a
`RadialGradient`), `stroke(path, color or gradient, style)`, `save()`,
`restore()`, `translate`, `scale`, `rotate(degrees)`, `opacity`,
`clip(path)`, `transform(a, b, c, d, tx, ty)`, `evenOdd(on)`,
`text(s, x, y, style)`.

A `shader` declaration is a box a GPU program draws, written in
Asuka's syntax and compiled with the app (a mistake in it is a compile
error at its line), used as a view, its parameters its uniforms:

```asuka
shader Ring(tint: Color, thickness: float = 0.05) {
  fn fragment(pos: vec2) -> vec4 {
    let d = abs(distance(pos / size, vec2(0.5)) - 0.4)
    return tint * smoothstep(thickness, 0, d)
  }
}

component Badge {
  Ring(tint: .accent) { width: 120; height: 120 }
}
```

`fn fragment(pos: vec2) -> vec4` gives a pixel's color, premultiplied;
`size`, `scale` and `time` are the box's size in points, the pixels a
point and the seconds since it showed (`animated: true` runs it). A
parameter is a `float`, an `int`, a `bool` or a `Color`. See
[Shaders](https://asuka.egoist.dev/docs/components#shaders) for the language a shader is
written in and where it runs.

### The system

What a component uses to be part of the app is the prelude's, with no
import: what it registers while it lives, its environment, alerts, and
the app's windows.

| Function | What it does |
|---|---|
| `useKeys([Shortcut(…)])` | the window's keys while the component lives |
| `onOpenURL(u => …)`, `onOpenFiles(paths => …)`, `onShare(items => …)` | what the app is opened with, or what other apps share with it, while the component lives |
| `quit()`, `relaunch()` | quits the app as the user's Quit does (and starts it again), asking it first; desktop only |
| `onCloseRequest(fn() -> bool)`, `onQuit(e => …)`, `onSecondInstance((args, dir) => …)` | asked before the window the component is in closes; hears the app about to quit (`e.cancel()`), or started again, while the component lives |
| `onBack { … }` | asked when the user goes back from the screen, sheet or popover the component is in: `true` takes it (the screen stays), `false` lets it go |
| `onLifecycle(l => …)` | hears the app go to the foreground and the background, and memory warnings (a `Lifecycle`), while the component lives |
| `alert(title, message:, actions:)` | shows an alert now; its `actions` are `.button`s (none: an OK). An `Alert` element shows one while its `open` is true |
| `snackbar(message, action:, duration:, onDismiss:)` | a brief message at the bottom of the window, with an action (Undo); returns a closure that dismisses it |
| `environment()`, `screenEnvironment()` | the size, safe area, text size, platform and the rest (tracked): the screen's or sheet's own, and the whole screen's |
| `window.main()`, `window.current()` | the main window's `Ref`, and the one of the window the code is in: `show()`, `hide()`, `focus()`... from anywhere |
| `window.close()` | closes the window the code is in, without asking |

What the app asks of the system's services is in the [platform
SDK](https://asuka.egoist.dev/docs/stdlib)'s packages, imported as any other:

| Function | What it does |
|---|---|
| `device.share(text)` | the share sheet, for a text or a link |
| `try await device.shareFiles(paths, title:, from: ref)` | the share sheet for files, from a view (where a popover points); returns once it's closed |
| `device.haptic(.success)` | the platform's haptic feedback (a `device.Haptic`) |
| `device.copy(text)` | puts text on the clipboard |
| `device.openURL(url)` | opens url in the app that takes it |
| `accessibility.announce(text)` | has a screen reader say text |
| `accessibility.focus(ref)` | moves a screen reader to the view |
| `try await webview.cookies(url)` | the web views' cookies for url |
| `appearance.setColorScheme(.dark)` | the app's appearance: `.light`, `.dark`, or `.system` (the platform's) |

```asuka
import platform/device

component ShareLink(link: string) {
  Button("Share") {
    on press {
      device.haptic(.light)
      device.share(link)
    }
  }
}
```

What code the app has on each platform is `@platform`'s, a constant
known as the app compiles ([Code for one
platform](https://asuka.egoist.dev/docs/language/control-flow#code-for-one-platform)): `environment().platform`
is the one a view runs on, tracked like the rest of its environment.

`useKeys` and the `on…` registrations are items of a component's body
(or the app's block). The app's block also handles them as events: `on
openURL(url) { … }`, `on files(paths) { … }`, `on share(items) { … }`,
`on quit(e) { … }`, `on secondInstance(args, dir) { … }`, `on
foreground { … }`, `on background { … }`. Its first `Window` is the
main window; an app block that shows nothing outside its `Window`s (a
tray icon, a menu bar) has none ([Navigation](https://asuka.egoist.dev/docs/navigation#windows)).

`std/embed` compiles the package's files into the app, read as it
compiles, so every platform has them with nothing to load:

```asuka
import std/embed

struct Config { api: string; retries: int }

let welcome = embed.text("welcome.md")         // string
let logo = embed.bytes("art/logo.png")         // bytes
let config: Config = embed.json("config.json") // checked as the app compiles
let posts = embed.texts("posts/*.md")          // [path: text]
let icons = embed.files("icons/**/*.svg")      // [path: bytes]
```

A path is a string literal, relative to the package's directory (where
its `asuka.toml` is), inside it; a glob has `*`, `?`, `[abc]` and `**`
(any directories) and never names a file whose name starts with a dot.
`embed.json` reads the file as `json.decode` does, but as the app
compiles: a file that doesn't fit the type is a compile error, with its
line and column, not one at run time. A file that isn't there, or a
glob that names none, is a compile error too. With `asuka dev`,
changing an embedded file reloads the app in place, as changing its
code does. Files a platform reads as the app runs (images, fonts) go in
`assets/` instead ([std/embed](https://asuka.egoist.dev/docs/std/embed)).

### Navigation

```asuka
app {
  Tabs {
    Tab("Inbox") { icon: "tray"; Stack { Inbox() } }
    Tab("Settings") { icon: "gear"; Stack { Settings() } }
  }
}

screen Inbox {
  title: "Inbox"
  List {
    for m in messages key m.id {
      MessageRow(m) { on press { push(Thread(m.id)) } }
    }
  }
  toolbar {
    Button("Compose") { icon: "square.and.pencil"; on press { present(Compose()) } }
  }
}
```

A `screen` is a component whose root is a screen of a stack: its block
takes the screen's props (`title`, `largeTitle`, ...) and slots
(`toolbar`, `bottomToolbar`, `primary`). `push(…)` and `pop()` act on
the nearest stack, `present(…)` and `dismiss()` on sheets. `Split` is a
sidebar of `Destination`s (in `Sidebar` sections) beside the chosen
one's stack.

With a `Router`, every screen has an address. A screen declared with
its pattern is routable, its parameters typed; a call of it is its
address, checked as the app compiles:

```asuka
app {
  Router(initial: "/inbox") {
    Route("/inbox", layout: .stack, title: "Inbox") {
      Inbox
      Message
    }
  }
}

screen Inbox at "" {
  title: "Inbox"
  Button("The first") { href: Message(id: 1) } // /inbox/1
}

screen Message(id: int) at ":id" {
  title: "Message {id}"
  Button("Back to the inbox") { on press { navigate("/inbox") } }
}
```

See [Navigation](https://asuka.egoist.dev/docs/navigation).

### Text in other languages

`tr.inboxTitle(unread: n)` says a message of the app's catalogs
(`locales/en.json`, `de.json`, ...), checked when the app compiles: a
missing key or argument, or one of the wrong type, is an error. A message
said in a binding follows the app's language, so switching it
(`i18n.setLocale("de")`, std/i18n) changes what's shown, and a language
written right to left lays the app out from the right. See
[Languages](https://asuka.egoist.dev/docs/i18n).

### Native code

What only the platform can do goes in a native package, declared in
Asuka and implemented in each platform's own language:

```asuka
pub native interface Haptics {
  fn impact(style: Feedback)
}

pub native component MapView(region: Region) {
  prop pins: [Pin] = []
  event regionChanged(region: Region)
}
```

Making a module and calling it may fail (its platform may have no code
for it), so both take `try`: `let h = try Haptics()`, `try
h.impact(.light)`. `asuka native generate` writes the Swift, Kotlin,
JavaScript and C each platform implements. See [Native
packages](https://asuka.egoist.dev/docs/native).

C is called directly. `extern` declarations say what a C function
takes and gives back, in C's types, and Asuka sees each as one of its
own (an `Int32` is an `int`, a `CString` a `string`):

```asuka
extern type Db with release db_close
extern fn db_open(path: CString) -> Db?
extern fn db_close(db: Db)
extern fn db_each(db: Db, sql: CString, row: fn(CString?) -> bool) -> Int32

extern struct Point {
  x: Float32
  y: Float32
}
```

An `extern type` is a pointer C gives, freed by its release as the last
reference goes; an `extern struct` crosses by value; a function passed
to C is a callback. `asuka.toml`'s `[c]` says what C the app compiles
and links. See [C from Asuka](https://asuka.egoist.dev/docs/c).

### Tests

```asuka
test "archiving removes the row" {
  let app = mount(Inbox(), provide: Mailbox.fake([sample]))
  await app.idle()
  app.tap("Archive")
  app.expectNo("Hello")
}
```

`expect(cond)` fails a test when cond is false, saying what failed and
where (with a comparison's two sides). JSON comes from `std/json`:
`let notes: [Note] = try json.decode(text)` reads what's expected of it
(or `json.decode<[Note]>(text)`), `json.encode(notes)` writes it. Where a `json.Value` (any JSON) is
expected, a value JSON can say is one, as it's written: a map of values
of mixed types is a `[string: json.Value]` (`["port": port, "tls":
true]`), whose numbers stay numbers. A
generic function may read or write its `T` as JSON; each call then
needs a `T` JSON can say, which the compiler checks where it's called.

`asuka test` runs a package's tests with no device: it lays views out as
the platforms do, measures text at a fixed width per character, and time
moves only when nothing else can run (a `sleep(5s)` in a test takes no
time). `mount` returns the app, to drive and check: `tap`, `longPress`,
`type(_, into:)`, `submit`, `toggle`, `slide`, `select`, `swipe(_,
action:)`, `refresh`, `scrollTo`, `back`, `dismiss`, `pickDate`,
`pickColor`, `expect`, `expectNo`, `texts()`, `disabled(text)`,
`until`, `waitFor`, `idle`; `in(.navigationBar)`, `in(.sidebar)` and
`nextTo(label)` narrow what a text names; with a router, `mount(…, at:
"/inbox/1")`, `href`, `history()`, `goBack()`, `goForward()`. See
[Testing](https://asuka.egoist.dev/docs/testing).

Tests go in any file, and in a package's **test files**, whose names
end in `_test.asuka` (`parser_test.asuka`; `tray_linux_test.asuka` for
one platform). A test file's declarations and imports, its fixtures and
helpers, are part of the package only when its own tests run (`asuka
test`, `asuka check`, an editor): a build has none of them, nor any
test block, and a package that imports this one never sees them. So a
test file may import what only its tests need without that coming with
the app.

A test with parameters is a property test, run with values `asuka test`
generates for them; a failing input is shrunk to the simplest that still
fails. `bench "name" { … }` is a benchmark, timed by `asuka test
--bench`; its `measure { … }` is the part timed, after what sets it up,
and `keep(x)` keeps a result from being optimized out.
`expectSnapshot(value)` and `expectInlineSnapshot(value, "…")` compare
what a value prints as with its snapshot, which `asuka test` writes.

```asuka
fn fib(n: int) -> int {
  if n < 2 { return n }
  return fib(n - 1) + fib(n - 2)
}

test "fib grows" (n: int) {
  if n < 2 || n > 30 { return }
  expect(fib(n) >= fib(n - 1))
}

test "the first ones" {
  expectInlineSnapshot([fib(1), fib(2), fib(3), fib(10)], "[1, 1, 2, 55]")
}

bench "fib 20" {
  keep(fib(20))
}
```

## Diagnostic codes

- argument-count: a call with too many or too few arguments. Pass what the function declares; leave out only parameters with defaults.
- argument-label: a named argument the function has no parameter for. Use the parameter's name (the message suggests it), or pass the argument in order.
- async-in-effect: an `await` in an effect. Use `task(what) { … }`: it runs when the component appears and again, cancelling the last run, when `what` changes.
- await-outside-async: an `await` where nothing may wait. Mark the function `async`, or load data with `let x = load f()` or a `task { }` in a component.
- bad-operand: an operator on values it doesn't take. Convert one side (`float(n)`), use a method (`a.concat(b)`), or give the type the operator's method.
- brace-placement: a block's `{` goes at the end of the line before. Move the `{` to the end of the line before: `fn f() {`, `Row {`, `if x {`.
- cannot-infer-type: the compiler can't tell a value's type from what's written. Write the type where the value goes: `let ids: [int] = []`, `Type.case`, `f<T>(…)`.
- cleanup-reads-what-it-follows: an effect's cleanup that reads what the effect follows. Keep the run's value in a `let` and read that in the cleanup: `let r = room`, then `cleanup { leave(r) }`.
- closure-keeps-itself: a closure that calls itself through the variable it's stored in. Declare a function in the block instead, `fn name(…) { … }`: it can call itself.
- constraint-not-met: a type that lacks what a generic or a method needs of it. Use a type that has it (an id instead of the whole value), or give the type what's missing.
- deprecated-syntax: a form the language replaced, which `asuka fmt` rewrites. Run `asuka fmt -w`, or write `style: name`.
- duplicate-declaration: a name declared twice in one place. Rename one, or remove the one you don't need.
- effect-in-effect-pass: an effect's write made a binding run twice in one flush. Compute the value with a `let`, or a state that follows what the effect read: it settles in one pass.
- effect-loop: effects that keep dirtying each other in one flush. Give the value one source: compute it with a `let` or a state that follows, or write it in the handler that changes what the effect reads.
- effect-writes-what-it-follows: an effect that writes state it reads. Keep it in range where it's declared (`state n: int = min(n ?? 0, 3)`), follow something else (`effect(other) { … }`, `state x(other) = …`), or write it in the handler that changes what it reads.
- embed-file: `embed` of a file the package doesn't have. Fix the path, or add the file to the package.
- extension-conflict: an extension that adds a name the type already has, or another extension adds. Name the method otherwise, or keep one of the extensions.
- fn-body: a function without its body, or with one it can't have. Write the body (`{ … }` or `= expression`), or drop it from a native or extern declaration.
- follows-nothing: `state x()` with nothing to follow. Name what to follow, `state x(a) = …`, or write `state x = peek(…)`.
- format-spec: an interpolation's format that doesn't fit its value. Write the format for the value's type, as the message says: `{n:>6}`, `{x:.2f}`, `{n:#x}`, `{name:<12}`.
- group-not-calls: `load (…)` of what isn't each a call. Group the calls only (`load (api.user(id), api.posts(id))`), and pass the values to them, or beside the load.
- import-cycle: packages that import each other. Move what both need into a third package they both import.
- indentation: a line's indentation matches no open block, or holds a tab. Line the line up with the block it belongs to (the same indentation as the lines before it in that block), or write the block with braces.
- inline-snapshot: an inline snapshot that isn't a string written in the call. Write the snapshot as a string literal, or leave it out (`expectInlineSnapshot(value)`) and let `asuka test` write it on its first run.
- int-overflow: an integer literal that doesn't fit in its type. Use a number in the range, a wider type (`u16`, `int`, `u64`), a `float`, or, for an exact integer past 64 bits, `std/bigint`'s `BigInt` (`BigInt.parse("9223372036854775808")!`).
- interface-not-satisfied: a type that declares an interface it doesn't implement. Add the missing methods (the message lists them), with the interface's signatures.
- internal-error: a mistake of the compiler's own. Report it (with the program, or the smallest one that shows it) at https://github.com/egoist/asuka/issues.
- invalid-catalog: a catalog that isn't a valid file of messages. Fix the file where the message points: a string for each key, its placeholders as the source's.
- invalid-catch: a `catch` that can't be reached, or has nothing to catch. Put the specific catches first, the catch of everything last, and catch only what can throw.
- invalid-character: a character that isn't part of the language here. Remove the character, or put the text in a string or a comment.
- invalid-conformance: a type declares, or a type parameter needs, what isn't an interface. Name an interface there, or drop it.
- invalid-constant: a value made when the app compiles that isn't a literal, or doesn't parse. Pass the literal itself, and fix what the message says doesn't parse.
- invalid-declaration: a declaration written in a form its place doesn't take. Write it in the place's form: `let name = value` for a type's constant, a `static fn` for one that needs the type's parameters.
- invalid-entry: a program's `fn main` declared otherwise than it runs. Declare `fn main() { … }`, and read the arguments with std/os.
- invalid-extension: an `extend` of what can't be extended, or in a form it doesn't take. Extend a named type, or declare a function instead.
- invalid-extern: an extern declaration C can't stand behind. Declare the release function `extern fn close(x: T)`, and the rest as C has it.
- invalid-flag-spec: a type `flag.parse` can't read a command line into. Give each switch a default (`verbose: bool = false`), and options the types the message lists.
- invalid-for: a `for` over something it doesn't go over. Go over what the message says: `0..<n`, `m.keys()`, `for await x in stream`.
- invalid-literal: a number, a color, a duration or an escape that isn't written right. Write the literal in one of its forms, as the message says.
- invalid-modifier: `pub`, `mut`, `async`, `throws` or type parameters where they don't apply. Drop the modifier the message names.
- invalid-pattern: a pattern written in a form the language doesn't take. Write the pattern in its form, as the message shows: `.case(x)`, `if let`, `(k, v)`.
- invalid-project: the project's asuka.toml or directory can't be read as it says. Fix the value in asuka.toml as the message says.
- invalid-ref: `ref` or `as` naming what isn't one element's view. Name an element inside the component, or give the component one element to show.
- invalid-route: a screen's address or a navigation the routes can't make. Name the screen's parameters in its pattern, as it declares them.
- invalid-shader: a shader written with what the GPU's language doesn't have. Write it with numbers, vectors and GLSL's functions.
- invalid-style: a style declared or applied in a form it doesn't take. Put the styles in one list: `style: [a, b]`.
- invalid-variant: a variant declared or used in a way it can't work. Show one element (put the views in a `Row` or a `Column`), and set in a variant only what changes with it.
- let-else: a `let … else` whose else goes on, or has nothing to unwrap. End the `else` with `return` (or `throw`, `break`, `continue`), or drop it.
- load-outside-component: `load` outside a component's or a store's `let`. Declare `let x = load f(…)` in the component, or `await` the call in the function.
- load-waterfall: a load that waited for another one it doesn't use. Start both loads in the same component, side by side, and pass the results down.
- local-fn: a function in a block declared with what only a file's functions have. Declare it at the top of the file for any of those.
- long-hold: a load has held old content for seconds with nothing saying so. Read `x.loading` while it holds (dim the content, show a ring), or show a spinner in its place.
- loop-label: a loop's name that isn't used, or isn't there. Drop the name, or use it: `continue outer`.
- maybe-none: a value that may be `none` used as if it were there. Use `?.` (`p?.name`), give a default with `??`, or unwrap it first: `if let p = p { … }`.
- message-arguments: a message called with other arguments than its placeholders. Pass each placeholder by its name: `tr.greeting(name: n)`.
- misplaced-declaration: a declaration where its kind doesn't go. Move it where its kind goes, or use the kind this place holds (a worker's `var` for `state`).
- misplaced-statement: a statement where it can't do what it says. Move the statement where it means something, or write what you meant there.
- missing-await: a call of an async function without `await`. Write `await f(…)` (`try await` if it throws), or start it on its own with `spawn`.
- missing-prop: an element without a prop it needs. Set it in the element's block: `Badge { count: 3 }`, or give the prop a default.
- missing-return: a function with a result that can reach its end without one. Return a value on every path, or end the function with a `return`.
- missing-translation: a language's catalog without some of the source's messages. Translate the messages it lists, in that language's catalog.
- missing-try: a call of a function that throws without `try`. Write `try f(…)`, `try? f(…)` for `none` on an error, or `try f(…) catch { fallback }`.
- missing-type: a declaration whose type must be written. Write the type: `prop size: int`, `prop size = 12`, `state n: int = …`.
- mixed-numbers: an operator on numbers of two types. Convert one side to the other's type: `int(level) + offset`, `float(n) * 0.5`, `u8(n)` (which throws when n doesn't fit; `u8(truncating: n)` keeps its low bits).
- native-type: a native component's prop or event of a type the platform's view can't take. Use data the platform's code can read; report what happens with an event.
- no-children: children given to an element that takes none, or not those. Put the views in a container (`Column`, `Row`), or beside the element.
- no-such-operator: an operator other languages have and Asuka doesn't. Write `x += 1`, `if a { b } else { c }`, or `a < b && b < c`.
- no-value: something used as a value that gives none. Give the `if` an `else`, end the block with the value, or use a function that returns one.
- non-exhaustive-match: a `match` that doesn't cover every case. Add arms for what's missing, or `_ => …` for the rest.
- not-a-type: something used as a type that isn't one. Write the type you mean: `bool` for `void?`, one `?` for an optional.
- not-a-value: a type, a package or a method used where a value goes. Make a value of the type (`Point(x: 1, y: 2)`), use a package's member, or call the method.
- not-a-view: a value among views that isn't one. Show it in an element: `Text("{n}")`.
- not-an-error: a value thrown or caught that isn't an error type. Declare the type `: Error`, or throw `Error("…")`.
- not-assignable: a change of something that can't change. Make it a `var` (or `state`), change what a `let` reads, or declare the method `mut fn`.
- not-async-call: `load`, `all` or `race` given something that isn't an async call. Pass an async call (`load api.user(id)`), or use the value as it is.
- not-bindable: `bind` of a prop that doesn't report its changes. Bind a control's value, or pass the value (`text: s`) and handle its event.
- not-c-type: a type that can't cross to C. Pass `bytes` or a `Pointer` and a length, or declare an `extern struct`.
- not-callable: a call of something that isn't a function. Call a function, use the value as it is, or make one of the enum's cases (`Kind.name`).
- not-json: a type JSON can't read or write. Decode into a struct of data, and make the rest from it.
- not-optional: an optional's operator on a value that's always there. Drop the operator, or make the value optional where it's declared (`int?`).
- not-pub: a declaration another package, another file (private), a store or a worker keeps to itself. Mark it `pub` (or drop `private`) where it's declared, or use what's public: a type whose private fields have no default is made by its static fns.
- not-sendable: a value that can't cross to another thread. Pass the data the other thread needs, not the object that has it.
- not-supported: something the language doesn't do yet. Use the alternative the message names: a `View` or a closure prop for a generic component.
- optimistic-impure: an optimistic change that reads state, awaits or writes. Read what it needs before, into a `let`, and use that in the closure; write in the handler, not in the change.
- optimistic-not-loading: `optimistic` on what isn't a load. Change the state itself (`notes.push(n)`), or call `optimistic` on the load's `let`.
- optimistic-not-ready: `optimistic` on a load that isn't ready. Change what it shows once it's `.ready`: offer the action in the `.ready` arm, or check `x.value` first.
- optimistic-unconfirmed: an optimistic change went with its task, with no run of its load to confirm it. Before the task ends, ask the load again: `await reload f(…)`, or `x.reload()`.
- pattern-type: a pattern that can't match the value's type. Write a pattern of the value's type: `1`, not `"one"`, for an int.
- peek-untracked: `peek` where nothing is followed. Read the value itself.
- plural-categories: a plural or select message whose categories don't match its language's. Write the language's categories, as the message lists them.
- prop-not-assignable: a component assigns a prop it can't. Declare `event openChange(open: bool)` to assign it, or keep state of the component's own.
- prop-or-argument: a parameter, a prop, an event or children given in the wrong place. Move it to its place: `Badge(3) { compact: true; on tap { … } }`.
- recursive-definition: a value or a style defined in terms of itself. Break the cycle: compute one from values that don't read it back.
- recursive-type: a type that holds a whole copy of itself. Make the field on the way optional (`next: Node?`) or an array (`children: [Node]`).
- reload-arguments: `reload f(…)` with arguments `f` doesn't take. Give the arguments `f` takes, as its loads call it, or none: `reload f` reaches every load of `f`.
- reload-not-loaded: `reload` of what no load calls. Reload the function a `load` calls (`reload api.notes`), or a load by its handle (`x.reload()`).
- reserved-name: a name the language uses itself. Name it otherwise.
- return-value: a `return` with a value the function doesn't give, or without one it does. Drop the value, or declare the result (`-> T`) and return one everywhere.
- row-without-key: a `for` in a view over items that change, without a `key`. Say what identifies an item: `for t in todos key t.id`.
- spawn-captures-state: `spawn` that reads or changes state, or what isn't a value. Read the state into a `let` first, and pass that; assign what `spawn` returns.
- special-method: a method the language calls (equals, hash, description, toJSON, fromJSON) with a signature it can't call. Give the method the signature the message says, or another name if it isn't meant for that.
- statement-in-view: a statement among a component's views. Put statements in a handler (`on press { … }`), a fn, a task or an effect; a derived value is a `let`.
- syntax-error: the parser expected something else here. Read the message's "expected …, found …": write what it expected, or close what's open where it says.
- test-parameter: a test's parameter of a type asuka test can't generate. Take what the test is about as data (an id, a struct of fields), and make the rest from it in the test's body.
- too-large: a function too large for the bytecode. Split the function, or keep the data in a file (`embed.json`) instead of a literal.
- tr-in-package: `tr` in a package that isn't the app. Take the text as a parameter, and let the app pass `tr.key()`.
- type-arguments: the wrong number of type arguments. Give as many types as it declares: `Set<int>`, `Page<Item>`.
- type-mismatch: a value of another type than the one expected. Give a value of the type expected, or convert it: `float(n)`, `int(x)`, `"{n}"`.
- undefined-name: a name nothing declares where it's used. Fix the spelling (the message suggests a name close to it), import the package that declares it, or declare it.
- unexpected-block: a block after a call that takes none. Pass the values in the parentheses, and drop the block.
- unhandled-error: an error that can leave where nothing takes it. Mark the function `throws`, or catch the error: `try … catch { fallback }`, `do { } catch { }`, `try?`.
- unknown-event: a handler or an `emit` of an event the component doesn't declare. Use one of its events (the message suggests one), or declare it: `event name(…)`.
- unknown-member: a type, an enum or a package has no member of that name. Use one of its members (the message suggests one), or add it to the type.
- unknown-message: `tr.key` of a message the catalogs don't have. Use a key of the catalog (the message suggests one), or add it there.
- unknown-package: an import names no package. Fix the path, add the dependency (`asuka add`), or use the package's new name.
- unknown-prop: a prop, a slot or a variant the element doesn't have. Use one of its props (the message suggests one).
- unterminated: a string, a comment or an escape isn't closed. Close it.
- unused-catalog-entry: a catalog's description or argument that nothing uses. Remove it, or use the argument in the message.
- view-in-statement: a view made in code that runs. Declare the view in the body, under an `if` on state the handler sets.
- wide-fanout: one write ran more than 1,000 bindings and lets, more than once. Read what a row shows by its path (`todos[i].done`, `for t in todos key t.id`), or a set's `contains`, so a write runs only what reads it.
- worker-captures-state: a worker that reads the main thread's state. Read the state (or the var) on the main thread and pass its value to a `pub fn`.
- worker-misuse: a worker's own values read from outside, or a worker made or called where it can't be. Return the value from a `pub fn`, and call it with `await`.
- write-in-binding: state written where the UI follows it. Write state in a handler (`on press { … }`), a fn, a task or an effect.
- write-in-memo: state written while a `let` is computed. Write state in a handler, a fn, a task or an effect; a `let` only reads.
