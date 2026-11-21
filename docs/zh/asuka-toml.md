# asuka.toml

`asuka.toml` 放在应用目录下，用来告诉各个平台和应用商店它们需要知道的应用信息：名称和 ID、图标、应用能打开的链接和文件、签名用的密钥等。`asuka dev`、`asuka build` 和 `asuka publish` 都会读取它；其中 `asuka dev` 只在启动时读取一次，所以改动之后要停掉它再重新运行。

它是一个 [TOML](https://toml.io)（1.0）文件：每行一个 `键 = 值`，`[ios]` 这样的表头开始一张表，`#` 之后是注释。所有键都是可选的，没有这个文件的应用也能用默认值构建和运行。Asuka 不认识的键会被直接忽略，所以如果某个键看起来没有效果，先检查一下拼写。语法错误或类型不对时，Asuka 会指出出错的位置（`asuka.toml:3:9: build: want an integer, not a string`）；Asuka 无法接受的值会让构建停下来，并指出是哪个值、应该怎么写（`asuka.toml: accent "red" (want #rrggbb or #rrggbbaa)`）。路径都相对于应用目录。

本页列出了所有的键。发布时这些键如何配合，参见[打包与发布](shipping.md#asukatoml)；下文链接的各篇指南会进一步介绍对应键的作用。

```toml
name = "Notes"
id = "com.example.notes"
version = "1.2.0"
build = 7
icon = "icon.png"
accent = "#7c3aed"
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

## 应用

下面这些键在所有对它有意义的平台上都生效。

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `name` | 字符串 | 目录名 | 应用的名称：显示在图标下方和 Mac 的菜单栏里，也是 Windows 上的窗口标题、Linux 启动器里显示的名字以及网页的标题。构建产物的文件名也取自它（`Notes.app`、`Notes.exe`）。 |
| `id` | 字符串 | `dev.asuka.<directory>` | 在 iOS 和 Mac 上是 bundle ID；在 Android 和 Linux 上是 application ID（Linux 的 desktop 文件、图标、D-Bus 名称和数据目录都用它）；在 Windows 上是 AppUserModelID。请在第一次发布之前改好：应用商店和系统都靠它识别应用。在 Linux 上它只能包含字母、数字、`_` 和 `-`，至少分两段，每段都不能以数字开头。 |
| `version` | 字符串 | `1.0` | 用户看到的版本号。 |
| `build` | 整数 | `1` | 构建编号，每次上传都必须递增。`asuka publish` 会自动把构建编号为商店中现有最大编号的下一个，所以这里只是起始值。Windows 上 `.exe` 的文件版本由 `version` 和 `build` 组成（`1.2.0.7`）。 |
| `icon` | 路径 | 无，使用平台的通用图标 | 一张 1024×1024 的正方形 PNG，用于 iOS 时不能有透明区域。每个平台的构建会自动生成该平台需要的各种尺寸。上架 App Store 必须提供。 |
| `accent` | 颜色 | 平台默认 | `"#rrggbb"` 或 `"#rrggbbaa"`。在 iOS 上是 tint 色；在 Android 上是 Material 3 配色方案的种子色；在 Mac 上是 `Accent`、填充按钮和链接的颜色（AppKit 自带的控件仍然使用用户选择的颜色）；在 Windows 上是控件的强调色，在 Linux 上是 libadwaita 的强调色；在 Web 上是页面的强调色以及 manifest 的 `theme_color`。 |
| `orientations` | 字符串数组 | 两者都支持 | `"portrait"`、`"landscape"`：应用在手机上支持的方向。iPad 上总是支持全部四个方向，因为分屏浏览和台前调度要求如此；Android 16 在平板和折叠屏设备上也会忽略这项设置。 |
| `scheme` | 字符串 | 无 | 应用的 URL scheme：`notes://` 开头的 URL 会打开应用，并由应用的 `on openURL(url)` 接收，支持 iOS、Android、Mac、Windows 和 Linux。只能包含字母、数字、`+`、`-` 和 `.`，并以字母开头。见[深度链接](navigation.md#深度链接)。 |
| `links` | 字符串数组 | 无 | 域名，如 `example.com`，指向这些域名的 https 链接会打开应用：即 iOS 的 universal links 和 Android 的 App Links。域名需要提供关联文件。见[深度链接](navigation.md#深度链接)。 |
| `fileTypes` | 表数组 | 无 | 应用能打开的文件类型。见 [fileTypes](#filetypes)。 |
| `shareTarget` | `true` 或表 | 关闭 | 让应用出现在其他应用的分享面板里，仅限 iOS 和 Android。见 [shareTarget](#sharetarget)。 |
| `splash` | 表 | 无 | 启动屏，仅限 iOS 和 Android。见 [splash](#splash)。 |
| `widgets` | 表数组 | 无 | 主屏幕小组件，仅限 iOS 和 Android。见 [widgets](#widgets)。 |
| `shortcuts` | 表数组 | 无 | 提供给 Siri、聚焦搜索和启动器的快捷操作，仅限 iOS 和 Android。见 [shortcuts](#shortcuts)。 |
| `nfc` | 表 | 无 | `platform/nfc` 读写标签所需的配置：iOS 的 AID 和 FeliCa 系统代码，以及在 Android 上能打开应用的标签。见 [nfc](#nfc)。 |
| `background` | 布尔值 | `false` | 在桌面端让应用常驻托盘：启动时不打开窗口，没有窗口时也继续运行；在 Mac 上从启动起就不显示程序坞（Dock）图标（`LSUIElement`）。见[后台应用](desktop.md#后台应用)。 |
| `keepRunning` | 布尔值 | `false` | 在桌面端，最后一个窗口关闭后应用继续运行：主窗口只是被隐藏，直到应用再次显示它。`background` 应用本来就会继续运行。见[后台应用](desktop.md#后台应用)。 |
| `singleInstance` | 布尔值 | `true` | 在 Windows 和 Linux 上，再次启动时，新进程会把文件交给正在运行的应用（由它的 `on openFiles(paths)` 接收），然后自行退出。设为 `false` 则每启动一次就运行一个新实例。Mac 上无论如何都只运行一个实例。见[单实例](desktop.md#单实例)。 |
| `i18n` | 表 | 存在时为 `locales` | 应用的目录文件放在哪里。见 [i18n](#i18n)。 |
| `updates` | 表 | 无 | 桌面应用的签名更新。见 [updates](#updates)。 |
| `ios` | 表 | | iOS 构建的配置，部分也用于 Mac。见 [ios](#ios)。 |
| `mac` | 表 | | Mac 构建的描述文件。见 [mac](#mac)。 |
| `icloud` | 表 | `iCloud.<id>` | `platform/icloud` 所用的 iCloud 容器和应用在 iCloud Drive 中的文件夹。见 [icloud](#icloud)。 |
| `applePay` | 表 | 无 | `merchantIds = ["merchant.com.example"]`：Apple Pay 付款所用的商户 ID（`platform/pay`），会写进导入了这个包的 iOS 或 Mac 应用的 in-app-payments entitlement。见[钱包支付](device.md#钱包支付platformpay)。 |
| `android` | 表 | | Android 构建的配置。见 [android](#android)。 |
| `linux` | 表 | | Linux 构建对应用的描述。见 [linux](#linux)。 |
| `web` | 表 | | Web 构建的配置。见 [web](#web)。 |
| `bluetooth` | 表 | | 使用 `platform/bluetooth` 的应用在包本身所需之外的额外声明：后台运行、通过扫描定位。见 [bluetooth](#bluetooth)。 |
| `bin` | 表 | 无 | 项目自带的程序：放在各自目录里的命令行工具，会构建进桌面应用。见 [bin](#bin)。 |

Windows 构建只需要上面这些键；它的签名证书通过环境变量提供（见[打包与发布](shipping.md#windows)）。

## fileTypes

应用能打开的文件类型，无论是从文件管理器、邮件附件还是“打开方式”打开的，都由应用的 `on openFiles(paths)` 接收。每一项是一张表，写成 `[[fileTypes]]`：

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

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `extensions` | 字符串数组 | 必填 | 文件的扩展名，不带点：`"md"`、`"tar.gz"`。 |
| `name` | 字符串 | 第一个扩展名的大写形式加上 "Document" | 系统显示的文件类型名称。 |
| `mimeTypes` | 字符串数组 | Asuka 已知的对应 MIME 类型 | 文件的 MIME 类型。对于 Asuka 不认识的扩展名，类型是应用自有的：`application/x-<app>-<extension>`。 |
| `uti` | 字符串 | 已知扩展名用系统的 UTI，否则为 `<id>.<extension>` | Apple 平台上的统一类型标识符（Uniform Type Identifier）。位于应用 ID 之下的是应用自有的类型：会被导出（exported），应用也会成为这类文件的所有者。其他系统可能不认识的类型则会被导入（imported）。 |
| `conformsTo` | 字符串数组 | `public.data`、`public.content` | 导出或导入的类型所遵循的 UTI。加上 `public.plain-text` 后，快速查看（Quick Look）和文本编辑器也能处理它。 |
| `role` | 字符串 | `"editor"` | `"editor"` 或 `"viewer"`：应用对这类文件做什么（Mac 和 iOS 上的 `CFBundleTypeRole`）。 |
| `icon` | 路径 | 系统或应用的图标 | 一张正方形 PNG，在 Mac、Windows 和 Linux 上用作这类文件的图标。 |

各平台以各自的方式声明它们：Mac 和 iOS 写在 Info.plist 的 `CFBundleDocumentTypes` 中，Android 声明为 intent filter，Windows 在应用启动时写入当前用户的注册表，Linux 写在 desktop 文件和 shared-mime-info 包中，Web 写在 manifest 的 `file_handlers` 中。见[打包与发布](shipping.md#asukatoml)和[打开文件](desktop.md#打开文件)。

## shareTarget

让应用出现在其他应用的分享面板里，仅限 iOS 和 Android；分享过来的内容由应用的 `on share(items)` 接收。设为 `true` 可以接收文本和网页链接；设为表则可以指定还接收什么：

```toml
[shareTarget]
types = ["text", "image/*", "video/*"]
max = 10
```

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `types` | 字符串数组 | `["text"]` | `"text"`（文本和网页链接），以及按 MIME 类型指定的文件：`"image/*"`、`"video/*"`、`"audio/*"`、`"application/pdf"`，`"*/*"` 表示任意文件。 |
| `max` | 整数 | `1` | 一次最多接收几个文件，至少为 1。 |

在 iOS 上，它是应用里的一个分享扩展，有自己的 bundle ID（应用的 ID 加上 `.share`），发布构建用 `ios.shareProfile` 为它签名。接收文件的分享扩展还需要 App Group `group.<id>`。见[打包与发布](shipping.md#ios)和[接收其他应用的分享](device.md#接收其他应用的分享onshare)。

## splash

iOS 和 Android 上的启动屏：一张图片居中显示在纯色背景上，从用户点击图标开始，一直显示到应用的第一帧（或调用 `splash.Hide`）为止。桌面端和 Web 没有启动屏。

```toml
[splash]
image = "logo.png"
size = 96
background = "#4A63B5"
darkBackground = "#1B2440"
```

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `image` | 路径 | 无 | 一张正方形 PNG，通常是 logo。 |
| `size` | 整数 | `120` | 图片的宽度，单位为 point（dp），最大 160，这是 Android 12 启动图标圆圈能容纳的上限。 |
| `background` | 颜色 | `"#ffffff"` | `"#rrggbb"`，浅色模式下的背景色。 |
| `darkBackground` | 颜色 | 同 `background` | `"#rrggbb"`，深色模式下的背景色。 |

见[启动屏](device.md#启动屏platformsplash)。

## widgets

应用的主屏幕小组件，仅限 iOS 和 Android；由应用调用 `widget.set` 填充内容。

```toml
[[widgets]]
kind = "today"
name = "Today"
description = "What's next"
sizes = ["small", "medium", "rectangular"]
```

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `kind` | 字符串 | 必填 | 小组件的名字，供 `widget.Set` 使用：只能包含字母、数字和 `_`，以字母开头，且不能重复。 |
| `name` | 字符串 | 同 `kind` | 小组件库里显示的名称。 |
| `description` | 字符串 | 无 | 小组件库里显示的描述。 |
| `sizes` | 字符串数组 | `["small", "medium"]` | `small`、`medium`、`large`，以及 iOS 的 `extraLarge`（iPad）和锁屏上的 `circular`、`rectangular`、`inline`。 |

在 iOS 上，小组件是一个扩展，有自己的 bundle ID（应用的 ID 加上 `.widgets`），发布构建用 `ios.widgetProfile` 为它签名。见[主屏幕小组件](device.md#主屏幕小组件platformwidget)。

## shortcuts

系统为应用提供的快捷操作，最多十个：在 iOS 上是 App Shortcuts（Siri、聚焦搜索、快捷指令 App 和操作按钮），在 Android 上是启动器快捷方式。每个快捷操作都会用它的 URL 打开应用，由应用的 `on openURL(url)` 接收。

```toml
[[shortcuts]]
id = "newNote"
title = "New Note"
url = "notes://new"
symbol = "square.and.pencil"
phrases = ["New note in ${app}"]
```

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `id` | 字符串 | 必填 | 只能包含字母、数字和 `_`，以字母开头，且不能重复。 |
| `title` | 字符串 | 必填 | 系统显示的标题。 |
| `url` | 字符串 | 必填 | 打开应用时使用的 URL：属于应用 `scheme` 的 URL，或者 `links` 中某个域名的链接。 |
| `symbol` | 字符串 | 无 | SF Symbol 的名称，与 `Symbol` 接受的一样。 |
| `phrases` | 字符串数组 | `"<title> in ${app}"` | Siri 能识别的说法，每句都要包含代表应用名称的 `${app}`。仅限 iOS。 |

见[快捷方式](device.md#快捷方式siri聚焦搜索与启动器)。

## nfc

除了权限之外，`platform/nfc` 还需要平台提供的一些配置。只有导入了这个包的应用才会带上它们，没导入的应用一概没有：

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

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `iso7816` | 字符串数组 | 无 | iOS 发现卡片时依次选择的 ISO 7816 应用（AID：5 到 16 字节，十六进制），写入 Info.plist 的 `com.apple.developer.nfc.readersession.iso7816.select-identifiers`。设置了 `Poll` 的会话只会发现带有其中某个应用的卡片。 |
| `felica` | 字符串数组 | 无 | iOS 轮询的 FeliCa 系统代码（4 位十六进制数，不能是 `FFFF`），写入 `com.apple.developer.nfc.readersession.felica.systemcodes`。没有它，轮询 FeliCa 的会话会失败。 |
| `ndef` | 表数组 | 无 | 在 Android 上按消息的第一条记录打开应用的标签（NDEF_DISCOVERED）。每项是 `uri`（scheme、主机和路径前缀；URI 交给应用的 `on openURL(url)`）、`mime`（MIME 类型；文本记录是 `text/plain`）或 `external`（`domain:type`）三者之一；后两种标签交给 `nfc.opened()`。 |
| `tech` | 字符串数组的数组 | 无 | 在 Android 上按技术类型打开应用的标签（TECH_DISCOVERED）：每个列表里是标签必须全部具备的 `android.nfc.tech` 类（`IsoDep`、`NfcA`、`MifareUltralight` ……）。 |
| `required` | 布尔值 | `false` | 让应用只面向带 NFC 的手机：Google Play 不会向其他设备提供它。 |

见 [NFC 标签](nfc.md#asukatoml-与权限)。

## icloud

调用了 `platform/icloud` 文档功能的应用所用的 iCloud 容器，以及它们在 iCloud Drive 中如何显示。没有调用的应用一概没有这些配置：

```toml
[icloud]
containers = ["iCloud.com.example.journal"]
driveName = "Journal"
```

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `containers` | 字符串数组 | `["iCloud.<id>"]` | 应用的 iCloud 容器，第一个是默认容器：写入 entitlement 的 `com.apple.developer.icloud-container-identifiers` 和 `ubiquity-container-identifiers`。应用的 App ID 需要开启 iCloud 能力，并加上这些容器。 |
| `driveName` | 字符串 | 应用的 `name` | 第一个容器在 iCloud Drive 中的文件夹名（Info.plist 的 `NSUbiquitousContainers`）。 |
| `driveHidden` | 布尔值 | `false` | 不在 iCloud Drive 中显示应用的文档：它们照样同步，但只有应用自己能看到。 |

见 [iCloud](icloud.md#asukatoml-与-entitlement)。

## i18n

应用的消息目录文件放在哪里。目录文件放在 `locales` 目录下的应用不需要设置它。

```toml
[i18n]
dir = "translations"
source = "ja"
```

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `dir` | 路径 | `locales` | 存放目录文件的目录，每种语言一个文件，以它的 BCP 47 标签命名：`en.json`、`pt-BR.json`、`zh-Hans.json`。 |
| `source` | 字符串 | `en`，或唯一的那个目录文件的语言 | 编写应用所用的语言：它的目录文件包含所有消息，其他语言缺少的消息都回退到它。 |

见[多语言](i18n.md#目录文件)。

## updates

桌面应用的自动更新：签名更新所用的密钥，以及更新发布在哪里。`asuka update keygen` 会生成密钥对，并打印出这张表。

```toml
[updates]
publicKey = "…the contents of asuka-update.pub…"
github = "you/notes"
```

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `publicKey` | 字符串 | 必填 | `asuka-update.pub` 的内容，会编译进应用：应用只接受用对应私钥签名的更新。 |
| `github` | 字符串 | | `"owner/name"`：一个公开仓库，其中标签为 `v1.2.0` 这类形式的 release 存放着更新。 |
| `tagPrefix` | 字符串 | `"v"` | release 标签的前缀。换成其他前缀（如 `"desktop-v"`）后，仓库的最新 release 可以属于别的项目：应用会通过 GitHub API 找到带有这个前缀的最新 release。 |
| `url` | 字符串 | | 代替 `github`：一个提供这些文件的 HTTPS 目录（S3 或 R2 存储桶、CDN）；应用读取 `<url>/update-<target>.json`。 |
| `feed` | 字符串 | 无 | 你自己的 HTTPS 服务器，应用改为向它询问更新；URL 中可以包含 `{target}`、`{version}` 和 `{channel}`。它需要配合 `url` 使用，文件放在 `url` 那里。 |
| `s3` | 表 | 无 | 为 `url` 提供文件的存储桶，`asuka publish` 会上传到这里：`bucket`（必填）、`prefix`（桶内目录，默认为根目录）、`region`（默认取 `AWS_REGION`，其次 `AWS_DEFAULT_REGION`，否则为 `us-east-1`）、`endpoint`（R2、MinIO 等的地址，默认是 Amazon S3）、`pathStyle`（默认在设置了 `endpoint` 或桶名含点时启用）。 |
| `privateKey` | 路径 | 无 | 私钥的路径，用于在你自己的机器上构建（`~/` 表示主目录）。 |
| `changelog` | 路径 | 存在时为 `CHANGELOG.md` | 一个 Markdown 文件，其中 `## 1.2.0` 一节就是发布说明。 |
| `deltas` | 整数 | `3` | 为之前的多少个版本生成增量更新（只下载有变化的部分）；`0` 表示不生成。 |

`github` 和 `url` 必须设置其中一个，且不能同时设置。见[自动更新](shipping.md#自动更新)。

## ios

iOS 构建的配置。`team` 同时也是 Mac 构建所用的团队，`usage` 里的用途说明也会写进 Mac 的 Info.plist。

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `team` | 字符串 | 无 | 为应用签名的 Apple 开发者团队 ID（`ABCDE12345`）。 |
| `profile` | 字符串 | 已安装的最佳匹配 | 应用的描述文件（provisioning profile）：路径、名称或 UUID。`--profile` 会覆盖它。 |
| `shareProfile` | 字符串 | 已安装的最佳匹配 | 分享扩展（`shareTarget`）的描述文件，对应 ID `<id>.share`。 |
| `widgetProfile` | 字符串 | 已安装的最佳匹配 | 小组件扩展（`widgets`）的描述文件，对应 ID `<id>.widgets`。 |
| `notificationProfile` | 字符串 | 已安装的最佳匹配 | Notification Service Extension（`push.Rewrite`）的描述文件，对应 ID `<id>.notification`。 |
| `usage` | 表 | 各个包自带的说明 | 受保护资源的用途说明，以 Info.plist 的键为键，写在 `[ios.usage]` 中：`NSCameraUsageDescription = "To scan your receipts."`。应用导入的包会提供通用的说明，最好写上你自己的。 |
| `infoPlist` | 表 | 无 | 添加到 Info.plist 的字符串键。 |
| `backgroundModes` | 字符串数组 | 无 | 添加到 Info.plist 的 `UIBackgroundModes`：`"audio"` 可以在锁屏时继续录音和播放。 |
| `iphoneOnly` | 布尔值 | `false` | 不支持 iPad：应用在 iPad 上作为 iPhone 应用运行，窗口是手机大小。 |
| `storekit` | 字符串 | 无 | StoreKit 配置文件（Xcode 的 `.storekit`）。`asuka dev` 的模拟器构建会像 Xcode 的 scheme 那样，用它来售卖商品：见[应用内购买](purchases.md#测试购买)。 |
| `nonExemptEncryption` | 字符串 | `"no"` | 构建对自身加密情况的声明（`ITSAppUsesNonExemptEncryption`），这是一项需要你自己做出的法律声明：`"no"`、`"yes"`，或者 `"ask"`（不做声明，App Store Connect 会在每次上传时询问）。 |
| `collected` | 表数组 | 无 | 应用收集的数据，用于隐私清单（见下文）。 |
| `appStoreConnect` | 表 | | `asuka publish` 上传时使用的 App Store Connect API 密钥：`keyId`、`issuerId`（个人密钥没有）和 `key`，即 `.p8` 文件的路径（默认按 `altool` 的方式查找 `~/.appstoreconnect/private_keys` 下的 `AuthKey_<keyId>.p8`）。见[发布到应用商店](shipping.md#发布到应用商店)。 |

`collected` 的每一项是一类数据，使用 Apple 的名称并去掉前缀，写进 Asuka 生成的隐私清单（如果应用自带 `ios/PrivacyInfo.xcprivacy`，就会用它代替生成的清单，这一项也随之不再生效）：

```toml
[[ios.collected]]
type = "EmailAddress"
linked = true
tracking = false
purposes = ["AppFunctionality"]
```

| 键 | 类型 | 说明 |
|---|---|---|
| `type` | 字符串 | `"EmailAddress"` 即 `NSPrivacyCollectedDataTypeEmailAddress`。 |
| `linked` | 布尔值 | 是否与用户身份关联。 |
| `tracking` | 布尔值 | 是否用于追踪用户。 |
| `purposes` | 字符串数组 | `"AppFunctionality"` 即 `NSPrivacyCollectedDataTypePurposeAppFunctionality`。 |

见[打包与发布](shipping.md#ios)。

## mac

Mac 构建签名所用的描述文件；团队取自 `ios.team`。

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `profile` | 字符串 | `ASUKA_MAC_PROFILE`；没有时，如果应用需要，用已安装的最佳匹配 | Developer ID（或开发用）描述文件的路径、名称或 UUID，推送、通过 Apple 登录、App Attest 和 Apple Pay 都需要它。 |
| `appStoreProfile` | 字符串 | 已安装的最佳匹配 | Mac App Store 的描述文件，用于 `--appstore`。 |

`--profile` 和 `ASUKA_MAC_PROFILE` 会覆盖它们。见[打包与发布](shipping.md#macos)。

## android

Android 构建的配置。

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `keystore` | 路径 | 无 | 上传密钥所在的 keystore，用于为发布构建签名。keystore 的密码取自 `ASUKA_KEYSTORE_PASSWORD`，密钥的密码取自 `ASUKA_KEY_PASSWORD`（默认与 keystore 相同）。 |
| `keyAlias` | 字符串 | 无 | 上传密钥在 keystore 中的别名。 |
| `dynamicColor` | 布尔值 | `false` | 像 Material 应用那样从用户的壁纸取色（Android 12 及以上）；在其他情况下以 `accent` 为种子色。 |
| `permissions` | 字符串数组 | 各个包自带的权限 | 添加到 manifest 的权限：`"android.permission.CAMERA"`。写成 `"<name>:28"` 表示只在 Android 9（API 28）及以下申请。应用导入的包会自动添加它们需要的权限。 |
| `googleServices` | 路径 | 无 | Firebase 项目的 `google-services.json`，供 `platform/push` 使用。 |
| `mapsKey` | 字符串 | 无 | Google Maps Platform 的 API 密钥（Maps SDK for Android），供 `platform/maps` 使用。 |
| `serviceAccount` | 路径 | 无 | `asuka publish` 上传到 Google Play 时使用的 Google Cloud 服务账号的 JSON 密钥。不要把它放进仓库。 |

见[打包与发布](shipping.md#android)。

## linux

Linux 构建的 desktop 文件和 AppStream metainfo 对应用的描述，也就是启动器、软件中心和 Flathub 显示的内容。Flathub 和软件中心要求提供摘要、描述、许可证和开发者。

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `summary` | 字符串 | 无 | 一句话说明应用是做什么的。 |
| `description` | 字符串 | 无 | 几段介绍，段落之间用空行分隔。 |
| `categories` | 字符串数组 | `["Utility"]` | [freedesktop.org 的主分类](https://specifications.freedesktop.org/menu-spec/latest/category-registry.html)：`Utility`、`Office`、`Development`、`Network`…… |
| `keywords` | 字符串数组 | 无 | 启动器搜索应用时还能用的其他关键词。 |
| `license` | 字符串 | 无 | 应用的许可证，写成 SPDX 表达式：`MIT`、`GPL-3.0-or-later`、`LicenseRef-proprietary`。 |
| `developer` | 字符串 | 无 | 开发者的名称。 |
| `homepage` | 字符串 | 无 | 应用的网页。 |
| `flatpakRuntime` | 字符串 | 一个较新的版本 | Flatpak manifest 所要求的 GNOME 运行时版本，46 或更高。 |

见[打包与发布](shipping.md#linux)。

## web

Web 构建的配置。

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `vapidPublicKey` | 字符串 | 无 | 应用服务器的 VAPID 公钥（base64url），浏览器用它为 `platform/push` 订阅 Web Push。 |
| `base` | 字符串 | `"/"` | 网站所在的路径（`https://example.com/app/` 就写 `"/app/"`）：页面的 `<base href>`，路由器的 href 就是它下面的页面地址。`asuka build --base` 会覆盖它。见[地址与历史记录](web.md#地址与历史记录)。 |
| `colors` | 表 | Asuka 自带 | 页面的调色板，分浅色和深色：见下表。见[调色板](web.md#调色板)。 |

见 [Web Push](web.md#platformpushweb-push)。

`[web.colors]` 设置 Web 宿主绘制时使用的颜色，每个都写成 `"#rrggbb"`
或 `"#rrggbbaa"`。每个颜色都另有一个深色模式用的键，即在名字后加
`Dark`（如 `labelDark`），页面以深色显示时（浏览器的
`prefers-color-scheme`，或 `appearance.setColorScheme(.dark)`）使用它；不写的话，两种模式用同一个颜色。没写的颜色保持 Asuka 自带的值。这些设置只对 Web 生效：iOS、Android、Mac、Windows 和 Linux 仍然使用系统的颜色。

```toml
[web.colors]
label = "#16181d"
labelDark = "#e8e9eb"
background = "#ffffff"
backgroundDark = "#101114"
separator = "#dfe1e5"
separatorDark = "#3a3d42"
```

| 键（以及 `…Dark`） | 用在哪里 |
|---|---|
| `label` | `.label`，以及没有指定颜色的文字：Text、输入框的文字、Picker 的文字 |
| `secondaryLabel` | `.secondaryLabel`：对话框的说明文字、标签栏中未选中的标签、菜单和侧边栏的分组标题 |
| `tertiaryLabel` | `.tertiaryLabel`：输入框的占位文字、禁用的栏按钮和菜单项、sheet 的拖动条 |
| `background` | `.background`：页面、屏幕、列表、sheet、带边框按钮的填充色（以及浏览器的主题色） |
| `secondaryBackground` | `.secondaryBackground`：Split 的侧边栏、窗口的标题栏 |
| `groupedBackground` | `.groupedBackground`：分组列表的背景 |
| `secondaryGroupedBackground` | `.secondaryGroupedBackground`：分组列表的行 |
| `separator` | `.separator`：列表行之间的线、各种栏的边缘、带边框按钮和 Picker 的边框、Stepper 的分隔线、菜单的分隔线 |
| `card` | 浮起的表面：对话框、弹出框和菜单、Window 的面板、分段控件中选中的那一段 |
| `bar` | 顶部栏、标签栏和工具栏的背景，以及 snackbar 的背景 |

## bluetooth

导入了 `platform/bluetooth` 的应用，在这个包自动提供的权限和授权文案之外，还可以在这里声明额外的需求。没有导入这个包的应用，这些设置不起作用。

```toml
[bluetooth]
background = ["central"]
location = false
```

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `background` | 字符串数组 | 无 | 应用在后台保持的角色，并带有 iOS 的状态恢复（`bluetooth.restored()`）。`"central"`：iOS 的 `bluetooth-central` 模式（调用了 `backgroundScan` 的应用本来就有）。`"peripheral"`：iOS 的 `bluetooth-peripheral` 模式。Android 上两者都不需要额外声明：调用 `backgroundScan` 的应用会自动得到扫描的接收器。 |
| `location` | 布尔值 | `false` | 应用会通过扫描确定用户的位置（比如信标）：Android 的 `BLUETOOTH_SCAN` 不再带 `neverForLocation`，`bluetooth.request()` 也会一并请求位置权限（iOS：`NSLocationWhenInUseUsageDescription`）。 |

见[蓝牙](bluetooth.md#构建时添加的内容)。

## c

应用的 `extern` 声明所调用的 C 代码：为每个目标编进应用的内容，以及要链接的库。每个平台的表（`[c.ios]`、`[c.android]`、`[c.macos]`、`[c.windows]`、`[c.linux]`、`[c.web]`）会在顶层 `[c]` 的各个列表后面追加内容。包的 `asuka.toml` 也可以有自己的 `[c]`，导入这个包的应用会一并构建它。

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `sources` | 字符串数组 | 无 | C 文件，相对于 `asuka.toml` 的 glob（`c/*.c`；`**` 匹配任意层目录）。如果某个 glob 一个文件也没匹配到，构建会停下来。 |
| `include` | 字符串数组 | 无 | 源文件的头文件搜索目录（`-I`）。`asuka_ffi.h` 总是可以包含。 |
| `defines` | 字符串数组 | 无 | 宏，`NAME` 或 `NAME=value`（`-D`）。 |
| `flags` | 字符串数组 | 无 | 额外的编译器参数。 |
| `libraries` | 字符串数组 | 无 | 要链接的库：一个名字（`sqlite3`：`-lsqlite3`），或者相对于 `asuka.toml` 的文件路径（`lib/libfoo.a`）。 |
| `frameworks` | 字符串数组 | 无 | Apple 的框架，在 iOS 和 macOS 上链接（`Security`）。 |

`[c.prebuilt]` 列出厂商为每个目标预编译好的库：每个键是一个目标（`ios-arm64`、`ios-simulator-arm64`、`ios-simulator-x64`、`macos-arm64`、`macos-x64`、`android-arm64`、`android-arm`、`android-x64`、`linux-x64`、`linux-arm64`、`windows-x64`、`windows-arm64`、`web`；通用库用 `ios-simulator` 和 `macos`），每个值是静态库相对于 `asuka.toml` 的路径（`.a`；Windows 上也可以是 `.lib`），或者由多个路径组成的列表。构建某个目标时，如果这里没有它的库，构建会停下来并说明原因。

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

`[c.download.<name>]` 指定一个由构建下载、而不放在项目里的压缩包（库的源码，或厂商编译好的库）：`url`（`https://`，格式为 `.zip`、`.tar.gz` 或 `.tgz`）、`sha256`（压缩包的哈希，不一致会被拒绝）和 `strip`（去掉路径开头的几层目录，默认 0）。`[c]` 里以 `$<name>` 开头的路径指向包里的内容（`$sqlite/sqlite3.c`；单独的 `$sqlite` 是顶层目录）。第一次需要它的构建会把它下载到 Asuka 的缓存里，只下载一次。

```toml
[c]
sources = ["$sqlite/sqlite3.c"]
include = ["$sqlite"]

[c.download.sqlite]
url = "https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip"
sha256 = "0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9"
strip = 1
```

见[在 Asuka 中调用 C](c.md#构建)、[下载的源码](c.md#下载的源码)和[预编译的库](c.md#预编译的库)。

## bin

项目自带的[程序](programs.md)：每个 `[bin.<name>]` 是一个命令行工具，也就是放在自己目录里、带 `fn main` 的包，`<name>` 是它的可执行文件名（字母、数字、`-`、`_` 和 `.`）。项目本身就是程序时（`asuka new --bin`），不需要这一项。

| 键 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `path` | 路径 | `path` 或 `build` | 程序所在的目录，相对于 `asuka.toml`。程序用相对路径导入项目里的包（`import ../model`）。 |
| `build` | 字符串数组 | `path` 或 `build` | 代替 `path`：由其他工具链构建的程序（应用附带的 Go 或 Rust 工具）的构建命令，针对每个目标在项目目录中运行。`{out}` 是它写出可执行文件的位置；目标的占位符见下表。 |
| `env` | 表 | 无 | `build` 的环境变量，覆盖 CLI 的；值中的占位符同样会被替换。 |
| `output` | 路径 | `{out}` | `build` 把可执行文件留在哪里，相对于 `asuka.toml`。用于自己决定输出位置的工具链（cargo 的 `target/<triple>/release`）：CLI 会从那里取。 |

`build`、`env` 和 `output` 里的目标，用各个工具链自己的名字：

| 占位符 | Mac（Apple 芯片、Intel） | Linux | Windows |
|---|---|---|---|
| `{os}` | `macos` | `linux` | `windows` |
| `{arch}` | `arm64`、`x64` | `arm64`、`x64` | `arm64`、`x64` |
| `{exe}` | | | `.exe` |
| `{target}`，Rust 和 clang 的目标三元组，在该系统上构建 | `aarch64-apple-darwin`、`x86_64-apple-darwin` | `aarch64-unknown-linux-gnu`、`x86_64-unknown-linux-gnu` | `aarch64-pc-windows-msvc`、`x86_64-pc-windows-msvc` |
| `{target}`，从其他系统交叉构建 | | `aarch64-unknown-linux-musl`、`x86_64-unknown-linux-musl`（静态链接） | `aarch64-pc-windows-gnullvm`、`x86_64-pc-windows-gnu`（MinGW：不需要 Windows SDK） |
| `{goos}`、`{goarch}`，Go 的 | `darwin`；`arm64`、`amd64` | `linux`；`arm64`、`amd64` | `windows`；`arm64`、`amd64` |

`{target}` 和 Asuka 自己的工具链选的三元组一致：在目标系统上构建时用该系统自己的（Windows 上是 MSVC 的，也就是 rustup 在那里默认安装的），交叉构建时用 Zig 的。命令的环境变量里也有这些值，方便脚本使用：`ASUKA_OUT`、`ASUKA_OS`、`ASUKA_ARCH`、`ASUKA_EXE`、`ASUKA_TARGET`、`ASUKA_GOOS` 和 `ASUKA_GOARCH`。如果构建需要别的名字或参数（比如 Linux 上也用 musl，或只在某个系统上开启一个 feature），可以让 `build` 运行一个脚本，由脚本在运行时决定，见[由其他工具链构建的程序](programs.md#由其他工具链构建的程序)。

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

`asuka run --bin notes` 和 `asuka build --bin notes` 分别运行和构建其中一个（只有一个时可以省略 `--bin`），`asuka test` 会把每个程序的测试和应用的测试一起运行。桌面构建会把它们放进应用：Mac 的 `Contents/Helpers`、Linux 的 `bin`、Windows `.exe` 的旁边；手机和 Web 会忽略它们；`asuka dev` 启动时也会构建一次。应用用 `command.bundled(name)`（std/command）找到它们。应用自带的程序把数据保存在应用保存的地方。`asuka run`、`test` 和 `build --bin` 处理 Asuka 程序；由 `build` 构建的程序由它自己的工具链运行和测试。见[应用自带的程序](programs.md#应用自带的程序)。

## 环境变量

密钥之类的机密信息不要写进 `asuka.toml`，而是通过环境变量提供，CI 可以从它的 secrets 中读取。下面这些环境变量还会覆盖对应的键。

| 变量 | 说明 |
|---|---|
| `ASUKA_KEYSTORE`、`ASUKA_KEY_ALIAS` | 覆盖 `android.keystore` 和 `android.keyAlias`。 |
| `ASUKA_KEYSTORE_PASSWORD`、`ASUKA_KEY_PASSWORD` | keystore 的密码，以及与之不同时密钥的密码。 |
| `ASUKA_ASC_KEY_ID`、`ASUKA_ASC_ISSUER_ID`、`ASUKA_ASC_KEY` | 覆盖 `ios.appStoreConnect`；`ASUKA_ASC_KEY` 是 `.p8` 文件的路径或其内容。 |
| `ASUKA_PLAY_SERVICE_ACCOUNT` | 覆盖 `android.serviceAccount`：密钥文件的路径或其 JSON 内容。 |
| `ASUKA_MAC_PROFILE` | 覆盖 `mac.profile`。 |
| `ASUKA_UPDATE_PUBLIC_KEY`、`ASUKA_UPDATE_URL` | 覆盖 `updates.publicKey` 和 `updates.url`（设置后会取代 `github`）。 |
| `ASUKA_UPDATE_PRIVATE_KEY` | 更新签名私钥本身或它的路径，覆盖 `updates.privateKey`。 |
| `ASUKA_WINDOWS_CERT`、`ASUKA_WINDOWS_CERT_PASSWORD`、`ASUKA_WINDOWS_TIMESTAMP` | Windows 构建的签名证书（`.pfx`）、它的密码和时间戳服务器，这些不在 `asuka.toml` 里配置。见[打包与发布](shipping.md#windows)。 |
