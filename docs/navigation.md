# Navigation

An app's structure is made of stacks of screens, tabs, sheets, alerts
and menus, and on a desktop, windows and the menu bar. Each one is the
platform's own:

- **iOS**: a `UINavigationController`, a `UITabBarController`, sheet
  presentations, `UIAlertController`, `UIMenu`.
- **Android**: a top app bar with Material transitions and predictive
  back, Material's bottom navigation bar, a `BottomSheetDialog`, Material
  dialogs, `PopupMenu`.
- **macOS**: the window's toolbar (the back button, the screen's bar
  buttons and search field) and title, an `NSTabView`, sheet windows,
  `NSAlert`, `NSMenu`, and the menu bar.
- **Windows**: a header bar over the screen, a tab bar as WinUI's
  `NavigationView` at the top, modal windows, task dialogs, popup menus,
  and the window's menu bar, drawn as Windows 11's.
- **Linux**: libadwaita's: the window's header bar, `AdwNavigationView`,
  a view switcher, `AdwDialog`, `AdwAlertDialog`, GTK's popover menus,
  and the main menu.
- **The web**: a header bar of Asuka's, `<dialog>`s and popovers, and
  the browser's back button (see [the web](web.md)).

This page covers them, along with going back, routes (every screen at
an address, as on the web), and the links and shares that open the
app. [`examples/navigation`](../examples/navigation) has
all of them; [`examples/notes`](../examples/notes) has a menu bar and a
window.

## Stacks

`Stack` is a navigation stack. Its child is its first screen, and code
under it pushes more with `push`.

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

- `push(Screen(…))` shows a screen on top, with the platform's push
  animation.
- `pop()` removes the top screen. It never pops the first.
- `depth()` is how many screens the stack holds.

`push`, `pop` and `depth` act on the nearest stack around the code that
calls them: the screen's own, from its handlers and from what they made
(an alert's buttons, a menu's or a swipe's actions, a snackbar's
action, the rest of a handler after an `await`), on the first screen as
on any other.

On iOS and Android each stack has its own bar, and so it does on
Windows (a header bar Asuka draws over the screen, as WinUI's) and
on the web. On a Mac and on Linux a window has one bar, as their apps
do, and it belongs to the innermost stack on screen (in tabs, the shown
tab's): its back button, its top screen's title, bar buttons and search
field. On a Mac that's the window's toolbar and title; on Linux the
window's `AdwHeaderBar`. Pushes and pops slide on macOS, Linux and the
web; on Windows, screens change without a transition.

A stack fills the space it's given. Each screen is an
[owner](concepts.md): popping it, by code or by the user's back gesture,
disposes it, stopping its effects. A screen pushed again starts fresh,
so keep what must outlive it in state outside it (a top-level `state`,
a store), or in [storage](data.md).

## Screen options

A `screen` declaration's block sets its options before its content:

- `title` is the navigation bar's title (the top app bar's on Android,
  the header bar's on Windows, Linux and the web; on a Mac, the window's
  title; in a browser, the page's title too). It's a binding, so it can
  change.
- `toolbar { … }` holds the bar's buttons.
- `backGesture: false` turns the back gesture off for the screen (see
  [Back](#back)).
- `bind search: query` puts a search field in the bar.
- `zoomFrom` zooms the screen out of a view on the screen below.
- `largeTitle: true` shows the title large at the top of the screen on
  iOS, collapsing into the bar as it scrolls. Android's top app bar,
  the desktops' bars and the web's stay as they are. The bar follows
  the first scroll view that takes up most of the screen, so something
  can be behind it.
- `barColor` is the top app bar's color on Android, and the header's on
  the web, to continue a background the screen draws itself. iOS's bar
  is clear at the top of a screen already; the desktops' bars keep
  their own.
- `bottomToolbar { … }` holds buttons in a bar at the bottom of the
  screen, and `primary { … }` the screen's main action (see [Toolbar
  and primary action](#toolbar-and-primary-action));
  `primaryExtended: true` makes it Android's extended floating action
  button.

### Bar buttons

Each is a `Button` in the screen's `toolbar`. With an `icon`, the bar
shows the icon instead of the title, and screen readers read the title
(on a desktop, it's the icon's tooltip too).

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

On Android, actions go in the top app bar as there's room, and the rest
go in its overflow menu. A long press on an icon shows its title. On a
Mac they're toolbar items, on Windows buttons at the header bar's end,
and on Linux buttons in the header bar.

### Toolbar and primary action

`bottomToolbar` puts buttons in a bar at the bottom of the screen, as
SwiftUI's `ToolbarItem(placement: .bottomBar)`, and `primary` is the
screen's main action, such as compose or add: Material's floating
action button. They're `Button`s, as the bar's are: an `icon` is what
the button shows and its title what screen readers say, `disabled` dims
one, a `menu` makes one open a menu, and what they read is followed.

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

- iOS: the navigation controller's toolbar, with flexible space between
  the buttons, shown while the screen is on top and sliding with pushes
  and pops. iOS has no floating button, so `Primary` is the toolbar's
  trailing item, prominent on iOS 26 (Mail's Compose); a screen with only
  a `primary` gets the toolbar for it. The screen's safe area takes the
  toolbar, as UIKit's does. A pushed screen with a `bottomToolbar` hides its
  tabs' tab bar (from iOS 18), as a message in Mail does: iOS 26's
  floating tab bar would lie over the toolbar.
- Android: Material 3's `BottomAppBar`, the buttons as icon buttons from
  its start, those that don't fit in its overflow menu. `primary` is a
  `FloatingActionButton` at the bottom end of the screen, or docked in
  the bar (with no shadow) when there's a `bottomToolbar`. With
  `primaryExtended` it's an `ExtendedFloatingActionButton`, its title
  beside its icon, which shrinks to the icon as the screen's content
  scrolls down and extends as it scrolls up (above the bar when there's
  one). Both are the screen's own, so they go with it in pushes, pops and
  predictive back; the bar takes the navigation bar under it, and the
  screen's safe area takes the bar. A pushed screen with a `bottomToolbar`
  hides its tabs' navigation bar, as Material has it.
- macOS: SwiftUI's bottom bar is iOS's, so a Mac shows the `bottomToolbar`'s
  buttons in the window's toolbar after the bar's, and `primary` last,
  prominent (macOS 26's accent-tinted toolbar item).
- Windows: a command bar under the screen (Fluent's `CommandBar`): the
  buttons with their icons and titles from the leading edge, the ones
  that don't fit in a More options (…) menu, and `primary` as an accent
  button at the trailing end.
- Linux: a `GtkActionBar` at the bottom of the page, in an
  `AdwToolbarView` (GNOME's action bar): the buttons at its start,
  `primary` at its end as a `suggested-action` button with its icon and
  title.
- The web: a bar at the bottom of the stack (the buttons that don't fit
  in a More options menu), and `primary` a floating button over the
  bottom trailing corner; an extended one shows its title, folding to its
  icon as the content scrolls down.

A press runs the button's `on press`; in a test, `app.tap("Compose")`
presses it, and `app.in(.bottomToolbar)` narrows a tap to that bar.

### Search

`bind search: query` puts a search field in the bar, as SwiftUI's
`searchable` does. Typing writes `query`, and writing it from code
changes the field. `searchPlaceholder` is "Search" when it's empty, and
`on searchSubmit` runs on the keyboard's search key, or Enter on a
desktop.

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

On iOS it's a `UISearchController` under the title, which stays shown
as the screen scrolls. On Android it's Material's pattern: a search
button in the top app bar that expands into the field. `searchButton: true` keeps
it out of the way on iOS too, for a screen with its own controls at the bottom:
a button in the bar on iOS 26, and before that, hidden above the content until
it's pulled down.

On a desktop and on the web, the field is in the screen's bar:

- **macOS**: a search field in the toolbar (`NSSearchToolbarItem`).
- **Windows**: a Fluent text box with the search glyph, before the bar
  buttons, as WinUI puts a search box in a command bar.
- **Linux**: a search button in the header bar, and GTK's search bar
  under it, as GNOME's apps search; typing anywhere in the window opens
  it.
- **The web**: a field under the header (a button in it, with
  `searchButton: true`).

### Zooming from a view

`zoomFrom` is the `shared` key of a view on the screen below, such as a
photo in a grid or a card. The screen zooms out of that view as
it's pushed, and back into it as it's popped. See
[shared elements](motion.md#shared-elements).

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

On iOS this is the system's zoom transition (iOS 18 and later; before
that, the screen pushes as usual). Android does its own zoom, and
popping with the predictive back gesture goes back into the view. The
desktops and the web push and pop as usual.
[`examples/gallery`](../examples/gallery)'s photo grid does this.

## Back

Each platform goes back its own way, and Asuka leaves it so:

- **iOS**: the bar's back button, and a swipe from the screen's leading
  edge. `backGesture: false` turns the swipe off. The back button still pops.
- **Android**: the system back, by the button or the gesture, and the
  up arrow in the top app bar. Back pops the innermost stack on screen
  that has a screen to go back to: in tabs, the shown tab's stack. With
  the predictive back gesture, the top screen shrinks and follows the
  finger, showing the one below. When no stack has a screen to pop,
  back does what it does in any app, and leaves it.
  `backGesture: false` turns off the predictive animation, but back
  still pops the screen.
- **macOS**: the toolbar's back button, and View ▸ Back (⌘[), which is
  off while there's no screen to go back to. Escape dismisses a sheet.
- **Windows**: the header bar's back button, and Alt+Left, which pops
  the innermost stack of the window it's pressed in. Escape dismisses a
  sheet.
- **Linux**: libadwaita's: the header bar's back button, Alt+Left,
  Escape, and a swipe on a touchpad or touch screen. `backGesture:
  false` turns the swipe off; the back button still pops.
- **The web**: the header's back button, the browser's back button (and
  a phone's back gesture), Escape, Alt+Left (⌘[ on a Mac), and a swipe
  from the leading edge on a touch screen. See [the web](web.md).

`backGesture` changes nothing on macOS and Windows, which have no back
gesture. Whichever way the user goes back, the screen is disposed, as
with `pop()`. Back closes an open sheet or dialog first.

### Taking back yourself

`onBack { … }` in a component's body lets it decide, when the user goes
back, whether to go: to ask before throwing away a draft, or to close something of
the screen's own first (a search, an expanded panel). The handler
returns true when it took the back; when every handler returns false,
going back goes on.

```asuka
screen Editor {
  state text = ""
  state saved = ""
  state discarding = false

  title: "Editor"
  onBack {
    if text == saved { return false } // nothing to lose: the screen pops
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

It belongs to where the component is built, and goes with it:

- **A pushed screen**: its back button, Android's back, the edge swipe
  on iOS, ⌘[ on a Mac, Alt+Left on Windows and Linux, the browser's
  back. While the screen has a handler, iOS's and Linux's swipe is off
  on it (the swipe can't be asked, only stopped, as with UIKit's own
  apps) and Android doesn't preview the pop. iOS's back button is the
  screen's `backAction`.
- **A sheet**: a swipe down, a tap outside, Android's back, Escape on a
  desktop or in a browser, a sheet window's close button on Windows. A
  full-screen sheet has only Android's back and Escape.
  While it has a handler, the sheet can't be swiped away: trying asks
  the handler (iOS's `isModalInPresentation`; a sheet that isn't
  cancelable on Android). When the handler lets it go, the sheet closes
  and its `on dismiss` runs, as if the user had dismissed it.
- **Elsewhere** (outside any screen or sheet, or on a stack's first
  screen, which back leaves): Android's back with no screen to pop,
  where the app would otherwise close, and the browser's back, where
  the page would go back. iOS and the desktops have no such back.

The latest handler is asked first. `pop()` and closing a sheet from
code don't ask: only the user's going back does.

## Split views

`Split` is an app's top level when it has many destinations: a sidebar
of them, and the chosen one's screens beside it (SwiftUI's
`NavigationSplitView`). Its children are `Destination`s, in `Sidebar`
sections or alone.

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

Each destination's content is the first screen of a stack: code under
it pushes with `push`, and choosing another destination replaces the
stack's screens with its first, at once, disposing them. The first
screen's title is the destination's unless the screen has its own.
`bind selected` is the chosen destination, counted across the sections;
the user's choice writes it, and writing it shows another. A
`Sidebar`'s title heads its destinations (a destination's own
`section:` names one too).

| | Compact (a phone) | Wide |
|---|---|---|
| iOS | the split view collapsed into a stack, as iOS does it: the sidebar is the first screen, an inset grouped list; a row pushes its destination, and back returns to the list | `UISplitViewController`'s sidebar beside the content (Liquid Glass on iOS 26), which the user can hide |
| Android | Material's modal navigation drawer, opened by the menu button of the destination's first screen (or a swipe from the edge), closed by choosing or by back | a standard drawer, standing open beside the content (840dp and wider) |
| macOS | | `NSSplitViewController`, its sidebar a source list the user can collapse |
| Windows | | WinUI's `NavigationView` pane: the destinations beside the content, the chosen one with an accent pill |
| Linux | under 500 points wide, libadwaita's collapsed split view: the sidebar, then the chosen destination's screens, with a back button to the sidebar | `AdwNavigationSplitView`: a sidebar page of its own, its title in its own header bar, beside the content under the window's |
| The web | a drawer, opened from the detail's header | a sidebar beside the detail |

A destination's `badge` is shown at its row's trailing end, as Mail
counts a mailbox's unread messages (SwiftUI's `badge` on a sidebar row):
usually a count, though any short text works, and empty for none. It's
a binding, so it follows your data (`badge: "{unread}"` above); the row
changes in place, keeping the sidebar's selection, scroll and focus.

| | A destination's badge |
|---|---|
| iOS | a label accessory at the cell's trailing end, in the secondary color |
| Android | a counter at the drawer item's end, as Gmail's |
| macOS | the source list's count, in the secondary color, as Mail's mailboxes |
| Windows | Fluent's `InfoBadge` at the pane item's trailing end |
| Linux | a dimmed count at the row's end |
| The web | a count at the row's end |

Screen readers say it with the row's title, a count as so many items:
"Inbox, 3 items" (Android: Material's badge words, "3 new
notifications").

## Tabs

`Tabs` shows one page at a time, with a tab bar:

- **iOS**: a `UITabBarController`.
- **Android**: Material's bottom navigation bar.
- **macOS**: an `NSTabView`, its tabs on top.
- **Windows**: a bar of tabs on top, as WinUI's top `NavigationView`: an
  icon and a title each, an accent pill under the selected one; the
  arrow keys move between them.
- **Linux**: an `AdwViewStack`, its view switcher in the header bar on a
  wide window and a bar at the bottom of a narrow one (under 550
  points), as GNOME's apps have it.
- **The web**: a tab bar at the bottom.

Each `Tab` has a title, an `icon` and its content. `bind selected` is
the shown tab's index. Choosing a tab writes it, and writing it
switches tabs.

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

A tab's `badge` is shown on it (SwiftUI's `badge` on a tab item,
Compose's `BadgedBox` on a `NavigationBarItem`): usually a count, "3",
though any short text works, and empty for none. It's a binding: as it
changes, the tab alone changes.

| | A tab's badge |
|---|---|
| iOS | `UITabBarItem`'s `badgeValue`, the system's red badge |
| Android | the item's `BadgeDrawable`: a count as its number (999+ past 999), other text as its text |
| macOS | in the tab's label, "Inbox (3)": `NSTabView` draws its tabs' labels alone |
| Windows | Fluent's `InfoBadge` at the tab's trailing end |
| Linux | a count as the page's `badge-number`, beside its icon in the view switcher; other text as `needs-attention`'s dot |
| The web | a red badge at the tab's icon |

Screen readers say it with the tab's title, a count as so many items:
"Inbox, 3 items" (the platform's own words on iOS and Android).

Give each tab its own `Stack` so that it has a bar and keeps its own
history, as the platforms' apps do. A tab's content is built when it's
first chosen, and kept while you switch between them. Tab icons are
symbols, like a button's: see [Symbols and icons](components.md#symbols-and-icons).
On Android an icon must be one that Android has.

## Sheets

`Sheet` presents its content in a modal sheet while `open` is true:

- **iOS**: a sheet presentation with detents.
- **Android**: a `BottomSheetDialog`.
- **macOS**: a sheet window, attached to the window in use.
- **Windows**: a modal window over the window in use, which is disabled
  meanwhile.
- **Linux**: an `AdwDialog`: a floating dialog, or a bottom sheet in a
  narrow window.
- **The web**: a modal `<dialog>`, a bottom sheet on a narrow window and
  a centered card on a wide one.

It takes no room where it's declared, so put it anywhere in the screen
that opens it.

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

- `detents` are the heights it can rest at: `.medium` (half the
  screen), `.large` (all of it), or both, which starts at medium and
  lets the user drag it up. With none, it's large. When the
  keyboard shows, a sheet that can be large goes large. On a desktop a
  medium-only sheet is half the window's height, and any other most of
  it; the user resizes a sheet window on a Mac. `.fit` makes it
  as tall as its content instead, as a Mac's sheets are: for a form or a
  confirmation, whose content has a height of its own. It's a sheet's
  width, or wider when its content can't be that narrow (a fixed
  `width`), so the content shows whole: while it shows, its window
  can't be made narrower than the sheet and a margin on each side, and a
  window already narrower widens. It follows the
  content as it changes, up to the most a sheet may be, beyond which a
  `Scroll` in it scrolls; the user doesn't resize it. macOS and the
  web have such a sheet; elsewhere it's large.
- `grabber: true` shows the handle at its top (iOS, Android, and the
  web's bottom sheet).
- Its content is built when it opens and disposed when it closes, so
  state made in it starts fresh each time.

### Movable sheets

`movable: true` makes a sheet on a desktop a dialog the user can move,
as a desktop's modal dialogs are, rather than a sheet attached to the
window: for a detail or a form the user wants to look at beside what's
under it. It's as modal as a sheet: what's
behind it takes no input until it closes, and Escape dismisses it; a
click outside it doesn't, unless it says `dismissOutside: true`. It has no title bar: its content is all of it,
its own header its top (give it a Close), and the user moves it by its
background, anywhere that isn't a control, a pressable view or text to
select. It opens
centered over its window at the size `detents` gives; a `.fit` one is
as big as its content and follows it as it changes, as any `.fit` sheet.
`resizable: true` lets the user resize it too, for content that's
worth seeing larger, such as a long report: a movable sheet isn't
resizable without it, as a desktop's dialogs aren't (a `Window` is
resizable unless it says `resizable: false`). A resizable `.fit` sheet
opens at its content's size, which is then the least the user can size
it to, and after that keeps the size the user gives it, which its
content fills. `dismissOutside: true` makes a click outside it dismiss
it as Escape does, for a dialog the user glances at and leaves, such as a
detail opened from a list: without it, a click outside does nothing (a
Mac beeps), as a desktop's modal dialog does.

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

| | A movable sheet |
|---|---|
| macOS | a window of its own without a title bar (its window's shadow and corners; resizable at its edges with `resizable`), centered over its window and in front of it, fading in where it opens; its window takes no presses, scrolls or hovers meanwhile (a press there dismisses it with `dismissOutside`), and ⌘W closes it |
| Windows | a modal window over the window in use, which a sheet already is |
| The web | a card over a backdrop on a wide window, fading in where it opens; resized by its corner with `resizable`, dismissed by a press on the backdrop with `dismissOutside` |
| iOS, Android, Linux | a sheet as without it: a phone's sheet isn't moved, and libadwaita's dialogs aren't |

A `fullScreen` sheet ignores it.

### Full-screen sheets

`fullScreen: true` covers the whole screen instead, for a task that takes the
user's attention until it's done or given up: composing a message,
signing in, editing a photo (SwiftUI's `fullScreenCover`, Material's
full-screen dialog). `detents` and `grabber` don't apply, and nothing
outside it can be tapped or swiped away: it closes when `open` goes
false, so give it its own Cancel and Done. Android's back, and Escape on
a desktop or in a browser, dismiss it as they do a sheet, unless an
`onBack` in it keeps it (a draft to discard first).

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

  onBack { return !draft.isEmpty() } // back doesn't lose a draft

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

Its environment is the screen's own: on a phone its whole size, and the
safe area of the status bar, the notch, the home indicator or
navigation bar, and the keyboard, so `SafeArea` keeps its content clear
of them as it does the root's.

| | What it is |
|---|---|
| iOS | the controller presented `.fullScreen`: it slides up over everything, the screen under it gone until it closes |
| Android | a full-screen `Dialog` in a theme of its own, edge to edge (the status and navigation bars' insets, the cutout, the keyboard), sliding up with Material's emphasized easing; predictive back shrinks it toward the edge it's swiped from, its corners rounding, unless an `onBack` takes it |
| macOS | a sheet as big as all of its window's content under the title bar and toolbar, following the window as it resizes, which the user doesn't resize: a Mac has no full-screen modal, and a sheet is how AppKit gives a window over to one task |
| Windows | a borderless window over all of its window's content (the title bar stays the window's, which is disabled meanwhile), following it as it moves and resizes, its bottom corners rounded as Windows 11 rounds the window's: Fluent's full-size `ContentDialog` |
| Linux | a floating `AdwDialog` as big as its window, which libadwaita shows over all of it, as it shows a dialog that doesn't fit (on a phone, in a narrow window); it follows the window as it resizes |
| The web | a modal `<dialog>` over the whole page (focus stays in it; Escape and the browser's back dismiss it), or over a Window panel's content for a sheet declared in one; it slides up |

The user dismisses a sheet by swiping it down, on Android with back
or a tap outside it, and on a desktop or in a browser with Escape (on
Windows, its window's close button too; on the web, a click outside).
That sets `open` to false and runs `on dismiss`. Setting `open` to
false from code closes it without running `on dismiss`. So `on dismiss`
means "closed without your say", such as a draft left unsaved.

## Popovers

`Popover` shows its `content` in a bubble pointing at a view, over the
screen, while `open` is true: a few options, a detail, a small form.
Its children are what it points at:

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

It's as big as its content, no wider than `maxWidth` (320 by default),
text wrapping to it, and no taller than most of the screen. Its content
is built as it opens and disposed as it closes. A tap outside dismisses
it, setting `open` false and running `on dismiss`, as a sheet's; an
`onBack` in it can keep it.

| | The view | Where |
|---|---|---|
| iOS | `UIPopoverPresentationController`, a popover on iPhone too (not adapted into a sheet), Liquid Glass on iOS 26 | where UIKit fits it, its arrow on the anchor |
| Android | a `PopupWindow` on Material's container surface, 16dp corners, elevated | below the anchor, centered (above it without room) |
| macOS | `NSPopover`, transient | below the anchor, where AppKit fits it |
| Windows | a popup window, light dismissed, Windows 11's corners (WinUI's Flyout) | below the anchor, centered (above it without room) |
| Linux | a `GtkPopover`; Escape closes it too | below the anchor, where GTK finds room |
| The web | a popover | next to the anchor |

## Alerts and action sheets

`alert(title, …)` shows an alert now, and an `Alert` element shows one
while its `open` is true:

- **iOS**: a `UIAlertController`.
- **Android**: a Material dialog.
- **macOS**: an `NSAlert`, shown as a sheet over the window in use.
- **Windows**: a task dialog over the window in use.
- **Linux**: an `AdwAlertDialog`.
- **The web**: a modal `<dialog>`.

Pressing a button closes the alert and runs that button's `run` (an
`Alert`'s `Button`'s `on press`). With no buttons, it has a single OK.

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

A button's `role` says what it is:

- `.normal` (the default) is an ordinary button.
- `.cancel` is the way out. iOS puts it in its own place, and
  Android makes it the dialog's negative button. On a desktop, Escape
  presses it: on a Mac it's the last button, after the default one.
- `.destructive` is red: in iOS's style, in Material's error
  color on Android, AppKit's destructive button on a Mac, and
  libadwaita's destructive response on Linux.

On Android, the last other button is the dialog's positive button, at
the end, and a second one is neutral. With more than two buttons other
than Cancel, the dialog shows them as a list. On a Mac the first button
other than Cancel is the default, which Return presses; on Linux the
first `.normal` one is, in the accent color.

`actionSheet: true` shows the buttons as an action sheet, a list of choices
coming up from the bottom on iOS:

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

On Android an action sheet is a dialog with the actions as a list, and
its `message` isn't shown. On Windows its buttons are a task dialog's
command links; on Linux it's an alert dialog with no default button; on
a Mac it's an alert as any other; on the web, a dialog whose buttons are
a list. An alert closed some other way, such as back or a tap outside it
on Android, runs no button's (an `Alert`'s `on dismiss` runs).

### Text fields

An alert can ask for text, a name or a password, as SwiftUI's alert
with a `TextField` and a `SecureField` does. An `Alert`'s `TextField`s
show under the message. Each field's bound `text` starts with its value
and is written as the user types, so a button's `disabled` can follow
it: here, Sign In stays disabled until both fields are filled in, and
the alert updates the button while it's up.

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

A field also takes a `keyboard` and a `contentType` for AutoFill, as a
`TextField` does. `secure` hides what's typed. The first field has
the focus, with the keyboard up. Return in a field moves to the next
one. In the last field it presses the default button, which is the
first `.normal` one (UIAlertController's `preferredAction`), but only
while it's enabled. Writing a field's `text` while the alert is up
doesn't change what the field shows. An action sheet
shows no fields.

| Platform | The fields |
| --- | --- |
| iOS | `UIAlertController`'s text fields; the default action is its `preferredAction`, in bold |
| Android | outlined `TextInputLayout`s in the Material dialog (a password's with its toggle); the IME's action goes to the next field, and from the last presses the default action's button |
| macOS | `NSTextField`s and an `NSSecureTextField` as the `NSAlert`'s accessory view; Tab moves between them |
| Windows | a dialog of Asuka's own, laid out as WinUI's `ContentDialog` (a task dialog can't hold fields): the title, the message, Fluent text boxes, then the buttons with the default one in the accent color; Tab, Enter and Escape work as in a dialog, and Narrator reads each field by its placeholder |
| Linux | a boxed list of `AdwEntryRow`s and `AdwPasswordEntryRow`s, the `AdwAlertDialog`'s extra child |
| The web | inputs in the `<dialog>`, labeled by their placeholders, with `type=password`, `inputmode` and `autocomplete` |

## Snackbars

`snackbar(message, …)` shows a brief message at the bottom of the
window now, with an optional action such as Undo, and returns a closure
that dismisses it:

- **iOS**: UIKit has none, so Asuka draws one as iOS draws its own
  brief messages: a capsule in the system material (Liquid Glass on iOS
  26), centered above the tab bar or toolbar and the keyboard, on the
  topmost presented content. It springs in and out (it fades with Reduce
  Motion) and a swipe down dismisses it.
- **Android**: Material's `Snackbar`, which a swipe dismisses, above the
  screen's bottom navigation bar or floating action button (its anchor)
  and the keyboard; over a sheet, at the bottom of the sheet.
- **macOS**: an overlay at the bottom of the window in an
  `NSVisualEffectView`, with a close button.
- **Windows**: an overlay at the bottom of the window drawn as Fluent's
  InfoBar, with a close button.
- **Linux**: an `AdwToast` in the window's `AdwToastOverlay`.
- **The web**: an element with role `status` at the bottom of the
  window (a popover, in the top layer), with a close button; a swipe
  down dismisses it.

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

The message is shown once, so it's a string, as an alert's is. The
action is a `.button`: its `title` is shown, its `role`, `icon` and
`disabled` aren't. Pressing it dismisses the snackbar and runs its
`run`. `onDismiss` runs when it goes any other way: its time ran out,
the user swiped or closed it, another snackbar replaced it, or the
closure `snackbar` returned was called. So after an Undo, `onDismiss`
doesn't run: it's where what the action could undo is done for good.

`duration` is how long it shows: `.short` (the default, about four
seconds, Material's), `.long` (about ten) or `.indefinite` (until it's
dismissed or its action pressed).
Asuka lengthens it as the platform recommends for accessibility: Android
by `AccessibilityManager.getRecommendedTimeoutMillis`, which keeps one
with an action longer while TalkBack is on; on iOS, macOS and Linux,
which have no recommended timeout, one with an action stays until it's
dismissed while a screen reader is on, as Material does without one;
Windows also takes its "Dismiss notifications after" setting. On the
desktops and the web its time waits while the mouse is over it or the
keyboard's focus is in it. Screen readers announce it as it shows and
reach its buttons.

Only one shows at a time: a new snackbar replaces the one showing, as
Android's does (the old one's `onDismiss` runs first). On a desktop it
goes on the window an alert would: that of the view whose event shows
it, else of the component calling it, else the main window.

## Menus

A menu shows a list of `Action`s, the same type as alert buttons and
bar buttons, with an optional `icon` beside each title. There are two:

- A `Button` with `menu` opens it when tapped.
- `Menu { actions: […] }` puts one on any content: on a tap, or with
  `longPress: true` as a context menu.

On iOS it's a `UIMenu`, and on Android a `PopupMenu`. On a Mac it's an
`NSMenu`, on Windows a popup menu (a destructive item red, as on a Mac),
on Linux a `GtkPopoverMenu`, and on the web a popover. On the desktops
and the web a context menu opens with a right-click (a secondary
click), and on Windows, Linux and the web with a long press too. Both are in
[Components](components.md#menus). Rows of a `List` have swipe actions
too: see [Lists](components.md#lists). A `Menu` in a `MenuBar` is one of
the app's menus instead ([below](#the-menu-bar-and-keyboard-shortcuts)).

## Windows

A desktop app's main window holds what its `app` block shows, unless
the app has none ([below](#apps-without-a-main-window)). The block's
`Window` is the main window: its keys are the main window's options,
and its children what it shows:

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

Sizes are in points, of the window's content. With none, the main window
opens at 800 by 600 points, centered, and the user can't make it smaller
than 320 by 320. Its `title` is the app's name when it's empty; it's a
binding, so it can follow the app's language. The window comes back
where the user left it (see [below](#where-windows-were)), which wins
over `width`, `height`, `x` and `y`. iOS and Android ignore the options.
The main window shows unless its `open` is false: `bind open: shown`
shows and hides it as `shown` changes, and the user closing it, in an
app that keeps running, writes false.

Any other `Window` opens a window of its own while `open` is true, as a
`Sheet` opens a sheet:

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

- Its children are built when the window opens and disposed when it
  closes. `environment()` in it is the window's: its size follows the
  user resizing it, and its scale is its display's.
- The user closing it writes `open` false and raises `close`. Setting
  `open` to false from code closes it without `close`.
- It opens at 800 by 600 points unless it says otherwise, cascaded
  from the window in use on a Mac and on Windows. On Linux the desktop
  places it, as GTK 4 leaves to it (GNOME and KDE put a new window where
  it covers the fewest others). Stacks in it have their own bar, as in the main
  window. Sheets and alerts open over the window the user is working in.

| | A window |
|---|---|
| macOS | an `NSWindow`, with its own toolbar and title (its innermost stack's) |
| Windows | a top-level window, with its own title and taskbar button |
| Linux | an `AdwWindow`, with its own header bar |
| The web | a panel in the page, which the user can move and resize |

A phone has no windows: a `Window` there shows nothing. A component that
offers one offers it from what only a desktop has, as the menu bar below.

### Options

The main window and a `Window` take the same keys for how a window
opens; they're read as it opens (the ones that change later are the
handle's actions, below, but `title`, `alwaysOnTop`, `clickThrough`,
`flyout`, `edited` and `file` follow their bindings):

| Key | |
|---|---|
| `width`, `height` | its content's size, in points |
| `minWidth`, `minHeight`, `maxWidth`, `maxHeight` | the least and most the user can size it to; 0: the platform's least, no most |
| `x`, `y` | its frame's top left on the screen, in points from the main display's top left; with both 0 the main window is centered and a `Window` cascades (`center: true` centers it) |
| `resizable`, `minimizable`, `maximizable`, `closable` | false takes the user's abilities away: resizing it, and its buttons |
| `alwaysOnTop` | keeps it above other apps' windows |
| `titleBar` | `.hidden` lays the content out under the title bar, to the window's top edge, the window buttons over it |
| `background`, `transparent` | the window's color behind its content, or none, so what's behind it shows where the content draws nothing |
| `hidden` | opens it without showing it: `w.show()` shows it |
| `skipTaskbar` | leaves it out of the taskbar (Windows) and the Window menu (macOS) |
| `maximized`, `fullScreen` | opens it so |
| `stateKey` | where its place is kept, see [below](#where-windows-were) |
| `fitContent` | sizes it to its content, as that changes, within its least and most size ([The desktop](desktop.md#windows-sized-to-their-content)) |
| `panel` | a launcher's or a palette's window: floating over every app, taking the keyboard without bringing the app forward, closed by Escape or a click elsewhere ([Panels](desktop.md#panels-and-launchers)) |
| `clickThrough` | lets the mouse go through it to what's behind ([Click-through](desktop.md#click-through-windows)) |
| `flyout` | a `Window`'s: hangs it from a tray icon or a rectangle of its parent ([Flyouts](desktop.md#flyouts)) |
| `child`, `modal` | a `Window`'s: see [below](#child-and-modal-windows) |
| `menuBar`, `edited`, `file` | a window that shows a document: the app's menu bar in it (Windows, Linux), the dot in a Mac's close button or a `*` before the title, the file's icon in a Mac's title bar |

With a hidden title bar the content goes to the top edge, under the
window's buttons (a Mac's traffic lights, Windows' caption buttons, the
header bar's on Linux), and the window's environment has the title
bar's height as `safeTop`: a `SafeArea` keeps clear of the buttons,
and a background, a sidebar or a picture can go under them:

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

The strip under the buttons still drags the window, where the content
takes no clicks. On Windows the main window has no menu bar under a
hidden title bar; its shortcuts still work.

A window that shows only once it's ready opens `hidden` and shows from
code: its content is built, laid out and drawn meanwhile, so it shows
whole, never empty. An app that starts in the background (a tray icon's)
opens its windows from its menu, and needs no main window at all
([below](#apps-without-a-main-window)).

### Controlling a window

`Window as w` names a window's handle, a `Ref` of the `Window`: the
enclosing view's elements see it, and pass it on to components. A `Ref`
of any other view acts on the window that view is in.

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

- `w.show()` and `w.hide()` show and hide it; hidden, it stays open, its
  content living on. `w.focus()` shows it if it's hidden or minimized,
  brings it to the front with the keyboard in it, and brings the app to
  the front: how a tray icon's menu brings a window back (Windows lets
  an app come to the front after the user's input to it, as a click on
  its tray icon is). On a closed `Window`, `show()` and `focus()` open it.
- `w.minimize()`, `w.maximize()` and `w.restore()` (from either);
  `w.setFullScreen(on)`.
- `w.setSize(width, height)` (its content's, in points),
  `w.setPosition(x, y)` (its frame's top left, from the main display's
  top left), `w.center()`.
- `w.setAlwaysOnTop(on)` keeps it above other apps' windows, or not.
- `w.setClickThrough(on)` lets the mouse go through it, or not.
- `w.close()` closes it as the user does, asking first (below);
  `window.close()` closes the window the calling code is in (the main one
  outside every `Window`) without asking. The main window closing quits
  the app, unless it keeps running.
- Its state follows the user's changes as well as the app's: `w.visible`,
  `w.focused`, `w.minimized`, `w.maximized`, `w.fullScreen`,
  `w.alwaysOnTop`, `w.clickThrough`, `w.position` and `w.size` (each an
  `(x, y)` or `(width, height)`), and all of it at once as
  `w.windowState`. A binding that reads them follows them; a
  window's state is reported only once something reads it. A minimized
  window, or one behind others, is still visible.

`window.main()` is the main window's handle and `window.current()` the
handle of the window the calling code is in (its `Window`'s, else the
main window's), from anywhere: a menu's item, a tray icon's, a store.

The actions act on the window when the screen next updates, as state
writes do. On a
phone they do nothing, and the state reads as nothing shown.

### Asking before a window closes

The user closing a window (its close button, ⌘W, Alt+F4, the Dock's or
taskbar's menu), or `w.close()`, asks first:

- `WindowCloseGuard { when: …; on closeRequest { … } }` in a window
  keeps it open while `when` is true, and runs its `closeRequest`: ask
  about unsaved changes, then `window.close()` once the user has chosen.
- `onCloseRequest(fn() -> bool)`, an item of a component's body, asks a
  function for as long as the component lives: false keeps it open.
- A `Window`'s own `on closeRequest { … }` (the main window's too) takes
  the user's closing over: the window stays open until `open` is false
  or `window.close()`.

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

Every guard and function asked must agree. On the web the main window is
the tab: leaving the page or closing the tab asks too, and a refusal
keeps the page behind the browser's own "Leave site?" question (a page
can't word it). Quitting the app (⌘Q) doesn't ask the windows: it asks
the app's `on quit` ([The desktop](desktop.md#quitting)).

### Where windows were

Windows come back where the user left them, across launches: the main
window always (under the key `"main"`, or its `stateKey`), a `Window`
when it has a `stateKey`. Give each kind of window its own
(`"inspector"`, `"preferences"`). Where it was wins over its `width`,
`height`, `x` and `y`, which stay what the first launch opens with.

| | What's kept |
|---|---|
| macOS | its frame, with AppKit's frame autosave (in the app's defaults) |
| Windows | where it was and its size, in the screen's pixels and the DPI they were at, and whether it was maximized; put back whole on a monitor that's there |
| Linux | its size and whether it was maximized: a Wayland app can't place its window |
| The web | a `Window` panel's place and size, and whether it was maximized, in the page's `localStorage` |

Desktop apps keep theirs in `asuka-window.json` in the app's data
directory (Windows, Linux) or the app's defaults (macOS). Tests and
automation runs keep none.

### Child and modal windows

A `Window` with `child: true` is a child of the window it's declared in:
the main window, or the `Window` whose content has it. It stays above
that window, and closes with it (its content is disposed with that
window's). `modal: true` makes it a child that blocks that window until
it closes:

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
| macOS | a child window, ordered above its parent and moving with it | a sheet of its parent |
| Windows | owned by its parent: above it, minimized with it | owned, its parent disabled while it's open |
| Linux | transient for its parent, destroyed with it | transient and modal |
| The web | a panel as any other | a panel over a backdrop that takes the page's clicks |

### Apps without a main window

Without a `Window` of its own, the app block's main window holds what
the block shows. A block that shows nothing there has no main window at
all: its components show only `Window`s, a tray icon, a `MenuBar`,
global shortcuts. Its windows are the ones it opens, as a menu bar
app's or a document app's are:

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

- `window.main()` still names it, but its actions do nothing and its
  state reads as nothing shown, as on a phone; `window.current()`
  outside every `Window` is that handle too.
- Reopening the app (its Dock icon clicked, the app launched again)
  brings it to the front with the windows it has open; with none, it
  shows none.
- The user closing its last window ends the app, unless it keeps
  running (`asuka.toml`'s `keepRunning` or `background`), which a menu
  bar app does.

Whether there's a main window is decided as the app compiles, by what
its code can show on the platform it's built for (an `if @platform`
takes that platform's branch), not by what its state shows: a block
that may show a view has a main window, empty while it shows none. On
the web the tab is the main window, which shows nothing then; on a
phone the screen is the app's, whatever it shows.

### What each desktop can do

| | macOS | Windows | Linux (GTK 4) | The web: the tab | The web: a `Window` |
|---|---|---|---|---|---|
| Show, hide, focus | yes | yes | yes | focus only | yes |
| Minimize | yes | yes | yes (Wayland never reports it back) | no | to its title bar |
| Maximize, restore | zoom | yes | yes | no | over the app's area |
| Full screen | yes | yes | yes | the Fullscreen API | the Fullscreen API |
| Size, least size | yes | yes | yes | no | yes |
| Most size | yes | yes | no: GTK 4 has none | no | yes |
| Position, center | yes | yes | X11; Wayland: no, the compositor places windows | no (reads the browser window's) | yes |
| Always on top | yes | yes | X11 | no | above the other panels |
| Hidden title bar | yes | yes | yes | no | yes |
| Background, transparent | yes | background; transparent falls back to it | yes (transparent with a compositor) | background | yes |
| Skip the taskbar | the Window menu | yes | X11 | no | no |
| Panel, flyout | yes | yes | X11: kept above, placed; Wayland: no place, nothing above | no | yes |
| Fit its content | yes | yes | yes | no | yes |
| Click-through | yes | yes | yes | no | to the page |

## The menu bar and keyboard shortcuts

`MenuBar` gives a desktop app its menus, and their items their keyboard
shortcuts. Its children are `Menu`s, theirs `MenuItem`s:

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

- A `MenuItem` has a title, a `shortcut` and its `press`; `disabled` and
  `checked` change how it shows. `MenuItem`s in an item make it a
  submenu. `MenuSeparator()` is a line between groups of items.
- `role` is what the item is: `.destructive` draws it red (macOS,
  Windows; GTK sets it apart); `.quit`, `.close` and `.openRecent` are
  the platform's own items, which Asuka does itself with its own
  title (unless the item has one) and shortcut: quit the app (asking it
  first), close the window in use as its close button does, and the
  recent documents (a Mac's own Open Recent).
- A `Shortcut` is a `key` and its modifiers: `primary` is the platform's
  command key (⌘ on a Mac, Ctrl elsewhere), and `shift`, `alt` (a Mac's
  Option) and `control` the others. A key is a character (`"n"`, `","`,
  `"="`) or a name: `enter`, `escape`, `tab`, `space`, `delete`,
  `backspace`, `up`, `down`, `left`, `right`, `home`, `end`, `pageup`,
  `pagedown`, `f1` to `f12`.
- Its items are bindings: an item turned off or checked, a menu or an
  item that a `for` or an `if` shows, is sent again. While several
  `MenuBar`s show, the last one's are the app's; when it goes, the one
  before's. It takes no space; put it anywhere.
- `WindowKeys { keys: [Shortcut(key: "k", primary: true, run: search)] }`
  gives the window it's in keys that no view takes, whatever has the
  focus, while it shows (the last `WindowKeys`' win): keys that aren't
  menu items.

Each desktop shows them its own way:

- **macOS**: the menu bar, beside the Mac's own menus: the app's menu
  (About, Hide, Quit), File, Edit (Undo, Cut, Copy, Paste...), View
  (Back, Enter Full Screen) and Window. A menu titled File, Edit, View,
  Window or Help adds its items to that one; the others go between View
  and Window.
- **Windows**: a menu bar on the main window (and a `Window` with
  `menuBar: true`), its shortcuts working in every window of the app.
  Each menu and item gets an access key (Alt+F), and each item its
  shortcut after its title ("Ctrl+Shift+K").
- **Linux**: GNOME's apps have no menu bar, but a main menu: the header
  bar's last button. Each of the app's menus is a submenu of it (one
  menu's items are the main menu's own), and the shortcuts work in
  every window.
- **The web**: the shortcuts work in the page; it shows no menu bar.

iOS and Android ignore `MenuBar`, so an app can have one wherever it
runs. Its items are commands, so keep each reachable from the
interface too, for a phone.

## Toasts

Asuka has no toast component, since iOS and Android have no standard one
that apps share. Make one from a view placed over the screen,
shown with an `if` and brought in with a
[transition](motion.md#enter-and-exit). The gallery's toast does this, and
calls `accessibility.announce` so that screen readers say it too. See
[`examples/gallery`](../examples/gallery).

## Routes

With a `Router`, every screen has an address, an href such as
`/inbox/18` or `/search?q=go`, as a web page has. The app declares its
routes once, as elements: path patterns mapped to screens under the
navigators that show them. It navigates by address, and by screen:

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

What an address does then comes for free: the web's address bar, back
and forward show it, a reload or a pasted link opens it, a link of the
app's scheme or domains goes to it, and a desktop window opens where it
was left. [`examples/navigation`](../examples/navigation) is routed.

### Patterns and parameters

A route's path is relative to its parent's, unless it starts with `/`.
Its segments are matched one by one:

- `chat` matches itself;
- `:id` is a parameter, any one segment;
- `*path` is a catch-all, the rest of the path, none or more segments
  (last in a pattern): `docs/a.md`;
- `""` is the parent's own path, its index.

When several routes match, the most specific wins, as in expo-router:
segment by segment, a static one before a parameter before a
catch-all. `/files/new` takes `/files/new`; `/files/:id` takes
`/files/3`; `/files/*path` the rest.

A route's screen is either built in its block, `p => View`, given its
`Params` (`p.get("id")`: the path's parameter, else the query's;
`p.path`, `p.query` and `p.href` too), or a screen declared with its
pattern, `screen Thread(id: int) at ":id"`, named alone in the block.
Such a routed screen's parameters come from the address: the path's,
else the query's (`/search?q=go`'s `q`), converted to their types
(`int`, `float`, `bool`, an enum's case, `string`). One the address
doesn't say takes its default; without one, or with a value that
doesn't convert, the screen throws a `RouteError` to the nearest
`Boundary`. Its title is the screen's own.

### Navigators and sheets

A `Route` with routes in it is a navigator of them, by its `layout`, or
a group:

- `.stack`: a `Stack`. Its children are its screens, and sheets.
- `.tabs`: `Tabs`. Each child is a tab (`title`, `icon`, `badge`), a
  screen or a stack of its own.
- `.split`: a `Split`. Each child is a destination of the sidebar, a
  screen or a stack (the detail's).
- `.slot`: one child at a time, filling its place with no bar: an app's
  signed-out and signed-in parts.
- `.group` (the default): its children join its parent's navigator
  under its path and its guard.

A route with `sheet: true` is presented in a sheet over its navigator
(its stack's top screen, the tabs) rather than pushed or chosen; its
`detents`, `grabber` and `fullScreen` are a sheet's. Going back
dismisses it; so does navigating to a screen of the navigator under it.

### Navigating

- `navigate(href)` goes to the screen: the tabs, destinations and slots
  on the way switch to it; in its stack, an earlier screen with the same
  address is gone back to, else it's pushed; a sheet's is presented.
  `navigate(Thread(id: 7))` goes to a routed screen's address.
- `replace(href)` shows the screen in place of the current one (a
  sheet's place too), which is disposed; the history doesn't grow.
- `pop()` goes back in the innermost stack, `dismiss()` dismisses the
  newest sheet.
- `router.back()` goes back as the user's back would (without asking
  `onBack`): the newest sheet, else the innermost stack on screen;
  `router.canGoBack()` says whether it would. `router.dismiss()`
  dismisses the router's newest sheet.
- `router.href()` is the address shown, which a binding that reads it
  follows (a sidebar's selection, a link's state);
  `router.setQuery("q=go")` changes the shown screen's query, keeping
  the screen.

A `Button` or a pressable view with an `href` goes there when it's
pressed (after its `on press`). In a router, a rich text's link to one
of the app's hrefs (`Span(text: "the inbox", link: "/inbox")`) goes
there too. They act on the router around the code, else the main
window's.

`push` keeps working in a routed stack: it pushes a screen that has no
route. It has its stack's address under it and a history entry of its
own (the browser's back pops it), but a reload or a link can't bring it
back.

Every screen is an owner, as everywhere: a screen a route pops, replaces
or leaves (another tab's screen doesn't) is disposed.

### The screens under a link

A link builds the screens it stands for, so back from it goes somewhere
sensible: in each stack on the way, the stack's `initial` screen (its
first child unless it names another, as expo-router's
`initialRouteName`), then the stack's screens up the link's path
(`/inbox/:id` under `/inbox/:id/reply`), then the one linked to; the
other tabs at their first screens. Their parameters come from the
link's, by name; one that needs a parameter the link hasn't, or that a
guard keeps the user out of, isn't made.

### Redirects, guards and not found

A route with a `redirect` sends whoever comes to its path to another
href, with its parameters filled in, replacing it: `Route("/",
redirect: "/inbox")`, `Route("/m/:id", redirect: "/inbox/:id")`.

A `guard` is asked before its screens (and its children's) show, with
the href asked for: it returns `""` to let the user in, or where to send
them instead. It can read state: when what it read changes, the screens
shown are asked again, and those it no longer lets in go (with what's
over them in their stacks), the router going where the guard sends the
user. A signed-out user is sent to the login with where they were
going, and back there after:

```asuka
import std/url

state signedIn = false

app {
  Router {
    Route("/login", guard: to => if signedIn { "/" } else { "" }) { _ => Login() }
    // A group: everything else.
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

An href no route takes shows a plain screen with a way home. A
catch-all route of the app's own (`Route("/*path") { p => … }`) takes
them instead.

### Where it starts, and where it's kept

A router starts at the link the app was opened with, if it takes it;
else where its window was left; else at its `initial` (`/`). Its whole
state (each stack's screens, each tab's, the sheets) is kept with the
address, as JSON, and read back as far as the routes still match it:

| | Where it's kept |
|---|---|
| iOS, Android | in the app while it runs; not across launches (as a native app's navigation isn't) |
| macOS, Windows, Linux | with where each window was: the main window's always, a `Window`'s with a `stateKey`, in `asuka-routes.json` in the app's data directory; it opens there next time |
| The web | the address bar, and each history entry: back, forward and a reload come back to the screens they stand for (see [the web](web.md#addresses-and-history)) |
| `asuka dev` | every platform: a reload of new code comes back to the screens it showed |

A window's first router has its address; one in a `Window` has that
window's.

### Typed links

A routed screen's call is its address, checked as the app compiles:
`navigate(Thread(id: 7))` and `href: Thread(id: 7)` are
`/inbox/7`, from the one `Router` that routes `Thread`, its other
parameters the query (`Thread(id: 7, sort: .oldest)` is
`/inbox/7?sort=oldest`, escaped). A screen no `Router` routes, or two
do, is an error there, as is a call that leaves out a parameter the
path needs. An href written as a string is the app's to get right.

In a test, `mount(…, at: "/inbox/18")` starts at an address, and
`app.href`, `app.history()`, `app.goBack()` and `app.goForward()` follow
it as a browser would.

## Deep links

An app opens for URLs of its own scheme, and for https links to its own
domains, when `asuka.toml` lists them:

```toml
name = "Notes"
id = "com.example.notes"
scheme = "notes"
links = ["notes.example.com"]
```

- `scheme` opens the app for `notes://anything`: on iOS and Android,
  on a Mac (the app's `Info.plist` declares it), and on Linux once the
  app is installed (its desktop file handles the scheme; a link opened
  while the app runs goes to it, as GNOME's apps are one instance each),
  and on Windows (the app registers it for the user as it starts, under
  `HKEY_CURRENT_USER\Software\Classes`, again if it has moved; a link
  opened while the app runs goes to it). An app with an `id` runs once
  on Linux and Windows: launched again, it comes to the front. `asuka
  dev` registers no scheme on Linux or Windows, and its runs don't hand
  themselves to an installed copy of the app: try links with a build.
- `links` opens it for `https://notes.example.com/...`: universal links
  on iOS, App Links on Android. The domain must say so too. It serves
  `apple-app-site-association` for iOS and `assetlinks.json` for
  Android, as each platform's docs describe. The desktops don't open
  for them.

With a [router](#routes), these links need no code: the app's scheme
(`notes://inbox/18`, its host the first segment, as expo-router reads
it, so `/inbox/18`), a universal or app link's path, and a
notification's URL go to their route, at launch or while the app runs.
The app block's `on openURL(url)` gets the URLs no route takes.

Without a router, `on openURL(url)` gets each URL the app is opened
with, at launch or while it runs (`onOpenURL` in a component's body
does too, while the component lives). The URL that launched the app
comes before any handler exists, so it goes to the first handler. Route
from it with `push`, or by setting state.

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

[`examples/notes`](../examples/notes) adds a note for
`asukanotes://add?text=...`. A notification's URL arrives the same way
when the user taps it, on every platform (see [Device](device.md)). On
the web, a router's addresses are the page's: a link to the app's site
opens its screen, and opening the page doesn't run `on openURL`.

## Shares from other apps

With `shareTarget = true` in `asuka.toml`, the app shows in other apps'
share sheets, for text and links, such as "Share" in a browser or in
YouTube; a table, `{types = ["image/*", ...]}`, takes files too (see
[Device](device.md#shares-from-other-apps-onshare)). The app block's
`on share(items)` gets what's shared. A share that launched the app
goes to the first handler, as with `on openURL`.

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

On iOS the app comes with a share extension, which opens the app with
what's shared, as Android opens it: the extension shows no screen of
its own. What's shared is often a title and a link together (a page's
title, a text, and its URL, a link, from Safari).
[`examples/downloader`](../examples/downloader) looks for the link. The
desktops and the web aren't share targets: `on share` never runs
there.

To show a sheet of your app's over the app that shares, without opening
yours, see `platform/sharesheet` in
[Device](device.md#the-apps-own-share-sheet-platformsharesheet). To share
from your app instead, `device.share(text)` shows the system's share sheet,
and `device.shareFiles(paths)` shares files: the share menu on a Mac, the share UI
on Windows, the browser's on the web. Linux has no share sheet: they
copy what's shared to the clipboard, and say so in a toast.
