# Web

同じアプリが WebAssembly としてブラウザーでも動き、ビューはページの DOM になります。Web は iOS、Android、Mac、Windows、Linux と並ぶ Asuka のプラットフォームの一つで、同じコードから動きます。各ビューは要素になり、Asuka のレイアウト（flexbox と grid）が決めた位置に配置されるので、どのプラットフォームでも同じになります。テキストやコントロールのサイズは、ほかのプラットフォームと同じく、実際の要素で測ります。コントロールはブラウザー自身の要素（`<button>`、`<input>`、`<select>`、`<progress>`、`<dialog>`）なので、キーボード操作、フォーカス、自動入力、スペルチェック、スクリーンリーダーへの対応も最初から備わっています。

```sh
asuka dev --platform web examples/counter       # サーバーを起動してブラウザーで開き、保存するとリロード
asuka build --platform web examples/counter     # 静的サイトを examples/counter/dist/web に出力
```

どちらも WebAssembly のビルドに [Zig](https://ziglang.org/download) を使うので、インストールしておく必要があります。

以下では、Web で何ができるかを説明します。ブラウザーではスマートフォンと同じことができない場合は、代わりにどうなるかも説明します。

## asuka dev --platform web

`asuka dev --platform web` は、開発用アプリを一度だけビルドし、ページを `http://localhost:8790/`（または空いているポート）で配信して、ブラウザーで開きます。保存するたびにアプリが数ミリ秒でコンパイルし直され、ページは新しいバージョンをその場で読み込みます。ほかのプラットフォームの開発用アプリと同じく、アプリの状態はそのまま残ります。リロードのたびにターミナルに報告が出ます（`asuka: reloaded in 1.1 ms, 3 states kept`）。アプリの出力とページのエラーはターミナルに表示されます。保存した内容がコンパイルできないときや、アプリが捕捉しなかったエラーが起きたときは、ページの上に表示されます（[アプリに重なるエラー表示](development.md#アプリに重なるエラー表示)）。`asuka.toml`、アセット、ネイティブパッケージの JavaScript を変更すると、ページがリロードされます（アプリは最初からやり直しになります）。

`asuka dev` 共通のフラグ（`--lang`、`--dir`、`-v`）のほかに、次のフラグがあります。

- `--port`：サーバーのポート。
- `--lan`：ローカルネットワークにも配信し、そのアドレスを表示します。最初のアドレスは QR コードでも表示されるので、スマートフォンのカメラで読み取ればブラウザーでアプリが開きます。
- `--no-open`：ブラウザーを開きません。
- `--once`：配信するだけで、ファイルの変更は監視しません。

## asuka build --platform web

`asuka build --platform web` は、アプリを WebAssembly としてビルドして、`dist/web` に静的サイトを作ります。中身は次のとおりです。

- `index.html` と `manifest.webmanifest`（Web アプリマニフェスト。これがあると、ブラウザーがアプリのインストールを提案します。アイコンは `asuka.toml` の `icon` から作られ、ファビコンと iOS のホーム画面アイコンも含まれます）。
- アプリ本体の `asuka.<digest>.wasm`。最適化され、シンボルが取り除かれて（stripped）います。
- `asuka/<digest>/` 内の Asuka 自身のファイル（`std/sqlite` をインポートするアプリでは、SQLite の WebAssembly モジュールも含まれます）と、ネイティブパッケージのスクリプト。
- アプリの service worker である `sw.<digest>.js`。`index.html` と同じ場所に置かれ（スコープはサイトのフォルダー）、アプリのページが一つも開いていない間も、アプリの通知とプッシュ通知を表示します（[通知と Web Push](#通知と-web-push)）。
- アプリの `assets/`。
- 各スクリプトと wasm の gzip 圧縮版（`.gz`）。圧縮済みのファイルを配信できるサーバー向けです。
- `_headers`。Cloudflare と Netlify がこれを読みます。`index.html` とマニフェストは `no-cache`、ダイジェストを名前に含むファイルは無期限にキャッシュされるよう指定しています。
- `_redirects`。Netlify がこれを読みます。ファイルのないパスはアプリのページになるので、[ルーター](#アドレスと履歴)のアドレスでアプリが開きます。

`index.html` の `<base href>` は、サイトを配信するパスです。デフォルトは `/` で、`asuka.toml` の `web.base` か `--base` で変えられます（`https://example.com/app/` なら `"/app/"`）。ページはその下のどのアドレスからでも自分のファイルを見つけられ、ルーターの href はその下のページのアドレスになります（`/chat/7` は `https://example.com/app/chat/7`）。

このフォルダーは、どんな Web サーバーや CDN からでも配信できます。内容のダイジェストを名前に含むファイルは、内容が変わると名前も変わるので、無期限にキャッシュできます。再取得が必要なのは `index.html` だけです。`.wasm` は `application/wasm` として配信してください。カウンターアプリの WebAssembly は約 300 KB、gzip 圧縮で 115 KB で、その横に Asuka の JavaScript があります。これにアプリ自身のコードの分が加わります。

## アドレスと履歴

[`Router`](navigation.md#ルート) を使うと、ページのアドレスが表示中の画面を表します。`#` のフラグメントではなく、`pushState` と `replaceState` でサイトのベースの下の実際のパスを使います。ルーターのナビゲーションの各ステップはページの履歴のエントリーになり、ルーターの状態全体（各スタックの画面、各タブの画面、シート）を保持します。そのため、次のようになります。

- ブラウザーの戻ると進むで、スタック、タブ、シートを行き来し、各エントリーが表す画面に戻ります。ブラウザーで戻るときは、まず画面やシートの `onBack` に確認し、そこで戻る操作が処理されれば、ページはその場にとどまります。
- リロードすると、いちばん上の画面の下にある画面も含めて、同じ画面に戻ります。貼り付けたリンクやほかの場所から開いたリンクは、それが表す画面を組み立てます（[リンクの下の画面](navigation.md#リンクの下の画面)）。
- ページのタイトルは、いちばん上の画面のタイトルに追従します（`Title · App`）。

履歴は、react-navigation の linking と同じように動きます。より深く進むナビゲーション（プッシュ、シート、最初以外のタブ）はエントリーを追加します。戻るナビゲーションは、そのアドレスを持つ最新のエントリーに戻ります（履歴にそのエントリーが一度もなかった場合、たとえばディープリンクの画面を一つずつポップした場合は、エントリーを置き換えます）。それ以外（`replace(href)`、リダイレクト、ガードによる別の画面への誘導、最初以外のタブ同士の切り替え、`router.setQuery`）はエントリーを置き換えます。アプリ内の戻るボタン、Escape、Alt+← はスタックをポップし、あわせて履歴を戻ります。`Alert`、アプリ独自の `Sheet`、ルーターが扱わないスタックは、ルーターがない場合と同じく、ブラウザーの戻る操作のために自分のエントリーを持ちます。

ページの要素の中で動くアプリ（[埋め込み](#埋め込み)）は、ページのアドレスを変えません。ルーターのアドレスはアプリの内部にとどまります。`Window` パネルの中のルーターは、そのアドレスをパネルの `stateKey` とともに `localStorage` に保存します。アプリ自身のサイトを指す通知の URL は、その画面を開きます。

### すべてのアドレスを配信する

リロードや貼り付けたリンクでは、サーバーにそのアドレス自体（`/app/chat/7`）が要求されますが、そこにファイルはありません。サーバーはアプリの `index.html` を返す必要があります。`asuka dev --platform web` はそうしています。静的サイトの場合は次のとおりです。

| | |
|---|---|
| Netlify | ビルドが書き出す `_redirects` |
| Cloudflare Pages | 何も要りません。`404.html` のないサイトはシングルページアプリケーションとして配信されます（`_redirects` のルールがループしていると警告し、そのルールは無視します） |
| Vercel | `vercel.json`：`{"rewrites": [{"source": "/(.*)", "destination": "/index.html"}]}`（ベースの下で） |
| Firebase Hosting | `firebase.json` の `hosting.rewrites`：`[{"source": "**", "destination": "/index.html"}]` |
| GitHub Pages | `index.html` のコピーを `404.html` として置きます（どのアドレスに対しても、404 のステータスで返されます）。プロジェクトサイトでは `web.base` をリポジトリのパスにします |
| nginx | サイトの `location /app/` に `try_files $uri /app/index.html;` |
| Caddy | `file_server` ブロックに `try_files {path} /app/index.html` |
| S3 と CloudFront | ディストリビューションのエラーページで、403 と 404 にステータス 200 で `/index.html` を返します |

## ページから得られる環境

Web アプリから見える環境（`environment()`）は、ブラウザーのものです。

- `platform` は `.web` です。
- サイズはページ（アプリを要素の中で動かしている場合はその要素）のサイズで、ウィンドウに合わせて変わります。サイズクラスは Material のもので、幅から決まります。
- セーフエリアはページのもの（`env(safe-area-inset-*)`。ページが `viewport-fit=cover` のときの、スマートフォンのノッチやホームインジケーターの分）です。キーボードは、スマートフォンのキーボードがページを覆っている量です。
- `dark`（ダークモード）は `prefers-color-scheme`（またはアプリの `appearance.setColorScheme`）、`reduceMotion`（視差効果を減らす）は `prefers-reduced-motion`、`fontScale`（文字サイズ）はブラウザーのデフォルトのフォントサイズ（テキストはこれに従います）、`scale` はディスプレイのピクセル比から決まります。
- `locale`、`languages`、`timeZone`、週の始まりの曜日、時刻の表記（12 時間制か 24 時間制か）は、ブラウザーのもの（`navigator.languages`、`Intl`）です。

## ビューの対応

- テキストはブラウザーのテキストで、システムのフォント（`system-ui`。Apple のシステムでは San Francisco、Windows では Segoe UI、Android では Roboto）を使い、Web 向けのタイプランプ（本文は 16）で表示されます。`assets/fonts` に置いたアプリのフォントは、アプリが指定した名前（フォントの `family`）で読み込まれます。
- `Stack` は、一番上の画面を独自のヘッダーバーの下に表示します。ヘッダーバーには、戻るボタン、画面のタイトル、バーのボタン、検索フィールドが並びます。プッシュとポップはスライドで切り替わります。戻る操作にはブラウザーの機能も使えます。アプリ内に戻る先がある間は、ブラウザーの戻るボタン（とスマートフォンの戻るジェスチャー）でアプリ内を戻ります。Escape、Alt+←（Mac では ⌘[）、タッチスクリーンでの先頭側の端からのスワイプも同じです。ほかのプラットフォームと同じく、これらは `onBack` で受け取れます。ルーターを使う場合、アドレスバーには画面が表示され、履歴はそのナビゲーションのものになります（[アドレスと履歴](#アドレスと履歴)）。
- `Tabs` は画面下部のタブバーになります。`Split` は詳細の横に並ぶサイドバーになり、ウィンドウの幅が狭いときは、詳細のヘッダーから開くドロワーになります。
- `Sheet` はモーダルの `<dialog>` です。幅の狭いウィンドウではボトムシート（medium は高さの半分で、グラバーをドラッグすると引き上げたり閉じたりできます）、幅の広いウィンドウでは中央に置かれたカードになります。Escape、外側のクリック、下方向のスワイプで閉じます。`Alert` もモーダルの `<dialog>` で、アクションシートのボタンはリストになります。`Popover` とメニューは、指し示す対象の隣に表示されるポップオーバーです。`Window` は、ユーザーが移動やサイズ変更をできる、ページ内のパネルになります。
- `List` は、Mac、Windows、Linux と同じく仮想化されます。表示中の行（とその前後半画面分）だけがバインドされます。セクションのヘッダーは上部に固定され、チャットのリストは末尾に留まります。行のスワイプアクションは、その行のコンテキストメニュー（右クリックまたは長押し）になります。
- `Image` は `<img>`（メインスレッドの外でデコードされ、ブラウザーがキャッシュします）、`Video` は `<video>`、`WebView` は `<iframe>`（[Web ビュー](#web-ビュー)を参照）、`Canvas` は 2D の canvas、`Shape` は SVG のパス、`Shader` は WebGL 2（`shader` 宣言を GLSL ES 3.00 に変換します）になります。`Symbol` は、SF Symbol の名前に対して Android で表示されるのと同じ Material Symbol になります。
- `animate` は、フレームと見た目のプロパティを Web Animations API でアニメーションします（スプリングは CSS の `linear()` カーブで表現します）。出現・退場のトランジションと共有要素（`shared`）も、iOS と同じように動作します。
- タップやクリック、長押し（と右クリック）、ホバー、押下状態、パン、スワイプ、ピンチ（2 本指、またはトラックパッドのピンチ）、回転は、マウス、ペン、タッチのどれでも動作します。ドラッグ＆ドロップはブラウザーのもので、ほかのアプリからのファイルにも対応します。
- `MenuBar` の項目のショートカットは、ページ内で動作します（Mac では ⌘、それ以外では Ctrl）。ただし、ページには独自のメニューバーを表示しません。デスクトップのアプリでは、アプリのメニューが Mac ではメニューバーに、Windows ではウィンドウのメニューバーに、Linux ではヘッダーバーのメインメニューに表示されます。

## パレット

ページは Asuka のパレット、つまり Apple のライトとダークの色で描かれます。ライトモードでは `.label` は黒、ページの背景は白です。`asuka.toml` の `[web.colors]` で、そのどれでも置き換えられ、それぞれにダークモード用の色も添えられます。

```toml
[web.colors]
label = "#16181d"
labelDark = "#e8e9eb"
separator = "#dfe1e5"
separatorDark = "#3a3d42"
```

色は、その役割で描かれるすべての場所に届きます。`label` は色を指定していない Text とフィールドの文字の色、`separator` は行のあいだやコントロールのまわりの線、という具合です（[キーの一覧](asuka-toml.md#web)）。ページがダークのあいだは、ブラウザーの `prefers-color-scheme` によるものでも、アプリの `appearance.setColorScheme(.dark)` によるものでも、ダーク用の色が使われます。ダーク用の色がなければどちらのモードでも同じ色で、書かなかった色は Asuka の色のままです。この設定を使うのは Web だけで、ほかのプラットフォームはセマンティックカラーをシステムの色で描きます。

## アプリのファイル

アプリのディレクトリ（`fs.appDir()`）は、`std/fs` がそこに書き込んだファイルやクラッシュの記録も含めて保持されます。このディレクトリのファイルは、アプリの起動前にブラウザーのストレージ（IndexedDB）から読み込まれ、変更の少し後と、ページが非表示になったり閉じられたりしたときに書き戻されます。アプリのタブどうしは、これらのファイルを共有します。あるタブの書き込みはほかのタブに届き、受け取ったタブはファイルを読み直します（ただし、あるタブが変更してまだ書き戻していないファイルは、書き戻すまでそのタブの内容のままです。つまり、最後に書き込んだタブの内容が残ります）。アプリのキャッシュ（`fs.cacheDir()`）も同じように保持されますが、専用のデータベースに分けて保存されるため、ブラウザーはアプリのファイルに触れずにキャッシュだけを消せます。ユーザーがドロップ、ペースト、選択したものは、ページの一時ディレクトリ（`/tmp`、ページが開いている間は残る）にコピーされ、パスとしてアプリに渡されます。

`spawn { … }` は、ページの Web Worker の一つで実行されます（最大 4 つ。`spawn` が初めて必要としたときに起動します）。各ワーカーはアプリのコードをもう一度実行し、トップレベルの `let` を UI なしで作ります。そこで作られる値（トップレベルで開いたデータベースなど）は、そのワーカー自身のものです。ワーカーはアプリのファイルのコピーを持っています。ページが変更したファイルは次に実行するブロックと一緒に送られ、ブロックが変更したファイルは結果と一緒に戻ってきます。書き込み手が一つ増えるのと同じで、最後にファイルを書いた側の内容が残ります。`std/sqlite` のデータベースはページのものなので、ブロックからは使えません。

ブラウザーは、ユーザーが消去するまでオリジンのストレージを保持します。ただし、デバイスの空き容量がなくなると、ブラウザーがそのオリジンのストレージを削除することがあります（Safari は、ホーム画面に追加されていないサイトについては、7 日間アクセスがない場合にも削除します）。

## SQLite

`std/sqlite` は、ライブクエリも含めて、ほかのプラットフォームと同じように動作します。同じ SQLite と同じ Asuka のライブラリを、WebAssembly にビルドしたものです（`asqlite.wasm`。`asuka dev --platform web` と `asuka build --platform web` が、`std/sqlite` をインポートするアプリのためにビルドします）。SQLite はページのワーカーで動き、アプリのデータディレクトリにあるデータベースを、ブラウザーの Origin Private File System に保存します。データベース全体がメモリに読み込まれることはなく、コミットが戻った時点でディスクに書き込まれています。

ほかのプラットフォームとの違いは次のとおりです。

- `sqlite.open` と `db.migrate` は、ほかのプラットフォームではメインスレッドで実行されますが、Web のメインスレッドはワーカーを待てません。そのため、応答を待たずにワーカーへ送られ、その後のステートメントより先に実行されます。開けなかったデータベースでは、以降のステートメントがすべてその理由とともに失敗します。マイグレーションが失敗した（そしてロールバックされた）場合は、そのデータベースの次のステートメントがそのエラーで失敗し、コンソールにもその旨が表示されます。
- ジャーナルは write-ahead logging（接続間で共有するメモリが必要）ではなく、ロールバックジャーナルです。また、データベースへの接続は一つだけです。そのため、読み取りは進行中の書き込みと並行して走るのではなく、書き込みの完了を待ちます。
- アプリの複数のタブで、一つのデータベースを使えます。タブはデータベースを使っている間それを保持し、ほかのタブが求めると手放します（トランザクションの合間に手放し、トランザクションの途中では手放しません）。ほかのタブが保持しているデータベースに対するステートメントは、SQLite がほかのプロセスのロックを待つのと同じく、ビジータイムアウトの 5 秒間、そのタブが手放すまで再実行されます。あるタブのライブクエリには、ほかのタブの書き込みは伝わりません（ネイティブで、ほかのプロセスの書き込みが伝わらないのと同じです。`db.changed()` で再実行できます）。
- データベースはワーカーのファイルで、ページのファイルシステムにあるわけではありません。そのため、`std/fs` からは見えません。データディレクトリ以外の場所（`/tmp`）にあるデータベースは、ページが開いている間しか残りません。
- Origin Private File System がない場合（`https` や `localhost` から配信されていないページ。たとえば `asuka dev --lan` のページや、一部のブラウザーのプライベートウィンドウ）、ワーカーはデータベースを IndexedDB に保存し、使っている間はメモリ上に置きます。この場合、コミットが永続化されるのは、コミットが戻った少し後です。セキュアコンテキストでないページでは Web Locks も使えないため、二つのタブが一つのデータベースに書き込むと、互いの書き込みが失われることがあります（コンソールにその旨が表示されます）。
- 以前のバージョンの Asuka がアプリのファイルの中（IndexedDB）に保存していたデータベースは、アプリが初めて開いたときにワーカーへ移されます。

アプリを埋め込む独自のページ（後述）では、`start` に `sqlite` として SQLite のモジュールの URL を渡します。ビルドに含まれるファイル `"asuka/<digest>/lib/asqlite.wasm"` です。ワーカーのスクリプトは Asuka のもの（`asuka.js` と同じ場所にある `sqlite-worker.js`）です。

## ネイティブモジュール

標準ライブラリとプラットフォーム SDK のパッケージは、ブラウザーでは次のようになります。

- `platform/secure`：値は、ブラウザーが保持して外に出さない鍵（エクスポートできない WebCrypto の鍵）で暗号化（AES-GCM）され、IndexedDB に保存されます。キーチェーンではありません。アプリのオリジンで動くスクリプトは鍵そのものを読めませんが、その鍵での復号を依頼することはできます。`https`（または `localhost`）が必要です。
- `platform/device`：`info()` はユーザーエージェントから取得します（Chromium のクライアントヒントからは、システムのバージョンと Android スマートフォンの機種がわかります）。クリップボード、クリップボードの画像（PNG。キャッシュに保存されます）、`clipboardChanges()`（アプリ自身のコピーと、ユーザーがページにクリップボードの読み取りを許可している場合はほかのアプリのコピー）に対応します。`keepAwake`（Screen Wake Lock。ページが再び表示されると取得し直します）、`browse`（新しいタブ）、`pickPhotos` と `pickFiles`（ブラウザーのファイル選択ダイアログ。選んだファイルはキャッシュにコピーされます）にも対応します。
- `device.shareFiles`：ファイルを渡して共有シートを開きます（`navigator.share`）。共有シートがなければ、各ファイルをダウンロードします。`platform/preview`：ファイルを新しいタブで開きます（画像、動画、音声、テキスト、PDF、HTML）。それ以外の種類は `PreviewError.noViewer` になります。
- `platform/print`：HTML、ファイル（PDF はブラウザーのビューアーで）、アプリ自身の Web ビューを、ブラウザーの印刷ダイアログで印刷します。結果は常に `.unknown` です（ブラウザーが結果を知らせないためです）。`print.pdf` は Web では使えません（`PrintError.unsupported`）。ブラウザーが PDF を作れるのは、印刷ダイアログ（「PDF として保存」）を通したときだけです。
- `platform/compose`：`mailto:` と `sms:` のリンクを使います。添付ファイルがあれば共有シートを使います。
- `platform/notify` と `platform/push`：アプリの service worker を通じて、Notification API と Web Push を使います。[通知と Web Push](#通知と-web-push)を参照してください。
- `platform/network`：オンラインかどうかは `navigator.onLine` から取得します。Wi-Fi、モバイル回線、Ethernet の区別は、ブラウザーが教えてくれる場合（Android の Chrome）にわかります。モバイル回線は従量制（metered）、データセーバーは制限付き（constrained）として扱います。
- `platform/location`：Geolocation API を使います。`placeName` は Web では使えません（ブラウザーにはジオコーダーがないためです）。領域の監視とバックグラウンドの位置の更新は、位置情報から計算するので、ページが開いている間だけ働きます。
- `platform/sensors`：Generic Sensor API（Chrome）か、デバイスの motion イベントと orientation イベント（Safari はタップ時に許可を求めます）を使い、Android と同じ軸と単位で値を返します。気圧計と歩数計はありません。
- `platform/speech`：`speechSynthesis` と、ブラウザーの音声を使います。
- `platform/audio`：プレイヤーごとに audio 要素を一つ使います（ブラウザーの自動再生のルールが適用されるので、タップから再生してください）。`platform/record`：`MediaRecorder` を使います。ブラウザーが対応していれば MPEG-4 の AAC で、そうでなければ WebM の Opus で録音します。`platform/nowplaying`：Media Session API を使います。
- `platform/camera`：`take()` は、スマートフォンならカメラを開き、それ以外ではライブのカメラ映像とシャッターボタンのあるダイアログを開きます。`camera.View` は `<video>` に映したカメラ映像です（`capture()`、`record` に対応し、ブラウザーに `BarcodeDetector` があればコードのスキャンにも対応します）。`platform/scan` と `platform/vision` のコード読み取りは `BarcodeDetector` を使います。顔とテキストの認識は、ブラウザーが対応している場合にだけ使えます。
- `platform/imageedit`：canvas を使います（JPEG か PNG。リモートの画像には CORS が必要です）。
- `platform/maps`：Windows と同じく、OpenStreetMap のタイルから Asuka が描画する地図です。マーカーとそのコールアウト、ドラッグ、ホイール、ピンチ、ダブルクリック、キーボードによる移動、OpenStreetMap が求める帰属表示に対応します。航空写真と混合の地図は道路地図を表示します（無料の航空写真がないためです）。`showsUser` は、ページに位置情報の許可があればユーザーの現在地を表示します（許可を求めるのは `platform/location` で、地図が求めることはありません）。タイルは、利用ポリシーが Web ページに求めるとおりに `tile.openstreetmap.org` から取得します（画面に表示する分だけを、ブラウザーのキャッシュを使い、Referer 付きで取得します）。大量に使う場合は、`start` に `mapTiles`（`{z}`、`{x}`、`{y}` を含む別のサーバーの URL）を渡してください（[埋め込み](#埋め込み)）。
- `platform/crash`：その実行の記録は、スマートフォンと同じくアプリのファイルに保存されます。Asuka 自身の致命的なエラーはこの記録を書き込んでページのアプリを止め、次回の起動時に報告されます。キャッチされなかった JavaScript のエラー（Asuka のものか、パッケージの `web/` のスクリプトのもの）は、その場で `"exception"` のレポートになりますが、致命的なものとしては扱われません（ページは動き続けます）。ページは `localStorage` に自分の実行が生きていることを記録するので、アプリのほかの開いているページがその実行に手を出すことはありません。
- `platform/biometric`：WebAuthn を通じてプラットフォームの認証器（Touch ID、Face ID、Windows Hello、Android の画面ロック）を使い、ユーザー検証を必須にします。最初の `authenticate` で、アプリの名前を付けたサイトのパスキーが作られます。プロンプトはブラウザーのものなので、`reason` と `cancel` のテキストは表示されません。
- `platform/webauth`：ポップアップを使い、アプリと同じオリジンのページに戻ってきた時点で完了します（カスタムスキームではページを開けないためです）。
- `platform/screen`：`lock` に対応します（全画面表示のとき、またはインストールしたアプリでのみ）。バーを隠すと全画面表示になります。
- `platform/library`：`save` はファイルをダウンロードします。`platform/contacts`：`pick` は、ブラウザーに Contact Picker がある場合（Android の Chrome）に使えます。`add` は vCard を、`platform/calendar` の `add` は `.ics` ファイルをダウンロードします。
- `platform/nfc`：Web NFC の `NDEFReader` を使います。セキュアコンテキストの Android の Chrome で、NDEF メッセージの読み書き、タグの読み取り専用化、タグのシリアル番号が使えます。生のコマンドや容量は使えず、タグでページを開くこともできません。ほかのブラウザーには Web NFC がないので `NfcError.notAvailable` になります（[NFC タグ](nfc.md)）。
- `platform/tracking` は、ブラウザーが Global Privacy Control か Do Not Track を送っていれば `.denied` を、そうでなければ `.allowed` を、プロンプトを出さずに返します。
- `platform/pay`：ブラウザーに Apple Pay がある場合（Safari）は、Payment Request API を通じて Apple Pay を使います。マーチャントの検証は、Apple に登録したドメインについてアプリのサーバーが行います（`onValidateMerchant`）。それ以外のブラウザーでは、Google の Web API（pay.google.com から読み込む `pay.js`）を通じて Google Pay を使います。ボタンはそれぞれ純正のものです。シートはタップからしか表示できません。
- `device.haptic` は、ブラウザーが対応していれば（Android の Chrome）`navigator.vibrate` を使います。
- Web では使えず、その旨のエラー（またはパッケージ独自の `notAvailable`）になるもの：`platform/review`、`platform/integrity`、`platform/agerange`、`platform/appleauth`、`platform/icloud`、`platform/periodic`、`platform/widget`、`platform/liveactivity`、`platform/photos`（`.denied` を返します。写真の選択は `device.pickPhotos` でできます）、連絡先とカレンダーの読み取り、`platform/transcribe` の `file`、`platform/screen` の `preventCapture` と `screenshots()`。

## 通知と Web Push

Web アプリにはどれも service worker があります。service worker は Asuka が書き出し（`asuka dev` では `sw.js`、ビルドでは `sw.<digest>.js`）、ページが起動時にアプリのフォルダーをスコープとして登録します。fetch ハンドラーは持たないので、ページは service worker がない場合と同じように読み込まれます。service worker は、アプリのページが開いているかどうかにかかわらず、アプリの通知とプッシュ通知を表示し、そのクリックに応答します。ブラウザーが service worker を動かすのは、HTTPS か `localhost` から配信されたページだけです（`asuka dev` のアドレスは対象ですが、`--lan` のアドレスは対象外です）。

### platform/notify

- `notify.request()` は、ブラウザーのプロンプトで許可を求めます。ブラウザーはタップやクリックのときにしか許可を求めないので、その処理の中から呼び出してください。
- `notify.show(n)` は service worker を通じて通知を表示するので、ページを閉じた後もボタンは残り、クリックも機能します（service worker のないページ、つまり service worker を登録しない埋め込みページでは、ページ自身の通知がボタンなしで表示され、そのクリックはそのページにしか届きません）。
- 通知の `id` は、その通知のタグになります。同じ `id` の通知は前の通知を置き換え、`silent` でなければ改めてユーザーに知らせます。`subtitle` は本文の 1 行目、`image` は画像（`https` の URL かアセット）、メッセージの送信者の画像（`sender` の `image`）はアイコンになります。`badge` と `notify.setBadge` は、インストールしたアプリのバッジを設定します（`navigator.setAppBadge`）。
- カテゴリーのアクション（`notify.setCategories`）は、通知のボタンになります（Chrome と Edge は 2 個まで表示し、Firefox と Safari は表示しません）。`reply` を持つアクションは、ブラウザーがインライン返信に対応していればテキストフィールドになります（Android とデスクトップの Chrome。入力したテキストはレスポンスの `reply` に入ります。それ以外のブラウザーではボタンになります）。カテゴリーは service worker のために保存されるので、カテゴリーを指定したプッシュ通知にもボタンが付きます。
- 通知をタップすると、アプリのページにフォーカスが移り、通知の `url` に移動し（`onOpenURL`）、`notify.responses()` にレスポンスが届きます（`action` は `""`）。ボタンを押すと、そのボタンの `id` を伴って届きます。届くのは前面にあるページで、なければアプリのいずれかのページです。ページが一つも開いていなければ、service worker がページを開きます（ブラウザーはアプリのコードをページの中でしか動かさないためです）。アプリは、タップで起動したアプリと同じく、起動時にレスポンスを受け取ります。そのため、`notify.responses()` はルートコンポーネントで読んでください。`open` のないアクションは、開いているページを前面に出しません。ただし、ページが一つも開いていなければページを開き、前面に表示します。レスポンスの `defer()` には、完了まで生かしておく対象がありません。ページはそのまま動き続けるためです。
- `notify.present(decide)` は、ページが前面にあるときに届いた通知（プッシュ通知も含みます）をどう表示するかをアプリに尋ねます。`.banner`、`.quiet`（音なし）、`.hide` から選びます。
- `notify.showing()`、`remove`、`removeThread`、`removeAll`、`cancel` は、service worker の通知に対して働きます。以前のページが出した通知やプッシュ通知も対象です。
- Android のチャネルは、ここには存在しません。iOS と同じく、指定しても受け付けたうえで無視されます。

#### スケジュールした通知

アプリのページが一つも開いていない間、ブラウザーは決まった時刻に通知を表示できません。そのための API がなく（これを実現していた Chrome の Notification Triggers は 2021 年に開発が中止されました）、service worker はイベントで起こされたときにしか動かず、タイマーでは動かないためです。そこで `notify.schedule` は、ブラウザーでできる範囲のことを行います。

- スケジュールした内容は、ページではなくアプリのストア（アプリのページと service worker が共有する Cache Storage）に保存されます。
- アプリのページがどれか一つでも開いている間は、指定した時刻に表示されます。各ページは次の通知のためのタイマーを持ち（通知をスケジュールしたりキャンセルしたりしたページが、ほかのページに知らせます）、最初に期限に気づいたページが一度だけ表示します（Web Lock を使います）。バックグラウンドのページでは遅れることがあります。ブラウザーは、非表示のページのタイマーを 1 分に 1 回程度に間引くためです。遅くとも、ページが再び前面に来たときには表示されます。
- ページが一つも開いていない間に期限が来た通知は、次にページが開いたときに、予定時刻を過ぎた状態ですぐに表示されます。
- それより前にも、ベストエフォートとして、ページが開いていないときに何かが service worker を起こすたびに、service worker が期限の来た通知を表示します。起こすものとは、プッシュ通知、クリック、そして Chrome の Periodic Background Sync です。Periodic Background Sync は、何かがスケジュールされている間、ページが要求します。Chrome がこれを許可するのはインストールされたアプリ（ホーム画面に追加したアプリや、アドレスバーからインストールしたアプリ）だけで、実行されるのは多くても 12 時間に 1 回程度、それもブラウザーの判断次第です（ユーザーがアプリをどれだけ使っているか、バッテリー、ネットワーク）。ほかのブラウザーにはこの機能がありません。決してこれを当てにしないでください。

アプリを閉じていても必ず指定の時刻に表示したい通知は、その時刻にサーバーからプッシュ通知を送ってください。

### platform/push：Web Push

Web Push には VAPID の鍵ペア（RFC 8292）が必要です。アプリのサーバーは秘密鍵でプッシュに署名し、ブラウザーは公開鍵で購読します。公開鍵は `asuka.toml` に書きます。

```toml
[web]
vapidPublicKey = "BKpYsAT0ADj2pHQQ..."
```

鍵ペアは一度だけ作り、秘密鍵はサーバーに置いておきます。鍵ペアを作り直すと、購読もすべてやり直しになります。鍵ペアはどの Web Push ライブラリでも作れます（たとえば `npx web-push generate-vapid-keys`）。

`push.register()` は、通知の許可を求め（ブラウザーはタップのときにしか尋ねないので、タップから呼び出します）、その鍵でアプリの service worker を購読させ（別の鍵で作った購読は置き換えられます）、`push.Registration` を返します。

- `token`：購読の JSON です。形式は `PushSubscription.toJSON()` が返すものと同じで、`{"endpoint": "https://...", "expirationTime": null, "keys": {"p256dh": "...", "auth": "..."}}` のようになります。これをそのままサーバーに送ってください。Web Push のライブラリはこの形式をそのまま受け取れます。ブラウザーが購読を変えることがあるので（`pushsubscriptionchange`）、ほかのプラットフォームと同じく起動のたびに登録し、サーバーにあるものと違っていれば送ってください。
- `service`：`"webpush"`。
- `allowed`：true。許可がなければ `register` は失敗します。ブラウザーは、プッシュ通知を表示するサイトにしか購読を許さないためです（`userVisibleOnly`）。

`web.vapidPublicKey` がないとき、service worker がないとき（セキュアでないページ）、ブラウザーが Web Push に対応していないときは、理由を示して失敗します。iOS と iPadOS の Safari が Web Push に対応するのは、ホーム画面に追加したアプリだけです（16.4 以降）。

プッシュ通知は、ページが開いているかどうかにかかわらず通知として表示されます。アプリのページが開いていれば、そのページが先に受け取り（`push.received()`。開いているすべてのページが受け取ります）、内容を書き換えたり（`push.rewrite`。前面にあるページで、約 10 秒以内に実行されます。ページが開いていなければ、届いたままの内容で表示されます）、表示方法を決めたり（`notify.present`）できます。通知をタップすると、ローカル通知と同じく、`push.opened()` と `notify.responses()`（通知の `remote` が true になります）に届き、データに `"url"` があれば `onOpenURL` も呼ばれます。

#### サーバーが送る内容

ペイロードは JSON です。どの Web Push ライブラリでもそうするように、購読に合わせて暗号化し（RFC 8291 の `aes128gcm`）、VAPID トークン（RFC 8292）を付けて送ります。service worker は次のキーを読みます。ほかのプラットフォームの `push.Message` と同じキーです。

| キー | |
| --- | --- |
| `title`、`body`、`subtitle` | 表示される内容（サブタイトルは本文の 1 行目になります） |
| `data` | アプリ独自のキーで、`push.Message` の `data` になります（文字列でない値は JSON として渡されます）。`"url"` は、タップしたときにアプリに渡して開く URL です（`onOpenURL`） |
| `id` | 通知のタグ。同じ `id` のプッシュ通知は、前の通知を置き換えます |
| `category` | `notify.setCategories` で登録したカテゴリーの一つ。そのアクションが通知のボタンになります |
| `thread`、`summary` | `push.Message` の同名のフィールド。`notify.removeThread` とアプリのために使います |
| `image` | 画像。`https` の URL か、アプリのアセットの一つ |
| `badge` | インストールしたアプリのバッジ |
| `silent` | 音を鳴らさずに表示します |

```json
{
  "id": "chat-7",
  "title": "Ada",
  "body": "Lunch?",
  "category": "message",
  "thread": "chat-7",
  "data": { "url": "myapp://chat/7", "chat": "7" }
}
```

`title` も `body` もないプッシュは、データメッセージです。開いているページが受け取り（`push.received()`）、何も表示されません。データメッセージは、アプリが開いている間だけ送ってください。ブラウザーはプッシュのたびに通知が表示されることを期待しています。前面にページがないときに何も表示されないと、Chrome は独自の通知（「This site has been updated in the background」）を表示し、Safari はそれが何度か続くと購読を終了します。JSON でないペイロードは、本文として表示されます。ペイロードは最大 3993 バイトです。

暗号化、署名、送信は、どの Web Push ライブラリでもできます。Node なら [`web-push`](https://github.com/web-push-libs/web-push) ライブラリを使います。

```js
import webpush from "web-push";

webpush.setVapidDetails("mailto:ops@example.com", process.env.VAPID_PUBLIC_KEY, process.env.VAPID_PRIVATE_KEY);

// token：アプリの push.register() が返した token
await webpush.sendNotification(JSON.parse(token), JSON.stringify({
  title: "Ada", body: "Lunch?", category: "message", data: { url: "myapp://chat/7" },
}), { urgency: "high" });
// 404 か 410（err.statusCode）なら購読は終了しています。削除してください。
```

Go で書いたサーバーなら、Asuka 自身の送信側の実装、つまりリポジトリの [`webpush`](../../webpush/webpush.go) パッケージを使えます。使うのは標準ライブラリだけです。

```go
import "github.com/egoist/asuka/webpush"

var sender = &webpush.Sender{
	Keys:    webpush.Keys{Public: os.Getenv("VAPID_PUBLIC_KEY"), Private: os.Getenv("VAPID_PRIVATE_KEY")},
	Subject: "mailto:ops@example.com", // プッシュサービスがあなたに連絡するための宛先
}

func notifyChat(ctx context.Context, token string) error {
	sub, err := webpush.ParseSubscription(token) // アプリの push.register() が返した token
	if err != nil {
		return err
	}
	err = sender.Send(ctx, sub, webpush.Notification{
		ID: "chat-7", Title: "Ada", Body: "Lunch?", Category: "message",
		Data: map[string]string{"url": "myapp://chat/7"},
	}, &webpush.Options{Urgency: "high"})
	if errors.Is(err, webpush.ErrGone) {
		return forgetSubscription(token) // 購読が終了した：ユーザーかブラウザーによって
	}
	return err
}
```

`Options` では、プッシュの `TTL`（ブラウザーがオフラインのときにプッシュサービスが保持する期間。デフォルトは 4 週間）、`Urgency`、`Topic`（同じトピックの後のプッシュが、まだ待っている前のプッシュを置き換えます）を設定します。`webpush.GenerateKeys` で鍵ペアも作れます。ほかの言語にもそれぞれのライブラリ（pywebpush、web-push-php、Rust の `web-push` クレートなど）があり、送る JSON は同じです。

## Web ビュー

`WebView` は `<iframe>` で、ページを読み込むたびに新しく作られます。そのため独自の履歴を持ち、タブの履歴には何も追加しません。

アプリ自身のサイトのページ（アプリのアセット、`html`、アプリが書き出したファイル、ページと同じオリジンの任意の URL）では、すべての機能が使えます。Web ビューはページを本来の URL でフレームに読み込み、ページのスクリプトより先に `window.asuka.postMessage` と `preload` を用意します。そのほかに使えるのは次の機能です。`Ref` の `run(script)` と `eval(script)`（Promise は await されます）。`on loaded`、`on error`、`on state`（URL、タイトル、`pushState`、進捗、戻る・進む）。`Ref` の `goBack()`、`goForward()`、`reload()`、`stopLoading()`。リンク、フォーム、`location` の変更による読み込みは、Web ビューを通じて行われます。`block`（マークアップ、`fetch`、XHR、`sendBeacon`、WebSocket、EventSource、要素の `src` と `href`。ブロックした時点ですでに始まっていたリクエストは、送信されてしまうことがあります）。`on request`（Resource Timing）。`on newWindow`（`window.open`、`target=_blank`）。`on download`（添付ファイル、フレームで表示できない種類のファイル、`<a download>`）。`webview.cookies(url)`（`document.cookie`。`HttpOnly` の Cookie は対象外です）。`html` の相対リンクは、アプリのアセットを基準に解決されます。

ほかのサイトのページは、読み込むことしかできません。ブラウザーがサイトどうしを分離しているためです。読み込みが終わると `on loaded`、サイトに接続できないと `on error` が呼ばれます。サイトがフレーム内での表示を拒否している場合（`X-Frame-Options`、`frame-ancestors`）、Web ビューはそれを検知できず、ブラウザー独自のエラーページが表示されます。ページのリンクの移動先を追ったり、タイトルを読んだりすることはできません。スクリプトの実行、`block` や `preload`、リクエストの報告、Cookie の読み取り、`userAgent` の設定もできません（スクリプトと Cookie はエラーを返し、それ以外は一度だけ警告を出します）。フレームはトップレベルナビゲーションを許可しない sandbox になっているので、ページがアプリを別のページに置き換えてしまうことはありません。

ほかのサイトのページでも、協力的なページであれば、`postMessage` を通じて Web ビューとやり取りできます（Web ビューは自分のフレームからのメッセージだけを受け付け、準備ができたと伝えてきたオリジンにだけスクリプトを送ります）。

```js
// ページ側：アプリへのメッセージ（on message）、URL とタイトル、アプリの eval。
const app = (m) => parent.postMessage({ asuka: m.asuka ?? true, ...m }, "*");
addEventListener("message", async (e) => {
  if (e.source !== parent || !e.data?.asuka) return;
  if (e.data.asuka === "hello") app({ asuka: "ready", url: location.href, title: document.title });
  if (e.data.asuka === "eval") {
    try { app({ asuka: "result", id: e.data.id, value: JSON.stringify(await (0, eval)(e.data.script)) }); }
    catch (err) { app({ asuka: "result", id: e.data.id, error: String(err) }); }
  }
});
app({ asuka: "ready", url: location.href, title: document.title });
app({ text: "Hello from the page" }); // on message
```

`platform/js` は、独自のオリジンを持つ非表示の sandbox フレームでスクリプトを実行します。そのため、スクリプトはアプリのページやストレージにアクセスできません。

## ネイティブコード

[ネイティブパッケージ](native.md)は、モジュールとビューを Asuka で宣言します（`native interface`、`native component`）。Web では、それぞれを `web/` ディレクトリの JavaScript で実装します。`asuka native generate` が `<package>.asuka.js`（と、列挙型とエラーを入れる `<package>.decl.asuka.js`）を生成し、これがパッケージ独自の `<package>.js` をインポートします。`<package>.js` は、モジュールとビューごとに `make<Name>` 関数をエクスポートする ES モジュールです。`examples/media/segmented` のビューを例にとると、

```asuka
pub native component Segmented(items: [string]) {
  prop selected: int = 0
  event changed(index: int)
}
```

Web での実装は次のとおりです。

```js
// segmented/web/segmented.js
export function makeSegmented(events) {
  const el = document.createElement("div");
  let items = [], selected = 0;
  const render = () => { /* 項目ごとにボタンを一つ作り、クリックで events.changed(i) を呼ぶ */ };
  return {
    el, // ビュー。Asuka が与えるフレームのサイズになる
    setItems(v) { items = v; render(); },
    setSelected(v) { selected = v; render(); },
    measure: (c) => [items.length * 72, 32], // c の範囲内でのサイズ（c は {width, height, availW, availH}。制約がなければ NaN）
  };
}
```

各パッケージの `web/` は `asuka/<digest>/packages/<package>/` に配置され、ページはアプリの起動前にアプリのパッケージをインポートします。パッケージのほかのファイル（ヘルパーやアセット）も同じ場所に配置されるので、パッケージはそれらを `import.meta.url` からの相対パスでインポートしたり、fetch したりできます。これらのファイルが変わると、`asuka dev` がページをリロードします。

## 埋め込み

`asuka build --platform web` が作るページでは、アプリはページ全体に表示されます。独自のページの要素の中でアプリを動かすには、Asuka のローダーを読み込み、その要素の中でアプリを起動します。するとアプリは、その要素のサイズに従います。

```html
<link rel="stylesheet" href="asuka/<digest>/style.css">
<div id="app" style="width: 390px; height: 700px"></div>
<script type="module">
  import { start } from "./asuka/<digest>/asuka.js";
  start({ container: document.getElementById("app"), core: "asuka.<digest>.wasm", name: "com.example.app", serviceWorker: "sw.<digest>.js" });
</script>
```

一つのページで、それぞれ別の要素に入れて、複数のアプリを動かせます。`serviceWorker` を渡すと、アプリの service worker（ビルドに含まれる `sw.<digest>.js`。対象とするページがあるフォルダーから配信します）が登録されます。渡さなければ、通知はページ自身のものになり、`platform/push` は購読できません。`platform/push` を使うなら、`index.html` と同じく `vapidPublicKey` も渡してください。`platform/maps` のタイルを自前のサーバーから取得するなら、`mapTiles` を渡します（`"https://tiles.example.com/{z}/{x}/{y}.png"`。デフォルトは OpenStreetMap のタイルです）。

## 未対応の機能

- テキストフィールドのアクセサリーバーは、オンスクリーンキーボードがある場合（タッチスクリーン）にだけ表示されます。
- `platform/notify` でスケジュールした通知は、アプリのページが一つも開いていない間は、指定の時刻に表示できません。ブラウザーに通知をスケジュールする手段がないためです（[前述](#スケジュールした通知)）。
