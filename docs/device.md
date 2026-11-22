# Device and services

Asuka's packages reach what the device has beyond views: the camera, codes,
what's in an image, photos and files, previewing, printing and PDFs, the
mail and message composers, location, notifications and push, sound, Now Playing and recording,
the network's state, the shared library, work in the background, shares
from other apps, JavaScript, sign-in and biometrics, the screen, maps,
the stores' services, and crash reports.
This page covers each package: what it does, its API, the permissions it
brings and how it behaves on each platform.

A package has one API everywhere. iOS and Android have nearly all of
it. On macOS, Windows and Linux Asuka answers each package with the
desktop's own services, and in a browser with the web's APIs. Where a
platform has nothing to offer, the package answers as on a phone
without it: the package's `notAvailable` error, an error that names
what's missing, or a call that does nothing. Each package's section says what it does and
what each platform does it with. Each desktop and the browser also has a
section of its own ([On a Mac](#on-a-mac), [On Windows](#on-windows),
[On Linux](#on-linux), [In a browser](#in-a-browser)): what an app needs
set up there, and a table of where it differs from iOS and Android.

## Permissions and purpose strings

An app that imports a package is given what the package needs: Android
permissions in the manifest, iOS purpose strings in Info.plist, and the
platform code itself. An app that doesn't import it goes without, so a
photo app doesn't ask for the microphone. A Mac app is given the same
purpose strings and the entitlements they need; Windows, Linux and the
web need nothing declared (see each one's section below).

| Package | Android | iOS |
| --- | --- | --- |
| `platform/camera` | `CAMERA`, `RECORD_AUDIO` | `NSCameraUsageDescription`, `NSMicrophoneUsageDescription` |
| `platform/location` | `ACCESS_COARSE_LOCATION`, `ACCESS_FINE_LOCATION`, `ACCESS_BACKGROUND_LOCATION`, `FOREGROUND_SERVICE_LOCATION`, `RECEIVE_BOOT_COMPLETED`; the package's foreground service and its receiver (background updates, regions) | `NSLocationWhenInUseUsageDescription`, `NSLocationAlwaysAndWhenInUseUsageDescription`, the `location` background mode; for an app that calls `requestPrecise`, `NSLocationTemporaryUsageDescriptionDictionary` |
| `platform/notify` | `POST_NOTIFICATIONS` | |
| `platform/network` | `ACCESS_NETWORK_STATE` | |
| `platform/library` | `WRITE_EXTERNAL_STORAGE` (Android 9 and earlier) | `NSPhotoLibraryAddUsageDescription`; the app's Documents shown in Files |
| `platform/background` | `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_DATA_SYNC`, `POST_NOTIFICATIONS`, `WAKE_LOCK`; a foreground service | |
| `platform/scan` | ML Kit's barcode model (about 2 MB) | |
| `platform/vision` | ML Kit's text, face and barcode models (about 30 MB) | |
| `platform/photos` | `READ_MEDIA_IMAGES`, `READ_MEDIA_VISUAL_USER_SELECTED`; `READ_EXTERNAL_STORAGE` (Android 12 and earlier) | `NSPhotoLibraryUsageDescription` |
| `platform/push` | Firebase Cloud Messaging, `POST_NOTIFICATIONS` | the `aps-environment` entitlement; `remote-notification` background mode |
| `platform/appleauth` | | the Sign in with Apple entitlement |
| `platform/pay` | Google Pay's library, its API enabled, its callbacks service | the Apple Pay entitlement, with `asuka.toml`'s `applePay.merchantIds` |
| `platform/biometric` | `USE_BIOMETRIC` | `NSFaceIDUsageDescription` |
| `platform/record` | `RECORD_AUDIO`, `FOREGROUND_SERVICE_MICROPHONE`; a foreground service | `NSMicrophoneUsageDescription` |
| `platform/nowplaying` | `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_MEDIA_PLAYBACK`; a foreground service | the `audio` background mode |
| `platform/liveactivity` | `POST_NOTIFICATIONS`, `POST_PROMOTED_NOTIFICATIONS` | `NSSupportsLiveActivities` and the widget extension (for an app that calls it) |
| `platform/transcribe` | `RECORD_AUDIO` | `NSSpeechRecognitionUsageDescription` |
| `platform/screen` | `DETECT_SCREEN_CAPTURE` | |
| `platform/tracking` | | `NSUserTrackingUsageDescription` |
| `platform/review` | Play's in-app review library | |
| `platform/integrity` | Play Integrity | the App Attest entitlement |
| `platform/agerange` | Play Age Signals | the Declared Age Range entitlement |
| `platform/purchase` | Play Billing, `com.android.vending.BILLING` | StoreKit 2 (for an app that calls it); no entitlement |
| `platform/icloud` | (none: no iCloud) | the key-value store's entitlement, iCloud Documents and the app's containers, `NSUbiquitousContainers`, as the app calls them ([iCloud](icloud.md)) |
| `platform/maps` | Google's Maps SDK | |
| `platform/periodic` | WorkManager | `fetch` and `processing` background modes; the task identifiers |
| `platform/sensors` | `ACTIVITY_RECOGNITION` (the pedometer) | `NSMotionUsageDescription` (the pedometer) |
| `platform/speech` | the text-to-speech engine's visibility (`<queries>`) | |
| `platform/contacts` | `READ_CONTACTS` (for `list`) | `NSContactsUsageDescription` |
| `platform/calendar` | `READ_CALENDAR`, `WRITE_CALENDAR` (for `calendars`, `events`) | `NSCalendarsFullAccessUsageDescription`, `NSCalendarsWriteOnlyAccessUsageDescription`, `NSCalendarsUsageDescription` |
| `platform/compose` | the visibility of the apps for `mailto:` and `smsto:` (`<queries>`) | `mailto` in `LSApplicationQueriesSchemes` |
| `platform/nfc` | `NFC`; `uses-feature android.hardware.nfc` (not required); asuka.toml's `nfc` filters ([NFC tags](nfc.md)) | `NFCReaderUsageDescription`; Core NFC's entitlement; asuka.toml's `nfc` AIDs and FeliCa system codes |
| `platform/bluetooth` | `BLUETOOTH_SCAN` (`neverForLocation`), `BLUETOOTH_CONNECT`, `BLUETOOTH_ADVERTISE`; the background scan's receiver (for `backgroundScan`; see [Bluetooth](bluetooth.md#what-the-build-adds)) | `NSBluetoothAlwaysUsageDescription`; `bluetooth-central`, `bluetooth-peripheral` for `bluetooth.background` (`bluetooth-central` for `backgroundScan`), and the launch class that makes the managers iOS restores |

`platform/device`, `platform/audio`, `platform/splash`, `platform/imageedit`,
`platform/widget`, `platform/js`, `platform/secure`, `platform/preview` and
`platform/print` need none.

An entitlement needs its capability on the app's App ID
(developer.apple.com, Identifiers) for a release build: the provisioning
profile has it then. `asuka build --platform ios` stops with what's missing if the
profile lacks one; simulator builds have them all.

The purpose strings Asuka writes are generic ("To take a photo."). iOS
and macOS show them when they ask the user, so write your own in
`asuka.toml`'s `ios.usage`:

```toml
[ios.usage]
NSCameraUsageDescription = "To photograph your receipts."
NSLocationWhenInUseUsageDescription = "To find stores near you."
```

Each package asks the user the first time it needs to, not at launch.
Calls that wait (for the user, a location fix, the platform) are
`async`: `await` them in a handler, a task or another async function,
and the app goes on meanwhile ([Async](language/async.md#async)). What the
platform changes over time (the network, a permission, the account) is
tracked, a function a binding reads, or a `Stream` a task reads.

## On a Mac

A Mac app (`asuka dev` and `asuka build`) has the same purpose strings in
its Info.plist (location's as `NSLocationUsageDescription` too). Its
signature has the hardened runtime's entitlements for what they ask
(camera, microphone, location, contacts, calendars, photos), without
which a Developer ID build is refused them. Push's `aps-environment`,
Sign in with Apple, App Attest, Apple Pay and iCloud are entitlements only a provisioning
profile grants: `asuka build` embeds one for an app that uses them
(`asuka.toml`'s `mac.profile`, else `ASUKA_MAC_PROFILE`, else the best
match installed). Without one, the app builds, the CLI says what won't
work, and on the Mac registering for pushes and signing in with Apple
fail, and App Attest says it isn't supported. `asuka dev` signs ad hoc,
without a profile, so the same holds there. A Mac App Store build
(`--appstore`) is sandboxed, with the network and the files the user
picks. See [Shipping](shipping.md).

The packages work on macOS as on iOS, through the same frameworks
(AVFoundation, PhotoKit, Core Location, MapKit, Vision, Speech,
UserNotifications, AuthenticationServices), except where a Mac differs:

| Package | On a Mac |
| --- | --- |
| `platform/device` | `pickPhotos` and `pickFiles`: an open panel, a sheet on the window (no permission), copies in the app's caches. `browse` opens the page in the user's browser. `info`: `model` `"Mac"`, `system` `"macOS"` |
| `platform/camera` | `take()`: a sheet over the window with a live preview, Take Photo and Cancel (AppKit has no camera screen). `camera.View`'s "back" is the built-in camera (a Mac has no back one), or the one plugged in; scanning codes in it needs macOS 13 |
| `platform/record` | No audio session: other apps' sound plays on; `background` changes nothing, a Mac app records out of sight anyway |
| `platform/nowplaying` | `MPNowPlayingInfoCenter` and `MPRemoteCommandCenter`, as on iOS: Control Center's and the menu bar's Now Playing, the keyboard's media keys, AirPods. A Mac needs no audio session or background mode |
| `platform/contacts`, `platform/calendar` | `add` asks in an alert over the window (macOS has no new contact or event screen), then saves, asking for access first |
| `platform/photos` | PhotoKit, as on iOS; a Mac has no limited library: `manageLimited` does nothing |
| `platform/library` | Photos and videos go to Photos; `.downloads` and `.music` to the user's `~/Downloads` and `~/Music`, in the folder asked |
| `platform/biometric` | Touch ID's prompt, or the user's password (not with `noPasscode`) |
| `platform/push` | APNs, as on iOS. Needs a provisioning profile (above) |
| `platform/appleauth` | Apple's button and sheet, as on iOS. Needs a provisioning profile (above) |
| `platform/pay` | Apple Pay's sheet and button, as on iOS: a Mac with Touch ID pays, or one near the user's iPhone or Apple Watch. Needs a provisioning profile (above) |
| `platform/integrity` | App Attest, as on iOS. Needs a provisioning profile (above): without one `supported()` is false |
| `platform/background` | The task keeps App Nap from slowing the app; there's no notification, so `update` changes nothing |
| `platform/periodic` | `NSBackgroundActivityScheduler`: the tasks run while the app runs (it isn't launched for them); one that wants power waits while the Mac is on its battery |
| `platform/screen` | `lock`, `setBars` and `setBrightness` do nothing; `preventCapture` leaves the app's windows out of screenshots and recordings; `screenshots()` says nothing |
| `platform/appicon` | `set` changes the Dock's icon while the app runs, and again as it launches; the Finder keeps the app's own |
| `platform/transcribe` | On the device only; the app's bundle needs its purpose string, which `asuka build` writes |
| `platform/sensors` | A Mac has no motion sensors: `available` is false, and the streams and `steps` throw `SensorError.notAvailable` |
| `device.haptic` | The trackpad's feedback (a Force Touch trackpad): an alignment for `.selection`, a level change for the others |
| `platform/widget`, `platform/liveactivity`, `platform/agerange` | Not on a Mac: `widget.set` does nothing, `liveactivity.start` throws "liveactivity: not available", `agerange.request` `AgeRangeError.notAvailable` |
| `platform/purchase` | StoreKit 2, in the app's Swift library, which the build makes for an app that imports the package ([In-app purchases](purchases.md)) |
| `platform/icloud` | As on iOS, in the app's Swift library. Needs a provisioning profile with iCloud (above): without one, "icloud: not available" ([iCloud](icloud.md)) |
| `platform/splash` | A Mac app has no launch screen: `hold` and `hide` do nothing |
| `onShare`, shortcuts | iOS and Android only: a Mac app isn't offered in other apps' share menus, and `asuka.toml`'s `shortcuts` aren't added |

## On Windows

A Windows app declares nothing: `asuka build --platform windows` writes no
capabilities, and Windows asks a desktop app for no permission. The
privacy switches in Settings (Privacy & security) still hold for the
camera, the microphone and location: with one off, the camera and
recording throw their `notAllowed` errors, and location
`LocationError.denied`.

The packages work on Windows through its own services (Media Foundation,
WIC, WinRT, Windows Hello, SAPI, the Credential Manager), except where
Windows differs; where it has nothing, the package answers as on a
device without it:

| Package | On Windows |
| --- | --- |
| `platform/device` | `pickPhotos` and `pickFiles`: the open dialog over the window, copies in the app's caches. `browse` opens the page in the user's browser. The clipboard's images are PNGs. `keepAwake` keeps the display on. `info`: `model` `"PC"`, `system` `"Windows"`, `systemVersion` `"10.0.<build>"` |
| `platform/secure` | The user's generic credentials in the Credential Manager, named `<app>/<key>` (a value up to 2560 bytes) |
| `platform/camera` | Media Foundation's capture engine. `take()`: a dialog over the window with a live preview, Take Photo and Cancel (Enter and Escape). `camera.View` picks the camera Windows names front or rear, else the first; with `scan`, it reads codes in four frames a second with Asuka's own reader (as `scan.file` does). Settings' camera switch off: `CameraError.notAllowed` |
| `platform/record` (video) | H.264 and AAC in MPEG-4 (`.mp4`), the default microphone unless muted |
| `platform/audio` | MCI (DirectShow's playback); a URL is downloaded to the app's caches first |
| `platform/record` (sound) | The default microphone, AAC at 44.1 kHz (voice mono, music stereo); `background` changes nothing |
| `platform/nowplaying` | The app window's SystemMediaTransportControls (a hidden window of its own for an app without one): the volume flyout on Windows 10, quick settings' media controls on Windows 11, the lock screen, the keyboard's media keys. The skips are its fast-forward and rewind buttons; it has no toggle (the play/pause key sends play or pause) |
| `platform/speech` | SAPI's voices, those installed (Settings, Time & language, Speech) |
| `platform/network` | The Network List Manager: Wi-Fi, cellular or a cable; `metered` for a connection with a cost, `constrained` near or over its data limit |
| `platform/location` | WinRT's Geolocator. `placeName` asks OpenStreetMap's Nominatim (Windows's own geocoder needs a Bing Maps key): one request a second at most, answers kept in the app's caches. Regions: the `GeofenceMonitor`, while the app runs; background updates computed from the positions |
| `platform/biometric` | Windows Hello's prompt; `check()`'s kind is the sensor's; Hello always offers its PIN, and names its own cancel button |
| `platform/webauth` | A window over the app's with WebView2 in it (the app's web profile; `ephemeral`: a throwaway one) |
| `platform/js` | A WebView2 that's never shown |
| `platform/contacts`, `platform/calendar` | Windows gives the contacts and calendars to packaged apps only: `request` says `.denied`; `add` opens a vCard or an event file in the user's contacts or calendar app, and answers false (Windows doesn't say whether it was saved) |
| `platform/photos` | The Pictures folder (no permission to ask); screenshots are its Screenshots folder's |
| `platform/library` | The user's Downloads, Videos, Music and Pictures folders, in the folder asked |
| `platform/vision` | Windows's OCR, in the languages whose OCR is installed (Settings, Time & language), and its face detector; codes as `platform/scan` reads them |
| `platform/scan` | Windows has no reader of codes for apps: Asuka's own reads QR codes, Data Matrix, EAN-13 and EAN-8, UPC-A and UPC-E, Code 128 and Code 39 (not Aztec, PDF417, Code 93, Codabar nor ITF) |
| `platform/imageedit` | Windows's codecs read (HEIC and WebP where their extensions are installed); a video's frame through Media Foundation |
| `platform/maps` | Windows has no map control for desktop apps (its own needs a Bing Maps key): Asuka draws OpenStreetMap's tiles, downloaded two at a time and kept a week in the app's caches, with "© OpenStreetMap contributors" on the map; drag, the wheel or a pinch, double-click, + and -. `.satellite` and `.hybrid` show the streets (no free imagery), and `showsUser` shows nothing, as on Linux |
| `platform/background` | The task keeps the PC from sleeping; there's no notification, so `update` changes nothing |
| `platform/periodic` | Timers while the app runs (it isn't launched for them); one that wants power waits while the PC is on its battery |
| `platform/screen` | `lock`, `setBars` and `setBrightness` do nothing; `preventCapture` leaves the app's windows out of screenshots and recordings; `screenshots()` says nothing |
| `platform/appicon` | `set` changes the app's windows' and taskbar buttons' icon, and again as it launches; the Start menu and the executable keep the app's own |
| `platform/tracking` | No prompt: allowed while the advertising ID is on in Settings, else denied |
| `platform/push`, `platform/widget`, `platform/review`, `platform/integrity` | Packaged (Store) apps' or Apple's and Google's: `push.register` throws, `widget.set` does nothing, `review.request` shows nothing, `integrity.supported()` is false |
| `platform/liveactivity` | A phone's: `enabled` is false, `start` throws "liveactivity: not available" |
| `platform/purchase` | The Microsoft Store, for an app installed from it; otherwise `available()` is false and the rest throw `PurchaseError.notAvailable` ([In-app purchases](purchases.md)) |
| `platform/pay` | No wallet pays for a Windows app: `available` says `.none`, `pay` throws `PayError.notAvailable`, `PayButton` shows nothing |
| `platform/appleauth`, `platform/agerange`, `platform/icloud`, `platform/transcribe`, `platform/sensors` | Not on Windows: unavailable, as on a device without them |
| `device.haptic` | A PC has no haptics: it does nothing |
| `platform/splash` | A Windows app has no launch screen: `hold` and `hide` do nothing |
| `onShare`, shortcuts | iOS and Android only |

## On Linux

A Linux app declares nothing either. Where Asuka goes through an XDG
desktop portal (files, the camera, location, notifications, mail,
printing, opening files and links, keeping the system awake), the
portal asks the user as it does, and it works in a Flatpak sandbox as
out of one. The Flatpak manifest `asuka build --platform linux` writes
lets the app reach only the display, the GPU, the network and the
portals. Services Asuka reaches without a portal (Speech Dispatcher,
Evolution Data Server for contacts and calendars, NetworkManager for the
connection's type, iio-sensor-proxy, a Video4Linux camera) need the
`finish-args` that open them, added to the manifest; without them, the
package answers as on a system without the service. A notification due
later needs systemd outside the sandbox: in a Flatpak it shows only
while the app runs (see [On each platform](#on-each-platform)).

On Linux, Asuka reaches the desktop's services through the portals where
there's one, GLib and GTK, and the libraries a desktop has, each loaded
the first time it's used: a system without it answers an error that
names what's missing.

| Package | On Linux |
| --- | --- |
| `platform/device` | `pickPhotos` and `pickFiles`: GTK's file dialog (the portal's in a sandbox), copies in the app's caches. Clipboard images and changes through GDK's clipboard. `browse` opens the page in the user's browser. `keepAwake` keeps the session from idling. `info`: `model` `"PC"`, `system` `"Linux"`, `systemVersion` the distribution's (`/etc/os-release`) |
| `platform/secure` | The keyring (libsecret: GNOME Keyring, KWallet, KeePassXC), an item per key labeled with the app's name; without libsecret or a keyring, the calls fail saying so |
| `platform/network` | GNetworkMonitor (the portal's in a sandbox); Wi-Fi, ethernet or cellular from NetworkManager; never constrained |
| `platform/location` | The location portal (GeoClue's, which asks the user), else GeoClue itself; `placeName` with geocode-glib, when it's installed. GeoClue has no geofences: regions and background updates are computed from the positions while the app runs |
| `platform/speech` | Speech Dispatcher (started when it isn't running): its voices, rate, pitch, volume |
| `platform/audio`, `platform/record` | GStreamer: an AAC encoder (FDK, libav's...) records, the default source the microphone |
| `platform/nowplaying` | An MPRIS player on the session bus, named `org.mpris.MediaPlayer2.<app>.instance<pid>` while something shows: GNOME's and KDE's media controls and lock screens, the media keys, `playerctl`. MPRIS's `Seek` is a skip where the app skips, else a seek; the artwork is a `file://` or http(s) URL |
| `platform/library` | The user's folders (XDG's Pictures, Videos, Music, Downloads), in the folder asked |
| `device.shareFiles` | No share sheet: the files are copied to the clipboard, with a toast |
| `platform/camera`, `platform/record` | The camera portal (it asks the user once; PipeWire's cameras, `gstreamer1.0-pipewire`), else the first Video4Linux camera, through GStreamer. `take()`: a dialog over the window with a live preview, Take Photo and Cancel. `camera.View`: the preview, mirrored, filling its box; front and back are the same camera (a computer's face the user). Videos are MPEG-4 (H.264 from x264 or OpenH264, AAC), else WebM; scanning codes needs GStreamer's `zbar` element. No camera: `CameraError.noCamera`; refused: `.notAllowed` |
| `platform/scan`, `platform/vision` | Asuka's own reader reads codes, as on Windows: QR, Data Matrix, EAN, UPC, Code 128, Code 39 (no Aztec, PDF417, Code 93, Codabar or ITF). Tesseract (`libtesseract5` and a language's data) reads text; nothing finds faces: `faces` is empty. `vision.analyze` leaves out the text without Tesseract |
| `platform/imageedit` | GdkPixbuf's loaders decode (HEIF and WebP where their loaders are installed), upright; Asuka does the actions; video frames from GStreamer |
| `platform/webauth` | A dialog over the window with a WebKitGTK web view (`ephemeral`: a session of its own); closing it cancels |
| `platform/js` | A WebKitGTK web view that's never shown |
| `platform/maps` | libshumate's map of OpenStreetMap's tiles (`libshumate-1.0-1`); `.satellite` and `.hybrid` show the streets; no user location layer. Without libshumate, the view says so and reports `error` |
| `platform/background` | The task keeps the system from suspending; no notification, `update` changes nothing |
| `platform/periodic` | The tasks run on a timer while the app runs (it isn't launched for them); one that wants power waits while on the battery, one that wants the network while it's offline |
| `platform/screen` | `lock`, `setBars`, `preventCapture` and `setBrightness` do nothing; `screenshots()` says nothing |
| `platform/appicon` | `set` throws `notAvailable`: the launcher shows the desktop file's icon |
| `platform/sensors` | iio-sensor-proxy (convertibles, tablets): the light sensor, in lux, and the accelerometer's orientation as 1 g along the axis that's up, as it turns. No others: `available` is false, and the streams and `steps` throw `SensorError.notAvailable` |
| `platform/contacts`, `platform/calendar` | Evolution Data Server's address books and calendars (GNOME Contacts', GNOME Calendar's, the online accounts'): access is `.allowed` (calendars' `.full`) when it has one; `pick` is a dialog of the contacts with a search field; `add` asks, then saves in the default address book or the event's calendar. Without it: contacts' access is `.denied`, calendars' `.writeOnly`; `add` opens a vCard or an iCalendar file in the user's contacts or calendar app, and answers false; `contacts.pick` throws |
| `platform/photos`' library, `platform/appleauth`, `platform/transcribe`, `platform/widget`, `platform/liveactivity` | None on Linux: the library's access is `.denied`; `appleauth.available()` is false and its button shows nothing; `transcribe` throws `TranscribeError.notAvailable`, `liveactivity.start` "liveactivity: not available"; `widget.set` does nothing. `platform/splash` has nothing to hold |
| `platform/purchase` | No store: `available()` is false, the rest throw `PurchaseError.notAvailable` |
| `platform/icloud` | No iCloud: "icloud: not available" |
| `platform/biometric`, `platform/integrity`, `platform/agerange`, `platform/push` | None on Linux: not available, not supported. `platform/review` does nothing; `platform/tracking` answers `.allowed` |
| `platform/pay` | No wallet pays for a Linux app: `available` says `.none`, `pay` throws `PayError.notAvailable`, `PayButton` shows nothing |
| `platform/crash` | Asuka's crash output, which has a crash in native code too (a signal in GTK's code, a GLib abort), reported as `"native"`; the system's own record (systemd-coredump, apport) isn't read |
| `device.haptic` | A desktop has no haptics: it does nothing |
| `onShare`, shortcuts | iOS and Android only |

## In a browser

A page declares nothing: the browser asks the user the first time a
package needs the camera, the microphone, location or notifications.
Most of these APIs need a secure page, served over `https` or from
`localhost` (`asuka dev --platform web`'s, but not `--lan`'s addresses).
The packages call the web's APIs; [the web](web.md) has the details,
and its service worker for notifications and Web Push.

| Package | In a browser |
| --- | --- |
| `platform/device` | `pickPhotos` and `pickFiles`: the browser's file picker, copies in the caches. `browse`: a new tab. The clipboard: the app's own copies, and other apps' where the user lets the page read it. `keepAwake`: the Screen Wake Lock. `info` from the user agent (Chromium's client hints give the system's version and an Android phone's model) |
| `platform/secure` | The values encrypted (AES-GCM) in IndexedDB, with a key the browser won't let out; not a keychain (see [the web](web.md)) |
| `platform/camera`, `platform/record` | `take()` opens a phone's camera, or a dialog with the live camera and a shutter; `camera.View` is the camera in a `<video>`, with `capture()`, recording, and scanning codes where the browser has `BarcodeDetector` |
| `platform/scan`, `platform/vision` | `BarcodeDetector` reads codes; faces and text only where the browser has them |
| `platform/imageedit` | A canvas: JPEG or PNG; a remote image needs CORS |
| `platform/photos` | A page can't read the library: access is `.denied` (`device.pickPhotos` picks photos) |
| `platform/library` | `save` downloads the file |
| `platform/location` | The Geolocation API; `placeName` isn't on the web (a browser has no geocoder). Regions and background updates are computed from the positions, only while the page is open |
| `platform/network` | Online from `navigator.onLine`; Wi-Fi, cellular or Ethernet where the browser says (Chrome on Android), metered for cellular, constrained for Data Saver |
| `platform/notify`, `platform/push` | The Notification API and Web Push, through the app's service worker (`push.register`'s `service` is `"webpush"`); a notification due later can't show while no page of the app is open |
| `platform/audio`, `platform/record` | An audio element per player (the browser's autoplay rules apply: play from a tap); `MediaRecorder`, AAC in MPEG-4 where the browser records it, else Opus in WebM |
| `platform/nowplaying` | The Media Session API (`navigator.mediaSession`): the browser's media controls, the media keys, a phone's lock screen, shown while the page plays sound. It has no toggle (the key sends play or pause), and a browser without an action (stop, in some) leaves it out |
| `platform/speech` | `speechSynthesis`, the browser's voices |
| `platform/sensors` | The Generic Sensor API (Chrome) or device motion and orientation events (Safari asks on a tap), in Android's axes and units; no barometer or pedometer |
| `platform/biometric` | The platform's authenticator through WebAuthn (Touch ID, Face ID, Windows Hello, Android's screen lock): the first `authenticate` makes a passkey for the site. The prompt is the browser's: `reason` and `cancel` don't show |
| `platform/webauth` | A popup, done when it comes back to a page of the app's own origin (a custom scheme can't open a page) |
| `platform/js` | A hidden, sandboxed frame |
| `platform/contacts`, `platform/calendar` | `contacts.pick` where the browser has the Contact Picker (Chrome on Android); `add` downloads a vCard or an `.ics` file; reading them isn't on the web |
| `platform/screen` | `lock` only full screen or in an installed app; hiding the bars is full screen; no `preventCapture`, `screenshots()` or `setBrightness` |
| `platform/appicon` | `set` changes the page's icon (its tab's), and again on the next visit; an installed web app keeps the manifest's |
| `platform/background` | The page runs while it's open; a task adds nothing, and has no notification |
| `platform/tracking` | No prompt: `.denied` when the browser sends Global Privacy Control or Do Not Track, else `.allowed` |
| `device.haptic` | `navigator.vibrate`, where the browser has it (Chrome on Android) |
| `platform/review`, `platform/integrity`, `platform/agerange`, `platform/appleauth`, `platform/icloud`, `platform/periodic`, `platform/widget`, `platform/liveactivity` | Not on the web: an error that says so, or the package's `notAvailable` |
| `platform/purchase` | Google Play through the Digital Goods API, in a Trusted Web Activity; elsewhere no store (`PurchaseError.notAvailable`) |
| `platform/maps` | OpenStreetMap's tiles, drawn by Asuka, as on Windows; `showsUser` once the page has the location permission |
| `platform/pay` | Apple Pay where the browser has it (Safari), through the Payment Request API, the merchant validated by the app's server (`onValidateMerchant`); Google Pay elsewhere, through Google's web API (pay.js); each one's own button. A sheet shows only from a tap |
| `platform/crash` | Asuka's fatal errors, and uncaught JavaScript errors as `"exception"` reports that aren't fatal |
| `platform/splash` | `hold` and `hide` do nothing |
| `onShare`, shortcuts | iOS and Android only |

## The camera: platform/camera

`camera.View` shows what the camera sees, live: AVFoundation's preview
on iOS and the Mac, CameraX on Android, `getUserMedia` in a browser,
Media Foundation on Windows, GStreamer on Linux. `camera.capture()` takes
a photo from the view on screen; `camera.take()` shows the system's
camera screen instead (iOS's, Android's camera app; a sheet with a
preview on the desktop, the file picker's camera in a browser) and gives
the photo's file, or `none` if the user cancelled. Photos are JPEGs in
the app's caches: move one to keep it.

```asuka
import platform/camera

component Shutter {
  state facing: camera.Facing = .back
  state last = ""
  state status = ""

  Column {
    camera.View {
      facing: facing
      grow: 1
      on error(m) { status = m }
    }
    Row {
      gap: 12
      Button("Flip") { on press { facing = if facing == .back { .front } else { .back } } }
      Button("Take") { on press { last = try await camera.capture() } }
    }
    Text(status)
  }
}
```

The view asks for the camera the first time it shows; refused, or on a
device without one (the iOS simulator), it reports `error`, and
`capture` throws `CameraError.notAllowed` or `.noCamera` (`.noView` when
no `camera.View` is on screen). An app that imports `platform/camera` is
given the camera's and the microphone's purpose strings on iOS (set your
own in asuka.toml's `[ios.usage]`) and the `CAMERA` and `RECORD_AUDIO`
permissions on Android.

## Recording video: platform/record

A `camera.View` in video mode records: `record.start()` records what it
shows, with the microphone unless `mute`, until `record.stop()` (or
`maxDuration`), and gives the file, an MPEG-4 video in the app's caches.
`camera.record` and `camera.stopRecording` are the same.

```asuka
import platform/camera
import platform/record

component Clip {
  state recording = false
  state file = ""

  Column {
    camera.View { mode: .video; grow: 1 }
    Button(if recording { "Stop" } else { "Record" }) {
      on press {
        if recording {
          record.stop()
        } else {
          recording = true
          defer { recording = false }
          file = try await record.start(maxDuration: 30s)
        }
      }
    }
  }
}
```

AVCaptureMovieFileOutput on iOS and the Mac, CameraX's `VideoCapture` on
Android, `MediaRecorder` in a browser (WebM where it can't write MPEG-4),
Media Foundation's sink writer on Windows, GStreamer on Linux.

## Barcodes and QR codes: platform/scan

A `camera.View` with `scan` on reads the codes it sees, live, and reports
each with `code` (once while it stays in sight). `codes` limits the kinds
it looks for; none: every kind. Kinds are named `"qr"`, `"aztec"`,
`"datamatrix"`, `"pdf417"`, `"ean13"`, `"ean8"`, `"upca"`, `"upce"`,
`"code128"`, `"code39"`, `"code93"`, `"codabar"` and `"itf"`.

`scan.file(path)` reads the codes in an image file instead (a photo
taken or picked): Vision on iOS and the Mac, ML Kit on Android (its
model in the app, about 2 MB, so it works without Google Play services),
the browser's `BarcodeDetector` where it has one, and Asuka's own reader
on Windows and Linux, which have none an app can count on (QR, Data
Matrix, EAN, UPC, Code 128, Code 39).

```asuka
import platform/camera
import platform/scan

component Scanner {
  state found = ""

  Column {
    camera.View {
      scan: true
      codes: ["qr", "ean13"]
      grow: 1
      on code(c) { found = "{c.kind}: {c.value}" }
    }
    Text(found)
  }
}

async fn read(photo: string) throws -> [string] {
  var out: [string] = []
  for c in try await scan.file(photo, kinds: ["qr"]) { out.push(c.value) }
  return out
}
```

## What's in an image: platform/vision

`vision.analyze(path)` finds what's in an image, on the device: its text,
line by line and word by word, faces, and barcodes, each with its `Box`
(fractions of the image's width and height from its top left, the image
upright: multiply by a view's size for points). Ask only for what you
need (`text:`, `faces:`, `codes:`), and give the text's languages
(`languages: ["ja"]`) when you know them. Nothing leaves the device.

```asuka
import platform/vision

async fn words(photo: string) throws -> [string] {
  let r = try await vision.analyze(photo, faces: false, codes: false)
  return r.lines.map(l => l.text)
}
```

The Vision framework on iOS and the Mac, ML Kit and its bundled models
on Android (about 30 MB), the browser's Shape Detection API where it has
one, Windows' OCR and face detection, and Tesseract (where it's
installed) on Linux; codes on Windows and Linux are Asuka's own reader's.
Text and faces are each platform's own recognition (there's no reader of
them every platform could share), so what's found can differ from one
platform to another. `platform/scan` reads a code's value; this package
says where it is.

## The photo library: platform/photos

`platform/photos` reads the user's library, with their permission: PhotoKit on iOS
and the Mac, MediaStore on Android, the Pictures folder on Windows. Linux
and the web have no library an app can read: access is `.denied`. It's for an app whose screen is the library. To let the
user pick a few photos, `device.pickPhotos` asks for no permission: prefer it
where it'll do. Google Play asks an app that reads the library why it does, in
the Play Console. Every call is async.

```asuka
import platform/photos

component Screenshots {
  state thumbs: [string] = []

  task {
    if (try? await photos.request()) == .denied { return }
    let assets = (try? await photos.list(screenshots: true, limit: 60)) ?? []
    for a in assets {
      thumbs.push((try? await photos.thumbnail(a.id, size: 300)) ?? "")   // a file, for Image
    }
  }

  Scroll {
    for (i, t) in thumbs.enumerated() key i { Image(t) { width: 100; height: 100 } }
  }
}

async fn original(a: photos.Asset) throws -> string {
  let f = try await photos.export(a.id)   // the original, as a file
  return f.path
}
```

- `photos.current()` says what's allowed without asking: `.notAsked`, `.denied`,
  `.limited` (only the photos the user chose: iOS 14, Android 14) or `.allowed`.
  `photos.request()` asks the first time. `photos.manageLimited()` lets a
  `.limited` user change which photos, and returns once they're done: list
  again then.
- `photos.list(screenshots:, offset:, limit:)` is newest first, from
  `offset`, at most `limit` (100 by default). An `Asset` has its `id`,
  `width`, `height`, `created` (a `time.Time`) and `screenshot`.
  `screenshots: true` keeps only those: iOS knows; on Android it's what's
  in a Screenshots folder.
- `photos.thumbnail(id, size:)` is a JPEG file at least `size` pixels on
  its short side, kept in the app's caches for the next time.
- `photos.export(id)` copies the full image as it is in the library (HEIC,
  JPEG, PNG) to a file: its `path`, and its `name` in the library. iCloud
  photos download first.

## Photos, the browser, the clipboard: platform/device

`platform/device` is what the platform offers beyond views.

```asuka
import platform/device

component Gallery {
  state photos: [string] = []

  Column {
    Button("Pick photos") {
      on press { photos = (try? await device.pickPhotos(max: 3)) ?? [] }
    }
    Button("Help") {
      on press { try? await device.browse("https://example.com/help") }
    }
    for p in photos key p { Image(p) }
  }
}
```

- `device.pickPhotos(max:)` shows the system's photo picker
  (PHPicker on iOS, the Photo Picker on Android; the open dialog on the
  desktops, the file picker in a browser) for up to `max` images
  (1 to 255). It asks for no permission: the user picks, and the app sees
  only what was picked. It returns the images' files, which `Image`
  shows, or none if the user cancelled. The files are copies in the
  app's temporary files or caches: move one to keep it.
- `device.browse(url)` shows a web page in the app (SFSafariViewController,
  a Custom Tab), over the app's screens. The URL must be http or https.
  `device.openURL` leaves the app for the browser instead. A desktop opens
  the page in the user's browser, and a browser in a new tab.
- `device.clipboardText()` reads the clipboard's text, `none` if it has
  none. iOS asks the user the first time the app reads what another app
  put there. `device.copy(text)` writes it.
- `device.keepAwake(on)` keeps the screen on while `on` is true: a video,
  a recipe, a map. Turn it off when the screen that wants it goes.
- `device.pickFiles(types:, multiple:)` shows the system's document
  picker (iOS's Files browser, Android's storage picker, the open dialog
  on the desktops, the file picker in a browser; no permission). `types`
  are MIME types (`"application/pdf"`, `"image/*"`; none: any file) and
  `multiple` lets the user pick several. It returns a `device.File` for
  each, its `path` a copy in the app's caches, with its `name`, `type`
  and `size`, or none if the user cancelled.
- `device.clipboardHas()` says whether the clipboard holds `text`, an
  `image` or a `url`, without reading it (iOS doesn't ask the user for
  this): show a Paste button only when there's something to paste.
  `device.clipboardImage()` reads its image into a file, in the format it
  was copied in; `device.copyImage(path)` puts an image file on it.
  `device.clipboardChanges()` is a stream of what it holds at each change
  (iOS and Android say; the Mac and Windows are asked twice a second; a
  browser sees copies in the page, and others' where the user lets it
  read the clipboard).
- `device.info()` is the device and the app: `model` ("iPhone", "Pixel
  9", "Mac", "PC"), `system` ("iOS", "Android", "macOS", "Windows",
  "Linux"), `systemVersion`, and `appVersion` and `build`,
  `asuka.toml`'s `version` and `build`. As expo-application says,
  `source` is where the app came from (`.appStore`, `.testFlight`,
  `.playStore`, `.otherStore` such as the Microsoft Store or Flathub,
  `.direct` for a download or a sideloaded APK, `.development` for a
  developer's build), and `installedAt` and `updatedAt` are when it was
  installed and when this version was (none on the web, which reports
  `.unknown`): to show a beta's banner, or point a downloaded copy at
  the store.

```asuka
import platform/device

component Player {
  let info = load device.info()

  task {
    device.keepAwake(true)
    defer { device.keepAwake(false) }
    await sleep(1h)
  }

  Text(if let i = info.value { "{i.model}, {i.system} {i.systemVersion}" } else { "" })
}
```

## Signing in on the web: platform/webauth

`webauth.open(url)` shows a sign-in page in the system's own sign-in
sheet: `ASWebAuthenticationSession` on iOS, a Custom Tab on Android.
It's the browser's, so the user's sessions, passkeys and password
autofill work in it, and providers that refuse an app's own `WebView`
(Google's sign-in) don't refuse it. Use it for OAuth rather than a
`WebView`.

The Mac has `ASWebAuthenticationSession` too. Windows and Linux have no
system sheet: the page shows in a web view of the app's over its window
(WebView2, WebKitGTK), which has the app's web profile but not the
user's browser's sessions, and which a provider that refuses embedded
web views refuses there. A browser opens the page in a popup, done when
it comes back to a page of the app's own origin (a custom scheme can't
open a page).

```asuka
import platform/webauth

async fn signIn(authorizeURL: string) throws -> string? {
  do {
    // myapp://…?code=…: exchange the code.
    return try await webauth.open(authorizeURL, callbackScheme: "myapp")
  } catch webauth.WebAuthError.canceled {
    return none // the user closed it
  }
}
```

It ends in one of two ways:

- **A callback scheme.** When the page goes to a URL of
  `callbackScheme` (the app's own), the sheet closes and `open` returns
  the URL. On Android the scheme must be the app's, asuka.toml's
  `scheme`; it goes to `open` rather than the app's `openURL` event.
- **The app ends it.** Without a scheme, the page redirects somewhere
  the app hears from: an OAuth server that allows a redirect to the
  loopback interface (`http://localhost:port/callback`, RFC 8252) sends
  the code to a server the app runs on that port; racing the two
  (`race(server.next(), webauth.open(url))`) cancels `open`, which
  closes the sheet. On Android, cancelling brings the app back over the
  tab.

`open` throws `webauth.WebAuthError.canceled` when the user closes the
sheet. `ephemeral: true` asks for a private session, without the
browser's cookies (iOS's `prefersEphemeralWebBrowserSession`; Android's
ephemeral Custom Tab where the browser has one; a throwaway profile on
Windows and Linux).

## Copy, share and open: device.copy, device.share, device.openURL

Three of `platform/device`'s functions hand text to the system.

```asuka
import platform/device

component Links {
  Column {
    Button("Copy link") { on press { device.copy("https://example.com/s/42") } }
    Button("Share") { on press { device.share("https://example.com/s/42") } }
    Button("Open in browser") { on press { device.openURL("https://example.com") } }
    Button("Call") { on press { device.openURL("tel:+15555550100") } }
  }
}
```

- `device.copy(text)` puts text on the clipboard.
- `device.share(text)` shows the system's share sheet, to send it to an app, a
  contact, AirDrop or Nearby Share. A URL is shared as a link. The Mac
  shows its sharing services picker, Windows its share UI, a browser
  the Web Share sheet where it has one; Linux has none, so the text is
  copied to the clipboard, with a toast (as a browser without one
  copies it).
- `device.openURL(url)` asks the system to open a URL: a web address in the
  browser, another scheme (`tel:`, `mailto:`, another app's) in the app
  that handles it.

`device.shareFiles(paths, mime:, uti:, title:, from:)` shows the share sheet for
files: a video to send, a PDF to save to Files, photos for AirDrop. It's
async, and returns once the sheet is gone, whether the user shared or
not; it throws when the sheet couldn't show (a file that isn't there).

```asuka
import platform/device

component ShareReport(report: string) {
  state status = ""
  let button = Ref()

  Column {
    Button("Share") {
      ref: button
      on press {
        do {
          try await device.shareFiles([report], mime: "application/pdf", title: "Send the report", from: button)
        } catch e {
          status = e.message
        }
      }
    }
    Text(status)
  }
}
```

Its options are expo-sharing's:

| Option | |
| --- | --- |
| `mime` | The files' type, which the apps on the sheet go by. Android sets the intent's type to it. On iOS it's the type when `uti` is empty. Default: from the first file's extension. |
| `uti` | iOS: the files' Uniform Type Identifier (`com.adobe.pdf`), ahead of `mime`. |
| `title` | The sheet's title: Android's chooser, and a mail's subject on iOS. |
| `from` | A `Ref` of the view the sheet points at where it's a popover (iPad, the Mac), usually the button that shares. Default: the screen's middle. |

On iOS a file whose extension doesn't match its type is shared under a
name that does, as expo-sharing does: `image.pdf` shared as a JPEG goes
as `image.pdf.jpeg`, a link to the file, not a copy. On Android the files
go through the app's FileProvider; one outside the app's own folders is
copied to its caches first. On the Mac it's the sharing services
picker. On Windows it's the share UI (`device.shareFiles` returns once it
shows: Windows doesn't say when it's gone).
Linux has none: the files go on the clipboard, as a shared text does. A
browser shares the files with the Web Share sheet where it can, else
downloads each.

To be opened with URLs yourself, see `asuka.toml`'s `scheme` and `links` in
[Shipping](shipping.md#asukatoml), and the prelude's `onOpenURL` (or the
app block's `on openURL(url)`).

## Previewing files: platform/preview

`preview.show(sources)` shows files with the system's viewer, as a mail
app shows an attachment: PDFs, images, video, audio, Office and iWork
documents, text. A source is a file's path, an asset's (as `Image`
takes it), or an http(s) URL, which is downloaded to the app's caches
first, under its name (or with the extension of the type the server
gives).

```asuka
import platform/preview

component Invoice {
  prop path: string
  state status = ""

  Column {
    Button("Open the invoice") {
      on press {
        do {
          try await preview.show([path, "docs/terms.pdf"])
        } catch preview.PreviewError.noViewer {
          status = "Nothing on this device opens it"
        } catch e {
          status = e.message
        }
      }
    }
    Text(status)
  }
}
```

`start:` is the index of the file shown first; `edit: true` lets the
user mark up PDFs and images with Markup, which saves into the files
(iOS). It returns once the viewer is gone, and throws
`preview.PreviewError.noViewer` if nothing can show the files, or another
error (no such file, a download that failed).

| | Viewer | Returns |
| --- | --- | --- |
| iOS | Quick Look (`QLPreviewController`) over the screen: swipe between the files; share, Markup, search | when it's dismissed |
| Android | `ACTION_VIEW` of the `start` file in the app the user picks for its type (Drive's PDF viewer, Photos), through the app's FileProvider with the right to read it alone | when the user is back |
| macOS | Quick Look's panel (`QLPreviewPanel`), its arrows between the files | when the panel closes |
| Windows | the file's default app (`ShellExecuteEx`), or the "How do you want to open this?" dialog | once it's handed over |
| Linux | the file's default app (`GtkFileLauncher`, the portal's in a sandbox) | once it's handed over |
| Web | a new tab, for what a browser shows (images, video, audio, text, PDF, HTML); another type is `noViewer` | once it's opened |

Android has no system viewer, nor one that pages through several files:
it opens one, as Android's mail apps do with each attachment.
`preview.show` doesn't take a view for the viewer to grow from on iOS
(a `Ref`, as `device.shareFiles`'s `from:`) yet.

## Printing and PDFs: platform/print

`platform/print` shows the system's print dialog, with its printers and
options (copies, pages, paper, orientation; Save as PDF on Android and the
Mac), for a file or HTML markup; and lays HTML out on pages as a PDF.
The calls that print return a `print.Outcome` once the dialog is closed. The package's
name is the prelude's `print` too: import it under another name to print
text as well.

```asuka
import platform/print as printing

async fn printReceipt(receipt: string) throws -> string {
  let r = try await printing.html(receipt, job: "Receipt")
  return if r == .printed { "Sent to the printer" } else { "" }
}
```

- `print.file(source, job:, orientation:)` prints a PDF or an image (a
  path, an asset, an http(s) URL, as `preview.show` takes them). Another
  type throws `print.PrintError.unsupported`, except on Windows (where
  the file's app prints it) and Linux (where the printer takes what CUPS
  can print).
- `print.html(markup, job:, orientation:)` prints markup, paginated as a
  browser prints a page: CSS's `@media print` and `break-before` apply.
  Images and styles come from absolute URLs (`https:`, `data:`).
- `print.webView(view, job:, orientation:)` prints the page a `WebView`
  shows, as it is now (what the user and its scripts did on it),
  paginated as its browser prints a page: `view` is the `Ref` the
  WebView is in (`ref:`). A Ref that holds no WebView the app shows
  throws `print.PrintError.unsupported`.

```asuka
import platform/print as printing

let page = Ref()

component Article(url: string) {
  Screen {
    title: "Article"
    toolbar {
      Button("Print") {
        icon: "printer"
        on press { try? await printing.webView(page, job: "Article") }
      }
    }
    WebView(url: url) {
      ref: page
      grow: 1
    }
  }
}
```

`job` is the job's name in the queue (default: the file's name, the
page's title, else the app's name), and `orientation` the one the dialog
starts with (`.portrait`, `.landscape`).

An `Outcome` is `.printed` (the job is in the printer's queue, or saved
as a PDF from the dialog), `.cancelled`, or `.unknown` where the platform
doesn't say. A job that fails is an error. `print.PrintError.unavailable`
says the device can't print.

| | Dialog | File | HTML | Outcome |
| --- | --- | --- | --- | --- |
| iOS | `UIPrintInteractionController`, a popover on the iPad | PDFs and images (`printingItem`) | WebKit's print formatter | printed or cancelled |
| Android | `PrintManager`'s print screen | a PDF written through; an image fit to the page, upright (as `PrintHelper`) | the WebView's print adapter | printed, cancelled or failed (the job's state) |
| macOS | `NSPrintOperation`'s panel, a sheet on the window, with a preview | PDFKit (a PDF; an image on a page) | WebKit's print operation | printed or cancelled |
| Windows | WebView2's: the system's print dialog | the file's app prints it (the shell's `print` verb), whatever its type | WebView2 | unknown |
| Linux | GTK's print dialog (the print portal's for a file) | the printer takes it (CUPS: PDFs, images) | WebKitGTK's print operation | printed or cancelled |
| Web | the browser's print dialog | PDFs in the browser's viewer, images | a hidden frame | unknown |

An image wider than tall starts in landscape on Android and iOS, as their
photo printing does. A WebView's page prints with the WebView's own
printing: its print formatter on iOS, its print adapter on Android,
WebKit's print operation on the Mac and on Linux, WebView2's print
dialog on Windows. On the web, a page of the app's own origin (an
`html:` string's) prints whole from its frame; another site's page
can't be reached from the app's, so the app's page prints with only the
WebView on it, as it shows. `platform/print` doesn't yet take a view for
the iPad's popover to point at.

### PDFs from HTML

`print.pdf(markup, page:, landscape:, margins:)` lays markup out on
pages, as `print.html` prints it but without a dialog, and writes a PDF
in the app's caches, named after the page's `<title>` ("Receipt.pdf";
"Document.pdf" without one). It returns its path, to share, mail or
move. Its text stays text: searchable, selectable.

```asuka
import platform/print as printing
import platform/preview

async fn receiptPDF(receipt: string) throws {
  let path = try await printing.pdf(receipt, page: printing.Page.a4, margins: printing.Margins(top: 48, right: 48, bottom: 48, left: 48))
  try await preview.show([path])
}
```

| Parameter | |
| --- | --- |
| `page` | The paper, in points: `Page.a3`, `a4`, `a5`, `letter`, `legal`, or a size of your own. Default: the user's region's (`print.regionPage(environment().locale)`), Letter in the Americas that use it (CLDR's paper sizes), A4 elsewhere. |
| `landscape` | Turns the page: its width the longer side. |
| `margins` | The space around each page's content, in points. Default: half an inch (36 points) on each side. |

iOS lays it out with WebKit (`UIPrintPageRenderer` with the web view's
print formatter), Android with its WebView's print adapter, the Mac with
WebKit's print operation, Windows with WebView2 (Chromium's
`Page.printToPDF`), Linux with WebKitGTK (printed to a file). A browser
makes PDFs only through its print dialog: there `pdf` throws an
`unsupported` error, and `print.html` lets the user Save as PDF.

## Mail and messages: platform/compose

`platform/compose` opens the system's mail and message composers, filled in
for the user to send, and answers a `compose.Result` once the user is
done: `.sent`, `.saved` (a draft), `.cancelled`, or `.unknown` where
another app has the mail and doesn't say.

```asuka
import platform/compose

async fn mailReceipt(receiptPDF: string) -> string {
  do {
    let r = try await compose.mail(
      to: ["support@example.com"],
      subject: "Order 1042",
      body: "<p>Hello,</p><p>The receipt is attached.</p>",
      html: true,
      attachments: [compose.Attachment(source: receiptPDF, name: "Receipt 1042.pdf")],
    )
    return if r == .sent { "Sent" } else { "" }
  } catch e {
    return if e.message == "compose: unavailable" { "Set up a mail account to send it" } else { e.message }
  }
}

async fn onMyWay() throws {
  let _ = try await compose.message(to: ["+15555550100"], body: "On my way")
}
```

`compose.mail` takes `to`, `cc` and `bcc` addresses, a `subject`, a `body`
(markup if `html`) and `attachments`; `compose.message` takes `to` (phone
numbers, or iMessage addresses), a `subject` (where Messages has subjects
on; elsewhere it goes before the body), a `body` and `attachments`. An
`Attachment` is a `source` (a path, an asset, an http(s) URL), its `type`
(default: its extension's) and the `name` the recipient sees (default:
the file's). Where a mail can't carry markup (a `mailto:` link, Simple
MAPI), its body goes as text, as `compose.plainText(html)` makes it.

`compose.check()` says what the device can do: `mail`, a mail account or
app; `mailComposer`, iOS's own composer, which says what the user did and
takes attachments; `text`, text messages; and `textAttachments`,
messages with attachments. A call the device can't do throws
"compose: unavailable".

| | Mail | Messages | Result |
| --- | --- | --- | --- |
| iOS | Mail's composer (`MFMailComposeViewController`) over the app; without a Mail account, a `mailto:` link for the mail app the user set (without attachments) | Messages' composer (`MFMessageComposeViewController`) | sent, saved, cancelled; unknown for a link |
| Android | the mail apps only: `ACTION_SENDTO` `mailto:`, or with attachments `ACTION_SEND` with `mailto:` as its selector, the files through the app's FileProvider; markup as spans (`EXTRA_TEXT`) and as `EXTRA_HTML_TEXT` | the default SMS app: `smsto:` with `sms_body`, or with attachments `ACTION_SEND` | unknown, once the user is back |
| macOS | a `mailto:` link for the user's mail app; with attachments, the mail share service (`NSSharingService`), which takes no CC or BCC (they throw "compose: unavailable") | Messages (`NSSharingService`) | unknown, or cancelled |
| Windows | a `mailto:` link; with attachments, Simple MAPI (`MAPISendMailW`: Outlook, Thunderbird; the new Outlook and Windows's Mail app don't take it: unavailable) | unavailable | unknown, or cancelled (MAPI) |
| Linux | the email portal (the mail app's composer, with the attachments), else `xdg-email`, else a `mailto:` link | unavailable | unknown, or cancelled (the portal) |
| Web | a `mailto:` link; with attachments, the Web Share sheet | an `sms:` link; with attachments, the Web Share sheet | unknown |

The iOS simulator has no Mail account and no Messages: try the composers
on a device.

## Location: platform/location

`platform/location` is where the device is, now and as it moves, with the
user's permission, which it asks for the first time. It's Core Location
on iOS and the Mac, and Android's location manager, which needs no Google
Play services; WinRT's Geolocator on Windows, the location portal (or
GeoClue) on Linux, and the Geolocation API in a browser. With the app out
of sight too, it follows the device in the background and watches regions
for the app, below.

`location.current()` asks for permission if it hasn't, then returns where
the device is. If the user hasn't allowed it, it throws
`LocationError.denied`.

```asuka
import platform/location

component Where {
  state where = ""

  Column {
    Button("Where am I?") {
      on press {
        do {
          let p = try await location.current()
          where = "{p.latitude.fixed(4)}, {p.longitude.fixed(4)} (within {p.accuracy.fixed(0)} m)"
        } catch location.LocationError.denied {
          where = "Allow location in Settings"
        } catch e {
          where = e.message
        }
      }
    }
    Text(where)
  }
}
```

`location.positions()` is a stream: the position now, then each as the
device moves. Read it in a task, and it stops with the component:

```asuka
import platform/location

component Track {
  state here = location.Position()

  task {
    do {
      for await p in location.positions() { here = p }
    } catch e {
      print(e.message)
    }
  }

  Text("{here.latitude.fixed(5)}, {here.longitude.fixed(5)}")
}
```

`location.placeName(latitude, longitude)` is what a place is called
("Mission District, San Francisco"), from the platform's geocoder, which
asks its servers: ask only when the user wants the name. Linux asks
geocode-glib where it's installed; Windows and a browser have none, and
it throws there.

A `location.Position` has `latitude`, `longitude`, `accuracy` (meters:
the radius the device is within), `altitude` (meters above sea level; 0 if
not known), `speed` and `course` (negative when not known) and `time`
(a `time.Time`). A crossing has its `region`, `kind`, `at` (the
position, when the system says) and `time`.

### Permission levels

The user allows the location while the app is in use (`.whenInUse`), or
always (`.always`), which the regions need, and the app's events with the
app not running. They also choose precise positions or approximate ones
(within a few kilometers: iOS's Precise Location, Android's
Approximate). `location.authorization()` is what they allowed, tracked:
read in a binding, it follows the changes they make in Settings.

```asuka
import platform/location

component Allowed {
  let auth = location.authorization()

  Column {
    Text("Allowed: {auth.level}; precise: {auth.precise}{if auth.off { "; location services are off" } else { "" }}")
    Button("Allow always") { on press { let _ = try? await location.requestAlways() } }
    Button("Location settings") { on press { try? await location.openSettings() } }
  }
}
```

- `location.request()` asks for `.whenInUse`, the first time; asking
  again after `.denied` shows nothing: only Settings changes it
  (`location.openSettings()`).
- `location.requestAlways()` asks for `.always` (after `.whenInUse`).
  iOS asks once; Android 11 and later take the user to the app's location
  settings, to choose "Allow all the time".
- `location.requestPrecise(purpose)` asks the user who allowed only
  approximate positions for precise ones: on iOS for now (`purpose` is a
  key of asuka.toml's `ios.locationPurposes`), on Android for good.

A desktop app has `.always` once allowed: it runs out of sight anyway.
Linux and a browser say nothing before the app asks for a position, so
they're `.notAsked` until one comes.

### Updates in the background

`location.startUpdates(…)` starts updates that go on with the app out of
sight, and across launches (on iOS and Android the system starts the app
for them), until `location.stopUpdates()`. `location.updates()` is their
stream, of `Batch`es (`positions`, oldest first): read it as the app
starts, so the batches the system started the app for find a reader.

```asuka
import platform/location

async fn track() throws {
  try await location.startUpdates(
    accuracy: .high,
    distance: 20,
    activity: .fitness,
    delay: 30s,
    indicator: true,
    notification: location.Notification(title: "Tracking your route", text: "Your run is being recorded"),
  )
}

async fn followUpdates() throws {
  for await b in location.updates() {
    let done = b.hold()   // keeps the app running while it's sent
    defer { done() }
    print("{b.positions.count()} positions")
  }
}
```

`accuracy` trades exactness for power (`.high`, `.navigation`,
`.balanced`, `.low`, `.passive`); `distance` is how far the device moves
between two updates; `interval` how often they come (Android's);
`activity` tunes iOS's updates (and its pausing, with `autoPause`);
`delay` lets updates come together, so the app wakes less; `significant`
gets only the significant changes (about 500 m), at almost no power, and
on iOS they start the app even after the user ended it. `indicator` shows
iOS's blue pill while the app gets updates out of sight. On Android,
`notification` runs a foreground service of the location type, whose
notification it is: with it, updates come at their rate with the app out
of sight. `location.updating()` says whether they run (they may have been
started at an earlier launch).

On a desktop and in a browser, the updates are the positions the app
watches while it runs.

### Regions

`location.monitor(region)` watches a circle for the app, until
`location.stopMonitoring(id)`: its transitions come through
`location.crossings()`, with the app in the background or not running
(iOS and Android start it). If the device is inside as monitoring starts,
an `.enter` comes. It needs `.always` on iOS and Android
(`LocationError.needsAlways`); past the system's limit it throws
`tooManyRegions`.

```asuka
import platform/location

async fn watchHere() throws {
  let p = try await location.current()
  try await location.monitor(location.Region(id: "here", latitude: p.latitude, longitude: p.longitude, radius: 150, events: [.enter, .exit]))
}

async fn followCrossings() throws {
  for await c in location.crossings() {
    print("{c.kind} {c.region}")
  }
}
```

`location.supported()` says what the system does itself: `regions` (iOS,
Android, macOS and Windows monitor them; on Linux and in a browser this
package computes them from the positions while the app runs),
`maxRegions` (iOS and macOS 20), `dwell` (whether `.dwell` comes) and
`relaunch` (whether the system starts the app for an event).

### Events with the app not running

The system keeps a batch or a crossing until the app has read it: one
that came as the app ended is given again at the next start. An event
keeps the app running a few seconds; `b.hold()` and `c.hold()` keep it
running until the function they return is called (or the system's time
is up: about 30 seconds on iOS, 10 on Android), for work that goes on
after the event is read (sending it to a server).

### Testing

`location.fake()` gives the package a `FakeLocation` instead of the
platform: `answer` is what the user answers when asked, `move(latitude,
longitude)` takes the device somewhere (the positions' readers see it, a
batch comes while updates run, and the regions it enters or leaves cross),
`allow(level)` is the user changing it in Settings, and `pending` counts
the events whose `hold()` isn't done.

```asuka
import platform/location

test "a region, crossed" {
  let loc = location.fake()
  loc.answer = .always
  loc.move(35.6812, 139.7671)
  let _ = try await location.requestAlways()
  try await location.monitor(location.Region(id: "here", latitude: 35.6812, longitude: 139.7671, radius: 150))
  loc.move(35.70, 139.7671) // 2 km north: an exit
}
```

## Notifications: platform/notify

`platform/notify` shows local notifications, now or at a time the app sets,
whether or not the app is running then. Tapping one opens the app with
its URL. It also has what a messaging app needs, for `platform/push`'s
notifications too: actions and replies typed in the notification,
threads, Android's channels, messages in conversations, the badge, the
notifications showing, and which ones show while the app is open.

```asuka
import platform/notify

async fn remindToWater() throws {
  if !(try await notify.request()) {
    return
  }
  try await notify.schedule(notify.Notification(
    id: "water",
    title: "Water the plants",
    body: "It's been a day.",
    url: "myapp://plants",
    after: 24h,
  ))
}
```

- `notify.request()` asks the user to allow notifications the first
  time, and answers whether they're allowed: their answer then, or what
  they've set in Settings since.
- `notify.schedule(n)` sets `n` to show `n.after` from now (at least a
  second); `notify.show(n)` shows it now. One with the `id` of one
  waiting or showing replaces it. Both are async.
- `notify.cancel(id)` takes the notification away, waiting or shown.

A `Notification`'s `url` should be one of the app's scheme (`asuka.toml`'s
`scheme`): the app's `openURL` gets it when the user taps the
notification. Its other fields:

| Field | What it does |
|---|---|
| `subtitle` | A line under the title (iOS); Android's sub text beside the app's name |
| `data` | The app's own keys, given back in `showing`, `responses` and `present` |
| `category` | A category's ID: the actions it shows (below) |
| `thread` | Groups it with the thread's others: iOS's thread, an Android group with a summary notification Asuka keeps |
| `summary` | The line under an Android group's summary ("3 new messages"); iOS writes its own |
| `channel` | The Android channel it's in (below); iOS has none |
| `badge` | The app icon's number when it shows (iOS, macOS); Android's notification number |
| `silent` | No sound |
| `image` | A picture, as `Image`'s sources (an asset, a file, an http(s) URL): iOS's attachment, Android's big picture |

**iOS** shows a notification while the app is open too, as a banner,
unless `present` says otherwise. **Android** posts a scheduled one from an
inexact alarm (no exact-alarm permission needed), so it may come a
little after its time.

### Actions and replies

A category is a kind of notification and its buttons. Set the app's
categories as it starts, before notifications that name them come (a
push's too: `aps.category` on iOS, the data's `"category"` on Android).
They hold across launches.

```asuka
import platform/notify

component Chats {
  task {
    notify.setCategories([notify.Category(id: "message", actions: [
      notify.Action(id: "reply", title: "Reply", icon: "arrowshape.turn.up.left",
        reply: notify.Reply(button: "Send", placeholder: "Message")),
      notify.Action(id: "read", title: "Mark as Read"),
      notify.Action(id: "delete", title: "Delete", destructive: true, unlock: true),
      notify.Action(id: "open", title: "Open", open: true),
    ])])

    for await r in notify.responses() {
      match r.action {
        "" => openChat(r.notification.thread)   // a tap on the notification
        "reply" => {
          let done = r.defer()                   // sending takes a moment
          defer { done() }
          try? await send(r.notification.thread, r.reply)
        }
        "read" => notify.removeThread(r.notification.thread)
        _ => {}
      }
    }
  }

  Text("Chats")
}

// The app's own.
fn openChat(thread: string) {}
async fn send(thread: string, text: string) throws {}

app { Chats() }
```

- An `Action` has its `id` and `title`, an `icon` (an SF Symbol; iOS
  15 and macOS 12 show it, Android shows actions without icons),
  `destructive` (iOS shows it red), `unlock` (the device unlocks first:
  iOS, Android 12 and later), `open` (it opens the app) and `reply` (a
  text field, with its send button's title and placeholder).
- `notify.responses()` is a stream of what the user does: a tap
  (`action` `""`) or an action, `reply` the text typed, `notification`
  the one answered (`platform/push`'s too). A response that came before
  anything read the stream (the one that launched the app, or woke it)
  waits for the first reader, so read it as the app starts, in a task of
  its root.
- An action without `open` runs in the background: the app runs
  (started without a screen if it isn't running: its root and that
  task), and doesn't come to the front. Work that goes on after the
  response is read calls `r.defer()`, and the function it returns once
  it's finished: the system keeps the app running until then, or until
  its time is up (iOS gives about 30 seconds, Android about 10).

**Android** takes a background action's notification away, as iOS does,
unless the app shows it again (`show`, with its `id`) by the time it's
done. After a reply, Android wants the notification posted again (its
spinner turns until then): Asuka adds the reply to a conversation's
notification, under the user's name, and takes another away.

### Threads and the notifications showing

A `thread` groups a chat's notifications: iOS stacks them, Android puts
them in a group under a summary notification, which Asuka keeps as they
come and go.

- `notify.showing()` lists the app's notifications that show (in
  Notification Center, the shade), its pushes' too, newest first: each
  `Delivered` has its `id`, `title`, `subtitle`, `body`, `url`, `data`,
  `category`, `thread`, `time` (a `time.Time`), and `remote`
  for a push.
- `notify.remove(ids)`, `notify.removeThread(thread)` (a chat that's
  read) and `notify.removeAll()` take them away.

### What shows while the app is open

```asuka
import platform/notify

component Chat(id: string) {
  effect(id) {
    notify.present(n => if n.thread == id { .hide } else { .banner })   // the chat is on screen
    cleanup { notify.stopPresenting() }
  }

  Text("Chat")
}
```

`notify.present(decide)` has `decide` say how each notification that
comes while the app is in the foreground shows: `.banner` (as with the
app closed), `.quiet` (in Notification Center, the shade, without a
banner or sound) or `.hide`. It's called as the notification comes, and
must answer at once: Asuka waits a second at most, then shows it.
`notify.stopPresenting()` shows them all again.

### Messages in conversations

A message shows as the platform shows messages, with the sender's
picture: iOS's communication notification, Android's conversation (the
shade's Conversations section, with the conversation's shortcut).

```asuka
import platform/notify

async fn whosIn() throws {
  let ana = notify.Person(id: "u7", name: "Ana", image: "https://example.com/ana.jpg")
  let chat = notify.Conversation(id: "climbing", title: "Climbing", group: true)
  try await notify.show(notify.Notification(body: "Who's in for Saturday?", category: "message").from(ana, chat))
}
```

- `n.from(sender, c)` makes `n` a message: `sender` said `n.body` in
  `c`. Its `id` and `thread` default to the conversation's `id` (a 1:1
  conversation's is the sender's), its `title` to the sender's name (the
  group's title, for a group).
- A `Person` has an `id` (the same each time), a `name`, an `image` (as
  `Image`'s sources) and `bot`. A `Conversation` has its `id`, a
  group's `title` and `image`, `group`, `me` (the user as the
  conversation shows them; Android names their replies with it, "You" by
  default) and `earlier` messages (each a `Message`: `from`, `text`,
  `time`, a `time.Time?`: none when not known).
- **Android** shows a conversation as one notification, its messages in
  it: a message adds to the conversation's notification that shows (or
  `earlier` sets what comes before it). **iOS** shows each message as a
  notification of its own, in the conversation's thread.
- **iOS** needs the Communication Notifications capability on the App
  ID: an app that calls `from` is given its entitlement and Info.plist's
  `NSUserActivityTypes` (`INSendMessageIntent`), and a release build's
  profile must have it. Without it, a message shows as a plain
  notification. **macOS** shows a message as a plain notification.

### Android's channels

A channel is a kind of notification the user can turn off or quiet in
the app's settings. Android shows a notification with its channel's
sound, vibration and importance; iOS has no channels, and ignores them.

```asuka
import platform/notify

fn makeChannels() {
  notify.setChannelGroups([notify.ChannelGroup(id: "chats", name: "Chats")])
  notify.setChannels([
    notify.Channel(id: "messages", name: "Messages", importance: .high, group: "chats"),
    notify.Channel(id: "digest", name: "Daily digest", importance: .low, noBadge: true),
  ])
}
```

- `notify.setChannels(cs)` makes the channels, renames the ones made
  before, and deletes those it made before that aren't in `cs`. A
  channel's `importance` (`.default`, `.high`: heads-up, `.low`: no
  sound, `.min`), `silent`, `noVibration` and `noBadge` are its when
  it's made; after, they're the user's to change.
  `notify.setChannelGroups(gs)` does the same for groups (deleting a
  group deletes its channels).
- A notification without a `channel`, or with one that isn't, is in
  the default channel ("Notifications"), which stays. A push names its
  channel with FCM's `android.notification.channel_id`, or a data
  message's `"channel"`.

### The badge

`notify.setBadge(n)` sets the number on the app's icon; `0` takes it
away. **iOS** and **macOS** (the Dock) show it as set. **Android** has no
badge of its own: launchers that badge count the app's notifications (or
show a dot), so the badge goes as they're read (`remove`,
`removeThread`), and `setBadge` does nothing; a notification's `badge`
is the number a launcher may show. A channel with `noBadge` doesn't
count.

### On each platform

| | iOS | Android | macOS | Windows | Linux | Web |
|---|---|---|---|---|---|---|
| Show, schedule, cancel | ✓ | ✓ | ✓ | ✓ (toasts) | ✓ (scheduled: a systemd user timer, whether the app runs or not) | ✓ (scheduled: while a page of the app is open) |
| Actions, replies | ✓ | ✓ | ✓ | ✓ (five buttons) | actions (a reply opens the app) | buttons where the browser shows them (Chrome, Edge: two); replies where it has inline replies |
| Background actions | ✓ (about 25 s) | ✓ (about 9 s) | ✓ | ✓ (about 25 s; started for it) | while the app runs | in a page of the app (the worker opens one) |
| Threads | ✓ | ✓ (a group and its summary) | ✓ | not grouped; `removeThread` | ✓ | not grouped; `removeThread` |
| Channels | ignored | ✓ | ignored | ignored | ignored | ignored |
| Conversations | ✓ (communication notifications) | ✓ (MessagingStyle, shortcuts) | plain | plain | plain | plain, the sender's picture its icon |
| Badge | ✓ | the launcher's | ✓ (the Dock) | the taskbar button's overlay | the launcher API's (Dash to Dock, Plasma) | the installed app's (`navigator.setAppBadge`) |
| Showing, removing | ✓ | ✓ | ✓ | ✓ (the notification center's) | what the app showed this run | ✓ (the service worker's) |
| `present` | ✓ | ✓ | ✓ | ✓ (not scheduled ones) | ✓ | ✓ |

**macOS** needs the app's bundle, which `asuka dev` and `asuka build` run it
from: an executable run on its own has no bundle ID, and its calls
throw an error that says so. **Windows** shows toasts, now or scheduled,
with their image, a category's actions as buttons and a reply as a text
box. The app registers itself the first time (a Start menu shortcut with
its AppUserModelID, asuka.toml's `id`, and a COM activator: its executable,
in the user's registry), so that a tap or a button reaches the app,
which Windows starts for it if it isn't running; for a background
action it stays out of sight and quits once the response is handled. The
badge is a count drawn on the app's taskbar button (a packaged app's,
the system's badge). Windows shows a scheduled toast however `present`
would. **Linux** sends them as GNotifications
when the app has its desktop file (or runs in a sandbox), else to the
desktop's notification server itself (GNOME Shell turns away an app it
has no desktop file for); taps and buttons reach the app while it runs.
A notification due later is a systemd user timer (`systemd-run --user`),
which runs the app's executable without a window to show it, hands it
to the app if it runs, and launches the app for a tap; without systemd
(a Flatpak, `asuka dev`) it shows while the app runs. **A browser** shows
them through the app's service worker, with Web Push for `platform/push`:
[the web](web.md) has the details.

## Sound: platform/audio

`audio.play(source)` plays a sound once: a file in the app's assets, a
file's path, or an http(s) URL. `audio.Player(source)` is a store that
holds one sound, ready to play as it's made: `play`, `pause`,
`seek(seconds:)`, `setVolume`, `setRate` (the pitch kept, for speech),
`setLoop`, and `status()`, where it is (`position`, `duration`,
`playing`, `ended`). It stops and lets go of the sound when the
component that made it goes, or at `release()`.

```asuka
import platform/audio

component Episode(url: string) {
  state player = audio.Player(url)
  state status = audio.Status()

  task {
    while true {
      status = await player.status()
      await sleep(250ms)
    }
  }

  Column {
    Text("{status.position.fixed(0)} / {status.duration.fixed(0)} s")
    Button(if status.playing { "Pause" } else { "Play" }) {
      on press { if status.playing { player.pause() } else { player.play() } }
    }
    Button("Ding") { on press { audio.play("ding.wav") } }
  }
}
```

AVPlayer on iOS and the Mac, Media3's ExoPlayer on Android, an `<audio>`
in a browser, MCI on Windows, GStreamer's `playbin` on Linux. A sound
plays with the screen locked only when the app says it's playing media:
see Now Playing below.

### Now Playing and the media keys: platform/nowplaying

`nowplaying.NowPlaying` tells the system what the app plays, while it's
in the view: the lock screen's and Control Center's player on iOS, the
media notification on Android (a foreground service, so it keeps
playing), the Now Playing widget on the Mac, the media flyout on
Windows (SMTC), MPRIS on Linux (GNOME's media controls), the Media
Session API in a browser. Its events are the system's commands: the
media keys, the headphones' buttons, the lock screen's. Given a
`player`, it follows its position and plays and pauses it itself.

```asuka
import platform/audio
import platform/nowplaying

component Playing(url: string) {
  state player = audio.Player(url)

  Column {
    nowplaying.NowPlaying {
      title: "Episode 12"
      artist: "The Show"
      artwork: "cover.jpg"
      player: player
      skipInterval: 15
      on next { player.seek(0) }
    }
    Button("Play") { on press { player.play() } }
  }
}
```

Without a `player`, give `state` (`.playing`, `.paused`), `duration` and
`position`, and handle `play`, `pause`, `seek(position)`,
`skipForward(seconds)` and `skipBackward(seconds)`. When several are in
the view, the last one shown is what the system shows. An app that
imports it gets iOS's `audio` background mode and Android's
foreground-service permissions.

## Network state: platform/network

`platform/network` is the device's network: whether it's online, over Wi-Fi or
cellular, and whether the connection costs. Use it to download large
files on Wi-Fi only, or to say the app is offline. It's NWPathMonitor on
iOS and the Mac, ConnectivityManager on Android, the Network List
Manager on Windows, GNetworkMonitor and NetworkManager on Linux, and
`navigator.onLine` in a browser, which says what carries it only where
it can (Chrome on Android).

`network.status()` is the network, tracked: read in a binding, it
follows the changes while something reads it, and only then does
anything watch the network.

```asuka
import platform/network

component NetworkBadge {
  let net = network.status()

  Text(if !net.connected { "Offline" } else if net.metered || net.constrained { "{net.type}: large downloads wait for Wi-Fi" } else { "{net.type}" })
}
```

A `network.Status` has:

- `connected`.
- `type`: `.none`, `.wifi`, `.cellular`, `.ethernet` or `.other` (a
  VPN alone, Bluetooth...); as text, its name (`"wifi"`).
- `metered`: the connection costs or is limited, as mobile data or a
  phone's hotspot.
- `constrained`: the user asked apps to use less data (iOS's Low Data
  Mode, Android's Data Saver, a browser's Data Saver), or, on Windows,
  the connection is near or over its data limit. Never on Linux.

Until the platform first answers, and where it says nothing of its
network, it's `connected` with the `type` `.other`.

`network.current()` is the network now, untracked, and
`network.changes()` a stream of it: now, then at each change.

## The user's activity: platform/activity

`platform/activity` says when the user last gave the app input: a press
(a mouse's button, a finger or a pen touching down, a trackpad's click),
a key, a wheel or trackpad scroll, a text edit (a software keyboard's
too). A pointer only moving doesn't count, nor what the app does itself
(a scroll it starts, a field's text it sets). Use it for an inactivity
timeout: a warning, then signing out, after so long with no input. It
only watches: the app's views get every event as before.

`activity.idle()` is how long since the last input, `activity.lastInput()`
when it was (a `time.Time`), and `activity.inputs()` a stream of each
input's time, at most one a second.

```asuka
import platform/activity

component SignedIn {
  prop limit: Duration
  prop warning: Duration
  state left: Duration? = none
  state signedOut = false

  // Asleep until the warning's due; then a countdown, which input ends.
  task {
    while !signedOut {
      let idle = activity.idle()
      if idle >= limit {
        signedOut = true
      } else if idle >= limit - warning {
        left = limit - idle
        await sleep(1s)
      } else {
        left = none
        await sleep(limit - warning - idle)
      }
    }
  }

  Column {
    if signedOut {
      Text("Signed out")
    } else if let l = left {
      Text("Signing out in {l.seconds().fixed(0)} seconds")
    } else {
      Text("Signed in")
    }
  }
}
```

The platforms watch every window of the app, never taking an event from
its views: on iOS a gesture recognizer on each window that sees a touch
or a key's press begin and fails at once, a pan that takes no touches
for a trackpad's or mouse's scrolling, and the text fields' and text
views' changes; on the Mac a local event monitor and the text views'
changes; on Windows a hook on the main thread's messages; on Linux
controllers in the capture phase of each window; in a browser the
document's capturing, passive listeners (`pointerdown`, `keydown`,
`wheel`, `input`). The clock goes on while the device sleeps, so the
time asleep counts as idle. Android hasn't it yet: there, and where an
app runs with no platform code (`asuka test`'s host, a program),
`activity.available()` is false and `idle()` is the time since the app
started. `power.idleTime()` is the other kind: the whole computer's,
every app's input, on desktops only.

Tests drive a fake: `activity.fake()` gives a `FakeActivity`, whose
`input()` is the user's, on the test's clock.

```asuka
test "a warning, then signing out" {
  let user = activity.fake()
  let app = mount(SignedIn { limit: 15min; warning: 30s })
  await sleep(14min + 40s + 500ms)
  app.expect("Signing out in 20 seconds")
  user.input()
  await sleep(1s)
  app.expect("Signed in")
}
```

## Saving to the library: platform/library

`platform/library` saves files where the user and other apps find them: a
video to the gallery, a song to the music, anything to Downloads.

```asuka
import platform/library

async fn keep(file: string) -> string {
  do {
    let _ = try await library.save(file, collection: .movies, folder: "My App", move: true)
    return "Saved"
  } catch e {
    return "Couldn't save: {e.message}"
  }
}
```

`library.save(path, collection:, name:, folder:, mime:, move:)` copies the
file into the collection, off the main thread (a large video takes a
while), and answers where it went. It asks for what it needs the first
time.

- `collection`: `.downloads` (the default), `.movies`, `.music` or
  `.pictures`.
- `name`: the file's name there (default: its own). A name taken gets a
  number.
- `folder`: a folder of the collection's for the app's files; empty: the
  collection itself.
- `mime`: its type (default: from its extension, `library.mimeOf(name)`).
- `move`: remove the file once it's saved, for a download in the app's
  caches: no second copy.

Where each collection goes:

| Collection | Android | iOS | macOS | Windows, Linux |
| --- | --- | --- | --- | --- |
| `.downloads` | Downloads | the app's Documents, in the Files app | `~/Downloads` | the user's Downloads |
| `.movies` | the gallery's videos | Photos | Photos | the user's Videos |
| `.music` | Music | the app's Documents, in the Files app | `~/Music` | the user's Music |
| `.pictures` | the gallery's pictures | Photos | Photos | the user's Pictures |

On Windows and Linux, and in `~/Downloads` and `~/Music` on a Mac, the
file goes under `folder` in the user's folder, answering with its path;
Linux's folders are XDG's. A browser downloads the file, whatever the
collection: it lands where the browser puts downloads.

**Android** uses the MediaStore collections from Android 10, with no
permission, and answers with a `content://` URI. Before Android 10, it
writes to the shared folders after asking for the storage permission.

**iOS** asks to add to Photos (add only: the app can't read the library)
and answers with the asset's identifier; `folder` doesn't apply there.
Other collections go to the app's Documents, under `folder`, answering
with the file's path; Asuka makes Documents visible in the Files app.

## Background work: platform/background

`platform/background` keeps the app working while it's out of sight: a
download, an upload, an export the user started, which takes minutes.

`background.start(title:, text:, progress:, actions:, onAction:)` starts
a task: the app keeps running, with a notification showing its title,
text, progress and buttons, until the task's `end()`. `onAction` gets
the ID of a notification button the user taps. `update(…)` shows new
ones, its progress as it goes. Start a task while the app is on screen,
as the user asks for the work.

```asuka
import platform/background

component Export {
  state status = ""
  state cancelled = false

  Column {
    Button("Export") {
      on press {
        cancelled = false
        let buttons = [background.Action(id: "cancel", title: "Cancel")]
        let job = background.start(title: "Exporting", progress: -1, actions: buttons, onAction: a => {
          if a == "cancel" { cancelled = true }
        })
        defer { job.end() }
        var i = 1
        while i <= 10 && !cancelled {
          await sleep(1s)   // the work
          let p = float(i) / 10
          job.update(title: "Exporting", text: "{(p * 100).fixed(0)}%", progress: p, actions: buttons)
          i += 1
        }
        status = if cancelled { "Cancelled" } else { "Exported" }
      }
    }
    Text(status)
  }
}
```

- `title` and `text`.
- `progress`: 0 to 1; negative: under way, how far not known.
- `actions`: the notification's buttons, each a `background.Action` (an
  `id` and a `title`).

Each `update` replaces all of them, so give the `actions` again. The
work itself is the app's async code: it goes on while the task lives.
[`examples/downloader`](../examples/downloader) downloads in a task,
with Pause, Resume and Cancel.

**Android** stops a backgrounded app within minutes. A task keeps it
running in a foreground service (of the data sync type), with an ongoing
notification for each task, showing its progress and up to three
buttons, and keeps the CPU awake. On Android 13 and later, starting a
task asks the user to allow notifications if they haven't; the work goes
on either way. Android doesn't let an app in the background start one.
Android 15 limits a data sync service to six hours a day. Google Play
asks, in the Play Console, what the app's data sync foreground service is
for: declare it before you upload.

**iOS** gives a backgrounded app about 30 seconds more while a task
lives, and shows no notification: `update` does nothing there, and
`onAction` is never called. Out of time, iOS ends the task and the work
may stop.

**A desktop** app keeps running out of sight: a task keeps the system
from slowing or suspending it (App Nap on a Mac, sleep on Windows and
Linux), with no notification: `update` does nothing, and `onAction` is
never called. **A browser** runs the page while it's open, with or
without a task.

## Shares from other apps: onShare

`onShare(items => …)` in a component's body hears what another app
shares with this one from its share sheet, while the component lives: a
link from YouTube or a browser, a text from Notes, photos from the
gallery, a PDF from Files. The app block takes it as an event too, `on
share(items) { … }`. It's iOS's and Android's: on the desktops and in a
browser the app isn't offered in other apps' share menus, and nothing
comes. `asuka.toml`'s `shareTarget` says what the app is offered for:

```toml
shareTarget = true
```

takes texts and links. A table takes files too, by MIME type, and at
most `max` at once (default 1):

```toml
[shareTarget]
types = ["text", "image/*", "video/*"]
max = 10
```

A type is `"text"` (texts and web links) or a MIME type: `"image/*"`,
`"video/*"`, `"audio/*"`, `"application/pdf"`, `"*/*"` for any file.

```asuka
component Shared {
  state link = ""
  state photos: [string] = []

  onShare(items => {
    for it in items {
      match it.kind {
        .link => { link = it.text }
        .image => { photos.push(it.path) }
        _ => {}
      }
    }
  })

  Column {
    Text(link)
    for p in photos key p { Image(p) }
  }
}
```

Each `SharedItem` has a `kind`: `.text`, `.link`, `.image`, `.video`,
`.audio` or `.file`. A text or a link is in `text`. A file is at `path`,
the app's own copy in its caches: move it where the app keeps it, or
remove it when you're done. It comes with `name`, its name in the app it
came from (`IMG_0042.HEIC`), its `mime` type and its `size` (-1 if not
known). The items come in the order the other app gave them. A page
shared from a browser comes as its title, a text, and its link.

What came before anything heard it, the share that launched the app,
goes to the first `onShare` (or the app's `on share`). A component's
`onShare` stops when the component goes. To open a screen of the app's
for what's shared, see [Navigation](navigation.md).

**Android** opens the app itself with the share (`ACTION_SEND`, and
`ACTION_SEND_MULTIPLE` when `max` is over 1), for the types' intent
filters. The app copies the files to its caches before `onShare` gets
them, while its access to them still holds.

**iOS** has a share extension in the app, which opens the app with what's
shared, as Android opens it: the extension shows no screen of its own.
Its activation rule comes from the types: Apple's keys for texts,
links, images, videos and any file, which keep `max`; a predicate on the
attachments' types for others (audio, a PDF), which doesn't. The
extension takes whatever it's given, so check `kind` and `mime`. Files
go from the extension to the app through an App Group,
`group.<app ID>`, which a device build's profiles must have (see
[Shipping](shipping.md#ios)).

Under `asuka dev`, the player takes shares of anything, whatever the
app's `shareTarget`.

What a link leads to (a page, an image, a video) is the app's to find
out, with `std/http`: expo-sharing's "resolved" payloads, in Asuka.

### The app's own share sheet: platform/sharesheet

By default a share opens the app. A share sheet of the app's own is a
screen shown over the app that shares instead, as "Save to Raindrop" or
"Add to Notes" is: the user saves, and is back where they were. With
`platform/sharesheet`, the app block shows it when the app runs as its
sheet:

```asuka
import platform/sharesheet

component SaveLink {
  state link = ""
  state note = ""

  onShare(items => {
    for it in items {
      if it.kind == .link {
        link = it.text
      }
    }
  })

  Column {
    padding: 16; gap: 12; grow: 1
    if link.isEmpty() {
      ProgressRing()
    } else {
      Text(link)
    }
    TextField { bind text: note; placeholder: "Note" }
    Row {
      gap: 12
      Button("Cancel") {
        on press { sharesheet.cancel() }
      }
      Button("Save") {
        variant: .filled
        on press {
          save(link, note)
          sharesheet.done()
        }
      }
    }
  }
}

component Root {
  Text("Links")
}

// The app's own.
fn save(link: string, note: string) {}

app {
  if sharesheet.active() {
    SaveLink()
  } else {
    Root()
  }
}
```

`sharesheet.active()` is true when the app runs as its sheet: its root
is the sheet then, and the rest of the app isn't made. The sheet hears
what's shared with `onShare`, as the app does; until the items have
come (a video being copied), show a spinner. It ends with
`sharesheet.done()`, `sharesheet.cancel()`, or `sharesheet.openApp()`,
which opens the app with the items for its `onShare`, as without a
sheet. `asuka.toml`'s `shareTarget` says what the sheet is offered for,
as above; an app that uses `active()` without one is offered for
nothing (the CLI says so).

The sheet is a root of its own, in a process of its own, as iOS runs
share extensions: on **iOS** the app's share extension, which iOS shows
as a sheet, running the app from the framework it shares with the app;
on **Android** an Activity in a `:share` process, shown as
a Material bottom sheet over the other app (dragging it down, tapping
outside and back cancel it). The process ends with the sheet. A file's
`path` is the sheet's until it closes: copy what the app keeps.

What the app keeps is the same in its sheet: its files (`std/fs`'s
app directory, SQLite databases there), `std/storage`'s values,
`platform/secure`. Android's two processes share the app's files. On iOS
the app's files and values move to the App Group (`group.<app ID>`),
once, the first time it runs with a sheet, and its keychain group is
shared with the extension; a device build's profiles need the App
Groups capability (see [Shipping](shipping.md#ios)). The app doesn't
hear what its sheet wrote while it was running: look again when it
comes back to the front.

When the app uses `sharesheet.active`, Asuka builds the sheet instead
of the extension that opens the app. On iOS the app's code is then a
framework that the app and its extension both load, as Xcode shares
code with an extension: the app isn't bigger for its sheet. On the desktops and in a browser the app isn't offered in
other apps' share menus: `active()` is false there.

In a test, `sharesheet.fake()` makes the app run as its sheet
(`mountApp()` shows it), and keeps how it closed (`closed`); a mounted
app's `device.share(items)` is another app sharing them:

```asuka
import platform/sharesheet

component SaveLink {
  state link = ""

  onShare(items => {
    link = items[0].text
  })

  Column {
    Text(link)
    Button("Save") {
      on press { sharesheet.done() }
    }
  }
}

app {
  if sharesheet.active() {
    SaveLink()
  } else {
    Text("Links")
  }
}

test "the sheet saves" {
  let sheet = sharesheet.fake()
  let app = mountApp()
  app.share([SharedItem(kind: .link, text: "https://asuka.dev")])
  app.expect("https://asuka.dev")
  app.tap("Save")
  expect(sheet.closed == .done)
}
```

## JavaScript: platform/js

`platform/js` runs JavaScript off screen, in the platform's own engine: a web
view that's never shown (WKWebView on iOS and the Mac, Android's WebView,
WebView2 on Windows, WebKitGTK on Linux), or in a browser a hidden,
sandboxed frame. It's for code
that only has a JavaScript form, as a site's player whose functions sign
its video URLs.

`js.eval(script)` runs a script, statements as a `<script>`'s, in a
fresh page of its own, and returns its value as JSON: the last
statement's value, or what the promise it returns resolves to (`null` for
`undefined`). A thrown error, or a rejected promise, is the error, with
the script's message. The page is gone after: what one script sets isn't
there for the next. It waits without holding the main thread.

```asuka
import platform/js
import std/json

async fn doubled() throws -> [int] {
  let v = try await js.eval("const xs = [1, 2, 3].map(x => x * 2); xs")   // "[2,4,6]"
  return try json.decode<[int]>(v)
}

async fn later() throws -> string {
  let v = try await js.eval("new Promise(done => setTimeout(() => done('later'), 100))", timeout: 10s)
  return try json.decode<string>(v)
}
```

A script may run for its `timeout` (a minute by default); one that runs
out throws `js.JsError.timeout`. Cancelling the task (the screen that
started it going) lets its page go.

For script in a page the user sees, use `WebView`
([Web pages](components.md#web-pages)).

## Push notifications: platform/push

`push.register()` asks the user to allow notifications (the first time),
registers the device with Apple's (APNs) or Google's (Firebase Cloud
Messaging) service, and answers a `Registration`: its `token`, its
`service` (`"apns"`, `"fcm"`) and whether notifications are `allowed`.
Send the token to your server, which sends notifications to it; register
at each launch, since the token can change. A Mac registers with APNs
as iOS does (with a provisioning profile: see [On a Mac](#on-a-mac)). On
the web it's Web Push (`"webpush"`, the token the browser's
subscription; see [the web](web.md)). Windows and Linux have no push
service for an app: `register` throws there.

```asuka
import platform/push

component Root {
  task {
    if let r = try? await push.register() {
      try? await saveDeviceToken(r.service, r.token)
    }
  }
  task {
    for await m in push.opened() { openThread(m.data["thread"] ?? "") }
  }

  Text("Inbox")
}

// The app's own.
async fn saveDeviceToken(service: string, token: string) throws {}
fn openThread(id: string) {}

app { Root() }
```

- `push.received()` is a stream of the messages that come while the app
  runs. A message is shown as a notification too, in the foreground as
  well, if it has a title or body. Data messages (silent pushes) come
  here only.
- `push.opened()` is a stream of the notifications the user taps,
  including the one that launched the app, which waits for the first
  reader: read it as the app starts. A message's data `"url"` also goes
  to the app's `openURL`, as `platform/notify`'s do.
- A `push.Message` has its `id`, `title`, `body` and `data` (iOS: the
  payload's keys beside `aps`; Android: the message's data).

iOS needs the Push Notifications capability on the App ID for a release
build; the simulator gets a token too. Android needs a Firebase project:
add the Android app to it, download its `google-services.json`, and name
it in `asuka.toml`:

```toml
[android]
googleServices = "google-services.json"
```

Asuka writes its settings into the app as Google's Gradle plugin would.
Without it, `register` throws with what to do.

### Categories, threads and channels in a push

A push uses `platform/notify`'s categories (its actions, `notify.responses()`),
threads, channels, badge and `present`, named in its payload:

- **iOS**: `aps`'s `category`, `thread-id` and `badge`, as APNs has them.
- **Android**: a notification message's `android.notification.channel_id`
  and `image`; a data message's `"title"`, `"body"`, `"subtitle"`,
  `"category"`, `"thread"`, `"summary"`, `"channel"`, `"image"`, `"badge"`
  and `"url"`. A data message with a `"title"` or `"body"` shows as a
  notification (Asuka builds it, with its actions); one without is
  silent (`received()` only). A notification message that comes while
  the app is in the background is shown by Firebase, without actions or
  a thread: send data messages for them.

A `push.Message` has these too: `subtitle`, `category`, `thread`,
`channel`, `image`, `badge`, `summary`.

### Rewriting a push before it shows

`push.rewrite(f)` has the app change each push before it shows: decrypt
an end-to-end encrypted payload, set its title and body, attach an
image, make it a message in a conversation with the sender's picture,
set its thread and category. Call it as the app starts:

```asuka
import platform/push
import platform/notify

async fn reveal(m: push.Message) throws -> push.Message? {
  let box = m.data["box"] ?? ""
  if box.isEmpty() {
    return none   // not ours: as it came
  }
  let msg = try await decrypt(box)   // with a key in platform/secure
  var out = m
  out.body = msg.text
  out.category = "message"
  return out.from(notify.Person(id: msg.fromID, name: msg.fromName, image: msg.avatar),
    notify.Conversation(id: m.data["chat"] ?? ""))
}

component Root {
  task {
    push.rewrite(reveal)
  }

  Text("Chats")
}

// The app's own.
struct Plain {
  text: string
  fromID: string
  fromName: string
  avatar: string
}

async fn decrypt(box: string) throws -> Plain {
  return Plain(text: box, fromID: "ana", fromName: "Ana", avatar: "")
}

app { Root() }
```

`f` answers the push to show; `none`, or an error it throws, shows the
push as it came. Pushes are rewritten one at a time, in the order they
came. `image` is downloaded and attached; `from` makes it a message, as
`notify.Notification`'s `from` does. `push.stopRewriting()` shows them as
they come again.

**iOS** rewrites in a Notification Service Extension, which `asuka build`
makes for an app that uses `push.rewrite`: iOS starts it for each push
with `"mutable-content": 1` in its `aps` (and an `alert`, which shows if
the rewrite fails). It runs the app without a screen, as a launch in the
background does, and the rewrite runs once the app has called
`push.rewrite`: keep what its root does at launch light, or make the
extension's root one that only calls it (`push.inExtension()` is true
there: `if push.inExtension() { Rewrites() } else { Root() }`). iOS
gives it about 30 seconds and 24 MB, and shows the push as it came when
time runs out. What the app keeps is the same there: its files and
`std/storage`'s values are in the App Group the two share, its
keychain items (`platform/secure`) in the app's group. A release build
needs a provisioning profile for the extension's bundle ID, the app's
plus `.notification` (`asuka.toml`'s `ios.notificationProfile` names
one; by default, the best match installed), with the App Groups
capability and the app's group. A rewrite can't hide a push on iOS. The
simulator runs the extension only for pushes that come from APNs, not
for `xcrun simctl push`.

**Android** rewrites FCM's data messages in the app's process: the app
starts without a screen if it isn't running, rewrites within about ten
seconds, and the notification shows. A data message whose rewrite leaves
no title or body doesn't show. A notification message that comes while
the app is in the background is shown by Firebase, without a rewrite:
send data messages.

**The web** rewrites a push in the page in front, before the service
worker shows it; with no page open, it shows as it came.

In a test, `push.fakeRewrite(m)` is what shows of a push that comes:
the rewrite's push, or `m` itself.

## Sign in with Apple: platform/appleauth

`appleauth.SignInButton` is Apple's own button (its title, logo and
colors, in the user's language), and `appleauth.signIn` shows the
system's sheet. The App Store requires Sign in with Apple of an app that
offers another company's sign-in for its accounts.

```asuka
import platform/appleauth

component SignIn {
  state status = ""

  Column {
    appleauth.SignInButton {
      type: .continueWithApple
      variant: .automatic   // black in light mode, white in dark
      height: 44
      on press {
        do {
          let c = try await appleauth.signIn(scopes: [.fullName, .email], nonce: "n-0S6_WzA2Mj")
          try await signInWithApple(c.identityToken, c.authorizationCode)
        } catch appleauth.AppleAuthError.canceled {
        } catch e {
          status = e.message
        }
      }
    }
    Text(status)
  }
}

// The app's own: its server checks the token with Apple.
async fn signInWithApple(token: string, code: string) throws {}
```

The `Credential`'s `user` is the account's stable ID; send its
`identityToken` (a JWT Apple signs) and `authorizationCode` to your
server, which checks them with Apple. `email`, `givenName` and
`familyName` come only the first time the user signs in to the app: keep
them then. `AppleAuthError.canceled` is the user closing the sheet.
`appleauth.credentialState(user)` says whether the user still uses Sign
in with Apple for the app (`.revoked`: sign them out). On Android there's
none: `SignInButton` shows nothing, `available()` is false and `signIn`
throws `AppleAuthError.notAvailable`; so on Windows, Linux and the web.
Sign in with Apple there on the web (`platform/webauth`). The Mac has it as
iOS does, with a provisioning profile (see [On a Mac](#on-a-mac)). In a
test, `appleauth.fake()` answers as the user would: its `credential`,
`cancel` and `states`.

## Wallet payments: platform/pay

`platform/pay` takes payments with the platform's wallet: Apple Pay on
iOS, macOS and in Safari, Google Pay on Android and in the other
browsers. It's for physical goods and services (an order, a ride, a
ticket, a donation); digital content sold in the app is the stores'
in-app purchases. The user pays in the wallet's sheet, and the app gets
the wallet's payment token for its processor (Stripe, Adyen,
Braintree...), which its server charges. Asuka never charges a card.

```asuka
import platform/pay

fn request() -> pay.Request = pay.Request(
  country: "US",
  currency: "USD",
  items: [pay.Item(label: "Tea set", price: .parse("40.00")!), pay.Item(label: "Tax", price: .parse("3.20")!, kind: .tax)],
  total: pay.Item(label: "Asuka Tea", price: .parse("43.20")!),
  shipping: [.name, .address, .email],
  apple: pay.Apple(merchantId: "merchant.com.example.tea"),
  google: pay.Google(gateway: "stripe", gatewayParams: ["stripe:version": "2018-10-31", "stripe:publishableKey": "pk_test_123"]),
  onShippingContact: c => {
    // The items and total for the address.
    if c.address.countryCode != "US" {
      throw pay.Invalid(field: .unserviceable, message: "We ship in the United States only.")
    }
    return pay.Update(total: pay.Item(price: .parse(if c.address.state == "CA" { "43.20" } else { "40.00" })!))
  },
  onAuthorize: p => {
    try await charge(p.token)   // done: paid; what it throws shows in the sheet
  },
)

component Checkout {
  state placed = ""
  let wallet = load pay.available(request())

  Column {
    if wallet.value?.canPay ?? false {
      pay.PayButton {
        type: .buy
        variant: .automatic   // black in light mode, white in dark
        height: 48
        on press {
          do {
            let p = try await pay.pay(request())
            placed = "Thanks, {p.shipping.name}!"
          } catch pay.PayError.canceled {
          } catch e {
            placed = e.message
          }
        }
      }
    }
    Text(placed)
  }
}

// The app's own: its server charges the token.
async fn charge(token: string) throws {}
```

`pay.available(request)` says what the wallet can do for the request's
cards: its `wallet`, `canPay` (show the button) and `hasCard` (the user
has one it takes). Show `PayButton` only where `canPay` holds; it's the
wallet's own (`PKPaymentButton`, Google's `PayButton`, Safari's and
Google's on the web), whose brand guidelines ask that apps don't draw
their own: its words (its `type`: `.buy`, `.checkout`, `.book`,
`.donate`, `.order`, `.subscribe`, `.plain`, and Apple's others) and logo
are in the user's language, and its `variant` is `.black` on light
backgrounds, `.white` on dark ones, `.whiteOutline` on white, or
`.automatic`. Its height is the wallet's (44 points on iOS, 30 on the
Mac, Google's 48); give it the width. Where Apple Pay has no card yet,
Apple's guidelines show the `.setUp` button, which calls `pay.setUp()`
to add one (or the Buy button, whose sheet adds one too); Google Pay's
sheet adds a card itself.

`pay.pay(request)` shows the sheet and returns the `Payment` once it
closes, or throws `PayError.canceled` (and `.busy` while another sheet
shows). Amounts are [`std/decimal`](stdlib.md)'s `Decimal`s, exact, in
the currency's major unit (`price: .parse("12.75")!`, at most two
decimals for Google Pay; negative for a discount); each platform gets
the same number (Apple's `NSDecimalNumber`, Google's text). The
package's first release took them as text (`amount: "12.75"`), which
it still reads, for one more release, where the `price` is zero. The `total`'s label is the business's name, as the card
statement shows it ("Pay Asuka Tea" in Apple Pay's sheet); the `items`
are the lines above it, their `kind` for Google Pay's sheet. The request
also says the cards taken (`networks`, `cards`, `cardCountries`), what to
ask for with the shipping and billing contacts (`.name`, `.email`,
`.phone`, `.address`), the `shippingType` and `shippingMethods` (the
first chosen), where Google Pay ships (`shippingCountries`), and Apple
Pay's coupon codes (`coupons`). `recurring` makes it a subscription's
payment, with its regular billing and a trial; `deferred`, one charged
later (a hotel); `automaticReload`, a stored balance's top up. The wallet
shows the charges to come, and its token is a merchant token for them.

The handlers are async, and run while the sheet waits (about 30 seconds
at most), so they may call the app's server: `onShippingContact` as the
user picks an address (the wallets give only its city, state, postal
code and country before the payment: enough for the tax and the shipping
methods), `onShippingMethod`, `onPaymentMethod` (a card's surcharge) and
`onCoupon`. Each answers an `Update` (new items, total or methods; what's
none stays), or throws `pay.Invalid(field:, message:)`, which the sheet
shows by its field: `.shippingPostalCode`, `.unserviceable` for an
address the app doesn't ship to, `.couponInvalid`; its `update` is what
the sheet shows meanwhile, and an `Update`'s `errors` give several. Write their
messages for the user. `onAuthorize` gets the `Payment` as the user pays
(Face ID, Touch ID, a PIN): send its `token` to the server, which charges
it. What it throws, the sheet shows, and the user may try again; done,
it closes the sheet with a check mark, and `pay` returns the payment
with its card, contacts and shipping method.

The token is what the processor's SDKs send. Apple Pay's is the
`PKPaymentToken`'s `paymentData`, a JSON object (Stripe takes it as
`pk_token`, Braintree and Adyen as the Apple Pay token), signed for the
merchant ID's payment processing certificate, which the processor holds.
Google Pay's is tokenized by the request's `google.gateway`, with the
gateway's parameters (Stripe's publishable key, Adyen's or Braintree's
merchant account), or encrypted for the app's server with `directKey`.

Apple Pay needs a merchant ID (developer.apple.com, Identifiers, Merchant
IDs) with the processor's payment processing certificate, in
`asuka.toml`:

```toml
[applePay]
merchantIds = ["merchant.com.example.tea"]
```

An app that imports the package gets the in-app-payments entitlement
with them; its App ID needs the Apple Pay capability with the same IDs,
for the profile of a release build (a Mac app needs one too: see
[On a Mac](#on-a-mac)). The simulator's sheet pays with its test cards,
which return a token no processor charges. On the web, Apple Pay needs
the site's domain registered for the merchant ID, and
`onValidateMerchant`: the app's server asks Apple for a merchant session
with the merchant identity certificate. Google Pay's `TEST` environment
needs no account and returns test tokens; `pay.Google(production: true)`
needs the merchant ID of the Google Pay & Wallet Console (`merchantId`),
and the app approved there. An Android app that imports the package gets
the Google Pay library (`play-services-wallet`), its API enabled in the
manifest and the service its sheet calls back; others go without.

On iOS and the Mac it's PassKit's `PKPaymentAuthorizationController`
and `PKPaymentButton`. On Android it's the Google Pay API: `isReadyToPay`,
`loadPaymentData` through the Activity Result API, its dynamic price
updates and authorization through its callbacks service. In a browser,
Apple Pay where the browser has it (Safari, and an iPhone's or iPad's
browsers) through the Payment Request API's Apple Pay method; Google Pay
elsewhere, through Google's own web API (`pay.js`, loaded from
pay.google.com), which has the same request and callbacks as Android in
every browser. `pay` is called from the button's press: the browsers
show a sheet only in answer to one. Windows and Linux have no wallet for
apps: `available` says `.none`, `pay` throws `PayError.notAvailable` and
`PayButton` shows nothing.

Tests play the user with `pay.fake()`, a `FakeWallet`: while `pay` shows
its sheet (`sheet` isn't none), it changes the address, the method or
the coupon, and pays (`authorize`) or closes the sheet (`cancel`), and
the app's handlers answer:

```asuka
import platform/pay

test "the tax, then paid" {
  let w = pay.fake()
  let app = mount(Checkout())
  await app.idle()
  app.tap("PayButton")
  await app.idle()
  w.changeShippingContact(pay.Contact(address: pay.PostalAddress(state: "CA", countryCode: "US")))
  await app.idle()
  expect(w.sheet!.total.price.string() == "43.2")
  w.authorize(pay.Payment(token: "tok_visa", shipping: pay.Contact(name: "Ana")))
  await app.idle()
  app.expect("Thanks, Ana!")
}

component Checkout {
  state placed = ""

  pay.PayButton {
    on press {
      let p = try await pay.pay(pay.Request(
        total: pay.Item(label: "Asuka Tea", price: .parse("40.00")!),
        shipping: [.address],
        onShippingContact: c => pay.Update(total: pay.Item(price: .parse(if c.address.state == "CA" { "43.20" } else { "40.00" })!)),
      ))
      placed = "Thanks, {p.shipping.name}!"
    }
  }
  Text(placed)
}
```

In a test, a native component shows as its name, and a tap
on it is its `press`. [examples/checkout](../examples/checkout) sells a
tea set this way.

## Face ID and fingerprints: platform/biometric

`biometric.authenticate(reason:)` shows the system's prompt (Face ID,
Touch ID, Android's BiometricPrompt; Touch ID on a Mac, Windows Hello on
Windows, a passkey's WebAuthn prompt in a browser) and returns if it's
the device's owner, or throws `BiometricError.canceled`, `.notAvailable`
(no biometry and no passcode, or locked out) or `.failed`. It waits
without holding the main thread.

```asuka
import platform/biometric

component Lock {
  state unlocked = false

  Button(if unlocked { "Unlocked" } else { "Unlock" }) {
    on press {
      do {
        try await biometric.authenticate(reason: "Unlock your notes")
        unlocked = true
      } catch biometric.BiometricError.canceled {
      } catch e {
        unlocked = false
      }
    }
  }
}
```

- `reason` says why (iOS's subtitle; Android's title); `cancel:` is the
  cancel button's title.
- The user can use the device's passcode, PIN or pattern instead, unless
  `noPasscode: true`: biometry only (on Android, a strong one, Class 3).
- `biometric.check()` says what the device has: its `kind` (`.face`,
  `.fingerprint`, `.iris`, `.optic`), whether one is `enrolled`, and
  whether it has a `passcode`.

Linux has no system prompt for the owner: `check` says there's nothing,
and `authenticate` throws `.notAvailable`.

## Recording sound: platform/record

`record.audio()` records the microphone until `record.stopAudio()` (or
`maxDuration`) and gives the file: AAC in an MPEG-4 file (.m4a) in the
app's caches (or at `path`), which `platform/audio` plays. It asks for the
microphone the first time. `pauseAudio` and `resumeAudio` pause and go
on; `audioStatus()` is where it is (`level`, from 0 to 1, for a meter;
`elapsed`). `quality: .music` keeps more than a voice needs.

```asuka
import platform/record

component Memo {
  state recording = false
  state file = ""

  Button(if recording { "Stop" } else { "Record" }) {
    on press {
      if recording {
        record.stopAudio()
      } else {
        recording = true
        defer { recording = false }
        file = try await record.audio(maxDuration: 60s)
      }
    }
  }
}
```

`background: true` keeps it recording out of sight: on iOS the app needs
`"audio"` in asuka.toml's `ios.backgroundModes`; on Android it's a
foreground service whose notification says `title`, with a button to
stop. AVAudioRecorder on iOS and the Mac, `MediaRecorder` on Android and
in a browser, Media Foundation on Windows, GStreamer on Linux.

## Speech to text: platform/transcribe

`transcribe.file(path)` turns a recording into text, on the device:
nothing leaves it. Its `Result` has the `text`, its `segments` (each
word's or phrase's start and duration, in seconds, where the platform
gives them) and the `language` it heard. `transcribe.check(language)`
says whether the device can, before you record.

```asuka
import platform/transcribe

async fn caption(recording: string) throws -> string {
  let can = try await transcribe.check("en-US")
  if !can.onDevice { return "" }
  let r = try await transcribe.file(recording, language: "en-US")
  return r.text
}
```

The Speech framework on iOS and the Mac, asked to stay on the device;
Android's on-device recognizer (Android 13 and later); in a browser,
its own recognizer where it runs on the device (Chrome 139 and later),
which hears the recording played into it, so a file takes about its own
length and has no `segments`. Where the device can't (Windows, Linux, an
older Android, a browser without one), it throws
`TranscribeError.notAvailable` rather than send the sound to a server;
`.downloading` while the language's model comes (a browser starts
installing it then).

`transcribe.listen(language)` is dictation: it listens to the
microphone and gives what's said as it's said, a `Heard` each time, with
all the `text` so far (its last words may still change). The last one is
`final`: once `transcribe.finish()` is called, or the recognizer stops
hearing speech. Stopping the task that reads it stops listening at once,
keeping nothing more. It asks for the microphone and speech recognition
the first time, and runs on the device on the same platforms as `file`.

```asuka check
import platform/transcribe

component Dictation {
  state on = false
  state text = ""

  task(on) {
    if !on { return }
    for await h in transcribe.listen() {
      text = h.text
    }
    on = false
  }

  Column {
    Text(text)
    Button(if on { "Done" } else { "Dictate" }) {
      on press {
        if on { transcribe.finish() } else { on = true }
      }
    }
  }
}
```

## The screen: platform/screen

```asuka
import platform/screen

component Player {
  state note = ""

  task {
    screen.lock(.landscape)               // a video player
    screen.setBars(style: .light, hideStatus: true)
    screen.preventCapture(true)           // a ticket's code
    defer {
      screen.lock(.any)
      screen.setBars()
      screen.preventCapture(false)
    }
    for await _ in screen.screenshots() { note = "That screenshot shows your balance" }
  }

  Text(note)
}
```

- `screen.lock(o)` turns the app to `o` and keeps it there: `.portrait`,
  `.portraitUp`, `.landscape`, `.landscapeLeft`, `.landscapeRight`;
  `.any` unlocks it. Only the orientations `asuka.toml` allows. iPads in
  Split View or Stage Manager, and Android 16's large screens, ignore it.
- `screen.setBars(style:, hideStatus:, hideNavigation:)` sets the status
  bar's style (`.auto` follows dark mode; `.light` is light icons, for a
  dark picture under them; `.dark`, dark ones), and hides the status bar
  or Android's navigation bar (a swipe from the edge shows them a
  moment). The safe area follows.
- `screen.preventCapture(on)` keeps the app out of screenshots and
  recordings. Android makes the window secure: captures are black, and
  the recent apps show it blank. iOS can't stop a screenshot: it covers
  the app while the screen is recorded, mirrored or on AirPlay.
- `screen.screenshots()` is a stream of the screenshots the user takes
  (iOS; Android 14 and later).
- `screen.setBrightness(level)` shows the screen at `level`, from 0 to 1,
  while the app is in front (a boarding pass's code at 1, for the
  scanner); `none` gives it back to the system's brightness. The user's
  comes back as the app leaves and the app's as it returns: Android sets
  the window's own, iOS sets the screen's and puts the user's back.
  `screen.brightness()` is the brightness now.

A desktop window has no orientation or system bars: `lock` and
`setBars` do nothing there, and its displays' brightness is the user's
(`setBrightness` does nothing, `brightness()` is `none`). `preventCapture` keeps the app's windows out
of screenshots and recordings on the Mac and Windows (not on Linux); a
desktop doesn't tell the app of its screenshots. A browser locks the
orientation only full screen or in an installed app, hides the bars by
going full screen, and has neither, nor a brightness to set.

These are the app's, not a screen's: a screen that sets one sets it back
as it goes (a `defer` in its task).

## The app's icon: platform/appicon

`asuka.toml` lists the icons the app may show besides its own, and
each build makes them as it makes the app's icon:

```toml
icon = "icon.png"

[alternateIcons]
dark = "icons/dark.png"
classic = "icons/classic.png"
```

```asuka check
import platform/appicon

component IconPicker {
  state shown = appicon.current() ?? "default"

  Column {
    gap: 8
    for name in ["default"] + appicon.names() key name {
      Button(name) {
        on press {
          try? await appicon.set(if name == "default" { none } else { name })
          shown = appicon.current() ?? "default"
        }
      }
    }
    Text("Showing {shown}")
  }
}
```

- `appicon.names()` are `alternateIcons`' names, `appicon.current()`
  the one the app shows (`none`: its own).
- `appicon.set(name)` shows one, and `set(none)` the app's own again.
  The choice stays across launches and updates. A name the build has
  no icon of throws `AppIconError.unknown`.
- iOS changes the home screen's icon and tells the user so itself, in
  an alert the app can't leave out; one set while the app is in the
  background changes as it comes back. Android changes the launcher's
  icon as the app leaves the screen (changing it while the app shows can
  close it): the app drawer shows the new one, and a home screen
  shortcut to the old one goes.

## The launch screen: platform/splash

`asuka.toml`'s `splash` is the launch screen: an image (a square PNG, a
logo), `size` points wide (default 120, at most 160), in the middle of a
color, in light and dark mode.

```toml
[splash]
image = "logo.png"
size = 96
background = "#4A63B5"
darkBackground = "#1B2440"
```

It shows from the tap on the icon until the app's first frame: iOS's
launch screen, then a copy of it over the app until the first frame is
ready; Android 12's splash screen (the image in the middle of the icon's
circle) and, before, the window's background. The desktops have no
launch screen. On the web, when asuka.toml sets its `background`, its
colors (not its image) are the page's while it loads, until the app's
first frame, held or not. On the desktops and the web `hold` and `hide`
do nothing. To keep it up while the app loads, call `splash.hold()` as
it starts (in the root component) and `splash.hide()` once it's ready:

```asuka
import platform/splash

component Root {
  state ready = false

  task {
    splash.hold()
    await loadSession()
    ready = true
    splash.hide()
  }

  if ready { Home() }
}

async fn loadSession() { await sleep(300ms) } // the app's own: what its first screen needs
component Home { Text("Signed in") }
```

## Maps: platform/maps

`maps.View` is a map: Apple Maps on iOS and the Mac, Google Maps on
Android, OpenStreetMap's tiles on Windows and in a browser, and
libshumate's on Linux. Bind its `region` (a center and how many degrees
it spans) to move it and follow the user's moves; `markers` are its
pins, and `markerPress`, `press` and `regionChange` what the user does.

```asuka
import platform/maps

component Places {
  state region = maps.Region(center: maps.Coordinate(lat: 37.77, lng: -122.42), latDelta: 0.08, lngDelta: 0.08)
  state picked = ""

  Column {
    maps.View {
      bind region: region
      markers: [maps.Marker(id: "hq", at: maps.Coordinate(lat: 37.79, lng: -122.39), title: "HQ", color: .blue)]
      mapType: .muted
      grow: 1
      on markerPress(id) { picked = id }
    }
    Text(picked)
  }
}
```

`mapType` is `.standard`, `.satellite`, `.hybrid` or `.muted`, and
`showsUser` shows the user's location (ask for it first, with
`platform/location`). OpenStreetMap's maps show streets (in a browser,
`.muted` in quieter colors), and the user's location only in a browser. On Android, an app that imports the package is built with
Google's Maps SDK and needs a Maps SDK for Android key in asuka.toml's
`android.mapsKey`.

## Background refresh: platform/periodic

`platform/periodic` runs the app's code now and then while it isn't on
screen: a sync, a fetch of what's new. `periodic.schedule` asks the
system to run a task; `periodic.runs(name)` is the stream of its runs,
read in a task of the app's root.

```asuka
import platform/periodic

component Root {
  task {
    periodic.schedule("sync", every: 1h, network: true)
    for await run in periodic.runs("sync") {
      do {
        try await syncInbox()
        run.finish(true)
      } catch {
        run.finish(false)   // Android tries it again
      }
    }
  }

  Text("Inbox")
}

// The app's own.
async fn syncInbox() throws {}

app { Root() }
```

The system decides when, from `every` (15 minutes at least), the
battery, the network, and how the user uses the app; it may wait much
longer, and the user can turn it off (iOS's Background App Refresh).
`network` runs it only online and `charging` only while charging;
scheduling a task again changes it. A `Run` has its `budget`, the time
it may take, a little less than the system's: about 25 seconds on iOS
(a processing task, for `charging`, has minutes), 9 minutes on Android
(WorkManager). `run.finish(ok)` says it's done: `false` asks Android to
retry it. One stream reads the runs: a run of another task that comes
to it is finished at once, so an app keeps to one task, its runs read
in one place.

When the system runs a task with the app closed, the app starts without
a screen: its root runs, and the task that reads `runs`, which gets the
run it was started for (it waits for that reader). If the user opens the
app then, its screen appears. `periodic.cancel(name)` stops a task.

A desktop doesn't launch an app for its tasks: on macOS
(`NSBackgroundActivityScheduler`), Windows and Linux they run on a timer
while the app runs, and one that wants power waits while on the battery.
A browser runs no page's tasks while it's closed: there `schedule` does
nothing.

## The stores: platform/review, platform/tracking, platform/integrity, platform/agerange

`review.request()` asks the store for its rating prompt (the App
Store's, Google Play's in-app review). The stores decide whether it
shows, on their own quotas, and don't say; ask after a success, never
from a button. For a "Rate us" button, `device.openURL(review.appStoreURL(id))`
or `review.playStoreURL(id)`.

`tracking.request()` shows iOS's App Tracking Transparency prompt, which
an app must show before it reads the advertising identifier or shares data
with ad networks, and returns `.allowed`, `.denied`, `.restricted` or
`.notAsked`; `tracking.current()` reads it without asking. Android has
no such prompt: it's `.allowed`.

`platform/integrity` proves to your server that a request comes from your
app on a real device. iOS: App Attest. `integrity.generateKey()` makes a
key (keep its ID), `integrity.attestKey(id, challenge)` has Apple attest
it for your server's challenge, once, and `integrity.assertion(id,
data)` signs each request after. Android: Play Integrity.
`integrity.prepare(cloudProjectNumber:)` at launch, then
`integrity.token(requestHash:)` for each request. The server checks what
they return with Apple or Google. `integrity.supported()` is false on the
iOS simulator.

`agerange.request(13, 18)` asks for the user's age range without their
birth date, for the laws that ask apps to know a child: Apple's Declared
Age Range (iOS 26) and Play's Age Signals. The platform may ask the user
or their parent to share. The `Range` has a `status`, `.shared` with a
`lower` and `upper` age (0: open), `.declined`, or
`.verificationRequired` (Play); its `source` says how it's known.
`AgeRangeError.notAvailable` where the platform can't answer.

```asuka
import platform/agerange
import platform/review
import platform/tracking

async fn afterFirstOrder() throws {
  try? await review.request()
  if try await tracking.request() == .allowed { print("personalized ads") }
  let r = try await agerange.request(13, 18)
  if r.status == .shared && r.lower >= 18 { print("an adult") }
}
```

Off the phones: the Mac has the App Store's rating prompt, the tracking
prompt, and App Attest (with a provisioning profile: see
[On a Mac](#on-a-mac)), but not Declared Age Range. Windows, Linux and
the web have no store prompt (`review.request` does nothing), no
attestation (`integrity.supported` is false) and no age range
(`.notAvailable`). Tracking needs no prompt there: Windows answers
`.allowed` while the advertising ID is on in Settings, Linux `.allowed`,
a browser `.denied` when it sends Global Privacy Control or Do Not Track.

Selling in the app, `platform/purchase`, has a page of its own:
[In-app purchases](purchases.md). So does iCloud's key-value store and
documents, `platform/icloud`: [iCloud](icloud.md).

## Motion and sensors: platform/sensors

`platform/sensors` reads the device's motion and environment sensors, each
a stream that runs while it's read:

```asuka
import platform/sensors

component Level {
  state roll = 0.0

  task {
    for await m in sensors.motion(every: 50ms) { roll = m.attitude.roll }
  }

  Text("{(roll * 180 / 3.14159).fixed(0)}°")
}
```

- `sensors.accelerometer(every:)`, `gyroscope(every:)` and
  `magnetometer(every:)` give a `Vector` (`x`, `y`, `z`, `time`) about
  every interval: acceleration in g, rotation in radians per second, the
  magnetic field in microteslas (calibrated).
- `sensors.motion(every:)` is the fused device motion: `gravity` and the
  user's `acceleration` (in g), `rotation`, and the `attitude`: a `matrix`
  that turns device axes into East-North-Up, the `heading` (degrees from
  magnetic north; -1 without a magnetometer), and `pitch` and `roll`.
- `sensors.pressure(every:)` is the barometer (hectopascals, and the
  change in altitude), `sensors.light(every:)` the ambient light in lux
  (Android, Linux), and `sensors.stepUpdates()` the steps since watching
  began. `sensors.steps(from:, to:)` (times as `time.now()` gives them) is
  iOS's history (a week); elsewhere it throws `SensorError.notAvailable`.
- `sensors.available(sensor)` says whether the device has one
  (`Sensor.all` lists them); the simulator and emulators have few. A Mac
  and a PC have none; Linux has the light sensor and the accelerometer's
  orientation on convertibles and tablets (iio-sensor-proxy); a browser
  has the Generic Sensor API or device motion events, without a barometer
  or pedometer. A stream of a sensor the device lacks throws
  `notAvailable`.
- `sensors.simulate(json)` feeds the iOS simulator, which has no motion
  sensors, a reading in Core Motion's terms (`kind` 0 the accelerometer,
  3 motion; `acceleration`, `gravity`, `user`, `rotation`, `field`).

The values are the same everywhere: device axes as Android and the W3C
define them (x to the right of the screen, y up it, z out of it),
acceleration as the accelerometer measures it (a device lying face up
reads z = +1 g; iOS's are turned to match), and the attitude computed the
same way on every platform. Up to 200 readings a second; they come on the
main thread.

In a test, `sensors.fake()` gives the package a `FakeSensors`: the
sensors `present`, and readings sent through it (`accelerate(x, y, z)`,
`rotate`, `move(gravity, user:, rotation:, field:)`, `press(hPa)`,
`shine(lux)`, `walk(steps)`), each to the streams that read the sensor,
no more often than they asked.

```asuka
import platform/sensors

test "lying face up" {
  let s = sensors.fake()
  s.accelerate(0, 0, 1)
}
```

## Editing images and video frames: platform/imageedit

`imageedit.edit(source, actions)` edits an image with the platform's own
codecs and writes it as JPEG or PNG: what the camera and the photo
picker give (HEIC, JPEG, PNG, WebP...), turned upright as their
orientation says, for an upload, a cache or an `Image`. The actions run
in order: `.resize(width, height)` (a 0 keeps the aspect),
`.crop(x, y, width, height)`, `.rotate(degrees)`, `.flipHorizontal`,
`.flipVertical`, `.fill(...)` (a solid color: what was there is gone)
and `.pixelate(...)`. `imageedit.size(source)` reads an image's size;
`imageedit.videoFrame(source, at:)` takes a video's frame as an image.

```asuka
import platform/imageedit

async fn thumbnail(photo: string) throws -> string {
  let r = try await imageedit.edit(photo, [.resize(1080, 0), .crop(0, 0, 1080, 1080)], quality: 0.8)
  return r.path
}

async fn poster(video: string) throws -> string {
  return (try await imageedit.videoFrame(video, at: 1s, actions: [.resize(640, 0)])).path
}
```

Core Graphics and Image I/O on iOS and the Mac, `Bitmap` and
`MediaMetadataRetriever` on Android, a canvas in a browser, WIC and
Media Foundation on Windows, GdkPixbuf and GStreamer on Linux. A source
that isn't an image throws `EditError.noImage`.

### A view as an image

`imageedit.capture(view: ref)` draws the view a `Ref` holds, as it shows
now with everything in it, into a PNG (or `format: .jpeg`) in the app's
caches: a card to share, a receipt to save. The platform draws it, so its
widgets look as they do on the screen. `scale` is pixels per point (the
screen's by default).

```asuka check
import platform/device
import platform/imageedit

component Receipt(total: string) {
  let card = Ref()

  Column {
    View {
      ref: card
      padding: 16
      radius: 12
      background: #f2f2f7
      Text("Paid {total}")
    }
    Button("Share") {
      on press {
        do {
          let image = try await imageedit.capture(view: card)
          try await device.shareFiles(paths: [image.path], from: card)
        } catch e {
          print(e.message)
        }
      }
    }
  }
}
```

iOS and the Mac draw the view's layers, and Android draws its `View`
(the window's pixels where a video or a map is in it). Windows and Linux
take the window's pixels where the view is (Windows at the screen's
scale, whatever `scale` asks). A
browser copies the elements into an SVG image, so an image from another
site that doesn't allow it (CORS) comes out blank there.

## Text to speech: platform/speech

`speech.speak(text)` speaks, in the device's voice for the text's
language (or `Options(language:, voice:, rate:, pitch:, volume:)`), and
returns when it's done; what's spoken while it speaks waits its turn.
`stop()` stops (the speaking calls throw `SpeechError.stopped`),
`pause()` and `resume()` pause at a word and go on, `speaking()` says
whether it is, and `voices()` lists the voices, with their `quality`.

```asuka
import platform/speech

component Reader(text: string) {
  Row {
    gap: 12
    Button("Read") { on press { try await speech.speak(text, options: speech.Options(rate: 1.1)) } }
    Button("Stop") { on press { speech.stop() } }
  }
}
```

AVSpeechSynthesizer on iOS and the Mac, Android's `TextToSpeech`, the
browser's `speechSynthesis`, SAPI on Windows, Speech Dispatcher on
Linux. Other apps' sound ducks while it speaks.

## Contacts: platform/contacts

```asuka
import platform/contacts

component Invite {
  state who = ""

  Column {
    Button("Choose a friend") {
      on press {
        if let c = try? await contacts.pick() {
          who = "{c.name}, {c.phones.count()} phones"
        }
      }
    }
    Text(who)
  }
}
```

- `contacts.pick()` shows the system's contact picker: the user picks
  one, and the app sees only that one, with no permission asked (on
  Android, without the permission, its name only). It answers none if
  the user cancelled.
- `contacts.add(contact)` shows the system's new contact screen, filled
  in, where the user saves it, and answers whether they did; no
  permission asked.
- `contacts.request()` asks for access (`.notAsked`, `.denied`,
  `.limited`, `.allowed`), and `contacts.list(name:, limit:)` reads the
  address book (by name, up to a limit): each `Contact`'s names,
  `organization` and `jobTitle`, `phones` and `emails` with their
  labels, `birthday`, `note`. iOS 18's limited access lists the contacts
  the user chose. Without access, `list` throws "contacts: not allowed".

On a Mac it's the Contacts framework: `pick` is the system's picker, a
popover from the window's top, and `add` asks in an alert over the
window (macOS has no new contact screen), then saves. Linux reads and
writes Evolution Data Server's address books, as [On Linux](#on-linux)
says. Windows gives the contacts to packaged apps only: `request` says
`.denied`, `pick` throws, and `add` opens a vCard in the user's contacts
app. A browser picks one where it has the Contact Picker (Chrome on
Android), and `add` downloads a vCard.

## Calendars: platform/calendar

```asuka
import platform/calendar
import std/time

async fn addDemo() throws -> bool {
  let start = time.date(2026, 12, 1, hour: 15)
  return try await calendar.add(calendar.Event(title: "Demo", location: "Room 2", start: start, end: start + 30min))
}
```

- `calendar.add(event)` shows the system's event editor, filled in,
  where the user saves the event, and answers whether they did; no
  permission asked (iOS 17 and later). iOS says whether it was saved;
  Android's calendar apps don't.
- `calendar.request()` asks for access (`.notAsked`, `.denied`,
  `.writeOnly`, `.full`); with `.full`, `calendar.calendars()` lists the
  calendars and `calendar.events(from:, to:, calendars:)` the events
  between two times, each occurrence of a repeating one. Without it,
  they throw "calendar: not allowed".
- An event's `start` and `end` are `time.Time`s. An all-day event's
  are days: midnight UTC of the first, and of the day after the last.

On a Mac it's EventKit: `add` asks in an alert over the window (macOS
has no event editor for apps), then saves. Linux reads and writes
Evolution Data Server's calendars. Windows gives the calendars to
packaged apps only: `request` says `.denied`, and `add` opens an
iCalendar file in the user's calendar app (and answers false: Windows
doesn't say). A browser can't read calendars; `add` downloads an `.ics`
file.

## Home screen widgets: platform/widget

`asuka.toml`'s `widgets` declares the app's widgets; `widget.set` fills
them, from the app's code:

```toml
[[widgets]]
kind = "today"
name = "Today"
description = "What's next"
sizes = ["small", "medium", "rectangular"]
```

```asuka
import std/time
import platform/widget

fn showToday(meeting: time.Time) {
  widget.set("today", widget.Entry(
    title: "Next", symbol: "calendar", value: "3 tasks", body: "Call Ana at 4",
    progress: 0.4, showProgress: true, until: meeting, url: "myapp://today",
  ))
}
```

A widget shows its `Entry` with the platform's template, laid out for
its size: the `title` (with its `symbol`, in the `tint`), a large `value`,
the `body`, a `progress` bar, a live countdown to `until` (a
`time.Time`), a `caption`. It keeps showing it until the app sets
another, whether or not the app runs; tapping it opens the app with its
`url` (the app's `openURL`). `widget.setAll(kind, entries)`, each entry
with its `at`, is a timeline: each shows from its time.
`widget.clear(kind)` empties it.

Sizes: `small`, `medium`, `large`, and iOS's `extraLarge` (iPad) and
lock screen `circular`, `rectangular` and `inline`. iOS: a WidgetKit
extension the CLI builds into the app, sharing an App Group with it
(a release needs the App Groups capability, and a profile for the
extension's ID, the app's plus `.widgets`). Android: an App Widget for
each kind, Material You colors, sized to the launcher's cells. The
desktops and the web have none: `widget.set` does nothing there.

## Live activities: platform/liveactivity

A live activity shows what's happening now outside the app, as it
changes: a delivery on its way, a ride, a game's score, a timer. On iOS
it's a Live Activity (iOS 16.2): the lock screen, the Dynamic Island, a
banner on an iPhone without one. On Android it's an ongoing
notification, which Android 16 promotes to a Live Update: at the top of
the shade and the lock screen, with a chip in the status bar.

```asuka
import platform/liveactivity
import std/time

async fn followOrder(eta: time.Time) throws {
  let a = try await liveactivity.start(liveactivity.Content(
    title: "Order #1042", subtitle: "Ana is picking it up", value: "12 min",
    symbol: "bag.fill", until: eta, url: "myapp://orders/1042",
  ))

  // Later:
  a.update(liveactivity.Content(title: "Order #1042", subtitle: "2 stops away", value: "4 min", until: eta),
    alert: liveactivity.Alert(title: "Almost there", body: "Ana is 2 stops away"))
  a.end(liveactivity.Content(title: "Order #1042", subtitle: "Delivered"))
}
```

An activity shows its `Content` with the platform's template, which
leaves out what's empty: the `title` and `subtitle`, the `value` (large
at the side; the Dynamic Island's compact view; Android 16's status bar
chip), the `symbol` in the `tint`, a `progress` bar, and a countdown to
`until` (a `time.Time`) that runs without the app. With
`since`, `until` and `showProgress`, the bar fills by itself. Tapping it
opens the app with its `url` (the app's `openURL`).

`liveactivity.start(c, push:, relevance:)` runs while the app is on
screen (neither platform lets an app in the background start one);
`relevance` orders the app's activities. An `Alert` lights the screen up
with the update: iOS expands the Dynamic Island and shows it on an Apple
Watch; Android alerts with the notification again. `end(c, dismiss:)`
leaves the last content showing until the system takes it (`.later`,
the default: 4 hours at most), until a time (`.at(t)`), or takes it at
once (`.now`). `liveactivity.enabled()` answers whether the user allows
them (Android asks for notifications at the first `start`). The package
doesn't list the activities still running.

`liveactivity.events()` says what happens to them: one `.ended` (by the
user, the system or a server), one `.started` by a server, and the push
tokens (`.pushToken`, `.pushToStartToken`), each an `Event` with its
`activity` and `token`. With `push: true`, a server updates and ends an
activity while the app is away: send its push token and its `id` to the
server. On iOS it sends an APNs push of the `liveactivity` type, whose
`content-state` is the content as JSON (`Content`'s fields, with `since`
and `until` as `sinceMs` and `untilMs`); on Android, a Firebase data
message with `asuka.liveActivity` (`update`, `end`), `asuka.id` and
`asuka.content`. A server starts one with iOS 17.2's push-to-start token
(the attributes type `AsukaActivityAttributes`, with the new activity's
`id`), or on Android with `asuka.liveActivity` set to `start`. Pushing
needs `platform/push` in the app: the Push Notifications capability on iOS,
Firebase on Android.

The iOS build adds the widget extension for an app that calls the
package, with the activities' template (SwiftUI's, as the widgets'), and
`NSSupportsLiveActivities`; a release needs a profile for the
extension's ID, as widgets do. The user can turn Live Activities off for
the app in Settings, and Android 16's promotion in its notification
settings. The desktops and the web have none: `enabled()` is false and
`start` throws "liveactivity: not available".

## Shortcuts: Siri, Spotlight, the launcher

`asuka.toml`'s `shortcuts` are actions the system offers for the app.
Each opens the app with its URL, for the app's `openURL`:

```toml
[[shortcuts]]
id = "newNote"
title = "New Note"
url = "notes://new"
symbol = "square.and.pencil"
phrases = ["New note in ${app}"]
```

On iOS they're App Shortcuts: Siri hears their phrases (each with
`${app}`, the app's name), and they're in Spotlight, the Shortcuts app
and on the Action button, with no setup by the user (up to ten). The CLI
compiles them as App Intents and extracts their metadata as Xcode does.
On Android they're the launcher's shortcuts, shown on a long press of
the app's icon, with the symbol in the accent color. The desktops and
the web don't add them.

Simulator builds are signed as Xcode signs them, with the keychain's
Apple Development identity (`asuka.toml`'s team's): App Intents only run
in an app with a team. Without one, the build is signed ad hoc, and the
CLI says the shortcuts won't run.

### Parameters

A shortcut's `params` are what Siri and the Shortcuts app ask for. Each
fills its `{name}` in the URL, escaped:

```toml
[[shortcuts]]
id = "addTask"
title = "Add Task"
url = "todo://add?title={title}&list={list}"
phrases = ["Add a task in ${app}", "Add a ${list} task in ${app}"]

[[shortcuts.params]]
name = "title"
title = "Title"
prompt = "What's the task?"

[[shortcuts.params]]
name = "list"
title = "List"
type = "options"
options = [{ value = "work", title = "Work" }, { value = "home", title = "Home" }]
default = "home"
```

"Add a task in Todo" asks *What's the task?* and opens
`todo://add?title=Buy%20milk&list=home`; "Add a work task in Todo" goes
straight to the title. A param's `type` is `string` (the default), `int`,
`float`, `bool` or `options`. Siri asks for one with no `default` unless
it's `optional`, which leaves its place in the URL empty. A phrase can
name an `options` param (`${list}`): Siri hears each option's title.
Other types can't be in a phrase, since Siri can't hear just any text.
In the Shortcuts app, every param is a field of the action. On Android,
the launcher's shortcut opens the URL with each param's default, or
empty.

## Haptics: device.haptic

`device.haptic(kind)` plays haptic feedback
(UIFeedbackGenerator; `performHapticFeedback` on Android), a
`device.Haptic`: `.selection`,
`.light`, `.medium`, `.heavy`, `.success`, `.warning` or `.error`. A Mac
plays the trackpad's feedback (a Force Touch trackpad: an alignment for
`.selection`, a level change for the rest); a browser vibrates where it
can (Chrome on Android); Windows and Linux have no haptics, and do
nothing.

```asuka
import platform/device

component Save {
  Button("Save") { on press { device.haptic(.success) } }
}
```

See [Motion](motion.md) for feedback that goes with animation.

## Crash reports: platform/crash

An app that imports `platform/crash` keeps a report of each crash, and reads
it on its next launch, to send where it likes: its own server, Sentry,
Crashlytics. Asuka uploads nothing.

```asuka
import platform/crash

component Root {
  task {
    crash.set("user", "u42")
    for await r in crash.reports() {
      if (try? await send(r)) ?? false {   // your upload: a Report is JSON
        crash.delete(r.id)
      }
    }
  }

  Button("Check out") {
    on press {
      crash.breadcrumb("opened the cart")
      do {
        try await checkOut()
      } catch e {
        crash.record(e)   // an error handled, reported too
      }
    }
  }
}

// The app's own.
async fn send(r: crash.Report) throws -> bool = true
async fn checkOut() throws {}

app { Root() }
```

`crash.reports()` is a stream: the reports kept, oldest first, then each
that comes (a diagnostic iOS delivers a moment after launch, an error
recorded), once each. A report stays until `crash.delete(id)`; past 50
the oldest go. Its `id` is the same each time it's read, so a server can
tell one sent twice. `crash.record(e)` makes a report of an error the
app handled, `crash.breadcrumb(message)` adds to what the reports say
the app last did, and `crash.set(key, value)` sets a value they have
(`""` removes it).

A `Report` has its `id`, its `kind`, whether it was `fatal`, its `time`
(a `time.Time`), a `message` (a line: what crashed, and why),
its `text` (the whole report, as the platform wrote it), its `threads`
and its `context`. What's reported, as its `kind`, on each platform:

| | iOS | Android | macOS | Windows | Linux | Web |
|---|---|---|---|---|---|---|
| `"panic"`: Asuka's own fatal error, with the Asuka code's stack | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| `"exception"`: uncaught | NSException (Objective-C, Swift's `NSException`s) | a JVM `Throwable` | — | — | — | an uncaught JavaScript error (not fatal) |
| `"native"`: a signal, a Mach or Windows exception | MetricKit | the system's tombstone | MetricKit; a fatal signal, Asuka's report | Asuka's report | Asuka's report (a signal in native code) | — |
| `"hang"` | MetricKit's hangs (not fatal), the watchdog | ANRs, with their traces | MetricKit | — | — | — |
| `"oom"` | the memory limit, when MetricKit says | low-memory kills | — | — | — | — |
| `"resource"` | CPU and disk-write exceptions (not fatal) | excessive resource use | MetricKit | — | — | — |
| `"nonfatal"`: `crash.record(e)` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |

Asuka keeps the record on every platform: its own fatal error is
written with the Asuka stack as the run ends, and an uncaught exception
as the app ends (a handler set before, a crash SDK's, still runs
after). The rest the system hands over on a
later launch: iOS and macOS's MetricKit diagnostics, and Android's
record of how the app's processes ended
(`getHistoricalProcessExitReasons`, Android 11 and later; a native
crash's tombstone from Android 12). A crash the system reports again
after Asuka did (iOS's SIGABRT of an uncaught exception, Android's
`REASON_CRASH`) is left out. On macOS, Windows and Linux, a fatal signal
or an exception nothing handled is reported on the next launch.

Each report has its `context`: the app's version and build, the device's
model and system, the process and how long it had run, whether it was in
the foreground, and the last breadcrumbs (100 at most) and the values
set. They're written to the app's data directory as they change, about
as cheaply as a write, so they're on disk before the crash, whatever
kills the app. The lifecycle's changes are breadcrumbs too ("app:
background").

A report's `text` is what the platform gives (Asuka's crash output, a stack trace, a tombstone, an ANR's traces); its
`threads` are those stacks read into frames, the crashed one first. An
Asuka frame has its `function`, `file` and `line`. A native one has its
`binary`, the binary's UUID (Mach-O) or build ID (ELF) as `binaryId`, the
`address` and its `offset` in the binary: what a service symbolicates
with the build's symbols (see
[Shipping](shipping.md#symbols-for-crash-reports)).

**Sending to Sentry or Crashlytics.** For Sentry, make an event of each
report (its envelope endpoint, `https://<host>/api/<project>/envelope/`
with your DSN's key): `exception.values[0]` with the `kind` as its
`type`, the `message` as its `value`, and the crashed thread's frames as
its `stacktrace.frames`, outermost first (Sentry's order, the reverse of
a `Thread`'s), each with `function`, `filename` and `lineno`, or
`instruction_addr` and `package` for a native frame; and in
`debug_meta.images` each binary of the frames, `{"type": "macho",
"debug_id": binaryId, "image_addr": address - offset}` (`"elf"` on
Android), so the dSYMs or symbols you upload with `sentry-cli` apply.
The `context` goes in `contexts`, `tags` and `breadcrumbs`. Crashlytics
has no public endpoint to send to: send the reports to your server, or
record them in Crashlytics' SDK as non-fatal errors from your own native
package ([Native code](native.md)). Either way, App Store review asks
about crash data: if you send it, say so (`ios.collected` in
`asuka.toml`, `{type = "CrashData", purposes = ["AppFunctionality"]}`).

**On each platform.** iOS links MetricKit only into an app that reads
its reports, and delivers its diagnostics to the app, not its share
extension. MetricKit has a crash's process ID from iOS 17 on, which
ties it to its run's context; before, it's the run that was last before
it. It doesn't deliver on the simulator. Android needs nothing more;
before Android 11 there's no record of native crashes and ANRs. macOS:
MetricKit from macOS 12; an uncaught Objective-C exception that ends the
app is MetricKit's crash (AppKit catches those of the main thread and
logs them). Windows Error Reporting keeps its record as ever. In a
browser, the record is in the app's files (IndexedDB): the next launch
reads it. An uncaught JavaScript error, Asuka's or the app's
`web/*.js`, is an `"exception"` report at once, not fatal (the page goes
on), with the run's context; at most 20 a run, each once. A page marks
its run alive in the origin's `localStorage`, so the app's other open
pages leave its record alone. Under `asuka test` no record is kept:
`reports()` ends empty and the rest does nothing.

**Under `asuka dev`** the player keeps the record as a build does; its
own native crashes aren't the app's, and aren't reported to it; a build
of the app (`asuka dev --once`, `asuka build`) reports everything.
