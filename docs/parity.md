# What Asuka has, against SwiftUI and Expo

Each row is something apps often need: how SwiftUI and Expo do it, and
what Asuka has for it. A ✓ is on every platform Asuka runs on (iOS,
Android, the Mac, Windows, Linux and the web) unless the row says
otherwise; where a platform hasn't something (a Mac has no sensors, a
browser no home screen widgets), its package answers that it hasn't
(an `unsupported` or `notAvailable` error, or `available()` false). In
a browser, what each package does instead where a browser can't do
what a phone does is in [The web](web.md#native-modules).

The Asuka column names the element, the prop or the package
(`std/...` or `platform/...`, [The standard library and the platform
SDK](stdlib.md)) that does it,
and what it is on each platform: Asuka shows each platform's own
widgets and calls each platform's own APIs, from Swift on iOS and the
Mac, Kotlin on Android, JavaScript on the web and C on Windows and
Linux.

SwiftUI also runs on the Mac. Expo runs on iOS, Android and the web;
React Native reaches Windows and the Mac through Microsoft's React
Native for Windows and macOS, outside Expo. For Asuka against the
desktop's own frameworks (Electron, Tauri, Wails), see
[On the desktop](#on-the-desktop), and for what a desktop app needs
beyond its windows, [The desktop](#the-desktop); against MyGo, which
draws its own UI, see [Asuka and MyGo](mygo.md).

✓ has it · ◐ in part (what's missing is said) · ✗ not yet

## Layout

| | SwiftUI | Expo (React Native) | Asuka |
|---|---|---|---|
| Stacks, spacing, padding, alignment | `HStack`, `VStack`, `Spacer` | flexbox | ✓ `Row`, `Column`, `View` and flexbox's props: `grow`, `shrink`, `gap`, `align`, `justify`, `wrap`, `padding`, `margin` ([Layout](layout.md)) |
| Overlays, absolute placement | `ZStack`, `.overlay` | `position: absolute` | ✓ `absolute` with `top`, `left`, `start`, `end`... |
| Grids of views | `Grid`, `LazyVGrid` | community | ✓ `grid` (CSS grid: `Grid(columns:)`, `.fr`, `.repeat`, `.autoFill`; `gridColumn`, `gridRow`) |
| Scrolling | `ScrollView` | `ScrollView` | ✓ `Scroll` (vertical, `horizontal`) |
| Lazy lists, cell recycling | `List`, `LazyVStack` | `FlatList` | ✓ `List` (the platform's: `UICollectionView`, `RecyclerView`; on the Mac, Windows, Linux and the web Asuka recycles the rows in view, in the platform's scroll view) |
| Lazy grids | `LazyVGrid` | `FlatList numColumns` | ✓ a `List`'s `columns`, `gap` |
| Chat lists: start at the end, load older above | `defaultScrollAnchor(.bottom)`, `scrollPosition` | `FlatList inverted`, `maintainVisibleContentPosition` | ✓ a `List`'s `anchor: .bottom`, `on startReached`; rows added above what shows leave it in place, in every list |
| Sections, sticky headers | `Section` in a `List` | `SectionList`, `stickySectionHeadersEnabled` | ✓ a `List`'s `section` and `header` (sticky in plain and bare lists) |
| Selecting rows | `List(selection:)`, `EditButton` | community | ✓ a `List`'s `selecting`, a row's `selected`, `on select`, `on selectBegin` (iOS's edit mode and two-finger pan; Android's checkboxes and long press; a highlighted row on the desktops, a checkmark in a browser) |
| Safe area, keyboard | automatic | `SafeAreaView`, `KeyboardAvoidingView` | ✓ `SafeArea` (padded by the keyboard too), `environment()`'s `safeTop`... and `keyboard` |
| Reading a view's size, the window's | `GeometryReader`, size classes | `onLayout`, `useWindowDimensions` | ✓ `on layout(frame)`; `environment()`'s `width`, `height`, `widthClass`, `heightClass` |
| Aspect ratio, min and max sizes | `.aspectRatio`, `.frame(min:max:)` | style | ✓ `aspectRatio`, `minWidth`, `maxWidth`, `minHeight`, `maxHeight` |

## Controls

| | SwiftUI | Expo | Asuka |
|---|---|---|---|
| Text, styled text, links | `Text`, `AttributedString` | `Text` | ✓ `Text`, `RichText` (spans, links), `Selectable`; Markdown with `std/markdown` (parsed by Asuka, the same everywhere; drawn as the platform's styled text) |
| Buttons (variants, icons, menus) | `Button`, `Menu` | `Pressable` | ✓ `Button` (`variant`: plain, filled, tinted, gray, bordered, glass, glass prominent; `icon`; `menu`), `Menu` (a tap or `longPress`: context menus); items `disabled`, `checked`, in a `.submenu` or a titled `.section`; bar buttons that open menus |
| Text fields | `TextField`, `SecureField`, `TextEditor` | `TextInput` | ✓ `TextField` (`secure`, `keyboard`, `returnKey`, `contentType` for AutoFill, `multiline`, `plain`) |
| The cursor, inserting at it | `TextSelection` | `selection`, `onSelectionChange` | ✓ `bind selection`; a `Ref`'s `insert(text)` |
| Rich text editing (a mail's body) | `TextEditor` with `AttributedString` (iOS 26) | community (10tap, pell) | ✓ `bind rich`, a `Ref`'s `toggleFormat`, `format`, `setLink`, `on formats` (`UITextView`, `EditText`, `NSTextView`, a rich edit control, `GtkTextView`, `contenteditable`) |
| A bar on the keyboard | `.toolbar { ToolbarItemGroup(placement: .keyboard) }` | `InputAccessoryView` (iOS) | ✓ a `TextField`'s `accessory { … }` (iOS's `inputAccessoryView`; on Android a bar that follows the keyboard; in a browser on a touch screen) |
| Pasted images, keyboard GIFs and stickers | `onPasteCommand`, `PasteButton` | community (`onPaste`) | ✓ `pasteTypes`, `on paste` (what's pasted, or put in by the keyboard) |
| Toggle, slider | `Toggle`, `Slider` | community | ✓ `Toggle`, `Slider` |
| Segmented control | `Picker(.segmented)` | community | ✓ `Segmented` |
| Date and time | `DatePicker` | community | ✓ `DatePicker`, `time.Picker` (`mode`: date, time, both; `inline`) |
| Progress, spinner | `ProgressView` | `ActivityIndicator` | ✓ `Progress`, `ProgressRing` (with a value or without), `Activity` |
| Wheel and menu pickers, stepper | `Picker`, `Stepper` | community | ✓ `Wheel` (`UIPickerView`, `NumberPicker`; a pop-up menu on the desktops and the web), `Picker` (a pop-up button on iOS and the Mac, Material's exposed dropdown menu on Android, a combo box drawn as WinUI's on Windows, `GtkDropDown`, `<select>`), `Stepper` |
| Color picker | `ColorPicker` | community | ✓ `ColorPicker` (`UIColorWell`; on Android, which has none, a Material dialog of a palette, with opacity; `NSColorWell`, Windows' color dialog, `GtkColorDialogButton`, `<input type="color">`) |
| Icons | SF Symbols | `@expo/vector-icons` | ✓ `Symbol` (SF Symbols on iOS and the Mac; for the same names, Material Symbols on Android and the web, Segoe Fluent Icons on Windows, the icon theme's or Material Symbols on Linux: a curated set); SVG icons drawn as vectors with `std/svg` |
| Images (remote, cached, placeholders) | `AsyncImage` | `expo-image` | ✓ `Image` (`placeholder`, `blurHash` and `thumbHash` placeholders, a fading `transition`; cached in memory, and on disk on iOS and Android; each platform's decoders: ImageIO, `ImageDecoder`, `NSImage`, WIC, GDK's loaders, the browser's, so WebP, HEIC and AVIF where the platform reads them; animated GIF, APNG and WebP play everywhere, `animated`; SVG on the Mac, Linux and the web) |
| Lottie animations | community (lottie-ios) | `lottie-react-native` | ◐ `std/lottie`'s `lottie.View` (lottie-ios, lottie-android, lottie-web: JSON and dotLottie, `playing`, `loop`, `speed`, `progress` read and written, `segment`, `marker`, keypath `overrides`); nothing yet on Windows and Linux, where it's an empty box |
| Video | `VideoPlayer` | `expo-video` | ✓ `Video` (AVKit on iOS and the Mac, Media3 on Android, a `<video>` in WebView2 on Windows, `GtkVideo` (GStreamer) on Linux, `<video>` on the web) |
| Web content | `WKWebView` | `react-native-webview` | ✓ `WebView` (`WKWebView` on iOS and the Mac, Android's `WebView`, WebView2 on Windows, WebKitGTK on Linux: a URL or HTML, messages both ways, scripts before the page's (`preload`), blocked requests, its state and history, downloads, new windows, each request, a script's value, `webview.cookies(url)`; on the web an `<iframe>`, all of it for the app's own site); `platform/js` runs JavaScript off screen |
| Maps | `Map` | `expo-maps` | ✓ `platform/maps`'s `maps.View` (MapKit on iOS and the Mac, Google Maps on Android, OpenStreetMap's tiles on Windows, Linux (libshumate) and the web: a region, markers, the user's location, presses) |
| Custom drawing | `Shape`, `Path`, `Canvas` | `react-native-svg`, Skia | ✓ `Shape` and `Path` (fill, stroke, caps, joins, dashes, an animatable trim); `Canvas` (paths, gradients, text, transforms, clips: Core Graphics, Android's `Canvas`, GDI+, GSK, a 2D `<canvas>`); `std/svg` |
| Charts | Swift Charts | `victory-native`, community | ✓ `std/chart` (bars, lines, areas, points; drawn with `svg.Shape`, the same everywhere) |
| Shaders | `.colorEffect(ShaderLibrary...)` | Skia's runtime effects | ✓ `shader` declarations (Asuka, compiled with the app: Metal on iOS and the Mac, AGSL on Android 13 and later, WebGL 2 on the web, Direct3D 11 on Windows, OpenGL on Linux) |
| Native escape hatch | `UIViewRepresentable` | native modules | ✓ native packages: a `native interface` or `native component` declared in Asuka, implemented in Swift on iOS and the Mac, Kotlin on Android, JavaScript on the web, C on Windows and Linux ([Native code](native.md)); C libraries called directly with `extern` ([C from Asuka](c.md)) |
| Paging, page dots | `.scrollTargetBehavior(.paging)`, `TabView(.page)` | `react-native-pager-view` | ✓ a `Scroll`'s `paging`, `PageControl` |
| Checkbox | `Toggle` (`.checkbox`, macOS) | community | ✓ `Toggle { checkbox: true }` |

## Navigation and presentation

| | SwiftUI | Expo (expo-router) | Asuka |
|---|---|---|---|
| Stacks with native push, pop, back gestures | `NavigationStack` | Stack | ✓ `Stack`, `push`, `pop` |
| Tabs | `TabView` | Tabs | ✓ `Tabs`, `Tab` |
| Sidebar, drawer | `NavigationSplitView` | Drawer | ✓ `Split`, `Destination`, `Sidebar` (a stack on iPhone, a drawer on Android; `UISplitViewController`, `NSSplitViewController`, WinUI's `NavigationView` pane drawn on Windows, libadwaita's split view on Linux) |
| Badges on tabs and sidebar rows | `badge` | `tabBarBadge`, `drawerItem` counts | ✓ a `Tab`'s and a `Destination`'s `badge` (`badgeValue`, `BadgeDrawable`, a drawer counter; the source list's count on the Mac; Fluent's `InfoBadge`; a count on Linux and the web) |
| Bar buttons, titles, search | `.toolbar`, `.searchable` | header options | ✓ a screen's `title`, `largeTitle`, `toolbar { … }`, `bind search` |
| Bottom toolbar, primary action (FAB) | `ToolbarItem(placement: .bottomBar)` | — | ✓ a screen's `bottomToolbar`, `primary`, `primaryExtended`: UIKit's toolbar, Material's `BottomAppBar` and (extended) FAB, the window's toolbar on a Mac, a command bar on Windows, a `GtkActionBar` on Linux, a bar and a floating button on the web |
| Sheets with detents | `.sheet`, `.presentationDetents` | modal | ✓ `Sheet` (`detents`, `grabber`) |
| Full-screen modals | `.fullScreenCover` | `presentation: "fullScreenModal"` | ✓ `Sheet { fullScreen: true }` (iOS's `.fullScreen`; Material's full-screen dialog; over all of a desktop window's content; a `<dialog>` over the page) |
| Taking back, preventing dismissal | `.navigationBarBackButtonHidden`, `.interactiveDismissDisabled` | `BackHandler`, `usePreventRemove` | ✓ `onBack { … }` (screens, sheets, the app), `backGesture: false` |
| Popovers | `.popover` | — | ✓ `Popover` (a popover on iPhone too) |
| Alerts, action sheets | `.alert`, `.confirmationDialog` | `Alert` | ✓ `Alert` (`actionSheet`; text fields in it, a password's `secure`, buttons that follow them with `disabled`) |
| Snackbars, toasts (a brief message with Undo) | — | community (react-native-paper's `Snackbar`) | ✓ `snackbar(message, …)` (Material's on Android, `AdwToast` on Linux; drawn as the platform's on iOS, macOS, Windows and the web) |
| Deep links, universal and app links | `.onOpenURL` | Linking | ✓ a `Router` goes to its routes' links itself (the scheme's, universal and app links, notifications' URLs); `on openURL(url)` for the rest; `asuka.toml`'s `scheme` and `links` |
| URL routes: path patterns, nested navigators, an initial route under a link, redirects, guards, not found | `NavigationPath` with `Codable` values | expo-router (`[id]`, `[...rest]`, `unstable_settings`, `<Redirect>`, `Stack.Protected`, `+not-found`) | ✓ `Router` and `Route` (`:id`, `*rest`, `initial`, `redirect`, `guard`; `layout`: stacks, tabs, split views, slots; `sheet: true`), `navigate`, `replace`, `router.back()`, `router.setQuery`, `href` on buttons and views; the state comes back from its address (a reload, a desktop window's last run, `asuka dev`'s reloads) ([Routes](navigation.md#routes)) |
| Typed links | `NavigationPath` values | typed routes (generated `.d.ts`) | ✓ a routed screen's call is its address, checked as the app compiles: `navigate(Thread(id: 7))`, `href: Thread(id: 7)` |
| Pull to refresh, swipe actions | `.refreshable`, `.swipeActions` | `RefreshControl` | ✓ `on refresh`, `refreshing`; both edges: `swipeActions`, `leadingSwipeActions` |

## Motion

| | SwiftUI | Expo (Reanimated) | Asuka |
|---|---|---|---|
| Animating changes | `withAnimation` | `withTiming`, `withSpring` | ✓ `animate { … }`, `animate(.spring) { … }` (an `Animation`: duration, ease, linear, spring, delay, repeat, autoreverse) ([Motion](motion.md)) |
| Transforms | `.rotationEffect`, `.scaleEffect`, `.offset` | `transform` | ✓ `transform: Transform(…)` |
| Enter and exit transitions | `.transition` | entering/exiting | ✓ `transition` (`.opacity`, `.pop`, `.slide`, `Transition(…)` of your own) |
| Frame-driven animation | `TimelineView(.animation)` | `useFrameCallback` | ✓ `frames()`, `onFrame` |
| Shadows, gradients, blur | `.shadow`, `LinearGradient`, materials | style, expo-linear-gradient, expo-blur | ✓ `shadow`, `gradient`, `material` |
| Gestures | tap, long press, drag, magnify, rotate | Gesture Handler | ✓ `on press`, `on longPress`, `on pan`, `on pinch`, `on rotate`, `on swipe` |
| Keys of a hardware keyboard | `.onKeyPress`, `.keyboardShortcut` | `onKeyPress` (a `TextInput`'s; React Native for Windows and macOS: `onKeyDown`) | ✓ `keys` (from the focused view out, ahead of the control and the menus), `useKeys`, `WindowKeys` (the window's) |
| Pressed and hover states | `ButtonStyle`'s `isPressed`, `.onHover` | `Pressable`'s `pressed`, `onHoverIn`/`onHoverOut` | ✓ `pressed { … }` and `hovered { … }` variants; a `Ref`'s `pressed` and `hovered` for a group's or a peer's; `on pressedChange`, `on hoverChange` |
| Drag and drop | `.draggable`, `.dropDestination` | (none built in) | ✓ `dragItems`, `dropTypes`, `on drop` (the app's own values within it; texts, links and files with other apps; `dragging { … }` and `dragOver { … }` variants) |
| Shared-element transitions | `matchedGeometryEffect`, `.navigationTransition(.zoom)` | shared transitions | ✓ `shared` (a view grows from another's place and size), a screen's `zoomFrom` (iOS 18's zoom; Material's container transform) |
| Haptics | `sensoryFeedback` | `expo-haptics` | ✓ `device.haptic(…)` (a Force Touch trackpad on the Mac, `navigator.vibrate` in a browser) |
| Reduce Motion | `accessibilityReduceMotion` | `useReducedMotion` | ✓ `environment().reduceMotion` (iOS transitions dissolve; the desktops and the web apply `animate`'s changes at once) |

## Data and services

| | SwiftUI / Apple | Expo | Asuka |
|---|---|---|---|
| State | `@State`, `@Observable` | hooks | ✓ `state`, `store`s, derived `let`s (fine-grained signals: a write updates only what reads it) ([Concepts](concepts.md)) |
| Small persisted values | `@AppStorage` | AsyncStorage | ✓ `std/storage`'s `storage.Value`, `std/store` (UserDefaults, SharedPreferences, a file on Windows and Linux, `localStorage`) |
| SQLite | SwiftData / GRDB | `expo-sqlite` | ✓ `std/sqlite` (`migrate`, `query<T>` with rows decoded into `T`, `exec`, transactions, off the main thread; `watch<T>`: live queries that run again when a table they read is written and as their arguments change; the same SQLite on every platform, in a worker of the page's in a browser) ([Data](data.md)) |
| Secure storage | Keychain | `expo-secure-store` | ✓ `platform/secure` (the Keychain, the Android Keystore, Credential Manager, libsecret; on the web encrypted in IndexedDB under a key the browser won't let out, not a keychain) |
| Files, network | Foundation | `expo-file-system`, fetch | ✓ `std/fs`; `std/http` (the platform's own client: URLSession, `HttpURLConnection`, WinHTTP, libsoup, `fetch`), `http.download`, `http.stream` and server-sent events (`http.events`); `std/websocket` |
| Photos | `PhotosPicker` | `expo-image-picker` | ✓ `device.pickPhotos(max:)` |
| Documents | `UIDocumentPickerViewController` | `expo-document-picker` | ✓ `device.pickFiles(types:, multiple:)`; `platform/dialog` on the desktops |
| Camera | AVFoundation | `expo-camera` | ✓ `platform/camera`: `camera.take()` (the system's camera; on the Mac, Windows and Linux a dialog with a live preview), `camera.View` (a live preview, front or back), `capture()`, `record()` (AVFoundation, CameraX, Media Foundation, GStreamer, `getUserMedia`); `platform/record` records video with sound |
| Barcodes, QR codes | AVFoundation, Vision | `expo-camera` scanning | ✓ live in `camera.View` (`scan`, `on code`) and in image files (`scan.file`): Vision, ML Kit on Android (only in the apps that import it), the browser's `BarcodeDetector` where it has one, and on Windows and Linux Asuka's own reader in C (QR, Data Matrix, EAN, UPC, Code 128, Code 39) |
| Location | Core Location | `expo-location` | ✓ `platform/location` (Core Location, Android's location manager, WinRT's `Geolocator`, GeoClue or the location portal, the Geolocation API; when in use and always, precise or approximate; place names; background updates and regions, the app started for their events on iOS and Android, held while it works) |
| Translations, plurals, regional formats | String Catalogs, FormatStyle | `expo-localization`, i18next | ✓ catalogs and `tr.key(…)`, `std/i18n` ([Languages](i18n.md)) |
| Right-to-left layout | `layoutDirection` | `I18nManager` | ✓ follows the language (`start` and `end` mirror); `layoutDirection` for what reads one way everywhere |
| Local notifications | UserNotifications | `expo-notifications` | ✓ `platform/notify` (now or scheduled, images; UserNotifications on iOS and macOS, Android's, Windows' toasts with buttons and replies, Linux's desktop notifications; on the web through the app's service worker, a scheduled one showing at its time while a page of the app is open, else as the next opens: browsers can't schedule one) |
| Push notifications | APNs | `expo-notifications` | ✓ `platform/push` (APNs on iOS and macOS, Firebase Cloud Messaging on Android, Web Push with VAPID on the web: `register()` for the token, `received()`, `opened()`) |
| Notification actions, inline replies | `UNNotificationCategory`, `UNTextInputNotificationAction` | `setNotificationCategoryAsync` | ✓ `notify.setCategories`, `notify.responses()` (actions and replies; `r.defer()` keeps the app running for the work; on the web, the service worker's actions, inline replies where Chrome has them) |
| Threads, the notifications showing, the badge | `threadIdentifier`, `getDeliveredNotifications`, `setBadgeCount` | `getPresentedNotificationsAsync`, `setBadgeCountAsync` | ✓ a notification's `thread` (Android: a group with its summary kept), `notify.showing()`, `remove`, `removeThread`, `setBadge` |
| Notifications in the foreground | `willPresent` | `setNotificationHandler` | ✓ `notify.present(decide)` |
| Android notification channels | — | `setNotificationChannelAsync` | ✓ `notify.setChannels`, `setChannelGroups`; a push's channel (iOS has none) |
| Messages in conversations | Communication notifications (`INSendMessageIntent`) | community | ✓ `Notification(…).from(person, conversation)`, `Message.from`: iOS's communication notifications, Android's MessagingStyle and conversation shortcuts; plain elsewhere |
| Changing a push before it shows | Notification Service Extension | community (`expo-notifications` has none) | ✓ `push.rewrite(f)`: the app's code in iOS's Notification Service Extension (built and signed by `asuka build`), in Android's Firebase service for data messages |
| Clipboard, share sheet, browser | `UIPasteboard`, `ShareLink`, `SFSafariViewController` | `expo-clipboard`, `expo-sharing`, `expo-web-browser` | ✓ `device.copy(text)`, `device.clipboardText()`, `clipboardHas`, `clipboardImage`, `copyImage`, `clipboardChanges()`; `device.share(text)`, `device.shareFiles(paths)` (the share menu on a Mac, Windows' share UI, the browser's; Linux has none: copied, with a toast); `device.browse(url)` (`SFSafariViewController`, a Custom Tab), `device.openURL(url)` |
| Previewing files | `QLPreviewController`, `.quickLookPreview` | `react-native-file-viewer` | ✓ `platform/preview` (Quick Look on iOS and the Mac, several files, Markup; the file's app on Android, Windows and Linux; a new tab on the web) |
| Printing, PDFs from HTML | `UIPrintInteractionController`, `UIPrintPageRenderer` | `expo-print` | ✓ `platform/print` (files, HTML, a `WebView`, with the system's dialog and its outcome; `print.pdf`: paginated, the region's paper, margins) |
| Mail and message composers | `MFMailComposeViewController`, `MFMessageComposeViewController` | `expo-mail-composer`, `expo-sms` | ✓ `platform/compose` (`compose.mail`: recipients, subject, HTML body, attachments, what the user did on iOS, the mail app elsewhere; `compose.message`: Messages on iOS, Android's SMS app) |
| Keep awake, device and install info | `isIdleTimerDisabled`, `UIDevice` | `expo-keep-awake`, `expo-device`, `expo-application` | ✓ `device.keepAwake(on)`, `device.info()` (with where the app came from and when it was installed) |
| Network state | `NWPathMonitor` | `expo-network` | ✓ `platform/network` (`network.status()`: online, Wi-Fi or cellular, metered, Low Data Mode; NWPathMonitor, ConnectivityManager, the Network List Manager, NetworkManager, the Network Information API) |
| The user's last input, an inactivity timeout | an app-wide `UIWindow` subclass or gesture recognizer | community (`react-native-user-inactivity`) | ◐ `platform/activity` (`activity.idleTime()`, `lastInput()`, `inputs()`: presses, touches, keys, scrolls and text edits in every window of the app, never taken from its views; iOS, the Mac, Windows, Linux, the web; not yet Android, where `available()` is false) |
| Saving to the gallery, Downloads | `PHPhotoLibrary` | `expo-media-library` | ✓ `platform/library` (`library.save`: Photos, Android's gallery and Downloads, Music; the desktops' Downloads, Pictures, Movies and Music folders) |
| Work in the background | `beginBackgroundTask`, `BGTaskScheduler` | `expo-task-manager`, `expo-background-task` | ✓ `platform/background`: Android keeps the app running, its progress and buttons in a notification; iOS gives about 30 seconds; the desktops keep the app from being suspended (App Nap, sleep). `platform/periodic`: work the system runs now and then, the app started without a screen (BGTaskScheduler, WorkManager; on the desktops a timer while the app runs) |
| Receiving shares from other apps | Share extension | `expo-sharing` (`useIncomingShare`) | ✓ `on share(items)`: texts, links, images, videos, audio and files, one or many, with their name, MIME type and size (`asuka.toml`'s `shareTarget`: Android's intent filters and iOS's share extension and activation rule from it); `platform/sharesheet`, the app's own sheet over the app that shares (iOS's share extension; an Android activity), its data shared with the app; iOS and Android |
| Audio | AVAudioPlayer, AVAudioRecorder | `expo-audio` | ✓ `platform/audio` (`audio.play`, `audio.Player`: play, pause, seek, volume, rate, loop; AVPlayer, Media3's ExoPlayer, MCI, GStreamer, `<audio>`); `platform/record` (`record.audio`: quality, pause, a level) |
| Now Playing, lock screen controls, media keys | `MPNowPlayingInfoCenter`, `MPRemoteCommandCenter` | `expo-audio`'s lock screen controls, community (`react-native-track-player`) | ✓ `platform/nowplaying`'s `nowplaying.NowPlaying` (title, artist, artwork, duration, a position the system advances, play, pause, next, previous, seek; follows an `audio.Player` by itself; a MediaSession on Android, the SystemMediaTransportControls on Windows, MPRIS on Linux, the Media Session API on the web) |
| Sign in with Apple | `SignInWithAppleButton` | `expo-apple-authentication` | ✓ `platform/appleauth` (Apple's `SignInButton`, `signIn`, `credentialState`; iOS and macOS; elsewhere through `platform/webauth`) |
| Web sign-in (OAuth) | `ASWebAuthenticationSession` | `expo-auth-session`, `expo-web-browser` | ✓ `platform/webauth` (`ASWebAuthenticationSession`, a Custom Tab, a WebView2 window, a WebKitGTK dialog, a popup) |
| iCloud: key-value store, iCloud Drive | `NSUbiquitousKeyValueStore`, ubiquity containers, `NSMetadataQuery` | community (`react-native-cloud-storage`, `expo-icloud-storage`) | ✓ `platform/icloud` (the account; `icloud.Value`, kept in iCloud's key-value store; the app's documents in iCloud Drive, listed as they change with their download and upload status, read and written coordinated, downloaded, evicted, conflicts resolved; `icloud.fake()` for tests; iOS and macOS; no CloudKit databases) |
| Apple Pay, Google Pay | PassKit (`PayWithApplePayButton`) | `@stripe/stripe-react-native`'s Platform Pay, `react-native-payments` | ✓ `platform/pay` (the wallet's sheet and `PayButton`; the token for the app's processor; shipping, coupons and card changes that update the totals; errors by field; Apple Pay on iOS, macOS and in Safari, Google Pay on Android and in other browsers) |
| Face ID, fingerprints | LocalAuthentication | `expo-local-authentication` | ✓ `platform/biometric` (LocalAuthentication, BiometricPrompt, Windows Hello; on the web, the platform's authenticator through WebAuthn; Linux has none) |
| In-app purchases, subscriptions | StoreKit 2 | `expo-iap`, `react-native-iap`, RevenueCat | ✓ `platform/purchase` (products with localized prices and offers, purchases, pending payments, a stream of transactions from the app's start, finishing, entitlements, restore, subscription status, the manage, refund and offer code sheets; StoreKit 2 on iOS and macOS, Play Billing, the Microsoft Store, the Digital Goods API in a Trusted Web Activity; `purchase.fake()` for tests) ([In-app purchases](purchases.md)) |
| Store review | `requestReview` | `expo-store-review` | ✓ `platform/review` (the App Store's prompt, Play's in-app review) |
| Tracking permission | App Tracking Transparency | `expo-tracking-transparency` | ✓ `platform/tracking` |
| App integrity | App Attest | `expo-app-integrity` | ✓ `platform/integrity` (App Attest, Play Integrity) |
| Age range | Declared Age Range | `expo-age-range` | ✓ `platform/agerange` (Declared Age Range, Play Age Signals) |
| Sensors | Core Motion | `expo-sensors` | ✓ `platform/sensors` (accelerometer, gyroscope, magnetometer, device motion, barometer, light, pedometer; the same axes and units everywhere; Core Motion, SensorManager, iio-sensor-proxy's light and orientation on Linux, the Generic Sensor API; a Mac and Windows have none) |
| NFC tags | Core NFC | `react-native-nfc-manager` | ✓ `platform/nfc` (NDEF reading and writing, read-only tags, the tag's ID, raw commands; iOS's sheet, Android's reader mode, tags that open the app; Web NFC in Chrome on Android; the desktops have none) ([NFC tags](nfc.md)) |
| Bluetooth LE | Core Bluetooth | community (`react-native-ble-plx`) | ✓ `platform/bluetooth` (scans, connections, services, reads, writes, notifications; advertising and a GATT server; background scans and state restoration; Core Bluetooth, Android's BLE, WinRT's, BlueZ; Web Bluetooth's chooser in Chromium, no peripheral role) ([Bluetooth](bluetooth.md)) |
| Image editing | Core Graphics, ImageIO | `expo-image-manipulator` | ✓ `platform/imageedit` (resize, crop, rotate, flip, fill, pixelate; HEIC in, JPEG or PNG out, upright; Image I/O, Android's `Bitmap`, WIC, GdkPixbuf, a canvas) |
| Video thumbnails | `AVAssetImageGenerator` | `expo-video-thumbnails` | ✓ `imageedit.videoFrame` |
| A view as an image | `ImageRenderer`, `drawHierarchy` | `react-native-view-shot` | ✓ `imageedit.capture(view: ref)`: PNG or JPEG, drawn by the platform (layers, Android's `View`, the window's pixels on Windows and Linux; on the web an SVG copy of the elements) |
| Text, faces and codes in an image | Vision | no | ✓ `platform/vision`, with where each is (Vision; ML Kit on Android, only in the apps that import it; Windows' OCR and face detection; Tesseract's text on Linux, no faces; the browser's Shape Detection API where it has one) |
| Speech to text | Speech (`SFSpeechRecognizer`) | community (`expo-speech-recognition`) | ◐ `platform/transcribe` (a recording, on the device only: Speech on iOS and macOS, Android 13's on-device recognizer, a browser's on-device recognizer (Chrome 139 and later); `transcribe.listen`: dictation from the microphone, as it's said; not on Windows and Linux, whose recognizers take only the microphone) |
| The photo library | PhotoKit | `expo-media-library` | ✓ `platform/photos`: permission, listing, thumbnails, the original files (PhotoKit, MediaStore) |
| Text to speech | `AVSpeechSynthesizer` | `expo-speech` | ✓ `platform/speech` (a queue, voices, pitch, rate, pause; AVSpeechSynthesizer, TextToSpeech, SAPI, Speech Dispatcher, `speechSynthesis`) |
| Contacts | Contacts, ContactsUI | `expo-contacts` | ✓ `platform/contacts` (`pick()` and the new contact form without permission; the address book with it) |
| Calendars | EventKit, EventKitUI | `expo-calendar` | ✓ `platform/calendar` (the event form without permission; calendars and events with it; EventKit, the Calendar Provider) |
| Home screen widgets | WidgetKit | `expo-widgets` | ✓ `platform/widget` and `asuka.toml`'s `widgets` (a template in every size, the lock screen's on iOS; timelines, countdowns, taps with a URL; iOS and Android) |
| Live Activities | ActivityKit | `expo-live-activity` | ✓ `platform/liveactivity` (iOS: the lock screen and the Dynamic Island; Android 16's Live Updates; countdowns, progress, alerts, push tokens for a server's updates) |
| App Intents, shortcuts | App Intents, App Shortcuts | — | ✓ `asuka.toml`'s `shortcuts` (iOS: App Shortcuts with Siri phrases, in Spotlight and Shortcuts, and parameters: text, numbers, switches, options a phrase can name; Android: launcher shortcuts); not app entities yet |
| Custom fonts | `.font(.custom)` | `expo-font` | ✓ `assets/fonts`, `Font(family:)` |
| Lifecycle | `scenePhase` | AppState | ✓ `onLifecycle` |
| Launch screen | `UILaunchScreen` | `expo-splash-screen` | ✓ `asuka.toml`'s `splash`; `platform/splash` holds it while the app loads |
| Alternate app icons | `setAlternateIconName` | community (`expo-dynamic-app-icon`) | ✓ `asuka.toml`'s `alternateIcons`, `platform/appicon`'s `set` (iOS's home screen; Android's launcher, as the app leaves the screen; the Mac's Dock, Windows' windows and taskbar, the web page's icon; not on Linux, whose launchers show the desktop file's) |
| Status bar, navigation bar | `statusBarHidden`, `preferredStatusBarStyle` | `expo-status-bar`, `expo-navigation-bar` | ✓ `screen.setBars` (`platform/screen`) |
| Orientation lock | `supportedInterfaceOrientations` | `expo-screen-orientation` | ✓ `screen.lock` |
| Screenshots | `isCaptured`, `userDidTakeScreenshotNotification` | `expo-screen-capture` | ✓ `screen.preventCapture` (Android: black captures; iOS: covered while recorded; macOS and Windows: the windows left out), `screen.screenshots()` |
| Screen brightness | `UIScreen.brightness` | `expo-brightness` | ✓ `screen.setBrightness`, `screen.brightness()` (iOS and Android: the app's while it's in front, the user's put back as it goes; the desktops and the web leave it to the user) |
| Crash reports | MetricKit, Crashlytics, Sentry | Sentry, Crashlytics (`@react-native-firebase/crashlytics`) | ✓ `platform/crash`: Asuka's fatal errors with the Asuka stack, uncaught exceptions, MetricKit's crashes and hangs, Android's native crashes and ANRs, the desktops' crashes, with breadcrumbs and values; the app sends them where it likes (no service built in); symbols from `asuka build` |

## Appearance and accessibility

| | SwiftUI | Expo | Asuka |
|---|---|---|---|
| Dark mode, semantic colors, accent | automatic | `useColorScheme` | ✓ semantic colors, `environment().dark`, `asuka.toml`'s `accent` (Material 3's color scheme from it on Android, `dynamicColor` for the wallpaper's) |
| Dynamic Type, font scale | automatic | `allowFontScaling` | ✓ text styles and custom fonts scale; `environment().fontScale` |
| Screen reader labels, roles, hiding | `.accessibilityLabel`... | `accessibilityLabel`... | ✓ `label`, `hint`, `role`, `a11yHidden`; pressable views are buttons ([Accessibility](accessibility.md)) |
| Announcements | `AccessibilityNotification.Announcement` | `announceForAccessibility` | ✓ `accessibility.announce(text)` |
| Moving a screen reader's focus | `@AccessibilityFocusState` | `setAccessibilityFocus` | ✓ `accessibility.focus(ref)` |
| Is a screen reader on | `voiceOverEnabled` | `isScreenReaderEnabled` | ✓ `environment().screenReader` |
| Custom actions, adjustable | `.accessibilityAction`, `.accessibilityAdjustableAction` | `accessibilityActions` | ✓ `actions`, `on adjust` (and a list's swipe actions are TalkBack's and VoiceOver's actions) |

## Tooling

| | Xcode / Expo | Asuka |
|---|---|---|
| Live reload, keeping state | previews, Fast Refresh | ✓ `asuka dev`: the player swaps in each save in milliseconds, keeping every `state`, on the phones, the desktops and in the browser ([Development](development.md)) |
| Inspecting the views | Xcode's view debugger, React Native DevTools, Flutter DevTools | ✓ `asuka dev`'s inspector: the running app's views as a tree in a browser (element, text, frame), the one pointed at highlighted on the device ([Development](development.md#the-inspector)) |
| Checking the setup | Expo's doctor, `flutter doctor` | ✓ `asuka doctor`: what this computer can build for each platform, and what to install for the rest |
| Running on a phone | Xcode's Run, Expo's dev builds | ✓ `asuka dev --platform ios` or `android`: the simulator, an emulator, an Android phone (`adb reverse`), an iPhone by USB or Wi-Fi (`--device iphone`, signed by your team) |
| Tests | XCTest, Jest with React Native Testing Library | ✓ `asuka test`: `test` blocks, the app mounted with no device (`mount`, `app.tap`, `app.expect`), fakes for the device's packages ([Testing](testing.md)) |
| Release builds and signing | Xcode, EAS Build | ✓ `asuka build`: the app compiled ahead of time; an `.ipa`, an `.aab`, an `.app` signed and notarized (or a Mac App Store `.pkg`), a Windows folder with its `.exe` signed, a Linux install prefix with a `.tar.gz` and a Flatpak manifest, a static site ([Shipping](shipping.md)) |
| Uploading to the stores | Xcode, EAS Submit | ✓ `asuka publish`: TestFlight, App Review, Google Play's tracks; the Mac App Store and a Mac's TestFlight; a desktop build and its signed update |
| GUI tests | XCUITest, Detox, Maestro | ✓ `asuka test --gui`: the app's flows (`flows/*.flow`: `tap Add`, `type hello`, `see Count: 1`, `back`) on the simulator, an emulator, a phone, headless Chrome or this computer ([GUI tests](testing.md#gui-tests-flows)); a desktop app's steps and snapshots too (`ASUKA_SCRIPT`, `ASUKA_SNAPSHOT`) |
| Benchmarks | Instruments | ✓ `bench/` (against SwiftUI and Compose) |
| Embedding in an existing app | brownfield (React Native's) | ✓ `asuka build --platform ios --xcframework`, `asuka build --platform android --aar` |
| Over-the-air updates | EAS Update | ✗ (a release has the app compiled into it) |

## On the desktop

On the Mac, Windows and Linux, apps are often made with a web view
around HTML instead. How those frameworks compare with Asuka:

| | Electron | Tauri | Wails | React Native for Windows and macOS | Asuka |
|---|---|---|---|---|---|
| Platforms | Mac, Windows, Linux | Mac, Windows, Linux; iOS and Android (Tauri 2) | Mac, Windows, Linux | Windows, Mac (with React Native's iOS and Android) | Mac, Windows, Linux, iOS, Android, the web |
| What shows the interface | HTML and CSS in Chromium | HTML and CSS in the system's web view (WKWebView, WebView2, WebKitGTK) | HTML and CSS in the system's web view | the platform's native views | the platform's widgets: AppKit; Win32 and the common controls, drawn as Windows 11's; GTK 4 and libadwaita |
| The app's language | JavaScript (Node.js beside the page) | Rust, and JavaScript in the page | Go, and JavaScript in the page | JavaScript | Asuka (and Swift, C or JavaScript in its native packages) |
| What ships with the app | Chromium and Node.js | the app's binary (the web view is the system's) | the app's binary (likewise) | a JavaScript engine and React Native's runtime | one executable, the app compiled ahead of time; on Linux GTK 4 and libadwaita come from the system; no web view unless the app shows a `WebView` |

A web view app looks the same on every system, as its HTML and CSS
say, and uses the web's libraries. An Asuka app looks as each platform's
apps do (an app that sets no style is a Mac app on the Mac, a GNOME app
on Linux, a Windows 11 app on Windows) and gets their accessibility,
text input and menus, with one layout engine for all of them. `asuka
build` makes Windows and Linux builds from any system (with Zig, and no
GTK installed); a Mac's needs a Mac with Xcode.

## The desktop

What a desktop app needs beyond its windows and menu bar, against
Electron and MyGo ([The desktop](desktop.md)). A ✓ is on the Mac,
Windows and Linux unless the row says otherwise.

| | Electron | MyGo | Asuka |
|---|---|---|---|
| Controlling windows | `BrowserWindow` | `Window` | ✓ the `app` block's `Window` and any other `Window` (an app with only these, a tray's, has no main window), `window.main()`, `window.current()`, a window's `Ref` (`Window as w`): show, hide, focus, minimize, maximize, full screen, size, place, always on top, its state followed; `on closeRequest`, `WindowCloseGuard`; where they were, by `stateKey`; `child` and `modal` windows; `titleBar: .hidden` (Wayland: no place, center or always on top, which the compositor decides) |
| Tray icons | `Tray` | tray icons | ✓ `platform/tray`'s `tray.Tray` (an `NSStatusItem`; the notification area; a StatusNotifierItem on Linux): image or symbol, title, tooltip, menu, click |
| Submenus, a Quit item | `MenuItem.submenu`, roles | roles, submenus | ✓ `MenuBar`, `Menu`, `MenuItem` (items in an item are a submenu; `role: .quit`, `.close`, `.openRecent`); `Action`'s `.submenu` and `.section` in tray and Dock menus |
| The Dock menu, jump list tasks | `app.dock.setMenu`, `setUserTasks` | the Dock menu, jump lists | ✓ `tray.DockMenu` (the Dock's menu, the jump list's Tasks, Linux's launcher quicklist) |
| Apps in the background | `app.dock.hide`, not quitting on `window-all-closed` | an app in the background | ✓ `asuka.toml`'s `keepRunning` and `background` |
| Flyouts: a window hanging from a tray icon | a frameless `BrowserWindow` placed at `tray.getBounds()` by the app | flyouts | ✓ a `Window`'s `flyout`, hanging from `tray.anchor(id)` or a rectangle of its window: placed and flipped in the display, closed by a click elsewhere or Escape, a modal dialog over it (Linux: placed under X11 only) |
| Panels for launchers | `type: 'panel'` (macOS) | panels | ✓ a `Window`'s `panel`: floating over every app and Space, the keyboard without activating the app on a Mac, closed by Escape or a click elsewhere (Linux under Wayland: a layer-shell surface where gtk4-layer-shell is there) |
| Windows sized to their content | `setContentSize` by the app | `FitContent` | ✓ `fitContent`: Asuka's layout sizes the window as its content changes |
| Click-through windows | `setIgnoreMouseEvents` | `SetIgnoreMouseEvents`, `SetInputRegion` | ✓ `clickThrough`, `w.setClickThrough(on)` (the whole window) |
| The apps installed, for a launcher | none | `InstalledApps`, `FileIcon`, `LaunchApp` | ✓ `platform/apps`'s `list`, `search`, `icon`, `open` |
| Global shortcuts | `globalShortcut` | global shortcuts | ✓ `platform/hotkey`'s `hotkey.Hotkey` (Carbon's hot keys; `RegisterHotKey`; X11 grabs or Wayland's GlobalShortcuts portal) |
| Open, save and folder dialogs | `dialog.showOpenDialog`, `showSaveDialog` | dialogs | ✓ `platform/dialog`'s `open`, `save`, `chooseFolder`, `chooseFolders` (sheets on the Mac, the common item dialogs, GTK's file dialog or the portal; the web: the File System Access pickers or the file input; phones: `open` only) |
| Reveal, open in its app, move to the trash | `shell.showItemInFolder`, `openPath`, `trashItem` | the shell | ✓ `platform/shell`'s `reveal`, `open`, `trash`, `beep` |
| File associations | `fileAssociations` (electron-builder), `open-file` | file associations | ✓ `asuka.toml`'s `fileTypes`, `on files(paths)` (also iOS, Android and installed web apps) |
| One instance, its second launch | `requestSingleInstanceLock`, `second-instance` | a single instance | ✓ `on secondInstance(args, dir)`, `asuka.toml`'s `singleInstance` (the Mac passes files, not arguments) |
| Quitting, asking first | `app.quit`, `before-quit` | quitting | ✓ `quit()`, `relaunch()`, `on quit(e)` (`e.cancel()`, `e.sessionEnding`) |
| The app's folders | `app.getPath` | directories | ✓ `fs.appDir()` (the app's files), `configDir()` (its settings: Roaming AppData on Windows, `~/.config` on Linux), `cacheDir()` (Library/Caches, Android's cache, `~/.cache`; the web's kept apart) and `logDir()` (Library/Logs, `~/.local/state`), each in its platform's place, everywhere |
| Recent documents | `app.addRecentDocument` | recent documents | ✓ `platform/recent` (Open Recent on the Mac, the jump list, the user's recent files on Linux) |
| Document-based apps | — | documents | ✓ `platform/document`: `Documents`, a window per file, the File menu, autosave, recovery copies, save questions on close and quit, files changed by other apps (phones and the web: one document at a time, saved in place) |
| Starting at login | `app.setLoginItemSettings` | start at login | ✓ `platform/login` (`SMAppService`, macOS 13+; the Run key; XDG autostart or the Background portal) |
| Watching files | Node's `fs.watch` (chokidar) | `WatchFiles`, the watch plugin | ✓ `std/watch`'s `watch.path`: renames paired, atomic saves as writes, bursts coalesced; phones and the web: unsupported |
| Displays | `screen` | displays | ✓ `screen.displays()`, `nearest`, `matching` (Wayland: no work area; phones and the web: theirs) |
| Sleep, wake, lock, idle time, battery | `powerMonitor` | power | ✓ `platform/power` (phones: battery, Low Power Mode, lock; the web: the battery) |
| Progress and attention on the app's icon | `setProgressBar`, `flashFrame`, `dock.bounce` | the Dock and the taskbar | ✓ `dock.setProgress`, `dock.requestAttention` (Linux: docks with the Unity launcher API) |
| Accent color, Increase Contrast, Reduce Transparency | `systemPreferences` | accent color and accessibility | ✓ `platform/appearance` (no Reduce Transparency on Linux) |
| Now Playing and the media keys | (none: `navigator.mediaSession` in the page) | Now Playing | ✓ `platform/nowplaying` (`MPNowPlayingInfoCenter`; the window's SystemMediaTransportControls; an MPRIS player) |
| Auto-updates: signed releases, deltas, Sparkle's window | `autoUpdater` (Squirrel), electron-updater | the updater, its plugin | ✓ `platform/update` (`update.UpdateWindow`, `update.AppMenu()`, `checkForUpdates`, `check`, `install`, `relaunch`; Ed25519 signatures, delta updates, channels; GitHub releases, an S3 bucket or any HTTPS directory, a server of yours; `asuka publish --platform mac\|windows\|linux`); apps the system's packages, Flatpak or a store installed update through them; the web reloads |

Not yet, of what MyGo has: of its documents, versions (a Mac's Browse
All Versions) and locking; and of its click-through windows, an input
region (the parts of a window that still take clicks: Asuka's let the
mouse through all of it, or none).

## Not yet

In order of what apps miss most: App Intents with entities (a
shortcut's parameters are values, not the app's own things yet), widgets
of the app's own layout (one template for now), and over-the-air updates
(EAS Update's: a release has the app compiled into it). On Windows and Linux, Lottie (`lottie.View` is an empty box).
On Windows and Linux, speech to text. On Windows, a screen's `zoomFrom`
(a push slides instead; Linux slides as libadwaita's navigation does,
GNOME having no zoom).
