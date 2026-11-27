# The web

The same app runs in a browser, as WebAssembly, its views the page's
DOM. The web is one of Asuka's platforms beside iOS, Android, the Mac,
Windows and Linux, from the same code. Each view is an element placed
where Asuka's layout (flexbox and grid) puts it, the same on every
platform; text and controls are measured by the real elements, as on
the other platforms. Controls are the
browser's own (`<button>`, `<input>`, `<select>`, `<progress>`,
`<dialog>`), so keyboard handling, focus, autofill, spelling and screen
readers come with them.

```sh
asuka dev --platform web examples/counter       # serves it and opens it in your browser; reloads as you save
asuka build --platform web examples/counter     # a static site, in examples/counter/dist/web
```

Both need [Zig](https://ziglang.org/download) installed, to build for
WebAssembly.

What follows says what the web does, and, where a browser can't do what
a phone does, what happens instead.

## asuka dev --platform web

`asuka dev --platform web` builds the player once, serves the page at
`http://localhost:8790/` (or a free port) and opens it. Each save
compiles the app again, in milliseconds, and the page loads the new
version in place, keeping the app's state, as the other platforms'
players do; the terminal reports each reload (`asuka: reloaded in 1.1
ms, 3 states kept`). The app's output and the page's errors print in the
terminal; a save that doesn't compile, or an error the app didn't
catch, shows over the page ([Errors over the
app](development.md#errors-over-the-app)). A change to `asuka.toml`, the assets or a
native package's JavaScript reloads the page (the app starts again).

The flags, beside `asuka dev`'s own (`--lang`, `--dir`, `-v`):

- `--port`: the server's port.
- `--lan`: serve to your network too, and print its addresses, the
  first as a QR code: a phone's camera opens the app in its browser.
- `--no-open`: don't open the browser.
- `--once`: serve, without watching.

## asuka build --platform web

`asuka build --platform web` builds the app as WebAssembly and makes a
static site in `dist/web`:

- `index.html` and `manifest.webmanifest` (a web app manifest, so a
  browser offers to install the app; its icons from `asuka.toml`'s
  `icon`, with a favicon and iOS's home screen icon);
- the app, `asuka.<digest>.wasm`, optimized and stripped;
- Asuka's own files in `asuka/<digest>/` (with SQLite's WebAssembly
  module, for an app that imports `std/sqlite`), and its native
  packages' scripts;
- the app's service worker, `sw.<digest>.js`, beside `index.html` (its
  scope is the site's folder): it shows the app's notifications and
  pushes while no page of it is open
  ([Notifications and Web Push](#notifications-and-web-push));
- the app's `assets/`;
- a gzipped copy of each script and of the wasm (`.gz`), for servers that
  serve precompressed files;
- `_headers`, which Cloudflare and Netlify read: `index.html` and the
  manifest `no-cache`, the files named by their digests cached for good;
- `_redirects`, which Netlify reads: a path with no file is the app's
  page, so a [router](#addresses-and-history)'s addresses open the app.

`index.html`'s `<base href>` is the path the site is served at: `/` by
default, else `asuka.toml`'s `web.base` or `--base` (`"/app/"` for
`https://example.com/app/`). The page finds its files from any address
under it, and a router's hrefs are the page's addresses under it
(`/chat/7` is `https://example.com/app/chat/7`).

Serve the folder from any web server or CDN: the files named by their
content's digest change name when they change, so they can be cached for
good, and only `index.html` must be fetched again. Serve `.wasm` as
`application/wasm`. A counter is about 300 KB of WebAssembly, 115 KB
gzipped, with Asuka's JavaScript beside it; an app's own code adds to
that.

## Addresses and history

With a [`Router`](navigation.md#routes), the page's address is the
screen shown: real paths under the site's base, with `pushState` and
`replaceState`, not a `#` fragment. Each step of the router's navigation
is an entry of the page's history, keeping the router's whole state
(each stack's screens, each tab's, the sheets), so:

- the browser's back and forward go back and forward through the
  stacks, the tabs and the sheets, to the screens each entry stood for;
  the browser's back asks a screen's or a sheet's `onBack` first, and
  when it takes it, the page stays where it was;
- a reload comes back to the same screens, the ones under the top
  included; a link pasted, or opened from elsewhere, builds the screens
  it stands for ([The screens under a link](navigation.md#the-screens-under-a-link));
- the page's title follows the top screen's title (`Title · App`).

The history moves as react-navigation's linking moves it: a navigation
that goes deeper (a push, a sheet, another tab than the first) pushes an
entry; one that goes back goes back to the latest entry with its
address (or, when the history never had it, a deep link's screens popped
one by one, replaces the entry); the rest replace it (`replace(href)`, a redirect, a guard sending the user elsewhere, between tabs other than
the first, `router.setQuery`). The in-app back button, Escape and Alt+Left pop
the stack and go back in the history with it. An `Alert`, a `Sheet` of the
app's own and a stack the router doesn't route keep their own entry for
the browser's back, as without a router.

An app in an element of a page ([Embedding](#embedding)) leaves the
page's address alone: its router's address stays its own. A router in a
`Window` panel keeps its address with the panel's `stateKey`, in
`localStorage`. A notification's URL of the app's own site opens its
screen.

### Serving every address

A reload or a pasted link asks the server for the address itself
(`/app/chat/7`), which has no file: the server must answer with the
app's `index.html`. `asuka dev --platform web` does. For the static
site:

| | |
|---|---|
| Netlify | `_redirects`, which the build writes |
| Cloudflare Pages | nothing: a site with no `404.html` is served as a single-page app (it warns that `_redirects`' rule loops, and ignores it) |
| Vercel | `vercel.json`: `{"rewrites": [{"source": "/(.*)", "destination": "/index.html"}]}` (under the base) |
| Firebase Hosting | `firebase.json`'s `hosting.rewrites`: `[{"source": "**", "destination": "/index.html"}]` |
| GitHub Pages | a copy of `index.html` as `404.html` (served, with a 404 status, for any address); `web.base` set to the repository's path for a project site |
| nginx | `try_files $uri /app/index.html;` in the site's `location /app/` |
| Caddy | `try_files {path} /app/index.html` in its `file_server` block |
| S3 and CloudFront | the distribution's error pages: 403 and 404 answered with `/index.html`, status 200 |

## What the page reports

The environment a web app sees (`environment()`) is the browser's:

- `platform` is `.web`.
- The size is the page's (or the element's the app is in), and changes
  as the window does; the size classes are Material's, from the width.
- The safe area is the page's (`env(safe-area-inset-*)`: a phone's notch
  and home indicator, with the page's `viewport-fit=cover`), and the
  keyboard is how much of the page a phone's keyboard covers.
- `dark` is `prefers-color-scheme` (or the app's `appearance.setColorScheme`),
  `reduceMotion` `prefers-reduced-motion`, `fontScale` the browser's
  default font size (which text follows), `scale` the display's pixel
  ratio.
- `locale`, `languages` and `timeZone`, and the first weekday and the
  hour cycle, are the browser's (`navigator.languages`, `Intl`).

## How views map

- Text is the browser's text in the system's font (`system-ui`: San
  Francisco on Apple's systems, Segoe UI on Windows, Roboto on Android),
  on a type ramp of the web's (body 16). The app's fonts in
  `assets/fonts` load as the app names them (a font's `family`).
- A `Stack` shows its top screen under a header bar of its own: a back
  button, the screen's title, its bar buttons, its search field. Pushes
  and pops slide. Going back is the browser's too: its back button (and
  a phone's back gesture) goes back in the app while there's somewhere to
  go back to, as do Escape, Alt+Left (⌘[ on a Mac), and a swipe from the
  leading edge on a touch screen. `onBack` takes them as on the other
  platforms. With a router, the address bar shows the screen and the
  history is its navigation's ([Addresses and history](#addresses-and-history)).
- `Tabs` are a tab bar at the bottom; a `Split` a sidebar beside its
  detail, or a drawer the detail's header opens on a narrow window.
- A `Sheet` is a modal `<dialog>`: a bottom sheet on a narrow window
  (medium is half the height; the grabber drags it up or away), a
  centered card on a wide one. Escape, a click outside or a swipe down
  dismisses it. An `Alert` is a modal `<dialog>`, an action sheet's
  buttons a list. A `Popover` and a menu are popovers next to what they
  point at. A `Window` is a panel in the page the user can move and
  resize.
- A `List` is virtualized, as on the Mac, Windows and Linux: only the
  rows
  in view (and half a screen around them) are bound; sections' headers
  stick; a chat's list stays at its end. Its rows' swipe actions are
  their context menu (a right-click, or a long press).
- `Image` is an `<img>` (decoded off the main thread, cached by the
  browser), `Video` a `<video>`, `WebView` an `<iframe>` (see
  [Web views](#web-views)), `Canvas` a 2D
  canvas, `Shape` an SVG path, `Shader` WebGL 2 (a shader
  declaration compiled to GLSL ES 3.00), `Symbol` the Material Symbol Android
  shows for the SF Symbol name.
- `animate` animates frames and visual props with the Web Animations
  API (springs as CSS `linear()` curves); enter and exit transitions and
  shared elements (`shared`) work as on iOS.
- Presses, long presses (and right-clicks), hover, pressed states, pans,
  swipes, pinches (two fingers, or a trackpad's pinch) and rotations work
  with a mouse, a pen or touch; drag and drop is the browser's, files
  from other apps included.
- `MenuBar`'s items' shortcuts work in the page (⌘ on a Mac, Ctrl
  elsewhere); the page shows no menu bar of its own, where the desktop
  apps show the app's menus: the Mac's menu bar, the window's menu bar
  on Windows, the header bar's main menu on Linux.

## The palette

The page draws in a palette of Asuka's, Apple's colors in light and
dark: `.label` is black in light mode, the page's background white.
`[web.colors]` in `asuka.toml` replaces any of them, with a color for
dark mode beside each:

```toml
[web.colors]
label = "#16181d"
labelDark = "#e8e9eb"
separator = "#dfe1e5"
separatorDark = "#3a3d42"
```

A color reaches everything that draws in its role: `label` is the
color of a Text that names none and of a field's text, `separator` the
lines between rows and around controls, and so on ([the
keys](asuka-toml.md#web)). The page shows the dark ones while it's dark,
by the browser's `prefers-color-scheme` or the app's
`appearance.setColorScheme(.dark)`. A color without its dark one is the
same in both, and one left out keeps Asuka's. Only the web takes them:
the other platforms draw their semantic colors in the system's.

## The app's files

The app's directory (`fs.appDir()`), with what `std/fs` writes there
and the crash record, is kept: its files are loaded from the browser's
storage (IndexedDB) before the app starts and written back a moment
after they change, and as the page is hidden or closed. Tabs of the app share them: a tab's writes reach the others,
which read the files again (a file a tab changed and hasn't written yet
stays its own, until it's written: the last tab to write a file keeps
it). Its caches (`fs.cacheDir()`) are kept the same way, in a database
of their own, so the browser can clear them without the app's files.
What the user drops, pastes or picks is copied into the page's
temporary directory (`/tmp`, kept while the page is open), and comes to
the app as a path.

`spawn { … }` runs on one of the page's Web Workers (up to four, started
as `spawn` first needs them), each running the app's code again, its
top-level `let`s made without a UI: a value they make, as a database
opened at the top level, is that worker's own. A worker has a copy of
the app's files: what the page changed goes with the next block it
runs, and what the block changed comes back with its result, as one
more writer (the last to write a file keeps it). `std/sqlite`'s
databases are the page's: a block can't use them.

The browser keeps an origin's storage until the user clears it, or, when
the device runs out of space, until it evicts the origin's (Safari also
does after seven days without a visit, for a site that isn't installed
to the home screen).

## SQLite

`std/sqlite` works as on the other platforms, live queries included.
It's the same SQLite and the same library of Asuka's, built to
WebAssembly (`asqlite.wasm`, which `asuka dev --platform web` and `asuka
build --platform web` build for the apps that import `std/sqlite`). It
runs in a worker of the page's, which keeps the databases of the app's
data directory in the browser's origin private file system: a database
isn't loaded into memory whole, and a commit is on disk when it returns.

What differs:

- `sqlite.open` and `db.migrate` run on the main thread elsewhere, which
  can't wait for the worker on the web: they're sent to it without
  waiting, and run before the statements after them. A database that
  didn't open fails each statement with why; a migration that failed
  (and was rolled back) fails the database's next statement with its
  error, and the console says so.
- The journal is a rollback journal, not write-ahead logging (which needs
  memory shared between connections), and a database has one connection,
  so reads wait for a write in progress rather than running beside it.
- Several tabs of the app may use one database. A tab holds it while it
  uses it, and lets it go when another asks (between transactions, never
  in one); a statement that finds another tab holding it is run again
  until the tab lets it go, for the busy timeout's 5 seconds, as SQLite
  waits for another process's lock. A tab's live queries don't hear of
  another tab's writes (as another process's, natively: `db.changed()`
  runs them again).
- A database is a file of the worker's, not of the page's file system:
  `std/fs` doesn't see it. A database anywhere but the data directory
  (`/tmp`) lasts as long as the page.
- Without the origin private file system (a page that isn't served over
  `https` or from `localhost`, as `asuka dev --lan`'s; some browsers'
  private windows), the worker keeps databases in IndexedDB, in memory
  while it uses them: a commit is kept a moment after it returns. A page
  that isn't a secure context has no Web Locks either: two tabs writing
  one database there may lose each other's writes (the console says so).
- A database an earlier version of Asuka kept among the app's files (in
  IndexedDB) moves to the worker as the app first opens it.

A page of your own that embeds an app (below) passes `sqlite`, the URL
of SQLite's module, to `start`: `"asuka/<digest>/lib/asqlite.wasm"`, the
build's file. The worker's script is Asuka's (`sqlite-worker.js`, beside
`asuka.js`).

## Native modules

What the standard library's and the platform SDK's packages do in a
browser:

- `platform/secure`: the values are encrypted (AES-GCM) with a key the
  browser keeps and won't let out (a non-extractable WebCrypto key), in
  IndexedDB. It isn't a keychain: script running in the app's origin
  can't read the key, but it can ask it to decrypt. It needs `https`
  (or `localhost`).
- `platform/device`: `info()` from the user agent (Chromium's client hints
  give the system's version and an Android phone's model); the
  clipboard, its images (PNG, saved to the caches) and
  `clipboardChanges()` (the app's own copies, and other apps' where the
  user lets the page read the clipboard); `keepAwake` (the Screen Wake
  Lock, taken again when the page shows); `browse` (a new tab);
  `pickPhotos` and `pickFiles` (the browser's file picker; the files
  copied to the caches).
- `device.shareFiles`: the share sheet with the files (`navigator.share`),
  else each file downloaded. `platform/preview`: the file opened in a new
  tab (images, video, audio, text, PDF, HTML); another type is
  `PreviewError.noViewer`.
- `platform/print`: HTML, files (PDFs in the browser's viewer) and the app's
  own web views, with the browser's print dialog; the outcome is always
  `.unknown` (a browser doesn't say). `print.pdf` isn't on the web
  (`PrintError.unsupported`): a browser makes PDFs only through its print
  dialog (Save as PDF).
- `platform/compose`: `mailto:` and `sms:` links; with attachments, the share
  sheet.
- `platform/notify` and `platform/push`: the Notification API and Web Push,
  through the app's service worker: see
  [Notifications and Web Push](#notifications-and-web-push).
- `platform/network`: online from `navigator.onLine`; Wi-Fi, cellular or
  Ethernet where the browser says (Chrome on Android), metered for
  cellular, constrained for Data Saver.
- `platform/location`: the Geolocation API; `placeName` isn't on the web (a
  browser has no geocoder). Regions and background updates are computed
  from the positions, only while the page is open.
- `platform/sensors`: the Generic Sensor API (Chrome) or device motion and
  orientation events (Safari asks on a tap), in Android's axes and
  units; no barometer or pedometer.
- `platform/speech`: `speechSynthesis`, the browser's voices.
- `platform/audio`: an audio element per player (the browser's autoplay
  rules apply: play from a tap). `platform/record`: `MediaRecorder`,
  AAC in MPEG-4 where the browser records it, else Opus in WebM.
  `platform/nowplaying`: the Media Session API.
- `platform/camera`: `take()` opens a phone's camera, or a dialog with the
  live camera and a shutter; `camera.View` is the camera in a `<video>`
  (`capture()`, `record`, and scanning codes where the browser has
  `BarcodeDetector`). `platform/scan` and `platform/vision`'s codes use
  `BarcodeDetector`, faces and text only where the browser has them.
- `platform/imageedit`: a canvas (JPEG or PNG; a remote image needs CORS).
- `platform/maps`: a map Asuka draws of OpenStreetMap's tiles, as on
  Windows: markers, their callouts, a drag, the wheel, a pinch, a double
  click and the keyboard to move it, and the attribution OpenStreetMap
  asks for. The satellite and hybrid types show the streets (no free
  imagery); `showsUser` shows the user's location once the page has the
  location permission (`platform/location` asks; the map never does). The
  tiles come from `tile.openstreetmap.org` as its usage policy asks of a
  web page (only what's on screen, the browser's cache, its Referer);
  for heavy use, pass `mapTiles` (another server's URL, with `{z}`,
  `{x}` and `{y}`) to `start` ([Embedding](#embedding)).
- `platform/crash`: the record of the run is kept in the app's files,
  as on a phone. Asuka's own fatal error writes it and stops the page's
  app; the next launch reports it. An uncaught JavaScript error
  (Asuka's, or a package's `web/` script) is an `"exception"`
  report at once, not fatal (the page goes on). A page marks its run
  alive in `localStorage`, so other open pages of the app leave it
  alone.
- `platform/biometric`: the platform's authenticator through WebAuthn (Touch
  ID, Face ID, Windows Hello, Android's screen lock), user verification
  required: the first `authenticate` makes a passkey for the site, named
  after the app. The prompt is the browser's, so `reason` and `cancel`
  don't show.
- `platform/webauth`: a popup, done when it comes back to a page of the
  app's own origin (a custom scheme can't open a page).
- `platform/screen`: `lock` (only full screen, or an installed app); hiding
  the bars is full screen.
- `platform/library`: `save` downloads the file. `platform/contacts`: `pick`
  where the browser has the Contact Picker (Chrome on Android); `add`
  downloads a vCard, and `platform/calendar`'s `add` an `.ics` file.
- `platform/nfc`: Web NFC's `NDEFReader`, in Chrome on Android over a
  secure context: reading and writing NDEF messages, making tags
  read-only, the tag's serial number; no raw commands or capacity, and no
  tag opens the page. Other browsers have no Web NFC:
  `NfcError.notAvailable` ([NFC tags](nfc.md)).
- `platform/tracking` answers `.denied` when the browser sends Global
  Privacy Control or Do Not Track, else `.allowed`, without a prompt.
- `platform/pay`: Apple Pay where the browser has it (Safari), through the
  Payment Request API, the merchant validated by the app's server
  (`onValidateMerchant`) for a domain registered with Apple; Google Pay
  elsewhere, through Google's web API (`pay.js`, loaded from
  pay.google.com); each one's own button. A sheet shows only from a tap.
- `device.haptic` is `navigator.vibrate`, where the browser has it (Chrome on
  Android).
- Not on the web, with an error that says so (or the package's own
  `notAvailable`): `platform/review`, `platform/integrity`, `platform/agerange`,
  `platform/appleauth`, `platform/icloud`, `platform/periodic`, `platform/widget`,
  `platform/liveactivity`, `platform/photos` (`.denied`: `device.pickPhotos`
  picks photos), reading contacts and calendars, `platform/transcribe`'s
  `file`, `platform/screen`'s `preventCapture` and `screenshots()`.

## Notifications and Web Push

Every web app has a service worker, which Asuka writes (`sw.js` under
`asuka dev`, `sw.<digest>.js` in a build) and the page registers as it
starts, scoped to the app's folder. It has no fetch handler, so the page
loads as it would without it; it shows the app's notifications and
pushes, and answers their clicks, whether or not a page of the app is
open. A browser runs service workers only for pages served over HTTPS,
or from `localhost` (`asuka dev`'s, but not `--lan`'s addresses).

### platform/notify

- `notify.request()` asks with the browser's prompt; browsers ask only
  from a tap or a click, so call it from one.
- `notify.show(n)` shows it through the worker, so it keeps its buttons
  and its click works after the page closes. (A page without the worker,
  an embedding page that doesn't register it, shows the page's own
  notifications, without buttons; their clicks reach only that page.)
- A notification's `id` is its tag: one with the same `id` replaces it,
  and alerts again unless `silent`. `subtitle` is the body's first line,
  `image` the picture (an `https` URL or an asset), a message's sender's
  picture (`sender`'s `image`) the icon. `badge` and `notify.setBadge`
  set the installed app's badge (`navigator.setAppBadge`).
- A category's actions (`notify.setCategories`) are the notification's
  buttons (Chrome and Edge show two; Firefox and Safari none), an action
  with a `reply` a text field where the browser has inline replies
  (Chrome on Android and desktop: the response's `reply` has the text;
  elsewhere it's a button). The categories are kept for the worker, so a
  push that names one has them too.
- A tap focuses the app's page, goes to its `url` (`onOpenURL`) and
  comes in `notify.responses()` (`action` `""`); a button comes with its
  `id`, in the page in front, else any page of the app. With no page
  open, the worker opens one (a browser runs the app's code only in a
  page), and the app gets the response as it starts, as an app launched
  by a tap does: read `notify.responses()` from the root component. An
  action without `open` doesn't bring a page that's open to the front,
  but with none open it opens one, in front. A response's `defer()` has
  nothing to hold: the page runs on.
- `notify.present(decide)` asks the app how to show one that comes while
  the page is in front, a push's too: `.banner`, `.quiet` (no sound),
  `.hide`.
- `notify.showing()`, `remove`, `removeThread`, `removeAll` and `cancel`
  reach the worker's notifications, the ones from earlier pages and
  pushes too.
- Android's channels don't exist here: accepted and ignored, as on iOS.

#### Scheduled notifications

A browser can't show a notification at a time while no page of the app
is open: there's no API for it (Chrome's Notification Triggers, which
did, were abandoned in 2021), and a service worker runs only when an
event wakes it, not on a timer. So `notify.schedule` does what a browser can:

- What's scheduled is kept in the app's store (Cache Storage, which the
  app's pages and its worker share), not in the page.
- While any page of the app is open, it shows at its time: each page
  keeps a timer for the next one (a page that schedules or cancels one
  tells the others), and the first to find it due shows it, once (a Web
  Lock). A page in the background may be late: browsers slow hidden
  pages' timers to about once a minute; it shows when the page comes
  back in front at the latest.
- One that came due while no page was open shows when the next one
  opens, at once, with its due time passed.
- Before that, as a best effort, the worker shows the due ones whenever
  something wakes it with no page open: a push, a click, and Chrome's
  periodic background sync, which the page asks for while anything is
  scheduled. Chrome grants that only to an installed app (added to the
  home screen or installed from the address bar), runs it at most about
  every 12 hours, as the browser sees fit (how much the user engages with
  the app, the battery, the network), and other browsers don't have it:
  never count on it.

For a notification that must show at its time with the app closed, send
a push from your server at that time.

### platform/push: Web Push

Web Push needs a VAPID key pair (RFC 8292): the app's server signs its
pushes with the private key, and the browser subscribes with the public
one, which goes in `asuka.toml`:

```toml
[web]
vapidPublicKey = "BKpYsAT0ADj2pHQQ..."
```

Make the pair once, and keep the private key on the server; a new pair
needs new subscriptions. Any Web Push library makes one: `npx web-push
generate-vapid-keys`, say.

`push.register()` asks to allow notifications (from a tap: browsers ask
only then), subscribes the app's worker with the key (a subscription
made with another key is replaced) and answers with a
`push.Registration`:

- `token`: the subscription's JSON, as `PushSubscription.toJSON()` has
  it: `{"endpoint": "https://...", "expirationTime": null, "keys":
  {"p256dh": "...", "auth": "..."}}`. Send it to your server as it is:
  Web Push libraries take it as it is. The browser may change it
  (`pushsubscriptionchange`): register at each start, as on the other
  platforms, and send it when it's not the one the server has.
- `service`: `"webpush"`.
- `allowed`: true. Without permission, `register` fails: a browser
  subscribes only a site that shows its pushes (`userVisibleOnly`).

It fails, saying why, without `web.vapidPublicKey`, without the worker
(an insecure page), and where the browser has no Web Push: Safari on iOS
and iPadOS has it only for an app added to the Home Screen (16.4 and
later).

A push shows as a notification, whether or not a page is open. A page
of the app that is open gets it first (`push.received()`; every open
page), may rewrite it (`push.rewrite`, which runs in the page in front,
within about ten seconds; with no page open, a push shows as it came)
and says how it shows (`notify.present`). Its tap comes in
`push.opened()` and `notify.responses()` (its notification's `remote`
set) and, with a `"url"` in its data, `onOpenURL`, as a local
notification's does.

#### What the server sends

The payload is JSON, encrypted for the subscription (RFC 8291's
`aes128gcm`) and sent with a VAPID token (RFC 8292), as every Web Push
library does. The worker reads these keys, the same a `push.Message`
has on the other platforms:

| Key | |
| --- | --- |
| `title`, `body`, `subtitle` | what shows (the subtitle is the body's first line) |
| `data` | the app's own keys, `push.Message`'s `data` (a value that isn't a string comes as its JSON); `"url"` is what a tap opens the app with (`onOpenURL`) |
| `id` | the notification's tag: a push with the same `id` replaces it |
| `category` | one of `notify.setCategories`' categories: its actions are the notification's buttons |
| `thread`, `summary` | `push.Message`'s, for `notify.removeThread` and the app |
| `image` | a picture: an `https` URL, or one of the app's assets |
| `badge` | the installed app's badge |
| `silent` | shown without a sound |

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

A push without a `title` or `body` is a data message: the open pages get
it (`push.received()`), nothing shows. Send those only while the app is
open: a browser expects each push to show a notification, and when none
shows with no page in front, Chrome shows one of its own ("This site has
been updated in the background") and Safari ends the subscription after
a few. A payload that isn't JSON shows as the body. A payload is at most
3993 bytes.

Any Web Push library encrypts, signs and sends it. From Node, with the
[`web-push`](https://github.com/web-push-libs/web-push) library:

```js
import webpush from "web-push";

webpush.setVapidDetails("mailto:ops@example.com", process.env.VAPID_PUBLIC_KEY, process.env.VAPID_PRIVATE_KEY);

// token: the app's push.register() token
await webpush.sendNotification(JSON.parse(token), JSON.stringify({
  title: "Ada", body: "Lunch?", category: "message", data: { url: "myapp://chat/7" },
}), { urgency: "high" });
// A 404 or 410 (err.statusCode): the subscription ended; forget it.
```

A server in Go can use Asuka's own sender, the repository's
[`webpush`](../webpush/webpush.go) package, with the standard library
only:

```go
import "github.com/egoist/asuka/webpush"

var sender = &webpush.Sender{
	Keys:    webpush.Keys{Public: os.Getenv("VAPID_PUBLIC_KEY"), Private: os.Getenv("VAPID_PRIVATE_KEY")},
	Subject: "mailto:ops@example.com", // where the push service can reach you
}

func notifyChat(ctx context.Context, token string) error {
	sub, err := webpush.ParseSubscription(token) // the app's push.register() token
	if err != nil {
		return err
	}
	err = sender.Send(ctx, sub, webpush.Notification{
		ID: "chat-7", Title: "Ada", Body: "Lunch?", Category: "message",
		Data: map[string]string{"url": "myapp://chat/7"},
	}, &webpush.Options{Urgency: "high"})
	if errors.Is(err, webpush.ErrGone) {
		return forgetSubscription(token) // it ended: the user, or the browser
	}
	return err
}
```

Its `Options` set the push's `TTL` (how long the push service keeps it
for a browser that's offline; four weeks by default), `Urgency` and
`Topic` (a later push with the same topic replaces one still waiting);
`webpush.GenerateKeys` makes a key pair. Other languages have their own
libraries (pywebpush, web-push-php, the `web-push` crate…): they send
the same JSON.

## Web views

A `WebView` is an `<iframe>`, made anew for each page it loads, so it
keeps its own history and adds nothing to the tab's.

A page of the app's own site (its assets, `html`, a file it wrote, any
URL of the page's origin) gets everything: the view loads it into the
frame with its real URL, `window.asuka.postMessage` and `preload` there
before the page's scripts; a `Ref`'s `run(script)` and `eval(script)`
(a promise awaited); `on loaded`, `on error` and `on state` (URL,
title, `pushState`, progress, back and forward); the `Ref`'s `goBack()`,
`goForward()`, `reload()` and `stopLoading()`; its links, forms and
`location` changes load through the view; `block` (its markup, `fetch`,
XHR, `sendBeacon`, WebSocket, EventSource, elements' `src` and `href`;
a request already under way when it's caught may still go out);
`on request` (Resource Timing); `on newWindow` (`window.open`,
`target=_blank`); `on download` (an attachment, a type a frame can't
show, `<a download>`); `webview.cookies(url)` (`document.cookie`: not
`HttpOnly` ones). `html`'s relative links resolve against the app's
assets.

Another site's page only loads, because browsers keep sites apart:
`on loaded` once it has loaded, `on error` when the site can't be
reached. The view can't tell when a site refuses to be framed
(`X-Frame-Options`, `frame-ancestors`): the browser shows its own error
page. It can't follow where the page's links lead or read its title, and
can't run script there, `block` or `preload` it, report its requests,
read its cookies or set its `userAgent` (script and cookies answer with
an error, the others warn once). The frame is sandboxed without
top-level navigation, so the page can't take the app away.

A page of another site that cooperates talks with the view through
`postMessage` (the view accepts messages only from its own frame, and
sends script only to the origin that said it's ready):

```js
// In the page: messages to the app (on message), its URL and title, and the app's eval.
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

`platform/js` runs its script in a hidden sandboxed frame of an origin of its
own, so it can't reach the app's page or storage.

## Native code

A [native package](native.md) declares its modules and views in Asuka
(`native interface`, `native component`); on the web, each is
implemented in its `web/` directory's JavaScript. `asuka native
generate` writes `<package>.asuka.js` (and `<package>.decl.asuka.js`,
its enums and errors), which imports the package's own `<package>.js`:
an ES module exporting a `make<Name>` function per module and view. For
`examples/media/segmented`'s view,

```asuka
pub native component Segmented(items: [string]) {
  prop selected: int = 0
  event changed(index: int)
}
```

the web's is:

```js
// segmented/web/segmented.js
export function makeSegmented(events) {
  const el = document.createElement("div");
  let items = [], selected = 0;
  const render = () => { /* a button per item; a click calls events.changed(i) */ };
  return {
    el, // the view, sized to the frame Asuka gives it
    setItems(v) { items = v; render(); },
    setSelected(v) { selected = v; render(); },
    measure: (c) => [items.length * 72, 32], // its size within c ({width, height, availW, availH}, NaN when free)
  };
}
```

Each package's `web/` ships at `asuka/<digest>/packages/<package>/`,
and the page imports the app's packages before the app starts. A
package's other files (its helpers, its assets) ship beside it, so it
can import them or fetch them relative to `import.meta.url`. `asuka dev`
reloads the page when one changes.

## Embedding

The page `asuka build --platform web` makes runs the app full page. To run it in an
element of a page of your own, load Asuka's loader and start the app
in it; it then follows the element's size:

```html
<link rel="stylesheet" href="asuka/<digest>/style.css">
<div id="app" style="width: 390px; height: 700px"></div>
<script type="module">
  import { start } from "./asuka/<digest>/asuka.js";
  start({ container: document.getElementById("app"), core: "asuka.<digest>.wasm", name: "com.example.app", serviceWorker: "sw.<digest>.js" });
</script>
```

Several apps can run on one page, each in its element. `serviceWorker`
registers the app's worker (the build's `sw.<digest>.js`, from the
folder whose pages it serves): without it, notifications are the page's
own and `platform/push` can't subscribe. Pass `vapidPublicKey` too for
`platform/push`, as `index.html` does, and `mapTiles` for `platform/maps`' tiles
from a server of your own (`"https://tiles.example.com/{z}/{x}/{y}.png"`;
OpenStreetMap's by default).

## What it doesn't do yet

- A text field's accessory bar shows only where there's an on-screen
  keyboard (a touch screen).
- A notification scheduled with `platform/notify` can't show at its time
  while no page of the app is open: browsers have no way to schedule one
  ([above](#scheduled-notifications)).
