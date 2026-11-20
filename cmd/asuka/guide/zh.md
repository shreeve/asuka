# 一页读懂 Asuka

这是所装 CLI 编译的 Asuka 语言，以及使用它的方式。每个错误和警告都有一个代码；`asuka explain <code>` 会显示它的页面。

## 工作流程

- 检查：`asuka check --json`（每行一条诊断，各带代码、消息、帮助和页面）
- 查询代码：`asuka explain <code>`
- 测试：`asuka test --json`（每个测试的结果，以及应用运行时核心报告的问题）；运行：`asuka dev`，程序用 `asuka run`
- 格式化：`asuka fmt -w`
- 组件的主体是声明；状态在处理函数、fn、task 和 effect 里写入。视图里的列表需要 `key`。

## 语言

### Asuka 速览

Asuka 应用用 Asuka 编写。这是一门为界面而设计的小语言，界面使用各平台自己的控件。它是静态类型的，发布版本会提前编译，开发时保存即在原处重新加载。本页按你接触这门语言的顺序来介绍它；每一部分的完整内容见[语言参考](https://asuka.egoist.dev/zh/docs/language)。

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

一个文件由声明组成：组件、函数、类型、状态。`app { }` 是应用的根，其余部分的顺序随意。

#### 值

```asuka
fn values() -> string {
  let name = "Asuka"               // 常量：类型由推断得出
  var visits = 0                   // 变量
  visits += 1
  let greeting = "Hello, {name}"   // {…} 把值放进字符串
  let ratio: float = 2.5
  let level: u8 = 200              // 固定位宽的整数：i8 … i32、u8 … u64（int 是 64 位）
  let area = 2 ** 10               // 乘方：1024
  let tags = ["new", "work"]       // 数组：[string]
  let ages = ["ana": 31, "bo": 28] // 映射：[string: int]
  let nickname: string? = none     // 可选值：一个字符串或 none
  let shown = nickname ?? name     // none 时的后备值
  return "{greeting} {visits} {ratio} {level} {area} {tags.count()} {ages["ana"] ?? 0} {shown}"
}
```

`var` 用在函数和处理函数里：文件顶层的值不会改变，除非它是 `state`。不同类型的数字不能混在一起运算：`int(level) + visits` 先把 `u8` 转换成 `int`；算术结果超出类型的范围是一个错误，而不会回绕成另一个数字。值总是被复制，从不共享：修改数组的副本不会影响原来的数组。字符串、数组和映射都有你预期的方法（`count()`、`contains`、`map`、`filter`、`sorted`、`split` ……），见[值与类型](https://asuka.egoist.dev/zh/docs/language/types)。

#### 函数

```asuka
fn area(width: float, height: float = 1) -> float {
  return width * height
}

fn double(x: int) -> int = x * 2     // 只有一个表达式

let a = area(3, height: 4)           // 先按顺序，再按名字
let squares = [1, 2, 3].map(x => x * x)
```

参数先按顺序传，再按名字传（`area(3, height: 4)`）；有默认值的参数可以省略。`x => …` 是闭包。见[函数与闭包](https://asuka.egoist.dev/zh/docs/language/functions)。

#### 结构体与枚举

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

结构体由字段和方法组成；`mut fn` 会修改调用它的那个值（一个 `var` 或者状态）。`extend` 可以给任何类型添加方法和常量，`string` 和 `Color` 也不例外。枚举的分支可以携带值，`match` 必须覆盖每一个分支（漏了哪个，编译器会指出来）。`if`、`match` 和代码块也都是值。见[声明](https://asuka.egoist.dev/zh/docs/language/declarations)和[控制流](https://asuka.egoist.dev/zh/docs/language/control-flow)。

#### 组件与状态

```asuka
component Greeting(name: string) {
  prop excited = false                // 一个 prop，带默认值
  state draft = ""                    // 组件自己的状态

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

组件的头部接收参数；它的代码块里是 prop、状态和视图。状态变化时，只有读取它的地方会更新，不做任何 diff。由会变化的内容得来的状态（`state draft = name`）在被写入之前会一直跟随它；`peek(name)` 只读取一次。`bind` 建立双向绑定，`on press` 处理事件，`if` 和 `for` 决定有哪些视图。见[组件](https://asuka.egoist.dev/zh/docs/language/components)；多个组件共享的状态见 [Store](https://asuka.egoist.dev/zh/docs/language/stores)。

#### 异步与错误

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

`async` 的工作不会卡住界面，`task { }` 属于启动它的组件：组件消失时它就被取消。可能失败的函数要写 `throws`，调用它时要用 `try`（出错时想得到 none 就用 `try?`）。见[异步](https://asuka.egoist.dev/zh/docs/language/async)和[错误](https://asuka.egoist.dev/zh/docs/language/errors)。

组件显示这种调用的结果时用 `load`：`let n = load fetchCount(online: true)` 的值是 `.pending`、`.ready(count)` 或 `.failed(e)`，再次请求时会保留它显示的内容。`n.optimistic(f)` 立刻显示一个改动，`reload fetchCount` 让它的每一个 load 再请求一次，不管它们在哪里，而 `load (a(), b())` 让几个调用的结果一起到达。见[需要加载的数据](https://asuka.egoist.dev/zh/docs/language/async#需要加载的数据)。

#### 包

```asuka
import std/time

let today = time.now().format("EEE d MMM")
```

`pub` 让声明对其他包可见。标准库（`std/…`）和平台 SDK（`platform/…`）都随命令行工具一起提供：直接 import 就能用。名为 `tray_linux.asuka` 的文件只在 Linux 上属于它所在的包，而 `@platform == .linux` 是一个常量，所以只给某个平台的代码不会出现在其他平台的构建里。见[文件、包与导入](https://asuka.egoist.dev/zh/docs/language/packages)、[标准库与平台 SDK](https://asuka.egoist.dev/zh/docs/stdlib)以及[特定平台的代码](https://asuka.egoist.dev/zh/docs/language/control-flow#特定平台的代码)。

#### 测试

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

`asuka test` 不需要设备就能运行包里的 `test` 块：`mount` 显示一个视图，测试可以点击、输入，并检查显示的内容。见[测试](https://asuka.egoist.dev/zh/docs/language/tests)。

#### 接下来

- [语言参考](https://asuka.egoist.dev/zh/docs/language)：每一部分的完整内容，从类型及其方法到样式、导航和原生代码。
- [组件](https://asuka.egoist.dev/zh/docs/components)：构成应用的各种元素。
- [核心概念](https://asuka.egoist.dev/zh/docs/concepts)：什么会更新、所有者、主线程。

### 文件、包与导入

源文件以 `.asuka` 结尾。一个目录里的源文件构成一个**包**：同一个包的文件不需要导入，就能看到彼此的声明。加上 `pub` 的声明才对其他包可见，而 `private` 让声明只属于它所在的文件（见[私有的声明](https://asuka.egoist.dev/zh/docs/language/declarations#私有的声明)）。

```asuka
import std/http                 // 标准库
import platform/camera          // 平台 SDK
import @acme/charts               // 一个依赖（asuka add @acme/charts）
import ./components               // 与当前包并列的另一个包
import ./model.{Message, Folder}  // 直接使用这些名字，不用加包名
import ./theme as t               // 起一个更短的名字
import ../model                   // 上一级目录里的包
```

包里的名字要通过包名来使用：`http.get(url)`、`components.Card("Hi")`。包名取自路径的最后一段；最后一段不能用作名字的包（比如 `@acme/line-charts`），导入时要用 `as` 给它起一个名字。预置模块（prelude，即内置的类型、元素和函数）不需要导入。`std/...` 和 `platform/...` 这些包随命令行工具一起提供，都列在[标准库与平台 SDK](https://asuka.egoist.dev/zh/docs/stdlib)里；其他包不能以 `std/` 或 `platform/` 命名。

语句和声明在行尾结束；想在一行里写多条，用 `;` 隔开。如果一行以运算符、逗号或左括号结尾，就会接着下一行继续。反过来，如果下一行以 `.name` 或 `?.name` 开头，或者以一个不能作为表达式开头的二元运算符开头（除了 `-` 都是，因为 `-x` 可以开始一个表达式），这一行也会接着下一行继续。所以链式调用可以一步一行：

```asuka
let names = users
  .filter(u => u.active)
  .map(u => u.name)
let label = user?.nickname
  ?? user?.name
  ?? "Guest"
```

以 `.name` 开头的行在两个地方是独立的。在 `match` 里，它是下一个分支（`.ready(v) => …`）。另外，如果一个代码块以最后一个表达式作为它的值（作为值使用的 `if` 或 `match`、闭包的代码块），而这一行是块里单独的最后一行，那么它就是这个值：

```asuka
let tint: Color = if urgent {
  log("urgent")
  .red  // if 的值
} else { .gray }
```

在这种代码块里，连续两行或更多以 `.name` 开头的行仍然是一条链。

注释写作 `// …` 和 `/* … */`。`///` 注释用来给它下面的声明写文档，编辑器会显示出来。

名字以字母或 `_` 开头，后面可以跟字母、数字和 `_`。这里的字母可以来自任何文字，也包括它的组合符号：`total`、`नमस्ते`、`ราคา`、`名前` 都是合法的名字（即 Unicode 标识符，UAX #31 的 XID_Start 和 XID_Continue，与 Swift、Rust、Python 相同）。看起来一样的名字就是同一个名字：比较时会先转换成 Unicode 的 NFC 形式，所以无论 `é` 是作为一个字符输入的，还是由 `e` 加上重音符号组成的，都是同一个名字。

#### 特定平台的文件

文件名以某个平台的后缀结尾的文件，只有在为该平台构建应用时才属于它所在的包。单个平台的后缀是 `_ios`、`_android`、`_macos`、`_windows`、`_linux` 和 `_web`；`_desktop`（macOS、Windows 和 Linux）和 `_mobile`（iOS 和 Android）则对应一组平台。不同平台的文件可以各自声明同样的名字，包里的其他文件像使用普通声明一样使用它们：

```asuka
// feedback_mobile.asuka：iOS 和 Android
import platform/device

pub fn confirm() { device.haptic(.success) }

// feedback_desktop.asuka：macOS、Windows 和 Linux
import platform/accessibility

pub fn confirm() { accessibility.announce("Done") }

// feedback_web.asuka
pub fn confirm() {}
```

在每个平台上，一个名字只能声明一次：要么声明在所有平台都有的文件里，要么在每个需要它的平台各自的文件里声明。如果一个名字既在所有平台共有的文件里声明，又在某个平台的文件里声明，或者在同一个平台会同时包含的两个文件里都声明了（比如 `feedback_desktop.asuka` 和 `feedback_linux.asuka`），那么无论为哪个平台构建，都会报错。文件名只有后缀的文件（`linux.asuka`）是普通文件。

所有包都按文件名用同样的规则处理：应用自己的包、依赖、标准库都一样。构建出来的总是某一个平台的版本：`asuka dev` 和 `asuka build` 为它们运行应用的那个平台构建；`asuka test` 默认为 iOS 构建，`--platform` 可以指定别的平台；程序则为运行它的那台计算机构建（或者为 `--target` 指定的目标构建）。`asuka check` 会分别检查每个平台的文件；如果某个错误并非所有平台都有，它会注明出现在哪些平台上。比如一个名字只在部分平台的文件里声明，其他平台用到它的地方就会报错。编辑器打开某个平台的文件时，会连同该平台的其他文件一起分析；打开其他文件时按 iOS 的文件分析，并显示所有平台的错误。如果要在同一个文件里按平台写不同的代码，可以用 [`@platform`](https://asuka.egoist.dev/zh/docs/language/control-flow#特定平台的代码)。

#### 应用、程序与库

一个包要么是入口，要么是库。带 `app { }` 块的包是应用：这个块就是它显示的内容（见[导航](https://asuka.egoist.dev/zh/docs/language/navigation#导航)）。文件里声明了 `fn main()` 的包是**程序**，也就是命令行工具：`main` 不接受参数（参数通过 `os.args()` 读取），不返回值（`os.exit(code)` 以某个退出码结束程序），可以是 `async` 的，也可以抛出错误；`main` 结束，程序也就结束。两者都没有的包是库，供其他包导入。一个包不能同时是应用和程序，程序也不能声明组件：它不显示视图。只有构成应用或程序的那个包才有入口；它导入的包里的 `main` 只是一个普通函数。详见[程序](https://asuka.egoist.dev/zh/docs/programs)。

```asuka
import std/os

fn main() {
  print("Hello, {os.args().first() ?? "world"}!")
}
```

### 值与类型

| 类型 | 值 |
|---|---|
| `int` | 64 位整数：`42`、`1_000`、`0xff`、`0b1010`、`0o17` |
| `i8`、`i16`、`i32`、`u8`、`u16`、`u32`、`u64` | 8、16、32 和 64 位整数，分有符号（`i`）和无符号（`u`）两种：见[整数类型](#整数类型) |
| `float` | 64 位浮点数：`3.14`、`1e-3` |
| `bool` | `true`、`false` |
| `string` | UTF-8 文本：`"Hello, {name}"` |
| `char` | 一个 Unicode 码位（标量值）：`'a'`、`'\n'`、`'\u{1F600}'` |
| `bytes` | 原始字节 |
| `[T]` | 数组：`[1, 2, 3]`、`[]` |
| `[K: V]` | map，按插入顺序保存：`["a": 1]`、`[:]` |
| `Set<T>` | 集合：一组互不相同的值，按插入顺序保存；在需要集合的地方写 `["a", "b"]` |
| `(A, B)` | 元组：`(1, "one")` |
| `T?` | 可选值：一个 `T`，或者 `none` |
| `Duration` | `300ms`、`5s`、`2min`、`1h`（还有 `us`、`ns`） |
| `Color` | `#7c3aed`、`#7c3aed80`，以及语义色：`.label`、`.accent` …… |
| `Length` | `100`（点）、`50%`、`.auto` |
| `fn(A) -> B` | 函数和闭包 |

字符串的花括号里可以放任意表达式：`"{count} items"`、`"Total: {price * qty}"`。`\{` 表示一个花括号字符；`r"…"` 是原始字符串，没有转义也没有插值；要在原始字符串里写 `"`，用 `r#"…"#`：它在一个 `"` 后面跟着与开头同样多的 `#` 时才结束（`r#"say "hi""#`、`r##"a "# b"##`）。`"""…"""` 可以跨多行（第一行为空时不算在内，结束引号所在那一行的缩进也会去掉），里面的 `"` 和 `""` 照原样保留；`r"""…"""` 是跨多行的原始字符串。不是数字、布尔值或字符串的值，显示为文本时是这样的：类型有自己的 `description()` 时用它（见[声明](https://asuka.egoist.dev/zh/docs/language/declarations#相等哈希文本和-json)），枚举成员显示为它的名字，可选值显示为 `none` 或其中的值，错误显示为它的消息，颜色按写法显示（`#0a84ff`、`.accent`），时长以毫秒显示（`1500ms`），其他值显示为 JSON。JSON 表示不了的值也按同样的方式写出：元组写成括号里的各个元素，每个元素的写法和数组元素一样（`(1,"a","red")`）；键不是字符串的 map，每个键按它自己的写法写出（`{1:"a"}`）；函数没有文本形式，显示为它的类型（`<fn(int) -> int>`）。

插值可以像 Python 和 Rust 那样，在冒号后面说明值怎么写出来：`"{price:>8.2f}"`。格式是 `[[fill]align][sign][#][0][width][,|_][.precision][type]`：`<`、`>`、`^` 表示在宽度内左对齐、右对齐、居中，前面可以写填充字符（默认是空格）；`+` 让正数也带上加号；`0` 在符号之后用 0 补足宽度；`,` 或 `_` 给整数（以及浮点数的整数部分）分组；`.2` 是浮点数小数点后的位数，对文本来说是最多保留的字符数。类型可以是 `x`、`X`、`o`、`b`，把整数写成十六进制、八进制或二进制（加上 `#` 会在前面写出 `0x`、`0o`、`0b`），也可以是 `f`、`e`、`%`，把数字写成定点小数、指数形式或百分比。数字默认右对齐，其他值按它的文本左对齐，只能指定填充字符、对齐、宽度和精度。格式由编译器读取：和值对不上的格式会在写的地方报错（[format-spec](https://asuka.egoist.dev/zh/docs/errors/format-spec)）。

```asuka
fn row(name: string, count: int, share: float) -> string = "{name:<10}{count:>6,}{share:>8.1%}"

let price = "{1234.5:,.2f}"          // 1,234.50
let hex = "{255:#06x}"               // 0x00ff
let title = "{"Total":*^11}"         // ***Total***
let table = row("asuka", 12500, 0.4) // asuka     12,500   40.0%
```

宽度按字符（码位）计算，而不是按终端里显示的宽度。要按终端的列数算（中日韩文字和 emoji 占两列），用 `std/term` 的 `textWidth`、`padEnd` 和 `fit`，见[程序](https://asuka.egoist.dev/zh/docs/programs#对齐的表格)。

`print(a, b, c)` 把各个值按插值显示的样子写成标准输出（应用的日志）上的一行，值之间用空格隔开；`eprint` 则写到标准错误。和 Swift 的 `print` 一样，值后面可以加 `separator:`（值之间写什么）和 `terminator:`（最后写什么，默认是换行）：`print(x, y, separator: ", ")`、`print("Saving…", terminator: "")`。

元组的元素可以按位置读取：`pair.0`、`pair.1`；如果元组存在 `var` 里，也可以就地修改（`pair.0 += 1`）。写一个元组没有的位置（对一对值写 `pair.2`）是编译错误。也可以把元组拆开：`let (count, label) = pair`、`for (i, x) in xs.enumerated()`，或者在 `match` 里用元组模式（见[控制流](https://asuka.egoist.dev/zh/docs/language/control-flow#控制流)）。

map 的键和集合的元素必须满足 `Hashable`：整数、字符串、布尔值、字节串、时长、颜色，以及由它们组成的枚举、结构体、元组、可选值、数组、map 和集合（包含自身的结构体或枚举，只要其余部分满足，也满足；map 要求键和值都满足）。浮点数不满足，因为 NaN 不等于它自己；函数也不满足：

```asuka
struct Cell {
  row: int
  col: int
}

let marks: [Cell: string] = [Cell(row: 0, col: 2): "x"]
let seen: Set<(int, int)> = [(0, 0), (1, 2)]
```

#### 运算符

下表按结合从紧到松排列；同一行的运算符优先级相同，从左往右结合（`**` 和 `??` 从右往左）：

| 运算符 | 作用 |
|---|---|
| `**` | 乘方：`2 ** 10` 是 1024 |
| `!x`、`-x`、`~x` | 取反、取负、把整数的每一位取反 |
| `<<`、`>>` | 把整数的位左移、右移（有符号整数右移时保留符号） |
| `*`、`/`、`%`、`&`、`&*` | 乘、除、取余、按位与、回绕乘法 |
| `+`、`-`、<code>&#124;</code>、`^`、`&+`、`&-` | 加（也用于拼接字符串和数组）、减、按位或、按位异或、回绕加法和减法 |
| `..`、`..<` | 范围：`1..3`、`0..<n` |
| `==`、`!=`、`<`、`<=`、`>`、`>=` | 比较（不能连写） |
| `&&` | 与 |
| <code>&#124;&#124;</code> | 或 |
| `??` | 可选值里的值，为 none 时取默认值 |

`<`、`<=`、`>` 和 `>=` 可以比较整数、浮点数、字符串、时长、成员不携带值的枚举（按成员声明的顺序），以及带有 `compare` 方法的类型；结构体和枚举也能从自己的方法 `add`、`sub`、`mul`、`div`、`rem`、`pow` 和 `neg` 获得算术运算符（见[声明](https://asuka.egoist.dev/zh/docs/language/declarations#结构体)）。

`**` 比上表里的其他运算符结合得都紧，`-` 也不例外，而且从右往左结合，和 Python 以及数学里一样：`-2 ** 2` 是 `-(2 ** 2)`，即 -4；`2 ** 3 ** 2` 是 `2 ** 9`；`(-2) ** 2` 是 4。整数的乘方和底数是同一类型，指数可以是任意类型的整数，但必须大于或等于 0（负指数是一个错误：`2 ** -1` 不是整数）；浮点数的乘方，指数可以是浮点数，也可以是整数（`2.0 ** 0.5`、`x ** 2`）。`**=` 是对应的赋值写法。

按位运算符只接受整数，而且两边必须是同一类型。移位数可以是任意类型的整数；移位数达到或超过该类型的位宽时，所有位都会被移出（负数右移得到 -1），移位数为负数则是一个错误。`>>` 对有符号整数保留符号，对无符号整数在高位补 0。有了这些运算符和 `b[i]`（以整数形式读出一个字节），程序就能读写二进制数据，比如蓝牙特征值或 NFC 记录（[`std/encoding`](https://asuka.egoist.dev/zh/docs/std/encoding) 能按任一字节序读写整个整数）：

```asuka
let raw = b[1] | b[2] << 8                         // 小端序的 uint16
let celsius = if raw & 0x8000 != 0 { raw - 0x10000 } else { raw }
let out = bytes([flags, value & 0xff, value >> 8])
```

`int` 的取值范围是 −2⁶³ 到 2⁶³ − 1。`+`、`-`、`*`、`**` 的结果超出这个范围时，会像除以零（`/`、`%`）一样抛出错误 “integer overflow”：`catch` 可以捕获它；没有地方捕获时，它会交给最近的 `Boundary`（见[错误](https://asuka.egoist.dev/zh/docs/language/errors#错误)）。对最小的 `int` 取负（`-x`）、取绝对值（`abs(x)`）、除以 `-1`，以及数组的 `sum()` 超出范围，也都一样。计数、大小或价格一旦溢出，就是一个在出错的地方暴露出来的 bug，而不是一个被悄悄存下来的错误数字。每种整数类型都按同样的方式检查，只是用各自的范围：`u8` 的 `255 + 1` 会抛出错误。

```asuka
let big = 9223372036854775807
let n = try big + 1 catch { 0 }               // 0：抛出了错误
let next = big.addingChecked(1) ?? big          // 溢出时为 none：得到 big
```

超出 `int` 范围的数，可以用 `std/bigint` 的 `BigInt`：它是任意大小的整数，同样支持算术运算符和比较（见[标准库](https://asuka.egoist.dev/zh/docs/stdlib)）：

```asuka
import std/bigint.{BigInt}

let big = BigInt.of(9223372036854775807)
let past = big * big + BigInt.one           // 85070591730234615847396907784232501250
let digits = "{BigInt.factorial(30)}"       // 265252859812191058636308480000000
```

`&+`、`&-` 和 `&*` 则会回绕：结果超过最大的 `int` 时，按 64 位二进制补码的规则从最小值接着算下去（`9223372036854775807 &+ 1` 等于 `-9223372036854775808`）；固定位宽的类型则在各自的位宽处回绕（`u8` 的 `255 &+ 1` 是 0）。哈希、校验和以及二进制协议需要的正是这种行为；`&+=`、`&-=` 和 `&*=` 是对应的赋值写法：

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

想把溢出当作一个值来处理时，用 `a.addingChecked(b)`、`subtractingChecked` 和 `multipliedChecked`：它们返回 `int?`（对 `u8` 返回 `u8?`，依此类推），溢出时为 none。移位和按位运算符不会因为结果而抛出错误；对超出范围的浮点数调用 `int(x)`，得到离它最近的 `int`（NaN 得到 `0`）。

#### 整数类型

除了 `int`，还有固定位宽的整数，分有符号和无符号两种：

| 类型 | 值 |
|---|---|
| `i8`、`i16`、`i32` | −2⁷ 到 2⁷ − 1、−2¹⁵ 到 2¹⁵ − 1、−2³¹ 到 2³¹ − 1 |
| `int` | −2⁶³ 到 2⁶³ − 1（没有 `i64`：它就是 `int`） |
| `u8`、`u16`、`u32`、`u64` | 0 到 2⁸ − 1（255）、2¹⁶ − 1、2³² − 1、2⁶⁴ − 1 |

计数、索引、大小以及以分为单位的金额都用 `int`；其他类型留给本身就有固定位宽的东西：一个字节（`u8`）、像素的一个通道、文件格式或协议里的字段、哈希值、超出 `int` 范围的 ID（`u64`），以及 C 函数用到的值（`Int32` 就是 `i32`，见[在 Asuka 中调用 C](https://asuka.egoist.dev/zh/docs/c)）。每种类型都严格保持自己的范围：和 `int` 一样，算术结果超出范围就会抛出错误；JSON、CSV、TOML 和 SQLite 读进这种类型的值，也会经过同样的检查。

整数字面量的类型由此处期望的类型决定，而且必须放得下：`let level: u8 = 200`、`level + 1`、`let big: u64 = 18446744073709551615`；`let x: u8 = 256` 和 `let y: u8 = -1` 会在写的地方报错。没有期望的类型时，它就是 `int`。

类型不同的两个整数不能放在同一个运算符两边：`int` 和 `u8` 不行，就像 `int` 和 `float` 不行一样。`level` 是 `u8`、`offset` 是 `int` 时，`level + offset` 是一个错误（[mixed-numbers](https://asuka.egoist.dev/zh/docs/errors/mixed-numbers)），在需要 `int` 的地方传入 `u8` 也是。要用转换写明想要哪种：

```asuka
fn conversions(n: int, level: u8, ratio: float) -> [u8] {
  let total = int(level) + n        // 扩宽：总是放得下
  let a = u8(n)                     // n 放不下时是一个错误（"integer overflow"）
  let b = u8(exactly: n) ?? 0       // 一个 u8?：放不下时为 none
  let c = u8(truncating: n)         // 取低 8 位：u8(truncating: 300) 是 44，-1 得到 255
  let d = u8(clamping: n)           // 取最接近的值：低于范围得 0，高于范围得 255
  let e = u8(ratio)                 // 浮点数：向零取整，超出范围时取最接近的值
  return [a, b, c, d, e, u8(clamping: total)]
}
```

`u8.min` 和 `u8.max` 是类型的范围（`int.max` 是 9223372036854775807）；`u64.parse(text)` 从文本读出一个值，就像 `toInt()` 读出 `int` 一样（也支持 `radix:`），文本不是整数或放不下时返回 none。超过 `int` 最大值的 `u64`，在打印、解析、比较、除法和写入 JSON 时都按它本来的数值处理（`18446744073709551615`）。

`-x` 和 `abs(x)` 只接受有符号整数：无符号整数没有负数（`0 &- x` 会回绕）。各种整数类型都有 `int` 的方法：`toString(radix:)`、`padded(width)`、`float()`、`abs()` 以及那几个 checked 方法；它们都能使用格式说明（`"{id:x}"`），按数值排序和哈希（可以作 map 和集合的键，在泛型里满足 `Comparable` 和 `Hashable`），也能构成范围：`for i in u16(0)..<count`。

从 `bytes` 读出的字节是 `int`（`b[i]`，0 到 255），所以对它做算术不会在 8 位处回绕；`b.toArray()` 把这些字节作为 `[u8]` 返回，`bytes(xs)` 能从 `[u8]` 构造字节串，和从 `[int]` 构造一样。

#### 内置方法

字符串、bytes、数组、集合、map 和可选值都有方法。其中会修改值本身的方法（`push`、`pushFirst`、`insert`、`removeAt`、`removeWhere`、`pop`、`popFirst`、`clear`、`sort`、`remove`）只能用在 `var` 或状态上：

| 类型 | 方法 |
|---|---|
| `[T]` | `count()`、`count(where)`、`isEmpty()`、`first()`、`last()`、`get(i)`（返回 `T?`）、`push(x)`、`pushFirst(x)`、`insert(x, at: i)`、`removeAt(i)`、`removeWhere(p)`、`pop()`、`popFirst()`、`clear()`、`map(f)`、`flatMap(f)`（把 `f` 返回的数组依次接起来）、`compactMap(f)`（`f` 的结果中去掉 none）、`filter(p)`、`reduce(initial, f)`、`any(p)`、`all(p)`、`index(p)`（返回 `int?`）、`find(p)`、`contains(x)`、`sum()`、`min()`、`max()`（返回 `T?`；`min(by:)`、`max(by:)` 接受一个“小于”比较，`min(key:)`、`max(key:)` 按键比较）、`unique()`、`grouped(by: f)`（返回 `[K: [T]]`）、`chunked(n)`、`zip(other)`（两两配对，长度取较短的一方）、`reversed()`、`slice(from, to)`（省略 `to` 时到末尾）、`prefix(n)`、`suffix(n)`、`sorted()`、`sorted(by)`、`sorted(key:, descending:)`、`sort()`、`sort(by)`、`sort(key:, descending:)`、`enumerated()`、`joined(separator:)`（用于字符串数组）、`lazy()`（得到 `Seq<T>`）、`+` |
| `Set<T>` | `count()`、`isEmpty()`、`contains(x)`、`insert(x)`（返回是否新加入）、`remove(x)`（返回原本是否存在）、`union(s)`、`intersection(s)`、`subtracting(s)`、`isSubset(of: s)`、`isSuperset(of: s)`、`lazy()`；`Set(xs)` 是数组中互不相同的元素，`Set<T>()` 是空集合 |
| `[K: V]` | `m[k]`（返回 `V?`）、`m[k] = v`、`count()`、`isEmpty()`、`has(k)`、`get(k)`、`remove(k)`、`keys()`、`values()`（按插入顺序）、`mapValues(f)`、`filter((k, v) => …)`、`lazy()`（它的 `(key, value)` 对） |
| `string` | `count()`（字符数）、`byteCount()`、`isEmpty()`、`upper()`、`lower()`、`trim()`、`trimStart()`、`trimEnd()`、`trimPrefix(s)`、`trimSuffix(s)`、`contains(s, ignoreCase:)`、`hasPrefix(s, ignoreCase:)`、`hasSuffix(s, ignoreCase:)`、`equals(s, ignoreCase:)`、`compare(s, ignoreCase:)`（负数、零或正数）、`foldCase()`、`replace(old, new, count:)`（默认替换全部，count 指定次数）、`split(sep, count:)`、`fields()`（按空白分出的词）、`lines()`、`find(s)`、`findLast(s)`、`findAny(chars)`（字节偏移，找不到时为 none）、`slice(from, to)`（按字符）、`byteSlice(from, to)`（按字节：即 find 给出的偏移）、`padStart(width, with:)`、`padEnd(width, with:)`、`toInt()`、`toInt(radix:)`、`toFloat()`、`repeat(n)`、`chars()`、`graphemes()`、`lazy()`（它的 char，一个 `Seq<char>`）、`reversed()`、`bytes()`、`+` |
| `bytes` | `b[i]`（返回 `int`，取值 0 到 255；越界是一个错误）、`count()`、`isEmpty()`、`get(i)`（返回 `int?`）、`slice(from, to)`、`concat(b)`、`string()`（按 UTF-8 解码，无效的部分变成 U+FFFD）、`hex()` 和 `hex(separator)`（`"04 a1"`）、`toArray()`（返回 `[u8]`）；`bytes()` 是空的字节序列，`bytes([0x04, 0xa1])` 是这些字节，`bytes(xs)` 是一个 `[u8]` 里的字节，`s.bytes()` 是字符串的 UTF-8 字节 |
| `int`、`i8` … `u64` | `float()`、`abs()`（用于有符号整数）、`toString(radix:)`（`255.toString(radix: 16)` 是 `"ff"`）、`padded(width)`（`7.padded(2)` 是 `"07"`）、`addingChecked(o)`、`subtractingChecked(o)`、`multipliedChecked(o)`（返回 `int?`，对 `u8` 返回 `u8?`：溢出时为 none）；`T.min`、`T.max`、`T.parse(text, radix:)`（返回 `T?`） |
| `float` | `fixed(digits)`、`rounded()`、`floor()`、`ceil()`、`int()`（向零取整）、`abs()` |
| `Color` | `opacity(a)`：以原本不透明度的 a 倍绘制（语义颜色仍然是语义颜色）；`hex()`（`"#0a84ff"`；语义颜色则是它的名字：`".accent"`） |
| `Duration` | `nanoseconds()`、`milliseconds()`（整数）、`seconds()`、`minutes()`、`hours()`（浮点数）、`rounded(to:)`、`truncated(to:)`、`+`、`-`、`* n`（整数或浮点数：`1.5 * 1h`）、`/ n`、`/ d`（另一个 Duration：它能容纳几个，一个浮点数） |
| `Length` | `* n`（`width: 50% * 2`：缩放一个长度） |
| `T?` | `isNone()`、`isSome()` |

会调用你传入的函数的方法（`map`、`filter`、`reduce`、`sorted(by:)`、`grouped(by:)`、map 的 `mapValues` ……）都接受会抛出错误的函数，这时它们会把函数抛出的错误原样抛出，所以调用要加 `try`。传入的函数不会抛出错误时，它们也不会。

```asuka
fn port(text: string) throws -> int = text.toInt() ?? throw Error("not a port: {text}")

fn ports(lines: [string]) throws -> [int] = try lines.map(l => try port(l))
```

下面是其中几个方法的实际用法：

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
  let pages = sales.chunked(50)                               // [[Sale]]，每组 50 个
  var out = ["{best?.region ?? "none"} sold the most"]
  for (region, total) in totals { out.push("{region}: {total}") }
  for (i, page) in pages.enumerated() { out.push("page {i + 1}: {page.count()}") }
  out.push("{codes.reduce(0, (sum, c) => sum + c)}")
  return out
}
```

`unique()` 让每个元素只保留一次，留在它第一次出现的位置；`grouped(by:)` 的键也按第一次出现的顺序排列。两者都要求值是 `Hashable` 的（`grouped` 要求的是键）。`min()` 和 `max()` 的排序方式与 `sorted()` 相同，遇到相等的元素时取第一个。`chunked(n)` 的 `n` 小于 1 时会报错，就像 `xs[i]` 越界一样。

`sorted(key:)` 按函数给每个元素算出的键排序，键必须是 `Comparable` 的，并且每个元素只算一次：`people.sorted(key: p => p.age)`。加上 `descending: true` 就从大到小排；无论哪个方向，排序都是稳定的，键相等的元素保持原来的先后顺序。`sort(key:)` 原地排序一个 `var`，`min(key:)` 和 `max(key:)` 则给出键最小或最大的那个元素（键相等时取第一个）。如果顺序不是由某一个键决定的，就用 `sorted(by:)`，传入一个“小于”比较。

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

数组同时也是双端队列：`pushFirst(x)` 和 `popFirst()` 修改数组的开头，就像 `push(x)` 和 `pop()` 修改末尾一样，每次都是 O(1)（均摊：数组在开头被修改过之后，会在开头预留空间）；`insert(x, at: 0)` 和 `removeAt(0)` 也一样。在其他位置插入或删除时，只移动较短一侧的元素。所以队列就是一个数组：`queue.push(job)`，再 `queue.popFirst()`。

`toInt(radix:)` 按 2 到 36 之间的进制读取整个字符串，字母大小写均可，前面可以带正负号（`"ff".toInt(radix: 16)` 是 255，`"-101".toInt(radix: 2)` 是 -5），读不出来时返回 none；`toString(radix:)` 则把整数按进制写出来，字母用小写。进制不在 2 到 36 之间时会报错。

`min`、`max`、`abs`、`float(n)`、`int(x)` 以及其他整数类型的转换（`u8(x)`）都是函数；其余的（`math.sqrt`、`math.pow`、`math.pi` ……）在 `std/math` 里。

`upper()` 和 `lower()` 按 Unicode 的规则转换每个字母，在任何语言下结果都一样：`"straße".upper()` 是 `"STRASSE"`，单词末尾的 `Σ` 转成小写是 `ς`。`ignoreCase: true` 比较的是两个字符串的大小写折叠（`foldCase()`：`"Straße"` 和 `"STRASSE"` 都折叠成 `"strasse"`），和 Go 的 `strings.EqualFold`、Python 的 `casefold` 一样；匹配必须从完整的字符开始、在完整的字符结束，所以 `"ß"` 并不以 `"s"` 开头。这些方法都不遵循某种语言自己的规则：土耳其语的无点 ı 和带点 İ 和其他地方一样转换（`"İ".lower()` 是 `i` 加一个组合用的点），文本也不会做规范化（`"é"` 和 `e` 加组合重音符是不同的）。不区分大小写地给名字排序，可以把 `compare` 交给 `sorted(by:)`；`foldCase()` 能生成忽略大小写的键：

```asuka
fn byName(names: [string]) -> [string] = names.sorted(by: (a, b) => a.compare(b, ignoreCase: true) < 0)

fn sameUser(a: string, b: string) -> bool = a.trim().equals(b.trim(), ignoreCase: true)

fn isMarkdown(path: string) -> bool = path.hasSuffix(".md", ignoreCase: true)
```

`count()` 和 `chars()` 按码位计数；`graphemes()` 给出读者眼中的一个个字符，依据的是 Unicode 的文本分段规则（UAX #29）：`"e\u{301}"`、一面国旗（`"🇯🇵"`）、用 ZWJ 连起来的家庭 emoji 都各算一个。

`char` 是一个码位，和 Rust 的 char 一样：`for c in s` 遍历的、`s.chars()` 按顺序给出的就是它（由几个码位组成的字素，比如 `e` 加重音符号，是字符串：用 `graphemes()`）。它写在单引号里（`'a'`、`'\''`、`'\u{301}'`），按码位比较和排序，可以做 map 的键；打印出来就是它自己，在 JSON 里是只有一个字符的字符串。`c.isLetter()`、`isDigit()`、`isNumber()`、`isWhitespace()`、`isUpper()`、`isLower()`、`isPunctuation()` 和 `isSymbol()` 读取 Unicode 的分类，适用于任何文字（`'٣'.isDigit()`）；`upper()` 和 `lower()` 在结果仍是一个 char 时做映射（`'ß'.upper()` 仍是 `'ß'`：`"ß".upper()` 才是 `"SS"`）；`code()` 是它的编号，`char(n)` 给出编号为 n 的 char（n 不是码位时是错误）。`s + c` 和 `s += c` 把一个 char 加到字符串上。

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


`toInt()` 和 `toFloat()` 读的是整个字符串，读不出来就是 none：可选的正负号，接着是数字，前后不能有别的东西（`" 12"` 是 none，先 `trim()`）。`toInt()` 只认整数（`"+7"`、`"-42"`）；`toFloat()` 还认小数和指数（`"2.5"`、`".5"`、`"1e-3"`），以及浮点数打印出来的样子（`"Infinity"`、`"NaN"`）。无论用户的语言是什么，小数点都是 `.`。

#### 惰性序列

`xs.lazy()` 把数组的元素变成一个 `Seq<T>`，也就是惰性序列。在它上面调用 `map`、`filter`、`take` 等方法，只会得到另一个 `Seq`，什么也不计算；等到有东西读取它时（`for`、`toArray()`、`first()`、`reduce` ……），才一个一个地算出元素，每个元素一次走完所有步骤，中间不产生数组。因此，对一百万个元素做一串操作、最后只留三个时，只会计算这三个以及找到它们所需的那些元素；序列也可以没有尽头：

```asuka
fn squaresUnder(limit: int) -> [int] = Seq(from: 1).map(n => n * n).takeWhile(n => n < limit).toArray()

fn firstLong(lines: [string]) -> string? = lines.lazy().map(l => l.trim()).find(where: l => l.count() > 80)

fn fibonacci() -> Seq<int> = Seq(state: (0, 1), next: s => (s.0, (s.1, s.0 + s.1)))
```

map 的 `lazy()` 给出它的 `(key, value)` 对，集合的给出元素，字符串的给出字符，区间的（`(0..<n).lazy()`）给出其中的数字，而且不会先创建数组。`Seq(…)` 则用一个函数生成序列：

| 写法 | 元素 |
|---|---|
| `Seq(from: n, by: step)` | `n`、`n + step`、……，没有尽头；不写 `by` 时步长是 1 |
| `Seq(first: x, next: f)` | `x`、`f(x)`、`f(f(x))`、……，直到 `f` 返回 none |
| `Seq(state: s, next: f)` | `f(s)` 返回下一个元素和下一个状态 `(x, s2)`，结束时返回 none |
| `Seq(repeating: x, count: n)` | `x` 重复 `n` 次；不写 `count` 时没有尽头 |

| | 方法 |
|---|---|
| 得到新的 `Seq` | `map(f)`、`filter(p)`、`compactMap(f)`、`flatMap(f)`（把 `f` 返回的各个 `Seq` 的元素依次接起来）、`enumerated()`、`zip(other)`、`take(n)`、`skip(n)`、`takeWhile(p)`、`skipWhile(p)`、`chunked(n)`、`windows(n)`（每相邻的 `n` 个：`[1, 2, 3]` 的 `windows(2)` 是 `[1, 2]`、`[2, 3]`）、`onEach(f)`（读到每个元素时调用 `f`）、`unique()`、`concat(other)` |
| 读取它 | `toArray()`、`toSet()`、`toMap()`（由 `(key, value)` 对组成）、`count()`、`count(where:)`、`first()`、`last()`、`find(where:)`、`any(where:)`、`all(where:)`、`contains(x)`、`reduce(initial, f)`、`sum()`、`min()`、`max()`、`min(by:)`、`max(by:)`、`min(key:)`、`max(key:)`、`joined(separator:)` |

`take`、`find`、`first`、`any` 和 `all` 一旦得到答案就不再往下取元素，后面的元素根本不会被计算。每次读取 `Seq` 都从头开始（用 `for` 遍历两次，就会计算两次）。它保存的是创建时数据源的值，和其他值一样：之后再修改那个数组，不会影响它。

步骤里的函数要等到读取序列时才运行，因此不能抛出错误：需要抛出时，在 `for` 里做，或者用数组的 `map`。读取时立即调用的函数（`reduce`、`find(where:)`、`any`、`count(where:)`、`min(key:)` ……）可以抛出，这时调用会把它抛出的错误原样抛出，和数组的方法一样。

优先队列，以及按键的顺序保存的 map 和集合，是 `std/collections` 的 `Heap`、`SortedMap` 和 `SortedSet`（见[标准库](https://asuka.egoist.dev/zh/docs/stdlib)）。

#### 值语义

字符串、数组、集合、map、结构体和枚举都是**值**：赋值或传参时，从你能观察到的行为来看，得到的都是一份副本。副本的开销很小：多个副本共享同一块内存，直到其中一个被修改；如果一个值只有一处持有，修改时会直接原地修改。

```asuka
var a = [1, 2, 3]
var b = a
b.push(4)        // a 仍然是 [1, 2, 3]
```

store、闭包、task 和视图是**引用**：复制出来的仍是同一个东西。

语言里没有 null。可能缺失的值用可选值表示：

```asuka
let user: User? = none
if let u = user { Text(u.name) }
let name = user?.name ?? "Guest"
let first = items.first()!        // 为 none 时会崩溃：只在确定不可能为 none 时这样写
```

`?.` 之后，链条剩下的部分遇到 none 也会停下：`user?.boss.name` 的类型是 `string?`。

可选值不会嵌套：一个值要么有，要么没有。写 `int??` 是错误（一个 `?` 就够了）；泛型的 `T?` 里，如果 `T` 本身已经是可选值，`T?` 就是这个 `T`。

### 声明

```asuka
let pi = 3.14159             // 常量
var total = 0                // 变量
state count = 0              // 状态：界面会跟随变化的值
state shown = count          // 在被写入之前跟随 count：跟随的状态
```

类型会自动推断；在有助于阅读的地方可以写出来：`let ids: [int] = []`。

`type` 在文件顶层给类型起一个名字：`type Pair<A, B> = (A, B)`、`type Check = fn(string) -> bool`。它只是同一个类型的别名：`Pair<int, string>` 就是 `(int, string)`。

#### 文件里的值

在文件顶层，`let` 是常量，`state` 是应用的状态（见 [Stores](https://asuka.egoist.dev/zh/docs/language/stores)），`var` 则是包里的变量：包里的任何代码都可以读取和修改它；如果是 `pub` 的，其他包也可以（`util.hits += 1`）。var 不是状态：读取它时拿到的是它当时的值，显示它的视图不会因为它改变而更新（那是 `state` 的用途）。它适合放视图不显示的东西：缓存、计数器、程序的设置。

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

文件里的值在其他任何代码运行之前创建：一个包的值先于导入它的包的值，每个值都在它的初始化表达式读取的那些值之后创建（直接或间接读取自己是错误）。var 和局部变量一样原地修改：`cache[n] = v` 不会复制任何东西。它和状态一样属于主线程：`spawn` 的代码块和 [worker](https://asuka.egoist.dev/zh/docs/language/async#worker) 都不能使用它，也不能调用使用它的函数（编译器会指出是哪个）；先把它读出来，再把值传进去。`asuka dev` 重新加载应用时，文件里的 var 会和 let 一样按初始化表达式重新创建；状态则保留原来的值。

#### 结构体

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

创建结构体的方式是调用它，按名字或按顺序传入各个字段；有默认值的字段可以省略。结构体的方法直接用名字访问它的字段。`fn` 只读取这个值；`mut fn` 则可以修改它：给字段（以及字段的各个部分）赋值、调用它的其他 `mut fn`，或者给 `self` 赋一个新值。

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

`mut fn` 只能在可以改变的东西上调用：`var`、状态（组件的、store 的、应用的），或者它们的字段、元素、映射的条目（`carts[0].add(i)`、`order.cart.clear()`）。不能在 `let`、参数、prop 或调用的返回值上调用，编译器会指出来。在状态上调用时，不管改了多少个字段，这次调用都只算对该状态的一次修改：只有读取 `todos[i]` 的地方会更新，而不是每一行都更新。`mut fn` 不能是 `async` 的（如果一个值在 `await` 前后只改了一半，等待期间运行的每个处理函数都会看到它；异步的修改应该写成 store 的 `fn`）。它可以抛出错误：抛出之前做的修改会保留。在 `mut fn` 里创建的闭包拿到的是 `self` 当时的值，并且不能修改它。

结构体主体里的 `let` 是这个类型的常量（`Font.title`），在任何需要 `Font` 的地方都可以简写成 `.title`。

结构体或枚举主体里的 `static fn` 是这个类型自己的函数，没有 `self`：在类型上调用，和普通函数一样可以用具名参数和默认值（`Money.of(dollars: 2)`），在任何需要这个类型的地方也可以写成 `.of(…)`。

```asuka
struct Money {
  cents: int

  static fn of(dollars: int, cents: int = 0) -> Money = Money(cents: dollars * 100 + cents)
}

let price: Money = .of(dollars: 4, cents: 50)
```

结构体可以带类型参数，创建时会自动推断；也可以像泛型函数那样，把类型参数写在名字后面（store 也一样）：

```asuka
struct Page<T> {
  items: [T]
  next: string? = none
}

let page = Page(items: [1, 2, 3])     // 一个 Page<int>
let empty = Page<string>(items: [])   // 没有别的信息可推断时，直接写出来
```

泛型结构体的 `static fn` 先接受结构体的类型参数，再接受它自己的：可以从参数推断（`Stack.of([1, 2])`），可以从期望的类型推断（`let s: Stack<string> = .empty()`），也可以和结构体的名字写在一起（`Stack<int>.empty()`），或者写在函数名后面（`Stack.empty<int>()`，结构体的类型参数在前）。泛型结构体的 `let` 是所有类型共用的一个常量，所以它的类型不能提到类型参数：需要提到的常量（比如一个空的 `Stack<T>`）就写成 `static fn`。

```asuka
struct Stack<T> {
  items: [T] = []
  let limit = 1000

  static fn of(items: [T]) -> Stack<T> = Stack(items: items)
  static fn empty() -> Stack<T> = Stack()
}

let numbers = Stack.of([1, 2, 3])       // 一个 Stack<int>
let names: Stack<string> = .empty()     // 从期望的类型推断
let flags = Stack.empty<bool>()         // 直接写出
let most = Stack.limit
```

在 `match` 和 `if let` 里，结构体模式按字段名匹配结构体的字段：单写一个 `y` 会绑定字段 `y`，`y: py` 把它绑定为 `py`，`x: 0` 匹配一个值，没有写出的字段匹配任何值。模式可以嵌套：

```asuka
match line {
  Line(from: Point(x: 0, y: 0), dashed: true) => "dashed, from the origin"
  Line(from: Point(x: 0, y), to) => "from {y} on the y axis to {to.x}"
  Line(dashed) => "a line"
}
```

结构体和枚举可以从它们的方法获得运算符：`fn add(o: T) -> R`（或 `plus`）就是 `a + b`，`fn sub(o: T) -> R`（或 `minus`）就是 `a - b`，`fn mul(o: T) -> R`（或 `times`）就是 `a * b`，`fn div(o: T) -> R`（或 `quo`）就是 `a / b`，`fn rem(o: T) -> R` 就是 `a % b`，`fn pow(o: T) -> R` 就是 `a ** b`，`fn neg() -> R` 就是 `-a`，`fn compare(o: T) -> int`（负数、零或正数）就是 `<`、`<=`、`>` 和 `>=`，也让 `sorted()` 和 `T: Comparable` 可用（见[函数](https://asuka.egoist.dev/zh/docs/language/functions#接口与泛型)）。`T` 可以和值本身的类型不同（`Money * int`）；一个运算符有两个方法名时，两个方法各接受一种 `T`：`Decimal` 的 `mul(o: Decimal)` 和 `times(n: int)` 分别对应 `price * rate` 和 `price * 3`。反过来的顺序（`3 * price`）不会自动推出。只要方法返回的还是 `a` 的类型，`a += b` 就是借助这些方法的 `a = a + b`（`-=`、`*=`、`/=`、`%=`、`**=` 也一样），可以用在 var、状态、字段和元素上（状态只改变一次）。`std/time` 的 `Time` 就是这样加上一个 `Duration`、减去另一个 `Time` 的：

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

结构体不会逐个字段地排序：它的顺序是什么意思，由它的 `compare` 来决定。

#### 相等、哈希、文本和 JSON

结构体默认按字段判断相等（`==`）、作为 map 的键计算哈希、打印和写成 JSON；枚举则按它的 case 和负载。类型可以用语言会调用的方法来改变这些行为，就像 `compare` 定义了 `<` 一样：

| 方法 | 谁会调用它 |
|---|---|
| `fn equals(other: T) -> bool` | `==` 和 `!=`、map 的键、set 的元素、`contains`、数组的 `==` |
| `fn hash() -> int` | map 的键和 set 的元素：相等的值哈希必须相同，所以有自己的 `equals` 的类型也要有自己的 `hash`（`hashOf(x)` 给出一个值的哈希，用来组合） |
| `fn description() -> string` | 插值、`print`、快照 |
| `fn toJSON() -> J` | `json.encode`，以及它所在的值的文本：`J` 是 JSON 能表示的任何类型 |
| `static fn fromJSON(json: J) throws -> T` | `json.decode`；它抛出的错误就是解码的错误 |

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

无论值放在哪里，这些方法都会被用到，包括在数组、map 和其他结构体里面。有了它们，一个按字段含义判断相等的类型（不区分大小写的名字、不计顺序的集合）就可以做 map 的键；一个持有函数（比如排序方式）的类型，也能按它持有的内容判断相等、计算哈希和写成 JSON。泛型类型可以在它所在包的扩展里提供这些方法，只对类型参数满足扩展约束（`Equatable`、`Hashable`、`Comparable`）的那些类型生效：std/collections 的 `Heap<T>` 在 `T` 是 Hashable 时可以比较相等，在 `T` 是 Comparable 时可以从 JSON 读取。这些方法不能是 `async`、`mut` 或 private 的；没有负载的枚举只能有自己的 `description`（它按 case 判断相等和写出）。方法的签名不是语言所调用的那样时，编译器会指出（[special-method](https://asuka.egoist.dev/zh/docs/errors/special-method)）。

#### 私有的声明

`private` 让一个声明只属于它所在的文件：文件里的 `fn`、`let`、`var`、struct、enum、interface 或 type，以及结构体或枚举的字段、方法、`static fn` 和常量。这个文件里的代码（它的类型的方法、它的函数、它的扩展）可以照常使用它；其他文件不能用它的名字，只有这个包的测试文件（`name_test.asuka`）例外，因为它们要测试内部的东西。没写 `pub` 的属于整个包；写了 `private` 的只属于这个文件。

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

在别的文件里创建结构体时，私有字段不能给出，所以每个私有字段都需要默认值（`Counter()`）；有私有字段没有默认值的结构体只能在它自己的文件里创建，也就是通过它的 static fn，由它们来维护结构体的规则（比如 `Decimal` 的系数没有末尾的零）。私有方法不会成为运算符（`add`、`compare`），也不会让它的类型具备某个接口，因为这些在哪里都会被用到。

#### 扩展

`extend` 可以给一个类型添加方法、`mut fn`、`static fn` 和常量。这个类型可以是内置类型（`string`、`int`、`[T]`、`[K: V]`、`Set<T>`、`Color`、`Duration`），也可以是本包或其他包里的类型。

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

扩展的方法看待这个值的方式和类型自己的方法一样：可以访问它的字段，也可以直接用名字调用它的方法（`lower()` 就是 `self.lower()`）。扩展里定义的、属于该类型的常量，在任何需要这个类型的地方都可以写成 `.name`，和类型自己的常量一样，也可以写成 `Color.brand`；`static fn` 也是如此。扩展不能添加字段，也不能添加状态；它添加的名字也不能和类型自己的成员重名：类型的布局和成员始终由它的声明决定。泛型扩展把类型参数写在 `extend` 后面（`extend<K: Hashable, V> [K: V]`）。

扩展添加的东西只在它所在的包里可见，其中 `pub` 的成员在导入这个包的包里也可见，除此之外都不可见：依赖不会改变你代码里 `x.trim()` 的含义。如果你能看到的两个扩展给同一个类型添加了同名的成员，使用它的地方就会报错，并指出这两个包。

类型也可以通过扩展获得接口，前提是这些扩展在当前位置可见：`extend Money { fn area() -> float … }` 让 `Money` 成为一个 `Shape`。写成 `extend Money: Shape { … }` 就是明确声明这一点，编译器会像检查 `struct Money: Shape` 一样检查它；其他包里没有携带值的枚举，也是通过这种方式获得接口的。一个类型在整个程序里同名的方法只能有一个：如果两个包的扩展给同一个类型添加了同一个方法，而且都让它成为某个接口的实现，就会报错。

#### 枚举

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

枚举的 `mut fn` 同样可以修改这个值，通常是给 `self` 赋值：

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

在类型已知的地方，枚举的成员写作 `.name`。`match` 必须覆盖所有成员（或者以 `_ =>` 结尾）；模式可以绑定成员携带的值（`.from(p)`，也可以按标签绑定：`.server(code: c)`），可以匹配字面量（`0`、`"x"`），可以同时匹配多个成员（`.a | .b`），可以嵌套（`.from(Person(vip: true))`），还可以带守卫条件（`.from(p) if p.vip =>`）。编译器检查 `match` 是否覆盖了所有情况时会深入嵌套的模式，并把缺少的情况写成模式告诉你：`match doesn't cover .key(_, false)`。成员带标签的值如果有零值（`false`、`0`、`""`、空数组或空映射、`none`、枚举的第一个成员），构造时可以省略：`.button(title: "Copy", run: copy)` 就省略了 `Action.button` 的 `icon`、`role`、`disabled` 和 `checked`。

成员携带的值可以是这个枚举本身，因此枚举可以表示一棵树：

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

### 函数与闭包

```asuka
fn area(width: float, height: float = 1) -> float {
  return width * height
}

fn double(x: int) -> int = x * 2      // 只有一个表达式

area(3, height: 4)
let squares = [1, 2, 3].map(x => x * x)
let sorted = people.sorted((a, b) => a.age < b.age)
```

参数先按位置传，再按名字传。闭包写作 `x => expr`、`(a, b) => expr` 或 `x => { statements }`。闭包和周围的代码共享它捕获的变量：一方修改了，另一方也能看到。

```asuka
var n = 0
let inc = () => { n += 1 }
inc()
inc()                // n 现在是 2
```

状态也以同样的方式共享。只有 `spawn` 会复制它用到的东西（只能是普通的值，取它开始运行那一刻的值），因为它在另一个线程上运行。

闭包是否异步、是否会抛出错误，由它被传作的函数类型决定（`async fn(int) -> int`）。没有类型可依据时，比如存在 `let` 里的闭包，就由它的主体决定：其中有 `await` 就是异步的，有错误可能传出就会抛出错误。在闭包前写上 `async` 可以事先说明它是异步的，不管它现在有没有 `await`；这样以后再加上 `await`，调用它的地方也不受影响。这样的闭包不能传到需要不能等待的函数的地方。

```asuka
let refresh = async () => {
  await store.reload()
}
await refresh()
```

如果某个调用因为闭包主体里的代码而缺少 `await` 或 `try`，错误信息会指出是主体里的哪一处。

那些会当场调用闭包的内置方法，比如数组的 `map`、`filter`、`reduce`、`sorted(by:)` 等，都可以接受会抛出错误的闭包：这时调用本身也会抛出闭包抛出的错误，需要加 `try`，和 Swift 的 `rethrows` 一样。闭包不会抛出错误时，调用也不会。

```asuka
let counts = try lines.map(l => try parse(l))
let sorted = try names.sorted(by: (a, b) => try rank(a) < try rank(b))
```

函数、处理函数、闭包和方法的主体里也可以声明 `fn`。它像闭包一样能看到周围的东西，可以调用自己，也可以调用同一个代码块里紧挨着它声明的其他 `fn`（它们也能调用它）：

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

它和其他函数一样是一个值（`items.map(walk)`）。它的参数按顺序传入，不能带标签或默认值，它也不能是泛型的。

永远不会返回的函数（总是抛出错误、panic 或结束程序）用 `-> never` 表明这一点，`os.exit` 就是这样；调用它的地方会像 `return` 一样结束所在的代码块，它也可以出现在任何需要值的位置：`let port = text.toInt() ?? fail("not a port")`。

函数要调用自己，就这样写。如果把闭包存进一个变量，再通过这个变量调用自己（`var walk = …; walk = n => { … walk(c) … }`），闭包就持有这个变量，而变量又持有闭包：两者都永远不会被释放，因为没有什么会回收这样的循环引用，编译器也会对此给出警告。局部 `fn` 调用自己时不会持有自己。

#### 接口与泛型

接口是一组方法和字段；一个类型只要具备这些方法和字段，就实现了这个接口（不需要额外声明）。

```asuka
interface Shape { fn area() -> float }

fn total<T>(items: [T], size: fn(T) -> float) -> float {
  var sum = 0.0
  for i in items { sum += size(i) }
  return sum
}
```

接口本身也是一种类型：接口类型的值可以是任何具备这些方法的值，调用方法时调用的就是这个值自己的方法。

```asuka
let shapes: [Shape] = [Circle(radius: 1), Square(side: 2)]
let areas = shapes.map(s => s.area())
```

接口可以要求一个 `mut fn`（比如 prelude 里的 `Iterator<T>` 要求的 `mut fn next() -> T?`）：类型的 `mut fn` 满足它，签名相同的 `fn` 也满足它。反过来，`mut fn` 不能满足接口里的普通 `fn`，因为后者可以在任何值上调用。类型也可以通过[扩展](https://asuka.egoist.dev/zh/docs/language/declarations#扩展)添加的方法获得接口，前提是这些扩展在当前位置可见。

泛型函数和泛型类型可以接收类型参数。类型参数的约束可以是程序自己的接口，也可以是 `Equatable`（能用 `==` 比较的类型：内置的值，以及各部分都能比较、或有自己的 `equals` 的结构体、枚举、元组、数组、map 和可选值）、`Comparable`（能用 `<` 排序的类型：整数、浮点数、字符串、时长、按成员顺序排序的不携带值的枚举，以及带有 `fn compare(other: T) -> int` 的结构体和枚举；结构体不会逐个字段地排序）和 `Hashable`（能做 map 键的类型：除浮点数以外的内置值，以及由它们组成、或有自己的 `hash` 的结构体、枚举、元组、可选值、数组、map 和集合，见[值与类型](https://asuka.egoist.dev/zh/docs/language/types#值与类型)和[相等、哈希、文本和 JSON](https://asuka.egoist.dev/zh/docs/language/declarations#相等哈希文本和-json)），多个约束用 `&` 连接。类型实参必须具备的东西，泛型代码里它的值也都能用：接口的方法和字段会针对每一个编译出的具体类型直接调用。

接口的方法也可以带方法体，这样的方法由接口提供。一个类型只要具备那些没有方法体的方法，就实现了这个接口；对于接口提供的方法，如果类型自己声明了签名相同的方法，就用它自己的，否则用接口的。在接口提供的方法里，`self` 就是这个接口的值，接口的方法和字段可以直接用名字访问：

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

接口提供的方法可以在接口类型的值上调用，也可以在以这个接口为约束的类型参数（如下文的 `T: Shape`）上调用；没有被当作 `Shape` 使用的 `Square` 并没有 `describe()`。如果两个接口各自提供了同名的方法，而某个类型自己没有这个方法，它就不能同时作为这两个接口使用：编译器会指出这一点，这时给这个类型声明自己的方法即可。

要判断一个接口值究竟是什么类型，可以用实现了该接口的类型的模式去匹配它：结构体的模式（`Circle(radius)`）、带类型的名字（`c: Circle`），或者写明类型的枚举成员（`Unit.big(n)`）。因为任何类型都可能实现某个接口，对接口值的 `match` 必须以 `_` 结尾：

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

模式里的类型必须实现了这个接口（没有 `area()` 的 `Point` 不可能是 `Shape`），而不携带值的枚举只有在声明了这个接口时（`enum Unit: Shape`）才算实现了它。具体类型的值不需要判断：拿 `c: Circle` 去匹配一个 `Circle` 会报错。对类型参数的值做这种判断时，结果在函数针对每个类型编译时就确定了，不可能匹配的分支不会出现在程序里。`catch e: ParseError` 也是同样的判断，只不过判断的是 `Error`（见[错误](https://asuka.egoist.dev/zh/docs/language/errors)）。

泛型函数和泛型类型可以接收类型参数。类型参数的约束可以是程序自己的接口，也可以是 `Equatable`（能用 `==` 比较的类型：内置的值，以及各部分都能比较、或有自己的 `equals` 的结构体、枚举、元组、数组、map 和可选值）、`Comparable`（能用 `<` 排序的类型：整数、浮点数、字符串、时长、按成员顺序排序的不携带值的枚举，以及带有 `fn compare(other: T) -> int` 的结构体和枚举；结构体不会逐个字段地排序）和 `Hashable`（能做 map 键的类型：除浮点数以外的内置值，以及由它们组成的结构体、枚举、元组和可选值，见[值与类型](https://asuka.egoist.dev/zh/docs/language/types#值与类型)），多个约束用 `&` 连接。类型实参必须具备的东西，泛型代码里它的值也都能用：接口的方法和字段会针对每一个编译出的具体类型直接调用。

```asuka
fn sortByArea<T: Shape>(xs: [T]) -> [T] = xs.sorted((a, b) => a.area() < b.area())

fn largestOnce<T: Shape & Equatable>(xs: [T]) -> [T] { … }

struct Labeled<T: Shape> {
  item: T
  fn label() -> string = "{item.area()}"
}
```

泛型函数的类型实参会从参数和对返回值的期望中推断出来；推断不出来时，用 `f<T>(…)` 显式写出：`json.decode<[Note]>(text)`、`flag.parse<Options>(args)`。

泛型会针对用到的每个类型各编译一份，所以它不能再以更大的类型回到自己身上：`fn depth<T>(x: T) -> int` 里调用 `depth([x])`，就需要 `depth<[int]>`，接着是 `depth<[[int]]>`，没有尽头；`struct Nested<T> { more: Nested<[T]>? }` 也是一样。编译器会指出出问题的位置。包含自身的类型要用同样的类型实参（`Tree<T>` 里的 `Tree<T>`）。

只要具备接口的方法和字段，类型实参就满足这个接口：可以是结构体或枚举（成员不携带值的枚举也可以），可以是要求了这些方法和字段的接口，也可以是带有相应约束的另一个类型参数。方法的参数必须和接口里的一样：参数更多的方法，哪怕多出来的参数都有默认值（接口要求 `greet(loud: bool)`，类型里却是 `greet(loud: bool, times: int = 1)`），也不算接口的方法，因为通过接口调用时只会传入接口自己的参数。`T: Error` 可以读取错误的 `message`。

### 控制流

`if` 和 `match` 也可以作为值使用。作为值时，`if` 必须带 `else`，代码块的值就是它的最后一个表达式：

```asuka
let label = if unread > 0 { "{unread} new" } else { "Inbox" }
let icon = match (online, unread) {
  (false, _) => "wifi.slash"
  (true, 0) => "tray"
  _ => "tray.full"
}
```

元组模式（`(true, 0)`）用各自的模式分别匹配元组的每个元素。匹配可选的元组时，它和成员模式一样，在有值时匹配其中的元组（`none` 要单独写一个分支）：`if let (name, age) = found { … }`、`let (x, y) = cursor else { return }`。

带类型的名字（`c: Circle`）用来判断接口值是什么类型：值是这个类型时才匹配，并以这个类型绑定它；结构体的模式（`Circle(radius)`）和写明类型的枚举成员（`Unit.big(n)`）也是这样。对接口值的 `match` 要以 `_` 结尾，详见[接口与泛型](https://asuka.egoist.dev/zh/docs/language/functions#接口与泛型)：`if let c: Circle = shape { … }`、`let sq: Square = shape else { return }`。

`for` 可以遍历数组、集合、map 的 `(key, value)` 对、字符串的 char（`for c in name`）、惰性序列（`Seq`，见[值与类型](https://asuka.egoist.dev/zh/docs/language/types#惰性序列)），或者一个区间：`a..<b` 是从 `a` 到 `b` 但不包括 `b` 的整数，`a..b` 则包括 `b`（`0..<3` 就是 `[0, 1, 2]`；`for` 遍历区间时不会真的创建数组）。`while cond { … }` 在 cond 成立时一直循环；`break` 跳出循环，`continue` 直接进入下一轮。

`for` 也可以遍历你自己定义的类型的值：一种是迭代器，它的类型有 `mut fn next() -> T?`（`for` 在它的一份副本上反复调用，直到得到 `none`）；另一种是序列，它的 `fn iter()` 返回一个迭代器或一个 `Seq`（`for` 只调用一次）。prelude 里的 `Iterator<T>` 就表示这样的类型，可以用作约束：`fn sum<I: Iterator<int>>(xs: I) -> int`。迭代器返回 `none` 就表示结束，所以元素本身可能是 none（`T?`）的类型应当返回 `Seq`：`fn iter() -> Seq<int?> = slots.lazy()`。

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

在 `for` 或 `while` 的头部后面写 `as name` 可以给循环起名字：在它里面的任何一层循环中，`break name` 都会跳出这个循环，`continue name` 都会直接进入它的下一轮（被跳出的每一层循环的 `defer` 都会执行）。不能在闭包里这样用；声明了却没用到的名字，以及用到了却没声明的名字，都会报错。

```asuka
for row in grid as rows {
  for cell in row {
    if cell == target { found = true; break rows }
    if cell.isEmpty() { continue rows }
  }
}
```

`if let` 和 `while let` 取出可选值里的值，或者匹配一个模式，只有取得到时才执行代码块：`if let u = user { … }`、`while let job = queue.pop() { … }`、`if let .ready(v) = state { … }`。它们的条件可以是一串用逗号分隔的子句，`let` 和布尔条件可以任意交替。每个子句都能用到前面子句绑定的名字；所有子句都成立时才执行代码块（子句按顺序求值，遇到第一个不成立的就停下）：

```asuka
if let user = session.user, user.isAdmin, let team = teams[user.teamId] {
  Text("{team.name}'s admin")
} else {
  Text("Not an admin")
}
```

`if var` 和 `if let` 一样，只是绑定的名字是一份可以修改的副本，适合修改多个字段再写回去的代码：`if var m = draft { m.read = true; m.tags.push("seen"); draft = m }`。

`let … else` 要么绑定一个值，要么离开。可选值为 none（或者模式不匹配）时执行 `else` 代码块，而这个代码块必须离开：`return`、`throw`、`break`、`continue` 或 `panic(…)`（如果它可能继续往下执行，编译器会报错）。在它之后，这个名字就是其中的值，而不再是可选值：

```asuka
fn summary(id: string) -> string {
  let message = cache[id] else { return "(deleted)" }
  let .ready(body) = message.body else { return message.subject }
  return "{message.subject}: {body}"
}
```

`throw`、`return`、`break` 和 `continue` 也可以出现在需要值的地方：它们不提供值，而是直接离开（它们的类型是 `never`，可以放在任何类型的位置上）。写在 `??` 后面，就表示遇到 none 时该怎么办：

```asuka
let user = cache[id] ?? throw MailError.missing(id)
for line in lines {
  let n = line.toInt() ?? continue
  total += n
}
```

它们同样可以用作 `if` 的分支、`match` 的分支，或者闭包的主体：`x => x ?? throw Error("empty")`。

#### 修改可能不存在的东西

从 `var` 或状态开始、经过字段、下标和 `?.` 的 `?.` 链可以被赋值（`=`、`+=` 等），也可以调用它的 `mut fn`。链上遇到 `none` 时什么也不会发生：不会写入任何东西（右边的值甚至不会被计算），状态的读取者也不会收到通知。

```asuka
inbox[id]?.read = true          // 没有这条消息时什么也不做
session.user?.name = draft
todos.first()?.toggle()          // 错误：first() 给出的是一个值，而不是一个位置
```

`m[k, default: v]` 表示映射里的条目，没有这个条目时就是 `v`。读取时它的类型是 `V`（而不是 `V?`）；写入时（`=`、`+=`、`mut fn`、`push`）它写的是 `m[k]`，而且只有条目不存在时才会计算 `v`。计数和分组都只需要一行：

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

#### 特定平台的代码

`@platform` 是应用构建时所针对的平台，类型是 `Platform`：`.iOS`、`.android`、`.macOS`、`.windows`、`.linux` 或 `.web`。它是一个常量，编译应用时就已确定：它不会被追踪，凡是能写值的地方都能用，包括文件顶层和组件主体。由它决定的分支中，某个平台不会走的那一支不会进入该平台的构建：代码、字符串和视图都不会。

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

这适用于 `if` 及其 `else if`、`match @platform`（它不必覆盖 `.unknown`，因为 `@platform` 不会是这个值）、对 `@platform` 的比较做的 `&&`、`||` 和 `!`，以及视图中的 `if`：另一个分支的视图根本不会被创建。不过无论为哪个平台构建，每个分支都会被检查：平台的 API 在每个平台上都是同样的声明，所以 Linux 分支里的错误，在为 iOS 构建时同样会报出来。如果是某个平台专有的声明，单独放在一个文件里通常更清楚，见[特定平台的文件](https://asuka.egoist.dev/zh/docs/language/packages#特定平台的文件)。

`environment().platform` 是视图运行所在的平台，由系统给出（测试里由 `mount(…, platform:)` 给出）。它是被追踪的，供视图使用。要决定为某个平台构建哪些代码，用 `@platform`。

### 组件

组件是界面中的一块，参数在头部声明，其余所有内容都写在主体里：

```asuka
component MessageRow(message: Message) {
  prop compact = false            // 一个具名 prop，带默认值
  prop accessory: View? = none    // 一个插槽：由调用方传入的视图
  event archive                   // 它对外报告的事件
  state expanded = false          // 它自己的状态

  Menu {
    longPress: true               // 上下文菜单：长按或右键点击
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

使用组件的方式和使用内置元素完全一样。头部参数放在圆括号里，其余一切都放在代码块里：

- prop：`name: value`
- 处理函数：`on name { … }`
- 双向绑定：`bind name: target`
- 样式：`style: card`，或一个列表 `style: [card, pink]`
- 子元素

```asuka
MessageRow(m) {
  compact: true
  accessory: Badge(3)
  on archive { mailbox.archive(m.id) }
}
```

每样东西都只有一个位置：头部参数不能在代码块里设置，prop 也不能放在圆括号里传。

接收子元素的组件要声明 `prop children: View`，调用方代码块里的内容就会放到这里。具名插槽在代码块里写作 `name { … }`（`toolbar { … }`）。

`emit name(args)` 触发一个事件，由调用方的 `on name(args) { }` 来处理。没有人处理的事件什么也不做；把事件名当作值读取，得到的是调用方有没有处理它（`if click { … }`：比如托盘图标，调用方不接管点击时，点击就打开它的菜单）。

会 `await` 的处理函数属于声明它的组件，也就是接收事件的那个组件。它会一直运行，直到执行完毕或那个组件被移除为止；不管事件是谁触发的，也不管触发方后来怎样。比如弹出页的 `on done { shown = false; try await save(draft) }` 会先关掉弹出页，再继续保存。它在声明它的位置运行：`push` 推入的是那里的导航栈，它抛出而没有处理的错误交给那里外层的 `Boundary`。在 async 函数里，`emit` 会等处理函数执行完，并拿到它抛出的错误；在其他地方（不 `await` 的处理函数、普通的 `fn`），`emit` 只是启动它，然后继续往下执行。取消正在等待的代码，只会停止等待，不会停止处理函数。

只显示一个元素的组件（主体里的视图就是那一个元素，或另一个同样如此的组件）也接受样式属性：调用方给的样式落在那个元素上，并覆盖组件自己在那里设置的值。

```asuka
component Card(title: string) {
  Column {
    padding: 16; radius: 12; background: .secondaryBackground
    Text(title)
  }
}

Card("Plan") { padding: 8; width: 240 }   // padding 为 8，radius 为 12
```

#### 组件声明的变体

只显示一个元素的组件，可以给自己的状态起名字，交给调用方去设置样式：`variant name = condition`，一个像 `let` 一样随时保持最新的 bool。调用方在代码块里写 `name { … }`，条件成立时，里面的样式属性就叠加到那个元素上，就像 `View` 被按下时叠加 `pressed { … }` 一样：

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

- 变体的名字自成一个命名空间：可以和某个 prop 或 state 同名，比如这里的 `checked`。但不能用交互状态的名字（`pressed`），也不能用关键字。
- 变体叠加在元素上所有其他样式之上：元素自己的样式、调用方的样式，以及交互状态。两个变体同时成立时，后声明的胜出（这里 `disabled` 压过 `checked`）。
- `pub variant` 可以由其他包的调用方设置样式；没有 `pub` 的变体只属于声明它的包。
- 样式里也可以写变体的代码块（`style box { checked { … } }`）：应用到一个声明了这个变体的组件所显示的元素上时，代码块随变体生效；用在其他地方则什么都不做。
- 当组件显示的元素是容器时，调用方在组件的代码块里也可以写交互状态（`pressed { … }`、`hovered { … }`）。
- `Checkbox as box` 给它显示的那个元素的视图起名字：`box.checked` 会读取这个变体，并被追踪，就像 `card.hovered` 读取交互状态一样。

#### 组件的主体是声明

组件的主体**不是**运行一次就被抛到脑后的代码。主体里的每一行都声明了一件始终成立的事：

- `state x = …` 是组件自己拥有的状态。如果它的值读取了会变化的内容，它就会跟随那些内容（见[跟随的状态](#跟随的状态)）。
- `let y = …` 由它读取的内容派生而来，并会一直保持最新（只在被读取时才计算，而且只有当它读过的内容变化后才会再次计算）。
- 每个元素的每个 prop 值都以同样的方式保持最新：`count` 变化时，`Text("Count: {count}")` 跟着变化，其他东西都不会变。
- `if`、`for` 和 `match` 会按照条件显示对应的内容，现在如此，之后也一样。

`count` 变化时，重新运行的只有读取了它的部分：那个 `Text` 的文字，以及那个 `if`。没有任何组件会重新运行，平台的视图也只在确实有变化的地方才会被改动。

#### 跟随的状态

状态的值说明它从什么开始。如果这个值读取了会变化的内容（参数、prop、其他状态、`let`），状态就会跟随它：在被写入之前，状态就是这个值；它读过的内容变化后，状态又会回到这个值。

```asuka
component Edit(title: string) {
  state draft = title          // 用户输入之前是 title；title 变化后又回到 title
  state first = peek(title)    // 只读一次 title：一个初始值

  TextField { bind text: draft }
}
```

`state x(a, b) = e` 只跟随 `a` 和 `b`：其中任何一个变化时重新计算 `e`，不管 `e` 还读了别的什么。

```asuka
let results = items.filter(i => i.name.contains(query))
state highlighted(results) = 0                  // 结果变化时回到第一项
```

在状态自己的值里，状态的名字表示它当前的值，类型是可选值（第一次为 `none`）。这样可以在值仍然成立时保留它，不成立时再替换掉；这种状态需要写明类型：

```asuka
// 选中项还在结果里时保持不变，不在时移到第一个结果。
state selected: string? = if let s = selected, results.any(r => r.id == s) { s } else { results.first()?.id }
```

如果同一个处理函数里既写入了状态，又改变了它跟随的内容，以后发生的那一个为准。它跟随的内容刚变化时就去读它，读到的已经是新值。

`peek(e)` 就是 `e`，只是读取时不跟随它；凡是会被跟随的值里都能用：`Text("{count} (from {peek(count)})")` 会在 `count` 变化时更新，而第二次读取保持原样。处理函数、task、`effect(…)` 的主体和测试本来就不跟随任何东西，所以在这些地方写 `peek` 是错误。

#### 处理函数与语句

代码在这些地方运行：处理函数（`on press { … }`）、函数、`task` 和 `effect` 代码块，以及测试。在这些地方读取状态，得到的是它的当前值；写入状态，读取它的地方就会更新：

```asuka
on press {
  count += 1
  todos.push(Todo(title: draft))
  todos[0].done = true      // 写入 todos[0].done：只有读取它（或 todos[0]、整个 todos）的地方会更新
  draft = ""
}
```

一个处理函数里的多次写入，会在它结束时（或者执行到 `await` 时）一起显示出来。

语句有：`let`、`var`、`let … else`、赋值（`=`、`+= -= *= /= %=`、`&= |= ^= <<= >>=`、`&+= &-= &*=`、`??=`）、`if` 和 `if let`、`for x in xs`（数组或集合）、`for (k, v) in m`、`for (i, x) in xs.enumerated()`、`while` 和 `while let`、`match`、`return`、`break`、`continue`、`throw`、`do { } catch { }`、`defer { }`、`emit`（见[控制流](https://asuka.egoist.dev/zh/docs/language/control-flow#控制流)），以及 `fn` 声明（见[函数与闭包](https://asuka.egoist.dev/zh/docs/language/functions#函数与闭包)）。

#### 双向绑定

控件既要显示一个值，也要修改它。`bind` 把状态交给控件，两件事就都能做：

```asuka
state query = ""
state notify = true

TextField { bind text: query; placeholder: "Search" }
Toggle { bind on: notify; title: "Notifications" }
```

不用 `bind` 时，控件只负责报告变化：`TextField { text: name; on change(t) { name = t.trim() } }`。

组件的 prop 也可以这样绑定，只要组件用 `event <prop>Change` 报告它：一个有 `prop value` 和 `event valueChange(value: T)` 的组件，写 `bind value: when` 就会把状态传进去，并在组件发出变化时写回：

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

组件也可以给这样的 prop 赋值，就像它是自己的 state 一样：在处理函数、fn、effect 和 task 里赋值都行（通过元素的 `bind` 写入也算）。赋值的效果取决于调用方：

- 调用方绑定了它（`bind open: shown`）：赋值会发出 `openChange`，由它写入调用方的状态，prop 继续读这个状态。如果调用方不写入，prop 就保持原样。
- 调用方没有绑定（传了一个值，或者什么都没传）：赋的值从此归组件自己所有；如果调用方处理了 `openChange`，仍然会收到通知。调用方传入的值一旦变化，就会替换掉组件自己的值。

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

Disclosure("Details") { Text("…") }                  // 组件自己的
Disclosure("Details") { bind open: shown; Text("…") } // 调用方的
```

prop 只能整体改变：写 `open = …` 或 `items = items + [x]`，不能写 `items.push(x)`。没有对应 change 事件的 prop 不能赋值。

#### 视图列表

```asuka
for todo in todos key todo.id {
  TodoRow(todo) { on delete { todos.removeWhere(t => t.id == todo.id) } }
}
```

`for` 为每个键保留一行。某一项变化时，对应的行只更新它自己的绑定。新增一项会创建一行，删除一项会销毁它的那一行。移动了位置的行只会被移动，不会重建。直接放在 `List` 下面时，行只在显示在屏幕上时才存在，所以一个有 100,000 项的列表，开销也只是屏幕上可见的那些。

遍历会变化的内容时，`for` 要用 `key` 说明靠什么区分每一项：否则项目移动时，行的状态会留在原来的位置上。有些情况不需要 `key`：

```asuka
for i in 0..<count { Dot(i) }                      // 范围：数字就是键
for tag in tags { Chip(tag) }                      // 集合：元素就是键
for day in ["Mon", "Tue", "Wed"] { Text(day) }     // 常量：没有东西会移动
for (i, x) in xs.enumerated() key i { Slot(i, x) } // 有意按位置保留行
```

#### Effect 与 task

```asuka
effect { storage.set("draft", draft) }        // 立即运行，之后 draft 每次变化时再运行
effect(selected) { log(selected, user.plan) } // 立即运行，之后只在 selected 变化时再运行
task { try? await mailbox.refresh() }         // 组件显示期间运行一次
task(query) { results = await search(query) } // query 变化时再运行一次
task(user.id) { profile = try await load(user.id) } // id 变化时才重新运行，name 变化时不会
```

effect 会立即运行，之后每当它读取过的内容（包括它调用的函数读取的内容）变化时再次运行。它读取的就是代码里写到的那部分：`effect { log(user.name) }` 在名字变化时重新运行，头像变化时不会。`effect(a, b)` 只在 `a` 或 `b` 变化时再次运行，它的主体读取的东西都不会被跟随；`effect() { }` 什么都不跟随，只在组件建立时运行一次。task 是异步的（见[异步](https://asuka.egoist.dev/zh/docs/language/async#异步)）：组件出现时启动；圆括号里的内容变化时重新启动（并取消上一次运行）；组件消失时被取消。effect 和 task 圆括号里的内容会被比较：变化后如果值仍然相等，就什么都不会发生，所以用户改名时 `task(user.id)` 会继续运行。`app` 代码块里也可以写它们：它们属于应用本身，在应用运行期间一直有效（比如一个刷新循环，或者一个跟随应用状态的设置：`effect { appearance.setColorScheme(scheme) }`）。

effect 可以写入状态，但不能写入它跟随的状态：`effect { if n > 3 { n = 3 } }` 会在自己写入之后再运行一次，晚了一轮，而在那之前处理函数已经看到 `n` 是 4 了。编译器会报错，并给出替代的写法：跟随的状态（`state n: int = min(n ?? 0, 3)`、`state selected(results) = 0`），跟随别的内容的 `effect(…)`，或者在改变它读取内容的那个处理函数里直接写入。task 可以写入它跟随的状态：它通过取消来重新启动，最终会结束。

##### cleanup

```asuka
effect() {
  let id = bus.listen("refresh", e => reload())
  cleanup { bus.unlisten(id) }          // 只运行一次：组件消失时
}
effect(roomId) {
  let room = roomId
  chat.join(room)
  cleanup { chat.leave(room) }          // 下一次运行之前，以及 effect 消失时
}
cleanup { drafts.save(draft) }          // 组件消失时
```

effect 主体里的 `cleanup { … }` 会在 effect 下一次运行之前、以及 effect 消失时运行：这次运行建立了什么，它就撤销什么。组件显示期间只需建立一次的东西，写在 `effect() { }` 里：它调用的函数可以读写它所注册的地方（比如一个监听者列表），而 effect 不会去跟随这些东西。它可以写在 `if` 里（只撤销这个分支做过的事），也可以写在循环里（执行到几次就注册几次）；有多个时，后注册的先运行。写在组件、store 或 `app` 的主体里、和 effect 与 task 并列时，它会在这个实例消失时运行，排在 effect 的 cleanup 之后。

cleanup 是一个闭包：它能看到这次运行的局部变量；它不能 await，也不能抛出错误（它在 owner 消失的过程中运行，后面的 cleanup 还得接着运行；用 `try?` 或 `do { } catch { }` 处理错误）；它读取的东西不会被跟随。它读到的状态是它运行那一刻的值。在 effect 再次运行之前，那已经是 effect 跟随的东西变化之后了，所以 `cleanup { chat.leave(roomId) }` 离开的会是新房间：要把这次运行的值存进 `let`，否则编译器会给出警告（[cleanup-reads-what-it-follows](https://asuka.egoist.dev/zh/docs/errors/cleanup-reads-what-it-follows)）。它写入的东西算作它所在 effect 的写入。

处理函数、函数、task 和测试里不能写 cleanup：task 用 `defer` 撤销它启动的东西，`defer` 会在 task 结束或被取消时运行；处理函数和函数没有自己的 owner 来运行它。

### Store

store 是状态加上修改它的函数，由使用它的视图共享：

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

store 的创建方式和结构体一样（`Mailbox()`），它属于创建它的那个组件或 store。`provide` 让它对下面的所有内容可用，`use` 用来取到它：

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

组件提供的东西只属于它显示的视图：两个并排的组件各自提供一个 `Theme`，它们看到的都是自己的那个，而不是对方的。

用可选类型 `use` 时，如果上面没有谁提供，取到的就是 none。这适合既能单独使用、也能放在一组里使用的组件，比如复选框，可以放在复选框组里，也可以不放。

```asuka
store Group {
  pub state chosen: [string] = []
}

component Choice(value: string) {
  use group: Group?

  Text(if let g = group, g.chosen.contains(value) { "✓ {value}" } else { value })
}
```

写在文件顶层的 `state` 属于整个应用：全应用只有一份。

store 和结构体一样可以带类型参数，由参数或期望的类型推断出来：

```asuka
store Kept<T>(key: string, initial: T) {
  pub state value: T = initial
}

let size = Kept("size", 17.0)                 // 一个 Kept<float>
let tags: Kept<[string]> = Kept("tags", [])
```

在 store 的 fn 里，`self` 就是这个 store 本身，可以把它交给它创建的东西。

### 异步

需要等待的工作（网络、文件、定时器）用 `async` 表示，等待它则用 `await`：

```asuka
async fn loadThread(id: string) throws -> Thread {
  let (messages, people) = try await all(api.messages(id), api.people(id))
  let html = await spawn { renderMarkdown(messages) }
  return Thread(messages, people, html)
}
```

- 调用异步函数需要 `await`。处理函数、task 和测试里可以使用 `await`；组件的主体及其中的 `let` 不行。闭包被传作异步函数，或者主体里有 `await` 时，就是异步的；写成 `async () => …` 可以明确标出（[函数与闭包](https://asuka.egoist.dev/zh/docs/language/functions)）。
- **主线程。** 所有代码都在主线程上运行；`await` 让你的代码在等待时，其他工作可以先运行。状态可以在主线程上修改，而且只能在主线程上修改。
- `all(a, b)` 同时发起多个调用，并等待全部结果；`race(a, b)` 取第一个完成的结果，并取消其余的；此外还有 `sleep(300ms)` 和 `withTimeout(5s) { … }`，后者的值就是代码块的值（`let page = try await withTimeout(5s) { return try await fetch(url) }`），超时就抛出错误。它们是结构化的：被它们取消的工作会先停下、执行完自己的 `defer`，它们才返回或抛出错误。`await never()` 什么都不等，只等任务结束：适合只在读者离开时才结束的流，或者先要 await 才能持有某样东西的 task（`try await register(); defer { release() }; await never()`）。组件不需要 await 就能持有的东西，用 `cleanup { }` 撤销（见 [cleanup](https://asuka.egoist.dev/zh/docs/language/components#cleanup)）。
- `all(items, f, limit:)` 用 `items` 里的每一项同时调用异步函数 `f`，并按各项的顺序给出结果：`let pages = try await all(urls, u => try await http.get(u))`。写上 `limit: 8` 时，同一时刻最多运行 8 个，一个结束了才开始下一个；省略（或为 0）时全部同时开始。它和 `all(a, b)` 一样是结构化的：第一个抛出错误的调用会取消其余的、不再开始新的，等它们都停下后再抛出这个错误；等待它的任务结束时，它们也全部被取消。每一项都作为一个独立的任务在主线程上运行，所以可以等待网络或文件，也可以修改状态；要放到其他线程上的工作交给 `spawn`，`f` 可以 await 它。

  ```asuka
  async fn fetchAll(urls: [string]) throws -> [string] {
    let pages = try await all(urls, u => try await http.get(u), limit: 4)
    return pages.map(p => p.text())
  }
  ```
- `spawn { … }` 在另一个线程上运行一段代码，用于会拖住主线程的工作（解析、图片处理）。这段代码只能使用值，不能使用状态、文件里的 var、store、视图、原生对象或 C 指针，也不能使用保存着它们的文件级 `let`（比如一个数据库）：如果用了，编译器会报错。在 Web 上，它运行在页面的一个 Web Worker 里；Worker 只会自己生成文件里的值，不会生成其他东西。如果工作需要在多次调用之间保留某样东西（一个索引、一个它打开的数据库），就用 [worker](#worker)。
- `for await message in socket.messages { … }` 读取一个流，也就是 `Stream<T>`：随时间陆续到来的值。`Stream(send => { … })` 创建一个流：流第一次被读取时，这个代码块作为 task 开始运行，每次 `send(x)` 就是一个值；代码块返回时流结束，代码块抛出错误时流也以这个错误失败。离开循环（`break`、出错、owner 被销毁）会取消这个代码块。一个流只能有一个读取者。

  ```asuka
  fn levels() -> Stream<float> = Stream(send => {
    try await battery.watch(changed: level => send(level))
  })
  ```
- `live(initial, source)` 是一个由流保持最新的值，以读取它的函数的形式出现：在绑定中读取时，从第一次这样的读取开始跟随 `source()` 的流，直到最后一个读取者离开（随后流被取消：不会在没人需要时还监听平台）；在别处读取时，得到的是最后已知的值。包就是这样提供平台会变化的东西的，比如网络状态：

  ```asuka
  let level = live(1.0, levels)
  pub fn batteryLevel() -> float = level()
  ```
- **取消。** task 属于启动它的那个东西（组件事件的处理函数则属于声明它的组件，见[组件](https://asuka.egoist.dev/zh/docs/language/components)）。当那个东西消失时（比如用户离开了页面），task 就会被取消：它正在等待的那个 `await` 会结束它，它的 `defer` 会运行，并且不会报告任何错误。

#### Worker

`spawn` 在另一个线程上把一段代码运行一次。`worker` 则让一个值一直留在那里：它是一种类型，每个实例都运行在自己的线程上，带着只属于自己的 `var`。

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

- worker 的 `var`、`let` 和参数（`worker Index(language: string)`）都是它自己的：只有它的 fn 会在它的线程上读取和修改它们。worker 没有 `state`（视图不会读它），也没有 props、事件和视图。
- 从外部调用时，它的 `pub fn` 是异步的：`await index.find(q)`。参数以值的形式传过去，调用按发出的顺序一个接一个地运行，结果（或者 `throws` 函数抛出的错误）也以值的形式传回来。在 worker 内部，它们就是普通函数，不会 `await`；其他 fn 只供 worker 自己使用。
- 能传过去的东西和 `spawn` 一样：只能是值，不能是状态、store、视图、闭包或原生对象；worker 的 fn 也不能读取主线程的状态。worker 可以自己创建并保存 C 句柄（一个 [extern type](https://asuka.egoist.dev/zh/docs/c#c-持有的类型)，比如它打开的数据库）：这个句柄永远不会离开 worker，也在那里释放。
- worker 属于创建它的组件或 store。组件或 store 消失时，worker 就会停止：正在运行的调用会执行完，还在排队的调用会被取消；调用方的 task 看到的是自己被取消，而不是一个错误。没有 owner 的 worker 会在不再被引用、并且交给它的调用都执行完之后停止。
- 在 Web 上，每个 worker 都是一个独立的 Web Worker。

#### 需要加载的数据

`load` 把一个异步结果保存在组件里：

```asuka
screen Profile(id: string) {
  let user = load api.user(id)   // id 变化时重新运行

  match user {
    .pending => Spinner()
    .failed(e) => ErrorView(e) { on retry { user.reload() } }
    .ready(u) => ProfileCard(u) { opacity: if user.loading { 0.6 } else { 1 } }
  }
}
```

`Loading<T>` 的值是 `.pending`、`.ready(value)` 或 `.failed(error)` 之一，而且只有在第一次得到结果之前才是 `.pending`。当它的参数所读取的内容变化，或者 `.reload()` 让它再运行一次时，它会保持当前所处的情况，直到新的结果到来：`ProfileCard` 留在屏幕上，它的状态和滚动位置也都还在；那次即将过时的运行则会被取消。如果新结果和上一次相等，什么都不会变。

- `.value` 是它所显示的内容，第一次得到结果之前为 none。某次运行失败时它也会保留，所以 `.failed` 可以在错误旁边继续显示旧内容。
- `.loading` 在有一次运行正在进行时为 true，第一次也算在内：可以用它把内容调暗，或者在角落里显示一个转圈。读取它不会跟随值的变化，`match` 也不会跟随它。

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

用户输入时，列表会一直保留上一次的结果、滚动位置和各行的状态，直到新结果到来；转圈则表示有一次搜索正在进行。其他读取 `query` 的地方会立刻显示新的文字。

如果想从头来过（比如切换到另一个人时，不应该在加载期间还显示上一个人的资料），就在 `match` 的位置读取 `.loading`：

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

不过更常见的情况是，内容本来就各自独立：每个 id 推入一个页面，每个 key 一行，各自有自己的 `load`。

##### 立刻显示的改动

勾选、重命名、点赞：画面应该在服务器确认之前就显示改动，保存失败时再收回。`x.optimistic(f)` 会立刻显示经 `f` 改动后的 `x` 的内容：

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
            try await saveDone(id, done)   // 抛出错误会撤销它
            await reload fetchTodos()      // 它的结果会替换它
          }
        }
      }
    }
  }
}
```

- 这个改动是叠在 load 结果之上的一层。`match`、`.value` 和各行看到的都是它；只有读取了它改动过的路径的东西会重新运行，别的都不会：上面的勾选只会让它自己那一行重新运行。
- 它会在以下情况中最先发生的那一刻撤下：load 的下一个结果到来（来自创建它时正在进行的运行，或之后开始的运行：`reload()`、参数所读内容的变化），并在同一帧里显示，所以旧值不会闪回；创建它的处理函数、`fn` 或 `task` 抛出错误，这时它先被撤销，错误再继续传递；那段代码结束时既没有正在进行的运行，也没有请求运行。最后这种情况是个错误，开发构建会指出来（optimistic-unconfirmed）：在结束之前再请求一次这个 load。
- 多个改动按创建的顺序叠起来，各自独立地撤下：两个勾选同时进行、第一个失败时，只有第一个被撤销。
- `f` 是所显示内容的函数：它下面的内容变化时它会重新运行，所以它不读取也不写入状态（[optimistic-impure](https://asuka.egoist.dev/zh/docs/errors/optimistic-impure)）。先把它需要的东西读进一个 `let`，就像上面的 `id`。
- 对还没有 `.ready` 的 load，没有可以改动的内容，它什么也不做。

##### 重新加载改动影响到的数据

在编辑页面上重命名一条笔记：它下面的列表页面有自己的 load，`note.reload()` 管不到它。`reload` 用读取数据的那个调用来指明改动影响到了什么，于是这个调用的每一个 load 都会再请求一次，不管它在哪里：任何组件、任何栈里的任何页面、任何窗口。

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
      await reload api.list       // 这个页面下面的列表
      await reload api.note(id)   // 这条笔记的 load，不包括其他笔记的
      pop()
    }
  }
}

app {
  provide Api()
  Stack { Notes() }
}
```

- `reload f(args)` 会触及参数等于 `args` 的那些 `f` 的 load（用 `==` 比较，和依赖的比较方式一样）；`reload f` 则触及 `f` 的每一个 load，不管参数是什么。store 的方法会触及每一个实例的 load。
- 每一个都像 `x.reload()` 那样运行：保持它显示的内容，在得到结果之前 `.loading` 为 true。
- `await reload …` 会一直等到它触及的每一个 load 都得到结果或者失败（期间消失的 load 也算）：失败由各个 load 自己显示，只有在等待被取消时它才会抛出错误。不写 `await` 时，这些 load 照样运行，代码继续往下走。
- 调用就是键：一个函数或 store 方法的 load。闭包调用的 load，或者某个值的方法的 load，用它自己的 `x.reload()`。

##### 一起到达的 load

个人资料和它的帖子并排加载时，`id` 一变，它们的结果会一个接一个地换上：有那么一刻，新的人显示在上一个人的帖子上面。`load (a(…), b(…))` 是由几个调用组成的一个 load，它们的结果会一起显示：

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
  let page = load (fetchUser(id), fetchPosts(id, page: n))   // 一起显示

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

- 它是一个 `Loading<(A, B, …)>`，和别的 load 一样匹配：`.ready((u, posts))`。
- 每个调用保留各自的依赖：上面的 `n` 只会让 `fetchPosts` 重新运行，用户资料保持不变。`id` 变化时两个都会运行，新的用户资料会一直留着，等新的帖子到来。
- 在每个调用都得到过一次结果之前是 `.pending`；有一个失败时是 `.failed(e)`，取源码顺序里的第一个，`.value` 保留上一个完整的元组；任何一个调用在运行时 `.loading` 都为 true。
- `page.reload()` 会让每个调用都运行；`reload fetchPosts(id, page: n)` 只运行那一个；`page.optimistic(p => (p.0, p.1 + [draft]))` 改动的是整个元组。
- 每个元素都是一个调用：一个值没有什么可等待的（[group-not-calls](https://asuka.egoist.dev/zh/docs/errors/group-not-calls)）。

### 错误

可能失败的函数要标注 `throws`，调用它时需要 `try`：

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

let n = try? parse(text)   // 抛出错误时为 none
```

在表达式后面写 `catch`，可以给它一个备用值：正常时得到表达式的值；表达式里有地方抛出错误时，得到 `catch` 代码块的值（和 `if` 一样，是它的最后一个表达式），除非这个代码块直接离开：

```asuka
let port = try parsePort(text) catch e {
  log(e.message)
  80
}
let size = try await fileSize(path) catch { return }
try save(draft) catch { failed = true }   // 作为语句：不需要提供值
```

它的子句和 `do` 的一样：`catch`、`catch e`、`catch e: ParseError`、`catch MailError.server(code)`，可以连写多个，由第一个接住错误的子句处理。没有子句接住的错误会继续往外抛，所以所在的函数必须标注 `throws`，这和 `try` 的规则相同。如果子句能接住所有错误，整个表达式就不会抛出错误，因此可以用在 `try` 不能出现的地方：组件的 `let` 里（`let port = try parsePort(text) catch { 80 }`，会随着 `text` 的变化保持更新），或者不允许抛出错误的闭包里。它也可以配合 `try await` 使用，以及配合作为值的 `throw`：`let user = cache[id] ?? throw MailError.missing(id)`。

错误的 `message` 是它的 `message` 字段、它的 `fn message() -> string`，或者它的类型名和成员名。`Error("…")` 用一条消息创建一个错误。`catch` 永远不会捕获 task 的取消：取消会一直走到 task 的结尾，并在途中执行各个 `defer`。

`Error` 也可以像 `catch` 那样按错误类型的成员来匹配：`match e { MailError.offline => …; MailError.status(code) => …; _ => e.message }`（`Error` 可能是任何错误，所以匹配要以 `_` 结尾）。

处理函数或 task 没有捕获的错误，会交给包住这个组件的最近的 `Boundary`；如果没有，就交给应用自己的处理（开发时显示出来，发布版本中记录到日志）：

```asuka
Boundary {
  Feed()
  on error(e) { Text("Something went wrong: {e.message}") }
}
```

### 样式

每个元素都接收样式 prop：先是布局（`direction`、`justify`、`align`、`gap`、`padding`、`margin`、`width`、`height`、`grow`、`shrink`、`absolute`、`top`、`grid` ……），然后是外观（`background`、`radius`、`border`、`opacity`、`shadow`、`font`、`color`、`lines` ……）。详见[布局与样式](https://asuka.egoist.dev/zh/docs/layout)。

`style` 声明一组可以复用的 prop，用 `style:` 应用一个样式，或者一个样式列表：

```asuka
style card {
  padding: 16; radius: 12; gap: 8
  background: .secondaryBackground
}

style pink { background: #ff2d55 }

View { style: [card, pink]; opacity: 0.9 }
```

两个条目设置了同一个 prop 时，后面的优先；块里自己写的 prop 又优先于整个列表，不管 `style:` 写在块的什么位置。样式也用同样的写法应用别的样式，它自己的 prop 优先于被应用的样式：

```asuka
style largeCard { style: card; padding: 24 }
```

样式可以像 `fn` 一样接收参数（可以有默认值，也可以按名字传）。参数在用到它的地方是一个绑定：`chip(tint)` 会跟随 `tint` 变化，而且只有读到它的 prop 会更新。

```asuka
style chip(tint: Color, size: float = 13) {
  padding: [4, 10]; radius: 999
  background: tint.opacity(0.15); color: tint; font: Font(size: size)
}

Text(tag) { style: chip(.green) }
Text(count) { style: [card, chip(tint, size: 15)] }
```

列表的条目也可以是 `if` 或 `match`：它选中的分支里的样式生效；换到另一个分支时，那些样式设置过的每个 prop 都会退回到列表中排在它前面的条目所给的值（没有的话就是平台默认值）。在这里，`if` 不需要 `else`；`match` 不必覆盖所有情况，也不能绑定名字，分支写 `[]` 表示什么都不应用。

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

样式里可以写样式 prop、`style:`、变体（见下文）和 `use`。`pub style` 也能在别的包里使用：写作 `style: theme.card`，其中的值在声明它的包里求值。样式不能应用它自己，经过别的样式间接应用也不行。

不设置任何样式的元素，看起来就和平台自己的一样。

**主题。** 样式的值可以读取所在包的 `let` 和 `state`，所以一个设计令牌（token）就是其中之一（`pub let hairline = 2.0`）；如果是 `state`，改了它，所有读到它的地方都会跟着变。样式还可以 `use` 上层组件提供的 store，在应用样式的地方查找：应用里提供了自己那份的部分，就有自己的一套令牌。

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
  provide Theme(hairline: 1)   // 这里面细一些
  Column { style: framed }
}
```

**默认样式。** 以元素（或接收样式 prop 的组件）命名的样式，就是它的默认样式：本包里写的每个该类元素都会应用它，位于 `style:` 列表之下。别的包里的元素，以及别的包的组件内部的元素，保持它们自己的样子。

```asuka
style Button { radius: 8 }
style View { borderColor: .separator }
```

容器（`View`、`Row`、`Column`、`SafeArea`）可以写**变体**：以交互状态命名的一组样式 prop，在容器处于该状态时叠加到它的样式上，相当于 CSS 的 `:hover` 和 `:active`。状态有四种：`hovered`、`pressed`、`dragging`（正被拖动时）和 `dragOver`（它接受的拖动正停在它上方时），按这个顺序叠加，所以 `pressed` 会盖过 `hovered`。变体只覆盖它写出的 prop，它的值和普通 prop 一样是绑定：

```asuka
View {
  padding: 16; radius: 12; background: .secondaryBackground
  hovered { borderColor: .accent }
  pressed { background: .separator; opacity: 0.8 }
  on press { open() }
}
```

只有当某处用到了某个状态，视图才会上报它。因此没有写 `pressed` 变体的视图，仍然保留平台自己的按下效果。变体里也可以写 `style:`（`pressed { style: raised }`），它位于变体自己的 prop 之下；样式里也可以写变体，应用到容器上时生效（其他元素会忽略）：

```asuka
style button {
  padding: [8, 16]; radius: 8; background: .accent
  pressed { opacity: 0.7 }
}
```

组件还可以声明自己的变体（`variant checked = checked`，见[组件](https://asuka.egoist.dev/zh/docs/language/components#组件声明的变体)），样式里可以像写交互状态那样写它们的代码块：应用到一个组件声明了该变体的元素上时，代码块在变体成立期间叠加上去，位于交互状态之上；用在其他元素上则什么都不做。名字拼错会报错：样式只能写本包组件的变体，以及其他包里 `pub` 的变体。

```asuka
// 用于声明了 selected 变体的组件（例如列表框的选项）。
style option {
  padding: [6, 10]; radius: 6
  pressed { opacity: 0.8 }
  selected { background: .accent; color: .onAccent }
}
```

组件的变体位于调用方的 prop 之上：调用方设置的 `background` 会替换组件自己的，而组件的 `pressed` 依然会让它变暗。其他视图可以通过名字跟随一个视图的状态（见[给视图命名](#给视图命名)）：写成 `View as card { … }`，然后在任意绑定里读 `card.hovered`（或 `card.pressed`、`card.dragging`、`card.dragOver`），子视图可以这样做（整组悬停），兄弟视图也可以。与样式无关的反应则交给处理器：`on pressedChange(p)`、`on hoverChange(h)`。

#### 给视图命名

有些事不是给视图设置一个值，而是要对视图做点什么：播放视频、在网页里运行脚本、往输入框里插入文本、滚动列表。`Ref` 就是视图的句柄：在任意元素上写 `ref: r`，它的视图就放进 `r`；写成 `Element as name`，则会为所在代码块的剩余部分声明一个这样的句柄（写在只显示一个元素的组件上时，指的是那个元素的视图）。

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

只有视图显示期间，Ref 才持有它；在那之前和之后都是空的，这时命令什么也不做，状态都是 false。Ref 的状态会被追踪，读取它的绑定会跟着它变化：

| 适用于 | Ref 的成员 |
|---|---|
| 任意视图 | `hovered`、`pressed`、`dragging`、`dragOver`（bool，会被追踪）；`focus()`、`blur()` |
| 组件的（`Checkbox as box`） | 它的变体：`box.checked`（bool，会被追踪） |
| `Scroll`、`List` | `scrollTo(x, y, animated: true)` |
| `Video` | `play()`、`pause()`、`seek(seconds)` |
| `WebView` | `try await eval(script)`（脚本的值，以 JSON 表示）、`run(script)`（不等待结果）、`goBack()`、`goForward()`、`reload()`、`stopLoading()` |
| `TextField` | `insert(text)`、`format(.bold, on)`、`toggleFormat(.italic)`、`setLink(url)`（作用于富文本输入框的选区） |

Ref 可以传来传去（`device.shareFiles(paths, from: button)`、`accessibility.focus(field)`），也可以放在文件顶层的 `let` 里，供多个组件的视图使用。

### 动画

`animate { … }` 会立即运行代码块，块中对 frame 和绘制内容（不透明度、背景、圆角半径、形状的裁剪）所做的改动，都会以原生动画呈现。`animate(.spring) { … }` 接收一个 `Animation`：时长、曲线、延迟、重复次数，以及弹簧动画的阻尼和初速度。`await` 它时，会等到动画完成才返回；不 `await` 也可以。

```asuka
on press {
  animate { expanded = !expanded }
}
on press {
  await animate(Animation(duration: 500ms, curve: .spring)) { offset = 0 }
  done = true
}
```

`animate(() => …)` 和 `animate(anim, () => …)` 是同样的写法，只是用闭包代替代码块。`repeat: Animation.forever` 会一直重复，直到属性再次改变。`animating()` 表示当前所做的改动是否处在动画中。`onFrame(t => …)` 在显示器绘制的每一帧调用闭包，传入这一帧的时间，直到调用它返回的闭包，或组件消失为止：`let stop = onFrame(t => tick(t))`。`frames()` 把同样的帧作为 `Stream<Duration>` 提供，只在被读取时才会绘制帧：在任务里写 `for await t in frames() { … }`，循环结束或任务消失时就会停止。

### 绘制

`Canvas(g => …)` 用一个 `Graphics` 来绘制，坐标以点为单位、从左上角算起。它在布局完成后绘制一次，尺寸或读取的内容变化时再重新绘制。`Shape(fn(width, height) -> Path)` 是一个由平台负责填充和描边的轮廓（`fill`、`stroke`、`lineWidth`、`trimEnd`、`dash` ……）。

```asuka
Canvas(g => {
  let ring = g.path().arc(g.width / 2, g.height / 2, 40, 0, 6.28318)
  g.stroke(ring, .accent, StrokeStyle(width: 8, cap: .round))
  g.text("{percent}%", g.width / 2, g.height / 2, CanvasText(size: 20, align: .center))
}) { height: 100 }
```

`Path` 通过它的方法构建，每个方法都返回它自己：`Path().move(x, y)`、`line`、`quad`、`cubic`、`rect`、`roundedRect`、`ellipse`、`arc(cx, cy, radius, start, end, clockwise: true)`（弧度，从 x 轴正方向起算）、`close()`；`g.path()` 是画布自己的一个空路径，每次绘制都会复用。`Graphics` 只在绘制函数运行期间有效：`width`、`height`、`fill(path, color)`（也可以传 `LinearGradient` 或 `RadialGradient`）、`stroke(path, color 或渐变, style)`、`save()`、`restore()`、`translate`、`scale`、`rotate(degrees)`、`opacity`、`clip(path)`、`transform(a, b, c, d, tx, ty)`、`evenOdd(on)`、`text(s, x, y, style)`。

`shader` 声明是一个由 GPU 程序绘制的方框。它用 Asuka 的语法编写，随应用一起编译（写错了就是编译错误，并指出所在的行），用的时候当作视图，参数就是它的 uniform：

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

`fn fragment(pos: vec2) -> vec4` 给出一个像素的颜色（预乘 alpha）；`size`、`scale` 和 `time` 分别是框的尺寸（点）、每个点的像素数和显示以来的秒数（`animated: true` 让它走起来）。参数可以是 `float`、`int`、`bool` 或 `Color`。着色器所用的语言和它在各平台上怎么运行，见[着色器](https://asuka.egoist.dev/zh/docs/components#着色器)。

### 系统功能

组件作为应用的一部分所用到的功能都来自 prelude，不需要导入：它存在期间注册的东西、它的环境、警告框，以及应用的窗口。

| 函数 | 作用 |
|---|---|
| `useKeys([Shortcut(…)])` | 组件存在期间，窗口的快捷键 |
| `onOpenURL(u => …)`、`onOpenFiles(paths => …)`、`onShare(items => …)` | 组件存在期间，应用被打开时带来的内容，或其他应用分享给它的内容 |
| `quit()`、`relaunch()` | 像用户选择“退出”那样退出应用（并重新启动它），会先询问应用；仅桌面端 |
| `onCloseRequest(fn() -> bool)`、`onQuit(e => …)`、`onSecondInstance((args, dir) => …)` | 组件存在期间：组件所在窗口关闭前的询问；应用即将退出（`e.cancel()`）；应用再次被启动 |
| `onBack { … }` | 用户从组件所在的页面、弹出面板或气泡弹窗返回时询问：`true` 表示接管（页面留下），`false` 表示放行 |
| `onLifecycle(l => …)` | 组件存在期间：接收应用进入前台、后台，以及内存警告（一个 `Lifecycle`） |
| `alert(title, message:, actions:)` | 立即显示一个警告框；`actions` 是一组 `.button`（没有时只有一个 OK）。`Alert` 元素则在它的 `open` 为 true 时显示一个 |
| `snackbar(message, action:, duration:, onDismiss:)` | 在窗口底部显示一条简短消息，可以带一个操作（撤销）；返回一个用来关闭它的闭包 |
| `environment()`、`screenEnvironment()` | 尺寸、安全区域、文字大小、平台等（会被跟踪）：页面或弹出面板自己的，以及整个屏幕的 |
| `window.main()`、`window.current()` | 主窗口的 `Ref`，以及代码所在窗口的 `Ref`：在任何地方都可以 `show()`、`hide()`、`focus()` …… |
| `window.close()` | 不询问，直接关闭代码所在的窗口 |

应用向系统服务请求的功能，在[平台 SDK](https://asuka.egoist.dev/zh/docs/stdlib) 的包里，和其他包一样导入后使用：

| 函数 | 作用 |
|---|---|
| `device.share(text)` | 为一段文本或一个链接弹出分享面板 |
| `try await device.shareFiles(paths, title:, from: ref)` | 为文件弹出分享面板，从某个视图弹出（弹出框指向的位置）；面板关闭后返回 |
| `device.haptic(.success)` | 平台的触感反馈（一个 `device.Haptic`） |
| `device.copy(text)` | 把文本放到剪贴板 |
| `device.openURL(url)` | 用能处理 url 的应用打开它 |
| `accessibility.announce(text)` | 让屏幕阅读器读出文本 |
| `accessibility.focus(ref)` | 把屏幕阅读器的焦点移到该视图 |
| `try await webview.cookies(url)` | Web 视图为 url 保存的 cookie |
| `appearance.setColorScheme(.dark)` | 应用的外观：`.light`、`.dark`，或 `.system`（跟随平台） |

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

应用在各个平台上包含哪些代码，由 `@platform` 决定，它是编译时就已确定的常量（见[特定平台的代码](https://asuka.egoist.dev/zh/docs/language/control-flow#特定平台的代码)）；`environment().platform` 则是视图实际运行的平台，和环境里的其他值一样会被追踪。

`useKeys` 和这些 `on…` 注册写在组件的主体里（或 app 代码块里）。app 代码块还可以把它们当作事件来处理：`on openURL(url) { … }`、`on files(paths) { … }`、`on share(items) { … }`、`on quit(e) { … }`、`on secondInstance(args, dir) { … }`、`on foreground { … }`、`on background { … }`。它的第一个 `Window` 是主窗口；如果 app 代码块在它的 `Window` 之外什么都不显示（只有托盘图标、菜单栏），它就没有主窗口（见[导航](https://asuka.egoist.dev/zh/docs/navigation#窗口)）。

`std/embed` 在应用编译时读取包里的文件，把它们编进应用，所以每个平台都直接拥有这些文件，运行时不需要加载：

```asuka
import std/embed

struct Config { api: string; retries: int }

let welcome = embed.text("welcome.md")         // string
let logo = embed.bytes("art/logo.png")         // bytes
let config: Config = embed.json("config.json") // 编译时检查
let posts = embed.texts("posts/*.md")          // [路径: 文本]
let icons = embed.files("icons/**/*.svg")      // [路径: 字节]
```

路径是字符串字面量，相对于包的目录（`asuka.toml` 所在的目录），且必须在这个目录之内；通配符支持 `*`、`?`、`[abc]` 和 `**`（任意层目录），不会匹配名字以点开头的文件。`embed.json` 和 `json.decode` 的读法一样，只是发生在编译时：文件和类型对不上就是编译错误，并指出行和列，而不会等到运行时才出错。文件不存在、通配符一个文件也没匹配到，同样是编译错误。用 `asuka dev` 时，修改被嵌入的文件会让应用就地重新加载，就像修改代码一样。运行时由平台读取的文件（图片、字体）应放在 `assets/` 里（见 [std/embed](https://asuka.egoist.dev/zh/docs/std/embed)）。

### 导航

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

`screen` 是一种组件，它的根是导航栈中的一个页面：它的代码块接收页面的 prop（`title`、`largeTitle` ……）和插槽（`toolbar`、`bottomToolbar`、`primary`）。`push(…)` 和 `pop()` 作用于最近的导航栈，`present(…)` 和 `dismiss()` 作用于弹出面板（sheet）。`Split` 是一个由 `Destination` 组成的侧边栏（可以放在 `Sidebar` 分区里），旁边是选中目的地的导航栈。

有了 `Router`，每个页面都有一个地址。用模式声明的页面是可路由的，参数有类型；调用它就得到它的地址，并在编译时检查：

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

详见[导航](https://asuka.egoist.dev/zh/docs/navigation)。

### 多语言文本

`tr.inboxTitle(unread: n)` 说出应用目录文件（`locales/en.json`、`de.json`……）里的一条文案，并在应用编译时检查：缺少键或参数，或者参数类型不对，都会报错。在绑定里说出的文案会跟随应用的语言，所以切换语言（`i18n.setLocale("de")`，来自 std/i18n）时，显示的内容也会随之改变；从右往左书写的语言还会让整个应用从右侧开始排版。详见[多语言](https://asuka.egoist.dev/zh/docs/i18n)。

### 原生代码

只有平台才能做的事，放在原生包里：用 Asuka 声明，再用各平台自己的语言实现：

```asuka
pub native interface Haptics {
  fn impact(style: Feedback)
}

pub native component MapView(region: Region) {
  prop pins: [Pin] = []
  event regionChanged(region: Region)
}
```

创建模块和调用它都可能失败（当前平台可能没有它的代码），所以两者都要加 `try`：`let h = try Haptics()`、`try h.impact(.light)`。`asuka native generate` 会生成各平台需要实现的 Swift、Kotlin、JavaScript 和 C 代码。详见[原生代码](https://asuka.egoist.dev/zh/docs/native)。

C 则可以直接调用。`extern` 声明用 C 的类型写明一个 C 函数接收什么、返回什么，Asuka 把每个声明都当作自己的函数来看待（`Int32` 就是 `int`，`CString` 就是 `string`）：

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

`extern type` 是 C 给出的一个指针，最后一个引用消失时由它的 release 函数释放；`extern struct` 按值传递；传给 C 的函数就是回调。`asuka.toml` 的 `[c]` 写明应用要编译和链接哪些 C 代码。详见[在 Asuka 中调用 C](https://asuka.egoist.dev/zh/docs/c)。

### 测试

```asuka
test "archiving removes the row" {
  let app = mount(Inbox(), provide: Mailbox.fake([sample]))
  await app.idle()
  app.tap("Archive")
  app.expectNo("Hello")
}
```

`expect(cond)` 在 cond 为 false 时让测试失败，并指出失败的是什么、在哪里（如果是比较，还会给出两边的值）。JSON 由 `std/json` 处理：`let notes: [Note] = try json.decode(text)` 按期望的类型读取 JSON（也可以写成 `json.decode<[Note]>(text)`），`json.encode(notes)` 把值写成 JSON。在需要 `json.Value`（任意 JSON）的地方，JSON 能表示的值会按它写出的样子自动成为 `json.Value`：混合类型的值组成的 map 就是一个 `[string: json.Value]`（`["port": port, "tls": true]`），其中的数字仍然是数字。泛型函数也可以把它的 `T` 当作 JSON 读写；这样每次调用时，`T` 都必须是 JSON 能表示的类型，编译器会在调用处检查这一点。

`asuka test` 不需要设备就能运行包里的测试：它会像各平台那样对视图进行布局，按每个字符固定宽度来测量文字，而且只有在没有其他东西可以运行时，时间才会前进（测试里的 `sleep(5s)` 不花任何时间）。`mount` 返回应用，供你驱动和检查：`tap`、`longPress`、`type(_, into:)`、`submit`、`toggle`、`slide`、`select`、`swipe(_, action:)`、`refresh`、`scrollTo`、`back`、`dismiss`、`pickDate`、`pickColor`、`expect`、`expectNo`、`texts()`、`disabled(text)`、`until`、`waitFor`、`idle`；`in(.navigationBar)`、`in(.sidebar)` 和 `nextTo(label)` 缩小文字所指的范围；有路由器时，还有 `mount(…, at: "/inbox/1")`、`href`、`history()`、`goBack()`、`goForward()`。详见[测试](https://asuka.egoist.dev/zh/docs/testing)。

测试可以写在任何文件里，也可以写在包的**测试文件**里：文件名以 `_test.asuka` 结尾（`parser_test.asuka`；只给一个平台的写成 `tray_linux_test.asuka`）。测试文件里的声明和导入（测试数据、辅助函数）只在运行这个包自己的测试时才属于这个包（`asuka test`、`asuka check`、编辑器）：构建出来的应用里没有它们，也没有任何测试块；导入这个包的其他包也永远看不到它们。所以测试文件可以放心导入只有测试才用得到的包，这些包不会被带进应用。

带参数的测试是属性测试，`asuka test` 会为参数生成值来运行它；失败的输入会被缩减成仍然失败的最简单的那组。`bench "name" { … }` 是基准测试，由 `asuka test --bench` 计时；其中的 `measure { … }` 是要计时的部分，写在准备工作之后；`keep(x)` 防止计算结果被编译器优化掉。`expectSnapshot(value)` 和 `expectInlineSnapshot(value, "…")` 把一个值打印出来的样子和它的快照比较，快照由 `asuka test` 写入。

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

## 诊断代码

- argument-count: 调用时参数太多或太少. 按函数的声明传参；只有带默认值的参数可以省略。
- argument-label: 具名参数在函数里没有对应的形参. 使用形参的名字（消息会给出建议），或者按顺序传参。
- async-in-effect: 在 effect 里写了 `await`. 改用 `task(what) { … }`：它在组件出现时运行，`what` 变化时取消上一次运行并再次运行。
- await-outside-async: 在不能等待的地方写了 `await`. 把函数标记为 `async`；在组件里则用 `let x = load f()` 或 `task { }` 加载数据。
- bad-operand: 运算符用在了它不接受的值上. 转换其中一边（`float(n)`），改用方法（`a.concat(b)`），或者给类型加上这个运算符对应的方法。
- brace-placement: 代码块的 `{` 要写在上一行的末尾. 把 `{` 移到上一行末尾：`fn f() {`、`Row {`、`if x {`。
- cannot-infer-type: 编译器无法从代码中推断出值的类型. 在值所在的地方写出类型：`let ids: [int] = []`、`Type.case`、`f<T>(…)`。
- cleanup-reads-what-it-follows: effect 的 cleanup 读取了 effect 所跟随的东西. 把这次运行的值存进一个 `let`，在 cleanup 里读它：先 `let r = room`，再 `cleanup { leave(r) }`。
- closure-keeps-itself: 闭包通过保存它的变量调用自己. 改为在代码块里声明函数 `fn name(…) { … }`：它可以调用自己。
- constraint-not-met: 类型缺少泛型或方法对它的要求. 换成满足要求的类型（比如用 id 代替整个值），或者给类型补上缺少的部分。
- deprecated-syntax: 已被替换的旧写法，`asuka fmt` 会自动改写. 运行 `asuka fmt -w`，或者写成 `style: name`。
- duplicate-declaration: 同一个地方声明了两次的名称. 给其中一个改名，或者删掉不需要的那个。
- effect-in-effect-pass: effect 的写入让一个绑定在一次刷新里运行了两次. 用 `let` 或跟随 effect 所读内容的状态来计算这个值：它一轮就能稳定下来。
- effect-loop: 在一次刷新（flush）里不断互相弄脏的 effect. 让这个值只有一个来源：用 `let` 或跟随状态计算它，或者在修改 effect 所读内容的处理函数里写入它。
- effect-writes-what-it-follows: effect 写入了它读取的状态. 在声明处把值限制在范围内（`state n: int = min(n ?? 0, 3)`），改为跟随别的东西（`effect(other) { … }`、`state x(other) = …`），或者在修改它所读内容的处理函数里写入。
- embed-file: `embed` 的文件不在包里. 修正路径，或者把文件加到包里。
- extension-conflict: 扩展添加了类型已有的名称，或者另一个扩展也添加了同样的名称. 给方法换个名字，或者只保留其中一个扩展。
- fn-body: 函数缺少函数体，或者有了不该有的函数体. 写出函数体（`{ … }` 或 `= 表达式`），或者从 native、extern 声明里去掉它。
- follows-nothing: `state x()` 没有可跟随的东西. 写出要跟随的东西（`state x(a) = …`），或者写成 `state x = peek(…)`。
- format-spec: 插值的格式和它的值对不上. 按值的类型写格式，参照消息的说明：`{n:>6}`、`{x:.2f}`、`{n:#x}`、`{name:<12}`。
- group-not-calls: `load (…)` 里的元素不都是调用. 只把调用放进组里（`load (api.user(id), api.posts(id))`），把值传给这些调用，或者放在 load 旁边。
- import-cycle: 相互导入的包. 把两者都需要的部分移到第三个包里，让它们都导入它。
- indentation: 某行的缩进对不上任何打开的块，或者在用缩进写的块里含有制表符. 把这一行和它所属的块对齐（与该块前面几行的缩进相同），或者改用花括号写这个块。
- inline-snapshot: 内联快照不是直接写在调用里的字符串. 把快照写成字符串字面量，或者干脆省略（`expectInlineSnapshot(value)`），让 `asuka test` 在第一次运行时写上。
- int-overflow: 超出所属类型范围的整数字面量. 换一个范围内的数，改用更宽的类型（`u16`、`int`、`u64`）或 `float`；需要超出 64 位的精确整数时，用 `std/bigint` 的 `BigInt`（`BigInt.parse("9223372036854775808")!`）。
- interface-not-satisfied: 类型声明了某个 interface，却没有实现它. 按 interface 的签名补上缺少的方法（消息里列出了它们）。
- internal-error: 编译器自身的错误. 请到 https://github.com/egoist/asuka/issues 报告（附上程序，或者能重现问题的最小程序）。
- invalid-catalog: 不是有效消息文件的目录. 在消息指出的位置修正文件：每个键对应一个字符串，占位符与源语言一致。
- invalid-catch: 执行不到或无错可捕的 `catch`. 把具体的 catch 放在前面，捕获一切的放在最后，并且只捕获可能抛出的错误。
- invalid-character: 在这里不属于这门语言的字符. 删掉这个字符，或者把这段文本放进字符串或注释里。
- invalid-conformance: 类型声明的、或类型参数要求的东西不是 interface. 在那里写一个 interface，或者去掉它。
- invalid-constant: 编译时生成的值不是字面量，或者无法解析. 直接传入字面量，并修正消息指出的无法解析的部分。
- invalid-declaration: 声明的写法不被它所在的位置接受. 按这个位置的写法来写：类型的常量用 `let name = value`，需要类型参数的就写成 `static fn`。
- invalid-entry: 程序的 `fn main` 声明方式与它的运行方式不符. 声明 `fn main() { … }`，并通过 std/os 读取参数。
- invalid-extension: `extend` 了不能扩展的东西，或者写法不对. 扩展一个有名字的类型，或者改为声明一个函数。
- invalid-extern: C 无法支撑的 extern 声明. 把释放函数声明为 `extern fn close(x: T)`，其余部分照 C 的样子声明。
- invalid-flag-spec: `flag.parse` 无法把命令行读进的类型. 给每个开关一个默认值（`verbose: bool = false`），选项使用消息里列出的类型。
- invalid-for: `for` 遍历了它不能遍历的东西. 按消息所说遍历对应的东西：`0..<n`、`m.keys()`、`for await x in stream`。
- invalid-literal: 写法不正确的数字、颜色、时长或转义. 按消息所说，用它的某种写法来写这个字面量。
- invalid-modifier: 在不适用的地方写了 `pub`、`mut`、`async`、`throws` 或类型参数. 去掉消息里指出的那个修饰符。
- invalid-pattern: 写法不被语言接受的模式. 按消息给出的写法来写：`.case(x)`、`if let`、`(k, v)`。
- invalid-project: 项目的 asuka.toml 或目录无法按它所写的读取. 按消息所说修正 asuka.toml 里的值。
- invalid-ref: `ref` 或 `as` 指向的不是单个元素的视图. 给组件内部的某个元素命名，或者让组件只显示一个元素。
- invalid-route: 页面的地址，或者路由无法完成的导航. 在模式里按页面的声明写出它的参数。
- invalid-shader: shader 里用了 GPU 语言没有的东西. 用数字、向量和 GLSL 的函数来写。
- invalid-style: 样式的声明或应用方式不被接受. 把样式写进同一个列表：`style: [a, b]`。
- invalid-variant: 变体的声明或用法不可行. 让组件只显示一个元素（把视图放进 `Row` 或 `Column`），在变体里只设置随它变化的属性。
- let-else: `let … else` 的 else 会继续往下执行，或者根本没有可解包的东西. 用 `return`（或 `throw`、`break`、`continue`）结束 `else`，或者去掉它。
- load-outside-component: 在组件或 store 的 `let` 之外使用了 `load`. 在组件里声明 `let x = load f(…)`，或者在函数里 `await` 这个调用。
- load-waterfall: 一个 load 等待了另一个它并不使用的 load. 在同一个组件里并排启动这两个 load，再把结果传下去。
- local-fn: 代码块里的函数用了只有文件顶层函数才有的特性. 需要这些特性时，把它声明在文件顶层。
- long-hold: load 保留旧内容已有好几秒，却没有任何提示. 在保留期间读取 `x.loading`（把内容调暗、显示一个转圈），或者在它的位置显示加载指示。
- loop-label: 循环名没有被使用，或者不存在. 去掉这个名字，或者用上它：`continue outer`。
- maybe-none: 把可能为 `none` 的值当作一定存在来使用. 使用 `?.`（`p?.name`），用 `??` 给出默认值，或者先解包：`if let p = p { … }`。
- message-arguments: 调用消息时给的参数与它的占位符不符. 按名字传入每个占位符：`tr.greeting(name: n)`。
- misplaced-declaration: 声明出现在它这一类不能出现的地方. 把它移到这一类声明该在的地方，或者改用这里接受的写法（在 worker 里用 `var` 代替 `state`）。
- misplaced-statement: 语句写在了无法发挥作用的地方. 把语句移到有意义的地方，或者在这里写出你真正想做的事。
- missing-await: 调用 async 函数时没有写 `await`. 写成 `await f(…)`（会抛错时写 `try await`），或者用 `spawn` 单独启动它。
- missing-prop: 元素缺少它必需的属性. 在元素的代码块里设置它：`Badge { count: 3 }`，或者给这个 prop 一个默认值。
- missing-return: 有返回值的函数可能在没有返回值的情况下运行到结尾. 在每条路径上返回值，或者在函数结尾加上 `return`。
- missing-translation: 某种语言的目录缺少源语言的部分消息. 在那种语言的目录里翻译列出的消息。
- missing-try: 调用会抛错的函数时没有写 `try`. 写成 `try f(…)`；出错时想得到 `none` 就用 `try? f(…)`，想给出替代值就用 `try f(…) catch { fallback }`。
- missing-type: 必须写出类型的声明. 写出类型：`prop size: int`、`prop size = 12`、`state n: int = …`。
- mixed-numbers: 运算符两边是不同类型的数字. 把一边转换成另一边的类型：`int(level) + offset`、`float(n) * 0.5`、`u8(n)`（n 放不下时抛出错误；`u8(truncating: n)` 只保留低位）。
- native-type: 原生组件的属性或事件使用了平台视图无法接收的类型. 使用平台代码能读取的数据；发生了什么用事件来报告。
- no-children: 给不接受子视图的元素传了子视图，或者传的子视图不对. 把视图放进容器（`Column`、`Row`），或者放在这个元素旁边。
- no-such-operator: 其他语言有、Asuka 没有的运算符. 写成 `x += 1`、`if a { b } else { c }` 或 `a < b && b < c`。
- no-value: 当作值使用、却不产生值的东西. 给 `if` 加上 `else`，让代码块以那个值结尾，或者改用会返回值的函数。
- non-exhaustive-match: 没有覆盖所有情况的 `match`. 为缺少的情况加上分支，或者用 `_ => …` 处理其余情况。
- not-a-type: 当作类型使用、实际上却不是类型的东西. 写出你想要的类型：`void?` 应写成 `bool`，可选类型只要一个 `?`。
- not-a-value: 在需要值的地方用了类型、包或方法. 创建这个类型的值（`Point(x: 1, y: 2)`），使用包的成员，或者调用这个方法。
- not-a-view: 视图之间出现了不是视图的值. 放进元素里显示：`Text("{n}")`。
- not-an-error: 抛出或捕获的值不是错误类型. 把类型声明为 `: Error`，或者抛出 `Error("…")`。
- not-assignable: 修改了不能修改的东西. 把它改成 `var`（或 `state`），修改 `let` 读取的东西，或者把方法声明为 `mut fn`。
- not-async-call: 传给 `load`、`all` 或 `race` 的不是 async 调用. 传入一个 async 调用（`load api.user(id)`），或者直接使用这个值。
- not-bindable: `bind` 了一个不报告变化的属性. 绑定控件的值，或者直接传值（`text: s`）并处理它的事件。
- not-c-type: 无法传给 C 的类型. 传 `bytes`，或者 `Pointer` 加长度，或者声明一个 `extern struct`。
- not-callable: 调用了不是函数的东西. 调用一个函数，直接使用这个值，或者创建枚举的某个 case（`Kind.name`）。
- not-json: JSON 无法读写的类型. 解码成只包含数据的 struct，再从它构造其余的东西。
- not-optional: 对一定有值的值使用了可选类型的运算符. 去掉这个运算符，或者在声明处把值改成可选类型（`int?`）。
- not-pub: 其他包、其他文件（private）、store 或 worker 没有公开的声明. 在声明处加上 `pub`（或去掉 `private`），或者改用公开的内容：私有字段没有默认值的类型，用它的 static fn 来创建。
- not-sendable: 无法传到另一个线程的值. 传入另一个线程需要的数据，而不是持有这些数据的对象。
- not-supported: 语言暂时还不支持的写法. 改用消息里给出的替代写法：泛型组件可以改为接收 `View` 或闭包作为 prop。
- optimistic-impure: 乐观改动读取了状态、await 了或者写入了. 先把需要的东西读进一个 `let`，在闭包里用它；写入放在处理函数里，不要放在改动里。
- optimistic-not-loading: 对不是 load 的东西调用了 `optimistic`. 直接修改状态（`notes.push(n)`），或者对 load 的 `let` 调用 `optimistic`。
- optimistic-not-ready: 对还没有就绪的 load 调用了 `optimistic`. 等它 `.ready` 之后再改动它显示的内容：把这个操作放在 `.ready` 分支里，或者先检查 `x.value`。
- optimistic-unconfirmed: 乐观改动随它的任务一起撤销了：它的 load 没有一次运行来确认它. 在任务结束之前再次请求这个 load：`await reload f(…)`，或者 `x.reload()`。
- pattern-type: 与值的类型不可能匹配的模式. 写出与值同类型的模式：对 int 用 `1`，而不是 `"one"`。
- peek-untracked: 在不跟随任何东西的地方使用了 `peek`. 直接读取这个值。
- plural-categories: 复数或选择消息的类别与语言的类别不符. 按消息列出的类别写出这种语言的类别。
- prop-not-assignable: 组件给不能赋值的 prop 赋了值. 声明 `event openChange(open: bool)` 后再赋值，或者改用组件自己的状态。
- prop-or-argument: 参数、属性、事件或子视图放错了位置. 把它移到该在的位置：`Badge(3) { compact: true; on tap { … } }`。
- recursive-definition: 用自己来定义自己的值或样式. 打破循环：让其中一个从不会反过来读取它的值计算出来。
- recursive-type: 包含自身完整副本的类型. 把路径上的某个字段改成可选（`next: Node?`）或数组（`children: [Node]`）。
- reload-arguments: `reload f(…)` 给出了 `f` 不接受的参数. 给出 `f` 接受的参数，和它的 load 调用它的方式一致；或者不给参数：`reload f` 会触及 `f` 的每一个 load。
- reload-not-loaded: `reload` 的对象没有被任何 load 调用. 重新加载某个 `load` 调用的函数（`reload api.notes`），或者通过句柄重新加载一个 load（`x.reload()`）。
- reserved-name: 语言本身在用的名称. 换一个名字。
- return-value: `return` 带了函数不返回的值，或者在需要值时没有带. 去掉这个值，或者声明返回类型（`-> T`）并在每个地方都返回值。
- row-without-key: 视图里的 `for` 遍历会变化的数据，却没有 `key`. 写出用什么标识一项：`for t in todos key t.id`。
- spawn-captures-state: `spawn` 读取或修改了状态，或者用了不是值的东西. 先把状态读进一个 `let` 再传进去；把 `spawn` 的返回值赋给状态。
- special-method: 语言会调用的方法（equals、hash、description、toJSON、fromJSON）签名不对，语言无法调用. 按消息里说的签名来写这个方法；如果它不是为此而写的，就换个名字。
- statement-in-view: 组件的视图之间写了语句. 把语句放进处理函数（`on press { … }`）、fn、task 或 effect；派生的值用 `let`。
- syntax-error: 解析器在这里期望的是别的内容. 看消息里的“expected …, found …”：补上它期望的内容，或在它指出的位置闭合未闭合的部分。
- test-parameter: 测试参数的类型是 asuka test 无法生成的. 让测试接收它所关心的数据（一个 id、一个由字段组成的 struct），其余的东西在测试体里用这些数据构造出来。
- too-large: 函数太大，字节码装不下. 拆分函数，或者把数据放进文件（`embed.json`）而不是写成字面量。
- tr-in-package: 在不是应用的包里使用了 `tr`. 把文本作为参数接收，由应用传入 `tr.key()`。
- type-arguments: 类型参数的个数不对. 按声明给出相同个数的类型：`Set<int>`、`Page<Item>`。
- type-mismatch: 值的类型和期望的类型不同. 给出期望类型的值，或者显式转换：`float(n)`、`int(x)`、`"{n}"`。
- undefined-name: 在使用的地方没有任何声明的名称. 修正拼写（消息会给出相近的名称），导入声明它的包，或者声明它。
- unexpected-block: 在不接受代码块的调用后面写了代码块. 把值写在括号里，去掉代码块。
- unhandled-error: 错误可能从没有人接收它的地方离开. 把函数标记为 `throws`，或者捕获错误：`try … catch { fallback }`、`do { } catch { }`、`try?`。
- unknown-event: 处理或 `emit` 了组件没有声明的事件. 使用它已有的事件（消息会给出建议），或者声明它：`event name(…)`。
- unknown-member: 类型、枚举或包里没有这个名称的成员. 使用它已有的成员（消息会给出建议），或者把这个成员加到类型上。
- unknown-message: `tr.key` 用了目录里没有的消息. 使用目录里有的键（消息会给出建议），或者把它加进目录。
- unknown-package: 导入的路径不对应任何包. 修正路径，添加依赖（`asuka add`），或者改用包的新名称。
- unknown-prop: 元素没有这个属性、插槽或变体. 使用它已有的属性（消息会给出建议）。
- unterminated: 字符串、注释或转义没有闭合. 把它闭合。
- unused-catalog-entry: 目录里没有被使用的描述或参数. 删掉它，或者在消息里用上这个参数。
- view-in-statement: 在会运行的代码里创建视图. 在主体里声明这个视图，放在一个由处理函数设置的状态控制的 `if` 下面。
- wide-fanout: 一次写入让 1,000 多个绑定和 let 重新运行，而且不止一次. 让每行按路径读取它显示的内容（`todos[i].done`、`for t in todos key t.id`），或者使用 set 的 `contains`，这样一次写入只会让读取它的东西重新运行。
- worker-captures-state: worker 读取了主线程的状态. 在主线程上读取状态（或 var），把它的值传给 `pub fn`。
- worker-misuse: 从外部读取了 worker 自己的值，或者在不能创建、调用 worker 的地方这么做了. 用 `pub fn` 返回这个值，并用 `await` 调用它。
- write-in-binding: 在界面跟随的地方写入了状态. 在处理函数（`on press { … }`）、fn、task 或 effect 里写入状态。
- write-in-memo: 在计算 `let` 时写入了状态. 在处理函数、fn、task 或 effect 里写入状态；`let` 只读取。
