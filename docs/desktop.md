# The desktop

What only a desktop has, beyond windows and the menu bar
([Navigation](navigation.md#windows)): tray icons and the Dock menu,
flyouts, apps that live in the background, global shortcuts, panels and
launchers, finding the apps installed, click-through windows, file
dialogs and the file manager, files the app opens, one instance,
quitting, recent documents, document-based apps, starting at login,
updates, Now Playing and the media keys, the displays, power, progress
on the app's icon and the system's appearance. To follow changes to
files and folders (a folder the app shows, a document another app
saves), see [Watching files](data.md#watching-files-stdwatch), which
phones and the web have too.

Each one is a package of the platform SDK (or the standard library), an
element or a function of the prelude, and works on the Mac, Windows and Linux. A phone or a
browser that hasn't one answers that it hasn't (an `unsupported` or
`notAvailable` error, or `supported()` false), or shows nothing, so an
app can offer it only where it works.

## How a desktop app is made

On the Mac, an app is one executable in `Name.app`, as an iOS app is,
with its native packages' Swift built in, for one architecture: Apple
silicon's, or Intel's with `--arch amd64` (not one universal binary of
both, which would be twice the size). On Windows and Linux
an app is one executable, on Windows with the app's icon, manifest and
version built in; a Windows or Linux build comes from any system (with
Zig), with no GTK installed to build against. Every desktop app carries
its settings from `asuka.toml` (`asuka-app.json`: its name, ID,
version, accent, URL scheme, file types, updates):

| | the app | the settings |
|---|---|---|
| macOS (`Name.app`) | `Contents/MacOS/Name` | `Contents/Resources/asuka-app.json` |
| Windows (a folder) | `Name.exe` | `asuka-app.json`, beside it |
| Linux (a prefix) | `bin/<exe>` | `share/<exe>/asuka-app.json` |

`asuka dev` runs the app in a player instead, which takes each save as
you make it (see [Development](development.md#on-the-desktop)).

## Tray icons and the Dock menu

`platform/tray`'s `Tray` puts an icon in the tray while it's shown (it
shows nothing where it's put): an `NSStatusItem` in the Mac's menu bar,
an icon in the notification area on Windows, a StatusNotifierItem on
Linux (KDE, Xfce, Cinnamon, MATE, Budgie, and GNOME with the
AppIndicator extension).

```asuka
import platform/tray

component Focus {
  state minutes = 25
  tray.Tray("timer") {
    symbol: "moon.zzz"
    title: "{minutes}:00" // beside the icon
    tooltip: "Focus timer"
    menu: [
      .button(title: "Start", run: () => start()),
      .submenu(title: "Length", items: [
        .button(title: "25 minutes", checked: minutes == 25, run: () => { minutes = 25 }),
        .button(title: "50 minutes", checked: minutes == 50, run: () => { minutes = 50 }),
      ]),
      .section(title: "", items: [.button(title: "Quit", run: () => quit())]),
    ]
  }
  Timer()
}

fn start() {}

component Timer {
  Text("25:00")
}
```

Its id names it among the app's icons (a Mac brings it back where the
user ⌘-dragged it). Its `image` (an asset or a file, drawn 16 points
tall) or `symbol` (a template image on the Mac, which follows the menu
bar's appearance; `template: true` makes an image one), `title` (the
Mac, and Linux panels that show labels), `tooltip` and `menu` are
bindings, sent again as they change, and the icon goes with the
component that shows it. Without `on click`, a click opens the menu;
with it, a click raises `click` and a right click opens the menu.

Its `menu` is `Action`s, as a `Menu`'s: `.button`s (`disabled`,
`checked`, a `.destructive` role), `.submenu`s, and `.section`s, set
apart by separators. `tray.DockMenu { items: […] }` adds items to the
Dock icon's menu on the Mac; on Windows they're the jump list's Tasks,
which reach the running app; on Linux, the launcher's quicklist. While
several show, the last one's items are the Dock's.

`examples/tray` is a timer that lives in the menu bar.

## Flyouts

A flyout is a window that hangs from a tray icon, as the panels of the
Mac's menu bar extras and of Windows' notification area do, or from a
rectangle of a window. It's a `Window` with a `flyout`:

```asuka
import platform/tray

component Power {
  state open = false
  tray.Tray("power") {
    symbol: "bolt.fill"
    menu: [.button(title: "Quit", run: () => quit())] // a right click
    on click { open = !open }
  }
  Window {
    bind open: open
    flyout: Flyout(anchor: tray.anchor("power"), placement: .bottom)
    fitContent: true
    minWidth: 260
    maxHeight: 480
    Panel()
  }
}

component Panel {
  Text("Charging")
}
```

- It has no frame and no taskbar button. On a Mac, with no `background`,
  it shows the material of menus behind its content, with rounded
  corners and a shadow, so give its content no background there;
  Windows 11 rounds it and casts a shadow over the window color; Linux
  draws neither.
- `placement` is the side of the anchor it goes to and how it lines up
  with it (`.bottom`: below, centered). Where the display has no room
  there it flips to the other side (below a Mac's menu bar icon, above
  an icon of a taskbar at the bottom of the screen), then slides into
  the display, then shrinks to fit it. `gap` keeps it off the anchor.
- `anchor` is a binding: `tray.anchor(id)` is the tray icon shown with
  that id, and `Anchor(x:, y:, width:, height:)` a rectangle of the
  window the flyout is declared in, in points from its content's top
  left (a view's frame); there it's that window's child, and moves with
  it. It's placed again as the anchor changes.
- It takes the keyboard as it shows, without bringing the app to the
  front, and closes as the user closing it does (`open` false, `close`,
  or `closeRequest` asked) when they click elsewhere, go to another
  app, or press an Escape its content leaves.
- A dialog over it: a `modal` `Window` declared in its content shows
  over the flyout, above it and centered on it, and keeps the flyout
  open until it closes, when the flyout gets the keyboard back. An
  alert over it keeps it open too.
- `fitContent` sizes it to its content (below), placed again against
  its anchor as that grows or shrinks.

| | Tray flyout | Placed against |
|---|---|---|
| macOS | a borderless, nonactivating `NSPanel` at the level of menus, over the material of menus | the copy of the status item the user clicked (each display's menu bar has one) |
| Windows | a borderless, topmost tool window with Windows 11's rounded corners | the icon's rectangle (`Shell_NotifyIconGetRect`), the taskbar's edge considered |
| Linux | an undecorated window, out of the taskbar | under X11, where the panel says the icon was clicked; under Wayland, which lets no app place a window, where the desktop puts it |
| The web | a panel in the page, the page's top right for a tray anchor | a rectangle of its window, in the page |

`examples/tray`'s icon opens one with the timer in it, and a dialog over
it.

## Windows sized to their content

`fitContent: true` (a `Window`'s, the main window's too) sizes a window
to what it shows, and follows it as it changes: a flyout with a list of nodes
that grows, a launcher whose results come and go. Its width is its
content's widest line (max-content) within `minWidth` and `maxWidth`,
at which its text wraps; its height is its content's at that width
within `minHeight` and `maxHeight`, beyond which a `Scroll` in it
scrolls. Asuka lays the content out at that size as it lays out each
frame, and the window takes it in the same pass, so it shows at its size,
never at another first. A flyout is placed again against its anchor; a
window or a panel keeps its top edge and grows down. The user can't
resize it. On the web the main window is the tab, which fills the page.

## Apps in the background

`keepRunning = true` in `asuka.toml` keeps the app running when its last
window closes: the main window hides (its `open`, bound, goes false),
and showing it again brings it back (on the Mac, so does a click on the
Dock icon; on Linux, launching the app again). `background = true` also
starts the app with no window and, on the Mac, no Dock icon: an app of
the menu bar or the tray, whose `Tray` is the user's way in. A `Window`
it opens shows as any. Quit such an app from its menus: a `quit()` in
its tray icon's menu, the Dock's Quit.

Such an app needs no main window: an app block that shows nothing
outside its windows (a tray icon, a menu bar, the `Window`s its menus
open) has none, and the windows it opens are its only ones ([Apps
without a main window](navigation.md#apps-without-a-main-window)).
`examples/tray` is one.

## Global shortcuts

`hotkey.Hotkey` registers keys that reach the app while another app
is in front, for as long as it's shown (it shows nothing itself):

```asuka
import platform/hotkey

component Launcher {
  state problem = ""

  Column {
    hotkey.Hotkey(Shortcut(key: "space", alt: true)) {
      description: "Show the launcher"
      on press { window.main().focus() }
      on error(e) { problem = e.message }
    }
    if !problem.isEmpty() { Text(problem) }
  }
}
```

`on press` runs on the main thread at each press, and the shortcut
follows its binding (keys the user picks in settings): it's registered
again as it changes. `on error(e)` says why it failed: `.taken`
(another app has the keys), `.notAllowed` (the user declined, or the
desktop doesn't know the app), `.notAvailable`, `.noKey`.
`window.main()` is the main window's handle, wherever it's called from:
`focus()` brings it to the front, `show()`, `hide()` and the rest act
on it.

- **macOS**: Carbon's hot keys, with no Accessibility permission. The
  system's own shortcuts count as taken. A press doesn't bring the app
  forward: `focus()` a window to.
- **Windows**: `RegisterHotKey`; the press lets the app take the
  foreground.
- **Linux**: under X11 the app grabs the keys itself. Under Wayland the
  desktop binds them through the GlobalShortcuts portal (GNOME 48, KDE
  Plasma 6): it may ask the user to confirm or pick other keys, and it
  needs the app installed with its desktop entry. A window `on press`
  shows takes the focus with the press's activation token.
- Phones and the web have none (`.notAvailable`).

## Panels and launchers

A panel is the window of a launcher, a command palette or a quick
entry, as Spotlight's and Raycast's are: summoned from anywhere by a
global shortcut, it floats above every app's windows, on whichever
Space or virtual desktop the user is on and over apps in full screen,
stays out of the taskbar, the Dock's windows and the app switcher, and
takes the keyboard without bringing the app to the front on a Mac, so
the app the user was in keeps its menu bar and gets the keyboard back
as the panel goes. It's a window with `panel: true`: a launcher's main
window, in an app that runs in the background (`background = true`):

```asuka
import platform/hotkey

state shown = false

app {
  Window {
    bind open: shown // hidden until the shortcut shows it
    panel: true
    fitContent: true
    minWidth: 640
    maxWidth: 640
    maxHeight: 440
    on closeRequest { shown = false } // Escape, a click elsewhere: hide it
    Launcher()
  }
}

component Launcher {
  hotkey.Hotkey(Shortcut(key: "space", alt: true)) {
    description: "Show the launcher"
    on press { shown = !shown }
  }
  Search()
}

component Search {
  TextField { placeholder: "Search" }
}
```

- It has no frame. On a Mac, with no `background`, it shows the
  material of popovers behind its content, rounded and with a shadow;
  Windows 11 rounds it; Linux draws it as a window's content.
- Without `x` and `y` (or a `stateKey`), it shows on the display with
  the pointer, centered across it, a third of the way down, each time
  it shows; `w.setPosition` and `w.center()` place it for good. With
  `fitContent` it grows down from its top as results come.
- It closes as the user closing it does on an Escape its content leaves
  (a text field's too) and as it loses the keyboard: a click elsewhere,
  another app. Hide it instead (`on closeRequest`, above), so it shows
  again at once, as it was; the main window of an app that keeps
  running hides as it closes anyway.
- Showing it (`open` true, `w.show()`) gives it the keyboard;
  `w.focus()` does the same, never bringing the app forward.

| | A panel |
|---|---|
| macOS | an `NSPanel` with the nonactivating style, on every Space and over full-screen apps (`canJoinAllSpaces`, `fullScreenAuxiliary`), out of the window cycle, floating; never the main window |
| Windows | a borderless topmost tool window owned by a hidden window (no taskbar button), Windows 11's rounded corners; it takes the foreground, which a global shortcut's key press lets it |
| Linux | an undecorated window; under X11 a utility window kept above, on every workspace, out of the taskbar and pager; under Wayland an overlay layer-shell surface where the app runs with gtk4-layer-shell (`LD_PRELOAD`) on a compositor that has the protocol (KDE Plasma, Sway, Hyprland, COSMIC), else a window as others (GNOME places it, and keeps nothing above) |
| The web | a panel over the page, a third of the way down |

`examples/launcher` is a Spotlight of its own: ⌥Space (Alt+Space) shows
a panel with a search field, the apps installed as you type, with their
icons, and Enter opens the first.

## Finding apps

`platform/apps` lists the apps installed, as a launcher searches them:

```asuka
import platform/apps

component Apps {
  state installed: [apps.App] = []
  state query = ""
  state found: [apps.App] = []
  state icon = ""

  task { installed = (try? await apps.list()) ?? [] }
  task(installed, query) {
    found = (try? await apps.search(installed, query)) ?? []
    // A PNG for Image.
    if let first = found.first() { icon = (try? await apps.icon(first, 32)) ?? "" }
  }

  Column {
    TextField { bind text: query }
    for a in found key a.id {
      Button(a.name) { on press { try? await apps.open(a) } }
    }
    if !icon.isEmpty() { Image(icon) }
  }
}
```

- `list()` gives each app's `name` (as the system shows it, in the
  user's language), `id` and `path`, sorted by name; it reads the disk
  off the main thread. List them again as the panel shows: an app may
  have come or gone.
- `search` ranks them for what the user typed, ignoring case and
  accents: the name itself, then its start, then a word's start or the
  initials ("vsc" for Visual Studio Code), then anywhere, then its
  letters in order.
- `icon` draws an app's icon at a size in points as a PNG file in the
  app's caches, kept for the next time, and gives its path.
- `open` starts the app, or brings it to the front. Hide the panel
  first, so the app the user was in has the keyboard until the opened
  one takes it.

| | Where | ID | Icon | Open |
|---|---|---|---|---|
| macOS | `/Applications`, `/System/Applications`, `~/Applications` and a folder below each, Finder | the bundle identifier | the workspace's icon for it | `NSWorkspace` |
| Windows | the Start menu's apps (the shell's Applications folder), packaged ones too | the AppUserModelID | the shell's image for it | the shell |
| Linux | the desktop entries a menu shows (XDG, Flatpak's and Snap's too), by `OnlyShowIn`, `NoDisplay`, `TryExec` | the desktop file id | the entry's icon from the theme | GIO, with the display's launch context |

Phones and the web throw `apps.AppsError.unsupported`.

## Click-through windows

`clickThrough: true` (or `w.setClickThrough(true)`) lets the
mouse go through a window to what's behind it, another app's window or
the desktop: an overlay, a HUD, a desktop pet, made `transparent` and
`alwaysOnTop`. `w.clickThrough` reads it, followed. The window
takes no clicks, hover or wheel at all while it's on; switch it off to
let the user use it.

```asuka
state hud = true

app {
  Window {
    bind open: hud
    transparent: true
    alwaysOnTop: true
    clickThrough: true
    width: 240
    height: 80
    Meters()
  }
}

component Meters {
  Text("CPU 12%")
}
```

It's `ignoresMouseEvents` on a Mac, a layered `WS_EX_TRANSPARENT`
window on Windows and an empty input region on Linux
(`gdk_surface_set_input_region`). On the web a window in the page lets
clicks through to the page; nothing lets them out of the browser.

## File dialogs

`platform/dialog` shows the system's file dialogs and gives the user's own
paths, not copies:

```asuka
import platform/dialog
import std/fs
import platform/shell

async fn export(notes: string) throws {
  let md = [dialog.Filter(name: "Markdown", types: ["text/markdown"], extensions: ["md"])]
  if let path = try await dialog.save(name: "Notes.md", button: "Export", filters: md) {
    try fs.writeText(path, notes)
    try? await shell.reveal(path)
  }
}
```

`dialog.open` (several files with `multiple`, `filters` by MIME type
such as `image/*` or by extension), `dialog.save` (a suggested `name`;
the dialog asks before replacing a file), `dialog.chooseFolder` and
`dialog.chooseFolders`. `title`, `button` and `start` (a folder, or a
file in it) set them up. A cancel is no path (`none`, or an empty list)
and no error.

On the Mac they're sheets on the window; on Windows the common item
dialogs, modal to the window; on Linux GTK's file dialog, which is the
desktop's portal in a Flatpak. In a browser, open and chooseFolder copy
what's picked into the app's files, and the file save gives is written to
the file the user chose (Chrome, Edge) or downloaded. On iOS and Android,
open is the document picker (copies, as `device.pickFiles` gives), and
save and chooseFolder throw an `unsupported` error.

A test answers the dialogs itself: `dialog.fake()` installs a
`FakeDialogs` whose `opens`, `saveTo` and `folder` are what the user
picks (none, or `[]`: a cancel), and whose `shown` lists the dialogs
asked for (`"save Untitled.txt"`).

## The file manager

`platform/shell` does what the file manager does with a file:
`shell.reveal(path)` shows it selected in its folder (Finder, Explorer, or the
desktop's file manager), `shell.open(path)` opens it in the user's app for its
type (`shell.ShellError.noApp` if there's none) and a folder in the file
manager, `shell.trash(path)` moves it to the trash or the Recycle Bin and
never deletes it for good (it throws instead), and `shell.beep()` sounds
the alert. Phones and the web have no file manager: reveal and trash
throw an `unsupported` error, and on the web open shows the file in a
new tab. To show a file's contents in the app, use `platform/preview`.

## Opening files

Declare the kinds of files the app opens in `asuka.toml`'s `fileTypes`
([Shipping](shipping.md)), and handle them with the app block's `on
files(paths)` (or `onOpenFiles` in a component's body, while it lives):

```asuka
state opened: [string] = []

app {
  on files(paths) {
    for p in paths { opened.push(p) }
  }
  Column {
    for p in opened key p { Text(p) }
  }
}
```

Files opened together arrive in one call, as absolute paths: a double
click in Finder, Explorer or Files, Open With, a file dropped on the
Dock icon, a recent document chosen, and on Windows and Linux the files
on the command line. Those the app launched with wait for the first
handler. On iOS the path is the document where it is, readable until
the app quits; on Android and the web it's the app's own copy in its
caches.

## One instance

On Windows and Linux an app with an `id` runs once: starting it again
brings the running one to the front, the app block's `on
secondInstance(args, dir)` gets the new launch's arguments and working
directory, and the files and links among them go to `on files(paths)`
and `on openURL(url)` too:

```asuka
state newWindow = false

app {
  on secondInstance(args, dir) {
    if args.contains("--new-window") { newWindow = true }
  }
  Window { Main() }
}
component Main {
  Text(if newWindow { "A new window" } else { "Main" })
}
```

`onSecondInstance(fn([string], string))` does the same from a
component's body, for as long as it lives. Set `singleInstance = false`
in `asuka.toml` to run a new instance each time. The Mac always runs an
app once and passes files, not arguments: there it never runs.

## Quitting

`quit()` quits as the user's Quit does. The app block's `on quit(e)`
runs first, however the quit started (⌘Q, the Dock, closing the last
window, `quit()`, Ctrl+C or SIGTERM, logging out), and `e.cancel()`
keeps the app running, to ask about unsaved changes; `e.canceled()`
says whether a handler did. When `e.sessionEnding` is set the quit
can't be canceled: save at once, without a dialog.

```asuka
state unsaved = false
state confirmQuit = false

app {
  on quit(e) {
    if unsaved && !e.sessionEnding {
      e.cancel()
      confirmQuit = true // asks, then quit() again once it's saved
    }
  }
  Window { Editor() }
}
component Editor {
  Text("Editor")
}
```

`onQuit(fn(QuitEvent))` does the same from a component's body, for as
long as it lives. A window's own close is asked first, through its
guards ([Navigation](navigation.md#asking-before-a-window-closes)).

`relaunch()` quits the same way (the handlers can cancel it) and starts
the app again once it has exited, with the same arguments: after an
update, or a setting that takes effect at launch. Phones and the web,
whose apps don't quit themselves, ignore both.

## Updates

`platform/update`'s `update.UpdateWindow()`, shown once in the app's main
window, is Sparkle's updater: it checks for a new version soon after
launch and once a day, shows its release notes with Install Update,
Skip This Version and Remind Me Later, downloads it with a progress bar
(only what changed, when there's a delta from the running version),
checks that it's signed with the app's key, installs it and offers
Relaunch Now. `update.AppMenu()` in the `MenuBar` puts "Check for
Updates…" after About on the Mac (in Help elsewhere);
`update.CheckForUpdates()` is the item alone. `update.check()`,
`Release.install(progress:)`, `update.relaunch()` and the updater's
state (`update.updater`) make an interface of the app's own.

```asuka
import platform/update

app {
  Window {
    title: "Notes"
    update.UpdateWindow()
    MenuBar { update.AppMenu() }
    Text("Notes")
  }
}
```

`asuka update keygen` makes the key pair, `asuka.toml`'s `updates` says
where releases go (GitHub's releases, an S3 bucket or any HTTPS
directory, or a server of yours), and `asuka publish --platform mac`
(`windows`, `linux`) builds, signs and uploads them
([Shipping](shipping.md#updates)). The app downloads and puts the
update in place: a Mac app's new bundle waits for it to quit; Windows and
Linux replace the app's files while it runs. An app that the system's
packages, Flatpak, Snap or an app store installed is updated by them:
`await update.unsupported()` says so, and phones answer `.notAvailable`.
On the web, a page older than the site offers to reload.

## Recent documents

Call `recent.add(path)` (`platform/recent`) for each document the user
opens or saves.
The Mac lists them in the File menu's Open Recent and the Dock menu,
Windows in the jump list, Linux in the user's recent files. Choosing one
opens it through `on files(paths)`. `await recent.list()` returns them
newest first, skipping files that are gone, and `recent.clear()`
removes the app's own.

## Documents

`platform/document` makes an app whose windows each show a file, as a
text editor does: a document's model, and the windows and screens
around it. Describe the kind of document and its editor:

```asuka
import platform/document

let types = [document.Type(name: "Plain Text", extensions: ["txt"])]

app {
  document.Documents(types) { editor: doc => Editor(doc) }
}

component Editor(doc: document.Document<string>) {
  if let text = doc.content {
    TextField {
      text: text
      multiline: true
      grow: 1
      on change(t) { doc.edit(t) }
    }
  }
}
```

List the same types in `asuka.toml`'s `fileTypes`, so the system offers
the app for them. `examples/textedit` is an editor of this kind, with a
status line and a bar for a file another app changed.

A `Document` is a store: its `content` (none until the file is read)
and its `state` (`.loading`, `.saved`, `.edited`, `.saving`,
`.conflict`, `.failed`, and `error` saying why). `doc.edit(c)` changes
the content, and it's saved a moment after the last change; `save()`
saves now, `saveSoon()` from code that can't wait (a key's, a menu
item's). A file another program changed since it was read is a
conflict: `saveAnyway()` keeps the app's content, `revert()` takes the
file's. Its `file` is where it's kept ("" for an untitled one) and its
`name` what it's called; `saveAs(path)`, `rename(name)` and
`moveTo(dir)` move it, `unsaved()` says whether it has changes its file
hasn't, and `discard()` lets them go. `document.open(path, read:,
write:)` makes a document of any type of content, its own read and
write functions turning bytes into it and back; `Documents` edits text.

What the user gets:

- **On a desktop, a window per document**, named after its file, with
  the file's icon in a Mac's title bar and its edited dot in the close
  button while it has unsaved changes. The app's main window, whose
  content `Documents` is, is hidden as it starts. On Windows and Linux the app ends with its last window,
  as Notepad does; a Mac's keeps running, its menu bar's New and Open…
  there.
- **The File menu**, a Mac's as TextEdit's: New (⌘N) opens an untitled
  document, Open… (⌘O) the system's dialog, Open Recent the system's
  recent documents, then Close (⌘W), Save (⌘S), Duplicate (⇧⌘S), Save
  As… (⌥⇧⌘S), Rename…, Move To… and Revert to Saved. On Windows and
  Linux, Notepad's and GNOME Text Editor's: New, Open…, Open Recent (its
  documents and Clear Menu), Save, Save As… (Ctrl+Shift+S), Revert,
  Close and Quit. Each acts on the document whose window is in use.
  Its words follow the app's language (English, German, Spanish,
  French, Japanese and Chinese; English otherwise).
- **Autosave**: a document is saved a moment after the user stops
  typing; nothing asks. One that can't be saved in place (untitled,
  changed by another app, its file not writable) keeps its changes in a
  recovery copy in `document.recoveryDir()`, and a quit keeps them there
  too: the next launch opens those documents again, edited.
- **Questions where changes would be lost**: closing an untitled
  document with changes asks whether to keep it (Save…, Delete, Cancel);
  closing one whose save failed or met another app's change asks
  whether to save (Save, Don't Save, Cancel); Save over another app's
  change asks to Save Anyway or Revert, and Revert to Saved asks first.
  Move To… asks before replacing a file of its name.
- **On a phone and in a browser**, one document at a time: the app
  opens on a list of its documents, with New, and a document's screen
  is pushed over it, saved as the user edits. A row's swipe Duplicates,
  Renames (asking the name) or Deletes it (asking first).

## Starting at login

Offer `try await login.set(true)` (`platform/login`) as a setting, and
read it back with `await login.enabled()`. It's a login item on macOS 13 and later (a packaged
app only; `LoginError.needsApproval` until the user allows it in System
Settings), a Run entry on Windows, and an autostart entry on Linux (the
Background portal in a Flatpak). `await login.openedAtLogin()` tells a launch at
login apart, so the app can start out of sight.

## Now Playing and the media keys

`nowplaying.NowPlaying` puts what the app plays in the system's media
controls, while it's shown, and their buttons and the keyboard's media keys (play/pause,
next, previous) reach its handlers: Control Center's and the menu bar's
Now Playing on the Mac, the media controls of the volume flyout and
quick settings on Windows (the app window's
SystemMediaTransportControls), an MPRIS player on Linux, which GNOME's
and KDE's controls and `playerctl` drive. It's the same package as on
phones, where it's the lock screen's controls
([Device and services](device.md#now-playing-and-the-media-keys-platformnowplaying)):

```asuka
import platform/audio
import platform/nowplaying

component Spring {
  event next

  let music = audio.Player("music/spring.mp3")

  nowplaying.NowPlaying {
    title: "Spring"
    artist: "Vivaldi"
    // Its state and position; play, pause, seek on it.
    player: music
    on next { emit next }
  }
}
```

## Displays, power and the app's icon

`screen.displays()` follows the displays (tracked: read in a binding,
it watches them while it's read): where each one is, its work area,
scale and refresh rate, and which is primary; `screen.nearest` and
`screen.matching` pick the one under a point or a window. Wayland has no
work area: it's the display's bounds there.

`power.status()` follows the battery, Low Power Mode and the thermal
state. `power.suspends()`, `resumes()`, `locks()` and `unlocks()` are
streams of the computer sleeping and waking and the screen locking, and
`power.idleTime()` is how long the user has been away from the whole
computer. Phones have lock and unlock, but no suspend or idle time; the
web, the battery alone. The time since the user's last input in the app
itself, for an inactivity timeout, is `activity.idleTime()`
([platform/activity](device.md#the-users-activity-platformactivity)).

```asuka
import platform/power
import platform/screen

component Status {
  let p = power.status()
  let displays = screen.displays()

  task {
    for await _ in power.suspends() { print("going to sleep") }
  }

  Column {
    Text(if p.battery { "Battery {(p.level * 100).fixed(0)}%" } else { "No battery" })
    Text(displays.map(d => "{d.name} {d.bounds.width}x{d.bounds.height}@{d.scale}x").joined(separator: "\n"))
  }
}
```

`dock.setProgress` shows a task's progress on the Dock icon, the
taskbar button, or a Linux dock that has the Unity launcher API (GNOME's
own dash shows none). `dock.requestAttention` bounces or flashes the
icon while the app is in the background. The count on the icon is
`notify.setBadge`.

`appearance.current()` gives the system's accent color (a `Color?`),
Increase Contrast and Reduce Transparency, tracked, for what the app
draws itself; the views Asuka shows follow them already.
