# 1 ページでわかる Asuka

インストールした CLI がコンパイルする Asuka 言語と、その使い方です。すべてのエラーと警告にはコードがあり、`asuka explain <code>` でそのページを表示できます。

## 作業の流れ

- 検査：`asuka check --json`（1 行に 1 つの診断。それぞれにコード、メッセージ、ヘルプ、ページ）
- コードを調べる：`asuka explain <code>`
- テスト：`asuka test --json`（各テストの結果と、アプリの実行中にコアが報告するもの）。実行：`asuka dev`、プログラムは `asuka run`
- 整形：`asuka fmt -w`
- コンポーネントの本体は宣言です。状態はハンドラー、fn、タスク、エフェクトで書き込みます。ビューのリストには `key` が必要です。

## 言語

### Asuka のツアー

Asuka のアプリは Asuka で書きます。各プラットフォーム自身のウィジェットを使うインターフェースのために作られた、小さな言語です。静的型付けで、リリースビルドでは事前にコンパイルされ、開発中は保存するとその場で再読み込みされます。このページでは、言語に出会う順に見ていきます。それぞれの全体は[言語リファレンス](https://asuka.egoist.dev/ja/docs/language)にあります。

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

ファイルは宣言でできています：コンポーネント、関数、型、状態。`app { }` がアプリのルートで、それ以外の順番は自由です。

#### 値

```asuka
fn values() -> string {
  let name = "Asuka"               // 定数：型は推論される
  var visits = 0                   // 変数
  visits += 1
  let greeting = "Hello, {name}"   // {…} で値を文字列に埋め込む
  let ratio: float = 2.5
  let level: u8 = 200              // 固定幅の整数：i8 … i32、u8 … u64（int は 64 ビット）
  let area = 2 ** 10               // べき乗：1024
  let tags = ["new", "work"]       // 配列：[string]
  let ages = ["ana": 31, "bo": 28] // マップ：[string: int]
  let nickname: string? = none     // オプショナル：文字列か none
  let shown = nickname ?? name     // none のときの代わりの値
  return "{greeting} {visits} {ratio} {level} {area} {tags.count()} {ages["ana"] ?? 0} {shown}"
}
```

`var` は関数とハンドラーの中で使います。ファイルのトップレベルにあるものは、`state` でない限り変わりません。型の違う数値は混ぜられません。`int(level) + visits` は先に `u8` を変換します。また、型の範囲を超える算術はエラーになり、ラップアラウンドした数にはなりません。値は常にコピーされ、共有されることはありません。配列のコピーを変えても元の配列はそのままです。文字列、配列、マップには思いつくメソッドがそろっています（`count()`、`contains`、`map`、`filter`、`sorted`、`split` など）。[値と型](https://asuka.egoist.dev/ja/docs/language/types)を参照してください。

#### 関数

```asuka
fn area(width: float, height: float = 1) -> float {
  return width * height
}

fn double(x: int) -> int = x * 2     // 式ひとつ

let a = area(3, height: 4)           // 順番に、そのあと名前で
let squares = [1, 2, 3].map(x => x * x)
```

引数は順番に渡し、そのあと名前で渡します（`area(3, height: 4)`）。既定値のある引数は省略できます。`x => …` はクロージャです。[関数とクロージャ](https://asuka.egoist.dev/ja/docs/language/functions)を参照してください。

#### 構造体と列挙型

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

構造体はフィールドとメソッドからなり、`mut fn` は呼び出された値（`var` や状態）を変更します。`extend` を使うと、`string` や `Color` も含め、どんな型にもメソッドや定数を追加できます。列挙型のケースは値を持てます。`match` はすべてのケースを扱う必要があります（漏れていれば、どれが足りないかをコンパイラーが教えてくれます）。`if`、`match`、ブロックも値です。[宣言](https://asuka.egoist.dev/ja/docs/language/declarations)と[制御フロー](https://asuka.egoist.dev/ja/docs/language/control-flow)を参照してください。

#### コンポーネントと状態

```asuka
component Greeting(name: string) {
  prop excited = false                // prop と、その既定値
  state draft = ""                    // コンポーネント自身の状態

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

コンポーネントのヘッダーは引数を受け取り、ブロックには prop、状態、ビューが入ります。状態が変わると、それを読んでいるところだけが更新され、差分の計算はありません。変わるものから作った状態（`state draft = name`）は、書き込まれるまでそれに追従します。`peek(name)` は一度だけ読みます。`bind` は双方向バインディング、`on press` はイベントの処理で、`if` と `for` がどのビューがあるかを決めます。[コンポーネント](https://asuka.egoist.dev/ja/docs/language/components)を参照してください。複数のコンポーネントで共有する状態は [ストア](https://asuka.egoist.dev/ja/docs/language/stores) です。

#### 非同期処理とエラー

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

`async` の処理は画面を止めずに進み、`task { }` はそれを始めたコンポーネントのものです。コンポーネントがなくなるとキャンセルされます。失敗しうる関数は `throws` と書き、呼び出す側は `try` を付けます（エラーのときに none でよければ `try?`）。[非同期処理](https://asuka.egoist.dev/ja/docs/language/async)と[エラー](https://asuka.egoist.dev/ja/docs/language/errors)を参照してください。

コンポーネントがこうした呼び出しの結果を表示するときは `load` を使います。`let n = load fetchCount(online: true)` は `.pending`、`.ready(count)`、`.failed(e)` のいずれかで、もう一度求めるあいだも表示しているものを保ちます。`n.optimistic(f)` は変更をすぐに表示し、`reload fetchCount` はどこにあってもその load すべてにもう一度求めさせ、`load (a(), b())` は複数の呼び出しの答えをまとめて届けます。[読み込むデータ](https://asuka.egoist.dev/ja/docs/language/async#読み込むデータ)を参照してください。

#### パッケージ

```asuka
import std/time

let today = time.now().format("EEE d MMM")
```

`pub` を付けた宣言はほかのパッケージから見えます。標準ライブラリ（`std/…`）とプラットフォーム SDK（`platform/…`）は CLI に同梱されているので、import するだけで使えます。`tray_linux.asuka` という名前のファイルは Linux でだけパッケージに含まれ、`@platform == .linux` は定数なので、あるプラットフォーム向けのコードはほかのプラットフォームのビルドに入りません。`@debug` は `asuka dev` では true で、開発専用のコードはリリースビルドに入りません。[ファイル、パッケージ、インポート](https://asuka.egoist.dev/ja/docs/language/packages)、[標準ライブラリとプラットフォーム SDK](https://asuka.egoist.dev/ja/docs/stdlib)、[特定のプラットフォーム向けのコード](https://asuka.egoist.dev/ja/docs/language/control-flow#特定のプラットフォーム向けのコード)、[開発ビルド向けのコード](https://asuka.egoist.dev/ja/docs/language/control-flow#開発ビルド向けのコード)を参照してください。

#### テスト

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

`asuka test` はデバイスなしでパッケージの `test` ブロックを実行します。`mount` がビューを表示し、テストはタップや入力をして、表示されている内容を確認します。[テスト](https://asuka.egoist.dev/ja/docs/language/tests)を参照してください。

#### 次に読むもの

- [言語リファレンス](https://asuka.egoist.dev/ja/docs/language)：型とそのメソッドから、スタイル、ナビゲーション、ネイティブコードまで、それぞれの全体。
- [コンポーネント](https://asuka.egoist.dev/ja/docs/components)：アプリを形作る要素。
- [基本概念](https://asuka.egoist.dev/ja/docs/concepts)：何が更新されるか、オーナー、メインスレッド。

### ファイル、パッケージ、インポート

ソースファイルの拡張子は `.asuka` です。ソースファイルを入れたディレクトリが一つの**パッケージ**になります。同じパッケージのファイルどうしは、何もインポートしなくても互いの宣言が見えます。ほかのパッケージから見えるのは、`pub` を付けた宣言だけです。`private` を付けた宣言はそのファイルだけのものです（[非公開の宣言](https://asuka.egoist.dev/ja/docs/language/declarations#非公開の宣言)）。

```asuka
import std/http                 // 標準ライブラリ
import platform/camera          // プラットフォーム SDK
import @acme/charts               // 依存パッケージ（asuka add @acme/charts）
import ./components               // このパッケージと同じ階層にあるパッケージ
import ./model.{Message, Folder}  // パッケージ名を付けずに使う名前
import ./theme as t               // 短い別名
import ../model                   // 1 つ上のディレクトリにあるパッケージ
```

パッケージの名前は、パッケージ名を通して使います（`http.get(url)`、`components.Card("Hi")`）。パッケージ名はパスの最後の部分です。最後の部分が名前として使えないパッケージ（`@acme/line-charts`）は、`as` で名前を付けてインポートします。プレリュード（組み込みの型、要素、関数）はインポートする必要がありません。`std/...` と `platform/...` のパッケージは CLI に付属していて、一覧は[標準ライブラリとプラットフォーム SDK](https://asuka.egoist.dev/ja/docs/stdlib)にあります。ほかのパッケージには `std/` や `platform/` で始まる名前を付けられません。

文と宣言は行末で終わります。一行に複数書くときは `;` で区切ります。演算子、カンマ、開き括弧で終わる行は、次の行に続きます。次の行が `.name` や `?.name` で始まるとき、または式の先頭になれない二項演算子（`-` 以外のすべて。`-` は `-x` のように式の先頭になれます）で始まるときも、その行は次の行に続きます。そのため、メソッドチェーンは 1 ステップずつ行を分けて書けます。

```asuka
let names = users
  .filter(u => u.active)
  .map(u => u.name)
let label = user?.nickname
  ?? user?.name
  ?? "Guest"
```

`.name` で始まる行が独立したものになる場所が 2 つあります。`match` の中では、次のアーム（`.ready(v) => …`）です。また、最後の式をその値とするブロック（値として使う `if` や `match`、クロージャのブロック）の最後の行に単独で書かれているときは、その値です。

```asuka
let tint: Color = if urgent {
  log("urgent")
  .red  // if の値
} else { .gray }
```

このようなブロックでも、`.name` で始まる行が 2 行以上続けば、それは 1 つのチェーンです。

コメントは `// …` と `/* … */` です。`///` のコメントはその下にある宣言のドキュメントになり、エディターに表示されます。

名前は文字か `_` で始まり、そのあとに文字、数字、`_` が続きます。文字はどの言語の文字でもよく、結合文字も含みます（`total`、`नमस्ते`、`ราคา`、`名前`）。これは Unicode の識別子（UAX #31 の XID_Start と XID_Continue）で、Swift、Rust、Python と同じです。見た目が同じ名前は同じ名前です。名前は Unicode の NFC 形式で比較されるので、`é` を 1 文字で入力しても、`e` とアクセント記号で入力しても、同じ名前になります。

#### 特定のプラットフォーム向けのファイル

ファイル名がプラットフォームのサフィックスで終わるファイルは、そのプラットフォーム向けにアプリをビルドするときだけパッケージに含まれます。サフィックスは `_ios`、`_android`、`_macos`、`_windows`、`_linux`、`_web` のほか、複数のプラットフォームをまとめた `_desktop`（macOS、Windows、Linux）と `_mobile`（iOS、Android）があります。プラットフォームごとのファイルはそれぞれ同じ名前を宣言でき、パッケージのほかのファイルはそれをふつうの宣言と同じように使います。

```asuka
// feedback_mobile.asuka：iOS と Android
import platform/device

pub fn confirm() { device.haptic(.success) }

// feedback_desktop.asuka：macOS、Windows、Linux
import platform/accessibility

pub fn confirm() { accessibility.announce("Done") }

// feedback_web.asuka
pub fn confirm() {}
```

1 つの名前は、どのプラットフォームでも 1 回だけ宣言します。すべてのプラットフォームに含まれるファイルで宣言するか、その名前を持つプラットフォームごとのファイルでそれぞれ宣言するかのどちらかです。すべてのプラットフォームに含まれるファイルとあるプラットフォームのファイルの両方で宣言した名前や、あるプラットフォームで同時に含まれる 2 つのファイル（`feedback_desktop.asuka` と `feedback_linux.asuka`）の両方で宣言した名前は、どのプラットフォーム向けにビルドしてもエラーになります。サフィックスだけの名前のファイル（`linux.asuka`）は、ふつうのファイルです。

どのパッケージもファイル名で同じように扱われます。アプリのパッケージも、依存パッケージも、標準ライブラリも同じです。ビルドされるのは常にどれか 1 つのプラットフォーム向けです。`asuka dev` と `asuka build` はアプリを動かすプラットフォーム向けに、`asuka test` は `--platform` でほかを指定しない限り iOS 向けに、プログラムは実行するコンピューター向け（または `--target` の指定先向け）にビルドします。`asuka check` はプラットフォームごとにファイルをチェックし、エラーがすべてのプラットフォームのものでないときは、どのプラットフォームのエラーかを示します。たとえば一部のプラットフォームのファイルでしか宣言していない名前は、ほかのプラットフォームで使っている箇所がエラーになります。エディターは、あるプラットフォームのファイルをそのプラットフォームのファイルと合わせて読み、それ以外のファイルは iOS のファイルと合わせて読みます。エラーはすべてのプラットフォームのものを表示します。1 つのファイルの中でプラットフォームごとにコードを分けるには、[`@platform`](https://asuka.egoist.dev/ja/docs/language/control-flow#特定のプラットフォーム向けのコード) を使います。

#### アプリ、プログラム、ライブラリ

パッケージはエントリーポイントになるか、ライブラリになるかのどちらかです。`app { }` ブロックを持つパッケージはアプリで、このブロックが表示する内容になります（[ナビゲーション](https://asuka.egoist.dev/ja/docs/language/navigation#ナビゲーション)を参照）。ファイルで `fn main()` を宣言しているパッケージは**プログラム**、つまりコマンドラインツールです。`main` は引数を受け取らず（引数は `os.args()` で読みます）、値も返さず（`os.exit(code)` で終了コードを指定して終了します）、`async` にすることもエラーを投げることもできます。`main` が終わるとプログラムも終わります。どちらも持たないパッケージはライブラリで、ほかのパッケージからインポートされます。1 つのパッケージがアプリとプログラムの両方になることはできず、プログラムはコンポーネントを宣言できません。ビューを表示しないからです。エントリーポイントを持つのは、アプリやプログラムを構成するパッケージだけです。インポートしたパッケージにある `main` は、ほかの関数と変わりません。詳しくは[プログラム](https://asuka.egoist.dev/ja/docs/programs)を参照してください。

```asuka
import std/os

fn main() {
  print("Hello, {os.args().first() ?? "world"}!")
}
```

### 値と型

| 型 | 値 |
|---|---|
| `int` | 64 ビット整数：`42`、`1_000`、`0xff`、`0b1010`、`0o17` |
| `i8`、`i16`、`i32`、`u8`、`u16`、`u32`、`u64` | 8、16、32、64 ビットの整数。符号付き（`i`）と符号なし（`u`）があります：[整数型](#整数型) |
| `float` | 64 ビット浮動小数点数：`3.14`、`1e-3` |
| `bool` | `true`、`false` |
| `string` | UTF-8 のテキスト：`"Hello, {name}"` |
| `char` | 1 つの Unicode コードポイント（スカラー値）：`'a'`、`'\n'`、`'\u{1F600}'` |
| `bytes` | 生のバイト列 |
| `[T]` | 配列：`[1, 2, 3]`、`[]` |
| `[K: V]` | マップ（挿入した順に保持）：`["a": 1]`、`[:]` |
| `Set<T>` | セット（重複しない値の集まり、挿入した順に保持）：セットが求められる場所では `["a", "b"]` |
| `(A, B)` | タプル：`(1, "one")` |
| `T?` | オプショナル：`T` か `none` |
| `Duration` | `300ms`、`5s`、`2min`、`1h`（ほかに `us`、`ns`） |
| `Color` | `#7c3aed`、`#7c3aed80`、セマンティックカラー（`.label`、`.accent` など） |
| `Length` | `100`（ポイント）、`50%`、`.auto` |
| `fn(A) -> B` | 関数とクロージャ |

文字列の波括弧の中には、任意の式を書けます（`"{count} items"`、`"Total: {price * qty}"`）。`\{` は波括弧そのものを表します。`r"…"` はエスケープも埋め込みもない raw 文字列です。`"` を含む raw 文字列は `r#"…"#` と書きます。これは、開始時と同じ数の `#` が続く `"` で終わります（`r#"say "hi""#`、`r##"a "# b"##`）。`"""…"""` は複数行にまたがる文字列で（1 行目が空ならその行は含まれず、閉じ引用符の行のインデントも取り除かれます）、中の `"` や `""` はそのまま書けます。`r"""…"""` は複数行の raw 文字列です。数値、ブール値、文字列以外の値は、テキストとして表示すると次のようになります。型が自分の `description()` を持てばそれ（[宣言](https://asuka.egoist.dev/ja/docs/language/declarations#等しさハッシュテキストjson)）、列挙型のケースはその名前、オプショナルは `none` か中の値、エラーはそのメッセージ、色は書いたとおり（`#0a84ff`、`.accent`）、`Duration` はミリ秒（`1500ms`）、それ以外は JSON です。JSON で表せないものも同じように書き出されます。タプルは要素を括弧で囲んだ形になり、各要素は配列の要素と同じように書かれます（`(1,"a","red")`）。キーが文字列でないマップは、各キーをそれ自身の書き方で書きます（`{1:"a"}`）。関数はテキストにならないので、その型になります（`<fn(int) -> int>`）。

補間では、Python や Rust と同じように、コロンの後ろに書き方を指定できます：`"{price:>8.2f}"`。書式は `[[fill]align][sign][#][0][width][,|_][.precision][type]` です。`<`、`>`、`^` は幅の中で左揃え、右揃え、中央揃えにし、その前に埋め文字を書けます（省略すると空白）。`+` は正の数にも符号を付け、`0` は符号の後ろを 0 で埋めます。`,` か `_` は整数（と浮動小数点数の整数部）の桁を区切ります。`.2` は浮動小数点数の小数点以下の桁数で、テキストでは残す最大の文字数です。型は、整数を 16 進、8 進、2 進で書く `x`、`X`、`o`、`b`（`#` を付けると前に `0x`、`0o`、`0b` を書きます）か、数値を固定小数点、指数、パーセントで書く `f`、`e`、`%` です。数値は右揃え、それ以外の値はテキストとして左揃えになり、埋め文字、揃え、幅、精度だけを指定できます。書式はコンパイラーが読むので、値に合わない書式は書いた場所でエラーになります（[format-spec](https://asuka.egoist.dev/ja/docs/errors/format-spec)）。

```asuka
fn row(name: string, count: int, share: float) -> string = "{name:<10}{count:>6,}{share:>8.1%}"

let price = "{1234.5:,.2f}"          // 1,234.50
let hex = "{255:#06x}"               // 0x00ff
let title = "{"Total":*^11}"         // ***Total***
let table = row("asuka", 12500, 0.4) // asuka     12,500   40.0%
```

幅は文字（コードポイント）の数で数え、ターミナルでの表示幅ではありません。ターミナルの桁数で数えるには（漢字・かな・ハングルや絵文字は 2 桁）、`std/term` の `textWidth`、`padEnd`、`fit` を使います（[プログラム](https://asuka.egoist.dev/ja/docs/programs#揃った表)を参照）。

`print(a, b, c)` は、値を補間と同じ見た目で、スペースで区切って標準出力（アプリのログ）に 1 行書きます。`eprint` は標準エラーに書きます。Swift の `print` と同じく、値のあとに `separator:`（値のあいだに書くもの）と `terminator:`（最後に書くもの。指定しなければ改行）を渡せます：`print(x, y, separator: ", ")`、`print("Saving…", terminator: "")`。

タプルの要素は位置で読めます（`pair.0`、`pair.1`）。`var` のタプルなら、その場で変更もできます（`pair.0 += 1`）。タプルにない位置（2 要素のタプルの `pair.2`）はコンパイルエラーです。分解して読むこともできます：`let (count, label) = pair`、`for (i, x) in xs.enumerated()`、あるいは `match` のタプルパターン（[制御フロー](https://asuka.egoist.dev/ja/docs/language/control-flow#制御フロー)を参照）。

マップのキーとセットの要素は `Hashable` でなければなりません。整数、文字列、ブール値、バイト列、`Duration`、色と、それらからなる列挙型、構造体、タプル、オプショナル、配列、マップ、セットがそうです（自分自身を含む構造体や列挙型も、ほかの部分が `Hashable` なら `Hashable` です。マップはキーと値の両方が `Hashable` である必要があります）。浮動小数点数は NaN が自分自身と等しくないので `Hashable` ではなく、関数も違います。

```asuka
struct Cell {
  row: int
  col: int
}

let marks: [Cell: string] = [Cell(row: 0, col: 2): "x"]
let seen: Set<(int, int)> = [(0, 0), (1, 2)]
```

#### 演算子

結合の強いものから順に並べています。同じ行の演算子は優先順位が同じで、左から順に結合します（`**` と `??` は右から）。

| 演算子 | 意味 |
|---|---|
| `**` | べき乗：`2 ** 10` は 1024 |
| `!x`、`-x`、`~x` | 否定、符号反転、整数のビット反転 |
| `<<`、`>>` | 整数のビットを左へ、右へシフト（右シフトは符号付き整数の符号を保つ） |
| `*`、`/`、`%`、`&`、`&*` | 乗算、除算、剰余、ビット AND、ラップアラウンドする乗算 |
| `+`、`-`、<code>&#124;</code>、`^`、`&+`、`&-` | 加算（文字列や配列の連結にも使う）、減算、ビット OR、ビット XOR、ラップアラウンドする加算と減算 |
| `..`、`..<` | 範囲：`1..3`、`0..<n` |
| `==`、`!=`、`<`、`<=`、`>`、`>=` | 比較（連ねて書くことはできません） |
| `&&` | かつ |
| <code>&#124;&#124;</code> | または |
| `??` | オプショナルの中の値。`none` ならデフォルト値 |

`<`、`<=`、`>`、`>=` で順序づけられるのは、整数、浮動小数点数、文字列、`Duration`、値を持たないケースだけの列挙型（ケースを宣言した順）、それに `compare` メソッドを持つ型です。構造体と列挙型は、算術演算子もメソッド `add`、`sub`、`mul`、`div`、`rem`、`pow`、`neg` から得られます（[宣言](https://asuka.egoist.dev/ja/docs/language/declarations#構造体)を参照）。

`**` は上の表のほかの演算子より強く結合し、`-` よりも強く結合します。また、右から結合します。Python や数学と同じです。`-2 ** 2` は `-(2 ** 2)` で -4、`2 ** 3 ** 2` は `2 ** 9` で、`(-2) ** 2` は 4 です。整数のべき乗は底と同じ型になり、指数には任意の型の 0 以上の整数を取ります（負の指数はエラーです。`2 ** -1` は整数になりません）。浮動小数点数のべき乗は、指数に浮動小数点数も整数も取れます（`2.0 ** 0.5`、`x ** 2`）。代入には `**=` を使います。

ビット演算子が取るのは整数で、両辺は同じ型でなければなりません。シフト量は任意の型の整数です。型のビット幅以上シフトするとすべてのビットが押し出され（負の数の `>>` は -1 になります）、負の数でシフトするとエラーになります。`>>` は、符号付き整数では符号を保ち、符号なし整数では 0 で埋めます。これらの演算子と `b[i]`（バイトを整数として読む）を使えば、Bluetooth のキャラクタリスティックや NFC のレコードといったバイナリデータを読み書きできます（整数をまるごと、どちらのバイトオーダーでも読み書きするなら [`std/encoding`](https://asuka.egoist.dev/ja/docs/std/encoding)）。

```asuka
let raw = b[1] | b[2] << 8                         // リトルエンディアンの uint16
let celsius = if raw & 0x8000 != 0 { raw - 0x10000 } else { raw }
let out = bytes([flags, value & 0xff, value >> 8])
```

`int` が表せるのは −2⁶³ から 2⁶³ − 1 までです。`+`、`-`、`*`、`**` の結果がこの範囲を超えると、ゼロ除算（`/`、`%`）と同じようにエラー “integer overflow” が投げられます。`catch` で受け取れ、どこでも受け取られなければいちばん近い `Boundary` に届きます（[エラー](https://asuka.egoist.dev/ja/docs/language/errors#エラー)を参照）。最小の `int` の `-x` と `abs(x)`、`/ -1`、範囲を超える配列の `sum()` も同じです。個数やサイズ、金額がオーバーフローしたら、それは起きたその場で見つかるバグであって、黙って保存される間違った数ではありません。どの整数型も同じように、それぞれの範囲で検査されます。`u8` の `255 + 1` はエラーを投げます。

```asuka
let big = 9223372036854775807
let n = try big + 1 catch { 0 }               // 0：エラーが投げられた
let next = big.addingChecked(1) ?? big          // オーバーフローなら none：big になる
```

`int` の範囲を超える数には、`std/bigint` の `BigInt` を使います。任意の大きさの整数で、算術演算子も比較もそのまま使えます（[標準ライブラリ](https://asuka.egoist.dev/ja/docs/stdlib)を参照）。

```asuka
import std/bigint.{BigInt}

let big = BigInt.of(9223372036854775807)
let past = big * big + BigInt.one           // 85070591730234615847396907784232501250
let digits = "{BigInt.factorial(30)}"       // 265252859812191058636308480000000
```

`&+`、`&-`、`&*` は代わりにラップアラウンドします。最大の `int` を超えると、64 ビットの 2 の補数どおりに最小値の側から続きます（`9223372036854775807 &+ 1` は `-9223372036854775808`）。固定幅の型は、それぞれのビット幅でラップアラウンドします（`u8` の `255 &+ 1` は 0）。ハッシュ、チェックサム、バイナリプロトコルが求めるのはこの動作です。代入には `&+=`、`&-=`、`&*=` を使います。

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

オーバーフローを値として扱いたいときは、`a.addingChecked(b)`、`subtractingChecked`、`multipliedChecked` を使います。結果は `int?`（`u8` なら `u8?`、ほかの型も同様）で、オーバーフローすると none です。シフトとビット演算子が結果のせいでエラーを投げることはありません。範囲外の浮動小数点数に `int(x)` を使うと、いちばん近い `int` になります（NaN は `0`）。

#### 整数型

`int` のほかに、ビット幅の決まった整数が、符号付きと符号なしの両方あります。

| 型 | 値 |
|---|---|
| `i8`、`i16`、`i32` | −2⁷ から 2⁷ − 1、−2¹⁵ から 2¹⁵ − 1、−2³¹ から 2³¹ − 1 |
| `int` | −2⁶³ から 2⁶³ − 1（`i64` はありません。`int` がそれにあたります） |
| `u8`、`u16`、`u32`、`u64` | 0 から 2⁸ − 1（255）、2¹⁶ − 1、2³² − 1、2⁶⁴ − 1 |

個数、インデックス、サイズ、セント単位の金額には `int` を使います。ほかの型は、それ自体に決まった幅があるもののためのものです。バイト（`u8`）、ピクセルのチャンネル、ファイル形式やプロトコルのフィールド、ハッシュ、`int` の範囲を超える ID（`u64`）、C の関数の値（`Int32` は `i32`：[Asuka から C を呼び出す](https://asuka.egoist.dev/ja/docs/c)）などです。どの型も自分の範囲をそのまま守ります。`int` と同じく範囲を超える算術はエラーを投げ、JSON、CSV、TOML、SQLite からその型に読み込む値も検査されます。

整数リテラルは期待される型になり、その型に収まらなければなりません：`let level: u8 = 200`、`level + 1`、`let big: u64 = 18446744073709551615`。`let x: u8 = 256` や `let y: u8 = -1` は、書いた場所でエラーになります。期待される型がなければ `int` です。

型の違う 2 つの整数を 1 つの演算子で組み合わせることはできません。`int` と `float` を混ぜられないのと同じで、`int` と `u8` も混ぜられません。`level` が `u8`、`offset` が `int` なら `level + offset` はエラーで（[mixed-numbers](https://asuka.egoist.dev/ja/docs/errors/mixed-numbers)）、`int` を取るところに `u8` を渡すのもエラーです。どちらにするかは変換で書きます。

```asuka
fn conversions(n: int, level: u8, ratio: float) -> [u8] {
  let total = int(level) + n        // 拡大：必ず収まる
  let a = u8(n)                     // n が収まらなければエラー（"integer overflow"）
  let b = u8(exactly: n) ?? 0       // u8?：収まらなければ none
  let c = u8(truncating: n)         // 下位 8 ビット：u8(truncating: 300) は 44、-1 なら 255
  let d = u8(clamping: n)           // いちばん近い値：下回れば 0、上回れば 255
  let e = u8(ratio)                 // 浮動小数点数：0 の方向へ切り捨て、範囲外ならいちばん近い値
  return [a, b, c, d, e, u8(clamping: total)]
}
```

`u8.min` と `u8.max` は型の範囲です（`int.max` は 9223372036854775807）。`u64.parse(text)` は、`toInt()` が `int` を読むのと同じようにテキストから値を読み（`radix:` も使えます）、整数でないときや収まらないときは none を返します。`int` の最大値を超える `u64` も、表示、解析、比較、除算、JSON への書き出しで、そのままの数として扱われます（`18446744073709551615`）。

`-x` と `abs(x)` が取るのは符号付き整数です。符号なし整数には負の数がありません（`0 &- x` はラップアラウンドします）。整数型には `int` のメソッド（`toString(radix:)`、`padded(width)`、`float()`、`abs()`、チェック付きのメソッド）があり、書式指定（`"{id:x}"`）が使えます。数として順序づけとハッシュができ（マップやセットのキー、ジェネリクスでの `Comparable` と `Hashable`）、範囲も作れます：`for i in u16(0)..<count`。

`bytes` から読んだバイトは `int` です（`b[i]`、0 から 255）。そのため、それに対する算術は 8 ビットでラップアラウンドしません。`b.toArray()` はバイト列を `[u8]` として返し、`bytes(xs)` は `[int]` からと同じように `[u8]` からもバイト列を作ります。

#### 組み込みメソッド

文字列、バイト列、配列、セット、マップ、オプショナルにはメソッドがあります。値そのものを変更するメソッド（`push`、`pushFirst`、`insert`、`removeAt`、`removeWhere`、`pop`、`popFirst`、`clear`、`sort`、`remove`）は、`var` か状態に対してだけ呼べます。

| 型 | メソッド |
|---|---|
| `[T]` | `count()`、`count(where)`、`isEmpty()`、`first()`、`last()`、`get(i)`（`T?` を返す）、`push(x)`、`pushFirst(x)`、`insert(x, at: i)`、`removeAt(i)`、`removeWhere(p)`、`pop()`、`popFirst()`、`clear()`、`map(f)`、`flatMap(f)`（`f` が返す配列を順につなげる）、`compactMap(f)`（`f` の結果から none を除く）、`filter(p)`、`reduce(initial, f)`、`any(p)`、`all(p)`、`index(p)`（`int?` を返す）、`find(p)`、`contains(x)`、`sum()`、`min()`、`max()`（`T?` を返す。`min(by:)`、`max(by:)` は「より小さい」の比較を、`min(key:)`、`max(key:)` はキーを取る）、`unique()`、`grouped(by: f)`（`[K: [T]]` を返す）、`chunked(n)`、`zip(other)`（短いほうの長さまでペアにする）、`reversed()`、`slice(from, to)`（`to` を省くと末尾まで）、`prefix(n)`、`suffix(n)`、`sorted()`、`sorted(by)`、`sorted(key:, descending:)`、`sort()`、`sort(by)`、`sort(key:, descending:)`、`enumerated()`、`joined(separator:)`（文字列の配列）、`lazy()`（`Seq<T>` を返す）、`+` |
| `Set<T>` | `count()`、`isEmpty()`、`contains(x)`、`insert(x)`（新しく加わったかを返す）、`remove(x)`（あったかを返す）、`union(s)`、`intersection(s)`、`subtracting(s)`、`isSubset(of: s)`、`isSuperset(of: s)`、`lazy()`。`Set(xs)` は配列の重複しない要素、`Set<T>()` は空のセット |
| `[K: V]` | `m[k]`（`V?` を返す）、`m[k] = v`、`count()`、`isEmpty()`、`has(k)`、`get(k)`、`remove(k)`、`keys()`、`values()`（挿入した順）、`mapValues(f)`、`filter((k, v) => …)`、`lazy()`（`(key, value)` の組） |
| `string` | `count()`（文字数）、`byteCount()`、`isEmpty()`、`upper()`、`lower()`、`trim()`、`trimStart()`、`trimEnd()`、`trimPrefix(s)`、`trimSuffix(s)`、`contains(s, ignoreCase:)`、`hasPrefix(s, ignoreCase:)`、`hasSuffix(s, ignoreCase:)`、`equals(s, ignoreCase:)`、`compare(s, ignoreCase:)`（負、ゼロ、正のいずれか）、`foldCase()`、`replace(old, new, count:)`（count を指定しなければすべて置き換え）、`split(sep, count:)`、`fields()`（空白で区切った単語）、`lines()`、`find(s)`、`findLast(s)`、`findAny(chars)`（バイト単位の位置。見つからなければ none）、`slice(from, to)`（文字単位。`to` を省くと末尾まで）、`byteSlice(from, to)`（バイト単位：find が返す位置。`to` を省くと末尾まで）、`padStart(width, with:)`、`padEnd(width, with:)`、`toInt()`、`toInt(radix:)`、`toFloat()`、`repeat(n)`、`chars()`、`graphemes()`、`lazy()`（char の `Seq<char>`）、`reversed()`、`bytes()`、`+` |
| `bytes` | `b[i]`（0 から 255 の `int`。範囲外はエラー）、`count()`、`isEmpty()`、`get(i)`（`int?` を返す）、`slice(from, to)`（`to` を省くと末尾まで）、`concat(b)`、`string()`（UTF-8 として読み、不正な部分は U+FFFD になる）、`hex()` と `hex(separator)`（`"04 a1"`）、`toArray()`（`[u8]` を返す）。`bytes()` は空のバイト列、`bytes([0x04, 0xa1])` はそのバイト列、`bytes(xs)` は `[u8]` のバイト列、`s.bytes()` は文字列の UTF-8 |
| `int`、`i8` … `u64` | `float()`、`abs()`（符号付きの型）、`toString(radix:)`（`255.toString(radix: 16)` は `"ff"`）、`padded(width)`（`7.padded(2)` は `"07"`）、`addingChecked(o)`、`subtractingChecked(o)`、`multipliedChecked(o)`（`int?`、`u8` なら `u8?`：オーバーフローなら none）。`T.min`、`T.max`、`T.parse(text, radix:)`（`T?` を返す） |
| `float` | `fixed(digits)`、`rounded()`、`floor()`、`ceil()`、`int()`（0 の方向へ切り捨て）、`abs()` |
| `Color` | `Color.parse(text)`（`#rrggbb`、`#rgb`、`#rrggbbaa`：`Color?` を返す）、`Color.rgb(r, g, b, alpha:)`。`opacity(a)`：元の不透明度の a 倍で描く（セマンティックカラーはセマンティックカラーのまま）。`hex()`（`"#0a84ff"`。セマンティックカラーはその名前：`".accent"`） |
| `Duration` | `nanoseconds()`、`milliseconds()`（整数）、`seconds()`、`minutes()`、`hours()`（浮動小数点数）、`rounded(to:)`、`truncated(to:)`、`+`、`-`、`* n`（整数でも浮動小数点数でも：`1.5 * 1h`）、`/ n`、`/ d`（別の Duration：何個分か。浮動小数点数） |
| `Length` | `* n`（`width: 50% * 2`：長さを拡大縮小） |
| `T?` | `isNone()`、`isSome()` |

渡した関数を呼び出すメソッド（`map`、`filter`、`reduce`、`sorted(by:)`、`grouped(by:)`、マップの `mapValues` など）は、エラーを投げる関数も受け取れます。その場合、関数が投げたエラーをそのまま投げるので、呼び出しには `try` が必要です。エラーを投げない関数を渡したときは、メソッドも投げません。

```asuka
fn port(text: string) throws -> int = text.toInt() ?? throw Error("not a port: {text}")

fn ports(lines: [string]) throws -> [int] = try lines.map(l => try port(l))
```

いくつかを組み合わせると、次のようになります。

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
  let pages = sales.chunked(50)                               // [[Sale]]、50 件ずつ
  var out = ["{best?.region ?? "none"} sold the most"]
  for (region, total) in totals { out.push("{region}: {total}") }
  for (i, page) in pages.enumerated() { out.push("page {i + 1}: {page.count()}") }
  out.push("{codes.reduce(0, (sum, c) => sum + c)}")
  return out
}
```

`unique()` は各要素を最初に現れた位置に一度だけ残し、`grouped(by:)` もキーを最初に現れた順に並べます。どちらも値（`grouped` ではキー）が `Hashable` である必要があります。`min()` と `max()` は `sorted()` と同じ順序で比べ、等しい要素があれば最初のものを返します。`chunked(n)` の `n` が 1 未満だと、範囲外の `xs[i]` と同じくエラーになります。

`sorted(key:)` は、関数が各要素から求めるキーの順に並べます。キーは `Comparable` で、要素ごとに一度だけ求められます（`people.sorted(key: p => p.age)`）。`descending: true` を付けると大きいものから並び、どちらの向きでも安定ソートなので、キーが等しい要素は元の順序を保ちます。`sort(key:)` は `var` をその場で並べ替え、`min(key:)` と `max(key:)` はキーが最小、最大の要素を返します（等しければ最初のもの）。一つのキーでは決まらない順序には、「より小さい」の比較を取る `sorted(by:)` を使います。

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

配列は両端キューでもあります。`pushFirst(x)` と `popFirst()` は、`push(x)` と `pop()` が末尾を変えるのと同じように先頭を変え、どれも O(1)（償却。先頭を一度変えた配列は、先頭に余白を持ちます）です。`insert(x, at: 0)` と `removeAt(0)` も同じです。それ以外の位置での挿入や削除では、短い側の要素だけが動きます。つまりキューは配列そのもので、`queue.push(job)` で入れて `queue.popFirst()` で取り出します。

`toInt(radix:)` は文字列全体を 2 から 36 までの基数で読みます。英字は大文字でも小文字でもよく、先頭に符号を付けられます（`"ff".toInt(radix: 16)` は 255、`"-101".toInt(radix: 2)` は -5）。読めなければ none です。逆に `toString(radix:)` は整数をその基数で、英字は小文字で書き出します。基数が 2 から 36 の範囲外ならエラーです。

`min`、`max`、`abs`、`float(n)`、`int(x)`、それにほかの整数型への変換（`u8(x)`）は関数です。それ以外（`math.sqrt`、`math.pow`、`math.pi` など）は `std/math` にあります。

`upper()` と `lower()` は Unicode の規則で文字を変換し、どの言語でも結果は同じです。`"straße".upper()` は `"STRASSE"` になり、単語の末尾の `Σ` は小文字で `ς` になります。`ignoreCase: true` は二つの文字列のケースフォールディング（`foldCase()`。`"Straße"` も `"STRASSE"` も `"strasse"` になります）を比べます。Go の `strings.EqualFold` や Python の `casefold` と同じ考え方です。一致は文字の切れ目で始まって切れ目で終わる必要があるため、`"ß"` は `"s"` で始まるとはみなされません。どれも特定の言語の規則には従いません。トルコ語の点のない ı や点付きの İ もほかと同じように変換され（`"İ".lower()` は `i` と結合用の点）、文字列の正規化もしません（`"é"` と、`e` に結合アクセントを付けたものは別物です）。大文字小文字を区別せずに名前を並べ替えるには、`compare` を `sorted(by:)` に渡します。`foldCase()` は大文字小文字を無視するキーになります。

```asuka
fn byName(names: [string]) -> [string] = names.sorted(by: (a, b) => a.compare(b, ignoreCase: true) < 0)

fn sameUser(a: string, b: string) -> bool = a.trim().equals(b.trim(), ignoreCase: true)

fn isMarkdown(path: string) -> bool = path.hasSuffix(".md", ignoreCase: true)
```

`count()` と `chars()` はコードポイントを数えます。`graphemes()` は読み手が一文字と感じる単位を返します。Unicode のテキスト分割（UAX #29）に従うので、`"e\u{301}"`、国旗（`"🇯🇵"`）、ZWJ でつないだ家族の絵文字はそれぞれ一つです。

`char` は 1 つのコードポイントで、Rust の char と同じです。`for c in s` がたどり、`s.chars()` が順に返すのがこれです（`e` とアクセントのように複数のコードポイントからなる書記素は文字列で、`graphemes()` が返します）。単一引用符で書き（`'a'`、`'\''`、`'\u{301}'`）、コードポイントで比較・順序づけされ、マップのキーにもなります。出力するとそれ自身になり、JSON では 1 文字の文字列です。`c.isLetter()`、`isDigit()`、`isNumber()`、`isWhitespace()`、`isUpper()`、`isLower()`、`isPunctuation()`、`isSymbol()` は Unicode のカテゴリーを読み、どの文字体系にも使えます（`'٣'.isDigit()`）。`upper()` と `lower()` は結果が 1 つの char になるときに変換します（`'ß'.upper()` は `'ß'` のままで、`"ß".upper()` は `"SS"`）。`code()` はその番号で、`char(n)` は番号 n の char です（n がコードポイントでなければエラー）。`s + c` と `s += c` で文字列に char を足せます。

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


`toInt()` と `toFloat()` は文字列全体を読み、読めなければ none を返します。符号は付けても付けなくてもよく、その後に数字が続き、前後に余計なものがあってはいけません（`" 12"` は none なので、先に `trim()` します）。`toInt()` が読むのは整数（`"+7"`、`"-42"`）だけで、`toFloat()` は小数部と指数（`"2.5"`、`".5"`、`"1e-3"`）、それに浮動小数点数を表示したときの形（`"Infinity"`、`"NaN"`）も読みます。どちらも、ユーザーの言語にかかわらず小数点は `.` です。

#### 遅延シーケンス

`xs.lazy()` は配列の要素を `Seq<T>`、つまり遅延シーケンスにします。その `map`、`filter`、`take` などは別の `Seq` を作るだけで、何も計算しません。計算するのは、それを読むもの（`for`、`toArray()`、`first()`、`reduce` など）です。要素は一つずつ、すべての段階をまとめて通って求められ、途中で配列は作られません。そのため、百万個の要素に処理をつなげて最後に三つだけ残すなら、計算されるのはその三つと、それを見つけるまでに必要だった要素だけです。終わりのないシーケンスも作れます。

```asuka
fn squaresUnder(limit: int) -> [int] = Seq(from: 1).map(n => n * n).takeWhile(n => n < limit).toArray()

fn firstLong(lines: [string]) -> string? = lines.lazy().map(l => l.trim()).find(where: l => l.count() > 80)

fn fibonacci() -> Seq<int> = Seq(state: (0, 1), next: s => (s.0, (s.1, s.0 + s.1)))
```

マップの `lazy()` は `(key, value)` の組を、セットのものは要素を、文字列のものは文字を、範囲のもの（`(0..<n).lazy()`）は数を、配列を作らずに返します。`Seq(…)` は関数からシーケンスを作ります。

| 作り方 | 要素 |
|---|---|
| `Seq(from: n, by: step)` | `n`、`n + step`、……と終わりなく続きます。`by` を省くと 1 です |
| `Seq(first: x, next: f)` | `x`、`f(x)`、`f(f(x))`、……と、`f` が none を返すまで続きます |
| `Seq(state: s, next: f)` | `f(s)` が次の要素と次の状態の組 `(x, s2)` を返し、終わりでは none を返します |
| `Seq(repeating: x, count: n)` | `x` を `n` 回。`count` を省くと終わりなく続きます |

| | メソッド |
|---|---|
| 新しい `Seq` を返す | `map(f)`、`filter(p)`、`compactMap(f)`、`flatMap(f)`（`f` が返す `Seq` の要素を順につなげる）、`enumerated()`、`zip(other)`、`take(n)`、`skip(n)`、`takeWhile(p)`、`skipWhile(p)`、`chunked(n)`、`windows(n)`（隣り合う `n` 個ずつ。`[1, 2, 3]` の `windows(2)` は `[1, 2]`、`[2, 3]`）、`onEach(f)`（要素が読まれるたびに `f` を呼ぶ）、`unique()`、`concat(other)` |
| 読む | `toArray()`、`toSet()`、`toMap()`（`(key, value)` の組から）、`count()`、`count(where:)`、`first()`、`last()`、`find(where:)`、`any(where:)`、`all(where:)`、`contains(x)`、`reduce(initial, f)`、`sum()`、`min()`、`max()`、`min(by:)`、`max(by:)`、`min(key:)`、`max(key:)`、`joined(separator:)` |

`take`、`find`、`first`、`any`、`all` は答えが出た時点で要素を取り出すのをやめ、その先の要素は一切計算しません。`Seq` は読むたびに先頭から計算されます（`for` で二回回せば二回計算します）。ほかの値と同じく、作った時点のデータ元の値を保持するので、あとで配列を変更しても `Seq` は変わりません。

段階の関数はシーケンスが読まれるときに実行されるので、エラーを投げられません。投げる必要があるときは `for` の中で行うか、配列の `map` を使います。読むときにその場で呼ばれる関数（`reduce`、`find(where:)`、`any`、`count(where:)`、`min(key:)` など）は投げてもよく、その場合は配列のメソッドと同じく、呼び出しがそのエラーを投げます。

優先度付きキューと、キーの順に保つマップとセットは、`std/collections` の `Heap`、`SortedMap`、`SortedSet` です（[標準ライブラリ](https://asuka.egoist.dev/ja/docs/stdlib)を参照）。

#### 値のセマンティクス

文字列、配列、セット、マップ、構造体、列挙型は**値**です。代入したり引数に渡したりすると、見かけ上はコピーが渡されます。コピーのコストはわずかです。コピーどうしは、どれかが変更されるまで同じメモリを共有します。また、一か所からしか持たれていない値は、その場で変更されます。

```asuka
var a = [1, 2, 3]
var b = a
b.push(4)        // a は [1, 2, 3] のまま
```

ストア、クロージャ、タスク、ビューは**参照**です。コピーしても同じものを指します。

null はありません。ないかもしれない値はオプショナルで表します。

```asuka
let user: User? = none
if let u = user { Text(u.name) }
let name = user?.name ?? "Guest"
let first = items.first()!        // none ならクラッシュする。none になりえないときだけ書く
```

`?.` のあとは、チェーンの残りの部分も none で止まります。`user?.boss.name` は `string?` です。

オプショナルは入れ子になりません。値はあるかないかのどちらかです。`int??` と書くとエラーになり（`?` は一つで足ります）、ジェネリックな `T?` の `T` がすでにオプショナルなら、`T?` はその `T` です。

### 宣言

```asuka
let pi = 3.14159             // 定数
var total = 0                // 変数
state count = 0              // 状態：UI が追従する値
state shown = count          // 書き込まれるまで count に追従する状態
```

型は推論されます。書いたほうがわかりやすいところでは明示します（`let ids: [int] = []`）。

`type` は、ファイルのトップレベルで型に名前を付けます（`type Pair<A, B> = (A, B)`、`type Check = fn(string) -> bool`）。これは同じ型の別名（エイリアス）なので、`Pair<int, string>` は `(int, string)` そのものです。

#### ファイルの値

ファイルのトップレベルでは、`let` は定数、`state` はアプリの状態（[ストア](https://asuka.egoist.dev/ja/docs/language/stores)）、`var` はパッケージの変数です。パッケージのどのコードからも読み書きでき、`pub` ならほかのパッケージからもできます（`util.hits += 1`）。var は状態ではありません。読むとその時点の値が得られ、それを表示するビューは値が変わっても更新されません（それは `state` の役目です）。キャッシュ、カウンター、プログラムの設定など、ビューに表示しないものに使います。

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

ファイルの値は、ほかのどのコードよりも先に作られます。パッケージの値はそれをインポートするパッケージの値より先に、それぞれの値は初期化式が読む値のあとに作られます（直接にせよ間接にせよ自分自身を読むのはエラーです）。var はローカル変数と同じくその場で変更され、`cache[n] = v` は何もコピーしません。状態と同じくメインスレッドのものなので、`spawn` のブロックと [ワーカー](https://asuka.egoist.dev/ja/docs/language/async#ワーカー) では使えず、それを使う関数も呼べません（コンパイラーがどれかを示します）。先に読み出して、その値を渡します。`asuka dev` がアプリを再読み込みすると、ファイルの var は let と同じく初期化式から作り直されます。状態は値を保ちます。

#### 構造体

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

構造体は、フィールドを名前付きか順番どおりに渡して呼び出すことで作ります。デフォルト値のあるフィールドは省略できます。メソッドからはフィールドを名前でそのまま参照できます。`fn` は値を読むだけですが、`mut fn` は値を変更できます。フィールド（とその一部）に代入したり、ほかの `mut fn` を呼んだり、`self` に新しい値を代入したりできます。

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

`mut fn` は、変更できるものに対して呼び出します。`var`、状態（コンポーネント、store、アプリのもの）、またはそれらのフィールド、要素、マップのエントリー（`carts[0].add(i)`、`order.cart.clear()`）です。`let`、引数、prop、呼び出しの戻り値に対しては呼び出せず、コンパイラーがそう指摘します。状態に対して呼び出した場合、フィールドをいくつ変更しても、その状態の一回の変更として扱われます。更新されるのは `todos[i]` を読む部分だけで、すべての行ではありません。`mut fn` は `async` にできません（`await` をまたいで途中まで変更された値が、そのあいだに動くすべてのハンドラーから見えてしまうためです。非同期の変更は store の `fn` にします）。エラーを投げることはでき、その場合も投げる前に加えた変更は残ります。`mut fn` の中で作ったクロージャーは、作った時点の `self` の値を持ち、それを変更することはできません。

構造体の本体に書いた `let` はその型の定数（`Font.title`）になり、`Font` が求められる場所ならどこでも `.title` と書けます。

構造体や列挙型の本体に書いた `static fn` は、`self` を持たない、その型の関数です。ほかの関数と同じく名前付き引数とデフォルト値を使って型に対して呼び出せ（`Money.of(dollars: 2)`）、その型が求められる場所では `.of(…)` とも書けます。

```asuka
struct Money {
  cents: int

  static fn of(dollars: int, cents: int = 0) -> Money = Money(cents: dollars * 100 + cents)
}

let price: Money = .of(dollars: 4, cents: 50)
```

構造体は型パラメーターを取れます。型引数は作るときに推論されますが、ジェネリック関数と同じように名前の後に書くこともできます（ストアも同様です）。

```asuka
struct Page<T> {
  items: [T]
  next: string? = none
}

let page = Page(items: [1, 2, 3])     // Page<int>
let empty = Page<string>(items: [])   // ほかに手がかりがないときは書き出す
```

ジェネリックな構造体の `static fn` は、構造体の型パラメーター、続いて自分の型パラメーターを取ります。引数から推論されるか（`Stack.of([1, 2])`）、求められている型から推論されるか（`let s: Stack<string> = .empty()`）、構造体の名前と一緒に書くか（`Stack<int>.empty()`）、関数名のあとに書きます（`Stack.empty<int>()`。構造体のものが先）。ジェネリックな構造体の `let` は、どの型で作っても共通の 1 つの定数なので、その型に型パラメーターは使えません。使う必要がある定数（空の `Stack<T>` など）は `static fn` にします。

```asuka
struct Stack<T> {
  items: [T] = []
  let limit = 1000

  static fn of(items: [T]) -> Stack<T> = Stack(items: items)
  static fn empty() -> Stack<T> = Stack()
}

let numbers = Stack.of([1, 2, 3])       // Stack<int>
let names: Stack<string> = .empty()     // 求められている型から
let flags = Stack.empty<bool>()         // 書き出す
let most = Stack.limit
```

`match` と `if let` では、構造体パターンで構造体のフィールドを名前でマッチさせられます。`y` とだけ書くとフィールド `y` を束縛し、`y: py` なら `py` という名前で束縛し、`x: 0` は値にマッチします。書かなかったフィールドは何にでもマッチします。パターンは入れ子にできます。

```asuka
match line {
  Line(from: Point(x: 0, y: 0), dashed: true) => "dashed, from the origin"
  Line(from: Point(x: 0, y), to) => "from {y} on the y axis to {to.x}"
  Line(dashed) => "a line"
}
```

構造体と列挙型は、メソッドから演算子を得ます。`fn add(o: T) -> R`（または `plus`）は `a + b` に、`fn sub(o: T) -> R`（または `minus`）は `a - b` に、`fn mul(o: T) -> R`（または `times`）は `a * b` に、`fn div(o: T) -> R`（または `quo`）は `a / b` に、`fn rem(o: T) -> R` は `a % b` に、`fn pow(o: T) -> R` は `a ** b` に、`fn neg() -> R` は `-a` に、`fn compare(o: T) -> int`（負、ゼロ、正）は `<`、`<=`、`>`、`>=` になり、`sorted()` や `T: Comparable` にも使われます（[関数](https://asuka.egoist.dev/ja/docs/language/functions#インターフェースとジェネリクス)を参照）。`T` は値そのものと違う型でもかまいません（`Money * int`）。名前が二つある演算子では、それぞれのメソッドが別の `T` を受け取ります。`Decimal` の `mul(o: Decimal)` と `times(n: int)` が、`price * rate` と `price * 3` になります。逆の順序（`3 * price`）は導かれません。メソッドが `a` と同じ型を返すなら、`a += b` はこれらのメソッドによる `a = a + b` で（`-=`、`*=`、`/=`、`%=`、`**=` も同じ）、変数、状態、フィールド、要素に使えます（状態の変更は一度だけです）。`std/time` の `Time` が `Duration` を足し、別の `Time` を引けるのはこのためです。

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

構造体がフィールドごとに順序づけられることはありません。その順序が何を意味するかは、`compare` が決めます。

#### 等しさ、ハッシュ、テキスト、JSON

構造体は、フィールドによって等しいかを比べ（`==`）、マップのキーとしてハッシュし、出力し、JSON に書かれます。列挙型はケースとペイロードによります。型は、`compare` が `<` になるのと同じく、言語が呼ぶメソッドでこれを変えられます。

| メソッド | 呼ぶもの |
|---|---|
| `fn equals(other: T) -> bool` | `==` と `!=`、マップのキー、セットの要素、`contains`、配列の `==` |
| `fn hash() -> int` | マップのキーとセットの要素。等しい値は同じハッシュになるので、自分の `equals` を持つ型は自分の `hash` も持ちます（`hashOf(x)` は値のハッシュで、組み合わせるのに使います） |
| `fn description() -> string` | 補間、`print`、スナップショット |
| `fn toJSON() -> J` | `json.encode`、そしてその値を含む値のテキスト。`J` は JSON で表せる任意の型です |
| `static fn fromJSON(json: J) throws -> T` | `json.decode`。投げたエラーがデコードのエラーになります |

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

これらは値がどこにあっても使われます。配列、マップ、ほかの構造体の中でもそうです。これで、フィールドの意味で等しい型（大文字小文字を区別しない名前、順序を問わない集合）をマップのキーにでき、関数（順序など）を持つ型も、持っている中身で比較、ハッシュ、JSON 化できます。ジェネリックな型は、そのパッケージの拡張でこれらを与え、型引数が拡張の制約（`Equatable`、`Hashable`、`Comparable`）を満たす型にだけ効きます。std/collections の `Heap<T>` は、`T` が Hashable なら比較でき、Comparable なら JSON から読めます。これらは `async`、`mut`、private にはできず、ペイロードのない列挙型が持てるのは自分の `description` だけです（等しさと書き出しはケースによります）。シグネチャーが言語の呼ぶものと違えば、コンパイラーが知らせます（[special-method](https://asuka.egoist.dev/ja/docs/errors/special-method)）。

#### 非公開の宣言

`private` は宣言をそのファイルだけのものにします。ファイルの `fn`、`let`、`var`、struct、enum、interface、type、そして構造体や列挙型のフィールド、メソッド、`static fn`、定数に付けられます。そのファイルのコード（その型のメソッド、関数、拡張）からはほかと同じように使えますが、ほかのファイルからは名前で参照できません。例外はパッケージのテストファイル（`name_test.asuka`）で、内部をテストするために使えます。`pub` を付けないものはパッケージのもの、`private` を付けたものはファイルのものです。

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

ほかのファイルでは、構造体は private なフィールドなしで作られるので、それぞれに既定値が要ります（`Counter()`）。既定値のない private なフィールドを持つ構造体は、そのファイルの中でだけ、つまり static fn で作られ、それらが構造体の規則（`Decimal` の係数に末尾のゼロがないことなど）を守ります。private なメソッドは演算子（`add`、`compare`）にならず、型にインターフェイスを与えることもありません。それらはどこからでも使われるからです。

#### 拡張

`extend` は、型にメソッド、`mut fn`、`static fn`、定数を追加します。対象は組み込みの型（`string`、`int`、`[T]`、`[K: V]`、`Set<T>`、`Color`、`Duration`）でも、同じパッケージの型でも、ほかのパッケージの型でもかまいません。

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

拡張のメソッドからは、その型自身のメソッドと同じように値が見えます。フィールドを参照でき、メソッドも名前だけで呼び出せます（`lower()` は `self.lower()` のことです）。その型の定数は、型自身の定数と同じく、その型が求められる場所ならどこでも `.name` と書け、`Color.brand` とも書けます。`static fn` も同様です。拡張はフィールドも状態も追加できず、追加する名前を型自身のメンバーと同じにすることもできません。型のレイアウトとメンバーは、あくまで型の宣言が決めます。ジェネリックな拡張は、型パラメーターを `extend` の後に書きます（`extend<K: Hashable, V> [K: V]`）。

拡張が追加したものは、そのパッケージの中で見え、`pub` なメンバーはそのパッケージをインポートしたパッケージからも見えます。それ以外からは見えません。そのため、依存先のパッケージがあなたのコードの `x.trim()` の意味を変えてしまうことはありません。見えている二つの拡張が同じ型に同じ名前を追加している場合は、それを使った場所でエラーになり、両方のパッケージが示されます。

型は、見えている拡張が与えたものによってインターフェースを持つこともできます。`extend Money { fn area() -> float … }` によって `Money` は `Shape` になります。`extend Money: Shape { … }` と書けばそれを明示でき、コンパイラーは `struct Money: Shape` と同じように確認します。ほかのパッケージのペイロードのない列挙型にインターフェースを持たせるには、この書き方を使います。一つのプログラムの中で、型が持てる同じ名前のメソッドは一つだけです。二つのパッケージの拡張が同じ型に同じメソッドを与え、どちらもそれをインターフェースの実装にしている場合はエラーになります。

#### 列挙型

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

列挙型の `mut fn` も値を変更できます。多くの場合は `self` に代入します。

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

型がわかっている場所では、ケースを `.name` と書きます。`match` はすべてのケースを網羅しなければなりません（または `_ =>` で終えます）。パターンでは、ケースの値を束縛したり（`.from(p)`。ラベルでも書けます：`.server(code: c)`）、リテラルにマッチさせたり（`0`、`"x"`）、複数のケースをまとめたり（`.a | .b`）、入れ子にしたり（`.from(Person(vip: true))`）、ガードを付けたり（`.from(p) if p.vip =>`）できます。`match` がすべての値を網羅しているかを調べるとき、コンパイラーは入れ子のパターンの中までたどり、足りないものをパターンの形で示します（`match doesn't cover .key(_, false)`）。ケースのラベル付きの値のうち、ゼロ値（`false`、`0`、`""`、空の配列やマップ、`none`、列挙型の最初のケース）があるものは、作るときに省略できます。`.button(title: "Copy", run: copy)` は `Action.button` の `icon`、`role`、`disabled`、`checked` を省略しています。

ケースの値には、その列挙型自身も持たせられます。そのため、列挙型で木構造を表せます。

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

### 関数とクロージャ

```asuka
fn area(width: float, height: float = 1) -> float {
  return width * height
}

fn double(x: int) -> int = x * 2      // 式が一つだけの関数

area(3, height: 4)
let squares = [1, 2, 3].map(x => x * x)
let sorted = people.sorted((a, b) => a.age < b.age)
```

引数は位置で渡すものが先、名前で渡すものが後です。クロージャは `x => expr`、`(a, b) => expr`、`x => { statements }` と書きます。クロージャは、キャプチャした変数を周りのコードと共有します。どちらかが変更すれば、もう一方にもその変更が見えます。

```asuka
var n = 0
let inc = () => { n += 1 }
inc()
inc()                // n は 2
```

状態も同じように共有されます。使うものをコピーするのは `spawn` だけです（値だけを、実行を始めた時点のまま受け取ります）。別のスレッドで動くためです。

クロージャが非同期かどうか、エラーを投げるかどうかは、渡す先の関数型（`async fn(int) -> int`）で決まります。型が決まっていないとき、たとえば `let` に入れたクロージャでは、本体で決まります。`await` があれば非同期になり、エラーが外に出うるならエラーを投げます。前に `async` を書くと、今 `await` しているかどうかにかかわらず非同期だと明示できます。あとで `await` を足しても、呼び出す側には何も変わりません。そうしたクロージャは、待てない関数を求める場所には渡せません。

```asuka
let refresh = async () => {
  await store.reload()
}
await refresh()
```

クロージャの本体のせいで呼び出しに `await` や `try` が足りないときは、エラーが本体のどこが原因かを示します。

クロージャをその場で呼び出す組み込みメソッド（配列の `map`、`filter`、`reduce`、`sorted(by:)` など）は、エラーを投げるクロージャも受け取れます。その場合、呼び出し自体がクロージャの投げたエラーを投げるので、`try` が必要です。Swift の `rethrows` と同じです。クロージャが投げなければ、呼び出しも投げません。

```asuka
let counts = try lines.map(l => try parse(l))
let sorted = try names.sorted(by: (a, b) => try rank(a) < try rank(b))
```

`fn` は、関数、ハンドラー、クロージャ、メソッドの本体の中でも宣言できます。クロージャと同じように周りのものを参照でき、自分自身や、同じブロックで隣に宣言した `fn` を呼び出せます（隣の `fn` からも呼び出せます）。

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

ほかの関数と同じく値としても使えます（`items.map(walk)`）。引数は順番どおりに渡し、ラベルやデフォルト値は使えません。ジェネリックにもできません。

決して戻らない関数（必ずエラーを投げる、panic する、プログラムを終了する）は、`os.exit` のように `-> never` でそれを示します。その呼び出しは `return` と同じようにブロックを終わらせ、値が来る場所ならどこにでも書けます：`let port = text.toInt() ?? fail("not a port")`。

関数が自分自身を呼び出すときは、こう書きます。クロージャを変数に入れ、その変数を通して自分を呼び出す（`var walk = …; walk = n => { … walk(c) … }`）と、クロージャは変数を持ち、変数はクロージャを持つことになります。このような循環参照を回収する仕組みはないので、どちらも解放されません。コンパイラーはこれを警告します。ローカルな `fn` なら、自分自身を持たずに自分を呼び出せます。

#### インターフェースとジェネリクス

インターフェースはメソッドとフィールドの集まりです。それらを持っている型は、そのインターフェースを満たします（何も宣言する必要はありません）。

```asuka
interface Shape { fn area() -> float }

fn total<T>(items: [T], size: fn(T) -> float) -> float {
  var sum = 0.0
  for i in items { sum += size(i) }
  return sum
}
```

インターフェースはそれ自体が型でもあります。インターフェース型の値には、そのメソッドを持つどんな値でも入れられ、メソッドを呼ぶとその値のメソッドが呼ばれます。

```asuka
let shapes: [Shape] = [Circle(radius: 1), Square(side: 2)]
let areas = shapes.map(s => s.area())
```

インターフェースは `mut fn` を求めることもできます（prelude の `Iterator<T>` が求める `mut fn next() -> T?` など）。型の `mut fn` はそれを満たし、同じシグネチャーの `fn` も満たします。逆に、`mut fn` はインターフェースの普通の `fn` を満たしません。普通の `fn` はどんな値に対しても呼び出されるからです。型は、見えている[拡張](https://asuka.egoist.dev/ja/docs/language/declarations#拡張)が与えたメソッドによってインターフェースを持つこともできます。

ジェネリックな関数と型は型パラメーターを取ります。型パラメーターの制約には、プログラム自身のインターフェースと、`Equatable`（`==` で比べられる型：組み込みの値と、各部分が比べられるか自分の `equals` を持つ構造体、列挙型、タプル、配列、マップ、オプショナル）、`Comparable`（`<` で順序づけられる型：整数、浮動小数点数、文字列、`Duration`、ケースの順に並ぶ値を持たないケースだけの列挙型、`fn compare(other: T) -> int` を持つ構造体と列挙型。構造体がフィールドごとに順序づけられることはありません）、`Hashable`（マップのキーにできる型：浮動小数点数以外の組み込みの値と、それらからなるか自分の `hash` を持つ構造体、列挙型、タプル、オプショナル、配列、マップ、セット。[値と型](https://asuka.egoist.dev/ja/docs/language/types#値と型)と[等しさ、ハッシュ、テキスト、JSON](https://asuka.egoist.dev/ja/docs/language/declarations#等しさハッシュテキストjson)を参照）が使えます。複数の制約は `&` でつなぎます。型引数が持っていなければならないものは、ジェネリックなコードの中でその値から使えます。インターフェースのメソッドとフィールドは、コンパイルされる具体的な型ごとに直接呼び出されます。

インターフェースのメソッドには本体を書くこともでき、そのメソッドはインターフェースが提供するものになります。型がインターフェースを満たすのは、本体のないメソッドを持っているときです。提供されるメソッドについては、同じシグネチャのメソッドを型が自分で宣言していればそれが使われ、なければインターフェースのものが使われます。提供されるメソッドの中では `self` がそのインターフェースの値で、インターフェースのメソッドとフィールドは名前だけで使えます。

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

提供されるメソッドは、インターフェース型の値と、そのインターフェースを制約に持つ型パラメーター（後述の `T: Shape`）から呼べます。`Shape` として扱われていない `Square` には `describe()` はありません。2 つのインターフェースがそれぞれ同じ名前のメソッドを提供していて、型自身がそのメソッドを持っていない場合、その型を両方のインターフェースとして使うことはできません。コンパイラーがそう伝えるので、型に自分のメソッドを宣言してください。

インターフェースの値が実際にどの型なのかは、そのインターフェースを満たす型のパターンでマッチさせて調べます。構造体のパターン（`Circle(radius)`）、型つきの名前（`c: Circle`）、型を明記した列挙型のケース（`Unit.big(n)`）が使えます。どんな型でもインターフェースを満たしうるので、インターフェースの値に対する `match` は `_` で終わらせます。

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

パターンの型はそのインターフェースを満たしていなければなりません（`area()` を持たない `Point` が `Shape` であることはありません）。値を持たないケースだけの列挙型は、そのインターフェースを宣言したとき（`enum Unit: Shape`）だけ満たします。具体的な型の値には調べる必要がなく、`Circle` に `c: Circle` をマッチさせるとエラーになります。型パラメーターの値に対しては、関数が型ごとにコンパイルされるときに結果が決まり、マッチしえないアームはプログラムに含まれません。`catch e: ParseError` も同じ判定で、`Error` に対して行うものです（[エラー](https://asuka.egoist.dev/ja/docs/language/errors)を参照）。

ジェネリックな関数と型は型パラメーターを取ります。型パラメーターの制約には、プログラム自身のインターフェースと、`Equatable`（`==` で比べられる型：組み込みの値と、各部分が比べられるか自分の `equals` を持つ構造体、列挙型、タプル、配列、マップ、オプショナル）、`Comparable`（`<` で順序づけられる型：整数、浮動小数点数、文字列、`Duration`、ケースの順に並ぶ値を持たないケースだけの列挙型、`fn compare(other: T) -> int` を持つ構造体と列挙型。構造体がフィールドごとに順序づけられることはありません）、`Hashable`（マップのキーにできる型：浮動小数点数以外の組み込みの値と、それらからなるか自分の `hash` を持つ構造体、列挙型、タプル、オプショナル。[値と型](https://asuka.egoist.dev/ja/docs/language/types#値と型)を参照）が使えます。複数の制約は `&` でつなぎます。型引数が持っていなければならないものは、ジェネリックなコードの中でその値から使えます。インターフェースのメソッドとフィールドは、コンパイルされる具体的な型ごとに直接呼び出されます。

```asuka
fn sortByArea<T: Shape>(xs: [T]) -> [T] = xs.sorted((a, b) => a.area() < b.area())

fn largestOnce<T: Shape & Equatable>(xs: [T]) -> [T] { … }

struct Labeled<T: Shape> {
  item: T
  fn label() -> string = "{item.area()}"
}
```

ジェネリック関数の型引数は、引数と、戻り値に期待される型から推論されます。推論できないときは `f<T>(…)` で明示します：`json.decode<[Note]>(text)`、`flag.parse<Options>(args)`。

ジェネリックは使われる型ごとにコンパイルされるので、より大きな型で自分自身に戻ってくることはできません。`fn depth<T>(x: T) -> int` の中で `depth([x])` を呼ぶと `depth<[int]>`、次に `depth<[[int]]>` が必要になり、終わりがありません。`struct Nested<T> { more: Nested<[T]>? }` も同じです。コンパイラーはその場所を示します。自分自身を含む型は同じ型引数を使います（`Tree<T>` の中の `Tree<T>`）。

インターフェースのメソッドとフィールドを持っていれば、型引数はそのインターフェースを満たします。構造体や列挙型（値を持たないケースだけの列挙型も含む）、それらを求めるインターフェース、同じように制約された別の型パラメーターがそうです。メソッドは、インターフェースと同じパラメーターを取るときにだけ、そのインターフェースのメソッドになります。パラメーターが多いメソッドは、増えた分にデフォルト値があっても（インターフェースの `greet(loud: bool)` に対して `greet(loud: bool, times: int = 1)`）違います。インターフェース越しの呼び出しは、インターフェース自身のパラメーターしか渡さないからです。`T: Error` ならエラーの `message` を読めます。

### 制御フロー

`if` と `match` は値としても使えます。値として使う `if` には `else` が必要で、ブロックの値はその最後の式です。

```asuka
let label = if unread > 0 { "{unread} new" } else { "Inbox" }
let icon = match (online, unread) {
  (false, _) => "wifi.slash"
  (true, 0) => "tray"
  _ => "tray.full"
}
```

値から型がわからない分岐（空の `[]` や `[:]`、暗黙の `.case`）は、もう一方の分岐の型になります。`if let c = chosen { [c] } else { [] }` は `[Story]` です。

タプルパターン（`(true, 0)`）は、タプルの要素ごとにそれぞれのパターンでマッチさせます。オプショナルなタプルに対しては、ケースのパターンと同じように、値があればそのタプルにマッチします（`none` は別のアームで扱います）：`if let (name, age) = found { … }`、`let (x, y) = cursor else { return }`。

型つきの名前（`c: Circle`）は、インターフェースの値が何の型なのかを調べます。値がその型のときにマッチし、その型として束縛します。構造体のパターン（`Circle(radius)`）や型を明記した列挙型のケース（`Unit.big(n)`）も同じです。インターフェースの値に対する `match` は `_` で終わらせます。詳しくは[インターフェースとジェネリクス](https://asuka.egoist.dev/ja/docs/language/functions#インターフェースとジェネリクス)を参照してください：`if let c: Circle = shape { … }`、`let sq: Square = shape else { return }`。

`for` で回せるのは、配列、セット、マップの `(key, value)` の組、文字列の char（`for c in name`）、遅延シーケンス（`Seq`。[値と型](https://asuka.egoist.dev/ja/docs/language/types#遅延シーケンス)を参照）、範囲です。`a..<b` は `a` から `b` の手前までの整数、`a..b` は `b` を含む整数です（`0..<3` は `[0, 1, 2]`。範囲を `for` で回しても配列は作られません）。`while cond { … }` は cond が成り立つあいだ繰り返します。`break` はループを抜け、`continue` は次の周回に進みます。

`for` では、自分で定義した型の値も回せます。一つはイテレーターで、型に `mut fn next() -> T?` があるものです（`for` はそのコピーに対して `none` が返るまで呼び出します）。もう一つはシーケンスで、`fn iter()` がイテレーターか `Seq` を返すものです（`for` は一度だけ呼び出します）。prelude の `Iterator<T>` はそのような型を表し、制約に使えます（`fn sum<I: Iterator<int>>(xs: I) -> int`）。イテレーターは `none` で終わりを表すので、要素そのものが none になりうる（`T?` の）型は `Seq` を返します（`fn iter() -> Seq<int?> = slots.lazy()`）。

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

`for` や `while` の先頭の後に `as name` と書くと、ループに名前を付けられます。その内側のどのループからでも、`break name` でそのループを抜け、`continue name` でその次の周回に進めます（抜けるすべてのループの `defer` が実行されます）。クロージャーの中からは使えません。宣言して使わない名前や、宣言せずに使う名前はエラーになります。

```asuka
for row in grid as rows {
  for cell in row {
    if cell == target { found = true; break rows }
    if cell.isEmpty() { continue rows }
  }
}
```

`if let` と `while let` は、オプショナルの中の値を取り出すか、パターンにマッチさせ、取り出せたときだけブロックを実行します（`if let u = user { … }`、`while let job = queue.pop() { … }`、`if let .ready(v) = state { … }`）。条件は、カンマで区切った節の並びにもできます。`let` とブール値の条件は、どんな順番で並べてもかまいません。各節では、それより前の節で束縛した名前を使えます。ブロックが実行されるのはすべての節が成り立ったときです（節は順に評価され、成り立たない節が出たところで止まります）。

```asuka
if let user = session.user, user.isAdmin, let team = teams[user.teamId] {
  Text("{team.name}'s admin")
} else {
  Text("Not an admin")
}
```

`if var` は `if let` と同じですが、束縛する名前が変更できるコピーになります。いくつかのフィールドを変更して書き戻すコードに向いています（`if var m = draft { m.read = true; m.tags.push("seen"); draft = m }`）。

`let … else` は、値を束縛するか、その場を離れます。オプショナルが none のとき（またはパターンにマッチしないとき）は `else` のブロックが実行され、このブロックは必ず離れなければなりません。離れ方は `return`、`throw`、`break`、`continue`、`panic(…)` のいずれかです（先へ進めてしまう場合はコンパイラーがエラーにします）。この文より後では、名前はオプショナルではなく中の値を指します。

```asuka
fn summary(id: string) -> string {
  let message = cache[id] else { return "(deleted)" }
  let .ready(body) = message.body else { return message.subject }
  return "{message.subject}: {body}"
}
```

`throw`、`return`、`break`、`continue` は、値を書く場所にも置けます。値を返す代わりにその場を離れます（型は `never` で、どんな型の場所にも当てはまります）。`??` の後ろに書けば、none のときにどうするかを表せます。

```asuka
let user = cache[id] ?? throw MailError.missing(id)
for line in lines {
  let n = line.toInt() ?? continue
  total += n
}
```

`if` の分岐、`match` のアーム、クロージャの本体にも同じように書けます（`x => x ?? throw Error("empty")`）。

#### ないかもしれないものの変更

`var` や状態から始まり、フィールド、添字、`?.` をたどる `?.` チェーンには、代入（`=`、`+=` など）や `mut fn` の呼び出しができます。チェーンが `none` に当たると何も起きません。何も書き込まれず（右辺の値も計算されません）、状態を読む側にも通知されません。

```asuka
inbox[id]?.read = true          // そのメッセージがなければ何もしない
session.user?.name = draft
todos.first()?.toggle()          // エラー：first() が返すのは値で、場所ではない
```

`m[k, default: v]` はマップのエントリーで、エントリーがなければ `v` です。読むときの型は `V`（`V?` ではありません）で、書き込むとき（`=`、`+=`、`mut fn`、`push`）は `m[k]` に書き込みます。`v` が計算されるのはエントリーがないときだけです。数え上げやグループ分けが一行ずつで書けます。

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

#### 特定のプラットフォーム向けのコード

`@platform` は、アプリのビルド先のプラットフォームを表す `Platform` 型の値です（`.iOS`、`.android`、`.macOS`、`.windows`、`.linux`、`.web`）。これはコンパイル時に決まる定数で、追跡されません。ファイルのトップレベルやコンポーネントの本体も含め、値を書ける場所ならどこでも使えます。`@platform` で決まる分岐のうち、あるプラットフォームが通らない側は、そのプラットフォームのビルドに含まれません。コードも、文字列も、ビューも含まれません。

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

これは `if` とその `else if`、`match @platform`（`@platform` が `.unknown` になることはないので、`.unknown` を扱う必要はありません）、`@platform` の比較に対する `&&`、`||`、`!`、そしてビューの中の `if` に当てはまります。ビューの中の `if` では、もう一方の分岐のビューは作られることすらありません。ただし、どのプラットフォーム向けにビルドしても、すべての分岐がチェックされます。プラットフォームの API はどのプラットフォームでも同じ宣言なので、Linux 向けの分岐にある誤りは、iOS 向けにビルドするときもエラーになります。あるプラットフォーム専用の宣言は、専用のファイルに分けたほうがわかりやすいことがよくあります。[特定のプラットフォーム向けのファイル](https://asuka.egoist.dev/ja/docs/language/packages#特定のプラットフォーム向けのファイル)を参照してください。

`environment().platform` は、ビューが動いているプラットフォームをシステムが伝えるものです（テストでは `mount(…, platform:)` が伝えます）。ビューのために追跡されます。プラットフォームごとにどのコードをビルドするかは `@platform` で決めます。

#### 開発ビルド向けのコード

`@debug` は、アプリが開発ビルドかどうかを表す `bool` です。`asuka dev`、`asuka run`、`asuka test` では true、`asuka build` と `asuka publish` が作るものでは false になります。これも定数で、`@platform` と同じように畳み込まれます。リリースビルドが通らない側、たとえばデバッグメニュー、ステージングサーバーのアドレス、開発中だけ必要なログは、リリースビルドに含まれません。

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

`!@debug` も畳み込まれ、`@platform` の比較と `&&`、`||` で組み合わせた場合も同じです。通る側の分岐が return するなら、その後ろのコードも含まれません。`if !@debug { return cached }` と書けば、ブロックの残りはリリースビルドに入りません。`@platform` の分岐でも同じです。どのビルドでも両方の分岐がチェックされるので、開発ビルドの側で作業しているあいだに、リリースビルドの側が知らないうちに壊れることはありません。

テストは `asuka dev` と同じく開発ビルドです。`asuka test --release` を付けると、出荷するアプリと同じく `@debug` を false にしてテストを実行します。プログラムなら `asuka run --release` も同様です。ベンチマーク（`asuka test --bench`）はリリースビルドが実行するものを計測するので、`@debug` は false です。

### コンポーネント

コンポーネントは UI の部品です。パラメーターはヘッダーで宣言し、それ以外はすべて本体に書きます。

```asuka
component MessageRow(message: Message) {
  prop compact = false            // デフォルト値付きの名前付きプロパティ
  prop accessory: View? = none    // スロット：呼び出し側が渡すビュー
  event archive                   // コンポーネントが知らせるイベント
  state expanded = false          // コンポーネント自身の状態

  Menu {
    longPress: true               // コンテキストメニュー：長押しか副ボタンのクリック
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

使い方は組み込みの要素と同じです。ヘッダーのパラメーターは丸括弧の中に、それ以外はすべてブロックの中に書きます。

- プロパティ：`name: value`
- ハンドラー：`on name { … }`
- 双方向バインディング：`bind name: target`
- スタイル：`style: card`、またはリストで `style: [card, pink]`
- 子

```asuka
MessageRow(m) {
  compact: true
  accessory: Badge(3)
  on archive { mailbox.archive(m.id) }
}
```

どれも書く場所は一つだけです。ヘッダーのパラメーターをブロックの中で設定することも、プロパティを丸括弧の中で渡すこともできません。

子を受け取るコンポーネントは `prop children: View` を宣言します。呼び出し側のブロックの中身がそこに入ります。名前付きのスロットは、ブロックの中に `name { … }` と書きます（`toolbar { … }`）。

`emit name(args)` はイベントを発生させ、呼び出し側の `on name(args) { }` がそれを処理します。誰も処理しないイベントは何もしません。イベント名を値として読むと、呼び出し側がそれを処理しているかどうかになります（`if click { … }`：たとえばトレイアイコンは、呼び出し側がクリックを引き受けなければクリックでメニューを開きます）。

`await` するハンドラーは、それを宣言したコンポーネント、つまりイベントを受け取るコンポーネントに属します。誰がイベントを発生させたか、発生させた側がその後どうなったかに関係なく、終わるまで、またはそのコンポーネントがなくなるまで動き続けます。たとえばシートの `on done { shown = false; try await save(draft) }` は、シートを閉じてから保存を続けます。ハンドラーは宣言された場所で動きます。`push` はその場所のスタックに積まれ、ハンドラーが外に出したエラーはそこを囲む `Boundary` に届きます。非同期関数の中の `emit` はハンドラーが終わるのを待ち、そのエラーを受け取ります。それ以外の場所（`await` しないハンドラーや普通の `fn`）では、`emit` はハンドラーを開始して先へ進みます。待っている側をキャンセルしても止まるのは待つことだけで、ハンドラーは止まりません。

ひとつの要素だけを表示するコンポーネント（本体のビューがその要素ひとつ、またはそうなっている別のコンポーネント）は、スタイルのプロパティも受け取ります。呼び出し側が渡したスタイルはその要素に付き、コンポーネント自身がそこに設定した値より優先されます。

```asuka
component Card(title: string) {
  Column {
    padding: 16; radius: 12; background: .secondaryBackground
    Text(title)
  }
}

Card("Plan") { padding: 8; width: 240 }   // padding は 8、radius は 12
```

#### コンポーネントが宣言するバリアント

要素を一つだけ表示するコンポーネントは、自分の状態に名前を付けて、呼び出し側がスタイルを当てられるようにできます。`variant name = condition` と書くと、`let` と同じように常に最新に保たれる bool になります。呼び出し側はブロックに `name { … }` と書き、条件が成り立つあいだ、そのスタイルプロパティがその要素に重ねられます。`View` が押されているあいだ `pressed { … }` が重ねられるのと同じです。

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

- バリアントの名前は独自の名前空間にあります。ここの `checked` のように、プロパティや状態と同じ名前にできます。ただしインタラクションの状態の名前（`pressed`）やキーワードは使えません。
- バリアントは、その要素のほかのすべてのスタイルより優先されます。要素自身のスタイル、呼び出し側のスタイル、インタラクションの状態のどれよりも上です。二つが同時に成り立つときは、あとで宣言したほうが勝ちます（ここでは `disabled` が `checked` に勝ちます）。
- `pub variant` はほかのパッケージの呼び出し側もスタイルを当てられます。`pub` でないバリアントは、宣言したパッケージの中だけのものです。
- スタイルにもバリアントのブロックを書けます（`style box { checked { … } }`）。そのバリアントを宣言しているコンポーネントの要素に適用すると、ブロックはバリアントに追従します。それ以外の場所では何もしません。
- コンポーネントが表示する要素がコンテナなら、呼び出し側はコンポーネントのブロックにインタラクションの状態（`pressed { … }`、`hovered { … }`）も書けます。
- `Checkbox as box` は、表示している要素のビューに名前を付けます。`box.checked` でバリアントを読め、`card.hovered` で状態を読むのと同じく追跡されます。

#### コンポーネントの本体は宣言

コンポーネントの本体は、一度実行したら忘れられるコードでは**ありません**。一行一行が、ずっと成り立ち続けることを宣言しています。

- `state x = …` は、コンポーネントが持つ状態です。その値が変わるものを読んでいれば、状態はそれに追従します（[追従する状態](#追従する状態)を参照）。
- `let y = …` は、読んだものから導かれる値で、常に最新に保たれます（読まれたときにだけ計算され、読んだものが変わったときにだけ計算し直されます）。
- すべての要素のすべてのプロパティの値も、同じように最新に保たれます。`Text("Count: {count}")` は `count` が変わると変わり、ほかのものは何も変わりません。
- `if`、`for`、`match` は、条件が示すものを表示します。今も、これから先も。

`count` が変わったときに再び実行されるのは、それを読んでいるもの、つまりその `Text` のテキストと `if` だけです。コンポーネントが再実行されることはなく、プラットフォームのビューも、何かが変わった箇所にしか手が入りません。

#### 追従する状態

状態の値は、それが何から始まるかを表します。その値が変わるもの（パラメーター、プロパティ、ほかの状態、`let`）を読んでいれば、状態はそれに追従します。書き込まれるまではその値で、読んだものが変わると、またその値に戻ります。

```asuka
component Edit(title: string) {
  state draft = title          // 入力されるまでは title。title が変わればまた title
  state first = peek(title)    // title を一度だけ読む：初期値

  TextField { bind text: draft }
}
```

`state x(a, b) = e` は `a` と `b` だけに追従します。どちらかが変わると、`e` がほかに何を読んでいても `e` をもう一度計算します。

```asuka
let results = items.filter(i => i.name.contains(query))
state highlighted(results) = 0                  // 結果が変わったら先頭に戻る
```

状態自身の値の中では、状態の名前は現在の値をオプショナル（最初は `none`）として表します。値がまだ成り立つ間はそのまま保ち、成り立たなくなったら置き換える、という書き方ができます。この場合、状態の型は書いておきます。

```asuka
// 選択は結果の中にある間はそのまま。なくなったら最初の結果に移る。
state selected: string? = if let s = selected, results.any(r => r.id == s) { s } else { results.first()?.id }
```

一つのハンドラーの中で、状態への書き込みと、状態が追従するものの変更の両方があった場合は、後に起きたほうが勝ちます。追従するものが変わった直後に読むと、すでに新しい値になっています。

`peek(e)` は `e` そのものですが、読んでも追従しません。値が追従されるところならどこでも使えます。`Text("{count} (from {peek(count)})")` は `count` が変わると更新されますが、二つ目の読み取りは元のままです。ハンドラー、タスク、`effect(…)` の本体、テストはもともと何にも追従しないので、そこでの `peek` はエラーになります。

#### ハンドラーと文

コードが実行されるのは、ハンドラー（`on press { … }`）、関数、`task` と `effect` のブロック、そしてテストの中です。そこで状態を読むと現在の値が得られ、書き込むとそれを読んでいるものが更新されます。

```asuka
on press {
  count += 1
  todos.push(Todo(title: draft))
  todos[0].done = true      // todos[0].done への書き込み：それ（か todos[0]、todos 全体）を読むものだけが更新される
  draft = ""
}
```

ハンドラーの中での書き込みは、ハンドラーが終わったとき（または `await` に達したとき）にまとめて画面に反映されます。

文には次のものがあります：`let`、`var`、`let … else`、代入（`=`、`+= -= *= /= %=`、`&= |= ^= <<= >>=`、`&+= &-= &*=`、`??=`）、`if` と `if let`、`for x in xs`（配列かセット）、`for (k, v) in m`、`for (i, x) in xs.enumerated()`、`while` と `while let`、`match`、`return`、`break`、`continue`、`throw`、`do { } catch { }`、`defer { }`、`emit`（[制御フロー](https://asuka.egoist.dev/ja/docs/language/control-flow#制御フロー)を参照）、`fn` の宣言（[関数とクロージャ](https://asuka.egoist.dev/ja/docs/language/functions#関数とクロージャ)を参照）。

#### 双方向バインディング

コントロールは値を表示し、その値を変更します。`bind` で状態を渡すと、その両方を行えます。

```asuka
state query = ""
state notify = true

TextField { bind text: query; placeholder: "Search" }
Toggle { bind on: notify; title: "Notifications" }
```

`bind` を使わない場合、コントロールは変更を知らせるだけです（`TextField { text: name; on change(t) { name = t.trim() } }`）。

コンポーネントのプロパティも、コンポーネントが `event <prop>Change` でそれを知らせるなら、同じようにバインドできます。`prop value` と `event valueChange(value: T)` を持つコンポーネントに `bind value: when` と書くと、状態を渡し、コンポーネントが変更を発行したときに書き戻します。

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

コンポーネントは、こうしたプロパティに自分の状態のように代入することもできます。ハンドラー、fn、エフェクト、タスクの中で代入でき、要素の `bind` を通した書き込みも代入にあたります。代入の結果は呼び出し側によって変わります。

- バインドされている場合（`bind open: shown`）：代入すると `openChange` が発行され、それが呼び出し側の状態に書き込みます。プロパティはその状態を読み続けます。呼び出し側が書き込まなければ、プロパティは元のままです。
- バインドされていない場合（値を渡したか、何も渡していない）：代入した値はそれ以降コンポーネント自身のものになり、`openChange` を処理する呼び出し側には変更が通知されます。呼び出し側の値が変わると、それがコンポーネントの値を置き換えます。

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

Disclosure("Details") { Text("…") }                  // コンポーネント自身のもの
Disclosure("Details") { bind open: shown; Text("…") } // 呼び出し側のもの
```

プロパティは丸ごと変わります。`open = …` や `items = items + [x]` とは書けますが、`items.push(x)` とは書けません。対応する change イベントのないプロパティには代入できません。

#### ビューのリスト

```asuka
for todo in todos key todo.id {
  TodoRow(todo) { on delete { todos.removeWhere(t => t.id == todo.id) } }
}
```

`for` はキーごとに行を一つ保ちます。項目が変わった行は、自分のバインディングだけを更新します。新しい項目が加わると行が作られ、項目が削除されるとその行は破棄されます。移動した行は作り直されず、移動されるだけです。`List` の直下では、行は画面に表示されている間しか存在しません。そのため 100,000 件のリストでも、コストは見えている分だけです。

変わる項目を回す `for` では、`key` で何が項目を見分けるのかを書きます。書かないと、下の項目が移動しても行の状態は元の位置に残ってしまいます。いらない場合もあります。

```asuka
for i in 0..<count { Dot(i) }                      // 範囲：数がキー
for tag in tags { Chip(tag) }                      // 集合：要素がキー
for day in ["Mon", "Tue", "Wed"] { Text(day) }     // 定数：何も動かない
for (i, x) in xs.enumerated() key i { Slot(i, x) } // あえて位置で行を保つ
```

#### エフェクトとタスク

```asuka
effect { storage.set("draft", draft) }        // すぐに実行し、draft が変わるたびにまた実行する
effect(selected) { log(selected, user.plan) } // すぐに実行し、selected が変わったときだけまた実行する
task { try? await mailbox.refresh() }         // コンポーネントが表示されている間に一度だけ
task(query) { results = await search(query) } // query が変わるたびにもう一度
task(user.id) { profile = try await load(user.id) } // id が変わったときだけ。名前が変わってもやり直さない
```

エフェクトはすぐに実行され、読んだもの（呼び出した関数が読んだものも含む）が変わるたびに再び実行されます。読んだものとは、コードに書かれた部分のことです。`effect { log(user.name) }` は名前が変わると再実行されますが、アバターが変わっても再実行されません。`effect(a, b)` は `a` か `b` が変わったときに再び実行され、本体で読んだものには追従しません。`effect() { }` は何にも追従せず、コンポーネントが作られるときに 1 回だけ実行されます。タスクは非同期です（[非同期処理](https://asuka.egoist.dev/ja/docs/language/async#非同期処理)を参照）。コンポーネントが現れると始まり、丸括弧の中身が変わると前回の実行をキャンセルしてやり直し、コンポーネントがなくなるとキャンセルされます。エフェクトとタスクの丸括弧の中身は比較されます。変わっても値が等しければ何も起きないので、ユーザーの名前が変わっても `task(user.id)` はそのまま続きます。`app` ブロックにも書けます。アプリ自身のもので、アプリが動いている間ずっと有効です（更新のループや、アプリの状態に追従する設定：`effect { appearance.setColorScheme(scheme) }`）。

エフェクトは状態に書き込めますが、自分が追従している状態には書き込めません。`effect { if n > 3 { n = 3 } }` は自分の書き込みのあとにもう一度、一周遅れで実行されます。その前に、ハンドラーはもう `n` が 4 なのを見てしまっています。コンパイラーはこれをエラーにし、代わりの書き方を示します。追従する状態（`state n: int = min(n ?? 0, 3)`、`state selected(results) = 0`）、別のものに追従する `effect(…)`、あるいは読んでいるものを変えるハンドラーの中で書き込むことです。タスクは自分が追従しているものに書き込めます。キャンセルしてやり直すので、いずれ終わります。

##### クリーンアップ

```asuka
effect() {
  let id = bus.listen("refresh", e => reload())
  cleanup { bus.unlisten(id) }          // 1 回だけ：コンポーネントがなくなるとき
}
effect(roomId) {
  let room = roomId
  chat.join(room)
  cleanup { chat.leave(room) }          // 次の実行の前と、なくなるとき
}
cleanup { drafts.save(draft) }          // コンポーネントがなくなるとき
```

エフェクトの本体に書いた `cleanup { … }` は、エフェクトが次に実行される前と、エフェクトがなくなるときに実行されます。その実行で用意したものを元に戻します。コンポーネントが表示されているあいだ 1 回だけ用意するものは、`effect() { }` に書きます。そこで呼ぶ関数は、登録先（リスナーの一覧など）を読み書きしても、エフェクトに追従されません。`if` の中に書けば、その分岐でしたことだけを戻し、ループの中に書けば、たどり着いた回数だけ登録されます。複数あれば、あとに登録したものから実行されます。コンポーネント、ストア、`app` の本体に、エフェクトやタスクと並べて書くと、そのインスタンスがなくなるときに、エフェクトの cleanup のあとで実行されます。

cleanup はクロージャーです。その実行のローカル変数が見え、await も throw もできません（オーナーがなくなる途中で実行され、そのあとの cleanup も実行されなければならないからです。エラーは `try?` か `do { } catch { }` で受け止めます）。読んだものには追従しません。読む状態は、cleanup が実行されたときの値です。エフェクトが次に実行される前なら、それはエフェクトが追従しているものが変わったあとの値なので、`cleanup { chat.leave(roomId) }` は新しい部屋から抜けてしまいます。その実行の値は `let` に取っておきます。そうしないとコンパイラーが警告します（[cleanup-reads-what-it-follows](https://asuka.egoist.dev/ja/docs/errors/cleanup-reads-what-it-follows)）。cleanup が書き込んだものは、そのエフェクトの書き込みとして扱われます。

ハンドラー、関数、タスク、テストには書けません。タスクは始めたものを `defer` で戻します。`defer` はタスクが終わるかキャンセルされるときに実行されます。ハンドラーと関数には、cleanup を実行する自分のオーナーがありません。

### ストア

ストアは、状態とそれを変更する関数をまとめたもので、それを使うビューのあいだで共有されます。

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

ストアは構造体と同じように作り（`Mailbox()`）、作ったコンポーネントまたはストアのものになります。`provide` でその下にあるすべてから使えるようにし、`use` で取り出します。

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

コンポーネントが提供するものは、そのコンポーネントが表示するビューのためのものです。それぞれ `Theme` を提供する二つのコンポーネントを並べると、どちらも自分のものだけが見え、相手のものは見えません。

上のどこにも提供されていないストアを `use` するとエラーになり、提供すべきストアを示します（テストでは `mount(…, provide: [Api.fake(), Mailbox()])`。`provide` と同じく順に作られます）。オプショナル型で `use` すると、上のどこにも提供されていないときは none になります。単独でもグループの中でも動くコンポーネントに使います（チェックボックスグループの中にあってもなくてもよいチェックボックスなど）。

```asuka
store Group {
  pub state chosen: [string] = []
}

component Choice(value: string) {
  use group: Group?

  Text(if let g = group, g.chosen.contains(value) { "✓ {value}" } else { value })
}
```

ファイルの最上位に書いた `state` はアプリのもので、アプリ全体で一つだけです。

ストアは構造体と同じように型パラメータを取れます。型は引数か、期待される型から推論されます。

```asuka
store Kept<T>(key: string, initial: T) {
  pub state value: T = initial
}

let size = Kept("size", 17.0)                 // Kept<float>
let tags: Kept<[string]> = Kept("tags", [])
```

ストアの fn の中では、`self` はそのストア自身です。ストアが作るものに渡せます。

### 非同期処理

待つ必要のある処理（ネットワーク、ファイル、タイマー）は `async` で、それを待つのは `await` です。

```asuka
async fn loadThread(id: string) throws -> Thread {
  let (messages, people) = try await all(api.messages(id), api.people(id))
  let html = await spawn { renderMarkdown(messages) }
  return Thread(messages, people, html)
}
```

- 非同期関数を呼ぶには `await` が必要です。ハンドラー、タスク、テストでは `await` できますが、コンポーネントの本体とその中の `let` ではできません。クロージャは、非同期関数として渡されるか、本体で `await` していれば非同期になります。`async () => …` と書けば明示できます（[関数とクロージャ](https://asuka.egoist.dev/ja/docs/language/functions)）。
- **メインスレッド。** すべてのコードはメインスレッドで動きます。`await` で待っている間は、ほかの処理が動けます。状態を変更できるのはメインスレッドだけです。
- `all(a, b)` は複数の呼び出しを同時に実行し、すべての結果を待ちます。`race(a, b)` は最初に終わった結果を取り、残りをキャンセルします。ほかに `sleep(300ms)` と `withTimeout(5s) { … }` があります。`withTimeout` の値はブロックの値で（`let page = try await withTimeout(5s) { return try await fetch(url) }`）、時間切れになるとエラーを投げます。これらは構造化されていて、キャンセルした処理が止まり、その `defer` が実行されてから戻るかエラーを投げます。`await never()` はタスクが終わるまで待つだけです。読み手がいなくなったときにだけ終わるストリームや、何かを持つ前に await が必要なタスク（`try await register(); defer { release() }; await never()`）に使います。await せずに持てるものは、コンポーネントの `cleanup { }` で戻します（[クリーンアップ](https://asuka.egoist.dev/ja/docs/language/components#クリーンアップ)を参照）。
- `all(items, f, limit:)` は、非同期関数 `f` を `items` の各要素で同時に呼び出し、結果を要素の順に返します：`let pages = try await all(urls, u => try await http.get(u))`。`limit: 8` と書くと同時に動くのは最大 8 個までで、一つ終わるたびに次が始まります。省略する（または 0 にする）と、すべてが一度に始まります。`all(a, b)` と同じく構造化されていて、最初にエラーを投げたものが残りをキャンセルし、それ以上は始めず、それらが止まってからエラーを投げます。待っているタスクがなくなれば、すべてキャンセルされます。各要素はメインスレッド上でそれぞれ独立したタスクとして動くので、ネットワークやファイルを待ったり、状態を変えたりできます。別のスレッドで行う処理は `spawn` に任せ、`f` からそれを await します。

  ```asuka
  async fn fetchAll(urls: [string]) throws -> [string] {
    let pages = try await all(urls, u => try await http.get(u), limit: 4)
    return pages.map(p => p.text())
  }
  ```
- `spawn { … }` はブロックを別のスレッドで実行します。メインスレッドを止めてしまうような処理（パース、画像処理）に使います。中で使えるのは値だけで、状態、ファイルの var、ストア、ビュー、ネイティブのオブジェクト、C のポインター、そしてそれらを持つファイルの `let`（データベースなど）は使えません。使っていればコンパイラーが教えてくれます。Web では、ページの Web Worker の一つで実行されます。Worker はファイルの値だけを自分で作り、それ以外は作りません。呼び出しの間で何かを持ち続ける処理（インデックス、自分で開くデータベースなど）には [ワーカー](#ワーカー) を使います。
- `for await message in socket.messages { … }` はストリーム、つまり `Stream<T>` を読みます。時間とともに届く値です。`Stream(send => { … })` で作ります。ストリームが最初に読まれたときにブロックがタスクとして動き出し、`send(x)` のたびに値が一つ届きます。ブロックが戻るとストリームは終わり、ブロックが投げたエラーでストリームも失敗します。ループを抜けると（`break`、エラー、オーナーがなくなる）ブロックはキャンセルされます。ストリームの読み手は一つだけです。

  ```asuka
  fn levels() -> Stream<float> = Stream(send => {
    try await battery.watch(changed: level => send(level))
  })
  ```
- `live(initial, source)` は、ストリームが最新に保つ値で、それを読む関数として使います。バインディングの中で読むと、最初にそう読まれたときから最後の読み手がいなくなるまで `source()` のストリームに従います（そのあとストリームはキャンセルされ、誰も見ていないのにプラットフォームを監視し続けることはありません）。それ以外の場所で読むと、最後に分かっている値になります。ネットワークの状態のように、プラットフォームが変えるものをパッケージが提供するときに使います。

  ```asuka
  let level = live(1.0, levels)
  pub fn batteryLevel() -> float = level()
  ```
- **キャンセル。** タスクは、それを始めたものに属します（コンポーネントのイベントのハンドラーは、それを宣言したコンポーネントに属します。[コンポーネント](https://asuka.egoist.dev/ja/docs/language/components)を参照）。それがなくなると（ユーザーが画面を離れたときなど）、タスクはキャンセルされます。そのとき待っている `await` でタスクは終わり、`defer` が実行され、エラーは報告されません。

#### ワーカー

`spawn` はブロックを別のスレッドで一度だけ実行します。`worker` はそこに値を持ち続けます。インスタンスがそれぞれ自分のスレッドで動き、自分だけの `var` を持つ型です。

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

- ワーカーの `var`、`let`、パラメーター（`worker Index(language: string)`）はワーカー自身のものです。読み書きするのはワーカーの fn だけで、ワーカーのスレッドで行われます。`state`（ビューが読むことはありません）、props、イベント、ビューは持ちません。
- 外から呼ぶと、ワーカーの `pub fn` は非同期です（`await index.find(q)`）。引数は値として渡り、呼び出しは呼ばれた順に一つずつ実行され、結果、または `throws` の関数が投げたエラーも値として戻ってきます。ワーカーの中では普通の関数で、`await` はしません。それ以外の fn はワーカー専用です。
- 渡せるものは `spawn` と同じで、値だけです。状態、ストア、ビュー、クロージャ、ネイティブのオブジェクトは渡せません。ワーカーの fn はメインスレッドの状態も読みません。C のハンドルは自分で作って持てます（[extern type](https://asuka.egoist.dev/ja/docs/c#c-が所有する型)、開いたデータベースなど）。それがワーカーの外に出ることはなく、解放もワーカーの中で行われます。
- ワーカーは、それを作ったコンポーネントやストアに属します。それがなくなるとワーカーは止まります。実行中の呼び出しは最後まで動き、待っている呼び出しはキャンセルされます。呼び出し元のタスクには、エラーではなく自分自身のキャンセルとして見えます。オーナーのいないワーカーは、どこからも参照されなくなり、渡された呼び出しをすべて実行し終えると止まります。
- Web では、ワーカーはそれぞれ専用の Web Worker になります。

#### 読み込むデータ

`load` は非同期の結果をコンポーネントの中に保持します。

```asuka
screen Profile(id: string) {
  let user = load api.user(id)   // id が変わると実行し直す

  match user {
    .pending => Spinner()
    .failed(e) => ErrorView(e) { on retry { user.reload() } }
    .ready(u) => ProfileCard(u) { opacity: if user.loading { 0.6 } else { 1 } }
  }
}
```

`Loading<T>` は `.pending`、`.ready(value)`、`.failed(error)` のいずれかで、`.pending` になるのは最初の結果が届く前だけです。引数が読んでいるものが変わったときや、`.reload()` でもう一度実行したときは、新しい結果が届くまで今のケースのままです。`ProfileCard` は状態もスクロール位置もそのまま画面に残り、古くなりかけている実行はキャンセルされます。前回と等しい結果が届いても、何も変わりません。

- `.value` は表示しているもので、最初の結果が届くまでは none です。実行が失敗しても残るので、`.failed` ではエラーと並べて前の内容を表示できます。
- `.loading` は実行中のあいだ true になります（最初の実行も含みます）。内容を薄く表示したり、隅にインジケーターを出したりするのに使います。これを読んでも値の変化には追従せず、`match` もこれには追従しません。

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

ユーザーが入力しているあいだ、リストは新しい結果が届くまで前の結果を表示し続け、スクロール位置も各行の状態も保たれます。インジケーターは検索が実行中であることを示します。`query` を読んでいるほかの部分には、新しいテキストがすぐに表示されます。

最初からやり直したいとき（別の人のプロフィールを読み込んでいるあいだ、前の人のものを見せるべきではない場合など）は、`match` の代わりに `.loading` を読みます。

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

もっとも、多くの場合は内容がすでにそれぞれ独立しています。id ごとにプッシュされる画面や key ごとの行が、それぞれ自分の `load` を持つ形です。

##### すぐに表示する変更

チェック、名前の変更、いいね：サーバーが確認する前に画面に変更を表示し、保存に失敗したら取り消したいものです。`x.optimistic(f)` は、`x` が表示するものを `f` で変えて、すぐに表示します。

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
            try await saveDone(id, done)   // throw すると取り消される
            await reload fetchTodos()      // その答えが置き換える
          }
        }
      }
    }
  }
}
```

- 変更は load の答えの上に重なる層です。`match`、`.value`、各行にはそれが見えます。変更したパスを読むものだけが再び実行され、ほかは実行されません。上のチェックで実行し直されるのは、その行だけです。
- 層は次のうち最初に起きたときに外れます。load の次の答え（作ったときに実行中だった実行か、そのあと始まった実行のもの：`reload()`、引数が読むものの変化）が届いたとき。同じフレームで表示されるので、古い値が一瞬戻ることはありません。作ったハンドラー、`fn`、`task` から throw されたとき。取り消してから、エラーが先に進みます。そのコードが、実行中の実行も求めた実行もないまま終わったとき。最後のものは間違いで、開発ビルドが知らせます（optimistic-unconfirmed）。終わる前に load をもう一度求めます。
- 複数の変更は作った順に重なり、それぞれ独立に外れます。2 つのチェックが進行中で 1 つ目が失敗したら、取り消されるのは 1 つ目だけです。
- `f` は表示するものの関数です。その下にあるものが変わると再び実行されるので、状態を読むことも書くこともしません（[optimistic-impure](https://asuka.egoist.dev/ja/docs/errors/optimistic-impure)）。必要なものは先に `let` に読み込みます。上の `id` のように。
- `.ready` でない load には変える表示がないので、何もしません。

##### 変更が触れたものを読み込み直す

編集画面でノートの名前を変えても、その下の一覧画面には自分の load があり、`note.reload()` はそこまで届きません。`reload` は、変更が触れたものを、それを読む呼び出しで指定します。その呼び出しの load は、どこにあっても（どのコンポーネント、どのスタックのどの画面、どのウィンドウでも）すべてもう一度求めます。

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
      await reload api.list       // この画面の下にある一覧
      await reload api.note(id)   // このノートの load だけ。ほかのノートのものは含まない
      pop()
    }
  }
}

app {
  provide Api()
  Stack { Notes() }
}
```

- `reload f(args)` は、引数が `args` と等しい（依存関係の比較と同じ `==`）`f` の load に届きます。`reload f` は、引数にかかわらず `f` のすべての load に届きます。store のメソッドは、すべてのインスタンスの load に届きます。
- それぞれ `x.reload()` と同じように実行されます。表示しているものを保ち、答えが届くまで `.loading` は true です。
- `await reload …` は、届いたすべての load が答えるか失敗するまで待ちます（そのあいだに消えた load も数えます）。失敗はそれぞれの load が表示するもので、throw するのは待つことがキャンセルされたときだけです。`await` がなければ、load は実行され、コードは先に進みます。
- 呼び出しがキーです。関数か store のメソッドの load です。クロージャーの呼び出しや値のメソッドの load には、`x.reload()` があります。

##### いっしょに届く load

プロフィールとその投稿を並べて読み込むと、`id` が変わったときに答えが 1 つずつ入れ替わります。一瞬、新しい人が前の人の投稿の上に表示されます。`load (a(…), b(…))` は複数の呼び出しからなる 1 つの load で、答えはまとめて表示されます。

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
  let page = load (fetchUser(id), fetchPosts(id, page: n))   // まとめて表示

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

- これは `Loading<(A, B, …)>` で、ほかと同じように match します：`.ready((u, posts))`。
- 呼び出しはそれぞれ自分の依存関係を持ちます。上の `n` で実行されるのは `fetchPosts` だけで、ユーザーはそのままです。`id` が変わると両方が実行され、新しいユーザーは新しい投稿が届くまで保たれます。
- すべての呼び出しが一度答えるまでは `.pending`、1 つが失敗したら `.failed(e)`（ソースの順で最初のもの）で、`.value` は最後のそろったタプルを保ちます。どれかの呼び出しが実行中なら `.loading` は true です。
- `page.reload()` はすべての呼び出しを、`reload fetchPosts(id, page: n)` はその 1 つを実行します。`page.optimistic(p => (p.0, p.1 + [draft]))` はタプルを変えます。
- 要素はそれぞれ呼び出しです。値には待つものがありません（[group-not-calls](https://asuka.egoist.dev/ja/docs/errors/group-not-calls)）。

### エラー

失敗する可能性のある関数には `throws` を付け、呼び出すときには `try` が必要です。

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

let n = try? parse(text)   // エラーが投げられたら none
```

式の後ろに `catch` を書くと、代わりの値を用意できます。普段は式の値になり、式の中でエラーが投げられたときは `catch` のブロックの値（`if` と同じく、最後の式）になります。ブロックがその場を離れる場合は別です。

```asuka
let port = try parsePort(text) catch e {
  log(e.message)
  80
}
let size = try await fileSize(path) catch { return }
try save(draft) catch { failed = true }   // 文として使う：値は要らない
```

節は `do` のものと同じです（`catch`、`catch e`、`catch e: ParseError`、`catch MailError.server(code)`）。複数を続けて書くことができ、エラーを受け取った最初の節が実行されます。どの節も受け取らなかったエラーはそのまま先へ投げられるので、`try` と同じく、囲んでいる関数に `throws` が必要です。すべてのエラーを受け取る節があれば、式全体がエラーを投げなくなるので、`try` を置けない場所でも使えます。たとえばコンポーネントの `let`（`let port = try parsePort(text) catch { 80 }` は `text` が変わるたびに更新されます）や、エラーを投げてはいけないクロージャの中です。`try await` とも、値としての `throw` とも組み合わせられます（`let user = cache[id] ?? throw MailError.missing(id)`）。

エラーの `message` は、`message` フィールド、`fn message() -> string`、または型とケースの名前です。`Error("…")` はメッセージからエラーを作ります。タスクのキャンセルは `catch` では捕まりません。キャンセルはタスクの最後まで進み、その途中で `defer` を実行していきます。

`Error` は、`catch` と同じように、エラー型のケースでも照合できます：`match e { MailError.offline => …; MailError.status(code) => …; _ => e.message }`（`Error` はどんなエラーでもありうるので、`_` で終えます）。

ハンドラーやタスクで捕まえなかったエラーは、そのコンポーネントを囲むいちばん近い `Boundary` に届きます。`Boundary` がなければアプリのもの（開発中は画面に表示し、リリースビルドではログに記録します）に届きます。

```asuka
Boundary {
  Feed()
  on error(e) { Text("Something went wrong: {e.message}") }
}
```

### スタイル

すべての要素がスタイルのプロパティを受け取ります。まずレイアウト（`direction`、`justify`、`align`、`gap`、`padding`、`margin`、`width`、`height`、`grow`、`shrink`、`absolute`、`top`、`grid` など）、次に見た目（`background`、`radius`、`border`、`opacity`、`shadow`、`font`、`color`、`lines` など）です。詳しくは[レイアウトとスタイル](https://asuka.egoist.dev/ja/docs/layout)を参照してください。

`style` は再利用するプロパティをまとめて宣言します。`style:` で 1 つのスタイル、またはスタイルのリストを適用します。

```asuka
style card {
  padding: 16; radius: 12; gap: 8
  background: .secondaryBackground
}

style pink { background: #ff2d55 }

View { style: [card, pink]; opacity: 0.9 }
```

2 つの項目が同じプロパティを設定するときは、後の項目が優先されます。ブロック自身のプロパティは、`style:` をブロックのどこに書いても、リスト全体より優先されます。スタイルも同じ書き方でほかのスタイルを適用でき、自身のプロパティがそれらより優先されます。

```asuka
style largeCard { style: card; padding: 24 }
```

スタイルは `fn` と同じようにパラメーターを取れます（デフォルト値も、名前付きの引数も使えます）。パラメーターは使われる場所でバインディングになります。`chip(tint)` は `tint` に追従し、それを読むプロパティだけが更新されます。

```asuka
style chip(tint: Color, size: float = 13) {
  padding: [4, 10]; radius: 999
  background: tint.opacity(0.15); color: tint; font: Font(size: size)
}

Text(tag) { style: chip(.green) }
Text(count) { style: [card, chip(tint, size: 15)] }
```

リストの項目には `if` や `match` も書けます。選ばれた分岐のスタイルが適用され、別の分岐に移ると、それらが設定していた各プロパティは、リストでそれより前にある項目の値（なければプラットフォームのデフォルト）に戻ります。ここでの `if` には `else` が要りません。`match` はすべての場合を網羅する必要がなく、名前を束縛できません。分岐に `[]` と書くと何も適用しません。

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

スタイルに書けるのは、スタイルのプロパティ、`style:`、バリアント（後述）、`use` です。`pub style` は別のパッケージからも使えます。`style: theme.card` と書き、その値は宣言したパッケージの中で評価されます。スタイルは自分自身を適用できません。ほかのスタイルを経由する場合も同じです。

スタイルを何も指定しない要素は、プラットフォーム純正のものと同じ見た目になります。

**テーマ。** スタイルの値はそのパッケージの `let` や `state` を読めるので、デザイントークンはそのどちらかになります（`pub let hairline = 2.0`）。`state` なら、変えると読んでいるすべての場所が変わります。スタイルは上位のコンポーネントが提供するストアを `use` することもでき、スタイルが適用される場所で探されます。アプリの一部が独自のものを提供すれば、そこには独自のトークンが使われます。

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
  provide Theme(hairline: 1)   // この中では細く
  Column { style: framed }
}
```

**デフォルトのスタイル。** 要素（またはスタイルのプロパティを受け取るコンポーネント）の名前を付けたスタイルは、そのデフォルトのスタイルになります。パッケージの中で書かれたその種類のすべての要素に、`style:` のリストの下で適用されます。ほかのパッケージの要素や、そのコンポーネントの中の要素は、元の見た目のままです。

```asuka
style Button { radius: 8 }
style View { borderColor: .separator }
```

コンテナ（`View`、`Row`、`Column`、`SafeArea`）には**バリアント**を書けます。インタラクションの状態の名前を付けたスタイルのプロパティのまとまりで、コンテナがその状態にある間、元のスタイルの上に重ねられます。CSS の `:hover` や `:active` にあたるものです。状態は `hovered`、`pressed`、`dragging`（ドラッグされている間）、`dragOver`（受け付けるドラッグが上にある間）の 4 つで、この順に重なるため、`pressed` は `hovered` より優先されます。バリアントが変えるのは書いたプロパティだけで、その値はほかのプロパティと同じくバインディングです。

```asuka
View {
  padding: 16; radius: 12; background: .secondaryBackground
  hovered { borderColor: .accent }
  pressed { background: .separator; opacity: 0.8 }
  on press { open() }
}
```

ビューが状態を報告するのは、どこかでその状態が使われたときだけです。そのため `pressed` バリアントを持たないビューは、プラットフォーム自身の押下時の見た目をそのまま保ちます。バリアントの中でも `style:` を使えます（`pressed { style: raised }`）。バリアント自身のプロパティがそれより優先されます。スタイルにもバリアントを書けて、コンテナに適用されたときに働きます（ほかの要素では無視されます）。

```asuka
style button {
  padding: [8, 16]; radius: 8; background: .accent
  pressed { opacity: 0.7 }
}
```

コンポーネントは独自のバリアントを宣言することもでき（`variant checked = checked`。[コンポーネント](https://asuka.egoist.dev/ja/docs/language/components#コンポーネントが宣言するバリアント)を参照）、スタイルにはインタラクションの状態と同じようにそのブロックを書けます。そのバリアントを宣言しているコンポーネントの要素に適用すると、バリアントが成り立つあいだブロックが重ねられ、インタラクションの状態より優先されます。ほかの要素では何もしません。名前を書き間違えるとエラーになります。スタイルに書けるのは、自分のパッケージのコンポーネントのバリアントと、ほかのパッケージの `pub` なバリアントだけです。

```asuka
// selected バリアントを宣言するコンポーネント（リストボックスの選択肢など）向け。
style option {
  padding: [6, 10]; radius: 6
  pressed { opacity: 0.8 }
  selected { background: .accent; color: .onAccent }
}
```

コンポーネントのバリアントは、呼び出し側のプロパティより優先されます。呼び出し側の `background` はコンポーネント自身のものを置き換えますが、コンポーネントの `pressed` は変わらずそれを暗くします。ほかのビューは名前を通してビューの状態に追従できます（[ビューに名前を付ける](#ビューに名前を付ける)を参照）。`View as card { … }` と書き、任意のバインディングで `card.hovered`（あるいは `card.pressed`、`card.dragging`、`card.dragOver`）を読みます。子ビュー（グループ全体のホバー）でも兄弟のビューでも使えます。スタイル以外の反応にはハンドラーを使います：`on pressedChange(p)`、`on hoverChange(h)`。

#### ビューに名前を付ける

ビューには、値を設定するのではなく、ビューに対して何かをしたいこともあります。動画を再生する、ページでスクリプトを実行する、テキストフィールドに文字を入れる、リストをスクロールする、といった操作です。`Ref` はビューのハンドルです。任意の要素に `ref: r` と書くとそのビューが `r` に入り、`Element as name` と書くと、そのブロックの残りの部分で使えるハンドルが宣言されます（要素を一つだけ表示するコンポーネントに書くと、その要素のビューを指します）。

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

Ref がビューを持つのは、ビューが表示されている間だけです。その前後は空なので、コマンドは何もせず、状態はすべて false になります。状態は追跡されるので、それを読むバインディングは状態の変化に追従します。

| 対象 | Ref のメンバー |
|---|---|
| すべてのビュー | `hovered`、`pressed`、`dragging`、`dragOver`（bool、追跡される）、`focus()`、`blur()` |
| コンポーネント（`Checkbox as box`） | そのバリアント：`box.checked`（bool、追跡される） |
| `Scroll`、`List` | `scrollTo(x, y, animated: true)` |
| `Video` | `play()`、`pause()`、`seek(seconds)` |
| `WebView` | `try await eval(script)`（スクリプトの値を JSON で）、`run(script)`（完了を待たない）、`goBack()`、`goForward()`、`reload()`、`stopLoading()` |
| `TextField` | `insert(text)`、`format(.bold, on)`、`toggleFormat(.italic)`、`setLink(url)`（リッチなフィールドの選択範囲に対して） |

Ref はほかへ渡せます（`device.shareFiles(paths, from: button)`、`accessibility.focus(field)`）。ファイルの `let` に置けば、複数のコンポーネントのビューで使えます。

### アニメーション

`animate { … }` はブロックをすぐに実行し、その中でフレームや描画される内容（不透明度、背景、角の半径、シェイプのトリム）に加えた変更をネイティブにアニメーションさせます。`animate(.spring) { … }` は `Animation` を受け取ります。長さ、カーブ、遅延、繰り返し回数、スプリングの減衰と初速です。`await` すると、アニメーションが完了してから戻ります。`await` しなくてもかまいません。

```asuka
on press {
  animate { expanded = !expanded }
}
on press {
  await animate(Animation(duration: 500ms, curve: .spring)) { offset = 0 }
  done = true
}
```

`animate(() => …)` と `animate(anim, () => …)` は、ブロックの代わりにクロージャを渡す同じ書き方です。`repeat: Animation.forever` は、プロパティが再び変わるまで繰り返します。`animating()` は、いま加えている変更がアニメーションするかどうかを返します。`onFrame(t => …)` は、ディスプレイがフレームを描くたびにそのフレームの時刻を渡してクロージャを呼びます。戻り値のクロージャを呼ぶか、コンポーネントがなくなると止まります：`let stop = onFrame(t => tick(t))`。`frames()` は同じフレームを `Stream<Duration>` として返し、読まれている間だけフレームを描かせます。タスクの中で `for await t in frames() { … }` と書けば、ループが終わるかタスクがなくなったときに止まります。

### 描画

`Canvas(g => …)` は `Graphics` を使って、左上を原点にポイント単位で描画します。レイアウトが済むと描画し、サイズや読み取った内容が変わると描き直します。`Shape(fn(width, height) -> Path)` は、プラットフォームが塗りと線を描く輪郭です（`fill`、`stroke`、`lineWidth`、`trimEnd`、`dash` など）。

```asuka
Canvas(g => {
  let ring = g.path().arc(g.width / 2, g.height / 2, 40, 0, 6.28318)
  g.stroke(ring, .accent, StrokeStyle(width: 8, cap: .round))
  g.text("{percent}%", g.width / 2, g.height / 2, CanvasText(size: 20, align: .center))
}) { height: 100 }
```

`Path` はメソッドで組み立てます。どのメソッドもパス自身を返します：`Path().move(x, y)`、`line`、`quad`、`cubic`、`rect`、`roundedRect`、`ellipse`、`arc(cx, cy, radius, start, end, clockwise: true)`（ラジアン、x 軸の正の向きから）、`close()`。`g.path()` はキャンバス自身が持つ空のパスで、描画のたびに使い回されます。`Graphics` が有効なのは描画関数の実行中だけです：`width`、`height`、`fill(path, color)`（`LinearGradient` や `RadialGradient` も渡せます）、`stroke(path, color またはグラデーション, style)`、`save()`、`restore()`、`translate`、`scale`、`rotate(degrees)`、`opacity`、`clip(path)`、`transform(a, b, c, d, tx, ty)`、`evenOdd(on)`、`text(s, x, y, style)`。

`shader` 宣言は、GPU のプログラムが描く箱です。Asuka の構文で書き、アプリと一緒にコンパイルされ（間違いはその行を示すコンパイルエラーになります）、ビューとして使います。引数がそのまま uniform になります。

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

`fn fragment(pos: vec2) -> vec4` はピクセルの色（プリマルチプライド）を返します。`size`、`scale`、`time` はそれぞれ枠のサイズ（ポイント）、1 ポイントあたりのピクセル数、表示されてからの秒数です（`animated: true` で進みます）。引数は `float`、`int`、`bool`、`Color` のいずれかです。シェーダーを書く言語とプラットフォームごとの動き方は[シェーダー](https://asuka.egoist.dev/ja/docs/components#シェーダー)を参照してください。

### システムの機能

アプリの一部として働くためにコンポーネントが使うものは、プレリュードにあり、インポートは不要です。コンポーネントがある間に登録するもの、環境、アラート、アプリのウィンドウです。

| 関数 | 動作 |
|---|---|
| `useKeys([Shortcut(…)])` | コンポーネントがある間の、ウィンドウのキー操作 |
| `onOpenURL(u => …)`、`onOpenFiles(paths => …)`、`onShare(items => …)` | コンポーネントがある間に、アプリが開かれたときに渡されたもの、またはほかのアプリから共有されたもの |
| `quit()`、`relaunch()` | ユーザーが「終了」を選んだときと同じようにアプリを終了し（再び起動し）ます。先にアプリに確認します。デスクトップのみ |
| `onCloseRequest(fn() -> bool)`、`onQuit(e => …)`、`onSecondInstance((args, dir) => …)` | コンポーネントがあるあいだ：そのウィンドウが閉じる前の確認、アプリの終了の直前（`e.cancel()`）、アプリの再起動を受け取ります |
| `onBack { … }` | コンポーネントがある画面、シート、ポップオーバーからユーザーが戻ろうとしたときに問い合わせられます。`true` なら引き受け（画面は残る）、`false` なら戻らせます |
| `onLifecycle(l => …)` | コンポーネントがある間、アプリがフォアグラウンドやバックグラウンドに移ったこと、メモリ警告（`Lifecycle`）を受け取ります |
| `alert(title, message:, actions:)` | その場でアラートを表示します。`actions` は `.button` の配列です（なければ OK だけ）。`Alert` 要素は、`open` が true の間アラートを表示します |
| `snackbar(message, action:, duration:, onDismiss:)` | ウィンドウの下部に短いメッセージを、アクション（元に戻す）付きで表示します。閉じるためのクロージャーを返します |
| `environment()`、`screenEnvironment()` | サイズ、セーフエリア、文字サイズ、プラットフォームなど（追跡されます）。画面やシート自身のものと、画面全体のもの |
| `window.main()`、`window.current()` | メインウィンドウの `Ref` と、コードがあるウィンドウの `Ref`。どこからでも `show()`、`hide()`、`focus()` などができます |
| `window.close()` | コードがあるウィンドウを確認せずに閉じます |

システムのサービスに頼むことは[プラットフォーム SDK](https://asuka.egoist.dev/ja/docs/stdlib) のパッケージにあり、ほかのパッケージと同じようにインポートして使います。

| 関数 | 動作 |
|---|---|
| `device.share(text)` | テキストやリンクの共有シートを表示する |
| `try await device.shareFiles(paths, title:, from: ref)` | ファイルの共有シートを、あるビューから（ポップオーバーが指す位置に）表示する。シートが閉じると戻る |
| `device.haptic(.success)` | プラットフォームの触覚フィードバック（`device.Haptic`） |
| `device.copy(text)` | テキストをクリップボードに入れる |
| `device.openURL(url)` | url を扱えるアプリで開く |
| `accessibility.announce(text)` | スクリーンリーダーにテキストを読み上げさせる |
| `accessibility.focus(ref)` | スクリーンリーダーのフォーカスをそのビューに移す |
| `try await webview.cookies(url)` | Web ビューが url について保持している Cookie |
| `appearance.setColorScheme(.dark)` | アプリの外観：`.light`、`.dark`、`.system`（プラットフォームのもの） |

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

アプリが各プラットフォームでどのコードを持つかは、コンパイル時に決まる定数 `@platform` で決めます（[特定のプラットフォーム向けのコード](https://asuka.egoist.dev/ja/docs/language/control-flow#特定のプラットフォーム向けのコード)）。`environment().platform` はビューが実際に動いているプラットフォームで、環境のほかの値と同じように追跡されます。

`useKeys` とこれらの `on…` による登録は、コンポーネントの本体（またはアプリのブロック）に書きます。アプリのブロックでは、これらをイベントとしても扱えます：`on openURL(url) { … }`、`on files(paths) { … }`、`on share(items) { … }`、`on quit(e) { … }`、`on secondInstance(args, dir) { … }`、`on foreground { … }`、`on background { … }`。ブロックの最初の `Window` がメインウィンドウです。`Window` の外に何も表示しない（トレイアイコン、メニューバーだけの）app ブロックには、メインウィンドウがありません（[ナビゲーション](https://asuka.egoist.dev/ja/docs/navigation#ウィンドウ)）。

`std/embed` は、アプリのコンパイル時にパッケージのファイルを読んでアプリに組み込みます。どのプラットフォームでも、実行時に読み込むことなくそのファイルを使えます。

```asuka
import std/embed

struct Config { api: string; retries: int }

let welcome = embed.text("welcome.md")         // string
let logo = embed.bytes("art/logo.png")         // bytes
let config: Config = embed.json("config.json") // コンパイル時に検査される
let posts = embed.texts("posts/*.md")          // [パス: テキスト]
let icons = embed.files("icons/**/*.svg")      // [パス: バイト列]
```

パスは文字列リテラルで、パッケージのディレクトリ（`asuka.toml` のある場所）からの相対パスです。その外は指せません。グロブには `*`、`?`、`[abc]`、`**`（任意の階層のディレクトリ）が使え、名前がドットで始まるファイルには一致しません。`embed.json` は `json.decode` と同じようにファイルを読みますが、それをコンパイル時に行います。型に合わないファイルは、行と列を示すコンパイルエラーになり、実行時のエラーにはなりません。ファイルがない場合や、グロブに一致するファイルがない場合もコンパイルエラーです。`asuka dev` では、組み込んだファイルを変更するとコードを変更したときと同じようにアプリがその場で再読み込みされます。実行時にプラットフォームが読むファイル（画像、フォント）は `assets/` に置きます（[std/embed](https://asuka.egoist.dev/ja/docs/std/embed)）。

### ナビゲーション

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

`screen` は、ルートがスタックの画面になるコンポーネントです。ブロックには画面のプロパティ（`title`、`largeTitle` など）とスロット（`toolbar`、`bottomToolbar`、`primary`）を書きます。`push(…)` と `pop()` はいちばん近いスタックに、`present(…)` と `dismiss()` はシートに対して働きます。`Split` は、`Destination`（`Sidebar` のセクションに入れられます）を並べたサイドバーと、その横の選んだ行き先のスタックです。

`Router` を使うと、どの画面にもアドレスが付きます。パターン付きで宣言した画面はルートに載せられ、パラメーターには型があります。その呼び出しがアドレスになり、コンパイル時に検査されます。

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

詳しくは[ナビゲーション](https://asuka.egoist.dev/ja/docs/navigation)を参照してください。

### ほかの言語のテキスト

`tr.inboxTitle(unread: n)` はアプリのカタログ（`locales/en.json`、`de.json` など）のメッセージを返します。カタログはアプリのコンパイル時にチェックされ、キーや引数が足りなかったり型が違ったりするとエラーになります。バインディングの中で使ったメッセージはアプリの言語に従うので、言語を切り替えると（std/i18n の `i18n.setLocale("de")`）表示も変わります。右から左に書く言語では、アプリ全体が右から並びます。詳しくは[多言語対応](https://asuka.egoist.dev/ja/docs/i18n)を参照してください。

### ネイティブコード

プラットフォームにしかできないことは、ネイティブパッケージに入れます。Asuka で宣言し、各プラットフォームの言語で実装します。

```asuka
pub native interface Haptics {
  fn impact(style: Feedback)
}

pub native component MapView(region: Region) {
  prop pins: [Pin] = []
  event regionChanged(region: Region)
}
```

モジュールを作ることも呼び出すことも失敗する可能性がある（そのプラットフォームにコードがないかもしれない）ので、どちらにも `try` を付けます（`let h = try Haptics()`、`try h.impact(.light)`）。`asuka native generate` は、各プラットフォームが実装する Swift、Kotlin、JavaScript、C のコードを生成します。詳しくは[ネイティブコード](https://asuka.egoist.dev/ja/docs/native)を参照してください。

C は直接呼び出せます。`extern` 宣言では、C の関数が何を受け取り何を返すかを C の型で書きます。Asuka からは、それぞれが Asuka 自身の型に見えます（`Int32` は `int`、`CString` は `string`）。

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

`extern type` は C が渡すポインターで、最後の参照がなくなると release で解放されます。`extern struct` は値渡しで受け渡しされ、C に渡した関数はコールバックになります。アプリがコンパイルしてリンクする C は、`asuka.toml` の `[c]` に書きます。詳しくは [Asuka から C を呼び出す](https://asuka.egoist.dev/ja/docs/c)を参照してください。

### テスト

```asuka
test "archiving removes the row" {
  let app = mount(Inbox(), provide: Mailbox.fake([sample]))
  await app.idle()
  app.tap("Archive")
  app.expectNo("Hello")
}
```

`expect(cond)` は cond が false ならテストを失敗させ、何がどこで失敗したかを示します（比較なら両辺の値も示します）。JSON は `std/json` で扱います。`let notes: [Note] = try json.decode(text)` は求められている型として読み込み（`json.decode<[Note]>(text)` とも書けます）、`json.encode(notes)` は JSON に書き出します。`json.Value`（任意の JSON）が求められるところでは、JSON で表せる値は書き出したとおりの `json.Value` になります。型の混ざった値のマップは `[string: json.Value]` になり（`["port": port, "tls": true]`）、数値は数値のままです。ジェネリック関数も自分の `T` を JSON として読み書きできます。その場合、呼び出しごとに `T` は JSON で表せる型でなければならず、コンパイラーが呼び出し箇所でそれをチェックします。

`asuka test` は、パッケージのテストをデバイスなしで実行します。プラットフォームと同じようにビューをレイアウトし、テキストは 1 文字あたり固定の幅で測ります。また、時間はほかに実行できるものが何もないときにしか進みません（テストの中の `sleep(5s)` には時間がかかりません）。`mount` はアプリを返すので、それを操作して確かめます（`tap`、`longPress`、`type(_, into:)`、`submit`、`toggle`、`slide`、`select`、`swipe(_, action:)`、`refresh`、`scrollTo`、`back`、`dismiss`、`pickDate`、`pickColor`、`expect`、`expectNo`、`texts()`、`disabled(text)`、`until`、`waitFor`、`idle`）。`in(.navigationBar)`、`in(.sidebar)`、`nextTo(label)` で、テキストが指すものを絞り込めます。ルーターがあれば、`mount(…, at: "/inbox/1")`、`href`、`history()`、`goBack()`、`goForward()` も使えます。詳しくは[テスト](https://asuka.egoist.dev/ja/docs/testing)を参照してください。

テストはどのファイルにも書けますが、パッケージの**テストファイル**にも書けます。ファイル名が `_test.asuka` で終わるものです（`parser_test.asuka`。1 つのプラットフォーム用なら `tray_linux_test.asuka`）。テストファイルの宣言とインポート（テスト用のデータやヘルパー）は、そのパッケージ自身のテストを実行するとき（`asuka test`、`asuka check`、エディター）にだけパッケージの一部になります。ビルドにはそれらもテストブロックも含まれず、このパッケージをインポートするほかのパッケージからも見えません。そのため、テストにしか要らないパッケージをテストファイルでインポートしても、アプリには入りません。

パラメーターのあるテストはプロパティテストで、`asuka test` がパラメーターの値を生成して実行します。失敗した入力は、まだ失敗するいちばん単純なものに縮小されます。`bench "name" { … }` はベンチマークで、`asuka test --bench` が計測します。その中の `measure { … }` が計測される部分で、準備のあとに書きます。`keep(x)` は、結果が最適化で省かれないようにします。`expectSnapshot(value)` と `expectInlineSnapshot(value, "…")` は、値を出力したものをスナップショットと比べます。スナップショットは `asuka test` が書き込みます。

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

## 診断コード

- argument-count: 呼び出しの引数が多すぎる、または少なすぎる. 関数の宣言どおりに渡します。
- argument-label: 関数に対応するパラメーターのない名前付き引数. パラメーターの名前を使う（メッセージが提案します）か、順番に渡します。
- async-in-effect: エフェクトの中にある `await`. `task(what) { … }` を使います。
- await-outside-async: 待つことのできない場所にある `await`. 関数に `async` を付けます。
- bad-operand: 演算子が受け付けない値に使われている. 片方を変換する（`float(n)`）か、メソッドを使う（`a.concat(b)`）か、型に演算子のメソッドを追加します。
- brace-placement: ブロックの `{` は前の行の末尾に書く. `{` を前の行の末尾に移します（`fn f() {`、`Row {`、`if x {`）。
- cannot-infer-type: 書かれた内容から値の型を判断できない. 値を置く場所で型を書きます（`let ids: [int] = []`、`Type.case`、`f<T>(…)`）。
- cleanup-reads-what-it-follows: エフェクトが追従するものを読むエフェクトの cleanup. この実行の値を `let` に取っておき、cleanup ではそれを読みます。
- closure-keeps-itself: 自分を保存した変数を通して自分を呼ぶクロージャー. 代わりにブロックの中で関数 `fn name(…) { … }` を宣言します。
- constraint-not-met: ジェネリックやメソッドが必要とするものを型が持っていない. 満たす型を使う（値全体ではなく id を使う）か、足りないものを型に与えます。
- deprecated-syntax: 置き換えられた古い書き方（`asuka fmt` が書き直す）. `asuka fmt -w` を実行するか、`style: name` と書きます。
- duplicate-declaration: 同じ場所で 2 回宣言された名前. どちらかの名前を変えるか、不要なほうを削除します。
- effect-in-effect-pass: エフェクトの書き込みで、1 回のフラッシュの中でバインディングが 2 回実行された. 値を `let` か、エフェクトが読むものに追従する状態で計算します。
- effect-loop: 1 回のフラッシュの中で互いを汚し続けるエフェクト. 値の出どころを 1 つにします。
- effect-writes-what-it-follows: 自分が読む状態に書き込むエフェクト. 宣言の時点で範囲に収める（`state n: int = min(n ?? 0, 3)`）か、別のものに追従する（`effect(other) { … }`、`state x(other) = …`）か、読むものを変えるハンドラーの中で書き込みます。
- embed-file: パッケージにないファイルの `embed`. パスを直すか、ファイルをパッケージに追加します。
- extension-conflict: 型がすでに持つ名前、またはほかの拡張も追加する名前を追加する拡張. メソッドの名前を変えるか、拡張を 1 つだけ残します。
- fn-body: 本体のない関数、または持てない本体を持つ関数. 本体（`{ … }` か `= 式`）を書くか、native や extern の宣言から外します。
- follows-nothing: 追従するもののない `state x()`. 追従するものを書く（`state x(a) = …`）か、`state x = peek(…)` と書きます。
- format-spec: 値に合わない補間の書式. メッセージのとおり、値の型に合った書式を書きます：`{n:>6}`、`{x:.2f}`、`{n:#x}`、`{name:<12}`。
- group-not-calls: `load (…)` の要素が、どれも呼び出しというわけではない. 呼び出しだけをまとめ（`load (api.user(id), api.posts(id))`）、値はそれらの呼び出しに渡すか、load の横に置きます。
- import-cycle: 互いにインポートし合うパッケージ. 両方が必要とするものを 3 つ目のパッケージに移し、両方からそれをインポートします。
- indentation: 行のインデントが開いているどのブロックにも合わない、またはインデントで書いたブロックの中でタブを含む. その行を属するブロックにそろえます（そのブロックの前の行と同じインデントにします）。
- inline-snapshot: 呼び出しに直接書かれた文字列ではないインラインスナップショット. スナップショットを文字列リテラルとして書くか、省略して（`expectInlineSnapshot(value)`）最初の実行で `asuka test` に書かせます。
- int-overflow: 型の範囲に収まらない整数リテラル. 範囲内の数にするか、より広い型（`u16`、`int`、`u64`）や `float` を使います。
- interface-not-satisfied: 宣言した interface を実装していない型. 足りないメソッド（メッセージに一覧があります）を interface のシグネチャーで追加します。
- internal-error: コンパイラー自身の誤り. https://github.com/egoist/asuka/issues に報告してください（プログラム、またはそれを再現する最小のプログラムを添えて）。
- invalid-catalog: メッセージのファイルとして正しくないカタログ. メッセージが指す場所でファイルを直します。
- invalid-catch: 到達しない、または捕捉するものがない `catch`. 具体的な catch を先に、すべてを捕捉する catch を最後に置き、throw しうるものだけを捕捉します。
- invalid-character: ここでは言語の一部にならない文字. その文字を削除するか、テキストを文字列やコメントの中に入れます。
- invalid-conformance: 型が宣言する、または型パラメーターが求めるものが interface ではない. そこに interface を書くか、外します。
- invalid-constant: コンパイル時に作られる値が、リテラルでないか解析できない. リテラルそのものを渡し、解析できないとメッセージが示す部分を直します。
- invalid-declaration: 置かれた場所が受け付けない形で書かれた宣言. その場所の形で書きます。
- invalid-entry: 実行のされ方と違う形で宣言されたプログラムの `fn main`. `fn main() { … }` と宣言し、引数は std/os で読みます。
- invalid-extension: 拡張できないものの `extend`、または受け付けない形の `extend`. 名前のある型を拡張するか、代わりに関数を宣言します。
- invalid-extern: C が裏付けられない extern 宣言. 解放する関数を `extern fn close(x: T)` と宣言し、ほかは C のとおりに宣言します。
- invalid-flag-spec: `flag.parse` がコマンドラインを読み込めない型. スイッチには既定値を付け（`verbose: bool = false`）、オプションはメッセージが挙げる型にします。
- invalid-for: `for` がたどれないものをたどっている. メッセージのとおりのものをたどります（`0..<n`、`m.keys()`、`for await x in stream`）。
- invalid-literal: 正しく書かれていない数値、色、時間、エスケープ. メッセージのとおり、決まった形のどれかで書きます。
- invalid-modifier: 当てはまらない場所にある `pub`、`mut`、`async`、`throws`、型パラメーター. メッセージが示す修飾子を外します。
- invalid-pattern: 言語が受け付けない形で書かれたパターン. メッセージが示す形で書きます（`.case(x)`、`if let`、`(k, v)`）。
- invalid-project: プロジェクトの asuka.toml やディレクトリを、書いてあるとおりに読めない. メッセージのとおりに asuka.toml の値を直します。
- invalid-ref: 1 つの要素のビューではないものを指す `ref` や `as`. コンポーネントの中の要素に名前を付けるか、コンポーネントが表示する要素を 1 つにします。
- invalid-route: 画面のアドレス、またはルートでは作れないナビゲーション. 画面のパラメーターを、宣言のとおりにパターンに書きます。
- invalid-shader: GPU の言語にないものを使ったシェーダー. 数値、ベクトル、GLSL の関数で書きます。
- invalid-style: 受け付けない形で宣言・適用されたスタイル. スタイルを 1 つのリストにまとめます（`style: [a, b]`）。
- invalid-variant: 成り立たない形で宣言・使用されたバリアント. 要素を 1 つだけ表示し（ビューを `Row` や `Column` に入れる）、バリアントにはそれに合わせて変わるものだけを書きます。
- let-else: `let … else` の else がそのまま先へ進む、またはアンラップするものがない. `else` を `return`（または `throw`、`break`、`continue`）で終えるか、外します。
- load-outside-component: コンポーネントやストアの `let` の外にある `load`. コンポーネントで `let x = load f(…)` と宣言するか、関数の中で呼び出しを `await` します。
- load-waterfall: 使ってもいない別の load を待った load. 同じコンポーネントで 2 つの load を並べて始め、結果を下に渡します。
- local-fn: ブロック内の関数が、ファイルの関数にしかないものを使っている. それらが必要なら、ファイルのトップレベルで宣言します。
- long-hold: load が数秒間、何も示さずに古いコンテンツを保っている. 保っているあいだ `x.loading` を読む（コンテンツを薄くする、リングを表示する）か、その場所にスピナーを表示します。
- loop-label: 使われていない、または存在しないループ名. 名前を外すか、使います（`continue outer`）。
- maybe-none: `none` かもしれない値を、あるものとして使っている. `?.` を使う（`p?.name`）か、`??` で既定値を与えるか、先にアンラップします（`if let p = p { … }`）。
- message-arguments: プレースホルダーと違う引数で呼び出されたメッセージ. 各プレースホルダーを名前で渡します（`tr.greeting(name: n)`）。
- misplaced-declaration: その種類の宣言が置けない場所にある宣言. その種類が置ける場所に移すか、この場所で使える書き方にします（ワーカーでは `state` の代わりに `var`）。
- misplaced-statement: 書いた内容が意味をなさない場所にある文. 意味のある場所に文を移すか、ここで本当にしたいことを書きます。
- missing-await: async 関数を `await` なしで呼び出している. `await f(…)` と書く（throw するなら `try await`）か、`spawn` で単独で開始します。
- missing-prop: 必要なプロパティのない要素. 要素のブロックで設定する（`Badge { count: 3 }`）か、prop に既定値を与えます。
- missing-return: 結果を返す関数が、結果なしで最後まで到達しうる. すべての経路で値を返すか、関数を `return` で終えます。
- missing-translation: ソース言語のメッセージの一部がない言語のカタログ. 一覧にあるメッセージを、その言語のカタログで翻訳します。
- missing-try: throw する関数を `try` なしで呼び出している. `try f(…)` と書きます。
- missing-type: 型を書かなければならない宣言. 型を書きます（`prop size: int`、`prop size = 12`、`state n: int = …`）。
- mixed-numbers: 型の違う数値に演算子を使っている. 片方をもう片方の型に変換します：`int(level) + offset`、`float(n) * 0.5`、`u8(n)`（n が収まらなければエラーを投げます。
- native-type: プラットフォームのビューが受け取れない型のネイティブコンポーネントのプロパティやイベント. プラットフォームのコードが読めるデータを使い、起きたことはイベントで伝えます。
- no-children: 子を取らない要素に子を渡している、または渡す子が違う. ビューをコンテナー（`Column`、`Row`）に入れるか、要素の隣に置きます。
- no-such-operator: ほかの言語にはあって Asuka にはない演算子. `x += 1`、`if a { b } else { c }`、`a < b && b < c` と書きます。
- no-value: 値として使われているが、値を生まないもの. `if` に `else` を付けるか、ブロックを値で終えるか、値を返す関数を使います。
- non-exhaustive-match: すべての場合を網羅していない `match`. 足りない分岐を追加するか、残りを `_ => …` で受けます。
- not-a-type: 型として使われているが、型ではないもの. 意図した型を書きます。
- not-a-value: 値が入る場所で使われた型、パッケージ、メソッド. その型の値を作る（`Point(x: 1, y: 2)`）か、パッケージのメンバーを使うか、メソッドを呼び出します。
- not-a-view: ビューのあいだにある、ビューではない値. 要素の中で表示します（`Text("{n}")`）。
- not-an-error: エラー型ではない値を throw / catch している. 型を `: Error` と宣言するか、`Error("…")` を投げます。
- not-assignable: 変えられないものを変えている. `var`（または `state`）にするか、`let` が読むものを変えるか、メソッドを `mut fn` と宣言します。
- not-async-call: `load`、`all`、`race` に async 呼び出しではないものを渡している. async 呼び出しを渡す（`load api.user(id)`）か、値をそのまま使います。
- not-bindable: 変化を伝えないプロパティの `bind`. コントロールの値をバインドするか、値を渡して（`text: s`）イベントを処理します。
- not-c-type: C に渡せない型. `bytes` か `Pointer` と長さを渡すか、`extern struct` を宣言します。
- not-callable: 関数ではないものの呼び出し. 関数を呼び出すか、値をそのまま使うか、列挙型の case を作ります（`Kind.name`）。
- not-json: JSON で読み書きできない型. データだけの struct にデコードし、ほかのものはそこから作ります。
- not-optional: 必ず値がある値に、オプショナル用の演算子を使っている. 演算子を外すか、宣言でその値をオプショナル（`int?`）にします。
- not-pub: ほかのパッケージ、ほかのファイル（private）、ストア、ワーカーが公開していない宣言. 宣言に `pub` を付ける（または `private` を外す）か、公開されているものを使います。
- not-sendable: 別のスレッドに渡せない値. それを持つオブジェクトではなく、相手のスレッドが必要とするデータを渡します。
- not-supported: 言語がまだ対応していないもの. メッセージが示す代わりの書き方を使います。
- optimistic-impure: 状態を読む、await する、または書き込む楽観的な変更. 必要なものは先に `let` に読み込み、それをクロージャで使います。
- optimistic-not-loading: load でないものに `optimistic` を呼んだ. 状態そのものを変える（`notes.push(n)`）か、load の `let` に `optimistic` を呼びます。
- optimistic-not-ready: 準備ができていない load に `optimistic` を呼んだ. `.ready` になってから表示するものを変えます。
- optimistic-unconfirmed: 楽観的な変更が、load の確認する実行がないままタスクとともに外れた. タスクが終わる前に load をもう一度求めます：`await reload f(…)`、または `x.reload()`。
- pattern-type: 値の型に一致しえないパターン. 値と同じ型のパターンを書きます。
- peek-untracked: 何も追従しない場所での `peek`. 値そのものを読みます。
- plural-categories: 複数形や select のカテゴリーが言語のものと合わないメッセージ. メッセージが挙げるとおり、その言語のカテゴリーを書きます。
- prop-not-assignable: コンポーネントが代入できない prop に代入している. `event openChange(open: bool)` を宣言してから代入するか、コンポーネント自身の状態を持ちます。
- prop-or-argument: パラメーター、プロパティ、イベント、子が置き場所を間違えている. それぞれの場所に移します（`Badge(3) { compact: true; on tap { … } }`）。
- recursive-definition: 自分自身で定義された値やスタイル. 循環を断ちます。
- recursive-type: 自分自身の完全なコピーを含む型. 途中のフィールドをオプショナル（`next: Node?`）か配列（`children: [Node]`）にします。
- reload-arguments: `f` が受け取らない引数で `reload f(…)` した. `f` が受け取る引数を、load が呼ぶのと同じ形で渡すか、何も渡しません。
- reload-not-loaded: どの load も呼んでいないものを `reload` した. `load` が呼ぶ関数を reload する（`reload api.notes`）か、load をハンドルで読み込み直します（`x.reload()`）。
- reserved-name: 言語自身が使っている名前. 別の名前にします。
- return-value: 関数が返さない値を持つ `return`、または値が必要なのに値のない `return`. 値を外すか、結果の型（`-> T`）を宣言してすべての場所で値を返します。
- row-without-key: 変わるデータをたどる、`key` のないビューの `for`. 項目を識別するものを書きます（`for t in todos key t.id`）。
- spawn-captures-state: 状態や値でないものを読み書きする `spawn`. 先に状態を `let` に読み込んでから渡し、`spawn` が返したものを代入します。
- special-method: 言語が呼ぶメソッド（equals、hash、description、toJSON、fromJSON）が、言語の呼べないシグネチャーになっている. メッセージが示すシグネチャーにするか、その用途でなければ別の名前にします。
- statement-in-view: コンポーネントのビューのあいだにある文. 文はハンドラー（`on press { … }`）、fn、タスク、エフェクトに書きます。
- syntax-error: パーサーがここで別のものを期待している. メッセージの「expected …, found …」を読み、期待されているものを書くか、指摘された位置で開いているものを閉じます。
- test-parameter: asuka test が生成できない型のテストのパラメーター. テストが扱うものをデータ（ID、フィールドからなる struct）として受け取り、残りはテストの本体でそこから作ります。
- too-large: バイトコードに収まらないほど大きい関数. 関数を分けるか、データをリテラルではなくファイル（`embed.json`）に置きます。
- tr-in-package: アプリではないパッケージでの `tr`. テキストをパラメーターとして受け取り、アプリから `tr.key()` を渡してもらいます。
- type-arguments: 型引数の数が違う. 宣言のとおりの数だけ型を渡します（`Set<int>`、`Page<Item>`）。
- type-mismatch: 期待とは違う型の値. 期待される型の値を渡すか、変換します（`float(n)`、`int(x)`、`"{n}"`）。
- undefined-name: 使われている場所で何も宣言していない名前. 綴りを直す（近い名前をメッセージが提案します）か、宣言しているパッケージをインポートするか、宣言します。
- unexpected-block: ブロックを取らない呼び出しのあとにあるブロック. 値を括弧の中で渡し、ブロックを外します。
- unhandled-error: 受け取り手のいない場所からエラーが出ていきうる. 関数に `throws` を付けるか、エラーを捕捉します（`try … catch { fallback }`、`do { } catch { }`、`try?`）。
- unknown-event: コンポーネントが宣言していないイベントのハンドラーや `emit`. 既存のイベントを使う（メッセージが提案します）か、宣言します（`event name(…)`）。
- unknown-member: 型、列挙型、パッケージにその名前のメンバーがない. 既存のメンバーを使う（メッセージが提案します）か、型にそのメンバーを追加します。
- unknown-message: カタログにないメッセージの `tr.key`. カタログにあるキーを使う（メッセージが提案します）か、カタログに追加します。
- unknown-package: インポートがどのパッケージも指していない. パスを直すか、依存関係を追加する（`asuka add`）か、パッケージの新しい名前を使います。
- unknown-prop: 要素にないプロパティ、スロット、バリアント. 既存のプロパティを使います（メッセージが提案します）。
- unterminated: 文字列、コメント、エスケープが閉じていない. 閉じてください。
- unused-catalog-entry: 何にも使われていないカタログの説明や引数. 削除するか、メッセージの中で引数を使います。
- view-in-statement: 実行されるコードの中で作られたビュー. ビューを本体で宣言し、ハンドラーが設定する状態の `if` の下に置きます。
- wide-fanout: 1 回の書き込みが 1,000 を超えるバインディングと let を、2 回以上実行した. 行が表示するものをパスで読む（`todos[i].done`、`for t in todos key t.id`）か、セットの `contains` を使い、書き込みがそれを読むものだけを実行するようにします。
- worker-captures-state: メインスレッドの状態を読むワーカー. メインスレッドで状態（または var）を読み、その値を `pub fn` に渡します。
- worker-misuse: ワーカー自身の値を外から読んでいる、または作れない・呼べない場所でワーカーを作ったり呼んだりしている. 値を `pub fn` から返し、`await` で呼び出します。
- write-in-binding: UI が追従する場所で状態に書き込んでいる. 状態にはハンドラー（`on press { … }`）、fn、タスク、エフェクトの中で書き込みます。
- write-in-memo: `let` の計算中に状態に書き込んでいる. 状態にはハンドラー、fn、タスク、エフェクトで書き込みます。
