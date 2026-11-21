# 布局与样式

元素的样式属性决定它如何布局、长什么样。布局采用 CSS flexbox 和 grid，单位是点（point）；外观则包括颜色、圆角、边框、阴影和文字。本页会介绍这两部分，以及会变化的样式。布局在所有平台上都一样：Asuka 算好布局，再把平台的视图摆到对应的位置上。

## 平台默认值

没有设置的样式会沿用平台的默认值。一个完全不设样式的应用，看起来就和这个平台自带的应用一样，字体、颜色、深色模式和文字大小都与之一致：

- **iOS**：UIKit 的风格。
- **Android**：Material 3 的风格。
- **macOS**：AppKit 的风格。
- **Windows**：Windows 11 的风格（WinUI 的 Fluent 外观，由 Asuka 在 Win32 控件上绘制）。
- **Linux**：GNOME 的风格（GTK 4 和 libadwaita）。
- **Web**：浏览器自带的控件，外围配上 Asuka 自己的一套浅色和深色配色（见 [Web](web.md)）。

只设置你真正想改的部分；颜色和字体尽量用下面介绍的语义颜色和语义字体，而不是写死的值，这样应用才能一直跟随平台。

## 样式

每个元素都在自己的块里接收样式属性，写在它自身的属性之后：

```asuka
Text(title) { font: .headline; color: .secondaryLabel }
Button("Follow") { shrink: 0 }
View { height: 1; background: .separator }
```

`style` 声明一组可以复用的属性，`style:` 应用一个样式或一个样式列表。两个条目设置了同一个属性时，后面的优先；块里自己写的属性又优先于整个列表：

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

样式可以像函数一样接收参数，用自己的 `style:` 应用别的样式；列表里的 `if` 或 `match` 在条件成立时应用某个样式，条件不再成立时把每个属性恢复原样。以元素命名的样式（`style Button { radius: 8 }`）是本包里每个该类元素的默认样式。完整的语法，以及应用中某一部分可以单独修改的主题，见[样式](language/styles.md)。

视觉属性只在原生控件支持的地方生效：按钮会把 `color` 当作着色（tint），而视图会忽略 `font`。

## 排列视图

`Column` 把子视图从上到下排列，`Row` 则从行首排到行尾。`View` 是一个容器，排列方向由它的 `direction` 决定：默认是 `.column`，也可以设为 `.row`。

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

只设置了布局属性的容器不会生成原生视图：它的子视图会被直接放进最近的那个有原生视图的祖先里。因此多嵌套几层也没有任何开销。

## Flexbox

布局遵循 CSS flexbox，默认值与 React Native 相同：默认按列排列，尺寸包含内边距和边框。

- **`gap`**：子视图之间的间距；换行时也是行与行之间的间距。
- **`justify`** 决定子视图在主轴上的分布：`.start`（默认）、`.center`、`.end`、`.spaceBetween`、`.spaceAround`、`.spaceEvenly`。
- **`align`** 决定子视图在交叉轴上的对齐方式：`.stretch`（默认，子视图撑满交叉轴）、`.start`、`.center`、`.end`、`.baseline`。
- **`alignSelf`** 为单个子视图覆盖父视图的 `align`；默认值 `.auto` 表示沿用父视图的设置。
- **`wrap`** 允许子视图在放不下时折到下一行。
- **`grow`** 表示子视图在父视图主轴上分得的剩余空间份额。会伸展的子视图同样可以缩到比内容更小，所以把 `Scroll` 包在一个会伸展的视图里时，内容会正常滚动，而不会把这个视图撑到滚动内容的完整高度。
- **`shrink`** 表示一行放不下时子视图让出多少空间。默认是 0；设为 1 时，文字会换行，控件则保持其最小尺寸。
- **`basis`** 是子视图在主轴上的初始尺寸；默认值 `.auto` 表示取内容的尺寸。

```asuka
// 两个盒子分摊宽度，其中一个是另一个的两倍。
Row {
  gap: 8; height: 60
  View { grow: 2; background: .secondaryBackground }
  View { grow: 1; background: .secondaryBackground }
}

// 标签和数值分居两端。
Row {
  justify: .spaceBetween; align: .center
  Text("Total")
  Text("$42") { font: .title2 }
}

// 放不下时会折行的标签。
Row {
  wrap: true; gap: 8
  Text("Asuka"); Text("iOS"); Text("Android")
}
```

## 尺寸

长度的类型是 `Length`：

- 数字：以点为单位。对应 iOS 和 macOS 的点、Android 的 dp、Windows 上 100% 缩放时的一个像素（1/96 英寸）、Linux 上 GTK 的逻辑像素，以及 Web 上的 CSS 像素；
- 百分比，如 `50%`：父视图在同一方向上尺寸的百分比；
- `.auto`（默认）：取内容的尺寸，或由 flexbox 分配。

`width` 和 `height` 设置尺寸；`minWidth`、`minHeight`、`maxWidth` 和 `maxHeight` 限定尺寸的范围。`aspectRatio` 是宽高比（宽除以高）：已知一边时，它会算出另一边。

```asuka
View {
  width: 50%; minWidth: 120; maxWidth: 320
  aspectRatio: 16.0 / 9
  background: .gray
}
```

文字和控件会用平台自己的测量方式算出自身尺寸，只有想改变它们的大小时才需要指定尺寸。

一个点对应多少像素取决于屏幕的缩放比例：手机上是 2 或 3，Retina 屏的 Mac 上是 2，Windows 和 Linux 上取显示器的缩放设置（150% 时是 1.5），Web 上则是浏览器的设备像素比。具体是多少可以看环境的 `scale`（`environment().scale`）。把桌面窗口拖到缩放比例不同的显示器上时，窗口会按新显示器的比例绘制，而以点为单位的布局保持不变。

## 内边距与外边距

`padding` 和 `margin` 的类型是 `Edges`，可以分别设置每一边，单位是点：

- `16`：四边都是 16；
- `[8, 16]`：上下为 8，左右为 16；
- `[4, 8, 4, 8]`：依次是上、右、下、左；
- `Edges(top: 24)`：按名字逐边指定。

```asuka
Column {
  padding: [8, 16]; margin: Edges(top: 24)
  Text("Padded")
}
```

在 iOS、Android 和 Web 上，`Button` 的 `padding` 会像 CSS 那样取代平台自带的内边距：`minHeight: 48; padding: [10, 20]` 在这些平台上高 48 点，和设置了同样属性的 `View` 一样。没有设置 `padding` 的按钮保留平台自带的内边距，只设置了边框也是如此。在桌面平台上，按钮的内边距加在原生按钮外框的外面。

在从右到左书写的语言里，`Row` 从右侧开始排列，`justify` 和 `align` 也随之从右侧算起，文字的行首在右边。`left` 和 `right` 则和 CSS 一样，说左就是左、说右就是右。如果想把间距放在文字开始或结束的那一侧，请用 `start` 或 `end`：

```asuka
View { padding: Edges(start: 16, end: 8) } // 英语里左边是 16，阿拉伯语里则是右边
```

参见[多语言](i18n.md)。

## 绝对定位

`absolute: true` 让视图脱离文档流，再用 `top`、`right`、`bottom` 和 `left` 指定它在父视图中的位置。下面的例子在头像的角上放了一个徽标：

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

排在后面的子视图会画在前面的子视图之上。

## Grid

设置了 `grid` 的容器会按 CSS grid 排列子视图。网格轨道的类型是 `Track`：`.fr(1)`、`.points(80)`、`.percent(25)`、`.auto`、`.minContent`、`.maxContent`、`.minmax(a, b)`、`.repeat(n, [...])`、`.autoFill([...])`、`.autoFit([...])`。子视图通过 `gridColumn` 和 `gridRow` 跨越多条轨道（`.line(2)`、`.span(3)`、`.between(1, 3)`）。

```asuka
View {
  gap: 8
  grid: Grid(columns: [.repeat(3, [.fr(1)])])
  Avatar(); Avatar(); Avatar(); Avatar()
  View { gridColumn: .span(3); height: 20; background: .gray }
}
```

如果是内容很多、需要滚动的网格，请使用带 `columns` 的 `List`（见[组件](components.md)）。

## 安全区域、键盘与窗口

在手机上，根视图会铺满整个屏幕，延伸到状态栏、刘海和主屏幕指示条下面。`SafeArea` 是一个 `Column`，它在自身内边距的基础上，再加上安全区域的边距。键盘弹出时，它的底部内边距就是键盘的高度，所以放在底部的内容（比如文本框、发送按钮）始终会留在键盘上方。

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

这些边距取自它所在的那个页面：在导航栈的页面、标签页或弹出面板（sheet）里，就是那个页面的边距。如果要自己摆放内容，可以通过 `environment()` 拿到这些边距和键盘高度（见[核心概念](concepts.md#环境)）。

在 macOS、Windows 和 Linux 上，根视图铺满窗口的内容区域，位于标题栏（Linux 上是 header bar）下方。桌面窗口没有安全区域，也没有会遮住窗口的屏幕键盘：边距和键盘高度都是 0，`SafeArea` 就是一个只带自身内边距的 `Column`。不过还是建议照样使用它，这样同一个页面也能直接适配手机。

除非应用另行指定，窗口大小是 800×600 点（见[导航](navigation.md)）；手机上没有窗口。用户调整窗口大小时，环境里的 `width` 和 `height` 会随之变化，布局也会重新计算，你不需要做任何处理。如果想在特定尺寸下改变布局（比如空间足够时才显示侧边栏），可以像在平板上那样，根据环境的 `widthClass` 分支：

```asuka
if environment().widthClass == .expanded {
  Row { grow: 1; Sidebar(); Detail() }
} else {
  ItemList()
}
```

在浏览器中，铺满整个页面的应用会从视觉视口（visual viewport）获取设备的安全区域（`env(safe-area-inset-*)`，适用于带刘海的手机）和屏幕键盘的高度。嵌在页面某个局部的应用则两者都没有。

## 滚动

`Scroll` 让子视图可以滚动，默认是纵向，设置 `horizontal: true` 后改为横向。记得给它一个尺寸或者 `grow: 1`，让它有一个可以在其中滚动的区域。它的尺寸属性作用于滚动视图本身的盒子；它的 `padding`、`gap`、`justify`、`align` 和 `wrap` 则用来排列其中的内容。

```asuka
Scroll {
  grow: 1; padding: 16; gap: 12
  Text("Long content")
}
```

下拉刷新和滚动位置见[组件](components.md)，长列表见[列表](components.md#列表)。

## 颜色

`Color` 有以下几种：

- **平台默认色**：不设置该属性。
- **语义颜色**：由平台解析，因此会跟随深色模式变化：
  - 文字：`.label`、`.secondaryLabel`、`.tertiaryLabel`；
  - 背景：`.background`、`.secondaryBackground`、`.groupedBackground`，以及用于其上分组列表行的 `.secondaryGroupedBackground`（对应 iOS 的 inset grouped 单元格）；
  - `.separator`、`.accent`（应用的强调色）、`.onAccent`（在强调色上清晰可读的颜色，比如以强调色填充的按钮上的图标）、`.clear`（透明）；
  - 系统色调：`.red`、`.orange`、`.yellow`、`.green`、`.blue`、`.purple`、`.pink`、`.gray`。
- **固定颜色**：`#7c3aed`，或者带透明度的 `#7c3aed80`。

各平台的语义颜色来源：

| 平台 | 语义颜色 |
| --- | --- |
| iOS | UIKit 的颜色（`labelColor`、`systemBackground`、`systemRed` 等） |
| Android | Material 3 的颜色角色（color roles） |
| macOS | AppKit 的颜色（`labelColor`、`windowBackgroundColor`、`controlBackgroundColor`、`separatorColor`、`systemRedColor` 等） |
| Windows | Windows 11 的主题色（WinUI 的 Fluent 主题资源），分浅色和深色 |
| Linux | libadwaita 的具名颜色（`window_fg_color`、`window_bg_color`、`card_bg_color`、`destructive_color` 等），取主题中的定义，高对比度主题也包括在内 |
| Web | Asuka 自己的配色，以 CSS 变量的形式提供，分浅色和深色 |

在桌面端，分组背景就是普通背景：`.groupedBackground` 是窗口背景，`.secondaryGroupedBackground` 和 `.secondaryBackground` 一样，是卡片的背景。

```asuka
Column {
  padding: 16; gap: 4; background: .background; radius: 12
  Text("Title") { font: .headline }
  Text("Details") { font: .subheadline; color: .secondaryLabel }
}
```

`c.opacity(a)` 会以 0 到 1 之间的不透明度绘制颜色（相当于 SwiftUI 的 `.opacity`、Compose 的 `copy(alpha =)`）。语义颜色调整不透明度之后仍然是语义颜色，所以 `opacity(0.15)` 的强调色在浅色和深色模式下都是强调色的一层淡色，适合用作标签的背景、图表的填充或滑轨。

```asuka
let tint: Color = .accent
View { padding: [4, 10]; radius: 12; background: tint.opacity(0.15); Text("New") }
```

`color` 是文字的颜色，或者控件的着色。

## 背景、边框与阴影

- **`background`**：视图的背景色。
- **`radius`**：圆角半径，单位是点。
- **`border`** 和 **`borderColor`**：边框，画在盒子内侧。它会像内边距一样把内容往里推。
- **`shadow`**：投影。投影由视图的形状（也就是它的边界，再按 `radius` 取圆角）产生，与视图里画了什么无关：`Shadow(color:, radius:, x:, y:)`，其中 `radius` 是模糊半径（与 CSS 和 SwiftUI 的含义相同，即模糊 σ 的两倍），`x` 和 `y` 是偏移量。裁剪子视图的视图（设置了 `clip`）照样会投下阴影，就像一张裁剪了图片的圆角卡片那样。Android 上画的是它自己的高度（elevation）阴影：半径对应高度，颜色为阴影着色，偏移量由光源决定。macOS、Windows、Linux 和 Web 则与 iOS 的画法一致，模糊和偏移效果都相同。
- **`gradient`**：覆盖在背景上的线性渐变，`Gradient(stops: [...])`。默认从上往下渐变，也可以用 `startX`、`startY`、`endX` 和 `endY` 另行指定，以视图的尺寸为单位：(0, 0) 是左上角，(1, 1) 是右下角。每个 `Stop` 是一个颜色，以及它在 0 到 1 之间的位置。Android 会沿八个方向中最接近的那个绘制渐变；其他平台则严格沿你给出的线条绘制。
- **`material`**：一种半透明、会模糊其后内容的背景，即 iOS 的材质（material）：`.ultraThin`、`.thin`、`.regular`、`.thick`、`.bar`。
  - iOS：`UIVisualEffectView` 的模糊效果。
  - macOS：在视图内容后面放一个 `NSVisualEffectView`，模糊窗口中位于其后的内容。
  - Windows：由 Asuka 绘制的 Fluent 亚克力效果：把视图背后的内容模糊，再叠上浅色或深色的色调。
  - Web：CSS 的 `backdrop-filter`，即模糊加饱和度。
  - Android 和 Linux 无法模糊视图背后的内容，因此显示半透明的表面色（Linux 上是 libadwaita 的卡片颜色）。
- **`clip`**：把子视图裁剪到视图的边界和圆角之内。

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

// 从左到右。
View {
  height: 80
  gradient: Gradient(stops: [Stop(color: .blue, at: 0), Stop(color: .purple, at: 1)], startY: 0.5, endX: 1, endY: 0.5)
}
```

## 不透明度与隐藏

`opacity` 的取值范围是 0（透明）到 1（不透明，默认值）。要隐藏视图，请设置 `hidden`。隐藏的视图仍然在布局中占据空间；如果想把它连同其中的视图一起从布局中移除，请把它放进 `if` 里（见[核心概念](concepts.md#条件显示与列表渲染)）。

## 变换

`transform` 在绘制时对视图进行平移、缩放和旋转：`Transform(x:, y:, scale:, scaleX:, scaleY:, rotation:)`（旋转以角度为单位，顺时针）；布局感知不到这些变换。所有平台都支持。变换、`transition` 和 `shared` 主要用于动画，见[动效](motion.md)。

## 文字

`font` 设置文字的字体，类型是 `Font`：

- **语义字体**：平台的文本样式，会随用户设置的文字大小缩放：`.largeTitle`、`.title`、`.title2`、`.title3`、`.headline`、`.subheadline`、`.body`、`.callout`、`.footnote`、`.caption`、`.caption2`。
- **`Font(size: 20)`**：指定字号（单位是点）的系统字体。
- **字重**：`.bold()`、`.semibold()`、`.medium()`，或者用 `weighted(w)` 指定以下任意一种：`.ultralight`、`.thin`、`.light`、`.regular`、`.medium`、`.semibold`、`.bold`、`.heavy`、`.black`。

`color` 是文字颜色，`textAlign` 是对齐方式（`.start`、`.center`、`.end`、`.justify`），`lines` 是最多显示的行数，默认值 0 表示不限制。文字默认会自动换行。

```asuka
Text("Brand") { color: #7c3aed; font: .title.bold() }
Text("A long paragraph that stops after two lines.") { font: .body; lines: 2; textAlign: .center }
Text("Fixed size") { font: Font(size: 20).medium() }
```

如果一段文字里要混用多种样式、链接或内嵌视图，请看 [RichText](components.md)。

这些文本样式沿用了 iOS 的命名，各平台会把它们映射到自己的字号体系上，所以 `.body` 就是该平台应用正文所用的大小：

| 平台 | 文本样式 | `.body` |
| --- | --- | --- |
| iOS | UIKit 的文本样式，字体为 San Francisco | 17 |
| Android | Material 3 的字体比例（type scale，`.body` 对应 Body Large），字体为 Roboto | 16 |
| macOS | AppKit 的文本样式（`NSFont.preferredFont(forTextStyle:)`），字体为 San Francisco | 13 |
| Windows | Windows 11 的字号体系（type ramp），字体为 Segoe UI Variable（Windows 11 之前为 Segoe UI） | 14 |
| Linux | libadwaita 的字体类（`.title-1`、`.heading`、`.caption` 等），相对于桌面的界面字体 | 界面字体的字号 |
| Web | Asuka 自己的字号体系，使用系统字体（`system-ui`） | 16 |

在 Windows 上，每个样式取字号体系中最接近的一档：`.largeTitle` 对应 Title Large（40），`.title` 对应 Title（28），`.title2` 对应 Subtitle（20），`.headline` 对应 Body Strong（14，semibold），`.footnote` 和 `.caption` 对应 Caption（12）。在 Linux 上，以 GNOME 的 11 pt 界面字体为例，`.largeTitle` 对应 libadwaita 的 `.title-1`（181%，heavy），`.title` 对应 `.title-2`，`.headline` 对应 `.heading`（bold），`.caption` 对应 `.caption`（82%）。

哪些字体会随用户设置的文字大小缩放：

- **iOS**（动态字体，Dynamic Type）和 **Android**（字体缩放）：所有字体，包括语义字体、固定字号字体和自定义字体。
- **Windows**：所有字体，取决于“设置 > 辅助功能 > 文本大小”。
- **Linux**：所有字体，取决于桌面的文字缩放（GNOME 的“大号文本”、GTK 的字体 DPI）。各样式所参照的界面字体同样来自桌面设置（GTK 的 `gtk-font-name`）。
- **Web**：所有字体，取决于浏览器的文字大小，因为字体都以 `rem` 为单位。
- **macOS** 没有文字大小的设置，字体保持不变。

环境的 `fontScale` 就是这个缩放值，默认为 1（见[无障碍](accessibility.md)）。

## 自定义字体

把字体文件（`.ttf`、`.otf`）放进应用的 `assets/fonts` 目录。通过字体的 `family`，即 `Font(family: "Inter")` 或 `.body.family("Inter")`，来选择要用的字体：

- 文件名：`assets/fonts/Inter-Bold.ttf` 对应 `"Inter-Bold"`；
- 一组文件构成的字体族：`"Inter"` 会根据字重选用 `Inter-Regular`、`Inter-Bold` 等文件；
- 通用字体族：`"serif"`、`"monospace"`、`"rounded"`。在 iOS 和 macOS 上，它们是系统字体的几种设计（New York、SF Mono、SF Rounded）；在 Windows 上是 Georgia 和 Cascadia Mono；在 Linux 和 Android 上是系统的衬线和等宽字体族；在 Web 上是 `ui-serif`、`ui-monospace` 和 `ui-rounded`（前提是浏览器支持）。Android、Windows 和 Linux 没有圆体，会显示系统字体；
- 系统自带的字体族，例如 `"Georgia"`。

自定义字体会和它所基于的文本样式一样缩放。平台上没有的字体会用系统字体显示。所有平台都只为当前应用加载 `assets/fonts` 里的文件，不会把它们安装给其他应用使用。

```asuka
Text("Inter Bold") { font: .title.family("Inter-Bold") }
Text("Inter") { font: .body.family("Inter").semibold() }
Text("Code") { font: .footnote.family("monospace") }
```

## 深色模式

语义颜色会自动随深色模式切换，使用它们的应用不需要为此写任何代码。固定颜色则保持不变；如果想为不同模式分别指定颜色，可以读取环境中的 `dark`：

```asuka
View {
  height: 80; radius: 12
  background: if environment().dark { #1c1c1e } else { #ffffff }
}
```

## 会变化的样式

读取了状态的属性就是一个绑定：那个状态变化时，它会重新运行，并只把这一个属性发送给视图。变化的如果是布局属性，会重新布局；如果是视觉属性，则只更新这个视图。

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

把这类改动放进 `animate { }` 里，它就会以动画呈现（见[动效](motion.md)）。

## 强调色

`.accent` 以及用到它的控件，都从 `asuka.toml` 读取应用的强调色：

```toml
accent = "#7c3aed"
```

在 iOS 上它是着色（tint）；在 Android 上它是 Material 3 配色方案的种子色（Android 12 及以上）。在 `[android]` 中设置 `dynamicColor = true` 后，只要系统支持，Android 就会像其他 Material 应用一样，从用户的壁纸中取色。在 macOS、Windows 和 Linux 上，它会在 Asuka 的控件和颜色中替换系统的强调色；在 Web 上，它同时也是控件的 `accent-color`。

如果没有设置强调色，就使用平台自己的。在 macOS 和 Windows 上，这是用户自己选的强调色（macOS 的“系统设置 > 外观”；Windows 的“设置 > 个性化 > 颜色”）；在 Linux 上是 libadwaita 的蓝色；在 Web 上是 Asuka 自带的一种蓝色。参见[发布](shipping.md)。

在白色背景上清晰的颜色，放到深色背景上可能会显得太暗。`accentDark` 用来指定深色模式下的强调色：

```toml
accent = "#2563eb"
accentDark = "#93c5fd"
```

在 iOS、macOS、Windows、Linux 和 Web 上，只要应用以深色显示，无论是跟随系统，还是自己强制使用深色（见下文），`.accent` 和控件都会改用它；回到浅色时又用回 `accent`。不设置它时，两种模式都使用 `accent`。Android 会把 `accent` 本身作为配色方案的种子色，由它推导出深色色调，所以不使用 `accentDark`。

## 浅色与深色

应用会跟随系统的外观：iOS 和 macOS 的深色模式、Android 的深色主题、Windows 的应用模式（“设置 > 个性化 > 颜色”）、GNOME 的深色样式，以及浏览器的 `prefers-color-scheme`。应用也可以无视系统设置，强制以浅色或深色显示，就像应用内置的外观设置那样（对应 SwiftUI 的 `preferredColorScheme`；Android 上 AppCompat 的夜间模式；macOS 上应用的 `NSAppearance`；Linux 上 libadwaita 的 style manager）：见[应用](navigation.md)。语义颜色、平台控件和系统栏都会随之切换，无需重建应用；当前是哪种模式，可以看环境的 `dark`。用 [`std/store`](../stdlib.md) 保存用户的选择。
