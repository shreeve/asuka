# ナビゲーション

アプリの骨組みは、画面のスタック、タブ、シート、アラート、メニュー、そしてデスクトップではウィンドウとメニューバーでできています。どれも各プラットフォーム純正のものです。

- **iOS**：`UINavigationController`、`UITabBarController`、シートの表示（sheet presentation）、`UIAlertController`、`UIMenu`。
- **Android**：Material のトランジションと予測型「戻る」に対応したトップアプリバー、Material のボトムナビゲーションバー、`BottomSheetDialog`、Material のダイアログ、`PopupMenu`。
- **macOS**：ウィンドウのツールバー（戻るボタン、画面のバーボタンと検索フィールド）とタイトル、`NSTabView`、シートウィンドウ、`NSAlert`、`NSMenu`、メニューバー。
- **Windows**：画面の上に置くヘッダーバー、WinUI の `NavigationView` を上部に置いた形のタブバー、モーダルウィンドウ、タスクダイアログ、ポップアップメニュー、ウィンドウのメニューバー。いずれも Windows 11 の見た目で描きます。
- **Linux**：libadwaita のものです。ウィンドウのヘッダーバー、`AdwNavigationView`、ビュースイッチャー、`AdwDialog`、`AdwAlertDialog`、GTK のポップオーバーメニュー、メインメニュー。
- **Web**：Asuka が描くヘッダーバー、`<dialog>` とポップオーバー、ブラウザーの戻るボタン（[Web](web.md) を参照）。

このページでは、これらに加えて「戻る」操作と、アプリを開くリンクや共有についても説明します。[`examples/navigation`](../../examples/navigation) にはこれらがすべて入っています。メニューバーとウィンドウの例は [`examples/notes`](../../examples/notes) にあります。

## スタック

`Stack` はナビゲーションスタックです。子要素が最初の画面になり、その下のコードは `push` で画面を積み重ねて（push して）いきます。

```asuka
struct Note {
  id: int
  text: string
}

state notes = [Note(id: 1, text: "Milk"), Note(id: 2, text: "Call Ada")]

app {
  Stack { Notes() }
}

screen Notes {
  title: "Notes"
  List {
    for n in notes key n.id {
      Row {
        padding: [12, 20]
        on press { push(Detail(n)) }
        Text(n.text)
      }
    }
  }
}

screen Detail(n: Note) {
  title: n.text
  SafeArea {
    padding: 20
    gap: 12
    Text(n.text)
    Button("Done") { on press { pop() } }
  }
}
```

- `push(Screen(…))` は、プラットフォームの push アニメーションで画面を一番上に表示します。
- `pop()` は一番上の画面を取り除きます。最初の画面が pop されることはありません。
- `depth()` は、スタックにある画面の数です。

`push`、`pop`、`depth` は、それを呼ぶコードを囲むもっとも近いスタックに対して働きます。画面自身のハンドラーと、そこから作られたもの（アラートのボタン、メニューやスワイプのアクション、スナックバーのアクション、`await` のあとのハンドラーの続き）は、最初の画面でもほかの画面と同じく、その画面のスタックに対して働きます。

iOS と Android では、スタックごとに専用のバーがあります。Windows（WinUI と同じように、Asuka が画面の上に描くヘッダーバー）と Web でも同様です。一方、Mac と Linux では、それぞれのネイティブアプリと同じくウィンドウにバーは一つだけで、そのバーは画面上でもっとも内側にあるスタック（タブの中なら、表示中のタブのスタック）のものになります。つまり、そのスタックの戻るボタンと、一番上の画面のタイトル、バーボタン、検索フィールドが表示されます。Mac ではこれがウィンドウのツールバーとタイトルに、Linux ではウィンドウの `AdwHeaderBar` になります。push と pop は macOS、Linux、Web ではスライドします。Windows では、push された画面が WinUI と同じように少し下から上がりながらフェードインし、pop では同じ動きで戻っていきます。

スタックは与えられた領域いっぱいに広がります。各画面は [オーナー](concepts.md) です。コードからでもユーザーの戻るジェスチャーでも、pop された画面は破棄され、そのエフェクトも止まります。もう一度 push された画面はまっさらな状態から始まるので、画面より長く残したいものは、画面の外にある状態（トップレベルの `state` やストア）か [ストレージ](data.md) に置いてください。

## 画面のオプション

`screen` 宣言のブロックでは、内容の前に画面のオプションを設定します。

- `title` はナビゲーションバーのタイトルです（Android ではトップアプリバーの、Windows・Linux・Web ではヘッダーバーのタイトルです。Mac ではウィンドウのタイトルになり、ブラウザーではページのタイトルにもなります）。バインディングなので、値を変えられます。
- `toolbar { … }` にはバーのボタンを置きます。
- `backGesture: false` は、その画面での戻るジェスチャーを無効にします（[戻る操作](#戻る操作) を参照）。
- `bind search: query` は、バーに検索フィールドを置きます。
- `zoomFrom` は、下の画面にあるビューからズームして画面を表示します。
- `largeTitle: true` は、iOS で画面上部にタイトルを大きく表示し、スクロールに合わせてバーの中へ縮めます。Android のトップアプリバー、デスクトップのバー、Web のバーは変わりません。バーが追従するのは画面の大部分を占める最初のスクロールビューなので、その背後に別のものがあってもかまいません。
- `barColor` は、Android ではトップアプリバーの色、Web ではヘッダーの色です。画面が自分で描く背景をバーまでつなげるために使います。iOS のバーは画面の最上部ではもともと透明で、デスクトップのバーはそれぞれ自分の色を保ちます。
- `bottomToolbar { … }` には画面下部のバーに置くボタンを、`primary { … }` には画面の主要なアクションを置きます（[ツールバーと主要アクション](#ツールバーと主要アクション) を参照）。`primaryExtended: true` を指定すると、`primary` は Android の拡張フローティングアクションボタンになります。

### バーボタン

それぞれは、画面の `toolbar` の中の `Button` です。`icon` を指定すると、バーにはタイトルの代わりにアイコンが表示され、スクリーンリーダーはタイトルを読み上げます（デスクトップでは、タイトルはアイコンのツールチップにもなります）。

```asuka
import platform/device

screen Detail(text: string) {
  title: text
  toolbar {
    Button("Share") {
      icon: "square.and.arrow.up"
      on press { device.share(text) }
    }
    Button("Delete") {
      role: .destructive
      on press { pop() }
    }
  }
  Text(text)
}
```

Android では、アクションは入りきる分だけトップアプリバーに並び、残りはオーバーフローメニューに入ります。アイコンを長押しするとタイトルが表示されます。Mac ではツールバーの項目に、Windows ではヘッダーバーの末尾のボタンに、Linux ではヘッダーバーのボタンになります。

### ツールバーと主要アクション

`bottomToolbar` は、SwiftUI の `ToolbarItem(placement: .bottomBar)` と同じように、画面下部のバーにボタンを置きます。`primary` は新規作成や追加のような画面の主要なアクションで、Material のフローティングアクションボタンにあたります。どちらもバーのボタンと同じく `Button` です。`icon` はボタンに表示するもの、タイトルはスクリーンリーダーが読み上げる内容です。`disabled` を指定するとボタンが淡色表示になり、`menu` を指定するとメニューを開くボタンになります。読んでいる値の変化にも追従します。

```asuka
import platform/device

screen Message(from: string) {
  state unread = false

  title: from
  bottomToolbar {
    Button("Delete") {
      icon: "trash"
      role: .destructive
      on press { pop() }
    }
    Button("Reply") {
      icon: "arrowshape.turn.up.left"
      menu: [
        .button(title: "Reply", run: () => device.haptic(.light)),
        .button(title: "Forward", run: () => device.haptic(.light)),
      ]
    }
    Button("Mark as unread") {
      icon: "envelope"
      disabled: unread
      on press { unread = true }
    }
  }
  primary {
    Button("Compose") {
      icon: "square.and.pencil"
      on press { device.haptic(.light) }
    }
  }
  Text("A message from {from}")
}
```

- iOS：ナビゲーションコントローラーのツールバーです。ボタンの間には可変スペースが入り、ツールバーは画面が一番上にある間だけ表示され、push や pop に合わせてスライドします。iOS にはフローティングボタンがないので、`primary` はツールバーの末尾の項目になり、iOS 26 では強調表示されます（「メール」の新規作成ボタンと同じです）。`primary` しかない画面にも、そのためのツールバーが付きます。UIKit と同じく、ツールバーの分は画面のセーフエリアから差し引かれます。`bottomToolbar` を持つ画面を push すると、（iOS 18 以降）タブのタブバーが隠れます。「メール」でメッセージを開いたときと同じ動作で、iOS 26 のフローティングタブバーがツールバーの上に重なってしまうのを避けるためです。
- Android：Material 3 の `BottomAppBar` です。ボタンは先頭側からアイコンボタンとして並び、入りきらないものはオーバーフローメニューに入ります。`primary` は画面の下端の末尾側に置く `FloatingActionButton` で、`bottomToolbar` があるときはバーにドッキングされます（影は付きません）。`primaryExtended` を指定すると `ExtendedFloatingActionButton` になり、アイコンの横にタイトルが表示されます。このボタンは画面の内容を下へスクロールするとアイコンだけに縮み、上へスクロールすると再び広がります（バーがあるときはバーの上に表示されます）。どちらのボタンも画面自身のものなので、push、pop、予測型「戻る」で画面と一緒に動きます。バーはその下にあるシステムのナビゲーションバーの領域まで広がり、画面のセーフエリアからはバーの分が差し引かれます。`bottomToolbar` を持つ画面を push すると、Material の規定どおり、タブのナビゲーションバーが隠れます。
- macOS：SwiftUI のボトムバーは iOS 向けのものなので、Mac では `bottomToolbar` のボタンをウィンドウのツールバーの中、バーボタンの後ろに表示します。`primary` は最後に強調表示されます（macOS 26 の、アクセントカラーで色付けされたツールバー項目）。
- Windows：画面の下に置くコマンドバー（Fluent の `CommandBar`）です。ボタンはアイコンとタイトル付きで先頭側から並び、入りきらないものは「その他のオプション」（…）メニューに入ります。`primary` は末尾側のアクセントボタンになります。
- Linux：`AdwToolbarView` の中で、ページの下部に置く `GtkActionBar`（GNOME のアクションバー）です。ボタンは先頭側に並び、`primary` は末尾側に、アイコンとタイトル付きの `suggested-action` ボタンとして置かれます。
- Web：スタックの下部に置くバーです（入りきらないボタンは「その他のオプション」メニューに入ります）。`primary` は下端の末尾側の隅に浮かぶボタンになります。拡張版のボタンはタイトルを表示し、内容を下へスクロールするとアイコンだけに折りたたまれます。

ボタンを押すと、その `on press` が実行されます。テストでは、`app.tap("Compose")` でボタンを押せ、`app.in(.bottomToolbar)` でタップをそのバーに絞り込めます。

### 検索

`bind search: query` は、SwiftUI の `searchable` と同じように、バーに検索フィールドを置きます。ユーザーが入力した内容は `query` に書き込まれ、コードから書き込むとフィールドの内容が変わります。`searchPlaceholder` を指定しなければ "Search" になります。`on searchSubmit` は、キーボードの検索キー、デスクトップでは Enter キーで実行されます。

```asuka
state notes = ["Milk", "Call Ada", "Book flights"]

app {
  Stack { Notes() }
}

screen Notes {
  state query = ""

  let shown = notes.filter(n => n.contains(query, ignoreCase: true))

  title: "Notes"
  bind search: query
  searchPlaceholder: "Search notes"
  List {
    for n in shown key n {
      Text(n) { padding: [12, 20] }
    }
  }
}
```

iOS では、タイトルの下に `UISearchController` が置かれ、画面をスクロールしても表示されたままになります。Android では Material のパターンに従い、トップアプリバーの検索ボタンを押すとフィールドに展開します。画面の下部に独自のコントロールがあるときは、`searchButton: true` を指定すると iOS でも検索フィールドが邪魔になりません。iOS 26 ではバーのボタンになり、それより前のバージョンでは、内容を引き下げるまで内容の上に隠れています。

デスクトップと Web では、フィールドは画面のバーに置かれます。

- **macOS**：ツールバーの検索フィールド（`NSSearchToolbarItem`）。
- **Windows**：検索グリフ付きの Fluent のテキストボックス。WinUI がコマンドバーに検索ボックスを置くのと同じく、バーボタンの前に置かれます。
- **Linux**：ヘッダーバーの検索ボタンと、その下に開く GTK の検索バー。GNOME のアプリと同じ検索の仕方で、ウィンドウのどこで入力しても検索バーが開きます。
- **Web**：ヘッダーの下のフィールド（`searchButton: true` のときは、ヘッダー内のボタン）。

### ビューからのズーム

`zoomFrom` には、下の画面にあるビュー（グリッド内の写真やカードなど）の `shared` のキーを指定します。画面は push されるときにそのビューからズームして現れ、pop されるときにそのビューへ縮んで戻ります。[共有要素](motion.md#共有要素) を参照してください。

```asuka
component Thumbnail(id: int) {
  View {
    label: "Photo"
    aspectRatio: 1
    shared: "photo-{id}"
    on press { push(Photo(id)) }
    Image("https://picsum.photos/id/{id}/300/300") { grow: 1 }
  }
}

screen Photo(id: int) {
  title: "Photo"
  zoomFrom: "photo-{id}"
  Image("https://picsum.photos/id/{id}/1200/1200") { grow: 1 }
}
```

iOS では、システムのズームトランジションになります（iOS 18 以降。それより前のバージョンでは通常どおり push されます）。Android は独自のズームを行い、予測型「戻る」ジェスチャーで pop すると、画面がビューの中へ戻っていきます。Web と Mac でもズームします。画面はビューの位置から、ビューの形に切り抜かれたまま広がり、pop されるとビューの中へ縮んで戻ります。Windows では WinUI のドリルインと同じようにズームし、画面はビューの位置から広がり、pop されるとそこへ縮んで戻ります。Linux では通常どおり push・pop します。GNOME にはズームのトランジションがないためです。[`examples/gallery`](../../examples/gallery) の写真グリッドがこの機能を使っています。

## 戻る操作

戻る方法はプラットフォームごとに異なり、Asuka はそれぞれのやり方をそのまま生かします。

- **iOS**：バーの戻るボタンと、画面の先頭側の端からのスワイプ。`backGesture: false` はスワイプを無効にしますが、戻るボタンでは引き続き pop できます。
- **Android**：ボタンまたはジェスチャーによるシステムの「戻る」と、トップアプリバーの上矢印。「戻る」は、画面上でもっとも内側にあり、戻り先の画面を持つスタックを pop します。タブの中なら、表示中のタブのスタックです。予測型「戻る」ジェスチャーでは、一番上の画面が縮んで指に追従し、下の画面が見えてきます。pop できる画面を持つスタックがないときは、ほかのアプリと同じく、「戻る」でアプリを離れます。`backGesture: false` は予測型「戻る」のアニメーションを無効にしますが、「戻る」で画面は引き続き pop されます。
- **macOS**：ツールバーの戻るボタンと、View ▸ Back（⌘[）。戻り先の画面がない間、このメニュー項目は無効になります。Esc キーでシートを閉じます。
- **Windows**：ヘッダーバーの戻るボタンと Alt+←。Alt+← は、押されたウィンドウのもっとも内側のスタックを pop します。Esc キーでシートを閉じます。
- **Linux**：libadwaita の操作です。ヘッダーバーの戻るボタン、Alt+←、Esc キー、タッチパッドやタッチスクリーンでのスワイプ。`backGesture: false` はスワイプを無効にしますが、戻るボタンでは引き続き pop できます。
- **Web**：ヘッダーの戻るボタン、ブラウザーの戻るボタン（とスマートフォンの戻るジェスチャー）、Esc キー、Alt+←（Mac では ⌘[）、タッチスクリーンでの先頭側の端からのスワイプ。[Web](web.md) を参照してください。

戻るジェスチャーのない macOS と Windows では、`backGesture` は何も変えません。ユーザーがどの方法で戻っても、`pop()` と同じく画面は破棄されます。シートやダイアログが開いているときは、「戻る」はまずそれを閉じます。

### 「戻る」を自分で処理する

コンポーネントの本体で `onBack { … }` を使うと、ユーザーが戻ろうとしたときに、実際に戻るかどうかをコンポーネントが決められます。下書きを捨てる前に確認したり、画面自身が開いているもの（検索や展開したパネル）を先に閉じたりするために使います。ハンドラーは、「戻る」を自分で処理したときに true を返します。すべてのハンドラーが false を返したときは、そのまま戻ります。

```asuka
screen Editor {
  state text = ""
  state saved = ""
  state discarding = false

  title: "Editor"
  onBack {
    if text == saved { return false } // 失うものはない：画面は pop される
    discarding = true
    return true
  }

  Alert("Discard changes?") {
    bind open: discarding
    Button("Keep editing") { role: .cancel }
    Button("Discard") {
      role: .destructive
      on press {
        saved = text
        pop()
      }
    }
  }

  TextField {
    bind text: text
    multiline: true
  }
}
```

ハンドラーは、コンポーネントが組み立てられた場所に属し、コンポーネントと一緒に消えます。

- **push された画面**：その画面の戻るボタン、Android の「戻る」、iOS の端からのスワイプ、Mac の ⌘[、Windows と Linux の Alt+←、ブラウザーの「戻る」。画面にハンドラーがある間、iOS と Linux ではその画面でのスワイプが無効になり（UIKit の純正アプリと同じく、スワイプは問い合わせを挟めず、止めることしかできないためです）、Android では pop のプレビューを表示しません。iOS の戻るボタンは、画面の `backAction` です。
- **シート**：下へのスワイプ、外側のタップ、Android の「戻る」、デスクトップやブラウザーでの Esc キー、Windows ではシートウィンドウの閉じるボタン。フルスクリーンのシートでは、Android の「戻る」と Esc キーだけです。ハンドラーがある間は、シートをスワイプで閉じられません。閉じようとするとハンドラーに問い合わせます（iOS の `isModalInPresentation`、Android ではキャンセル不可のシート）。ハンドラーが閉じることを許すと、ユーザーが閉じたときと同じようにシートが閉じて、その `on dismiss` が実行されます。
- **それ以外**（画面やシートの外、またはスタックの最初の画面。最初の画面では「戻る」でスタックを離れます）：pop する画面がないときの Android の「戻る」（本来ならアプリが閉じるところ）と、ブラウザーの「戻る」（本来ならページが前に戻るところ）。iOS とデスクトップには、このような「戻る」はありません。

問い合わせは、最後に登録されたハンドラーから順に行われます。`pop()` やコードからシートを閉じる場合は問い合わせません。問い合わせるのは、ユーザーが戻ろうとしたときだけです。

## 分割ビュー

`Split` は、行き先（destination）が多いアプリの最上位に置くものです。行き先を並べたサイドバーと、その横に選んだ行き先の画面を表示します（SwiftUI の `NavigationSplitView`）。子要素は `Destination` で、`Sidebar` のセクションに入れても、単独で置いてもかまいません。

```asuka
state unread = 3

app {
  Split("Mail") {
    Destination("Inbox") {
      icon: "tray"
      badge: "{unread}"
      Mailbox("Inbox")
    }
    Destination("Sent") {
      icon: "paperplane"
      Mailbox("Sent")
    }
    Sidebar("Folders") {
      Destination("Receipts") { Mailbox("Receipts") }
      Destination("Travel") { Mailbox("Travel") }
    }
  }
}

screen Mailbox(name: string) {
  Text("No mail in {name}")
}
```

各行き先の内容はスタックの最初の画面です。その下のコードは `push` で画面を push できます。別の行き先を選ぶと、スタックの画面はすぐにその行き先の最初の画面に置き換わり、元の画面は破棄されます。最初の画面にタイトルがなければ、行き先のタイトルが使われます。`bind selected` は選ばれている行き先で、セクションをまたいで通しで数えます。ユーザーが選ぶと書き込まれ、書き込むと別の行き先が表示されます。`Sidebar` のタイトルは、その中の行き先の見出しになります（行き先自身の `section:` でも指定できます）。

行き先は `for` や `if` で作ることもできます。`Sidebar` の中でも外でもかまいません：フォルダーごと、リポジトリごとに 1 つ、といった具合です。それらが変わるとサイドバーも追従し、表示中の行き先は位置が変わっても表示されたままです（`selected` も追従します）。表示中の行き先がなくなったときは、その位置にある行き先が表示されます。

```asuka
state folders = ["Inbox", "Work"]

app {
  Split("Mail") {
    Sidebar("Folders") {
      for f in folders key f {
        Destination(f) { Mailbox(f) }
      }
    }
  }
}

screen Mailbox(name: string) {
  Text("No mail in {name}")
}
```

| | コンパクト（スマートフォン） | 幅広 |
|---|---|---|
| iOS | iOS の流儀どおり、スタックに折りたたまれた分割ビュー。サイドバーが最初の画面（インセットグループ化されたリスト）になり、行をタップするとその行き先が push され、「戻る」でリストに戻ります | 内容の横に置く `UISplitViewController` のサイドバー（iOS 26 では Liquid Glass）。ユーザーが隠すこともできます |
| Android | Material のモーダルナビゲーションドロワー。行き先の最初の画面のメニューボタン（または端からのスワイプ）で開き、行き先を選ぶか「戻る」で閉じます | 内容の横に開いたままの標準ドロワー（幅 840dp 以上） |
| macOS | | `NSSplitViewController`。サイドバーは、ユーザーが折りたためるソースリストです |
| Windows | | WinUI の `NavigationView` のペイン。行き先が内容の横に並び、選択中のものにはアクセントカラーのピルが付きます |
| Linux | 幅 500 ポイント未満では、libadwaita の折りたたまれた分割ビュー。まずサイドバーが表示され、次に選んだ行き先の画面が、サイドバーへの戻るボタン付きで表示されます | `AdwNavigationSplitView`。サイドバーは独立したページで、そのタイトルは専用のヘッダーバーに表示されます。ウィンドウのヘッダーバーの下にある内容の横に並びます |
| Web | 詳細のヘッダーから開くドロワー | 詳細の横のサイドバー |

行き先の `badge` は、行の末尾に表示されます。「メール」がメールボックスの未読数を表示するのと同じです（SwiftUI のサイドバー行の `badge`）。普通は数を表示しますが、短い文字列なら何でもかまいません。空文字列なら、バッジは表示されません。バインディングなのでデータに追従します（上の `badge: "{unread}"`）。行はその場で更新されるので、サイドバーの選択、スクロール位置、フォーカスは保たれます。

| | 行き先のバッジ |
|---|---|
| iOS | セルの末尾に置くラベルのアクセサリー（セカンダリーカラー） |
| Android | ドロワー項目の末尾に置くカウンター（Gmail と同じ） |
| macOS | ソースリストの件数表示（セカンダリーカラー）。「メール」のメールボックスと同じです |
| Windows | ペイン項目の末尾に置く Fluent の `InfoBadge` |
| Linux | 行の末尾に置く淡色の数字 |
| Web | 行の末尾に置く数字 |

スクリーンリーダーは、バッジを行のタイトルと一緒に読み上げます。数は項目数として読まれ、たとえば "Inbox, 3 items" のようになります（Android では Material のバッジの文言で、"3 new notifications" となります）。

## タブ

`Tabs` は、タブバーを付けて一度に一つのページを表示します。

- **iOS**：`UITabBarController`。
- **Android**：Material のボトムナビゲーションバー。
- **macOS**：上部にタブを並べた `NSTabView`。
- **Windows**：WinUI の上部 `NavigationView` と同じ、上部に並ぶタブのバー。各タブにはアイコンとタイトルがあり、選択中のタブの下にはアクセントカラーのピルが付きます。矢印キーでタブ間を移動できます。
- **Linux**：`AdwViewStack`。GNOME のアプリと同じく、幅の広いウィンドウではヘッダーバーにビュースイッチャーを置き、幅の狭いウィンドウ（550 ポイント未満）では下部のバーにします。
- **Web**：下部のタブバー。

各 `Tab` は、タイトル、`icon`、内容を持ちます。`bind selected` は表示中のタブのインデックスです。タブを選ぶと書き込まれ、書き込むとタブが切り替わります。

```asuka
state tab = 0
state unread = 3

app {
  Tabs {
    bind selected: tab
    Tab("Notes") {
      icon: "list.bullet"
      badge: if unread > 0 { "{unread}" } else { "" }
      Stack { Notes() }
    }
    Tab("Settings") {
      icon: "gear"
      Stack { Settings() }
    }
  }
}

screen Notes {
  title: "Notes"
  Text("No notes")
}

screen Settings {
  title: "Settings"
  Text("Nothing to set")
}
```

タブの `badge` は、タブの上に表示されます（SwiftUI のタブ項目の `badge`、Compose の `NavigationBarItem` に付ける `BadgedBox`）。普通は "3" のような数ですが、短い文字列なら何でもかまいません。空文字列なら、バッジは表示されません。バインディングなので、値が変わると、そのタブだけが更新されます。

| | タブのバッジ |
|---|---|
| iOS | `UITabBarItem` の `badgeValue`。システムの赤いバッジです |
| Android | 項目の `BadgeDrawable`。数はその数字（999 を超えると 999+）で、それ以外の文字列はそのまま表示します |
| macOS | タブのラベルの中に "Inbox (3)" のように表示します。`NSTabView` はタブのラベルをすべて自分で描くためです |
| Windows | タブの末尾に置く Fluent の `InfoBadge` |
| Linux | 数はページの `badge-number` として、ビュースイッチャーのアイコンの横に表示します。それ以外の文字列は `needs-attention` のドットになります |
| Web | タブのアイコンに付く赤いバッジ |

スクリーンリーダーは、バッジをタブのタイトルと一緒に読み上げます。数は項目数として読まれ、たとえば "Inbox, 3 items" のようになります（iOS と Android では、プラットフォーム自身の文言になります）。

タブにはそれぞれ専用の `Stack` を持たせてください。そうすれば、各プラットフォームのアプリと同じく、タブごとにバーがあり、それぞれの履歴が保たれます。タブの内容は最初に選ばれたときに組み立てられ、タブを切り替えても保持されます。タブのアイコンは、ボタンと同じくシンボルです。[シンボルとアイコン](components.md#シンボルとアイコン) を参照してください。Android では、Android に用意されているアイコンでなければなりません。

## シート

`Sheet` は、`open` が true の間、内容をモーダルシートで表示します。

- **iOS**：detent 付きのシート表示。
- **Android**：`BottomSheetDialog`。
- **macOS**：使用中のウィンドウに付くシートウィンドウ。
- **Windows**：使用中のウィンドウの上に重ねるモーダルウィンドウ。その間、元のウィンドウは無効になります。
- **Linux**：`AdwDialog`。フローティングのダイアログで、幅の狭いウィンドウではボトムシートになります。
- **Web**：モーダルな `<dialog>`。幅の狭いウィンドウではボトムシート、幅の広いウィンドウでは中央に置くカードになります。

シートは宣言した場所で領域を取らないので、シートを開く画面のどこに置いてもかまいません。

```asuka
component Notes {
  state open = false
  state note = ""
  state draft = ""

  Column {
    Text(note)
    Button("Add a note") {
      on press {
        draft = note
        open = true
      }
    }
    Sheet {
      bind open: open
      detents: [.medium, .large]
      grabber: true
      on dismiss { print("dismissed, not saved") }
      SafeArea {
        padding: 20
        gap: 12
        TextField {
          bind text: draft
          placeholder: "Write a note"
        }
        Button("Save") {
          variant: .filled
          on press {
            note = draft
            open = false
          }
        }
      }
    }
  }
}
```

- `detents` は、シートが止まれる高さです。`.medium`（画面の半分）、`.large`（画面全体）、またはその両方を指定します。両方のときは medium で始まり、ユーザーが上へドラッグして広げられます。何も指定しなければ large です。キーボードが表示されると、large になれるシートは large になります。デスクトップでは、medium だけのシートはウィンドウの高さの半分、それ以外のシートはウィンドウの高さの大部分になります。Mac では、ユーザーがシートウィンドウのサイズを変えられます。`.fit` を指定すると、Mac のシートのようにコンテンツの高さに合わせます。フォームや確認のように、コンテンツ自体に高さがあるシート向けです。幅はシートの幅ですが、コンテンツがそこまで狭くなれないとき（固定の `width` など）は、コンテンツがすべて見えるように広がります（Web ではページの幅まで）。Mac のシートや広いページのシートのようなカードでは、コンテンツが、シートより狭い固定の `width` を持つビュー 1 つ（それ自身の幅を持つダイアログなど）だけのとき、幅を持たないビューに包まれていても、そのビューの幅と周りのパディングの分の幅になります。狭いページのボトムシートは、ページの幅のままです。Mac では、シートより広がったシートが表示されている間、ウィンドウはシートと左右の少しの余白より狭くできず、すでに狭いウィンドウは広がります。コンテンツが変わると高さも追従し、シートの最大の高さを超える分は中の `Scroll` がスクロールします。ユーザーはサイズを変えられません。このシートに対応しているのは macOS と Web で、それ以外では large になります。
- `grabber: true` は、シートの上端にハンドルを表示します（iOS、Android、Web のボトムシート）。
- 内容は、シートが開くときに組み立てられ、閉じるときに破棄されます。そのため、中で作った状態は毎回まっさらな状態から始まります。

### 動かせるシート

`movable: true` を指定すると、デスクトップのシートは、ウィンドウに張り付いたシートではなく、デスクトップのモーダルダイアログのようにユーザーが動かせるダイアログになります。下にある内容と見比べたい詳細やフォームに向いています。モーダルであることはシートと同じで、閉じるまで後ろの内容は入力を受け付けません。Esc キーで閉じ、外側をクリックしても閉じません（`dismissOutside: true` を指定した場合を除きます）。タイトルバーはなく、コンテンツがシート全体で、その見出しが上端になります（「閉じる」を用意してください）。ユーザーは背景、つまりコントロールや押せるビュー、選択できるテキスト以外の場所をドラッグして動かします。開くときはウィンドウの中央に、`detents` が決めるサイズで表示されます。`.fit` のシートはコンテンツと同じ大きさで、ほかの `.fit` のシートと同じようにコンテンツの変化に合わせて大きさが変わります。`resizable: true` を指定すると、ユーザーはサイズも変えられます。長いレポートのように大きく表示する価値のある内容に向いています。指定しなければ、デスクトップのダイアログが普通そうであるように、動かせるシートのサイズは変えられません（`Window` は逆に、`resizable: false` と書かない限りサイズを変えられます）。サイズを変えられる `.fit` のシートは、開くときはコンテンツのサイズで、それがユーザーが縮められる最小のサイズにもなります。その後はユーザーが決めたサイズのままで、コンテンツはそれを埋めます。`dismissOutside: true` を指定すると、外側をクリックしたときに Esc キーと同じように閉じます。一覧から開く詳細のように、ちらっと見てすぐ離れるダイアログに向いています。指定しなければ、デスクトップのモーダルダイアログと同じく、外側をクリックしても何も起きません（Mac では警告音が鳴ります）。

```asuka
component Report {
  state showing = false
  prop report = ""

  Sheet {
    bind open: showing
    detents: [.fit]
    movable: true
    resizable: true
    Scroll { Text(report) { selectable: true } }
  }
}
```

| | 動かせるシート |
|---|---|
| macOS | タイトルバーのない独立したウィンドウ（ウィンドウの影と角の丸みはそのまま。`resizable` を指定すると縁をドラッグしてサイズを変えられます）。元のウィンドウの手前、その中央に、開く位置でフェードインします。表示中、元のウィンドウはクリック、スクロール、ホバーを受け付けず（`dismissOutside` を指定すると、そこをクリックしたときに閉じます）、⌘W で閉じます |
| Windows | 使用中のウィンドウの上に表示されるモーダルウィンドウ。シートはもともとこの形です |
| Web | 広いウィンドウでは、背景の上に表示されるカード。開く位置でフェードインします。`resizable` を指定すると右下の角をドラッグしてサイズを変えられます。`dismissOutside` を指定すると背景をクリックしたときに閉じます |
| iOS、Android、Linux | 指定しないときと同じシート。スマートフォンのシートは動かせず、libadwaita のダイアログも動かせません |

`fullScreen` のシートはこの指定を無視します。

### フルスクリーンのシート

`fullScreen: true` を指定すると、シートは代わりに画面全体を覆います。メッセージの作成、サインイン、写真の編集のように、終わるか諦めるまでユーザーの注意を引きつけておく作業のためのものです（SwiftUI の `fullScreenCover`、Material のフルスクリーンダイアログ）。`detents` と `grabber` は適用されず、外側をタップしたりスワイプしたりして閉じることもできません。`open` が false になったときに閉じるので、専用の「キャンセル」と「完了」を用意してください。Android の「戻る」と、デスクトップやブラウザーでの Esc キーは、通常のシートと同じくこのシートも閉じます。ただし、中の `onBack` が引き止めた場合（先に破棄を確認すべき下書きがあるときなど）は閉じません。

```asuka
component Mail {
  state composing = false

  Column {
    Button("New Message") { on press { composing = true } }
    Sheet {
      bind open: composing
      fullScreen: true
      Compose { on done { composing = false } }
    }
  }
}

component Compose {
  event done
  state draft = ""

  onBack { return !draft.isEmpty() } // 「戻る」で下書きを失わない

  SafeArea {
    grow: 1
    padding: 16
    gap: 12
    Row {
      align: .center
      Button("Cancel") {
        variant: .plain
        on press { emit done }
      }
      Text("New Message") {
        grow: 1
        textAlign: .center
      }
      Button("Send") {
        variant: .filled
        on press { emit done }
      }
    }
    TextField {
      bind text: draft
      multiline: true
      grow: 1
    }
  }
}
```

フルスクリーンのシートの環境は、画面そのものの環境です。スマートフォンでは画面全体の大きさを持ち、ステータスバー、ノッチ、ホームインジケーターまたはナビゲーションバー、キーボードのセーフエリアも持ちます。そのため `SafeArea` は、ルートと同じように、内容がそれらに重ならないように配置します。

| | 実体 |
|---|---|
| iOS | `.fullScreen` で表示するコントローラー。すべてのものの上にスライドして現れ、閉じるまで下の画面は見えなくなります |
| Android | 独自のテーマを持つ全画面の `Dialog`。エッジ・ツー・エッジで表示し（ステータスバーとナビゲーションバーのインセット、ディスプレイカットアウト、キーボードに対応）、Material の強調イージング（emphasized easing）でスライドして現れます。予測型「戻る」では、スワイプした側の端へ向かって縮み、角が丸くなります（`onBack` が横取りした場合を除きます） |
| macOS | ウィンドウのタイトルバーとツールバーの下にある内容全体と同じ大きさのシート。ウィンドウのサイズ変更に追従し、ユーザーがシートのサイズを変えることはできません。Mac にはフルスクリーンのモーダルがなく、AppKit でウィンドウを一つの作業に専念させるにはシートを使うためです |
| Windows | ウィンドウの内容全体を覆う枠なしのウィンドウ（タイトルバーは元のウィンドウのもので、その間、元のウィンドウは無効になります）。元のウィンドウの移動やサイズ変更に追従し、Windows 11 がウィンドウの角を丸めるのと同じく、下の角が丸くなります。Fluent のフルサイズの `ContentDialog` にあたります |
| Linux | ウィンドウと同じ大きさのフローティングの `AdwDialog`。libadwaita は、収まりきらないダイアログを（スマートフォンや幅の狭いウィンドウで）ウィンドウ全体に重ねて表示しますが、それと同じようにウィンドウ全体を覆って表示されます。ウィンドウのサイズ変更に追従します |
| Web | ページ全体を覆うモーダルな `<dialog>`（フォーカスはその中にとどまり、Esc キーとブラウザーの「戻る」で閉じます）。Window パネルの中で宣言されたシートでは、そのパネルの内容を覆います。スライドして現れます |

ユーザーは、シートを下へスワイプして閉じられます。Android では「戻る」やシートの外側のタップで、デスクトップやブラウザーでは Esc キーでも閉じられます（Windows ではシートウィンドウの閉じるボタン、Web では外側のクリックでも閉じます）。このとき `open` は false になり、`on dismiss` が実行されます。コードから `open` を false にした場合は、`on dismiss` を実行せずに閉じます。つまり `on dismiss` は「アプリの指示によらずに閉じられた」ことを表し、保存されないまま残った下書きの処理などに使います。

## ポップオーバー

`Popover` は、`open` が true の間、ビューを指す吹き出しの中に `content` を表示し、画面の上に重ねます。いくつかの選択肢、詳しい情報、小さなフォームなどに使います。子要素が、吹き出しの指す対象になります。

```asuka
component Sharing {
  state open = false

  Popover {
    bind open: open
    maxWidth: 280
    content {
      Column {
        padding: 16
        gap: 8
        Text("Shared with 3 people") { font: .headline }
        Button("Manage") { on press { open = false } }
      }
    }
    Button("") {
      icon: "person.2"
      label: "Sharing"
      on press { open = true }
    }
  }
}
```

ポップオーバーの大きさは内容に合わせて決まりますが、幅は `maxWidth`（既定は 320）を超えず、テキストはその幅で折り返します。高さも画面の大部分を超えることはありません。内容は開くときに組み立てられ、閉じるときに破棄されます。外側をタップすると閉じ、シートと同じく `open` が false になって `on dismiss` が実行されます。中の `onBack` で、閉じるのを引き止められます。

| | ビュー | 位置 |
|---|---|---|
| iOS | `UIPopoverPresentationController`。iPhone でもポップオーバーのまま表示し（シートに変換しません）、iOS 26 では Liquid Glass になります | UIKit が収まる位置を決め、矢印がアンカーを指します |
| Android | Material のコンテナーサーフェス上の `PopupWindow`。角の丸みは 16dp で、エレベーションが付きます | アンカーの下に中央揃え（スペースがなければ上） |
| macOS | transient な `NSPopover` | アンカーの下、AppKit が収まる位置を決めます |
| Windows | ライトディスミスのポップアップウィンドウ。角は Windows 11 のものです（WinUI の Flyout） | アンカーの下に中央揃え（スペースがなければ上） |
| Linux | `GtkPopover`。Esc キーでも閉じます | アンカーの下、GTK がスペースを見つけた位置 |
| Web | ポップオーバー | アンカーの隣 |

## アラートとアクションシート

`alert(title, …)` は、その場でアラートを表示します。`Alert` 要素は、`open` が true の間アラートを表示します。

- **iOS**：`UIAlertController`。
- **Android**：Material のダイアログ。
- **macOS**：使用中のウィンドウにシートとして表示する `NSAlert`。
- **Windows**：使用中のウィンドウの上に表示するタスクダイアログ。
- **Linux**：`AdwAlertDialog`。
- **Web**：モーダルな `<dialog>`。

ボタンを押すとアラートが閉じ、そのボタンの `run`（`Alert` の `Button` なら `on press`）が実行されます。ボタンがなければ、OK ボタンが一つだけ表示されます。

```asuka
component Note {
  state confirming = false

  Column {
    Button("Delete") { on press { confirming = true } }
    Button("Delete now") {
      on press {
        alert("Delete this note?", message: "You can't undo this.", actions: [
          .button(title: "Cancel", role: .cancel),
          .button(title: "Delete", role: .destructive, run: () => remove()),
        ])
      }
    }
    Alert("Delete this note?") {
      bind open: confirming
      message: "You can't undo this."
      Button("Cancel") { role: .cancel }
      Button("Delete") {
        role: .destructive
        on press { remove() }
      }
    }
  }
}

fn remove() {}
```

ボタンの `role` は、そのボタンの役割を表します。

- `.normal`（既定）は通常のボタンです。
- `.cancel` は、操作をやめるためのボタンです。iOS では専用の位置に置かれ、Android ではダイアログのネガティブボタンになります。デスクトップでは Esc キーでこのボタンが押されます。Mac では、デフォルトボタンの後ろの一番最後のボタンになります。
- `.destructive` は赤で表示されます。iOS のスタイル、Android では Material のエラーカラー、Mac では AppKit の破壊的なボタン、Linux では libadwaita の destructive な応答になります。

Android では、キャンセル以外のボタンのうち最後のものがダイアログのポジティブボタンとして末尾に置かれ、もう一つあればそれはニュートラルボタンになります。キャンセル以外のボタンが 3 つ以上あると、ダイアログはそれらをリストとして表示します。Mac では、キャンセル以外の最初のボタンがデフォルトボタンになり、Return キーで押されます。Linux では、最初の `.normal` のボタンがデフォルトになり、アクセントカラーで表示されます。

`actionSheet: true` を指定すると、ボタンをアクションシートとして表示します。iOS では、画面の下から現れる選択肢のリストです。

```asuka
component PhotoButton {
  Button("Photo") {
    on press {
      alert("Photo", actionSheet: true, actions: [
        .button(title: "Take a photo", run: () => {}),
        .button(title: "Choose from library", run: () => {}),
        .button(title: "Cancel", role: .cancel),
      ])
    }
  }
}
```

Android では、アクションシートはアクションをリストとして並べたダイアログになり、`message` は表示されません。Windows ではボタンがタスクダイアログのコマンドリンクになり、Linux ではデフォルトボタンのないアラートダイアログに、Mac ではほかと変わらないアラートに、Web ではボタンをリストとして並べたダイアログになります。Android の「戻る」や外側のタップなど、ボタン以外の方法でアラートが閉じられた場合は、どのボタンも実行されません（`Alert` の `on dismiss` が実行されます）。

### テキストフィールド

SwiftUI のアラートに `TextField` や `SecureField` を入れるのと同じように、アラートで名前やパスワードなどのテキストを入力してもらうこともできます。`Alert` の中の `TextField` は、メッセージの下に表示されます。各フィールドにバインドした `text` はその値を初期値とし、ユーザーが入力するたびに書き込まれるので、ボタンの `disabled` をそれに追従させられます。次の例では、両方のフィールドが埋まるまで「Sign In」が無効のままになり、アラートの表示中もボタンの状態が更新されます。

```asuka
component SignIn {
  state open = false
  state user = ""
  state pass = ""

  Column {
    Button("Sign in") { on press { open = true } }
    Alert("Sign in") {
      bind open: open
      TextField {
        bind text: user
        placeholder: "Name"
        contentType: "username"
      }
      TextField {
        bind text: pass
        placeholder: "Password"
        secure: true
        contentType: "password"
      }
      Button("Cancel") { role: .cancel }
      Button("Sign In") {
        disabled: user.isEmpty() || pass.isEmpty()
        on press { print("signing in {user}") }
      }
    }
  }
}
```

フィールドは、`TextField` と同じく、`keyboard` と自動入力（AutoFill）のための `contentType` も受け取ります。`secure` を指定すると、入力内容が隠されます。最初のフィールドにフォーカスがあり、キーボードが表示された状態で始まります。フィールドで Return を押すと、次のフィールドに移ります。最後のフィールドで Return を押すと、デフォルトのボタン、つまり最初の `.normal` のボタン（UIAlertController の `preferredAction`）が押されます。ただし、そのボタンが有効な間だけです。アラートの表示中にフィールドの `text` に書き込んでも、フィールドの表示は変わりません。アクションシートには、フィールドは表示されません。

| プラットフォーム | フィールド |
| --- | --- |
| iOS | `UIAlertController` のテキストフィールド。デフォルトのアクションは `preferredAction` で、太字で表示されます |
| Android | Material のダイアログ内に置くアウトライン付きの `TextInputLayout`（パスワードには表示切り替えボタン付き）。IME のアクションで次のフィールドへ移り、最後のフィールドではデフォルトのアクションのボタンを押します |
| macOS | `NSAlert` のアクセサリービューとして置く `NSTextField` と `NSSecureTextField`。Tab キーでフィールド間を移動します |
| Windows | WinUI の `ContentDialog` と同じレイアウトで、Asuka が自前で用意するダイアログ（タスクダイアログにはフィールドを置けないため）。タイトル、メッセージ、Fluent のテキストボックス、ボタンの順に並び、デフォルトのボタンはアクセントカラーになります。Tab、Enter、Esc キーは通常のダイアログと同じように動作し、ナレーターは各フィールドをプレースホルダーで読み上げます |
| Linux | `AdwEntryRow` と `AdwPasswordEntryRow` を並べた boxed list。`AdwAlertDialog` の extra child として置きます |
| Web | `<dialog>` 内の input 要素。プレースホルダーをラベルとし、`type=password`、`inputmode`、`autocomplete` を設定します |

## スナックバー

`snackbar(message, …)` は、ウィンドウの下部に短いメッセージをその場で表示します。「元に戻す」のようなアクションを任意で付けられ、スナックバーを閉じるクロージャーを返します。

- **iOS**：UIKit にはスナックバーがないので、iOS 自身が短いメッセージを表示するときと同じ見た目で Asuka が描きます。システムのマテリアル（iOS 26 では Liquid Glass）でできたカプセル形で、最前面に表示されているコンテンツの上に重ね、タブバーやツールバー、キーボードより上の位置に中央揃えで置かれます。スプリングで出入りし（「視差効果を減らす」がオンのときはフェード）、下へスワイプすると閉じます。
- **Android**：Material の `Snackbar`。スワイプで閉じられ、画面のボトムナビゲーションバーかフローティングアクションボタン（アンカー）とキーボードより上に表示されます。シートの上では、シートの下部に表示されます。
- **macOS**：ウィンドウ下部に置く、`NSVisualEffectView` によるオーバーレイ。閉じるボタン付きです。
- **Windows**：ウィンドウ下部に置く、Fluent の InfoBar として描くオーバーレイ。閉じるボタン付きです。
- **Linux**：ウィンドウの `AdwToastOverlay` 内の `AdwToast`。
- **Web**：ウィンドウ下部に置く、role が `status` の要素（最上位レイヤーに置くポップオーバー）。閉じるボタン付きで、下へスワイプしても閉じます。

```asuka
component Notes {
  state notes = ["Milk", "Call Ada"]

  fn remove(i: int) {
    let gone = notes.removeAt(i)
    snackbar("Note deleted", action: .button(title: "Undo", run: () => { notes.insert(gone, at: i) }), onDismiss: () => {
      print("deleted for good: {gone}")
    })
  }

  Column {
    for (i, n) in notes.enumerated() key n {
      Button(n) { on press { remove(i) } }
    }
  }
}
```

メッセージは一度表示するだけのものなので、アラートと同じく文字列です。アクションは `.button` で、その `title` は表示され、`role`、`icon`、`disabled` は表示されません。アクションを押すとスナックバーが閉じ、その `run` が実行されます。`onDismiss` は、それ以外の方法で消えたときに実行されます。つまり、時間切れになったとき、ユーザーがスワイプしたか閉じたとき、別のスナックバーに置き換えられたとき、`snackbar` が返したクロージャーが呼ばれたときです。そのため、「元に戻す」が押された後は `onDismiss` は実行されません。アクションで取り消せる処理を確定させる場所が `onDismiss` です。

`duration` は表示時間です。`.short`（既定。Material と同じく約 4 秒）、`.long`（約 10 秒）、`.indefinite`（閉じられるか、アクションが押されるまで）から選びます。Asuka は、アクセシビリティのためにプラットフォームが推奨するとおりに表示時間を延ばします。Android では `AccessibilityManager.getRecommendedTimeoutMillis` に従い、TalkBack がオンの間は、アクション付きのスナックバーを長く表示します。推奨タイムアウトのない iOS、macOS、Linux では、スクリーンリーダーがオンの間、アクション付きのスナックバーは閉じられるまで表示されたままになります。推奨タイムアウトがないときの Material の挙動と同じです。Windows では、さらに「この時間が経過した後に通知を閉じる」（Dismiss notifications after）の設定も反映します。デスクトップと Web では、マウスがスナックバーの上にある間や、キーボードのフォーカスがその中にある間は、時間が進みません。スクリーンリーダーは、表示されたときにスナックバーを読み上げ、そのボタンにも移動できます。

一度に表示されるスナックバーは一つだけです。Android と同じく、新しいスナックバーは表示中のものを置き換えます（古いほうの `onDismiss` が先に実行されます）。デスクトップでは、アラートと同じウィンドウに表示されます。つまり、スナックバーを表示するきっかけとなったイベントのビューがあるウィンドウ、それがなければ呼び出したコンポーネントのウィンドウ、それもなければメインウィンドウです。

## メニュー

メニューは `Action` のリストを表示します。アラートのボタンやバーボタンと同じ型で、各タイトルの横には任意で `icon` を付けられます。メニューを表示する方法は 2 つあります。

- `menu` を指定した `Button` は、タップするとメニューを開きます。
- `Menu { actions: […] }` は、任意の内容にメニューを付けます。タップで開くか、`longPress: true` を指定してコンテキストメニューにします。

iOS では `UIMenu`、Android では `PopupMenu` です。Mac では `NSMenu`、Windows ではポップアップメニュー（Mac と同じく、破壊的な項目は赤）、Linux では `GtkPopoverMenu`、Web ではポップオーバーになります。デスクトップと Web では、コンテキストメニューは右クリック（副ボタンのクリック）で開き、Windows、Linux、Web では長押しでも開きます。どちらの方法も [コンポーネント](components.md#メニュー) で説明しています。`List` の行にはスワイプアクションもあります。[リスト](components.md#リスト) を参照してください。`MenuBar` の中の `Menu` は、代わりにアプリのメニューの一つになります（[後述](#メニューバーとキーボードショートカット)）。

## ウィンドウ

デスクトップアプリのメインウィンドウには、`app` ブロックが表示するものが入ります。ただし、メインウィンドウのないアプリもあります（[後述](#メインウィンドウのないアプリ)）。`app` ブロックの `Window` がメインウィンドウで、そのキーがメインウィンドウのオプション、子要素が表示する内容になります。

```asuka
app {
  Window {
    title: "Notes"
    width: 420
    height: 640
    minWidth: 320
    minHeight: 360
    Stack { NotesScreen() }
  }
}

screen NotesScreen {
  title: "Notes"
  Text("No notes")
}
```

サイズはウィンドウの内容の大きさで、単位はポイントです。何も指定しなければ、メインウィンドウは 800 × 600 ポイントで中央に開き、ユーザーは 320 × 320 ポイントより小さくできません。`title` が空なら、アプリの名前になります。バインディングなので、アプリの言語に追従させられます。ウィンドウはユーザーが最後に置いた場所に戻り（[後述](#ウィンドウの位置の復元)）、それが `width`、`height`、`x`、`y` より優先されます。iOS と Android は、これらのオプションを無視します。メインウィンドウは `open` が false でない限り表示されます。`bind open: shown` とすると `shown` に合わせて表示・非表示が切り替わり、動き続けるアプリでユーザーがウィンドウを閉じると false が書き込まれます。

ほかの `Window` は、`Sheet` がシートを開くのと同じように、`open` が true の間、別のウィンドウを開きます。

```asuka
state notes = ["Milk"]

component Notes {
  state stats = false
  Column {
    Button("Statistics") { on press { stats = true } }
    Window {
      bind open: stats
      title: "Statistics"
      width: 280
      height: 140
      Text("{notes.count()} notes")
    }
  }
}
```

- 子要素は、ウィンドウが開くときに組み立てられ、閉じるときに破棄されます。その中の `environment()` はそのウィンドウのもので、サイズはユーザーによるサイズ変更に追従し、スケールはウィンドウのあるディスプレイのものになります。
- ユーザーがウィンドウを閉じると、`open` に false が書き込まれ、`close` が発生します。コードから `open` を false にした場合は、`close` を発生させずに閉じます。
- 指定がなければ 800 × 600 ポイントで開きます。Mac と Windows では、使用中のウィンドウから少しずらして重ねる（カスケードする）位置に開きます。Linux では、GTK 4 がデスクトップに任せているとおり、配置はデスクトップが決めます（GNOME と KDE は、新しいウィンドウをほかのウィンドウをもっとも覆わない位置に置きます）。中のスタックには、メインウィンドウと同じく専用のバーがあります。シートとアラートは、ユーザーが作業中のウィンドウの上に開きます。

| | ウィンドウ |
|---|---|
| macOS | `NSWindow`。専用のツールバーとタイトル（もっとも内側のスタックのもの）を持ちます |
| Windows | トップレベルウィンドウ。専用のタイトルとタスクバーボタンを持ちます |
| Linux | `AdwWindow`。専用のヘッダーバーを持ちます |
| Web | ページ内のパネル。ユーザーが移動したりサイズを変えたりできます |

スマートフォンにはウィンドウがないので、そこでは `Window` は何も表示しません。ウィンドウを提供するコンポーネントは、後述のメニューバーのように、デスクトップにしかないものから提供してください。

### オプション

メインウィンドウと `Window` は、ウィンドウの開き方について同じキーを受け取ります。キーはウィンドウが開くときに読まれます（あとから変えるのはハンドルの操作で、後述します。ただし `title`、`alwaysOnTop`、`clickThrough`、`flyout`、`edited`、`file` はバインディングに追従します）。

| キー | |
|---|---|
| `width`、`height` | 内容のサイズ（ポイント） |
| `minWidth`、`minHeight`、`maxWidth`、`maxHeight` | ユーザーが変えられるサイズの最小と最大。0 ならプラットフォームの最小値、最大はなし |
| `x`、`y` | 画面上のフレームの左上。メインディスプレイの左上からのポイント。両方 0 なら、メインウィンドウは中央に、`Window` はカスケードして開きます（`center: true` で中央） |
| `resizable`、`minimizable`、`maximizable`、`closable` | false にすると、ユーザーの操作（サイズ変更と各ボタン）を取り除きます |
| `alwaysOnTop` | ほかのアプリのウィンドウより常に手前に置きます |
| `titleBar` | `.hidden` にすると、内容をタイトルバーの下、ウィンドウの上端まで広げ、ウィンドウのボタンを内容の上に重ねます |
| `background`、`transparent` | 内容の背後のウィンドウの色。または色なしにして、内容が何も描かないところにウィンドウの背後を見せます |
| `hidden` | 表示せずに開きます。`w.show()` で表示します |
| `skipTaskbar` | タスクバー（Windows）と Window メニュー（macOS）に出しません |
| `maximized`、`fullScreen` | その状態で開きます |
| `stateKey` | 位置を保存するキー。[後述](#ウィンドウの位置の復元) |
| `fitContent` | 最小と最大のサイズの範囲で、内容に合わせてサイズを変え続けます（[デスクトップ](desktop.md#コンテンツに合わせたウィンドウサイズ)） |
| `panel` | ランチャーやパレットのウィンドウ。すべてのアプリの上に浮かび、アプリを前面に出さずにキーボードを受け取り、Escape やほかの場所のクリックで閉じます（[パネル](desktop.md#パネルとランチャー)） |
| `clickThrough` | マウスを通過させ、背後にあるものに届けます（[クリックスルー](desktop.md#クリックスルーウィンドウ)） |
| `flyout` | `Window` 用。トレイアイコンや親の矩形からぶら下げます（[フライアウト](desktop.md#フライアウト)） |
| `child`、`modal` | `Window` 用。[後述](#子ウィンドウとモーダルウィンドウ) |
| `menuBar`、`edited`、`file` | ドキュメントを表示するウィンドウ用。ウィンドウ内のアプリのメニューバー（Windows、Linux）、Mac の閉じるボタンの点やタイトルの前の `*`、Mac のタイトルバーのファイルアイコン |

タイトルバーを隠すと、内容はウィンドウの上端まで広がり、ウィンドウのボタン（Mac の信号機ボタン、Windows のキャプションボタン、Linux のヘッダーバーのボタン）の下に入ります。ウィンドウの環境の `safeTop` はタイトルバーの高さになるので、`SafeArea` はボタンを避け、背景やサイドバー、画像はボタンの下まで広げられます。

```asuka
app {
  Window {
    titleBar: .hidden
    background: #1e1e1e
    SafeArea {
      grow: 1
      padding: 16
      Editor()
    }
  }
}

component Editor {
  TextField { multiline: true; grow: 1 }
}
```

画面自身のセーフエリアは、この帯に覆われる部分です。そのため画面の中の `SafeArea` もこの帯を避け、ウインドウのボタンと同じ高さに見出しを置くこともできます。上端がこの帯の下にあるスクロールビューは、内容が帯の下から始まり、スクロールすると帯の下に入ります。iOS のナビゲーションバーの下と同じです（境界の見せ方は `edgeEffect`：[コンポーネント](components.md#scroll)）。内容が `Split` のウインドウでは、タイトルバーを隠しても隠さなくても、Mac の分割ビューのアプリと同じようにサイドバーがウインドウのボタンの下まで伸び、ツールバーは詳細側の上にあります。

内容がクリックを受け取らないところでは、ボタンの下の帯でウィンドウをドラッグできます。Windows では、タイトルバーを隠したメインウィンドウにはメニューバーがありませんが、ショートカットは使えます。

準備ができてから表示したいウィンドウは、`hidden` で開いてコードから表示します。その間に内容が組み立てられ、レイアウトされ、描画されるので、空のまま表示されることはありません。バックグラウンドで起動するアプリ（トレイアイコンのアプリ）は、ウィンドウをメニューから開きます。メインウィンドウはまったく要りません（[後述](#メインウィンドウのないアプリ)）。

### ウィンドウの操作

`Window as w` で、ウィンドウのハンドル（その `Window` の `Ref`）に名前を付けます。囲んでいるビューの要素から見え、コンポーネントに渡せます。ほかのビューの `Ref` は、そのビューがあるウィンドウに作用します。

```asuka
component Editor(w: Ref) {
  Column {
    Button("Full Screen") { on press { w.setFullScreen(true) } }
    Text(if w.focused { "Typing here" } else { "In the background" })
  }
}

component Main {
  state open = false
  Window as w {
    bind open: open
    title: "Editor"
    Editor(w: w)
  }
}
```

- `w.show()` と `w.hide()` で表示・非表示を切り替えます。非表示にしても開いたままで、内容も残ります。`w.focus()` は、非表示や最小化されていれば表示し、キーボードを受け取る状態で手前に出し、アプリも前面に出します。トレイアイコンのメニューからウィンドウを呼び戻すときに使います（Windows では、アプリへのユーザーの入力のあとでないとアプリを前面に出せません。トレイアイコンのクリックは入力にあたります）。閉じている `Window` に `show()` や `focus()` を呼ぶと開きます。
- `w.minimize()`、`w.maximize()`、`w.restore()`（どちらの状態からでも）、`w.setFullScreen(on)`。
- `w.setSize(width, height)`（内容のサイズ、ポイント）、`w.setPosition(x, y)`（フレームの左上、メインディスプレイの左上から）、`w.center()`。
- `w.setAlwaysOnTop(on)` で、ほかのアプリのウィンドウより常に手前に置くかどうかを切り替えます。
- `w.setClickThrough(on)` で、マウスを通過させるかどうかを切り替えます。
- `w.close()` は、ユーザーが閉じるときと同じようにウィンドウを閉じ、先に確認します（後述）。`window.close()` は、呼び出したコードがあるウィンドウ（どの `Window` の中でもなければメインウィンドウ）を確認せずに閉じます。メインウィンドウが閉じると、動き続けるアプリでない限り、アプリは終了します。
- ウィンドウの状態は、アプリだけでなくユーザーによる変化にも追従します。`w.visible`、`w.focused`、`w.minimized`、`w.maximized`、`w.fullScreen`、`w.alwaysOnTop`、`w.clickThrough`、`w.position`、`w.size`（それぞれ `(x, y)` と `(width, height)`）があり、`w.windowState` ですべてをまとめて読めます。これらを読むバインディングは追従します。ウィンドウの状態が報告されるのは、何かがそれを読んでからです。最小化されたウィンドウや、ほかのウィンドウの後ろにあるウィンドウも、visible のままです。

`window.main()` はメインウィンドウのハンドル、`window.current()` は呼び出したコードがあるウィンドウ（その `Window` のもの、なければメインウィンドウのもの）のハンドルで、メニューの項目、トレイアイコンのメニュー、ストアなど、どこからでも使えます。

操作は、状態の書き込みと同じく、次に画面が更新されるときにウィンドウに反映されます。スマートフォンでは何もせず、状態は何も表示されていないことを示します。

### ウィンドウを閉じる前の確認

ユーザーがウィンドウを閉じるとき（閉じるボタン、⌘W、Alt+F4、Dock やタスクバーのメニュー）や `w.close()` を呼んだときは、先に確認します。

- ウィンドウの中の `WindowCloseGuard { when: …; on closeRequest { … } }` は、`when` が true のあいだウィンドウを開いたままにし、`closeRequest` を実行します。未保存の変更について確認し、ユーザーが選んだら `window.close()` を呼びます。
- コンポーネントの本体に書く `onCloseRequest(fn() -> bool)` は、そのコンポーネントがあるあいだ関数に問い合わせます。false を返すと開いたままです。
- `Window` 自身の `on closeRequest { … }`（メインウィンドウにもあります）は、ユーザーが閉じる操作を引き受けます。ウィンドウは、`open` が false になるか `window.close()` が呼ばれるまで開いたままです。

```asuka
screen NoteScreen {
  state draft = ""
  state asking = false
  WindowCloseGuard {
    when: !draft.isEmpty()
    on closeRequest { asking = true }
  }
  Sheet {
    bind open: asking
    Column {
      Text("Discard the new note?")
      Button("Cancel") { on press { asking = false } }
      Button("Discard") { on press { window.close() } }
    }
  }
  TextField { bind text: draft }
}
```

問い合わせを受けるすべてのガードと関数が同意しなければ、ウィンドウは閉じません。Web では、メインウィンドウはタブそのものです。ページから離れるときやタブを閉じるときにも確認し、拒否するとページはブラウザー自身の「このサイトを離れますか？」という確認の背後に残ります（この文言はページからは変えられません）。アプリの終了（⌘Q）では、ウィンドウには確認せず、アプリの `on quit` に確認します（[デスクトップ](desktop.md#終了)）。

### ウィンドウの位置の復元

ウィンドウは、次に起動したときも、ユーザーが最後に置いた場所に戻ります。メインウィンドウは常に（キー `"main"`、または `stateKey` で）戻り、`Window` は `stateKey` を持つときに戻ります。ウィンドウの種類ごとに別のキーを付けてください（`"inspector"`、`"preferences"`）。前回の位置は `width`、`height`、`x`、`y` より優先されます。これらの値は、初回起動時に開くときに使われます。

| | 保存されるもの |
|---|---|
| macOS | フレーム。AppKit のフレーム自動保存を使います（アプリの defaults に保存） |
| Windows | 位置とサイズ（画面のピクセル単位と、そのときの DPI）、最大化されていたかどうか。接続されているモニターの上に、はみ出さないように戻します |
| Linux | サイズと、最大化されていたかどうか。Wayland ではアプリがウィンドウの位置を決められないためです |
| Web | `Window` パネルの位置とサイズ、最大化されていたかどうか。ページの `localStorage` に保存します |

デスクトップのアプリは、この情報をアプリのデータディレクトリにある `asuka-window.json`（Windows、Linux）、またはアプリの defaults（macOS）に保存します。テストや自動実行では何も保存しません。

### 子ウィンドウとモーダルウィンドウ

`child: true` の `Window` は、宣言されたウィンドウ（メインウィンドウか、内容にそれを含む `Window`）の子になります。親より常に手前にあり、親と一緒に閉じます（内容は親の内容と一緒に破棄されます）。`modal: true` にすると、閉じるまで親をブロックする子になります。

```asuka
component Main {
  state prefs = false

  Column {
    Button("Preferences…") { on press { prefs = true } }
    Window {
      bind open: prefs
      modal: true
      title: "Preferences"
      width: 480
      height: 360
      Preferences()
    }
  }
}

component Preferences {
  Text("Preferences")
}
```

| | `child` | `modal` |
|---|---|---|
| macOS | 子ウィンドウ。親より手前に並び、親と一緒に動きます | 親のシート |
| Windows | 親に所有されます。親より手前にあり、親と一緒に最小化されます | 所有され、開いている間は親が無効になります |
| Linux | 親に対して transient。親と一緒に破棄されます | transient かつモーダル |
| Web | ほかと同じパネル | ページのクリックを受け止める背景の上のパネル |

### メインウィンドウのないアプリ

app ブロックに自分の `Window` がなければ、ブロックが表示するものはメインウィンドウに入ります。そこに何も表示しないブロックには、メインウィンドウそのものがありません。コンポーネントが表示するのは `Window`、トレイアイコン、`MenuBar`、グローバルショートカットだけです。メニューバーアプリや書類ベースのアプリと同じく、ウィンドウはアプリが開いたものだけになります。

```asuka
import platform/tray

state open = false

app {
  Clock()
}

component Clock {
  tray.Tray("clock") {
    symbol: "clock"
    menu: [
      .button(title: "Show Clock", run: () => { open = true }),
      .button(title: "Quit", run: () => quit()),
    ]
  }
  Window {
    bind open: open
    title: "Clock"
    width: 240
    height: 120
    Text("12:00")
  }
}
```

- `window.main()` はこの場合もメインウィンドウを指しますが、その操作は何もせず、状態はスマートフォンと同じく何も表示されていないことを示します。どの `Window` の中でもない場所の `window.current()` も、このハンドルになります。
- アプリをもう一度開くと（Dock アイコンのクリック、アプリの再起動）、開いているウィンドウとともにアプリが前面に出ます。開いているウィンドウがなければ、何も表示しません。
- ユーザーが最後のウィンドウを閉じるとアプリは終了します。ただし、動き続けるアプリ（`asuka.toml` の `keepRunning` か `background`）は別で、メニューバーアプリはこちらです。

メインウィンドウがあるかどうかは、状態が何を表示しているかではなく、アプリのコンパイル時に、ビルド先のプラットフォームでコードが表示しうるもので決まります（`if @platform` はそのプラットフォームの分岐を通ります）。ビューを表示する可能性のあるブロックにはメインウィンドウがあり、何も表示していない間は空になります。Web ではタブがメインウィンドウで、そのときは何も表示しません。スマートフォンでは、何を表示していても画面はアプリのものです。

### デスクトップごとの対応状況

| | macOS | Windows | Linux (GTK 4) | Web：タブ | Web：`Window` |
|---|---|---|---|---|---|
| 表示・非表示・フォーカス | 対応 | 対応 | 対応 | フォーカスのみ | 対応 |
| 最小化 | 対応 | 対応 | 対応（Wayland では状態が通知されません） | 非対応 | タイトルバーだけに縮小 |
| 最大化・復元 | ズーム | 対応 | 対応 | 非対応 | アプリの領域いっぱいに広げる |
| フルスクリーン | 対応 | 対応 | 対応 | Fullscreen API | Fullscreen API |
| サイズ・最小サイズ | 対応 | 対応 | 対応 | 非対応 | 対応 |
| 最大サイズ | 対応 | 対応 | 非対応（GTK 4 にはありません） | 非対応 | 対応 |
| 位置・中央配置 | 対応 | 対応 | X11 のみ。Wayland では非対応（コンポジターがウィンドウを配置します） | 非対応（ブラウザーのウィンドウの位置は読めます） | 対応 |
| 常に手前に表示 | 対応 | 対応 | X11 のみ | 非対応 | ほかのパネルより手前 |
| タイトルバーを隠す | 対応 | 対応 | 対応 | 非対応 | 対応 |
| 背景・透明 | 対応 | 背景のみ。透明は背景で代替 | 対応（透明はコンポジターがある場合） | 背景のみ | 対応 |
| タスクバーに表示しない | 「ウインドウ」メニュー | 対応 | X11 のみ | 非対応 | 非対応 |
| パネル・フライアウト | 対応 | 対応 | X11：手前に保ち、配置も可能。Wayland：配置できず、手前にも保てません | 非対応 | 対応 |
| 内容に合わせたサイズ | 対応 | 対応 | 対応 | 非対応 | 対応 |
| クリックスルー | 対応 | 対応 | 対応 | 非対応 | ページまで通す |

## メニューバーとキーボードショートカット

`MenuBar` は、デスクトップアプリにメニューを与え、その項目にキーボードショートカットを割り当てます。子要素は `Menu`、`Menu` の子要素は `MenuItem` です。

```asuka
state notes: [string] = []
state done = 0

fn add() { notes.push("New note") }
fn export() {}
fn clear() { done = 0 }

component Notes {
  MenuBar {
    Menu("File") {
      MenuItem("New Note") {
        shortcut: Shortcut(key: "n", primary: true)
        on press { add() }
      }
      MenuSeparator()
      MenuItem("Export…") {
        shortcut: Shortcut(key: "e", primary: true, shift: true)
        on press { export() }
      }
      MenuItem("Close") { role: .close }
    }
    Menu("Edit") {
      MenuItem("Clear Completed") {
        shortcut: Shortcut(key: "k", primary: true, shift: true)
        role: .destructive
        disabled: done == 0
        on press { clear() }
      }
    }
  }
  NoteList()
}

component NoteList {
  Column {
    for (i, n) in notes.enumerated() key i { Text(n) }
  }
}
```

- `MenuItem` は、タイトル、`shortcut`、`press` を持ちます。`disabled` と `checked` で表示が変わります。項目の中に `MenuItem` を入れると、その項目はサブメニューになります。`MenuSeparator()` は、項目のグループの間に入れる区切り線です。
- `role` は項目の種類です。`.destructive` は赤で表示します（macOS、Windows。GTK では別のセクションに分けます）。`.quit`、`.close`、`.openRecent` はプラットフォーム自身の項目で、Asuka がプラットフォームのタイトル（項目にタイトルがなければ）とショートカットで実行します。それぞれ、アプリの終了（先にアプリに確認します）、使用中のウィンドウを閉じるボタンと同じように閉じること、最近使ったドキュメント（Mac 標準の「最近使った項目を開く」）です。
- `Shortcut` は、`key` とその修飾キーからなります。`primary` はプラットフォームのコマンドキー（Mac では ⌘、それ以外では Ctrl）、`shift`、`alt`（Mac の Option）、`control` がほかの修飾キーです。キーは文字（`"n"`、`","`、`"="`）か名前で指定します。名前は `enter`、`escape`、`tab`、`space`、`delete`、`backspace`、`up`、`down`、`left`、`right`、`home`、`end`、`pageup`、`pagedown`、`f1` から `f12` までです。
- 項目はバインディングです。項目が無効になったりチェックされたり、`for` や `if` が表示するメニューや項目が現れたり消えたりすると、送り直されます。複数の `MenuBar` が表示されているときは最後のものが使われ、それがなくなると一つ前のものが使われます。領域は取らないので、どこに置いてもかまいません。
- `WindowKeys { keys: [Shortcut(key: "k", primary: true, run: search)] }` は、表示されているあいだ、置かれたウィンドウにキーを与えます。フォーカスがどこにあっても、どのビューも処理しないキーに働きます（最後の `WindowKeys` のものが優先されます）。メニュー項目にしないキーに使います。

デスクトップごとの表示は次のとおりです。

- **macOS**：メニューバー。Mac 標準のメニューと並んで表示されます。標準のメニューは、アプリケーションメニュー（About、Hide、Quit）、File、Edit（Undo、Cut、Copy、Paste...）、View（Back、Enter Full Screen）、Window です。File、Edit、View、Window、Help というタイトルのメニューは、対応する標準メニューに項目を追加します。それ以外のメニューは View と Window の間に入ります。
- **Windows**：メインウィンドウ（と `menuBar: true` の `Window`）のメニューバー。ショートカットはアプリのすべてのウィンドウで使えます。各メニューと項目にはアクセスキー（Alt+F）が付き、各項目のタイトルの後ろにはショートカット（"Ctrl+Shift+K"）が表示されます。
- **Linux**：GNOME のアプリにはメニューバーがなく、代わりにメインメニュー（ヘッダーバーの最後のボタン）があります。アプリの各メニューはそのサブメニューになり（メニューが一つだけなら、その項目がメインメニューに直接並びます）、ショートカットはすべてのウィンドウで使えます。
- **Web**：ショートカットはページ内で使えます。メニューバーは表示しません。

iOS と Android は `MenuBar` を無視するので、どのプラットフォームで動くアプリにもメニューバーを持たせられます。ただし、メニューの項目はコマンドなので、スマートフォンのために、それぞれ画面上の UI からも実行できるようにしておいてください。

## トースト

iOS にも Android にも、アプリ間で共通して使われる標準のトーストはないため、Asuka にはトーストのコンポーネントがありません。画面の上に重ねたビューを `if` で表示し、[トランジション](motion.md#表示と非表示のトランジション) で登場させて作ってください。ギャラリーのトーストはこの方法で作られていて、スクリーンリーダーにも読み上げさせるために `accessibility.announce` を呼んでいます。[`examples/gallery`](../../examples/gallery) を参照してください。

## ルート

`Router` を使うと、ウェブページと同じように、どの画面にもアドレス（`/inbox/18` や `/search?q=go` のような href）が付きます。アプリはルートを要素として一度だけ宣言します。パスのパターンを画面に対応させ、その画面を表示するナビゲーターの下に置きます。あとはアドレスでも、画面でもナビゲートできます。

```asuka
app {
  Router(initial: "/inbox") {
    Route("/", redirect: "/inbox")
    Route(layout: .tabs) {
      Route("/inbox", layout: .stack, title: "Inbox", icon: "tray") {
        Inbox
        Thread
        Route(":id/reply", sheet: true, fullScreen: true) { p => Reply(id: p.get("id")) }
      }
      Route("/settings", layout: .stack, title: "Settings", icon: "gear") {
        Settings
        Route("about", title: "About") { _ => About() }
      }
    }
  }
}

screen Inbox at "" {
  title: "Inbox"
  Button("A message") { href: Thread(id: 18) }
}

screen Thread(id: int) at ":id" {
  title: "Message {id}"
  Button("Reply") { href: "/inbox/{id}/reply" }
}

screen Reply(id: string) {
  title: "Reply"
  Button("Send") { on press { dismiss() } }
}

screen Settings at "" {
  title: "Settings"
  Button("About") { on press { navigate("/settings/about") } }
}

screen About {
  Text("Version 1.0")
}
```

アドレスがあれば、次のことがそのまま手に入ります。Web のアドレスバー、戻る、進むがアドレスを表示し、再読み込みや貼り付けたリンクでその画面が開き、アプリのスキームやドメインへのリンクがその画面へ向かい、デスクトップのウィンドウは前回離れた場所で開きます。[`examples/navigation`](../../examples/navigation) はルートを使っています。

### パターンとパラメーター

ルートのパスは、`/` で始まらない限り、親のパスからの相対パスです。セグメントは一つずつ照合されます。

- `chat` はそれ自体に一致します。
- `:id` はパラメーターで、任意の一つのセグメントに一致します。
- `*path` はキャッチオールで、パスの残り（0 個以上のセグメント）に一致します（パターンの最後に置きます）。たとえば `docs/a.md` です。
- `""` は親自身のパス、つまりそのインデックスです。

複数のルートが一致するときは、expo-router と同じく、もっとも具体的なものが選ばれます。セグメントごとに、静的なものがパラメーターより、パラメーターがキャッチオールより優先されます。`/files/new` は `/files/new` に、`/files/3` は `/files/:id` に、それ以外は `/files/*path` に一致します。

ルートの画面は、ブロックの中で `p => View` として組み立てるか（`Params` を受け取ります。`p.get("id")` はパスのパラメーター、なければクエリのもの。`p.path`、`p.query`、`p.href` もあります）、`screen Thread(id: int) at ":id"` のようにパターン付きで宣言した画面の名前をブロックに書きます。このようなルート付きの画面のパラメーターはアドレスから取られます。まずパスから、なければクエリから（`/search?q=go` の `q`）取り、パラメーターの型（`int`、`float`、`bool`、列挙型のケース、`string`）に変換します。アドレスにないパラメーターは既定値になります。既定値がない場合や、値を変換できない場合は、画面がもっとも近い `Boundary` へ `RouteError` を投げます。タイトルは画面自身のものです。

### ナビゲーターとシート

中にルートを持つ `Route` は、その `layout` に応じてそれらのナビゲーターになるか、グループになります。

- `.stack`：`Stack`。子が画面とシートになります。
- `.tabs`：`Tabs`。各子がタブ（`title`、`icon`、`badge`）になり、画面一つでも、独自のスタックでもかまいません。
- `.split`：`Split`。各子がサイドバーの行き先になり、画面一つでも、（詳細側の）スタックでもかまいません。
- `.slot`：一度に一つの子だけを、バーなしでその場所いっぱいに表示します。アプリのサインアウト時とサインイン時の部分などに使います。
- `.group`（既定）：子は、このルートのパスとガードの下で、親のナビゲーターに加わります。

`sheet: true` のルートは、push されたり選ばれたりする代わりに、そのナビゲーター（スタックの一番上の画面やタブ）の上にシートとして表示されます。`detents`、`grabber`、`fullScreen` はシートのものです。「戻る」で閉じ、その下のナビゲーターの画面へナビゲートしても閉じます。

### ナビゲーション

- `navigate(href)` はその画面へ移動します。途中のタブ、行き先、スロットはそこへ切り替わります。その画面のスタックでは、同じアドレスの画面が前にあればそこまで戻り、なければ push します。シートの画面なら表示します。`navigate(Thread(id: 7))` はルート付きの画面のアドレスへ移動します。
- `replace(href)` は、現在の画面（シートの場所も同じ）の代わりにその画面を表示し、元の画面は破棄されます。履歴は増えません。
- `pop()` はもっとも内側のスタックで戻り、`dismiss()` はいちばん新しいシートを閉じます。
- `router.back()` は、ユーザーの「戻る」と同じように戻ります（`onBack` には問い合わせません）。いちばん新しいシート、なければ画面上でもっとも内側のスタックです。`router.canGoBack()` は戻れるかどうかです。`router.dismiss()` は、ルーターのいちばん新しいシートを閉じます。
- `router.href()` は表示中のアドレスで、それを読むバインディングは変化に追従します（サイドバーの選択やリンクの状態など）。`router.setQuery("q=go")` は、画面はそのままに、表示中の画面のクエリを変えます。

`href` を持つ `Button` や押せるビューは、押されると（`on press` のあとで）そこへ移動します。ルーターの中では、リッチテキストの中のアプリ自身の href へのリンク（`Span(text: "the inbox", link: "/inbox")`）もそこへ移動します。どれも、そのコードを囲むルーター、なければメインウィンドウのルーターに対して働きます。

ルート付きのスタックでも `push` は使えます。ルートのない画面を push します。その画面のアドレスはスタックのアドレスのままで、履歴のエントリーも一つ持ちます（ブラウザーの「戻る」で pop されます）が、再読み込みやリンクでは戻ってきません。

ほかの場所と同じく、どの画面もオーナーです。ルートによって pop、置き換え、離脱された画面（別のタブの画面は除きます）は破棄されます。

### リンクの下の画面

リンクは、それが表す画面を組み立てるので、そこから戻るとまともな場所に戻ります。途中の各スタックでは、まずスタックの `initial` の画面（別の子を指定しない限り最初の子。expo-router の `initialRouteName` にあたります）、次にリンクのパスをたどった画面（`/inbox/:id/reply` の下には `/inbox/:id`）、最後にリンク先の画面です。ほかのタブは最初の画面のままです。それぞれのパラメーターは、名前でリンクのパラメーターから取られます。リンクにないパラメーターを必要とする画面や、ガードが入れない画面は作られません。

### リダイレクト、ガード、見つからないとき

`redirect` を持つルートは、そのパスに来たユーザーを、パラメーターを埋めた別の href へ置き換えで送ります。`Route("/", redirect: "/inbox")`、`Route("/m/:id", redirect: "/inbox/:id")` のように使います。

`guard` は、その画面（と子の画面）が表示される前に、行き先の href を渡されて問い合わせられます。`""` を返せば通し、href を返せばそこへ送ります。ガードは状態を読めます。読んだ状態が変わると、表示中の画面がもう一度問い合わせられ、通されなくなった画面は（スタックの中でその上にある画面ごと）消え、ルーターはガードが送る先へ移動します。サインアウトしたユーザーを、行き先を付けてログイン画面へ送り、ログイン後にそこへ戻すには次のようにします。

```asuka
import std/url

state signedIn = false

app {
  Router {
    Route("/login", guard: to => if signedIn { "/" } else { "" }) { _ => Login() }
    // グループ：それ以外のすべて。
    Route(guard: to => if signedIn { "" } else { "/login?next={url.escape(to)}" }) {
      Route("/", layout: .stack) { Home }
    }
  }
}

screen Login {
  Button("Sign In") { on press { signedIn = true } }
}

screen Home at "" {
  title: "Home"
  Button("Sign Out") { on press { signedIn = false } }
}
```

どのルートも受け取らない href では、ホームへ戻る手段だけを持つ簡単な画面が表示されます。アプリ独自のキャッチオールのルート（`Route("/*path") { p => … }`）があれば、代わりにそちらが受け取ります。

### どこから始まり、どこに保存されるか

ルーターは、アプリを開いたリンクを受け取れるならそこから、なければウィンドウを前回離れた場所から、それもなければ `initial`（`/`）から始まります。状態全体（各スタックの画面、各タブの画面、シート）はアドレスとともに JSON として保存され、今のルートが一致する範囲で読み戻されます。

| | 保存される場所 |
|---|---|
| iOS、Android | 実行中のアプリの中。起動をまたいでは保存されません（ネイティブアプリのナビゲーションと同じです） |
| macOS、Windows、Linux | 各ウィンドウの位置と一緒に、アプリのデータディレクトリの `asuka-routes.json` に保存されます。メインウィンドウは常に、`Window` は `stateKey` があるときに保存され、次回はそこから開きます |
| Web | アドレスバーと履歴の各エントリー。戻る、進む、再読み込みで、それぞれが表す画面に戻ります（[Web](web.md#アドレスと履歴) を参照） |
| `asuka dev` | すべてのプラットフォーム。新しいコードを読み込み直すと、表示していた画面に戻ります |

ウィンドウの最初のルーターはそのウィンドウのアドレスを持ちます。`Window` の中のルーターは、その `Window` のアドレスを持ちます。

### 型付きのリンク

ルート付きの画面の呼び出しがそのアドレスになり、アプリのコンパイル時に検査されます。`navigate(Thread(id: 7))` と `href: Thread(id: 7)` は、`Thread` をルートに持つ唯一の `Router` から `/inbox/7` になります。画面のほかのパラメーターはクエリになります（`Thread(id: 7, sort: .oldest)` は `/inbox/7?sort=oldest`。エスケープされます）。どの `Router` もルートに持たない画面、二つの `Router` がルートに持つ画面、パスが必要とするパラメーターを省いた呼び出しは、その場でエラーになります。文字列で書いた href の正しさは、アプリの責任です。

テストでは、`mount(…, at: "/inbox/18")` でアドレスから始められ、`app.href`、`app.history()`、`app.goBack()`、`app.goForward()` でブラウザーのようにアドレスを追えます。

## ディープリンク

`asuka.toml` に記載しておくと、アプリは独自スキームの URL や、自分のドメインへの https リンクで開くようになります。

```toml
name = "Notes"
id = "com.example.notes"
scheme = "notes"
links = ["notes.example.com"]
```

- `scheme` を指定すると、`notes://anything` でアプリが開きます。対応するのは、iOS と Android、Mac（アプリの `Info.plist` でスキームを宣言します）、インストール後の Linux（デスクトップファイルがスキームを処理します。GNOME のアプリはそれぞれ単一インスタンスなので、アプリの実行中に開かれたリンクは実行中のアプリに届きます）、そして Windows（アプリは起動時に `HKEY_CURRENT_USER\Software\Classes` の下にユーザー単位でスキームを登録し、アプリが移動していれば登録し直します。アプリの実行中に開かれたリンクは実行中のアプリに届きます）です。`id` を持つアプリは、Linux と Windows では一つしか起動しません。もう一度起動すると、すでに動いているアプリが前面に出ます。`asuka dev` は Linux や Windows ではスキームを登録せず、その実行がインストール済みのアプリに処理を引き渡すこともありません。リンクは、ビルドしたアプリで試してください。
- `links` を指定すると、`https://notes.example.com/...` でアプリが開きます。iOS ではユニバーサルリンク、Android では App Links です。ドメインの側でもそれを宣言する必要があり、各プラットフォームのドキュメントに従って、iOS 向けに `apple-app-site-association` を、Android 向けに `assetlinks.json` を配信します。デスクトップでは、これらのリンクでアプリは開きません。

[ルーター](#ルート) があれば、これらのリンクにはコードがいりません。アプリのスキーム（`notes://inbox/18`。expo-router と同じくホストを最初のセグメントとして読むので `/inbox/18`）、ユニバーサルリンクや App Links のパス、通知の URL は、起動時でも実行中でも、それぞれのルートへ向かいます。`app` ブロックの `on openURL(url)` は、どのルートも受け取らない URL を受け取ります。

ルーターがなければ、`on openURL(url)` は、起動時でも実行中でも、アプリを開いた URL を一つずつ受け取ります（コンポーネントの本体の `onOpenURL` も、そのコンポーネントがある間は同じです）。アプリを起動した URL はハンドラーが登録される前に届くので、最初のハンドラーに渡されます。ハンドラーの中では、`push` を使うか、状態に書き込んで画面を切り替えてください。

```asuka
import std/url

state added: [string] = []

app {
  on openURL(raw) {
    if let u = url.parse(raw) {
      if u.host == "add" { added.push(u.query("text") ?? "") }
    }
  }
  Column {
    for (i, a) in added.enumerated() key i { Text(a) }
  }
}
```

[`examples/notes`](../../examples/notes) は、`asukanotes://add?text=...` でメモを追加します。通知の URL も、ユーザーが通知をタップしたときに、すべてのプラットフォームで同じ方法で届きます（[デバイスとサービス](device.md) を参照）。Web では、ルーターのアドレスがページのアドレスです。アプリのサイトへのリンクはその画面を開き、ページを開いても `on openURL` は実行されません。

## ほかのアプリからの共有

`asuka.toml` に `shareTarget = true` と書くと、ブラウザーや YouTube の「共有」のように、ほかのアプリの共有シートにアプリが表示され、テキストやリンクを受け取れるようになります。`shareTarget` をテーブルにして `types = ["image/*", ...]` を指定すると、ファイルも受け取れます（[デバイスとサービス](device.md#ほかのアプリからの共有onshare) を参照）。`app` ブロックの `on share(items)` は、共有された内容を受け取ります。`on openURL` と同じく、アプリを起動した共有は最初のハンドラーに渡されます。

```asuka
state shared: [SharedItem] = []

app {
  on share(items) {
    for it in items { shared.push(it) }
  }
  Column {
    for (i, it) in shared.enumerated() key i { Text("{it.kind}: {it.text}{it.path}") }
  }
}
```

iOS では、アプリに共有拡張機能（share extension）が付きます。この拡張機能は、Android と同じように、共有された内容を持ってアプリを開きます。拡張機能が独自の画面を表示することはありません。共有される内容は、タイトルとリンクの組であることがよくあります（Safari からは、ページのタイトルがテキストとして、その URL がリンクとして届きます）。[`examples/downloader`](../../examples/downloader) は、その中からリンクを探します。デスクトップと Web は共有先にならないので、そこでは `on share` は実行されません。

自分のアプリを開かずに、共有元のアプリの上に自分のアプリのシートを表示するには、[デバイスとサービス](device.md#アプリ独自の共有シートplatformsharesheet) の `platform/sharesheet` を参照してください。逆に自分のアプリから共有するには、`device.share(text)` でシステムの共有シートを表示し、`device.shareFiles(paths)` でファイルを共有します。Mac では共有メニュー、Windows では共有 UI、Web ではブラウザーの共有 UI になります。Linux には共有シートがないので、これらの関数は共有する内容をクリップボードにコピーし、そのことをトーストで知らせます。
