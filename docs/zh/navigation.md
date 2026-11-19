# 导航

一个应用的骨架由这几样东西搭成：页面组成的导航栈、标签页、弹出面板（sheet）、警告框和菜单；到了桌面端，还有窗口和菜单栏。它们用的都是各平台自己的实现：

- **iOS**：`UINavigationController`、`UITabBarController`、sheet 形式的模态呈现、`UIAlertController`、`UIMenu`。
- **Android**：带 Material 过渡动画和预测性返回的顶部应用栏、Material 的底部导航栏、`BottomSheetDialog`、Material 对话框、`PopupMenu`。
- **macOS**：窗口的工具栏（返回按钮、当前页面的导航栏按钮和搜索框）和标题、`NSTabView`、sheet 窗口、`NSAlert`、`NSMenu`，以及菜单栏。
- **Windows**：盖在页面上方的顶栏（header bar）、顶部的标签栏（即 WinUI 的 `NavigationView`）、模态窗口、任务对话框、弹出菜单，以及窗口的菜单栏，全都按 Windows 11 的样子绘制。
- **Linux**：全部来自 libadwaita：窗口的顶栏、`AdwNavigationView`、视图切换器、`AdwDialog`、`AdwAlertDialog`、GTK 的 popover 菜单，以及主菜单。
- **Web**：由 Asuka 绘制的顶栏、`<dialog>` 和 popover，以及浏览器的后退按钮（见 [Web](web.md)）。

本页逐一介绍它们，也会讲到返回操作、路由（像 Web 一样，每个页面都有自己的地址），以及如何通过链接和分享打开应用。[`examples/navigation`](../../examples/navigation) 演示了上面所有内容；[`examples/notes`](../../examples/notes) 里有菜单栏和窗口的例子。

## 导航栈

`Stack` 是一个导航栈。它的子元素是它的第一个页面，它下面的代码用 `push` 推入更多页面。

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

- `push(Screen(…))` 把一个页面压到栈顶，并播放平台的推入动画。
- `pop()` 移除栈顶的页面，但永远不会弹出第一个页面。
- `depth()` 返回栈里有多少个页面。

`push`、`pop` 和 `depth` 作用于调用它们的代码外面最近的那个导航栈：页面自己的事件处理函数，以及它们创建的东西（警告框的按钮、菜单或滑动的操作、snackbar 的操作、`await` 之后处理函数的其余部分），用的都是页面所在的栈，第一个页面也不例外。

在 iOS 和 Android 上，每个导航栈都有自己的导航栏；Windows（Asuka 仿照 WinUI，在页面上方绘制一条顶栏）和 Web 也是这样。而在 Mac 和 Linux 上，和这两个平台的原生应用一样，一个窗口只有一条栏，它归屏幕上最内层的导航栈所有（在标签页里，就是当前标签页的导航栈）：栏上显示的是这个栈的返回按钮，以及它栈顶页面的标题、导航栏按钮和搜索框。这条栏在 Mac 上就是窗口的工具栏和标题，在 Linux 上则是窗口的 `AdwHeaderBar`。在 macOS、Linux 和 Web 上，推入和弹出页面时会有滑动动画；在 Windows 上，页面直接切换，没有过渡效果。

导航栈会填满分配给它的空间。每个页面都是一个 [owner](concepts.md)：无论是代码弹出页面，还是用户用返回手势退出，页面都会被销毁，其中的 effect 也随之停止。同一个页面再次推入时会从头开始，所以需要比页面活得更久的东西，要放在页面之外的状态里（顶层的 `state`、一个 store），或者存进[存储](data.md)。

## 页面选项

`screen` 声明的代码块在内容之前设置页面的选项：

- `title` 是导航栏的标题（在 Android 上是顶部应用栏的标题；在 Windows、Linux 和 Web 上是顶栏的标题；在 Mac 上是窗口标题；在浏览器里同时也是网页标题）。它是一个绑定，所以可以随时变化。
- `toolbar { … }` 放导航栏上的按钮。
- `backGesture: false` 为这个页面关闭返回手势（见[返回](#返回)）。
- `bind search: query` 在导航栏里放一个搜索框。
- `zoomFrom` 让页面从下层页面上的某个视图中缩放展开。
- `largeTitle: true` 在 iOS 上把标题用大字号显示在页面顶部，随着滚动收进导航栏。Android 的顶部应用栏、桌面端和 Web 的栏都保持原样。导航栏跟随的是第一个占据页面大部分区域的滚动视图，所以滚动视图后面还可以垫着别的视图。
- `barColor` 设置 Android 上顶部应用栏的颜色，以及 Web 上顶栏的颜色，用来延续页面自己绘制的背景。iOS 的导航栏在页面顶部时本来就是透明的；桌面端的栏则保持各自的颜色。
- `bottomToolbar { … }` 放屏幕底部一条栏里的按钮，`primary { … }` 放页面的主操作（见[工具栏与主操作](#工具栏与主操作)）；设置 `primaryExtended: true` 后，它在 Android 上是扩展的浮动操作按钮。

### 导航栏按钮

每个按钮都是页面 `toolbar` 里的一个 `Button`。设置了 `icon` 时，导航栏显示图标而不是标题，屏幕阅读器则会读出标题（在桌面端，标题同时也是图标的悬停提示）。

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

在 Android 上，空间放得下的按钮会放进顶部应用栏，放不下的进入它的溢出菜单；长按图标会显示它的标题。在 Mac 上，这些按钮是工具栏项；在 Windows 上，是顶栏末端的按钮；在 Linux 上，是顶栏里的按钮。

### 工具栏与主操作

`bottomToolbar` 把按钮放进屏幕底部的一条栏里，相当于 SwiftUI 的 `ToolbarItem(placement: .bottomBar)`；`primary` 是页面的主操作，比如写邮件或新建，也就是 Material 的浮动操作按钮。它们和导航栏按钮一样都是 `Button`：按钮显示的是 `icon`，屏幕阅读器读的是标题；`disabled` 让按钮变暗，带 `menu` 的按钮点击后弹出菜单；它们读取的内容变化时都会实时反映出来。

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

- iOS：导航控制器的工具栏，按钮之间用弹性空白隔开；页面在最上层时显示，推入和弹出时跟着页面一起滑动。iOS 没有浮动按钮，所以 `primary` 是工具栏末尾的按钮，在 iOS 26 上以醒目样式显示（就像“邮件”的写邮件按钮）；只有 `primary` 的页面也会为它显示工具栏。和 UIKit 一样，页面的安全区域会让出工具栏的位置。从 iOS 18 起，推入的页面如果带有 `bottomToolbar`，会隐藏所在标签页的标签栏，就像“邮件”里打开一封邮件时那样：否则 iOS 26 悬浮的标签栏会压在工具栏上。
- Android：Material 3 的 `BottomAppBar`，按钮以图标按钮的形式从起始端排开，放不下的进入它的溢出菜单。`primary` 是屏幕底部末端的 `FloatingActionButton`；有 `bottomToolbar` 时，它嵌在栏里（没有阴影）。设置 `primaryExtended` 后，它是 `ExtendedFloatingActionButton`，图标旁边显示标题：内容向下滚动时收缩成只剩图标，向上滚动时再展开（有底栏时位于底栏上方）。两者都属于页面自己，所以推入、弹出和预测性返回时都跟着页面走；底栏会延伸到下面的系统导航栏，页面的安全区域会让出底栏的位置。和 Material 的规范一样，推入的页面如果带有 `bottomToolbar`，会隐藏所在标签页的底部导航栏。
- macOS：SwiftUI 的底部栏就是 iOS 那一套，所以在 Mac 上，`bottomToolbar` 的按钮显示在窗口工具栏里、排在导航栏按钮之后，`primary` 排在最后，以醒目样式显示（macOS 26 用强调色着色的工具栏项）。
- Windows：页面下方的一条命令栏（Fluent 的 `CommandBar`）：按钮带着图标和标题从开头排开，放不下的进入“更多选项”（…）菜单，`primary` 是末端的一个强调色按钮。
- Linux：`AdwToolbarView` 里、页面底部的 `GtkActionBar`（GNOME 的操作栏）：按钮在开头，`primary` 在末端，是一个带图标和标题的 `suggested-action` 按钮。
- Web：导航栈底部的一条栏（放不下的按钮进入“更多选项”菜单），`primary` 是浮在右下角（从右到左的布局中是左下角）的按钮；扩展样式的按钮会显示标题，内容向下滚动时收起成只剩图标。

按下按钮时会运行它的 `on press`；在测试中，`app.tap("Compose")` 会按下它，`app.in(.bottomToolbar)` 把点击限定在那条栏里。

### 搜索

`bind search: query` 在导航栏里放一个搜索框，作用和 SwiftUI 的 `searchable` 一样。用户输入会写进 `query`，代码写入它也会改变搜索框里的内容。`searchPlaceholder` 不设置时默认是 "Search"；按下键盘上的搜索键（桌面端是回车键）时会运行 `on searchSubmit`。

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

在 iOS 上，它是标题下方的一个 `UISearchController`，页面滚动时也一直显示。在 Android 上则沿用 Material 的模式：顶部应用栏里放一个搜索按钮，点开后展开成搜索框。如果页面底部有自己的控件，可以设置 `searchButton: true`，让 iOS 上的搜索框也让出位置：在 iOS 26 上它会变成导航栏里的一个按钮；更早的版本里，它藏在内容上方，往下拉才会出现。

在桌面端和 Web 上，搜索框放在页面的栏里：

- **macOS**：工具栏里的搜索框（`NSSearchToolbarItem`）。
- **Windows**：一个带搜索图标的 Fluent 文本框，位于导航栏按钮之前，和 WinUI 在命令栏里放搜索框的做法一样。
- **Linux**：顶栏里的一个搜索按钮，下方是 GTK 的搜索栏，和 GNOME 应用的搜索方式一致；在窗口里任何地方直接打字都会打开它。
- **Web**：顶栏下方的一个输入框（设置 `searchButton: true` 时，则是顶栏里的一个按钮）。

### 从视图缩放展开

`zoomFrom` 是下层页面上某个视图的 `shared` 键，这个视图可以是网格里的一张照片，也可以是一张卡片。页面推入时会从这个视图中放大展开，弹出时再缩回去。参见[共享元素](motion.md#共享元素)。

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

在 iOS 上，这用的是系统的缩放过渡（iOS 18 及以上；更早的版本照常推入）。Android 上是 Asuka 自己实现的缩放，用预测性返回手势弹出时也会缩回到视图里。桌面端和 Web 照常推入和弹出。[`examples/gallery`](../../examples/gallery) 的照片网格就用了这个效果。

## 返回

每个平台都有自己的返回方式，Asuka 不做改动：

- **iOS**：导航栏的返回按钮，以及从页面前缘（leading edge）向内滑动。`backGesture: false` 会关掉这个滑动手势，但返回按钮依然可以弹出页面。
- **Android**：系统返回（按钮或手势都可以），以及顶部应用栏里的向上箭头。返回会弹出屏幕上最内层、并且还有页面可退的那个导航栈：在标签页里，就是当前标签页的导航栈。使用预测性返回手势时，栈顶页面会缩小并跟着手指移动，露出下面的页面。如果所有导航栈都没有页面可弹，返回就和在其他应用里一样，直接离开应用。`backGesture: false` 会关掉预测性返回动画，但返回仍然会弹出页面。
- **macOS**：工具栏的返回按钮，以及菜单项 View ▸ Back（⌘[），没有页面可退时该菜单项不可用。按 Escape 可以关闭弹出面板。
- **Windows**：顶栏的返回按钮，以及 Alt+Left，它会弹出按键所在窗口里最内层的导航栈。按 Escape 可以关闭弹出面板。
- **Linux**：libadwaita 提供的那一套：顶栏的返回按钮、Alt+Left、Escape，以及在触控板或触摸屏上滑动。`backGesture: false` 会关掉滑动手势，返回按钮依然可以弹出页面。
- **Web**：顶栏的返回按钮、浏览器的后退按钮（以及手机上的返回手势）、Escape、Alt+Left（Mac 上是 ⌘[），以及在触摸屏上从前缘滑动。参见 [Web](web.md)。

macOS 和 Windows 本来就没有返回手势，所以 `backGesture` 在这两个平台上不起作用。不管用户用哪种方式返回，页面都会像调用了 `pop()` 一样被销毁。如果当前有打开的弹出面板或对话框，返回会先把它关掉。

### 自己处理返回

在组件体里写 `onBack { … }`，让组件在用户返回时自己决定要不要真的返回：比如先问一下是否丢弃草稿，或者先关掉页面自己的东西（一个搜索、一个展开的面板）。处理函数返回 true 表示这次返回由它接管了；如果所有处理函数都返回 false，返回就照常进行。

```asuka
screen Editor {
  state text = ""
  state saved = ""
  state discarding = false

  title: "Editor"
  onBack {
    if text == saved { return false } // 没有可丢失的内容：页面照常弹出
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

处理函数跟着组件走：组件在哪里构建，它就管哪里的返回；组件移除时，它也一起移除。具体来说：

- **推入的页面**：页面的返回按钮、Android 的返回、iOS 上的边缘滑动、Mac 上的 ⌘[、Windows 和 Linux 上的 Alt+Left、浏览器的后退。页面上有处理函数时，iOS 和 Linux 会在这个页面上关掉滑动返回（滑动手势只能整个禁止，没法先询问再决定，UIKit 自家的应用也是这样），Android 也不会预览弹出的动画。iOS 的返回按钮会触发页面的 `backAction`。
- **弹出面板**：向下滑动、点击面板外部、Android 的返回、桌面端或浏览器里的 Escape，以及 Windows 上 sheet 窗口的关闭按钮。全屏面板则只能用 Android 的返回和 Escape 关闭。面板上有处理函数时就无法被滑走：用户尝试滑动时会询问处理函数（iOS 上是 `isModalInPresentation`；Android 上则是不可取消的 sheet）。如果处理函数放行，面板就会关闭并运行它的 `on dismiss`，就像用户自己关掉了它一样。
- **其他位置**（不在任何页面或面板里，或者在导航栈的第一个页面上，因为返回会离开这一页）：Android 上没有页面可弹时的返回，这时应用本来会关闭；浏览器的后退，这时网页本来会后退。iOS 和桌面端没有这样的返回。

最后注册的处理函数最先被询问。`pop()` 和用代码关闭面板都不会询问处理函数，只有用户主动返回才会。

## 分栏视图

当应用有很多个目的地时，可以把 `Split` 用作应用的顶层：一侧是列出这些目的地的侧边栏，旁边显示选中目的地的页面（相当于 SwiftUI 的 `NavigationSplitView`）。它的子元素是 `Destination`，可以放在 `Sidebar` 分区里，也可以单独放。

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

每个目的地的内容是一个导航栈的第一个页面：它下面的代码用 `push` 推入页面。选择另一个目的地时，栈里的页面会立即被替换成那个目的地的第一个页面，原来的页面随之销毁。第一个页面没有自己的标题时，用目的地的标题。`bind selected` 是选中的目的地，跨所有分区连续计数；用户的选择会写入它，代码写入它也会切换到对应的目的地。`Sidebar` 的标题是其中目的地的分区标题（目的地自己的 `section:` 也可以指定分区）。

| | 紧凑（手机） | 宽屏 |
|---|---|---|
| iOS | 按 iOS 的做法折叠成一个导航栈：侧边栏是第一个页面，显示为 inset grouped 列表；点一行会推入对应的目的地，返回则回到列表 | `UISplitViewController` 的侧边栏放在内容旁边（iOS 26 上是 Liquid Glass 风格），用户可以把它隐藏 |
| Android | Material 的模态导航抽屉，通过目的地第一个页面上的菜单按钮（或从屏幕边缘滑动）打开，选择目的地或按返回时关闭 | 标准抽屉，常驻在内容旁边（宽度 840dp 及以上） |
| macOS | | `NSSplitViewController`，侧边栏是用户可以折叠的源列表（source list） |
| Windows | | WinUI 的 `NavigationView` 窗格：目的地列在内容旁边，选中的那个带有强调色的胶囊指示条 |
| Linux | 窗口宽度小于 500 点时，使用 libadwaita 折叠后的分栏视图：先显示侧边栏，再显示选中目的地的页面，页面上有回到侧边栏的返回按钮 | `AdwNavigationSplitView`：侧边栏是独立的一页，标题在它自己的顶栏里，旁边是内容，内容上方是窗口的顶栏 |
| Web | 一个抽屉，从详情页的顶栏打开 | 侧边栏放在详情旁边 |

目的地的 `badge` 显示在它那一行的末端，就像“邮件”在邮箱旁边显示未读邮件数（相当于 SwiftUI 在侧边栏行上用 `badge`）：通常是一个数字，不过任何简短的文字都可以；为空时不显示。它是一个绑定，所以会跟随你的数据变化（上面的 `badge: "{unread}"`）；变化时那一行原地更新，侧边栏的选中项、滚动位置和焦点都不受影响。

| | 目的地的角标 |
|---|---|
| iOS | 单元格末端的一个标签附件，使用次要文字颜色 |
| Android | 抽屉项末端的计数，和 Gmail 一样 |
| macOS | 源列表的计数，使用次要文字颜色，和“邮件”的邮箱一样 |
| Windows | 窗格项末端的 Fluent `InfoBadge` |
| Linux | 行末端一个变暗的数字 |
| Web | 行末端的一个数字 |

屏幕阅读器会把它和行的标题一起读出来，数字读作若干项：“Inbox, 3 items”（Android 上用 Material 角标的说法，“3 new notifications”）。

## 标签页

`Tabs` 一次只显示一个页面，并带有一条标签栏：

- **iOS**：`UITabBarController`。
- **Android**：Material 的底部导航栏。
- **macOS**：`NSTabView`，标签在顶部。
- **Windows**：顶部的一排标签，仿照 WinUI 顶部样式的 `NavigationView`：每个标签有一个图标和一个标题，选中的标签下方有强调色的胶囊指示条；可以用方向键在标签之间切换。
- **Linux**：`AdwViewStack`，和 GNOME 应用一样，窗口较宽时视图切换器放在顶栏里，窗口较窄时（窄于 550 点）则变成底部的一条栏。
- **Web**：底部的标签栏。

每个 `Tab` 有标题、`icon` 和它的内容。`bind selected` 是当前显示的标签页的索引：用户选择标签时会写入它，代码写入它也会切换标签。

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

标签页的 `badge` 显示在标签上（相当于 SwiftUI 在标签项上用 `badge`，或 Compose 在 `NavigationBarItem` 上用 `BadgedBox`）：通常是一个数字，比如“3”，不过任何简短的文字都可以；为空时不显示。它是一个绑定：变化时只更新这一个标签。

| | 标签页的角标 |
|---|---|
| iOS | `UITabBarItem` 的 `badgeValue`，系统的红色角标 |
| Android | 标签项的 `BadgeDrawable`：数字显示为数字（超过 999 显示 999+），其他文字原样显示 |
| macOS | 写进标签的标题里，比如“Inbox (3)”：`NSTabView` 的标签只能显示标题 |
| Windows | 标签末端的 Fluent `InfoBadge` |
| Linux | 数字作为页面的 `badge-number`，显示在视图切换器里的图标旁边；其他文字则显示为 `needs-attention` 的小圆点 |
| Web | 标签图标上的红色角标 |

屏幕阅读器会把它和标签的标题一起读出来，数字读作若干项：“Inbox, 3 items”（iOS 和 Android 上用平台自己的说法）。

给每个标签页一个独立的 `Stack`，这样它既有自己的导航栏，也能保留自己的浏览历史，和各平台的原生应用一样。标签页的内容在第一次被选中时构建，并在切换标签时保留下来。标签图标和按钮图标一样，都是符号：见[符号和图标](components.md#符号与图标)。在 Android 上，图标必须是 Android 自带的。

## 弹出面板

`open` 为 true 时，`Sheet` 会在模态的弹出面板里展示它的内容：

- **iOS**：带停靠高度（detent）的 sheet 呈现。
- **Android**：`BottomSheetDialog`。
- **macOS**：一个 sheet 窗口，附着在当前使用的窗口上。
- **Windows**：盖在当前窗口上方的模态窗口，期间原窗口不可操作。
- **Linux**：`AdwDialog`：一个浮动的对话框，窗口较窄时则是底部面板。
- **Web**：一个模态 `<dialog>`，窄窗口里是底部面板，宽窗口里是居中的卡片。

它在声明的位置不占空间，所以可以放在打开它的那个页面里的任何位置。

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

- `detents` 是面板可以停留的高度：`.medium`（半屏）、`.large`（全屏），或者两者都要。两者都设置时，面板从半屏开始，用户可以把它往上拖到全屏。不设置时就是全屏。键盘弹出时，能变成全屏的面板会自动变成全屏。在桌面端，只设置了 medium 的面板高度是窗口的一半，其他情况则占窗口的大部分；在 Mac 上，用户可以自己调整 sheet 窗口的大小。`.fit` 则让面板和内容一样高，就像 Mac 上的 sheet：适合表单或确认这类内容本身有高度的面板。内容变化时高度随之变化，超过面板的最大高度时由其中的 `Scroll` 滚动；用户不能调整它的大小。目前只有 macOS 支持，其他平台上它是全屏的。
- `grabber: true` 在面板顶部显示拖动把手（iOS、Android，以及 Web 的底部面板）。
- 它的内容在面板打开时构建、关闭时销毁，所以在里面创建的状态每次都是全新的。

### 可移动的面板

`movable: true` 让桌面端的面板变成用户可以移动的对话框，就像桌面上的模态对话框那样，而不是贴在窗口上的 sheet：适合用户想放在下面内容旁边对照着看的详情和表单。它和普通面板一样是模态的：关闭之前，它后面的内容不接受任何输入；Escape 会关闭它，点击它外面则不会。它没有标题栏：内容就是它的全部，它自己的标题行就是顶端（要给它一个“关闭”按钮）；用户拖动它的背景来移动它，也就是控件、可点击的视图和可选中的文字以外的任何地方。它打开时居中显示在它的窗口上方，大小由 `detents` 决定；`.fit` 的面板和内容一样大，并像任何 `.fit` 面板那样随内容变化。`resizable: true` 让用户还能调整它的大小，适合值得放大来看的内容，比如一份很长的报告：没有它，可移动的面板不能调整大小，就像桌面上的对话框通常不能一样（`Window` 则默认可以调整大小，除非写上 `resizable: false`）。可调整大小的 `.fit` 面板打开时和内容一样大，这也是用户能把它缩到的最小尺寸；此后它保持用户给它的大小，内容填满它。

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

| | 可移动的面板 |
|---|---|
| macOS | 一个没有标题栏的独立窗口（保留窗口的阴影和圆角；有 `resizable` 时可以拖动边缘调整大小），居中显示在它所在窗口的前面，在打开的位置淡入；在此期间，它所在的窗口不接受点击、滚动和悬停，⌘W 会关闭它 |
| Windows | 盖在当前窗口上的模态窗口，普通面板本来就是这样 |
| Web | 宽窗口上一张盖在遮罩上的卡片，在打开的位置淡入；有 `resizable` 时拖动右下角来调整大小 |
| iOS、Android、Linux | 和不设置时一样的面板：手机上的面板不能移动，libadwaita 的对话框也不能 |

`fullScreen` 的面板会忽略它。

### 全屏弹出面板

`fullScreen: true` 让面板盖住整个屏幕，适合那些需要用户专心完成或主动放弃的任务：写一封邮件、登录、编辑一张照片（相当于 SwiftUI 的 `fullScreenCover`，以及 Material 的全屏对话框）。`detents` 和 `grabber` 对它不起作用，也没有面板外部可以点击，不能滑走：只有 `open` 变成 false 时它才会关闭，所以要给它放上自己的“取消”和“完成”按钮。Android 的返回，以及桌面端或浏览器里的 Escape，会像关闭普通面板一样关闭它，除非其中的 `onBack` 拦下了（比如要先确认是否丢弃草稿）。

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

  onBack { return !draft.isEmpty() } // 返回不会丢掉草稿

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

它的环境就是屏幕本身的环境：在手机上，尺寸是整个屏幕，安全区域包括状态栏、刘海、Home 指示条或系统导航栏，以及键盘，所以 `SafeArea` 能像在根组件里一样让内容避开它们。

| | 它是什么 |
|---|---|
| iOS | 以 `.fullScreen` 方式呈现的控制器：从下方滑上来盖住一切，关闭之前下面的页面都看不到 |
| Android | 一个全屏 `Dialog`，使用它自己的主题，铺满屏幕边缘（处理状态栏和导航栏的 inset、刘海和键盘），以 Material 的强调缓动曲线从下方滑入；预测性返回时，它会朝滑动开始的那一侧缩小，四角变圆，除非被 `onBack` 拦下 |
| macOS | 一个 sheet，大小等于窗口标题栏和工具栏以下的全部内容，随窗口一起调整大小，用户不能单独调整它：Mac 没有全屏模态，而 AppKit 让一个窗口专心处理一件事的方式就是 sheet |
| Windows | 一个无边框窗口，盖住所在窗口的全部内容（标题栏仍属于原窗口，期间原窗口被禁用），随窗口一起移动和调整大小，底部两角像 Windows 11 的窗口那样是圆角：相当于 Fluent 的全尺寸 `ContentDialog` |
| Linux | 一个和窗口一样大的浮动 `AdwDialog`，libadwaita 会让它盖住整个窗口，就像对待放不下的对话框那样（在手机上或窄窗口里）；它会随窗口一起调整大小 |
| Web | 盖住整个页面的模态 `<dialog>`（焦点留在里面；Escape 和浏览器的后退会关闭它）；如果面板是在 Window 面板里声明的，就只盖住那个面板的内容；它从下方滑入 |

用户可以向下滑动来关闭面板；在 Android 上还可以按返回或点击面板外部；在桌面端或浏览器里可以按 Escape（Windows 上还可以点面板窗口的关闭按钮，Web 上还可以点击面板外部）。这些操作会把 `open` 设为 false，并运行 `on dismiss`。如果是代码把 `open` 设为 false，面板同样会关闭，但不会运行 `on dismiss`。因此，`on dismiss` 的含义是“面板在你没有决定的情况下被关掉了”，比如草稿还没保存就被关掉。

## 气泡弹窗

`open` 为 true 时，`Popover` 会在页面上方显示一个指向某个视图的气泡，里面是它的 `content`：几个选项、一段详情或者一个小表单。它的子元素就是气泡所指向的对象：

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

气泡的大小由内容决定：宽度不超过 `maxWidth`（默认 320），文字在这个宽度内换行；高度不超过屏幕的大部分。它的内容在打开时构建、关闭时销毁。点击外部会关闭它，把 `open` 设为 false 并运行 `on dismiss`，和弹出面板一样；气泡里的 `onBack` 可以把它留住。

| | 视图 | 位置 |
|---|---|---|
| iOS | `UIPopoverPresentationController`，在 iPhone 上也是 popover（不会被适配成 sheet），iOS 26 上是 Liquid Glass 风格 | 由 UIKit 决定放在哪里，箭头指向锚点视图 |
| Android | 放在 Material 容器表面（container surface）上的 `PopupWindow`，16dp 圆角，带投影 | 锚点下方居中（下方放不下时放到上方） |
| macOS | `NSPopover`，transient 类型 | 锚点下方，具体位置由 AppKit 决定 |
| Windows | 一个弹出窗口，点击别处即关闭（light dismiss），带 Windows 11 的圆角（相当于 WinUI 的 Flyout） | 锚点下方居中（下方放不下时放到上方） |
| Linux | `GtkPopover`；按 Escape 也能关闭 | 锚点下方，GTK 找得到空间的地方 |
| Web | 一个 popover | 锚点旁边 |

## 警告框和操作表

`alert(title, …)` 会立即显示一个警告框，`Alert` 元素则在它的 `open` 为 true 时显示一个：

- **iOS**：`UIAlertController`。
- **Android**：Material 对话框。
- **macOS**：`NSAlert`，以 sheet 的形式显示在当前窗口上。
- **Windows**：显示在当前窗口上的任务对话框。
- **Linux**：`AdwAlertDialog`。
- **Web**：模态 `<dialog>`。

按下任意按钮都会关闭警告框，并运行该按钮的 `run`（`Alert` 里的 `Button` 则运行它的 `on press`）。没有按钮时，警告框只有一个 OK 按钮。

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

按钮的 `role` 表明它的用途：

- `.normal`（默认）是普通按钮。
- `.cancel` 是用来退出的按钮。iOS 会把它放在专门的位置，Android 则把它作为对话框的否定按钮（negative button）。在桌面端，按 Escape 会触发它；在 Mac 上，它是最后一个按钮，排在默认按钮之后。
- `.destructive` 显示为红色：在 iOS 上用系统的破坏性样式，在 Android 上用 Material 的错误色，在 Mac 上是 AppKit 的破坏性按钮，在 Linux 上是 libadwaita 的 destructive 响应。

在 Android 上，除取消按钮外的最后一个按钮是对话框的肯定按钮（positive button），位于末尾；如果还有第二个，它就是中性按钮。取消之外的按钮超过两个时，对话框会把它们显示成一个列表。在 Mac 上，除取消外的第一个按钮是默认按钮，按回车键会触发它；在 Linux 上，第一个 `.normal` 按钮是默认按钮，以强调色显示。

设置 `actionSheet: true` 后，这些按钮会显示成一个操作表（action sheet），在 iOS 上就是从底部升起的一列选项：

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

在 Android 上，操作表是一个把按钮排成列表的对话框，并且不显示 `message`。在 Windows 上，它的按钮是任务对话框的命令链接（command link）；在 Linux 上，它是一个没有默认按钮的警告对话框；在 Mac 上，它和普通警告框没有区别；在 Web 上，它是一个按钮排成列表的对话框。如果警告框是以其他方式关闭的（比如在 Android 上按返回或点击外部），不会运行任何按钮（`Alert` 的 `on dismiss` 会运行）。

### 输入框

警告框可以请用户输入文字，比如名字或密码，就像 SwiftUI 在警告框里放 `TextField` 和 `SecureField` 那样。`Alert` 里的 `TextField` 显示在消息下方。每个输入框绑定的 `text` 以它当前的值作为初始内容，用户输入时会写入它，所以按钮的 `disabled` 可以跟着它变：下面的例子里，两个输入框都填好之前，“Sign In”一直是禁用的，警告框显示期间会实时更新这个按钮。

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

和 `TextField` 一样，输入框也接受 `keyboard`，以及用于自动填充的 `contentType`。`secure` 会隐藏输入的内容。第一个输入框会获得焦点，键盘随之弹出。在输入框里按回车会跳到下一个；在最后一个输入框里按回车，则会按下默认按钮，也就是第一个 `.normal` 按钮（相当于 UIAlertController 的 `preferredAction`），但只有在它可用时才会生效。警告框显示期间，代码写入输入框的 `text` 不会改变输入框里显示的内容。操作表不显示输入框。

| 平台 | 输入框 |
| --- | --- |
| iOS | `UIAlertController` 的文本框；默认操作就是它的 `preferredAction`，以粗体显示 |
| Android | Material 对话框里的描边 `TextInputLayout`（密码框带显示切换按钮）；输入法的操作键跳到下一个输入框，在最后一个输入框里则按下默认操作的按钮 |
| macOS | 作为 `NSAlert` 附属视图的 `NSTextField` 和 `NSSecureTextField`；用 Tab 在它们之间切换 |
| Windows | Asuka 自己的对话框，按 WinUI `ContentDialog` 的样子排版（任务对话框放不了输入框）：标题、消息、Fluent 风格的文本框，然后是按钮，默认按钮使用强调色；Tab、Enter 和 Escape 的行为和普通对话框一致，讲述人会用占位文字来读出每个输入框 |
| Linux | 由 `AdwEntryRow` 和 `AdwPasswordEntryRow` 组成的卡片列表，作为 `AdwAlertDialog` 的额外子控件 |
| Web | `<dialog>` 里的输入元素，以占位文字作为标签，带有 `type=password`、`inputmode` 和 `autocomplete` |

## Snackbar

`snackbar(message, …)` 会立即在窗口底部显示一条简短的消息，可以附带一个操作，比如“撤销”；它返回一个用来关闭它的闭包：

- **iOS**：UIKit 没有 snackbar，所以 Asuka 按 iOS 显示自己简短提示的样子绘制一个：使用系统材质（iOS 26 上是 Liquid Glass）的胶囊，居中显示在标签栏或工具栏以及键盘的上方，位于最上层呈现的内容之上。它以弹簧动画出现和消失（开启“减弱动态效果”时改为淡入淡出），向下滑动可以关闭。
- **Android**：Material 的 `Snackbar`，可以滑动关闭，显示在页面的底部导航栏或浮动操作按钮（它的锚点）以及键盘的上方；在弹出面板上时，显示在面板底部。
- **macOS**：窗口底部的一个浮层，放在 `NSVisualEffectView` 里，带关闭按钮。
- **Windows**：窗口底部的一个浮层，绘制成 Fluent InfoBar 的样子，带关闭按钮。
- **Linux**：窗口的 `AdwToastOverlay` 里的一个 `AdwToast`。
- **Web**：窗口底部一个 role 为 `status` 的元素（一个位于顶层的 popover），带关闭按钮；向下滑动可以关闭。

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

消息只显示一次，所以和警告框的消息一样是字符串。操作是一个 `.button`：显示它的 `title`，它的 `role`、`icon` 和 `disabled` 都不显示。按下操作会关闭 snackbar 并运行它的 `run`。以其他任何方式消失时则运行 `onDismiss`：时间到了、用户滑掉或关掉了它、被另一个 snackbar 替换，或者调用了 `snackbar` 返回的闭包。所以用户点了“撤销”之后，`onDismiss` 不会运行：这里正适合把那个本可以撤销的操作真正落实。

`duration` 是它显示多久：`.short`（默认，约四秒，与 Material 一致）、`.long`（约十秒）或 `.indefinite`（直到被关闭或按下它的操作）。Asuka 会按平台对无障碍的建议延长显示时间：Android 按 `AccessibilityManager.getRecommendedTimeoutMillis`，开启 TalkBack 时，带操作的 snackbar 会显示得更久；iOS、macOS 和 Linux 没有推荐的超时时间，所以屏幕阅读器开启时，带操作的 snackbar 会一直显示到被关闭，这和 Material 在没有推荐值时的做法一样；Windows 还会参考“在此时间后关闭通知”这项设置。在桌面端和 Web 上，鼠标悬停在它上面或键盘焦点在它里面时，计时会暂停。它出现时，屏幕阅读器会播报它，也能操作到它的按钮。

同一时间只显示一个：新的 snackbar 会替换正在显示的那个，和 Android 一样（旧的会先运行 `onDismiss`）。在桌面端，它显示在警告框会出现的那个窗口：触发它的事件所在视图的窗口，否则是调用它的组件所在的窗口，再否则是主窗口。

## 菜单

菜单显示一组 `Action`，它和警告框按钮、导航栏按钮是同一个类型，每个标题旁边还可以加一个 `icon`。菜单有两种用法：

- 设置了 `menu` 的 `Button`，点按时打开菜单。
- `Menu { actions: […] }` 可以给任何内容加上菜单：点按时打开，或者设置 `longPress: true`，作为上下文菜单使用。

在 iOS 上它是 `UIMenu`，在 Android 上是 `PopupMenu`。在 Mac 上是 `NSMenu`；在 Windows 上是弹出菜单（和 Mac 一样，破坏性的菜单项显示为红色）；在 Linux 上是 `GtkPopoverMenu`；在 Web 上是一个 popover。在桌面端和 Web 上，右键（也就是辅助点按）会打开上下文菜单；在 Windows、Linux 和 Web 上，长按也可以。这两种用法在[组件](components.md#菜单)里都有介绍。`List` 的行还支持滑动操作：见[列表](components.md#列表)。放在 `MenuBar` 里的 `Menu` 则是应用的一个菜单（见[下文](#菜单栏和键盘快捷键)）。

## 窗口

桌面应用的主窗口用来显示 `app` 块展示的内容，除非应用没有主窗口（见[下文](#没有主窗口的应用)）。`app` 块里的 `Window` 就是主窗口：它的键是主窗口的选项，它的子元素是主窗口显示的内容：

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

尺寸以点（point）为单位，指的是窗口内容区的大小。什么都不设置时，主窗口以 800×600 点的大小居中打开，用户不能把它缩到 320×320 以下。`title` 为空时使用应用名；它是一个绑定，所以可以跟随应用的语言变化。窗口会回到用户上次离开时的位置（见[下文](#记住窗口位置)），这一点优先于 `width`、`height`、`x` 和 `y`。iOS 和 Android 会忽略这些选项。除非 `open` 为 false，主窗口都会显示：`bind open: shown` 会随 `shown` 的变化显示或隐藏它；在保持运行的应用里，用户关闭主窗口时会把它写成 false。

其他 `Window` 在 `open` 为 true 时打开一个自己的窗口，就像 `Sheet` 打开弹出面板一样：

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

- 它的子元素在窗口打开时构建、关闭时销毁。在里面调用 `environment()` 得到的是这个窗口的环境：尺寸会随用户调整窗口而变化，缩放比例取决于窗口所在的显示器。
- 用户关闭窗口时，`open` 会被写成 false，并触发 `close`。用代码把 `open` 设为 false 也会关闭窗口，但不会触发 `close`。
- 除非另外指定，窗口以 800×600 点打开；在 Mac 和 Windows 上，它会相对当前使用的窗口层叠错开。在 Linux 上，窗口位置由桌面环境决定，因为 GTK 4 把这件事交给了它（GNOME 和 KDE 会把新窗口放在遮挡其他窗口最少的地方）。窗口里的导航栈和主窗口里一样，有自己的栏。弹出面板和警告框会在用户正在使用的那个窗口上打开。

| | 窗口 |
|---|---|
| macOS | 一个 `NSWindow`，有自己的工具栏和标题（来自它最内层的导航栈） |
| Windows | 一个顶层窗口，有自己的标题和任务栏按钮 |
| Linux | 一个 `AdwWindow`，有自己的顶栏 |
| Web | 页面里的一个面板，用户可以移动它、调整它的大小 |

手机没有窗口：`Window` 在手机上什么也不显示。提供窗口的组件应该只从桌面端才有的入口提供它，比如下面要讲的菜单栏。

### 窗口选项

主窗口和 `Window` 接受同一组键，用来控制窗口如何打开；这些键在窗口打开时读取（之后要改变的是句柄的操作，见下文，不过 `title`、`alwaysOnTop`、`clickThrough`、`flyout`、`edited` 和 `file` 会跟随各自的绑定）：

| 键 | |
|---|---|
| `width`、`height` | 内容区的大小，单位为点 |
| `minWidth`、`minHeight`、`maxWidth`、`maxHeight` | 用户能把窗口调到的最小和最大尺寸；为 0 时，最小值取平台的下限，最大值不设上限 |
| `x`、`y` | 窗口框架左上角在屏幕上的位置，以主显示器的左上角为原点，单位为点；两者都为 0 时，主窗口居中，`Window` 层叠错开（设置 `center: true` 则让它居中） |
| `resizable`、`minimizable`、`maximizable`、`closable` | 设为 false 会去掉用户的相应能力：调整窗口大小，以及对应的窗口按钮 |
| `alwaysOnTop` | 让窗口保持在其他应用的窗口之上 |
| `titleBar` | `.hidden` 让内容延伸到标题栏下方、直到窗口顶边，窗口按钮浮在内容上面 |
| `background`、`transparent` | 内容背后的窗口颜色；或者不要颜色，让窗口后面的东西从内容没有绘制的地方透出来 |
| `hidden` | 打开窗口但不显示，调用 `w.show()` 才显示出来 |
| `skipTaskbar` | 不在任务栏（Windows）和 Window 菜单（macOS）里出现 |
| `maximized`、`fullScreen` | 以最大化或全屏的状态打开 |
| `stateKey` | 保存窗口位置时用的键，见[下文](#记住窗口位置) |
| `fitContent` | 让窗口大小随内容变化，限制在最小和最大尺寸之间（[桌面端](desktop.md#大小适应内容的窗口)） |
| `panel` | 启动器或命令面板那样的窗口：浮在所有应用之上，能接收键盘输入，却不会把应用切到前台；按 Escape 或点击别处时关闭（[面板](desktop.md#面板与启动器)） |
| `clickThrough` | 让鼠标穿过窗口，落到后面的东西上（[点击穿透](desktop.md#鼠标穿透窗口)） |
| `flyout` | 仅用于 `Window`：让窗口从托盘图标或父窗口的某个矩形区域弹出（[Flyout](desktop.md#浮出面板)） |
| `child`、`modal` | 仅用于 `Window`：见[下文](#子窗口和模态窗口) |
| `menuBar`、`edited`、`file` | 显示文档的窗口用：分别对应窗口里的应用菜单栏（Windows、Linux）、Mac 关闭按钮里的圆点或标题前的 `*`、Mac 标题栏里的文件图标 |

隐藏标题栏后，内容会一直延伸到窗口顶边，位于窗口按钮（Mac 的红绿灯、Windows 的标题栏按钮、Linux 顶栏上的按钮）下方；窗口环境的 `safeTop` 则等于标题栏的高度。这样 `SafeArea` 会避开这些按钮，而背景、侧边栏或图片可以铺到按钮下面：

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

在内容不接收点击的地方，按钮下方的那一条区域仍然可以拖动窗口。在 Windows 上，隐藏标题栏后主窗口就没有菜单栏了，但菜单的快捷键依然有效。

如果希望窗口准备好之后才显示，可以用 `hidden` 打开它，再从代码里显示：在此期间内容已经构建、布局并绘制好了，所以窗口一出现就是完整的，绝不会先闪一下空白。在后台启动的应用（比如托盘图标应用）从菜单里打开它的窗口，根本不需要主窗口（见[下文](#没有主窗口的应用)）。

### 控制窗口

`Window as w` 给窗口的句柄起个名字，它是这个 `Window` 的 `Ref`：外围视图里的元素都能看到它，也可以把它传给组件。其他视图的 `Ref` 作用于该视图所在的窗口。

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

- `w.show()` 和 `w.hide()` 用来显示和隐藏窗口；隐藏后窗口仍然是打开的，内容也继续存在。`w.focus()` 会在窗口隐藏或最小化时把它显示出来，把它带到最前面并让它获得键盘焦点，同时把应用切到前台：托盘图标的菜单就是这样把窗口找回来的（Windows 只允许应用在用户对它有过输入之后切到前台，点击它的托盘图标就算一次输入）。对已经关闭的 `Window` 调用 `show()` 和 `focus()` 会重新打开它。
- `w.minimize()`、`w.maximize()` 和 `w.restore()`（从最小化或最大化状态还原都可以）；`w.setFullScreen(on)`。
- `w.setSize(width, height)`（内容区的大小，单位为点）、`w.setPosition(x, y)`（窗口框架的左上角，以主显示器的左上角为原点）、`w.center()`。
- `w.setAlwaysOnTop(on)` 设置窗口是否保持在其他应用的窗口之上。
- `w.setClickThrough(on)` 设置鼠标是否穿透窗口。
- `w.close()` 像用户操作那样关闭窗口，会先询问（见下文）；`window.close()` 则不询问，直接关闭调用它的代码所在的窗口（不在任何 `Window` 里时是主窗口）。主窗口关闭时，应用就会退出，除非它保持运行。
- 无论变化来自用户还是应用，窗口的状态都会跟着更新：`w.visible`、`w.focused`、`w.minimized`、`w.maximized`、`w.fullScreen`、`w.alwaysOnTop`、`w.clickThrough`、`w.position` 和 `w.size`（分别是 `(x, y)` 和 `(width, height)`），也可以用 `w.windowState` 一次读出全部状态。读取它们的绑定会跟随它们；只有在有东西读取时，才会报告窗口的状态。最小化的窗口、被其他窗口挡住的窗口，都仍然算作可见。

`window.main()` 是主窗口的句柄，`window.current()` 是调用它的代码所在窗口的句柄（在某个 `Window` 里就是那个 `Window` 的，否则是主窗口的），在任何地方都能用：菜单项里、托盘图标里、store 里。

这些操作和状态写入一样，要到屏幕下一次更新时才作用到窗口上。在手机上它们什么也不做，读出来的状态是什么也没显示。

### 关闭窗口前先询问

当用户关闭窗口（点关闭按钮、按 ⌘W 或 Alt+F4、用程序坞或任务栏的菜单），或者调用 `w.close()` 时，都会先询问：

- 窗口里的 `WindowCloseGuard { when: …; on closeRequest { … } }` 在 `when` 为 true 时让窗口保持打开，并执行它的 `closeRequest`：先问问用户是否保存更改，用户做出选择后再调用 `window.close()`。
- 组件里的 `onCloseRequest(fn() -> bool)` 在组件存在期间询问一个函数：返回 false 会让窗口保持打开。
- `Window` 自己的 `on closeRequest { … }`（主窗口也有）接管用户的关闭操作：窗口会保持打开，直到 `open` 为 false 或调用 `window.close()`。

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

所有被询问的守卫和函数都同意，窗口才会关闭。在 Web 上，主窗口就是浏览器标签页：离开页面或关闭标签页时同样会询问；如果被拒绝，页面会由浏览器自带的“离开网站？”提示拦下来（提示文字无法由页面指定）。退出应用（⌘Q）时不会询问各个窗口，而是询问应用的 `on quit`（见[桌面端](desktop.md#退出)）。

### 记住窗口位置

窗口会在多次启动之间记住位置，回到用户上次离开时的地方：主窗口总是如此（用键 `"main"`，或者它的 `stateKey`），`Window` 则要设置了 `stateKey` 才会记住。每一种窗口都应该用自己的键（如 `"inspector"`、`"preferences"`）。记住的位置优先于 `width`、`height`、`x` 和 `y`，这几个值只决定第一次启动时窗口怎么打开。

| | 保存的内容 |
|---|---|
| macOS | 窗口框架，通过 AppKit 的 frame autosave 保存（存在应用的 defaults 里） |
| Windows | 位置和大小（以屏幕像素计，并记下当时的 DPI），以及是否最大化；恢复时会让窗口完整地落在一台现有的显示器上 |
| Linux | 大小和是否最大化：Wayland 应用无法自己摆放窗口位置 |
| Web | `Window` 面板的位置、大小以及是否最大化，保存在页面的 `localStorage` 里 |

桌面应用把这些信息存在应用数据目录下的 `asuka-window.json` 里（Windows、Linux），或者存在应用的 defaults 里（macOS）。测试和自动化运行时什么都不保存。

### 子窗口和模态窗口

设置了 `child: true` 的 `Window` 是它声明所在窗口的子窗口，这个父窗口可以是主窗口，也可以是内容里包含它的那个 `Window`。子窗口始终位于父窗口之上，并随父窗口一起关闭（它的内容会随父窗口的内容一起销毁）。`modal: true` 则让它成为一个会阻塞父窗口的子窗口，直到它关闭为止：

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
| macOS | 子窗口，层级排在父窗口之上，并跟随父窗口移动 | 父窗口的 sheet |
| Windows | 由父窗口拥有（owned）：位于父窗口之上，随父窗口一起最小化 | 由父窗口拥有，打开期间父窗口不可操作 |
| Linux | 相对父窗口设为 transient，随父窗口一起销毁 | transient 且模态 |
| Web | 和其他面板一样的面板 | 一个面板，下面垫着一层遮罩，拦截页面上的点击 |

### 没有主窗口的应用

如果 app 代码块没有自己的 `Window`，它显示的内容就放在主窗口里。如果它在那里什么都不显示，应用就根本没有主窗口：它的组件只显示 `Window`、托盘图标、`MenuBar` 和全局快捷键。它的窗口就是它自己打开的那些，菜单栏应用和文档类应用都是这样：

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

- `window.main()` 依然指向主窗口，但对它的操作什么也不做，读出来的状态是什么也没显示，和在手机上一样；在任何 `Window` 之外调用的 `window.current()` 也是这个句柄。
- 重新打开应用（点击程序坞图标、再次启动应用）时，应用会带着已经打开的窗口来到前台；如果一个窗口也没打开，就什么也不显示。
- 用户关闭最后一个窗口时，应用就会结束，除非它保持运行（`asuka.toml` 的 `keepRunning` 或 `background`），菜单栏应用就是这样做的。

有没有主窗口，是在编译应用时根据它的代码在目标平台上可能显示什么来决定的（`if @platform` 取该平台的分支），而不是看它的状态当前显示了什么：一个可能显示视图的代码块就有主窗口，哪怕它此刻什么都没显示，主窗口也只是空着。在 Web 上，标签页就是主窗口，这种情况下它什么也不显示；在手机上，屏幕始终属于应用，无论它显示什么。

### 各桌面平台的支持情况

| | macOS | Windows | Linux (GTK 4) | Web：标签页 | Web：`Window` |
|---|---|---|---|---|---|
| 显示、隐藏、聚焦 | 支持 | 支持 | 支持 | 只能聚焦 | 支持 |
| 最小化 | 支持 | 支持 | 支持（Wayland 不会回报最小化状态） | 不支持 | 收起到标题栏 |
| 最大化、还原 | 缩放（zoom） | 支持 | 支持 | 不支持 | 铺满应用区域 |
| 全屏 | 支持 | 支持 | 支持 | Fullscreen API | Fullscreen API |
| 大小、最小尺寸 | 支持 | 支持 | 支持 | 不支持 | 支持 |
| 最大尺寸 | 支持 | 支持 | 不支持：GTK 4 没有这个概念 | 不支持 | 支持 |
| 位置、居中 | 支持 | 支持 | X11 支持；Wayland 不支持，窗口位置由合成器决定 | 不支持（能读取浏览器窗口的位置） | 支持 |
| 置顶 | 支持 | 支持 | 仅 X11 | 不支持 | 位于其他面板之上 |
| 隐藏标题栏 | 支持 | 支持 | 支持 | 不支持 | 支持 |
| 背景色、透明 | 支持 | 支持背景色；透明会退回为背景色 | 支持（透明需要合成器） | 背景色 | 支持 |
| 不显示在任务栏 | 不显示在 Window 菜单 | 支持 | 仅 X11 | 不支持 | 不支持 |
| 面板、flyout | 支持 | 支持 | X11：保持置顶，可以定位；Wayland：无法定位，也不能置顶 | 不支持 | 支持 |
| 适应内容大小 | 支持 | 支持 | 支持 | 不支持 | 支持 |
| 点击穿透 | 支持 | 支持 | 支持 | 不支持 | 穿透到页面 |

## 菜单栏和键盘快捷键

`MenuBar` 为桌面应用提供菜单，并给其中的菜单项配上键盘快捷键。它的子元素是 `Menu`，`Menu` 的子元素是 `MenuItem`：

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

- `MenuItem` 有一个标题、一个 `shortcut` 和它的 `press`；`disabled` 和 `checked` 会改变它的显示方式。菜单项里再放 `MenuItem`，它就成了子菜单。`MenuSeparator()` 是菜单项分组之间的分隔线。
- `role` 说明菜单项是什么：`.destructive` 把它显示为红色（macOS、Windows；GTK 会把它单独隔开）；`.quit`、`.close` 和 `.openRecent` 是平台自己的菜单项，由 Asuka 负责执行，并使用平台自己的标题（除非菜单项设置了标题）和快捷键：退出应用（会先询问应用）、像点关闭按钮那样关闭当前使用的窗口，以及最近使用的文档（Mac 自带的“打开最近使用”）。
- `Shortcut` 由一个 `key` 和它的修饰键组成：`primary` 是平台的命令键（Mac 上是 ⌘，其他平台上是 Ctrl），`shift`、`alt`（Mac 上的 Option）和 `control` 是其他修饰键。键可以是一个字符（`"n"`、`","`、`"="`），也可以是一个键名：`enter`、`escape`、`tab`、`space`、`delete`、`backspace`、`up`、`down`、`left`、`right`、`home`、`end`、`pageup`、`pagedown`，以及 `f1` 到 `f12`。
- 菜单项都是绑定：某个菜单项被禁用或勾选、`for` 或 `if` 显示的菜单或菜单项出现又消失，都会重新发送。同时显示了多个 `MenuBar` 时，以最后一个为准；它消失后，就换成前一个。它不占空间，放在哪里都可以。
- `WindowKeys { keys: [Shortcut(key: "k", primary: true, run: search)] }` 在显示期间给所在的窗口加上一些按键：无论焦点在哪里，只要没有视图处理它们就会生效（以最后一个 `WindowKeys` 为准），适合那些不是菜单项的按键。

各桌面平台以各自的方式显示菜单：

- **macOS**：显示在菜单栏里，和 Mac 自带的菜单放在一起：应用菜单（About、Hide、Quit）、File、Edit（Undo、Cut、Copy、Paste……）、View（Back、Enter Full Screen）和 Window。标题是 File、Edit、View、Window 或 Help 的菜单，会把菜单项并入对应的系统菜单；其他菜单则放在 View 和 Window 之间。
- **Windows**：主窗口（以及设置了 `menuBar: true` 的 `Window`）上的一条菜单栏，快捷键在应用的所有窗口里都有效。每个菜单和菜单项都会分配一个访问键（如 Alt+F），每个菜单项的标题后面会显示它的快捷键（如 "Ctrl+Shift+K"）。
- **Linux**：GNOME 应用没有菜单栏，而是有一个主菜单，也就是顶栏上的最后一个按钮。应用的每个菜单都是主菜单的一个子菜单（只有一个菜单时，它的菜单项直接放进主菜单），快捷键在所有窗口里都有效。
- **Web**：快捷键在页面里有效，但不显示菜单栏。

iOS 和 Android 会忽略 `MenuBar`，所以无论应用跑在哪个平台上，都可以放心带上菜单栏。不过菜单项都是命令，为了照顾手机，每个命令也要能从界面上找到。

## Toast 提示

Asuka 没有提供 toast 组件，因为 iOS 和 Android 都没有一个各应用通用的标准 toast。你可以自己做一个：把一个视图放在页面上方，用 `if` 控制它是否显示，再配一个[过渡](motion.md#进入与退出)让它进场。gallery 示例里的 toast 就是这么做的，它还调用了 `accessibility.announce`，让屏幕阅读器也把内容读出来。见 [`examples/gallery`](../../examples/gallery)。

## 路由

有了 `Router`，每个页面都有一个地址，也就是一个 href，比如 `/inbox/18` 或 `/search?q=go`，就像网页一样。应用只需以元素的形式把路由声明一次：路径模式映射到页面，页面放在显示它们的导航器之下。之后既可以按地址导航，也可以按页面导航：

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

地址带来的这些能力都是现成的：Web 的地址栏、后退和前进会显示它，刷新或粘贴链接会打开它，应用 scheme 或域名的链接会前往它，桌面窗口会在上次离开的地方打开。[`examples/navigation`](../../examples/navigation) 就用了路由。

### 模式与参数

路由的路径相对于父路由的路径，除非它以 `/` 开头。路径的各段逐一匹配：

- `chat` 匹配它自己；
- `:id` 是一个参数，匹配任意一段；
- `*path` 是兜底参数，匹配路径的剩余部分，零段或多段（只能放在模式末尾）：比如 `docs/a.md`；
- `""` 是父路由自己的路径，也就是它的索引页。

多个路由都能匹配时，最具体的那个胜出，和 expo-router 一样：逐段比较，静态段优先于参数，参数优先于兜底参数。`/files/new` 匹配 `/files/new`；`/files/:id` 匹配 `/files/3`；`/files/*path` 匹配其余的。

路由的页面有两种写法：在它的代码块里构建，`p => View`，拿到它的 `Params`（`p.get("id")`：路径里的参数，没有就取查询参数里的；还有 `p.path`、`p.query` 和 `p.href`）；或者是用模式声明的页面，`screen Thread(id: int) at ":id"`，在代码块里单独写出它的名字。这样的路由页面，参数来自地址：先取路径里的，再取查询参数里的（`/search?q=go` 的 `q`），并转换成参数的类型（`int`、`float`、`bool`、枚举的 case、`string`）。地址里没有的参数取默认值；没有默认值，或者值无法转换时，页面会向最近的 `Boundary` 抛出 `RouteError`。它的标题就是页面自己的标题。

### 导航器与弹出面板

包含子路由的 `Route` 按它的 `layout` 成为这些子路由的导航器，或者是一个分组：

- `.stack`：一个 `Stack`。它的子路由是它的页面，以及弹出面板。
- `.tabs`：`Tabs`。每个子路由是一个标签页（`title`、`icon`、`badge`），可以是一个页面，也可以是它自己的导航栈。
- `.split`：一个 `Split`。每个子路由是侧边栏里的一个目的地，可以是一个页面，也可以是一个导航栈（详情区的）。
- `.slot`：一次只显示一个子路由，铺满它所在的位置，没有导航栏：比如应用的未登录和已登录两部分。
- `.group`（默认）：它的子路由在它的路径和守卫之下，并入父路由的导航器。

设置了 `sheet: true` 的路由会在它的导航器（栈顶的页面、标签页）上方以弹出面板呈现，而不是被推入或选中；它的 `detents`、`grabber` 和 `fullScreen` 就是弹出面板的选项。返回会关闭它；导航到它下面那个导航器里的页面也会关闭它。

### 导航

- `navigate(href)` 前往这个页面：途经的标签页、目的地和 slot 都会切换过去；在它所在的栈里，如果前面已有相同地址的页面，就退回到那个页面，否则推入新页面；弹出面板的页面则会被呈现出来。`navigate(Thread(id: 7))` 前往一个路由页面的地址。
- `replace(href)` 用这个页面替换当前页面（弹出面板的位置也一样），当前页面随之销毁；历史记录不会增加。
- `pop()` 在最内层的栈里返回，`dismiss()` 关闭最新的弹出面板。
- `router.back()` 像用户的返回一样后退（不询问 `onBack`）：先是最新的弹出面板，否则是屏幕上最内层的栈；`router.canGoBack()` 表示能否后退。`router.dismiss()` 关闭路由器最新的弹出面板。
- `router.href()` 是当前显示的地址，读取它的绑定会跟随它变化（比如侧边栏的选中项、链接的状态）；`router.setQuery("q=go")` 修改当前页面的查询参数，页面保持不变。

带 `href` 的 `Button` 或可按压视图被按下时会前往那里（在它的 `on press` 之后）。在路由器里，富文本中指向应用自己 href 的链接（`Span(text: "the inbox", link: "/inbox")`）也会前往那里。它们作用于代码外面的路由器，没有的话就是主窗口的路由器。

在有路由的栈里 `push` 依然可用：它推入一个没有路由的页面。这个页面的地址是它所在栈的地址，也有自己的一条历史记录（浏览器的后退会弹出它），但刷新或链接无法再把它带回来。

和其他地方一样，每个页面都是一个 owner：被路由弹出、替换或离开的页面（切换到别的标签页的页面不算）都会被销毁。

### 链接下方的页面

链接会构建它所代表的那些页面，这样从它返回时能回到合理的地方：途经的每个栈里，先是栈的 `initial` 页面（除非指定了别的，否则就是它的第一个子路由，相当于 expo-router 的 `initialRouteName`），然后是沿链接路径往上的那些页面（`/inbox/:id/reply` 下面是 `/inbox/:id`），最后是链接指向的页面；其他标签页停在各自的第一个页面。这些页面的参数按名字从链接的参数里取；需要链接里没有的参数的页面，或者被守卫拦在外面的页面，都不会被创建。

### 重定向、守卫与未找到

设置了 `redirect` 的路由会把来到它路径的用户送到另一个 href，并填入它的参数，替换掉当前的地址：`Route("/", redirect: "/inbox")`、`Route("/m/:id", redirect: "/inbox/:id")`。

`guard` 会在它的页面（以及子路由的页面）显示之前被询问，参数是要去的 href：返回 `""` 表示放行，否则返回要把用户送去的地方。守卫可以读取状态：它读取的状态变化时，会重新询问当前显示的页面，不再被放行的页面会离开（连同栈里压在它们上面的页面），路由器转到守卫指定的地方。未登录的用户会被送到登录页，并带上原本要去的地方，登录后再回到那里：

```asuka
import std/url

state signedIn = false

app {
  Router {
    Route("/login", guard: to => if signedIn { "/" } else { "" }) { _ => Login() }
    // 一个分组：其余的一切。
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

没有任何路由能处理的 href 会显示一个简单的页面，上面有回到首页的入口。应用自己的兜底路由（`Route("/*path") { p => … }`）可以接管这些地址。

### 从哪里开始，保存在哪里

路由器从打开应用的那个链接开始（如果它能处理的话）；否则从窗口上次离开的地方开始；再否则从它的 `initial`（`/`）开始。它的完整状态（每个栈的页面、每个标签页的页面、弹出面板）会和地址一起以 JSON 的形式保存，读回时会尽可能按现在的路由还原：

| | 保存在哪里 |
|---|---|
| iOS、Android | 应用运行期间保存在应用里；重新启动后不保留（和原生应用的导航一样） |
| macOS、Windows、Linux | 和窗口上次的位置保存在一起：主窗口总是保存，`Window` 设置了 `stateKey` 才保存，存在应用数据目录的 `asuka-routes.json` 里；下次打开时回到那里 |
| Web | 地址栏，以及每一条历史记录：后退、前进和刷新都会回到对应的页面（见 [Web](web.md#地址与历史记录)） |
| `asuka dev` | 所有平台：载入新代码后，回到重新加载前显示的页面 |

窗口里的第一个路由器使用这个窗口的地址；`Window` 里的路由器则使用那个窗口的地址。

### 类型安全的链接

调用一个路由页面就得到它的地址，并在编译时检查：`navigate(Thread(id: 7))` 和 `href: Thread(id: 7)` 都是 `/inbox/7`，来自路由 `Thread` 的那唯一一个 `Router`，页面的其他参数作为查询参数（`Thread(id: 7, sort: .oldest)` 是 `/inbox/7?sort=oldest`，会被转义）。没有任何 `Router` 路由的页面、被两个 `Router` 路由的页面，或者漏掉了路径需要的参数，都会在调用处报错。写成字符串的 href 则要由应用自己保证正确。

在测试里，`mount(…, at: "/inbox/18")` 从某个地址开始，`app.href`、`app.history()`、`app.goBack()` 和 `app.goForward()` 则像浏览器一样跟随地址。

## 深度链接

只要在 `asuka.toml` 里声明，应用就可以通过自己 scheme 的 URL 打开，也可以通过指向自己域名的 https 链接打开：

```toml
name = "Notes"
id = "com.example.notes"
scheme = "notes"
links = ["notes.example.com"]
```

- `scheme` 让 `notes://anything` 这样的链接可以打开应用，支持的平台有：iOS 和 Android；Mac（由应用的 `Info.plist` 声明）；Linux，前提是应用已经安装（由它的 desktop 文件处理这个 scheme；应用运行时打开的链接会交给正在运行的应用，因为 GNOME 应用都是单实例的）；以及 Windows（应用启动时会在 `HKEY_CURRENT_USER\Software\Classes` 下为当前用户注册，应用被移动过的话会重新注册；应用运行时打开的链接会交给它）。在 Linux 和 Windows 上，设置了 `id` 的应用只会运行一个实例：再次启动时，已有的实例会被切到前台。`asuka dev` 在 Linux 和 Windows 上不会注册 scheme，它运行的应用也不会把自己交给已安装的那份应用，所以要测试链接，请使用构建出来的版本。
- `links` 让 `https://notes.example.com/...` 这样的链接可以打开应用：在 iOS 上是通用链接（universal links），在 Android 上是 App Links。域名那边也要声明这一点：按照各平台文档的说明，为 iOS 提供 `apple-app-site-association`，为 Android 提供 `assetlinks.json`。桌面端不会因为这类链接打开应用。

有了[路由](#路由)，这些链接不需要写任何代码：应用 scheme 的链接（`notes://inbox/18`，和 expo-router 一样，主机部分算作第一段，也就是 `/inbox/18`）、通用链接和 App Links 的路径、通知携带的 URL，都会在启动时或运行中直接前往对应的路由。`app` 块的 `on openURL(url)` 收到的是没有路由能处理的那些 URL。

没有路由器时，`on openURL(url)` 会收到打开应用的每一个 URL，无论是在启动时还是在运行中（组件体里的 `onOpenURL` 也一样，在组件存在期间有效）。启动应用的那个 URL 到达时还没有任何处理函数，所以它会交给第一个注册的处理函数。你可以在处理函数里用 `push` 跳转，或者通过设置状态来切换页面。

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

[`examples/notes`](../../examples/notes) 在收到 `asukanotes://add?text=...` 时会添加一条笔记。用户点击通知时，通知携带的 URL 也会以同样的方式送达，所有平台都一样（见[设备](device.md)）。在 Web 上，路由器的地址就是页面的地址：指向应用站点的链接会打开对应的页面，打开页面本身不会运行 `on openURL`。

## 接收其他应用的分享

在 `asuka.toml` 里设置 `shareTarget = true` 后，应用就会出现在其他应用的分享面板里，可以接收文本和链接，比如浏览器或 YouTube 里的“分享”；改成 `[shareTarget]` 表并设置 `types = ["image/*", ...]` 还能接收文件（见[设备](device.md#接收其他应用的分享onshare)）。`app` 块的 `on share(items)` 会收到分享过来的内容。和 `on openURL` 一样，启动应用的那次分享会交给第一个处理函数。

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

在 iOS 上，应用会自带一个分享扩展（share extension），它会带着分享的内容打开应用，和 Android 的做法一样：扩展本身不显示任何界面。分享的内容常常是一个标题加一个链接（比如从 Safari 分享时，页面标题作为文本，URL 作为链接）。[`examples/downloader`](../../examples/downloader) 会从中找出那个链接。桌面端和 Web 不能作为分享目标，在这些平台上 `on share` 永远不会运行。

如果想在发起分享的应用上方显示你自己应用的面板，而不打开你的应用，请参阅[设备](device.md#应用自己的分享面板platformsharesheet)里的 `platform/sharesheet`。反过来，要从你的应用往外分享，`device.share(text)` 会弹出系统的分享面板，`device.shareFiles(paths)` 则用来分享文件：在 Mac 上是分享菜单，在 Windows 上是系统的分享界面，在 Web 上是浏览器的分享界面。Linux 没有分享面板，这两个函数会把要分享的内容复制到剪贴板，并用一个 toast 告诉用户。
