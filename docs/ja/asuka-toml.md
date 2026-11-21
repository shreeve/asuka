# asuka.toml

`asuka.toml` はアプリのディレクトリに置くファイルで、プラットフォームとストアが知っておくべきアプリの情報を書きます。アプリの名前と ID、アイコン、アプリで開くリンクとファイル、署名の鍵などです。`asuka dev`、`asuka build`、`asuka publish` がこのファイルを読みます。`asuka dev` は起動時に読むので、変更したら一度止めて実行し直してください。

ファイルは [TOML](https://toml.io) で書きます。`key = value` の行を並べ、`[name]` という見出しの下にテーブルを書きます。`#` から行末まではコメントです。

キーはすべて省略できます。このファイルがないアプリも、デフォルトの値でビルドして実行できます。Asuka が知らないキーは無視されるので、キーが効いていないように見えるときはつづりを確認してください。TOML として正しくない書き方や型の違う値があると、ビルドは止まり、その場所を行と列で示します（`asuka.toml:3:9: build: want an integer, not a string`）。Asuka が受け付けられない値の場合も、どのキーに何が求められているかを表示します（`asuka.toml: accent "red" (want #rrggbb or #rrggbbaa)`）。パスはアプリのディレクトリからの相対パスです。

このページではすべてのキーを紹介します。リリースのためにキーをどう組み合わせるかは[配布](shipping.md#asukatoml)で、各キーの働きの詳細は、下にリンクしたそれぞれのガイドで説明しています。

```toml
name = "Notes"
id = "com.example.notes"
version = "1.2.0"
build = 7
icon = "icon.png"
accent = "#7c3aed"
accentDark = "#a78bfa"
scheme = "notes"
links = ["notes.example.com"]
shareTarget = true

[[fileTypes]]
name = "Notes Document"
extensions = ["note"]

[splash]
image = "logo.png"
background = "#ffffff"
darkBackground = "#000000"

[ios]
team = "ABCDE12345"

[ios.usage]
NSCameraUsageDescription = "To scan your receipts."

[android]
keystore = "upload.jks"
keyAlias = "upload"
dynamicColor = true

[linux]
summary = "Write things down"
categories = ["Office"]
license = "MIT"

[updates]
publicKey = "…"
github = "you/notes"
```

## アプリ

これらのキーは、そのキーに意味があるすべてのプラットフォームで使われます。

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `name` | 文字列 | ディレクトリの名前 | アプリの名前です。アイコンの下、Mac のメニューバー、Windows のウィンドウのタイトル、Linux のランチャー、Web ページのタイトルに表示されます。ビルドで作られるファイルの名前にもなります（`Notes.app`、`Notes.exe`）。 |
| `id` | 文字列 | `dev.asuka.<directory>` | iOS と Mac では bundle ID、Android と Linux では application ID（Linux ではデスクトップファイル、アイコン、D-Bus の名前、データディレクトリに使われます）、Windows では AppUserModelID です。最初のリリースの前に変えてください。ストアとシステムはこの ID でアプリを識別します。Linux では英字、数字、`_`、`-` だけを使い、二つ以上の部分からなり、どの部分も数字で始まってはいけません。 |
| `version` | 文字列 | `1.0` | ユーザーに見えるバージョンです。 |
| `build` | 整数 | `1` | ビルド番号で、アップロードのたびに大きくする必要があります。`asuka publish` はストアにある最大の番号に 1 を足した番号を自動で付けるので、この値は開始の番号にすぎません。Windows のファイルバージョンは `version` と `build` を合わせたもの（`1.2.0.7`）になります。 |
| `icon` | パス | なし（プラットフォームの汎用アイコン） | 1024×1024 の正方形の PNG です。iOS 向けには透明部分を含めないでください。ビルドごとに、そのプラットフォームが必要とするサイズが作られます。App Store に出すには必須です。 |
| `accent` | 色 | プラットフォームの色 | `"#rrggbb"` または `"#rrggbbaa"`。iOS では tint、Android では Material 3 のカラースキームのシード、Mac では `Accent`、塗りつぶしのボタン、リンクの色（AppKit 自身のコントロールはユーザーの設定した色のまま）、Windows ではコントロールのアクセントカラー、Linux では libadwaita のアクセントカラー、Web ではページとマニフェストの `theme_color` になります。 |
| `accentDark` | 色 | `accent` | ダークモードのアクセントカラーで、書き方は `accent` と同じです。iOS、Mac、Windows、Linux、Web では、アプリがダークで表示されているあいだ（システムの外観に従っているときも、`appearance.setColorScheme(.dark)` を呼んだときも）、`.accent` とコントロールがこの色を使います。Android の Material 3 のカラースキームはダーク用の色も `accent` から作るので、この値は使いません。`accent` がなければ、ライトモードではプラットフォームの色になります。 |
| `orientations` | 文字列の配列 | 両方 | `"portrait"`、`"landscape"`。スマートフォンでアプリが対応する画面の向きです。iPad では Split View とステージマネージャに必要なので、四つの向きすべてに対応します。Android 16 では、タブレットと折りたたみ端末でこの指定は無視されます。 |
| `scheme` | 文字列 | なし | アプリの URL スキームです。iOS、Android、Mac、Windows、Linux で、`notes://` の URL がアプリを開き、アプリの `on openURL(url)` がその URL を受け取ります。英字、数字、`+`、`-`、`.` を使い、先頭は英字にします。[ディープリンク](navigation.md#ディープリンク)を参照してください。 |
| `links` | 文字列の配列 | なし | `example.com` のようなドメインで、その https のリンクがアプリを開きます。iOS のユニバーサルリンクと Android の App Links です。ドメイン側で関連付けのファイルを配信します。[ディープリンク](navigation.md#ディープリンク)を参照してください。 |
| `fileTypes` | テーブルの配列 | なし | アプリが開くファイルの種類です。[fileTypes](#filetypes) を参照してください。 |
| `shareTarget` | `true` またはテーブル | オフ | iOS と Android で、ほかのアプリの共有シートにアプリを表示します。[shareTarget](#sharetarget) を参照してください。 |
| `splash` | テーブル | なし | iOS と Android の起動画面です。[splash](#splash) を参照してください。 |
| `widgets` | テーブルの配列 | なし | iOS と Android のホーム画面ウィジェットです。[widgets](#widgets) を参照してください。 |
| `shortcuts` | テーブルの配列 | なし | iOS と Android で、Siri、Spotlight、ランチャーに出すアクションです。[shortcuts](#shortcuts) を参照してください。 |
| `nfc` | テーブル | なし | `platform/nfc` のタグに必要なものです。iOS の AID と FeliCa のシステムコード、Android でアプリを開くタグを指定します。[nfc](#nfc) を参照してください。 |
| `background` | 真偽値 | `false` | デスクトップで、トレイに常駐するアプリにします。起動時にウィンドウを開かず、ウィンドウがなくても動き続け、Mac では起動時から Dock にアイコンを出しません（`LSUIElement`）。[バックグラウンドのアプリ](desktop.md#バックグラウンドで動くアプリ)を参照してください。 |
| `keepRunning` | 真偽値 | `false` | デスクトップで、最後のウィンドウを閉じてもアプリを動かし続けます。メインウィンドウは、アプリがもう一度表示するまで隠れるだけです。`background` のアプリはもともと動き続けます。[バックグラウンドで動くアプリ](desktop.md#バックグラウンドで動くアプリ)を参照してください。 |
| `singleInstance` | 真偽値 | `true` | Windows と Linux で、二回目の起動はファイルを実行中のアプリに渡して（その `on openFiles(paths)` に）終了します。`false` にすると、起動した回数だけアプリが実行されます。Mac ではどちらの場合もアプリは一つしか実行されません。[単一インスタンス](desktop.md#単一インスタンス)を参照してください。 |
| `i18n` | テーブル | `locales`（あれば） | アプリのカタログの場所です。[i18n](#i18n) を参照してください。 |
| `updates` | テーブル | なし | デスクトップアプリの署名付きアップデートです。[updates](#updates) を参照してください。 |
| `ios` | テーブル | | iOS のビルドの設定と、Mac のビルドの設定の一部です。[ios](#ios) を参照してください。 |
| `mac` | テーブル | | Mac のビルドのプロファイルです。[mac](#mac) を参照してください。 |
| `icloud` | テーブル | `iCloud.<id>` | `platform/icloud` で使う、アプリの iCloud コンテナーと iCloud Drive のフォルダーです。[icloud](#icloud) を参照してください。 |
| `applePay` | テーブル | なし | `merchantIds = ["merchant.com.example"]`：Apple Pay（`platform/pay`）の支払いに使うマーチャント ID です。パッケージをインポートした iOS と Mac のアプリの、アプリ内決済のエンタイトルメントに入ります。[ウォレットでの支払い](device.md#ウォレット決済platformpay)を参照してください。 |
| `android` | テーブル | | Android のビルドの設定です。[android](#android) を参照してください。 |
| `linux` | テーブル | | Linux のビルドがアプリについて書き出す情報です。[linux](#linux) を参照してください。 |
| `web` | テーブル | | Web のビルドの設定です。[web](#web) を参照してください。 |
| `bluetooth` | テーブル | | `platform/bluetooth` を使うアプリが、パッケージに必要なもの以上に求めるものです。バックグラウンドと、スキャンによる位置情報を指定します。[bluetooth](#bluetooth) を参照してください。 |
| `bin` | テーブル | なし | プロジェクトのプログラムです。それぞれ専用のディレクトリにあるコマンドラインツールで、デスクトップアプリに組み込まれます。[bin](#bin) を参照してください。 |

Windows のビルドに必要なのはこれだけです。署名の証明書は環境変数で渡します（[配布](shipping.md#windows)を参照）。

## fileTypes

アプリが開くファイルの種類です。ファイルマネージャー、メールの添付ファイル、「このアプリケーションで開く」から開かれたファイルは、アプリの `on openFiles(paths)` が受け取ります。種類ごとに `[[fileTypes]]` のテーブルを一つ書きます。

```toml
[[fileTypes]]
name = "Markdown Document"
extensions = ["md", "markdown"]
role = "viewer"

[[fileTypes]]
name = "Notes Document"
extensions = ["note"]
icon = "note.png"
```

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `extensions` | 文字列の配列 | 必須 | ファイル名の拡張子で、ドットは付けません（`"md"`、`"tar.gz"`）。 |
| `name` | 文字列 | 最初の拡張子を大文字にして「Document」を付けたもの | システムが表示するファイルの種類の名前です。 |
| `mimeTypes` | 文字列の配列 | Asuka がその拡張子について知っている MIME タイプ | ファイルの MIME タイプです。Asuka が知らない拡張子では、アプリ独自のタイプ `application/x-<app>-<extension>` になります。 |
| `uti` | 文字列 | 既知の拡張子ならシステムのもの、それ以外は `<id>.<extension>` | Apple のプラットフォームでの Uniform Type Identifier です。アプリの ID の下にある UTI はアプリ独自のタイプとして扱われ、エクスポートされ、アプリがそのファイルの所有者になります。それ以外の、システムが知らないかもしれない UTI はインポートされます。 |
| `conformsTo` | 文字列の配列 | `public.data`、`public.content` | エクスポートまたはインポートするタイプが準拠する UTI です。`public.plain-text` を指定すると、Quick Look とテキストエディターがそのファイルを扱えるようになります。 |
| `role` | 文字列 | `"editor"` | `"editor"` か `"viewer"`。アプリがそのファイルで何をするかを表します（Mac と iOS の `CFBundleTypeRole`）。 |
| `icon` | パス | システムのアイコン、またはアプリのアイコン | 正方形の PNG で、Mac、Windows、Linux でのファイルのアイコンになります。 |

ファイルの種類の宣言方法はプラットフォームごとに異なります。Mac と iOS では Info.plist の `CFBundleDocumentTypes`、Android ではインテントフィルター、Windows ではアプリの起動時にユーザーのレジストリ、Linux ではデスクトップファイルと shared-mime-info のパッケージ、Web ではマニフェストの `file_handlers` で宣言します。[配布](shipping.md#asukatoml)と[ファイルを開く](desktop.md#ファイルを開く)を参照してください。

## shareTarget

iOS と Android で、ほかのアプリの共有シートにアプリを表示します。共有されたものはアプリの `on share(items)` が受け取ります。`shareTarget = true` とするとテキストと Web のリンクを受け取ります。ほかのものも受け取るときはテーブルで指定します。

```toml
[shareTarget]
types = ["text", "image/*", "video/*"]
max = 10
```

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `types` | 文字列の配列 | `["text"]` | `"text"`（テキストと Web のリンク）と、MIME タイプで指定するファイル（`"image/*"`、`"video/*"`、`"audio/*"`、`"application/pdf"`、何でも受け取るなら `"*/*"`）。 |
| `max` | 整数 | `1` | 一度に受け取るファイルの数で、1 以上です。 |

iOS では、アプリの中の共有拡張機能（share extension）になります。独自の bundle ID（アプリの ID に `.share` を付けたもの）を持ち、リリースでは `ios.shareProfile` で署名します。ファイルを受け取る場合は、App Group `group.<id>` も必要です。[配布](shipping.md#ios)と[ほかのアプリからの共有](device.md#ほかのアプリからの共有onshare)を参照してください。

## splash

iOS と Android の起動画面です。アイコンをタップしてからアプリの最初のフレームが出るまで（または `splash.Hide` を呼ぶまで）、単色の背景の中央に画像を表示します。デスクトップと Web には起動画面がありません。

```toml
[splash]
image = "logo.png"
size = 96
background = "#4A63B5"
darkBackground = "#1B2440"
```

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `image` | パス | なし | 正方形の PNG（ロゴ）。 |
| `size` | 整数 | `120` | 画像の幅をポイント（dp）で指定します。最大は 160 で、これが Android 12 のアイコンの円に収まる最大の大きさです。 |
| `background` | 色 | `"#ffffff"` | ライトモードでの背景色（`"#rrggbb"`）。 |
| `darkBackground` | 色 | `background` | ダークモードでの背景色（`"#rrggbb"`）。 |

[起動画面](device.md#起動画面platformsplash)を参照してください。

## widgets

iOS と Android でのアプリのホーム画面ウィジェットです。中身はアプリから `widget.set` で埋めます。

```toml
[[widgets]]
kind = "today"
name = "Today"
description = "What's next"
sizes = ["small", "medium", "rectangular"]
```

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `kind` | 文字列 | 必須 | `widget.Set` で使うウィジェットの名前です。英字、数字、`_` を使い、先頭は英字にします。ウィジェットごとに異なる名前にします。 |
| `name` | 文字列 | `kind` | ウィジェットギャラリーに表示される名前です。 |
| `description` | 文字列 | なし | ウィジェットギャラリーに表示される説明です。 |
| `sizes` | 文字列の配列 | `["small", "medium"]` | `small`、`medium`、`large`、そして iOS の `extraLarge`（iPad）と、ロック画面の `circular`、`rectangular`、`inline`。 |

iOS では、ウィジェットは独自の bundle ID（アプリの ID に `.widgets` を付けたもの）を持つ拡張機能になり、リリースでは `ios.widgetProfile` で署名します。[ホーム画面ウィジェット](device.md#ホーム画面ウィジェットplatformwidget)を参照してください。

## shortcuts

システムがアプリのために表示するアクションで、最大 10 個です。iOS の App Shortcuts（Siri、Spotlight、ショートカットアプリ、アクションボタン）と、Android のランチャーのショートカットになります。どれも、アプリの `on openURL(url)` に渡される URL でアプリを開きます。

```toml
[[shortcuts]]
id = "newNote"
title = "New Note"
url = "notes://new"
symbol = "square.and.pencil"
phrases = ["New note in ${app}"]
```

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `id` | 文字列 | 必須 | 英字、数字、`_` を使い、先頭は英字にします。ショートカットごとに異なる ID にします。 |
| `title` | 文字列 | 必須 | システムが表示する名前です。 |
| `url` | 文字列 | 必須 | アプリを開く URL です。`scheme` の URL か、`links` のどれかの URL にします。 |
| `symbol` | 文字列 | なし | `Symbol` が受け取るのと同じ、SF Symbol の名前です。 |
| `phrases` | 文字列の配列 | `"<title> in ${app}"` | Siri が聞き取るフレーズで、どれにもアプリの名前を表す `${app}` を含めます。iOS だけで使われます。 |

[ショートカット](device.md#ショートカットsirispotlightランチャー)を参照してください。

## nfc

`platform/nfc` が、権限以外にプラットフォームに求めるものです。パッケージをインポートしたアプリにだけ適用されます（インポートしないアプリには何も加わりません）。

```toml
[nfc]
iso7816 = ["D2760000850101"]
felica = ["8008"]
ndef = [
  {uri = "https://example.com/t/"},
  {mime = "application/vnd.example.ticket"},
  {external = "example.com:ticket"},
]
tech = [["IsoDep"], ["NfcA", "MifareUltralight"]]
```

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `iso7816` | 文字列の配列 | なし | iOS がカードを見つけたときに、この順に選択する ISO 7816 のアプリケーション（AID：5〜16 バイト、16 進数）です。Info.plist の `com.apple.developer.nfc.readersession.iso7816.select-identifiers` になります。`Poll` を使うセッションは、このどれかを持つカードだけを見つけます。 |
| `felica` | 文字列の配列 | なし | iOS がポーリングする FeliCa のシステムコード（16 進数 4 桁、`FFFF` 以外）です。`com.apple.developer.nfc.readersession.felica.systemcodes` になります。これがないと、FeliCa をポーリングするセッションは失敗します。 |
| `ndef` | テーブルの配列 | なし | Android で、メッセージの最初のレコードによってアプリを開くタグ（NDEF_DISCOVERED）です。それぞれ `uri`（スキーム、ホスト、パスのプレフィックス。URI はアプリの `on openURL(url)` に届きます）、`mime`（MIME タイプ。テキストのレコードは `text/plain`）、`external`（`domain:type`）のどれか一つを指定します。タグは `nfc.opened()` に届きます。 |
| `tech` | 文字列の配列の配列 | なし | Android で、技術によってアプリを開くタグ（TECH_DISCOVERED）です。各リストには、タグがすべて備えている必要のある `android.nfc.tech` のクラス（`IsoDep`、`NfcA`、`MifareUltralight` など）を並べます。 |
| `required` | 真偽値 | `false` | NFC のあるスマートフォン専用のアプリにします。Google Play は、NFC のない端末にはアプリを表示しません。 |

[NFC タグ](nfc.md#asukatoml-と権限)を参照してください。

## icloud

`platform/icloud` の書類の関数を呼ぶアプリの iCloud コンテナーと、それを iCloud Drive にどう表示するかの設定です（呼ばないアプリには何も加わりません）。

```toml
[icloud]
containers = ["iCloud.com.example.journal"]
driveName = "Journal"
```

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `containers` | 文字列の配列 | `["iCloud.<id>"]` | アプリの iCloud コンテナーで、最初のものがデフォルトになります。エンタイトルメントの `com.apple.developer.icloud-container-identifiers` と `ubiquity-container-identifiers` に入ります。アプリの App ID でも、iCloud ケイパビリティとともにこれらのコンテナーを有効にしておく必要があります。 |
| `driveName` | 文字列 | アプリの `name` | iCloud Drive での、最初のコンテナーのフォルダーの名前です（Info.plist の `NSUbiquitousContainers`）。 |
| `driveHidden` | 真偽値 | `false` | アプリの書類を iCloud Drive に表示しません。書類は同期されますが、見えるのはアプリだけです。 |

[iCloud](icloud.md#asukatoml-とエンタイトルメント)を参照してください。

## i18n

アプリのメッセージカタログの場所です。カタログが `locales` ディレクトリにあるアプリには不要です。

```toml
[i18n]
dir = "translations"
source = "ja"
```

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `dir` | パス | `locales` | カタログを入れるディレクトリです。カタログは言語ごとに一つで、BCP 47 のタグを名前にします（`en.json`、`pt-BR.json`、`zh-Hans.json`）。 |
| `source` | 文字列 | `en`、またはカタログが一つしかなければその言語 | アプリを書いている言語です。そのカタログにはすべてのメッセージがあり、ほかの言語のカタログはそれにフォールバックします。 |

[多言語対応](i18n.md#カタログ)を参照してください。

## updates

デスクトップアプリのアップデートの設定で、アップデートに署名する鍵と、アップデートを公開する場所を指定します。`asuka update keygen` が鍵ペアを作り、このテーブルを出力します。

```toml
[updates]
publicKey = "…the contents of asuka-update.pub…"
github = "you/notes"
```

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `publicKey` | 文字列 | 必須 | `asuka-update.pub` の中身で、アプリに組み込まれます。アプリは、秘密鍵で署名されたアップデートしか受け付けません。 |
| `github` | 文字列 | | `"owner/name"`。アップデートを置く公開リポジトリで、`v1.2.0` のようなタグを付けたリリースにアップデートを入れます。 |
| `tagPrefix` | 文字列 | `"v"` | リリースのタグの接頭辞です。`"desktop-v"` のように別の接頭辞にすると、リポジトリの最新リリースが別のもののリリースであってもかまいません。その場合、アプリは GitHub API を通じて、この接頭辞のタグを持つ最新のリリースを探します。 |
| `url` | 文字列 | | `github` の代わりに、ファイルを配信する HTTPS のディレクトリ（S3 や R2 のバケット、CDN）を指定します。アプリは `<url>/update-<target>.json` を読みます。 |
| `feed` | 文字列 | なし | アプリが代わりに問い合わせる独自の HTTPS サーバーで、`{target}`、`{version}`、`{channel}` を含む URL で指定します。ファイルの置き場所として `url` も必要です。 |
| `s3` | テーブル | なし | `url` を配信するバケットで（`[updates.s3]`）、`asuka publish` がここにアップロードします。`bucket`（必須）、`prefix`（ディレクトリ。デフォルトはバケットのルート）、`region`（デフォルトは `AWS_REGION`、なければ `AWS_DEFAULT_REGION`、それもなければ `us-east-1`）、`endpoint`（R2 や MinIO のもの。デフォルトは Amazon S3）、`pathStyle`（デフォルトでは、`endpoint` を指定したとき、またはバケット名にドットを含むときに有効）を指定します。 |
| `privateKey` | パス | なし | 秘密鍵のパスで、自分のマシンでビルドするときに使います（`~/` はホームディレクトリです）。 |
| `changelog` | パス | `CHANGELOG.md`（あれば） | Markdown のファイルで、その `## 1.2.0` のセクションがリリースノートになります。 |
| `deltas` | 整数 | `3` | 差分アップデート（変更された部分だけをダウンロードするアップデート）を提供する、さかのぼるバージョンの数です。`0` なら提供しません。 |

`github` と `url` のどちらか一方が必要で、両方は指定できません。[アップデート](shipping.md#自動アップデート)を参照してください。

## ios

iOS のビルドの設定です。`team` は Mac のビルドのチームにもなり、`usage` の利用目的の文字列は Mac の Info.plist にも入ります。

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `team` | 文字列 | なし | アプリに署名する Apple Developer のチーム ID（`ABCDE12345`）です。 |
| `profile` | 文字列 | インストール済みのうち最も合うもの | アプリのプロビジョニングプロファイルで、パス、名前、UUID のいずれかで指定します。`--profile` で上書きできます。 |
| `shareProfile` | 文字列 | インストール済みのうち最も合うもの | 共有拡張機能（`shareTarget`）のプロファイルで、ID `<id>.share` 用です。 |
| `widgetProfile` | 文字列 | インストール済みのうち最も合うもの | ウィジェット拡張機能（`widgets`）のプロファイルで、ID `<id>.widgets` 用です。 |
| `notificationProfile` | 文字列 | インストール済みのうち最も合うもの | Notification Service Extension（`push.Rewrite`）のプロファイルで、ID `<id>.notification` 用です。 |
| `usage` | テーブル | 各パッケージのもの | 保護されたリソースの利用目的の文字列を、Info.plist のキーごとに指定します（`[ios.usage]` に `NSCameraUsageDescription = "To scan your receipts."`）。アプリがインポートするパッケージが汎用的な文字列を用意していますが、自分で書いてください。 |
| `infoPlist` | テーブル | なし | Info.plist に追加する文字列のキーです。 |
| `backgroundModes` | 文字列の配列 | なし | Info.plist の `UIBackgroundModes` に追加します。`"audio"` を指定すると、画面がロックされていても録音と再生ができます。 |
| `iphoneOnly` | 真偽値 | `false` | iPad を対象から外します。iPad ではアプリは iPhone アプリとして、スマートフォンサイズのウィンドウで動きます。 |
| `storekit` | 文字列 | なし | StoreKit の構成ファイル（Xcode の `.storekit`）です。Xcode のスキームと同じように、`asuka dev` のシミュレーター向けビルドはこのファイルの商品を販売します。[アプリ内課金](purchases.md#購入のテスト)を参照してください。 |
| `nonExemptEncryption` | 文字列 | `"no"` | ビルドが暗号化について申告する内容（`ITSAppUsesNonExemptEncryption`）です。これは法的な申告で、内容は開発者が判断します。`"no"`、`"yes"`、または何も申告しない `"ask"` を指定します。`"ask"` の場合は、アップロードのたびに App Store Connect が尋ねます。 |
| `collected` | テーブルの配列 | なし | アプリが収集するデータで、プライバシーマニフェストに使います（下記を参照）。 |
| `appStoreConnect` | テーブル | | `asuka publish` がアップロードに使う App Store Connect API キーです。`keyId`、`issuerId`（個人キーでは不要）、`key`（`.p8` のパス。デフォルトは、`altool` と同じく `~/.appstoreconnect/private_keys` にある `AuthKey_<keyId>.p8`）を指定します。[公開](shipping.md#ストアへの公開)を参照してください。 |

`collected` の各要素（`[[ios.collected]]`）はデータの種類で、Apple の名前から接頭辞を除いたもので指定します。これは Asuka が書き出すプライバシーマニフェストに使われます（アプリ独自の `ios/PrivacyInfo.xcprivacy` があれば、Asuka のマニフェストとこの設定はそれに置き換わります）。

```toml
[[ios.collected]]
type = "EmailAddress"
linked = true
tracking = false
purposes = ["AppFunctionality"]
```

| キー | 型 | 説明 |
|---|---|---|
| `type` | 文字列 | `"EmailAddress"` は `NSPrivacyCollectedDataTypeEmailAddress` を表します。 |
| `linked` | 真偽値 | ユーザーの身元と結び付けられるかどうか。 |
| `tracking` | 真偽値 | ユーザーのトラッキングに使われるかどうか。 |
| `purposes` | 文字列の配列 | `"AppFunctionality"` は `NSPrivacyCollectedDataTypePurposeAppFunctionality` を表します。 |

[配布](shipping.md#ios)を参照してください。

## mac

Mac のビルドがアプリの署名に使うものです。チームは `ios.team` です。

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `profile` | 文字列 | `ASUKA_MAC_PROFILE`。なければ、プロファイルが必要なアプリではインストール済みのうち最も合うもの | Developer ID（または開発用）のプロビジョニングプロファイルのパス、名前、UUID です。プッシュ通知、Sign in with Apple、App Attest に必要です。 |
| `appStoreProfile` | 文字列 | インストール済みのうち最も合うもの | `--appstore` で使う Mac App Store のプロファイルです。 |

`--profile` と `ASUKA_MAC_PROFILE` で上書きできます。[配布](shipping.md#macos)を参照してください。

## android

Android のビルドの設定です。

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `keystore` | パス | なし | リリースに署名するアップロード鍵のキーストアです。キーストアのパスワードは `ASUKA_KEYSTORE_PASSWORD`、鍵のパスワードは `ASUKA_KEY_PASSWORD`（デフォルトはキーストアと同じ）で渡します。 |
| `keyAlias` | 文字列 | なし | キーストアの中のアップロード鍵のエイリアスです。 |
| `dynamicColor` | 真偽値 | `false` | Material のアプリと同じように、ユーザーの壁紙から色を取ります（Android 12 以降）。それ以外の環境では `accent` がシードになります。 |
| `permissions` | 文字列の配列 | 各パッケージのもの | マニフェストに追加する権限です（`"android.permission.CAMERA"`）。`"<name>:28"` と書くと、Android 9（API 28）までだけ権限を要求します。アプリがインポートするパッケージは、必要な権限を自分で追加します。 |
| `googleServices` | パス | なし | `platform/push` で使う、Firebase プロジェクトの `google-services.json` です。 |
| `mapsKey` | 文字列 | なし | `platform/maps` で使う、Google Maps Platform の API キー（Maps SDK for Android）です。 |
| `serviceAccount` | パス | なし | `asuka publish` が Google Play へのアップロードに使う、Google Cloud のサービスアカウントの JSON キーです。リポジトリには入れないでください。 |

[配布](shipping.md#android)を参照してください。

## linux

Linux のビルドが、デスクトップファイルと AppStream の metainfo に書き出すアプリの情報です。ランチャー、ソフトウェアセンター、Flathub に表示されます。Flathub とソフトウェアセンターは、概要、説明、ライセンス、開発者を求めます。

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `summary` | 文字列 | なし | アプリが何をするかを一行で書きます。 |
| `description` | 文字列 | なし | 空行で区切った数段落の説明です。 |
| `categories` | 文字列の配列 | `["Utility"]` | [freedesktop.org のメインカテゴリー](https://specifications.freedesktop.org/menu-spec/latest/category-registry.html)：`Utility`、`Office`、`Development`、`Network` など。 |
| `keywords` | 文字列の配列 | なし | ランチャーでアプリを見つけるための追加のキーワードです。 |
| `license` | 文字列 | なし | アプリのライセンスを SPDX 式で書きます（`MIT`、`GPL-3.0-or-later`、`LicenseRef-proprietary`）。 |
| `developer` | 文字列 | なし | 開発者の名前です。 |
| `homepage` | 文字列 | なし | アプリの Web ページです。 |
| `flatpakRuntime` | 文字列 | 最近のもの | Flatpak のマニフェストが要求する GNOME ランタイムのバージョンで、46 以降です。 |

[配布](shipping.md#linux)を参照してください。

## web

Web のビルドの設定です。

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `vapidPublicKey` | 文字列 | なし | アプリのサーバーの VAPID 公開鍵（base64url）です。ブラウザーは、`platform/push` の Web Push を購読するときにこれを使います。 |
| `base` | 文字列 | `"/"` | サイトを配信するパスです（`https://example.com/app/` なら `"/app/"`）。ページの `<base href>` になり、ルーターの href はその下のアドレスになります。`asuka build --base` で上書きできます。[アドレスと履歴](web.md#アドレスと履歴)を参照してください。 |
| `colors` | テーブル | Asuka の色 | ページのパレットで、ライトとダークがあります。下の表を見てください。[パレット](web.md#パレット)も参照してください。 |

[Web Push](web.md#platformpushweb-push) を参照してください。

`[web.colors]` は、Web のホストが描画に使う色を設定します。どれも `"#rrggbb"`
または `"#rrggbbaa"` で書きます。それぞれの色には、名前に `Dark` を付けた
ダークモード用のキー（`labelDark` など）もあり、ページがダークで表示されているあいだ（ブラウザーの
`prefers-color-scheme`、または `appearance.setColorScheme(.dark)`）はそちらが使われます。書かなければ、どちらのモードでも同じ色です。書かなかった色は Asuka の色のままです。これは Web だけの設定で、iOS、Android、Mac、Windows、Linux はシステムの色を使い続けます。

```toml
[web.colors]
label = "#16181d"
labelDark = "#e8e9eb"
background = "#ffffff"
backgroundDark = "#101114"
separator = "#dfe1e5"
separatorDark = "#3a3d42"
```

| キー（と `…Dark`） | 使われる場所 |
|---|---|
| `label` | `.label` と、色を指定していない文字：Text、フィールドの文字、Picker の文字 |
| `secondaryLabel` | `.secondaryLabel`：アラートのメッセージ、タブバーの選ばれていないタブ、メニューとサイドバーの見出し |
| `tertiaryLabel` | `.tertiaryLabel`：フィールドのプレースホルダー、無効なバーのボタンとメニュー項目、シートのグラバー |
| `background` | `.background`：ページ、画面、リスト、シート、枠付きボタンの塗り（とブラウザーのテーマカラー） |
| `secondaryBackground` | `.secondaryBackground`：Split のサイドバー、ウィンドウのタイトルバー |
| `groupedBackground` | `.groupedBackground`：グループ化したリストの背景 |
| `secondaryGroupedBackground` | `.secondaryGroupedBackground`：グループ化したリストの行 |
| `separator` | `.separator`：リストの行の区切り線、バーの境界、枠付きボタンと Picker の枠、Stepper の仕切り、メニューの区切り |
| `card` | 浮き上がった面：アラート、ポップオーバーとメニュー、Window のパネル、セグメンテッドコントロールの選ばれたセグメント |
| `bar` | ヘッダーバー、タブバー、ツールバーの背景と、スナックバーの背景 |

## bluetooth

`platform/bluetooth` をインポートしたアプリが、パッケージが与えるもの（権限と利用目的の説明文）以上に求めるものです。パッケージをインポートしないアプリでは何もしません。

```toml
[bluetooth]
background = ["central"]
location = false
```

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `background` | 文字列の配列 | なし | アプリがバックグラウンドで続ける役割で、iOS の状態の復元（`bluetooth.restored()`）も有効になります。`"central"`：iOS の `bluetooth-central` モード（`backgroundScan` を呼ぶアプリには指定しなくても付きます）。`"peripheral"`：iOS の `bluetooth-peripheral` モード。Android ではどちらも不要です。`backgroundScan` を呼ぶアプリには、スキャンのレシーバーが与えられます。 |
| `location` | 真偽値 | `false` | アプリがスキャン（ビーコン）からユーザーの居場所を割り出す場合に指定します。Android の `BLUETOOTH_SCAN` に `neverForLocation` を付けず、`bluetooth.request()` は位置情報の権限も求めるようになります（iOS：`NSLocationWhenInUseUsageDescription`）。 |

[Bluetooth](bluetooth.md#ビルドが加えるもの)を参照してください。

## c

アプリの `extern` 宣言が呼び出す C です。ターゲットごとにアプリへ何をコンパイルし、何をリンクするかを書きます。各プラットフォームのテーブル（`[c.ios]`、`[c.android]`、`[c.macos]`、`[c.windows]`、`[c.linux]`、`[c.web]`）は、トップレベルのテーブルのリストに追加されます。パッケージの `asuka.toml` には独自の `[c]` があり、パッケージをインポートしたアプリはそれもビルドします。

| キー | 型 | デフォルト | 説明 |
|---|---|---|---|
| `sources` | 文字列の配列 | なし | C のファイルです。`asuka.toml` からの相対パスの glob で指定します（`c/*.c`。`**` は任意の階層のディレクトリにマッチします）。どのファイルにもマッチしない glob があると、ビルドは止まります。 |
| `include` | 文字列の配列 | なし | ソースがインクルードするディレクトリです（`-I`）。`asuka_ffi.h` は常にインクルードできます。 |
| `defines` | 文字列の配列 | なし | マクロです。`NAME` または `NAME=value` と書きます（`-D`）。 |
| `flags` | 文字列の配列 | なし | コンパイラーに渡す追加のフラグです。 |
| `libraries` | 文字列の配列 | なし | リンクするライブラリです。名前（`sqlite3` なら `-lsqlite3`）か、`asuka.toml` からの相対パスで指定したファイル（`lib/libfoo.a`）を書きます。 |
| `frameworks` | 文字列の配列 | なし | iOS と macOS でリンクする Apple のフレームワークです（`Security`）。 |

`[c.prebuilt]` には、ベンダーがターゲットごとにビルドしたライブラリを書きます。キーはターゲット（`ios-arm64`、`ios-simulator-arm64`、`ios-simulator-x64`、`macos-arm64`、`macos-x64`、`android-arm64`、`android-arm`、`android-x64`、`linux-x64`、`linux-arm64`、`windows-x64`、`windows-arm64`、`web`。ユニバーサルライブラリには `ios-simulator` と `macos`）、値は `asuka.toml` からの相対パスで書いた静的ライブラリ（`.a`。Windows では `.a` か `.lib`）か、そのリストです。ここにライブラリのないターゲットをビルドすると、その旨を表示して止まります。

```toml
[c]
sources = ["c/*.c"]
include = ["c/include", "vendor/acme/include"]

[c.ios]
libraries = ["z"]

[c.web]
defines = ["NO_ZLIB"]

[c.prebuilt]
ios-arm64 = "vendor/acme/ios-arm64/libacme.a"
ios-simulator = "vendor/acme/ios-simulator/libacme.a"
android-arm64 = "vendor/acme/android-arm64/libacme.a"
```

`[c.download.<name>]` は、プロジェクトに置く代わりにビルドがダウンロードするアーカイブ（ライブラリのソースや、ベンダーのビルド済みライブラリ）です。`url`（`https://` で、`.zip`、`.tar.gz`、`.tgz` のいずれか）、`sha256`（アーカイブのもの。違うものは拒否されます）、`strip`（パスの先頭から取り除くディレクトリの数。既定は 0）を書きます。`[c]` の中で `$<name>` から始まるパスはその中を指します（`$sqlite/sqlite3.c`。`$sqlite` だけなら最上位ディレクトリ）。それを必要とする最初のビルドが、一度だけ Asuka のキャッシュにダウンロードします。

```toml
[c]
sources = ["$sqlite/sqlite3.c"]
include = ["$sqlite"]

[c.download.sqlite]
url = "https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip"
sha256 = "0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9"
strip = 1
```

[Asuka から C を呼び出す](c.md#ビルド)、[ダウンロードするソース](c.md#ダウンロードするソース)、[ビルド済みのライブラリ](c.md#ビルド済みのライブラリ)を参照してください。

## bin

プロジェクトの[プログラム](programs.md)です。`[bin.<name>]` のそれぞれが、専用のディレクトリにある `fn main` を持つパッケージ、つまりコマンドラインツールで、`<name>` が実行ファイルの名前になります（英数字、`-`、`_`、`.`）。プロジェクト自体がプログラム（`asuka new --bin`）なら不要です。

| キー | 型 | デフォルト | 内容 |
|---|---|---|---|
| `path` | パス | `path` か `build` | プログラムのディレクトリ。`asuka.toml` からの相対パスです。プログラムはプロジェクトのパッケージを相対パスでインポートします（`import ../model`）。 |
| `build` | 文字列の配列 | `path` か `build` | `path` の代わりに、ほかのツールチェーンが作るプログラム（アプリに同梱する Go や Rust のツール）のビルドコマンド。ターゲットごとにプロジェクトのディレクトリで実行されます。`{out}` は実行ファイルの書き出し先です。ターゲットのプレースホルダーは下の表のとおりです。 |
| `env` | テーブル | なし | `build` の環境変数。CLI の環境に上書きされ、値の中の同じプレースホルダーが置き換えられます。 |
| `output` | パス | `{out}` | `build` が実行ファイルを置く場所。`asuka.toml` からの相対パスです。出力先を自分で決めるツールチェーン（cargo の `target/<triple>/release`）のためのもので、CLI はそこから取り出します。 |

`build`、`env`、`output` の中のターゲットは、ツールチェーンごとの名前で書きます。

| プレースホルダー | Mac（Apple シリコン、Intel） | Linux | Windows |
|---|---|---|---|
| `{os}` | `macos` | `linux` | `windows` |
| `{arch}` | `arm64`、`x64` | `arm64`、`x64` | `arm64`、`x64` |
| `{exe}` | | | `.exe` |
| `{target}`（Rust と clang のターゲットトリプル、そのシステム上でビルド） | `aarch64-apple-darwin`、`x86_64-apple-darwin` | `aarch64-unknown-linux-gnu`、`x86_64-unknown-linux-gnu` | `aarch64-pc-windows-msvc`、`x86_64-pc-windows-msvc` |
| `{target}`（ほかのシステムからクロスビルド） | | `aarch64-unknown-linux-musl`、`x86_64-unknown-linux-musl`（静的リンク） | `aarch64-pc-windows-gnullvm`、`x86_64-pc-windows-gnu`（MinGW：Windows SDK は不要） |
| `{goos}`、`{goarch}`（Go の名前） | `darwin`；`arm64`、`amd64` | `linux`；`arm64`、`amd64` | `windows`；`arm64`、`amd64` |

`{target}` は Asuka 自身のツールチェーンが選ぶトリプルと同じです。そのシステム上でビルドするときはそのシステムのもの（Windows では MSVC のもので、rustup がそこで標準でインストールするもの）、クロスビルドでは Zig のものになります。コマンドの環境変数にも同じ値が入るので、スクリプトから使えます：`ASUKA_OUT`、`ASUKA_OS`、`ASUKA_ARCH`、`ASUKA_EXE`、`ASUKA_TARGET`、`ASUKA_GOOS`、`ASUKA_GOARCH`。別の名前やフラグが必要なビルド（Linux でも musl を使う、あるシステムだけで feature を有効にするなど）は、`build` でスクリプトを実行し、実行時にスクリプトに決めさせます。[ほかのツールチェーンがビルドするプログラム](programs.md#ほかのツールチェーンがビルドするプログラム)を参照してください。

```toml
[bin.notes]
path = "cli"

[bin.notes-sync]
path = "sync"

[bin.notes-server]
build = ["go", "build", "-o", "{out}", "./cmd/server"]
env = { GOOS = "{goos}", GOARCH = "{goarch}", CGO_ENABLED = "0" }

[bin.notes-index]
build = ["cargo", "build", "--release", "--manifest-path", "index/Cargo.toml", "--target", "{target}"]
output = "index/target/{target}/release/notes-index{exe}"
```

`asuka run --bin notes` と `asuka build --bin notes` で 1 つを実行・ビルドします（1 つだけなら `--bin` は省略できます）。`asuka test` はアプリのテストと一緒に各プログラムのテストも実行します。デスクトップのビルドはプログラムをアプリに入れます。Mac では `Contents/Helpers`、Linux では `bin`、Windows では `.exe` の隣です。スマートフォンと Web は無視し、`asuka dev` は起動時に一度ビルドします。アプリは `command.bundled(name)`（std/command）でそれを見つけます。`asuka run`、`test`、`build --bin` は Asuka のプログラムを扱い、`build` で作るプログラムはそのツールチェーンで実行・テストします。アプリに付属するプログラムは、データをアプリと同じ場所に保存します。[アプリに付属するプログラム](programs.md#アプリに付属するプログラム)を参照してください。

## 環境変数

秘密の情報は `asuka.toml` に書かず、環境変数で渡してください。CI では CI のシークレットから渡します。これらの環境変数は、対応するキーを上書きもします。

| 変数 | 説明 |
|---|---|
| `ASUKA_KEYSTORE`、`ASUKA_KEY_ALIAS` | `android.keystore` と `android.keyAlias` を上書きします。 |
| `ASUKA_KEYSTORE_PASSWORD`、`ASUKA_KEY_PASSWORD` | キーストアのパスワードと、それと異なる場合の鍵のパスワードです。 |
| `ASUKA_ASC_KEY_ID`、`ASUKA_ASC_ISSUER_ID`、`ASUKA_ASC_KEY` | `ios.appStoreConnect` を上書きします。`ASUKA_ASC_KEY` は `.p8` のパスかその中身です。 |
| `ASUKA_PLAY_SERVICE_ACCOUNT` | `android.serviceAccount` を上書きします。キーのパスか、その JSON です。 |
| `ASUKA_MAC_PROFILE` | `mac.profile` を上書きします。 |
| `ASUKA_UPDATE_PUBLIC_KEY`、`ASUKA_UPDATE_URL` | `updates.publicKey` と `updates.url` を上書きします（`url` を指定すると `github` は使われなくなります）。 |
| `ASUKA_UPDATE_PRIVATE_KEY` | アップデートの秘密鍵そのもの、またはそのパスで、`updates.privateKey` を上書きします。 |
| `ASUKA_WINDOWS_CERT`、`ASUKA_WINDOWS_CERT_PASSWORD`、`ASUKA_WINDOWS_TIMESTAMP` | Windows のビルドの署名証明書（`.pfx`）、そのパスワード、タイムスタンプサーバーです。これらは `asuka.toml` には書けません。[配布](shipping.md#windows)を参照してください。 |
