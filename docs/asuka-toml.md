# asuka.toml

`asuka.toml`, in the app's directory, says what the platforms and the
stores need to know of the app: its name and ID, its icon, the links
and files it opens, its signing keys. `asuka dev`, `asuka build` and
`asuka publish` read it; `asuka dev` reads it as it starts, so stop it
and run it again after a change.

It's a [TOML](https://toml.io) file: `key = value` lines, tables under
`[name]` headers, and comments from `#` to the end of the line.

Every key is optional, and an app without the file builds and runs with
the defaults. A key Asuka doesn't know is ignored, so check a key's
spelling when it seems to do nothing. A value Asuka can't take stops the
build, saying which and what it wants (`asuka.toml: accent "red" (want
#rrggbb or #rrggbbaa)`), and so does a line TOML can't read or a value
of the wrong type, with its line and column (`asuka.toml:3:9: build:
want an integer, not a string`). Paths are relative to the app's
directory.

This page lists every key. [Shipping](shipping.md#asukatoml) tells how
they come together for a release, and each guide linked below tells
more of what its keys do.

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

[windows]
publisher = "Example Inc."

[updates]
publicKey = "…"
github = "you/notes"
```

## The app

These apply on every platform the key means something on.

| Key | Type | Default | What it is |
|---|---|---|---|
| `name` | string | the directory's name | The app's name: under its icon, in the Mac's menu bar, the window's title on Windows, the launcher's on Linux, the web page's title. It names the build's files too (`Notes.app`, `Notes.exe`). |
| `id` | string | `dev.asuka.<directory>` | The bundle ID on iOS and the Mac, the application ID on Android and Linux (its desktop file, icon, D-Bus name and data directory), the AppUserModelID on Windows. Change it before the first release: the stores and the systems know the app by it. On Linux it must be letters, digits, `_` and `-`, in two parts or more, none starting with a digit. |
| `version` | string | `1.0` | The version users see. |
| `build` | integer | `1` | The build's number, which must grow with each upload. `asuka publish` numbers a build one above the store's highest itself, so this is only where it starts. Windows's file version is `version` and `build` together (`1.2.0.7`). |
| `icon` | path | none: the platform's generic icon | A square PNG, 1024×1024, without transparency for iOS. Each build makes the sizes its platform wants. The App Store needs one. |
| `alternateIcons` | table | none | Other icons the app may show instead of its own, by name: `dark = "icons/dark.png"`, square PNGs as `icon`. Names are letters and digits, starting with a letter. `platform/appicon` switches between them ([The app's icon](device.md#the-apps-icon-platformappicon)). |
| `accent` | color | the platform's | `"#rrggbb"` or `"#rrggbbaa"`: the tint on iOS, the seed of the Material 3 color scheme on Android, the color of `Accent`, filled buttons and links on the Mac (AppKit's own controls keep the user's), the controls' accent on Windows and libadwaita's on Linux, the page's and the manifest's `theme_color` on the web. |
| `accentDark` | color | `accent` | The accent in dark mode, in the same form: on iOS, the Mac, Windows, Linux and the web, `.accent` and the controls take it while the app shows dark (the system's appearance, or `appearance.setColorScheme(.dark)`). Android's Material 3 scheme makes its dark colors from `accent`, so it doesn't use it. Without `accent`, light mode has the platform's. |
| `orientations` | strings | both | `"portrait"`, `"landscape"`: the orientations the app takes on a phone. iPads take all four, as Split View and Stage Manager need, and Android 16 ignores it on tablets and foldables. |
| `scheme` | string | none | The app's URL scheme: `notes://` URLs open it, and the app's `on openURL(url)` gets them, on iOS, Android, the Mac, Windows and Linux. Letters, digits, `+`, `-` and `.`, a letter first. See [Deep links](navigation.md#deep-links). |
| `links` | strings | none | Domains, as `example.com`, whose https links open the app: iOS's universal links and Android's App Links. The domain serves the association file. See [Deep links](navigation.md#deep-links). |
| `fileTypes` | array of tables | none | The kinds of files the app opens. See [fileTypes](#filetypes). |
| `shareTarget` | `true` or table | off | The app in other apps' share sheets, on iOS and Android. See [shareTarget](#sharetarget). |
| `splash` | table | none | The launch screen, on iOS and Android. See [splash](#splash). |
| `widgets` | array of tables | none | Home screen widgets, on iOS and Android. See [widgets](#widgets). |
| `shortcuts` | array of tables | none | Actions for Siri, Spotlight and the launcher, on iOS and Android. See [shortcuts](#shortcuts). |
| `nfc` | table | none | What `platform/nfc`'s tags need: iOS's AIDs and FeliCa system codes, the tags that open the app on Android. See [nfc](#nfc). |
| `background` | boolean | `false` | On the desktop, an app that lives in the tray: no window at launch, kept running with none, and on the Mac no Dock icon from launch (`LSUIElement`). See [Apps in the background](desktop.md#apps-in-the-background). |
| `keepRunning` | boolean | `false` | On the desktop, the app keeps running when its last window closes: the main window hides, until the app shows it again. A `background` app keeps running anyway. See [Apps in the background](desktop.md#apps-in-the-background). |
| `singleInstance` | boolean | `true` | On Windows and Linux, a second launch hands its files to the running app (its `on openFiles(paths)`) and quits. `false` runs the app as often as it's started. The Mac runs an app once either way. See [One instance](desktop.md#one-instance). |
| `i18n` | table | `locales`, when there | Where the app's catalogs are. See [i18n](#i18n). |
| `updates` | table | none | A desktop app's signed updates. See [updates](#updates). |
| `ios` | table | | The iOS build's, and some of the Mac's. See [ios](#ios). |
| `mac` | table | | The Mac build's profiles. See [mac](#mac). |
| `icloud` | table | `iCloud.<id>` | The app's iCloud containers and its iCloud Drive folder, for `platform/icloud`. See [icloud](#icloud). |
| `applePay` | table | none | `merchantIds = ["merchant.com.example"]`: the merchant IDs Apple Pay pays with (`platform/pay`), in the in-app-payments entitlement of an iOS or Mac app that imports the package. See [Wallet payments](device.md#wallet-payments-platformpay). |
| `android` | table | | The Android build's. See [android](#android). |
| `linux` | table | | What a Linux build says of the app. See [linux](#linux). |
| `web` | table | | The web build's. See [web](#web). |
| `bluetooth` | table | | What an app of `platform/bluetooth` asks beyond the package's needs: the background, the location from scans. See [bluetooth](#bluetooth). |
| `health` | table | | What an Android app of `platform/health` reads and writes in Health Connect, and its privacy policy. See [health](#health). |
| `ads` | table | | The AdMob app IDs of an app of `platform/ads`. See [ads](#ads). |
| `analytics` | table | | Google Analytics for `platform/analytics`: the web stream the web and the desktops send to, and what collection and consent start as. See [analytics](#analytics). |
| `bin` | tables | none | The project's programs: command-line tools in directories of their own, built into the desktop apps. See [bin](#bin). |

A Windows build needs nothing beyond these: its signing certificate is
given in the environment (see [Shipping](shipping.md#windows)).

## fileTypes

The kinds of files the app opens, from the file manager, a mail's
attachment or "Open With"; the app's `on openFiles(paths)` gets them. Each is a
`[[fileTypes]]` table:

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

| Key | Type | Default | What it is |
|---|---|---|---|
| `extensions` | strings | required | The files' name extensions, without the dot: `"md"`, `"tar.gz"`. |
| `name` | string | the first extension in capitals and "Document" | What the files are, as the system shows it. |
| `mimeTypes` | strings | those Asuka knows for the extensions | Their MIME types. For an extension Asuka doesn't know, the type is the app's own: `application/x-<app>-<extension>`. |
| `uti` | string | the system's for a known extension, else `<id>.<extension>` | Their Uniform Type Identifier on Apple's platforms. One under the app's ID is the app's own type: exported, and the app is its files' owner. Another the system may not know is imported. |
| `conformsTo` | strings | `public.data`, `public.content` | The UTIs an exported or imported type conforms to. `public.plain-text` makes Quick Look and text editors take it. |
| `role` | string | `"editor"` | `"editor"` or `"viewer"`: what the app does with them (`CFBundleTypeRole` on the Mac and iOS). |
| `icon` | path | the system's, or the app's | A square PNG, the files' icon on the Mac, Windows and Linux. |

Each platform declares them its own way: Info.plist's
`CFBundleDocumentTypes` on the Mac and iOS, intent filters on Android,
the user's registry on Windows as the app starts, the desktop file and
a shared-mime-info package on Linux, the manifest's `file_handlers` on
the web. See [Shipping](shipping.md#asukatoml) and
[Opening files](desktop.md#opening-files).

## shareTarget

The app in other apps' share sheets, on iOS and Android; the app's `on share(items)`
gets what's shared. `shareTarget = true` takes texts and web links; a
table says what else:

```toml
[shareTarget]
types = ["text", "image/*", "video/*"]
max = 10
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `types` | strings | `["text"]` | `"text"` (texts and web links), and files by MIME type: `"image/*"`, `"video/*"`, `"audio/*"`, `"application/pdf"`, `"*/*"` for any. |
| `max` | integer | `1` | How many files it takes at once, 1 or more. |

On iOS it's a share extension in the app, with its own bundle ID (the
app's plus `.share`), which a release signs with `ios.shareProfile`. One
that takes files also needs the App Group `group.<id>`. See
[Shipping](shipping.md#ios) and
[Shares from other apps](device.md#shares-from-other-apps-onshare).

## splash

The launch screen on iOS and Android: an image in the middle of a color,
from the tap on the icon until the app's first frame (or
`splash.Hide`). On the web, when it sets `background`, its colors (not
its image) are the page's while it loads, until the app's first frame.
The desktops have none.

```toml
[splash]
image = "logo.png"
size = 96
background = "#4A63B5"
darkBackground = "#1B2440"
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `image` | path | none | A square PNG, a logo. |
| `size` | integer | `120` | The image's width in points (dp), at most 160, the most Android 12's icon circle holds. |
| `background` | color | `"#ffffff"` | `"#rrggbb"`, in light mode. |
| `darkBackground` | color | `background` | `"#rrggbb"`, in dark mode. |

See [The launch screen](device.md#the-launch-screen-platformsplash).

## widgets

The app's home screen widgets, on iOS and Android; `widget.set` fills
them from the app.

```toml
[[widgets]]
kind = "today"
name = "Today"
description = "What's next"
sizes = ["small", "medium", "rectangular"]
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `kind` | string | required | The widget's name for `widget.Set`: letters, digits and `_`, a letter first, each widget's its own. |
| `name` | string | `kind` | What the widget gallery calls it. |
| `description` | string | none | What the widget gallery says of it. |
| `sizes` | strings | `["small", "medium"]` | `small`, `medium`, `large`, and iOS's `extraLarge` (iPad) and lock screen `circular`, `rectangular` and `inline`. |

On iOS the widgets are an extension with its own bundle ID (the app's
plus `.widgets`), which a release signs with `ios.widgetProfile`. See
[Home screen widgets](device.md#home-screen-widgets-platformwidget).

## shortcuts

Actions the system offers for the app, at most ten: iOS's App Shortcuts
(Siri, Spotlight, the Shortcuts app, the Action button) and Android's
launcher shortcuts. Each opens the app with its URL, for its
`on openURL(url)`.

```toml
[[shortcuts]]
id = "newNote"
title = "New Note"
url = "notes://new"
symbol = "square.and.pencil"
phrases = ["New note in ${app}"]
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `id` | string | required | Letters, digits and `_`, a letter first, each shortcut's its own. |
| `title` | string | required | What the system shows. |
| `url` | string | required | What the app is opened with: a URL of its `scheme`, or one of its `links`. |
| `symbol` | string | none | An SF Symbol's name, as `Symbol` takes it. |
| `phrases` | strings | `"<title> in ${app}"` | What Siri hears, each with `${app}`, the app's name, and maybe an `options` param's `${name}`. iOS only. |
| `params` | array of tables | none | What the shortcut asks for, each filling its `{name}` in `url`: `name`, `title`, `type` (`string`, `int`, `float`, `bool`, `options`, `entity`), `prompt`, `optional`, `default`, an `options` param's `options` (`{ value, title }`), and an `entity` param's `entity`: its kind, the app's own things of it (`platform/shortcuts`). Android opens `url` with the defaults. See [Parameters](device.md#parameters). |

See [Shortcuts](device.md#shortcuts-siri-spotlight-the-launcher).

## nfc

What `platform/nfc` needs of the platforms beyond its permissions, for
an app that imports it (an app that doesn't gets none of it):

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

| Key | Type | Default | What it is |
|---|---|---|---|
| `iso7816` | strings | none | The ISO 7816 applications (AIDs: 5 to 16 bytes, in hex) iOS selects as it finds a card, in order: Info.plist's `com.apple.developer.nfc.readersession.iso7816.select-identifiers`. A session with `Poll` finds only the cards with one of them. |
| `felica` | strings | none | The FeliCa system codes (4 hex digits, not `FFFF`) iOS polls for: `com.apple.developer.nfc.readersession.felica.systemcodes`, without which a session that polls FeliCa fails. |
| `ndef` | tables | none | The tags that open the app on Android by their message's first record (NDEF_DISCOVERED), each one of `uri` (its scheme, host and path prefix; the URI comes to the app's `on openURL(url)`), `mime` (a MIME type; text records are `text/plain`) and `external` (`domain:type`); the tag comes to `nfc.opened()`. |
| `tech` | arrays of strings | none | The tags that open the app on Android by their technologies (TECH_DISCOVERED): each list, the `android.nfc.tech` classes a tag must all have (`IsoDep`, `NfcA`, `MifareUltralight`...). |
| `required` | boolean | `false` | Makes the app one for phones with NFC alone: Google Play doesn't offer it to others. |

See [NFC tags](nfc.md#asukatoml-and-permissions).

## icloud

The iCloud containers of an app that calls `platform/icloud`'s
documents, and how they show in iCloud Drive (an app that doesn't gets
none of it):

```toml
[icloud]
containers = ["iCloud.com.example.journal"]
driveName = "Journal"
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `containers` | strings | `["iCloud.<id>"]` | The app's iCloud containers, the first its default: the entitlements' `com.apple.developer.icloud-container-identifiers` and `ubiquity-container-identifiers`. Its App ID needs them, with the iCloud capability. |
| `driveName` | string | the app's `name` | The first container's folder in iCloud Drive (Info.plist's `NSUbiquitousContainers`). |
| `driveHidden` | boolean | `false` | Leaves the app's documents out of iCloud Drive: they sync, but only the app sees them. |

See [iCloud](icloud.md#asukatoml-and-entitlements).

## i18n

Where the app's message catalogs are. An app with catalogs in a
`locales` directory needs none of it.

```toml
[i18n]
dir = "translations"
source = "ja"
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `dir` | path | `locales` | The directory of catalogs, one per language, named after its BCP 47 tag: `en.json`, `pt-BR.json`, `zh-Hans.json`. |
| `source` | string | `en`, or the only catalog there is | The language the app is written in: its catalog has every message, and the others fall back to it. |

See [Languages](i18n.md#catalogs).

## updates

A desktop app's updates, and a phone app's updates of its code: the key
they're signed with, and where they're published. `asuka update keygen` makes the key pair and prints this
table.

```toml
[updates]
publicKey = "…the contents of asuka-update.pub…"
github = "you/notes"
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `publicKey` | string | required | `asuka-update.pub`'s contents, built into the app: it takes only updates signed with the secret key. |
| `github` | string | | `"owner/name"`: a public repository whose releases, tagged `v1.2.0`, hold the updates. |
| `tagPrefix` | string | `"v"` | The releases' tags' prefix. Another, as `"desktop-v"`, lets the repository's latest release be another's: apps then find the newest tagged with it through the GitHub API. |
| `url` | string | | Instead of `github`: an HTTPS directory that serves the files (an S3 or R2 bucket, a CDN); apps read `<url>/update-<target>.json`. |
| `feed` | string | none | An HTTPS server of yours that apps ask instead, its URL with `{target}`, `{version}` and `{channel}`. It needs `url`, where the files are. |
| `s3` | table | none | The bucket that serves `url`, which `asuka publish` uploads to: `bucket` (required), `prefix` (its directory; default its root), `region` (default `AWS_REGION`, else `AWS_DEFAULT_REGION`, else `us-east-1`), `endpoint` (R2's, MinIO's; default Amazon S3), `pathStyle` (default: with `endpoint`, or a bucket name with dots). |
| `privateKey` | path | none | The secret key's path, for builds on your machine (`~/` is the home directory). |
| `changelog` | path | `CHANGELOG.md`, when there is one | A Markdown file whose `## 1.2.0` section is the release notes. |
| `deltas` | integer | `3` | How many versions before get a delta update, a download of what changed alone; `0`, none. |

It needs either `github` or `url`, not both. See
[Updates](shipping.md#updates).

## ios

The iOS build's. `team` is the Mac build's team too, and `usage`'s
purpose strings go in the Mac's Info.plist as well.

| Key | Type | Default | What it is |
|---|---|---|---|
| `team` | string | none | The Apple Developer team ID that signs the app (`ABCDE12345`). |
| `profile` | string | the best match installed | The app's provisioning profile: its path, name or UUID. `--profile` overrides it. |
| `shareProfile` | string | the best match installed | The share extension's (`shareTarget`), for the ID `<id>.share`. |
| `widgetProfile` | string | the best match installed | The widget extension's (`widgets`), for the ID `<id>.widgets`. |
| `notificationProfile` | string | the best match installed | The Notification Service Extension's (`push.Rewrite`), for the ID `<id>.notification`. |
| `usage` | table | the packages' own | Purpose strings of protected resources, by Info.plist key: `NSCameraUsageDescription = "To scan your receipts."` under `[ios.usage]`. The packages the app imports give generic ones; write your own. |
| `infoPlist` | table | none | String keys added to Info.plist. |
| `backgroundModes` | strings | none | Added to Info.plist's `UIBackgroundModes`: `"audio"`, to record and play with the screen locked. |
| `iphoneOnly` | boolean | `false` | Leaves iPad out: the app runs there as an iPhone app, in a phone-sized window. |
| `googleServices` | path | none | The Firebase project's `GoogleService-Info.plist`, for `platform/analytics`: the iOS and Mac builds carry it. It must be an Apple app's of the app's `id`. |
| `storekit` | string | none | A StoreKit configuration file (Xcode's `.storekit`) that `asuka dev`'s simulator build sells from, as Xcode's scheme does: see [In-app purchases](purchases.md#testing-your-purchases). |
| `nonExemptEncryption` | string | `"no"` | What a build says of its encryption (`ITSAppUsesNonExemptEncryption`), a legal statement that's yours to make: `"no"`, `"yes"`, or `"ask"`, which says nothing, so App Store Connect asks at each upload. |
| `collected` | array of tables | none | The data the app collects, for its privacy manifest (see below). |
| `appStoreConnect` | table | | The App Store Connect API key `asuka publish` uploads with: `keyId`, `issuerId` (none for an individual key) and `key`, the `.p8`'s path (default: `AuthKey_<keyId>.p8` in `~/.appstoreconnect/private_keys`, as `altool` looks). See [Publishing](shipping.md#publishing). |

Each of `collected` is a kind of data, by Apple's names without their
prefixes, for the privacy manifest Asuka writes (an app's own
`ios/PrivacyInfo.xcprivacy` replaces it, and this with it):

```toml
[[ios.collected]]
type = "EmailAddress"
linked = true
tracking = false
purposes = ["AppFunctionality"]
```

| Key | Type | What it is |
|---|---|---|
| `type` | string | `"EmailAddress"` is `NSPrivacyCollectedDataTypeEmailAddress`. |
| `linked` | boolean | Tied to the user's identity. |
| `tracking` | boolean | Used to track them. |
| `purposes` | strings | `"AppFunctionality"` is `NSPrivacyCollectedDataTypePurposeAppFunctionality`. |

See [Shipping](shipping.md#ios).

## mac

What a Mac build signs the app with, and hands it out in; its team is
`ios.team`.

| Key | Type | Default | What it is |
|---|---|---|---|
| `profile` | string | `ASUKA_MAC_PROFILE`, else, for an app that needs one, the best match installed | A Developer ID (or development) provisioning profile's path, name or UUID, which push, Sign in with Apple, App Attest and Apple Pay need. |
| `appStoreProfile` | string | the best match installed | A Mac App Store profile, for `--appstore`. |
| `packages` | strings | `["zip", "dmg"]` | What a build hands the app out in: `zip`, the app zipped; `dmg`, a disk image. |

`--profile` and `ASUKA_MAC_PROFILE` override the profiles, `--package`
the packages. See
[Shipping](shipping.md#macos).

## android

The Android build's.

| Key | Type | Default | What it is |
|---|---|---|---|
| `keystore` | path | none | The upload key's keystore, which signs a release. Its password is `ASUKA_KEYSTORE_PASSWORD`'s, and the key's `ASUKA_KEY_PASSWORD`'s (default: the keystore's). |
| `keyAlias` | string | none | The upload key's alias in the keystore. |
| `dynamicColor` | boolean | `false` | Takes the colors from the user's wallpaper (Android 12 and later), as Material apps do; `accent` is the seed elsewhere. |
| `permissions` | strings | the packages' own | Permissions added to the manifest: `"android.permission.CAMERA"`. `"<name>:28"` asks for one up to Android 9 (API 28) only. The packages the app imports add those they need. |
| `googleServices` | path | none | The Firebase project's `google-services.json`, for `platform/push` and `platform/analytics`. |
| `mapsKey` | string | none | A Google Maps Platform API key (Maps SDK for Android), for `platform/maps`. |
| `serviceAccount` | path | none | The Google Cloud service account's JSON key `asuka publish` uploads to Google Play with. Keep it out of the repository. |

See [Shipping](shipping.md#android).

## tv

What the TVs' builds take: Android TV's declaration, and the TV
launchers' image of the app.

```toml
[tv]
android = true
banner = "banner.png"
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `android` | boolean | `false` | The Android build is an Android TV (and Google TV) app too: the TV launcher's intent, touch and the Leanback library not required, so Google Play offers it to TVs, and the banner. |
| `banner` | path | made from `icon` | A landscape PNG (16:9, 1920 × 1080 or larger), the TV launchers' image of the app: Android TV's banner (320 × 180), Apple TV's icon and top shelf, each cut to its shape from the middle. Without it, the icon in the middle of `background`. |
| `background` | color | the icon's corner's | `"#rrggbb"`: what a banner made from the icon has behind it, the back layer of Apple TV's icon. |

An Apple TV build (`asuka build --platform tvos`) needs no key: it's
the iOS app's, signed with a tvOS profile. See [TVs](tv.md).

## car

What an app that imports `platform/car` is on a car's screen. The
category decides which cars show it and which templates it has, and
what each platform's build adds: the category's CarPlay entitlement and
a CarPlay scene on iOS; the Car App Library's service with its category,
`automotive_app_desc.xml` and the Car App API level on Android (an
audio app's: a media browser). It does nothing for an app that doesn't
import the package.

```toml
[car]
category = "audio"
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `category` | string | none | What the app is on a car: `audio`, `communication`, `charging`, `parking` or `fueling` (CarPlay and Android Auto), `quickFood` or `drivingTask` (CarPlay only), `pointOfInterest`, `iot` or `weather` (Android Auto only). Without it, the app doesn't show on cars. |
| `minCarApiLevel` | integer | `1` | The Car App API level the app's templates need on Android Auto, 1 to 7 (tabs: 6). |

Apple grants CarPlay's entitlements on request; a signed build whose
profile lacks the category's says so. See [Car screens](car.md#categories-asukatomls-car).

## linux

What a Linux build's desktop file, AppStream metainfo and packages say
of the app: what launchers, software centers and Flathub show. Flathub
and the software centers want the summary, description, license and
developer.

| Key | Type | Default | What it is |
|---|---|---|---|
| `summary` | string | none | A line saying what the app does. |
| `description` | string | none | A few paragraphs, separated by blank lines. |
| `categories` | strings | `["Utility"]` | [freedesktop.org's main categories](https://specifications.freedesktop.org/menu-spec/latest/category-registry.html): `Utility`, `Office`, `Development`, `Network`... |
| `keywords` | strings | none | More words a launcher finds the app by. |
| `license` | string | none | The app's license, as an SPDX expression: `MIT`, `GPL-3.0-or-later`, `LicenseRef-proprietary`. |
| `developer` | string | none | The developer's name. |
| `homepage` | string | none | The app's web page. |
| `flatpakRuntime` | string | a recent one | The GNOME runtime's version the Flatpak manifest asks for, 46 or later. |
| `packages` | strings | all | What a build hands the app out in: `tar.gz`, `deb`, `rpm`, `appimage` and `flatpak` (the manifest). `--package` overrides it. |
| `maintainer` | string | `developer` | Who the `.deb` and the `.rpm` say maintains them: `"Name <email>"`. |
| `debDepends` | strings | none | More packages the `.deb` depends on, in Debian's syntax: `"libfoo1 (>= 1.2)"`, `"a \| b"`. |
| `rpmRequires` | strings | none | More packages the `.rpm` requires, in RPM's: `"foo >= 1.2"`, `"(a or b)"`. |

See [Shipping](shipping.md#linux).

## windows

What a Windows build's installer and MSIX say of the app, and what it
hands the app out in.

| Key | Type | Default | What it is |
|---|---|---|---|
| `packages` | strings | `["zip", "installer"]` | What a build hands the app out in: `zip`, the app's folder zipped; `installer`, a setup `.exe` that installs it for the user; `msix`, a package for the Microsoft Store and sideloading. `--package` overrides it. |
| `publisher` | string | the app's name | Who makes the app, as Installed apps and the Store show it. |
| `homepage` | string | none | The app's web page, in Installed apps. |
| `desktopShortcut` | boolean | `false` | The installer puts a shortcut on the desktop too, beside the Start menu's. |
| `identityName` | string | the app's `id` | The MSIX package's name: for the Store, Partner Center's Package/Identity/Name. |
| `publisherId` | string | `"CN="` and `publisher` | The MSIX package's publisher, the subject of the certificate it's signed with (`"CN=Example Inc., O=Example Inc., C=US"`): for the Store, Partner Center's Package/Identity/Publisher. |

See [Shipping](shipping.md#windows).

## web

The web build's.

| Key | Type | Default | What it is |
|---|---|---|---|
| `vapidPublicKey` | string | none | The app server's VAPID public key (base64url), with which the browser subscribes to Web Push for `platform/push`. |
| `base` | string | `"/"` | The path the site is served at (`"/app/"` for `https://example.com/app/`): the page's `<base href>`, under which a router's hrefs are its addresses. `asuka build --base` overrides it. See [Addresses and history](web.md#addresses-and-history). |

See [Web Push](web.md#platformpush-web-push).

## bluetooth

What an app that imports `platform/bluetooth` asks for beyond what the
package gives it (its permissions and purpose string). It does nothing
for an app that doesn't import the package.

```toml
[bluetooth]
background = ["central"]
location = false
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `background` | strings | none | The roles the app keeps in the background, with iOS's state restoration (`bluetooth.restored()`). `"central"`: iOS's `bluetooth-central` mode (an app that calls `backgroundScan` has it anyway). `"peripheral"`: iOS's `bluetooth-peripheral` mode. Android needs nothing for either: `backgroundScan` gives an app its scan's receiver. |
| `location` | boolean | `false` | The app finds where the user is from its scans (beacons): Android's `BLUETOOTH_SCAN` without `neverForLocation`, and the location permission `bluetooth.request()` then asks for too (iOS: `NSLocationWhenInUseUsageDescription`). |

See [Bluetooth](bluetooth.md#what-the-build-adds).

## health

What an app that imports `platform/health` reads and writes on Android,
in Health Connect: each kind listed gives the app that kind's permission
(`android.permission.health.READ_STEPS`, `WRITE_STEPS`), which the
package doesn't declare itself, since Google Play asks an app to justify
each health permission it declares. `health.request()` throws
`HealthError.notAllowed` for a kind that isn't listed. iOS needs none of
it: HealthKit asks for any kind.

```toml
[health]
read = ["steps", "heartRate", "sleep"]
write = ["steps"]
privacyPolicy = "https://example.com/privacy"
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `read` | strings | none | The kinds the app reads, by `health.Kind`'s names: `steps`, `distance`, `activeEnergy`, `floors`, `heartRate`, `restingHeartRate`, `oxygenSaturation`, `respiratoryRate`, `weight`, `height`, `bodyFat`, `water`, `sleep`, `mindfulness`. |
| `write` | strings | none | The kinds the app writes, likewise. |
| `privacyPolicy` | string | none | The app's privacy policy, an http or https URL, needed with `read` or `write`: Health Connect's permission screen and the system's settings link to it, and it opens in the browser. |

Health Connect is built into Android 14 and later; on Android 9 to 13
it's an app from Google Play, and without it `health.available()` is
false. See [Health data](device.md#health-data-platformhealth).

## ads

The AdMob app IDs of an app that imports `platform/ads`, from each app's
settings in AdMob: Google Mobile Ads reads its platform's as the app
starts, and a build for iOS or Android without it fails. The app gets
Google's SKAdNetwork identifiers in its Info.plist too, so that installs
its ads lead to are counted on iOS.

```toml
[ads]
ios = "ca-app-pub-3940256099942544~1458002511"
android = "ca-app-pub-3940256099942544~3347511713"
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `ios` | string | none | The iOS app's AdMob app ID, `ca-app-pub-…~…` (not an ad unit's, with a `/`). |
| `android` | string | none | The Android app's, likewise. |
| `skAdNetworks` | strings | none | More ad networks' SKAdNetwork identifiers (`"abcd1234.skadnetwork"`), for ads from other networks through AdMob's mediation, beside Google's. |

The IDs above are Google's sample apps', for development with its test
ads (`ads.testUnit`). See [Ads](device.md#ads-platformads).

## analytics

What an app that imports `platform/analytics` tells Google Analytics
beyond its Firebase files (`ios.googleServices`,
`android.googleServices`): the GA4 web stream the web (gtag.js) and
Windows and Linux (the Measurement Protocol) send to, and what
collection and Consent Mode start as on every platform, until the app
changes them (`setEnabled`, `setConsent`). It does nothing for an app
that doesn't import the package.

```toml
[analytics]
measurementId = "G-XXXXXXXXXX"
apiSecret = "…"
enabled = false
consent = { adStorage = "denied", adUserData = "denied", adPersonalization = "denied" }
```

| Key | Type | Default | What it is |
|---|---|---|---|
| `measurementId` | string | none | A GA4 web stream's measurement ID (`G-…`; a Firebase web app's `measurementId`), for the web and the desktops. |
| `apiSecret` | string | none | A Measurement Protocol API secret of that stream (Admin, Data streams), for Windows and Linux; never on a web page. `ASUKA_ANALYTICS_API_SECRET` overrides it. |
| `endpoint` | string | GA's, `https://www.google-analytics.com/mp/collect` | Where Windows and Linux send their events: a Measurement Protocol collector of yours, such as a server-side tagging server's (`https://sgtm.example.com/mp/collect`). |
| `enabled` | boolean | `true` | `false` starts collection off, until the app calls `analytics.setEnabled(true)`: Firebase's `FIREBASE_ANALYTICS_COLLECTION_ENABLED` and `firebase_analytics_collection_enabled`. |
| `consent` | table | granted | Consent Mode's defaults, `"granted"` or `"denied"` each: `analyticsStorage`, `adStorage`, `adUserData`, `adPersonalization`. |

See [Analytics](device.md#analytics-platformanalytics).

## c

The C the app's `extern` declarations call: what it compiles into the
app for each target, and what it links. Each platform's table
(`[c.ios]`, `[c.android]`, `[c.macos]`, `[c.windows]`, `[c.linux]`,
`[c.web]`) adds to the top one's lists. A package's `asuka.toml` has a
`[c]` of its own, which an app that imports the package builds too.

| Key | Type | Default | What it is |
|---|---|---|---|
| `sources` | strings | none | C files, globs relative to `asuka.toml` (`c/*.c`; `**` matches any directories). A glob that names no file stops the build. |
| `include` | strings | none | Directories the sources include from (`-I`). `asuka_ffi.h` is always there. |
| `defines` | strings | none | Macros, `NAME` or `NAME=value` (`-D`). |
| `flags` | strings | none | More of the compiler's flags. |
| `libraries` | strings | none | Libraries to link: a name (`sqlite3`: `-lsqlite3`) or a file's path relative to `asuka.toml` (`lib/libfoo.a`). |
| `frameworks` | strings | none | Apple's frameworks, linked on iOS and macOS (`Security`). |

`[c.prebuilt]` names a vendor's libraries, built for each target: each
key a target (`ios-arm64`, `ios-simulator-arm64`, `ios-simulator-x64`,
`macos-arm64`, `macos-x64`, `android-arm64`, `android-arm`,
`android-x64`, `linux-x64`, `linux-arm64`, `windows-x64`,
`windows-arm64`, `web`; `ios-simulator` and `macos` for a universal
library), each value a static library's path relative to `asuka.toml`
(`.a`; on Windows `.a` or `.lib`), or a list of them. A build for a
target it names no library for stops, saying so.

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

`[c.download.<name>]` is an archive the build downloads instead of
keeping it in the project (a library's sources, a vendor's libraries):
`url` (`https://`, a `.zip`, `.tar.gz` or `.tgz`), `sha256` (the
archive's; another is refused) and `strip` (leading directories its
paths lose; default 0). A path in `[c]` that starts with `$<name>` is
in it (`$sqlite/sqlite3.c`; `$sqlite` alone, its top directory). It's
downloaded once, by the first build that needs it, into Asuka's cache.

```toml
[c]
sources = ["$sqlite/sqlite3.c"]
include = ["$sqlite"]

[c.download.sqlite]
url = "https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip"
sha256 = "0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9"
strip = 1
```

See [C from Asuka](c.md#building), [Downloaded
sources](c.md#downloaded-sources) and [Prebuilt
libraries](c.md#prebuilt-libraries).

## bin

The project's programs ([Programs](programs.md)): each `[bin.<name>]`
is a command-line tool, a package with `fn main` in a directory of its
own, and `<name>` is its executable's name (letters, digits, `-`, `_`
and `.`). A project that is a program itself (`asuka new --bin`) needs
none.

| Key | Type | Default | What it is |
|---|---|---|---|
| `path` | path | `path` or `build` | The program's directory, relative to `asuka.toml`. It imports the project's packages relatively (`import ../model`). |
| `build` | strings | `path` or `build` | Instead of `path`: the command that builds a program another toolchain makes (a Go or Rust tool the app ships), run in the project's directory for each target. `{out}` is where it writes the executable; the target's are below. |
| `env` | table | none | `build`'s environment, over the CLI's, with the same placeholders replaced in its values. |
| `output` | path | `{out}` | Where `build` leaves the executable, relative to `asuka.toml`, for a toolchain that picks its own place (cargo's `target/<triple>/release`): the CLI takes it from there. |

The target in `build`, `env` and `output`, in each toolchain's names:

| Placeholder | Mac (Apple silicon, Intel) | Linux | Windows |
|---|---|---|---|
| `{os}` | `macos` | `linux` | `windows` |
| `{arch}` | `arm64`, `x64` | `arm64`, `x64` | `arm64`, `x64` |
| `{exe}` | | | `.exe` |
| `{target}`, Rust's and clang's triple, built there | `aarch64-apple-darwin`, `x86_64-apple-darwin` | `aarch64-unknown-linux-gnu`, `x86_64-unknown-linux-gnu` | `aarch64-pc-windows-msvc`, `x86_64-pc-windows-msvc` |
| `{target}`, cross-built from another system | | `aarch64-unknown-linux-musl`, `x86_64-unknown-linux-musl` (static) | `aarch64-pc-windows-gnullvm`, `x86_64-pc-windows-gnu` (MinGW: no Windows SDK needed) |
| `{goos}`, `{goarch}`, Go's | `darwin`; `arm64`, `amd64` | `linux`; `arm64`, `amd64` | `windows`; `arm64`, `amd64` |

`{target}` is the triple Asuka's own toolchain picks: the system's own
on that system (MSVC's on Windows, as rustup installs by default there),
and Zig's when it cross-builds. The command's environment has them all
too, for a script: `ASUKA_OUT`, `ASUKA_OS`, `ASUKA_ARCH`, `ASUKA_EXE`,
`ASUKA_TARGET`, `ASUKA_GOOS` and `ASUKA_GOARCH`. A build that wants
other names or flags (musl on Linux too, a feature on one system) runs
a script that picks them; see [Programs another toolchain
builds](programs.md#programs-another-toolchain-builds).

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

`asuka run --bin notes` and `asuka build --bin notes` run and build one
(`--bin` left out when there's one), `asuka test` runs each one's tests
with the app's. A desktop build puts them in the app: the Mac's
`Contents/Helpers`, Linux's `bin`, beside the Windows `.exe`; phones and
the web leave them out, and `asuka dev` builds them once as it starts.
The app finds one with `command.bundled(name)` (std/command). An
app's program keeps its data where the app does. `asuka run`, `test`
and `build --bin` take the Asuka ones; a program `build` makes is its
toolchain's to run and test. See [An app's
programs](programs.md#an-apps-programs).

## The environment

Secrets stay out of `asuka.toml`: give them in the environment, which
CI does from its secrets. These also override the keys they name.

| Variable | What it is |
|---|---|
| `ASUKA_KEYSTORE`, `ASUKA_KEY_ALIAS` | Over `android.keystore` and `android.keyAlias`. |
| `ASUKA_KEYSTORE_PASSWORD`, `ASUKA_KEY_PASSWORD` | The keystore's password, and the key's when it differs. |
| `ASUKA_ASC_KEY_ID`, `ASUKA_ASC_ISSUER_ID`, `ASUKA_ASC_KEY` | Over `ios.appStoreConnect`; `ASUKA_ASC_KEY` is the `.p8`'s path or its text. |
| `ASUKA_PLAY_SERVICE_ACCOUNT` | Over `android.serviceAccount`: the key's path or its JSON. |
| `ASUKA_MAC_PROFILE` | Over `mac.profile`. |
| `ASUKA_UPDATE_PUBLIC_KEY`, `ASUKA_UPDATE_URL` | Over `updates.publicKey` and `updates.url` (which then replaces `github`). |
| `ASUKA_UPDATE_PRIVATE_KEY` | The updates' secret key itself, or its path, over `updates.privateKey`. |
| `ASUKA_WINDOWS_CERT`, `ASUKA_WINDOWS_CERT_PASSWORD`, `ASUKA_WINDOWS_TIMESTAMP` | A Windows build's signing certificate (a `.pfx`), its password and the timestamp servers, which `asuka.toml` doesn't hold. See [Shipping](shipping.md#windows). |
