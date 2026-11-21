# レイアウトとスタイル

要素のスタイルのプロパティは、その要素をどう配置し、どう見せるかを指定します。レイアウトは CSS の flexbox と grid で、単位はポイントです。見た目は色、角丸、枠線、影、テキストで決まります。このページでは、この両方と、変化するスタイルについて説明します。レイアウトはどのプラットフォームでも同じです。Asuka が計算し、その結果の位置にプラットフォームのビューを配置します。

## プラットフォームのデフォルト

設定しなかったスタイルは、プラットフォームのデフォルトのままになります。スタイルを何も設定しないアプリは、フォント、色、ダークモード、文字サイズまで含めて、そのプラットフォーム純正のアプリと同じ見た目になります。

- **iOS**：UIKit の見た目。
- **Android**：Material 3 の見た目。
- **macOS**：AppKit の見た目。
- **Windows**：Windows 11 の見た目（WinUI の Fluent デザイン。Asuka が Win32 のコントロールの上に描画します）。
- **Linux**：GNOME の見た目（GTK 4 と libadwaita）。
- **Web**：ブラウザーのコントロールと、その周りを囲む Asuka 独自のライト・ダークのパレット（[Web](web.md) を参照）。

変えたいものだけを設定してください。また、固定の色やフォントより、後述するセマンティックカラーとセマンティックフォントを使ってください。そうすれば、アプリはプラットフォームに追従し続けます。

## スタイル

どの要素も、ブロックの中で自身のプロパティに続けてスタイルのプロパティを受け取ります。

```asuka
Text(title) { font: .headline; color: .secondaryLabel }
Button("Follow") { shrink: 0 }
View { height: 1; background: .separator }
```

`style` は再利用するプロパティをまとめて宣言し、`style:` で 1 つのスタイルかスタイルのリストを適用します。同じプロパティを設定する項目が 2 つあれば後のものが優先され、ブロック自身のプロパティはリストより優先されます。

```asuka
style card {
  padding: 16; radius: 12; gap: 8
  background: .secondaryBackground
}

style chip(tint: Color) {
  padding: [4, 10]; radius: 999
  background: tint.opacity(0.15); color: tint
}

View { style: card; background: .accent }
Text("New") { style: chip(.green) }
Row { style: [card, if selected { chip(.accent) }] }
```

スタイルは関数と同じようにパラメーターを取り、自身の `style:` でほかのスタイルを適用できます。リストの中の `if` や `match` は、条件が成り立つ間だけスタイルを適用し、成り立たなくなると各プロパティを元に戻します。要素の名前を付けたスタイル（`style Button { radius: 8 }`）は、パッケージ内のその種類のすべての要素のデフォルトになります。構文の全体と、アプリの一部だけで変えられるテーマについては[スタイル](language/styles.md)を参照してください。

見た目のプロパティは、ネイティブのウィジェットが対応している場合にだけ反映されます。たとえばボタンは `color` をティントカラーとして使い、ビューは `font` を無視します。

## ビューの並べ方

`Column` は子を上から下へ、`Row` は先頭（leading）から末尾（trailing）へ並べます。`View` は、`direction` で方向が決まるコンテナーです。`direction` は `.column`（デフォルト）か `.row` です。

```asuka
Row {
  align: .center; gap: 12
  Avatar()
  Column {
    grow: 1; gap: 2
    Text("Ada Lovelace") { font: .headline }
    Text("Notes on the Analytical Engine") { font: .subheadline; color: .secondaryLabel; lines: 1 }
  }
  Button("Follow") { shrink: 0 }
}
```

レイアウトのプロパティしか持たないコンテナーは、ネイティブのビューを作りません。その子は、ネイティブのビューを持つ最も近い祖先の中に配置されます。そのため、いくら入れ子にしてもコストはかかりません。

## Flexbox

レイアウトは CSS の flexbox に従い、デフォルト値は React Native と同じです。つまり、方向は縦（column）で、サイズにはパディングと枠線が含まれます。

- **`gap`**：子どうしの間隔です。子が折り返したときは、行と行の間隔にもなります。
- **`justify`** は、主軸方向の子の配置です。`.start`（デフォルト）、`.center`、`.end`、`.spaceBetween`、`.spaceAround`、`.spaceEvenly` があります。
- **`align`** は、交差軸方向の配置です。`.stretch`（デフォルト。子が交差軸いっぱいに広がります）、`.start`、`.center`、`.end`、`.baseline` があります。
- **`alignSelf`** は、その子についてだけ親の `align` を上書きします。デフォルトの `.auto` なら、親の値に従います。
- **`wrap`** を設定すると、子が収まらないときに複数の行に折り返します。
- **`grow`** は、親の主軸方向に余ったスペースのうち、その子が受け取る割合です。伸びる子は、中身のサイズより小さく縮むこともできます。そのため、`Scroll` を囲む伸びるビューは、スクロールの中身の高さ全体に広がるのではなく、その中でスクロールします。
- **`shrink`** は、行があふれたときにその子が譲る量で、CSS の `flex-shrink` と同じです。設定しなければ 1 です（テキストは折り返し、コントロールは最小サイズを保ちます）。0 にするとそのサイズを保ちます（番号の列、アイコン、折り返すテキストの隣のボタンなど）。
- **`basis`** は、主軸方向での子の初期サイズです。デフォルトの `.auto` は中身のサイズです。

```asuka
// 幅を分け合う二つのボックス。一方はもう一方の 2 倍。
Row {
  gap: 8; height: 60
  View { grow: 2; background: .secondaryBackground }
  View { grow: 1; background: .secondaryBackground }
}

// 両端に置いたラベルと値。
Row {
  justify: .spaceBetween; align: .center
  Text("Total")
  Text("$42") { font: .title2 }
}

// 複数行に折り返すチップ。
Row {
  wrap: true; gap: 8
  Text("Asuka"); Text("iOS"); Text("Android")
}
```

## サイズ

長さは `Length` で表します。

- 数値：ポイント。iOS と macOS ではポイント、Android では dp、Windows では拡大率 100% のときの 1 ピクセル（1/96 インチ）、Linux では GTK の論理ピクセル、Web では CSS ピクセルです。
- パーセンテージ（`50%`）：同じ軸方向の、親のサイズに対する割合。
- `.auto`（デフォルト）：中身のサイズ、または flexbox が割り当てるサイズ。

`width` と `height` でサイズを指定し、`minWidth`、`minHeight`、`maxWidth`、`maxHeight` でその範囲を制限します。`aspectRatio` は幅を高さで割った比です。一辺のサイズが決まっていれば、もう一辺をこの比から決めます。

```asuka
View {
  width: 50%; minWidth: 120; maxWidth: 320
  aspectRatio: 16.0 / 9
  background: .gray
}
```

テキストとコントロールは、プラットフォーム自身の計測方法で自分のサイズを測ります。サイズを指定するのは、それを変えたいときだけにしてください。

1 ポイントが何ピクセルになるかは、ディスプレイのスケールで決まります。スマートフォンでは 2 か 3、Retina の Mac では 2、Windows と Linux ではディスプレイの拡大率（150% なら 1.5）、Web ではブラウザーの device pixel ratio です。実際の値は環境の `scale`（`environment().scale`）でわかります。デスクトップのウィンドウをスケールの異なるディスプレイに移すと、そのディスプレイのスケールで描画されます。ポイント単位のレイアウトは変わりません。

## パディングとマージン

`padding` と `margin` は `Edges` で、辺ごとにポイントで指定します。

- `16`：すべての辺に 16。
- `[8, 16]`：上下に 8、左右に 16。
- `[4, 8, 4, 8]`：上、右、下、左の順。
- `Edges(top: 24)`：辺を名前で個別に指定。

```asuka
Column {
  padding: [8, 16]; margin: Edges(top: 24)
  Text("Padded")
}
```

iOS、Android、Web では、`Button` の `padding` は CSS と同じようにプラットフォーム標準の余白に置き換わります。`minHeight: 48; padding: [10, 20]` はこれらのプラットフォームで高さ 48 ポイントになり、同じプロパティを指定した `View` と同じです。`padding` を指定しないボタンはプラットフォーム標準の余白のままで、枠線だけを指定した場合も同じです。デスクトップでは、ボタンの余白はネイティブのボタンの外枠の外側に付きます。

右から左に書く言語では、`Row` は右から並び、`justify` と `align` もそれに従います。テキストの行が始まる側も右になります。一方、`left` と `right` は CSS と同じく、書かれたとおりの側を指します。テキストが始まる側や終わる側に余白を置きたいときは、`start` や `end` を使います。

```asuka
View { padding: Edges(start: 16, end: 8) } // 英語では左に 16、アラビア語では右に 16
```

[多言語対応](i18n.md)も参照してください。

## 絶対配置

`absolute: true` を設定するとビューは通常のフローから外れ、`top`、`right`、`bottom`、`left` で親の中での位置を指定できます。次の例では、アバターの角にバッジを重ねています。

```asuka
View {
  width: 44; height: 44
  Avatar()
  View {
    absolute: true; top: -4; right: -4
    width: 18; height: 18; radius: 9
    background: .red; align: .center; justify: .center
    Badge()
  }
}
```

後の子ほど、前の子の上に重ねて描画されます。

## Grid

`grid` を設定したコンテナーは、子を CSS grid で配置します。トラックは `Track` で、`.fr(1)`、`.points(80)`、`.percent(25)`、`.auto`、`.minContent`、`.maxContent`、`.minmax(a, b)`、`.repeat(n, [...])`、`.autoFill([...])`、`.autoFit([...])` があります。子を複数のトラックにまたがらせるには、`gridColumn` と `gridRow` を使います（`.line(2)`、`.span(3)`、`.between(1, 3)`）。

```asuka
View {
  gap: 8
  grid: Grid(columns: [.repeat(3, [.fr(1)])])
  Avatar(); Avatar(); Avatar(); Avatar()
  View { gridColumn: .span(3); height: 20; background: .gray }
}
```

多数の項目をスクロールできるグリッドで表示するなら、`columns` を指定した `List` を使ってください（[コンポーネント](components.md)を参照）。

## セーフエリア、キーボード、ウィンドウ

スマートフォンでは、ルートは画面全体を覆い、ステータスバー、ノッチ、ホームインジケーターの下にまで広がります。`SafeArea` は、自身のパディングに加えて、セーフエリアのインセット分のパディングを持つ `Column` です。キーボードが表示されている間は下のパディングがキーボードの高さになるので、下端にあるもの（テキストフィールドや送信ボタン）はキーボードの上に出たままになります。

```asuka
SafeArea {
  padding: 16; gap: 12
  Text("Todos") { font: .largeTitle.bold() }
  Scroll {
    grow: 1
    for t in todos key t.id { TodoRow(t) }
  }
  Composer()
}
```

インセットは、そのビューがある画面のものです。スタックの画面、タブ、シートの中なら、その画面のインセットになります。自分で配置したいときは、`environment()` でインセットとキーボードの高さを取得できます（[基本概念](concepts.md#環境)を参照）。

macOS、Windows、Linux では、ルートはタイトルバー（Linux ではヘッダーバー）の下にある、ウィンドウのコンテンツ領域を埋めます。デスクトップのウィンドウにはセーフエリアがなく、オンスクリーンキーボードが重なることもありません。そのため、インセットとキーボードの高さは 0 になり、`SafeArea` は自身のパディングだけを持つ `Column` になります。それでも `SafeArea` は使っておきましょう。そうすれば、同じ画面がスマートフォンにもそのまま収まります。

アプリが指定しない限り、ウィンドウのサイズは 800 × 600 ポイントです（[ナビゲーション](navigation.md)を参照）。スマートフォンにはウィンドウがありません。ユーザーがウィンドウのサイズを変えると、環境の `width` と `height` が変わってレイアウトが再実行されるので、アプリ側で処理することは何もありません。あるサイズでレイアウトを切り替えたいとき（スペースに余裕があればサイドバーを表示するなど）は、タブレットの場合と同じく、環境の `widthClass` で分岐します。

```asuka
if environment().widthClass == .expanded {
  Row { grow: 1; Sidebar(); Detail() }
} else {
  ItemList()
}
```

ブラウザーでは、ページ全体を占めるアプリは、デバイスのセーフエリア（ノッチのあるスマートフォンでの `env(safe-area-inset-*)`）と、visual viewport から得たオンスクリーンキーボードの高さを使います。ページの一部に埋め込まれたアプリには、どちらもありません。

## スクロール

`Scroll` は子を縦方向に、`horizontal: true` を指定すれば横方向にスクロールします。スクロールする範囲が決まるように、サイズか `grow: 1` を指定してください。サイズのプロパティはスクロールビュー自体のボックスに適用され、`padding`、`gap`、`justify`、`align`、`wrap` は中身の並べ方に使われます。

```asuka
Scroll {
  grow: 1; padding: 16; gap: 12
  Text("Long content")
}
```

引っ張って更新する機能とスクロール位置については[コンポーネント](components.md)を、長いリストについては[リスト](components.md#リスト)を参照してください。

## 色

`Color` は次のいずれかです。

- **プラットフォームのデフォルト**：プロパティを設定しなかった場合です。
- **セマンティックカラー**：実際の色はプラットフォームが決めるので、ダークモードに追従します。
  - テキスト：`.label`、`.secondaryLabel`、`.tertiaryLabel`。
  - 背景：`.background`、`.secondaryBackground`、`.groupedBackground`、そしてその上に置くグループ化されたリストの行のための `.secondaryGroupedBackground`（iOS の inset grouped のセル）。
  - `.separator`、`.accent`（アプリのアクセントカラー）、`.onAccent`（アクセントカラーの上で読みやすい色。アクセントカラーで塗りつぶしたボタンの上のアイコンなど）、`.clear`（透明）。
  - システムの色：`.red`、`.orange`、`.yellow`、`.green`、`.blue`、`.purple`、`.pink`、`.gray`。
- **固定色**：`#7c3aed`、またはアルファ付きの `#7c3aed80`。実行時に初めてわかる色（サーバーから届くラベルの色、ユーザーが保存したテーマ）は `Color.parse("#7c3aed")`（`Color?` を返し、色でないテキストなら none。`#` は省略可）か `Color.rgb(124, 58, 237, alpha: 0.5)`（各チャンネルは 0〜255）で作ります。

セマンティックカラーの出どころは次のとおりです。

| プラットフォーム | セマンティックカラー |
| --- | --- |
| iOS | UIKit の色（`labelColor`、`systemBackground`、`systemRed` など） |
| Android | Material 3 のカラーロール |
| macOS | AppKit の色（`labelColor`、`windowBackgroundColor`、`controlBackgroundColor`、`separatorColor`、`systemRedColor` など） |
| Windows | Windows 11 のテーマカラー（WinUI の Fluent テーマリソース）。ライトまたはダーク |
| Linux | libadwaita の名前付きカラー（`window_fg_color`、`window_bg_color`、`card_bg_color`、`destructive_color` など）。テーマの定義どおりで、ハイコントラストにも対応 |
| Web | Asuka のパレット。CSS 変数で、ライトとダークの両方 |

デスクトップでは、グループ化用の背景は通常の背景と同じです。`.groupedBackground` はウィンドウの背景色、`.secondaryGroupedBackground` は `.secondaryBackground` と同じくカードの背景色になります。

```asuka
Column {
  padding: 16; gap: 4; background: .background; radius: 12
  Text("Title") { font: .headline }
  Text("Details") { font: .subheadline; color: .secondaryLabel }
}
```

`c.opacity(a)` は、色を 0 から 1 の不透明度で描画した色です（SwiftUI の `.opacity`、Compose の `copy(alpha =)` にあたります）。セマンティックカラーはセマンティックカラーのままなので、`opacity(0.15)` を付けたアクセントカラーは、ライトでもダークでもアクセントカラーの淡い色になります。チップの背景、グラフの塗り、トラックなどに使えます。

```asuka
let tint: Color = .accent
View { padding: [4, 10]; radius: 12; background: tint.opacity(0.15); Text("New") }
```

`color` は、テキストの色、またはコントロールのティントカラーです。

## 背景、枠線、影

- **`background`**：ビューの背景色。
- **`radius`**：角を丸めます。単位はポイントです。
- **`border`** と **`borderColor`**：枠線。ボックスの内側に描画され、パディングと同じように中身を内側に寄せます。
- **`shadow`**：ドロップシャドウ。中に描かれた内容ではなく、ビューの形（`radius` で角を丸めた境界）から落ちる影です。`Shadow(color:, radius:, x:, y:)` で指定し、`radius` はぼかし半径（CSS や SwiftUI と同じく、ぼかしの σ の 2 倍）、`x` と `y` はオフセットです。子をクリップするビュー（`clip`）でも影は出ます。画像をクリップした角丸のカードに影が付くのと同じです。Android では代わりにエレベーションの影を描画します。半径がエレベーションになり、色は影の色味になり、オフセットは光源で決まります。macOS、Windows、Linux、Web では、iOS と同じぼかしとオフセットで描画します。
- **`gradient`**：背景に重ねる線形グラデーション（`Gradient(stops: [...])`）。`startX`、`startY`、`endX`、`endY` を指定しなければ上から下へのグラデーションになります。これらはビューのサイズを単位とした値で、(0, 0) が左上の角、(1, 1) が右下の角です。各 `Stop` は、色と、0 から 1 の位置です。Android では 8 方向のうち最も近い方向に沿って描画し、ほかのプラットフォームでは指定した線に沿って描画します。
- **`material`**：背後にあるものをぼかす半透明の背景で、iOS のマテリアルにあたります。`.ultraThin`、`.thin`、`.regular`、`.thick`、`.bar` があります。
  - iOS：`UIVisualEffectView` のぼかし。
  - macOS：ビューの内容の背後に置いた `NSVisualEffectView`。ウィンドウ内でその背後にあるものをぼかします。
  - Windows：Asuka が描画する Fluent のアクリル。ビューの背後にあるものをぼかし、ライトまたはダークの色味を付けます。
  - Web：CSS の `backdrop-filter` によるぼかしと彩度の調整。
  - Android と Linux はビューの背後をぼかせないため、サーフェスの色（Linux では libadwaita のカードの色）を半透明で表示します。
- **`clip`**：子をビューの境界と角丸でクリップします。

```asuka
View {
  height: 120; radius: 16
  gradient: Gradient(stops: [Stop(color: #6366f1, at: 0), Stop(color: #ec4899, at: 1)])
  shadow: Shadow(color: #00000050, radius: 16, y: 8)
}

View {
  padding: 12; border: 1; borderColor: .separator; radius: 8; clip: true
  Text("Bordered")
}

// 左から右へ。
View {
  height: 80
  gradient: Gradient(stops: [Stop(color: .blue, at: 0), Stop(color: .purple, at: 1)], startY: 0.5, endX: 1, endY: 0.5)
}
```

## 不透明度と非表示

`opacity` は 0（透明）から 1（不透明。デフォルト）の値です。ビューを隠すには `hidden` を設定します。隠したビューも、レイアウト上のスペースは占めたままです。レイアウトから外し、中のビューもまとめて取り除くには、`if` の中に入れます（[基本概念](concepts.md#表示の切り替えと繰り返し)を参照）。

## トランスフォーム

`transform` は、描画時にビューを移動、拡大縮小、回転させます。`Transform(x:, y:, scale:, scaleX:, scaleY:, rotation:)` で指定し、`rotation` は時計回りの度数です。レイアウトはトランスフォームの影響を受けません。トランスフォームはすべてのプラットフォームで描画されます。トランスフォーム、`transition`、`shared` はアニメーションのためのものです。[モーションとジェスチャー](motion.md)を参照してください。

## テキスト

`font` はテキストのフォントを指定します。値は `Font` です。

- **セマンティックフォント**：プラットフォームのテキストスタイルで、ユーザーの文字サイズ設定に合わせて拡大縮小されます。`.largeTitle`、`.title`、`.title2`、`.title3`、`.headline`、`.subheadline`、`.body`、`.callout`、`.footnote`、`.caption`、`.caption2` があります。
- **`Font(size: 20)`**：指定したサイズ（ポイント）のシステムフォント。
- **ウェイト**：`.bold()`、`.semibold()`、`.medium()`、または `weighted(w)` に `.ultralight`、`.thin`、`.light`、`.regular`、`.medium`、`.semibold`、`.bold`、`.heavy`、`.black` のいずれかを渡します。

`color` はテキストの色、`textAlign` は揃え方（`.start`、`.center`、`.end`、`.justify`）、`lines` は表示する最大行数です。`lines` のデフォルトの 0 は無制限を意味します。テキストはデフォルトで折り返します。

```asuka
Text("Brand") { color: #7c3aed; font: .title.bold() }
Text("A long paragraph that stops after two lines.") { font: .body; lines: 2; textAlign: .center }
Text("Fixed size") { font: Font(size: 20).medium() }
```

複数のスタイル、リンク、インラインのビューを含むテキストについては、[RichText](components.md) を参照してください。

テキストスタイルの名前は iOS のものです。各プラットフォームはこれを自身のタイプランプ（文字サイズの体系）に対応づけるので、`.body` は、そのプラットフォームのアプリが本文に使うサイズになります。

| プラットフォーム | テキストスタイル | `.body` |
| --- | --- | --- |
| iOS | UIKit のテキストスタイル。フォントは San Francisco | 17 |
| Android | Material 3 のタイプスケール（`.body` は Body Large）。フォントは Roboto | 16 |
| macOS | AppKit のテキストスタイル（`NSFont.preferredFont(forTextStyle:)`）。フォントは San Francisco | 13 |
| Windows | Windows 11 のタイプランプ。フォントは Segoe UI Variable（Windows 11 より前は Segoe UI） | 14 |
| Linux | libadwaita のタイプクラス（`.title-1`、`.heading`、`.caption` など）。デスクトップのインターフェースフォントに対する相対値 | インターフェースフォントのサイズ |
| Web | Asuka のタイプランプ。フォントはシステムのフォント（`system-ui`） | 16 |

Windows では、各スタイルはタイプランプの中で最も近いものになります。`.largeTitle` は Title Large (40)、`.title` は Title (28)、`.title2` は Subtitle (20)、`.headline` は Body Strong（14、semibold）、`.footnote` と `.caption` は Caption (12) です。Linux では、GNOME のインターフェースフォントが 11 pt の場合、`.largeTitle` は libadwaita の `.title-1`（181%、heavy）、`.title` は `.title-2`、`.headline` は `.heading`（bold）、`.caption` は `.caption`（82%）になります。

ユーザーの文字サイズ設定によって拡大縮小されるのは、次のとおりです。

- **iOS**（Dynamic Type）と **Android**（フォントサイズ）：セマンティック、固定サイズ、カスタムを問わず、すべてのフォント。
- **Windows**：すべてのフォント。「設定」>「アクセシビリティ」>「テキストのサイズ」に従います。
- **Linux**：すべてのフォント。デスクトップのテキストのスケーリング（GNOME の「大きな文字」(Large Text)、GTK のフォント DPI）に従います。スタイルの基準になるインターフェースフォントも、デスクトップの設定（GTK の `gtk-font-name`）に従います。
- **Web**：すべてのフォント。フォントは `rem` で指定されるので、ブラウザーの文字サイズ設定に従います。
- **macOS** には文字サイズの設定がないため、フォントはそのままです。

その倍率は環境の `fontScale` です。デフォルトは 1 です（[アクセシビリティ](accessibility.md)を参照）。

## カスタムフォント

フォントファイル（`.ttf`、`.otf`）は、アプリの `assets/fonts` に置きます。フォントを選ぶには、フォントの `family`（`Font(family: "Inter")` や `.body.family("Inter")`）に次のいずれかを指定します。

- ファイル名：`assets/fonts/Inter-Bold.ttf` なら `"Inter-Bold"`。
- ファイルのファミリー：`"Inter"` を指定すると、フォントのウェイトに応じて `Inter-Regular`、`Inter-Bold` などが使われます。
- 汎用ファミリー：`"serif"`、`"monospace"`、`"rounded"`。iOS と macOS ではシステムフォントのデザイン（New York、SF Mono、SF Rounded）、Windows では Georgia と Cascadia Mono、Linux と Android ではシステムの serif と monospace のファミリー、Web ではブラウザーが対応していれば `ui-serif`、`ui-monospace`、`ui-rounded` になります。Android、Windows、Linux には rounded にあたるものがないため、システムフォントで表示されます。
- システムにあるファミリー：`"Georgia"` など。

カスタムフォントは、元にしたテキストスタイルと同じように拡大縮小されます。プラットフォームにないフォントは、システムフォントで表示されます。`assets/fonts` のファイルは、どのプラットフォームでもそのアプリ専用に読み込まれ、ほかのアプリ向けにインストールされることはありません。

```asuka
Text("Inter Bold") { font: .title.family("Inter-Bold") }
Text("Inter") { font: .body.family("Inter").semibold() }
Text("Code") { font: .footnote.family("monospace") }
```

## ダークモード

セマンティックカラーはダークモードに合わせて自動的に変わるので、それを使うアプリはダークモードのためのコードを書く必要がありません。固定色は変わりません。モードごとに色を選ぶには、環境の `dark` を読みます。

```asuka
View {
  height: 80; radius: 12
  background: if environment().dark { #1c1c1e } else { #ffffff }
}
```

## 動的なスタイル

状態を読むプロパティはバインディングです。その状態が変わると再実行され、そのプロパティだけがビューに送られます。変わったのがレイアウトのプロパティならレイアウトが再実行され、見た目のプロパティならビューが更新されるだけです。

```asuka
component Choice {
  state selected = false

  View {
    padding: 12; radius: 10
    background: if selected { .accent } else { .secondaryBackground }
    border: if selected { 2 } else { 0 }
    borderColor: .label
    on press { selected = !selected }
    Text("Tap me")
  }
}
```

この変更を `animate { }` の中で行えば、アニメーションになります（[モーションとジェスチャー](motion.md)を参照）。

## アクセントカラー

`.accent` と、それを使うコントロールは、`asuka.toml` に書いたアプリのアクセントカラーを使います。

```toml
accent = "#7c3aed"
```

アクセントカラーは、iOS ではティントカラーになり、Android では Material 3 のカラースキームのシードカラーになります（Android 12 以降）。`[android]` に `dynamicColor = true` を指定すると、Android は Material のアプリと同じく、システムが対応していればユーザーの壁紙から色を取ります。macOS、Windows、Linux では、Asuka のコントロールと色の中で、システムのアクセントカラーの代わりに使われます。Web では、さらにコントロールの `accent-color` にもなります。

アクセントカラーを指定しなければ、プラットフォームのものが使われます。macOS と Windows ではユーザーが選んだアクセントカラー（macOS は「システム設定」>「外観」、Windows は「設定」>「個人用設定」>「色」）、Linux では libadwaita の青、Web では Asuka 独自の青です。[配布](shipping.md)も参照してください。

白い背景で読みやすい色でも、暗い背景では暗すぎることがあります。`accentDark` でダークモードのアクセントカラーを指定できます。

```toml
accent = "#2563eb"
accentDark = "#93c5fd"
```

iOS、macOS、Windows、Linux、Web では、アプリがダークで表示されているとき、システムに従っている場合でも、アプリが自分でダークにしている場合（後述）でも、`.accent` とコントロールがこの色を使い、ライトに戻ると `accent` に戻ります。指定しなければ、どちらのモードでも `accent` が使われます。Android は `accent` そのものをカラースキームのシードにしてダーク用の色調を作るので、`accentDark` は使いません。

## ライトとダーク

アプリはシステムの外観に従います。iOS と macOS のダークモード、Android のダークテーマ、Windows のアプリモード（「設定」>「個人用設定」>「色」）、GNOME のダークスタイル、ブラウザーの `prefers-color-scheme` です。アプリ独自の外観設定と同じように、システムの設定にかかわらずライトかダークで表示することもできます（SwiftUI の `preferredColorScheme`、Android では AppCompat のナイトモード、macOS ではアプリの `NSAppearance`、Linux では libadwaita のスタイルマネージャーにあたります）。[アプリ](navigation.md)を参照してください。セマンティックカラー、プラットフォームのコントロール、システムバーは、アプリを作り直すことなく切り替わり、今どちらなのかは環境の `dark` でわかります。ユーザーの選択は [`std/store`](../stdlib.md) で保存しましょう。
