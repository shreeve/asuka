package main

import (
	"errors"
	"fmt"
	"html"
	"os"
	"path/filepath"
	"regexp"
	"slices"
	"strconv"
	"strings"

	"github.com/egoist/asuka/internal/pkgs"
	"github.com/egoist/asuka/internal/toml"
)

// appConfig is an app's asuka.toml: what the stores and the platforms need
// to know about it. Every field is optional. The file is TOML, read
// through the fields' json tags (internal/toml).
//
//	name = "Counter"
//	id = "com.example.counter"
//	version = "1.2.0"
//	build = 7
//	icon = "icon.png"
//	accent = "#7c3aed"
//	accentDark = "#a78bfa"
//	orientations = ["portrait"]
//
//	[ios]
//	team = "ABCDE12345"
//
//	[ios.usage]
//	NSCameraUsageDescription = "To scan receipts"
//
//	[android]
//	permissions = ["android.permission.CAMERA"]
//	keystore = "upload.jks"
//	keyAlias = "upload"
//	dynamicColor = true
type appConfig struct {
	// Name is the name under the icon (default: the directory's name).
	Name string `json:"name"`
	// ID is the bundle ID and application ID (default: dev.asuka.<dir>).
	ID string `json:"id"`
	// Version is the version users see (default 1.0); Build counts the
	// builds uploaded, and must grow with each (default 1).
	Version string `json:"version"`
	Build   int    `json:"build"`
	// Icon is a square PNG, 1024×1024, without transparency for iOS.
	Icon string `json:"icon"`
	// AlternateIcons are the other icons the app may show
	// (platform/appicon's set), by name: square PNGs, as Icon.
	AlternateIcons map[string]string `json:"alternateIcons"`
	// Orientations: portrait, landscape (default both).
	Orientations []string `json:"orientations"`
	// C is the C the app's extern declarations call ([c], docs/c.md):
	// internal/appbuild reads it (and each dependency's) as it builds.
	C   pkgs.CConfig `json:"c"`
	IOS struct {
		Team string `json:"team"`
		// Profile is a provisioning profile's path, name or UUID (default:
		// the best match installed for the ID and distribution).
		Profile string `json:"profile"`
		// ShareProfile is the share extension's (shareTarget), whose ID is
		// the app's plus ".share" (default: the best match installed).
		ShareProfile string `json:"shareProfile"`
		// WidgetProfile is the widget extension's (widgets), whose ID is
		// the app's plus ".widgets" (default: the best match installed).
		WidgetProfile string `json:"widgetProfile"`
		// NotificationProfile is the Notification Service Extension's
		// (platform/push's rewrite), whose ID is the app's plus ".notification"
		// (default: the best match installed).
		NotificationProfile string `json:"notificationProfile"`
		// Usage are the purpose strings for protected resources
		// (NSCameraUsageDescription, ...).
		Usage map[string]string `json:"usage"`
		// InfoPlist adds string keys to Info.plist.
		InfoPlist map[string]string `json:"infoPlist"`
		// LocationPurposes are the reasons location.RequestPrecise shows,
		// by the purpose key it's given (Info.plist's
		// NSLocationTemporaryUsageDescriptionDictionary; default: "default",
		// "To show exactly where you are.").
		LocationPurposes map[string]string `json:"locationPurposes"`
		// NonExemptEncryption is what a build says of its encryption
		// (ITSAppUsesNonExemptEncryption), a legal statement that's yours to
		// make: "no" (the default), "yes", or "ask", which says nothing, so App
		// Store Connect asks at each upload.
		NonExemptEncryption string `json:"nonExemptEncryption"`
		// Collected is the data the app collects, for its privacy manifest:
		// what App Store Connect's report compares with its privacy labels.
		Collected []collectedData `json:"collected"`
		// BackgroundModes adds to Info.plist's UIBackgroundModes: "audio",
		// to keep recording and playing with the screen locked.
		BackgroundModes []string `json:"backgroundModes"`
		// StoreKit is a StoreKit configuration file (Xcode's .storekit: the
		// products, their prices, Ask to Buy, renewals sped up) that
		// asuka dev's build for the simulator sells from, as Xcode's scheme
		// does (platform/purchase, which asuka dev builds into the app).
		StoreKit string `json:"storekit"`
		// GoogleServices is the Firebase project's GoogleService-Info.plist,
		// for platform/analytics: the iOS and Mac builds carry it, as
		// Firebase reads it (analytics.go).
		GoogleServices string `json:"googleServices"`
		// IPhoneOnly leaves iPad out: the app runs there as an iPhone app,
		// in a phone-sized window. By default it's an iPad app too, in
		// Split View and Stage Manager, at any size the user gives it.
		IPhoneOnly bool `json:"iphoneOnly"`
		// AppStoreConnect is the App Store Connect API key asuka publish
		// uploads with: its ID, its issuer's (none for an individual key)
		// and its .p8's path (default: AuthKey_<id>.p8 in a private_keys
		// directory, as altool looks). ASUKA_ASC_KEY_ID, ASUKA_ASC_ISSUER_ID
		// and ASUKA_ASC_KEY (the path or the key's text) override them.
		AppStoreConnect struct {
			KeyID    string `json:"keyId"`
			IssuerID string `json:"issuerId"`
			Key      string `json:"key"`
		} `json:"appStoreConnect"`
	} `json:"ios"`
	// Mac is what a Mac build signs the app with (ios.team is its team too).
	Mac struct {
		// Profile is a Developer ID (or development) provisioning profile's
		// path, name or UUID, which push, Sign in with Apple and App Attest
		// need (default: ASUKA_MAC_PROFILE, else, for an app that uses them,
		// the best match installed); AppStoreProfile, a Mac App Store one,
		// for --appstore.
		Profile         string `json:"profile"`
		AppStoreProfile string `json:"appStoreProfile"`
		// Packages are what a build hands the app out in: "zip" and "dmg"
		// (default both; --package overrides it).
		Packages []string `json:"packages"`
	} `json:"mac"`
	// Accent is the app's accent color, "#rrggbb" or "#rrggbbaa": the
	// tint color on iOS; on Android, the seed of the Material 3 color
	// scheme (Android 12 and later; before, Asuka's own uses of the accent
	// take it). Default: the platform's.
	Accent string `json:"accent"`
	// AccentDark is the accent in dark mode, in the same form, on iOS,
	// macOS, Windows, Linux and the web (Android's color scheme makes its
	// dark colors from the accent). Default: Accent in both.
	AccentDark string `json:"accentDark"`
	// Background makes a desktop app one that runs in the background,
	// living in the tray (platform/tray), as app.RunInBackground does: no
	// window at launch, kept running with none, and on macOS no Dock
	// icon from launch (Info.plist's LSUIElement).
	Background bool `json:"background"`
	// KeepRunning keeps a desktop app running when its last window
	// closes: closing the main window hides it, until the app shows it
	// again (its tray icon, the Dock icon clicked on a Mac, the app
	// launched again). A background app keeps running anyway.
	KeepRunning bool `json:"keepRunning"`
	// Scheme is the app's URL scheme: myapp://anything opens it
	// (ui.OnOpenURL gets the URL).
	Scheme string `json:"scheme"`
	// FileTypes are the kinds of files the app opens (filetypes.go):
	// ui.OnOpenFiles gets them.
	FileTypes []fileTypeConfig `json:"fileTypes"`
	// SingleInstance false lets the app run as often as it's started on
	// Windows and Linux, each launch its own; by default (true) a second
	// launch hands its arguments to the running one
	// (app.OnSecondInstance, ui.OnOpenFiles) and quits. macOS's Launch
	// Services runs an app once either way.
	SingleInstance *bool `json:"singleInstance"`
	// Links are the web domains whose https links open the app: iOS's
	// universal links (the domain serves apple-app-site-association) and
	// Android's App Links (assetlinks.json).
	Links []string `json:"links"`
	// ShareTarget offers the app in other apps' share sheets (iOS: a
	// share extension that opens the app; Android: ACTION_SEND and
	// ACTION_SEND_MULTIPLE intent filters); ui.OnShare gets what's
	// shared. true: texts and links; or what it takes,
	// {"types": ["text", "image/*", "video/*"], "max": 10}.
	ShareTarget shareTarget `json:"shareTarget"`
	// NFC is what platform/nfc needs of the platforms (nfc.go): iOS's
	// AIDs and FeliCa system codes, Android's tags that open the app.
	NFC nfcConfig `json:"nfc"`
	// Splash is the launch screen: an image (a square PNG, a logo) in the
	// middle of a color, until the app's first frame (or splash.Hide).
	//
	//	[splash]
	//	image = "logo.png"
	//	size = 120
	//	background = "#ffffff"
	//	darkBackground = "#000000"
	Splash struct {
		Image string `json:"image"`
		// Size is the image's width in points (dp): default 120; at most
		// 160 (Android 12's splash icon's circle holds it).
		Size int `json:"size"`
		// Background and DarkBackground are "#rrggbb" (default: white;
		// the light one).
		Background     string `json:"background"`
		DarkBackground string `json:"darkBackground"`
		// colored is whether asuka.toml sets Background itself, rather
		// than loadConfig: only then is a loading web page its color.
		colored bool
	} `json:"splash"`
	// Widgets are the app's home screen widgets (platform/widget fills them).
	Widgets []widgetConfig `json:"widgets"`
	// TV is what the TVs' builds take (tv.go): Android TV's declaration,
	// the TV launchers' banner.
	TV tvConfig `json:"tv"`
	// tvToldDropped: a tvOS build said what it leaves out (dropTVExtensions).
	tvToldDropped bool
	// I18n says where the app's catalogs are, when they aren't in locales.
	I18n *i18nConfig `json:"i18n"`
	// Updates are a desktop app's (platform/update, updates.go): the public
	// key updates are signed with, and where they're published.
	Updates *updatesConfig `json:"updates"`
	// dir is the app's directory.
	dir string
	// Shortcuts are the app's actions for the system: on iOS, App
	// Shortcuts (Siri, Spotlight, the Shortcuts app, the Action button);
	// on Android, the launcher's shortcuts. Each opens the app with its URL.
	Shortcuts []shortcutConfig `json:"shortcuts"`
	// ApplePay is Apple Pay's (platform/pay): MerchantIDs are the
	// merchant identifiers the app pays with ("merchant.com.example"),
	// its in-app-payments entitlement's (iOS, macOS).
	ApplePay struct {
		MerchantIDs []string `json:"merchantIds"`
	} `json:"applePay"`
	// ICloud is platform/icloud's (icloud.go): Containers are the app's
	// iCloud containers, the first its default (default iCloud.<id>);
	// DriveName is the first one's folder in iCloud Drive (default: the
	// app's name), which DriveHidden leaves out of iCloud Drive.
	ICloud struct {
		Containers  []string `json:"containers"`
		DriveName   string   `json:"driveName"`
		DriveHidden bool     `json:"driveHidden"`
	} `json:"icloud"`
	// icloudKV, icloudDocs: the app uses platform/icloud's
	// key-value store, its documents (useICloud). displayName is the
	// app's name (loadApp), its iCloud Drive folder's.
	icloudKV, icloudDocs bool
	displayName          string
	// shareSheet: the share target shows the app's own sheet rather than
	// opening the app (platform/sharesheet's active, whose manifest derives
	// shareSheet): on iOS the share extension runs the app as its sheet,
	// on Android AsukaShareActivity takes the shares.
	shareSheet bool
	// pushRewrite: a package asks for the ASUKA_NOTIFICATION_SERVICE
	// host flag (platform/push's rewrite): on iOS the app has a
	// Notification Service Extension, which runs it.
	pushRewrite bool
	// locationPrecise: a package derives temporary full accuracy's
	// purposes (platform/location's requestPrecise): Info.plist has them.
	locationPrecise bool
	// nfcOn: the app imports platform/nfc (addNFC): its filters and
	// uses-feature go in the manifest.
	nfcOn bool
	// analyticsOn: the app imports platform/analytics (analyticsExtras):
	// its builds carry the Firebase files and the analytics settings.
	analyticsOn bool
	// liveActivity: the app uses platform/liveactivity
	// (ASUKA_LIVEACTIVITY): on iOS its widget extension has the Live
	// Activities' template, with or without widgets.
	liveActivity bool
	// androidModules are the host's optional Gradle modules the app's
	// packages need (their asuka.toml's android.hostModules).
	androidModules []string
	// androidLaunch are the app's packages' launch classes (their
	// manifests' android.launch), which the host's AsukaLauncher runs as
	// the process starts.
	androidLaunch []string
	// hostFlags are the iOS host's parts the app's packages need (their
	// manifests' ios.hostFlags); derived, what the CLI makes of the
	// app's asuka.toml for them (ios.derived: derivedNeeds).
	hostFlags, derived []string
	// mapsKey: the app imports platform/maps, whose Google Maps reads the
	// key (android.mapsKey) from the manifest.
	mapsKey bool
	// iosTrue and androidApp: the packages' Info.plist keys set to true
	// (their asuka.toml's ios.plist), and application elements of the
	// Android manifest (android.application).
	iosTrue    []string
	androidApp []string
	// entitlements are the iOS entitlements the packages need, with their
	// values for a simulator build (a release's are the profile's, which
	// must have them); plistArrays, Info.plist's arrays they add to.
	entitlements map[string]any
	plistArrays  map[string][]string
	androidTop   []string
	// flatpakArgs are the Flatpak's finish-args the packages need (the
	// system bus names they talk to).
	flatpakArgs []string
	// flatpakModules are the modules the Flatpak builds before the app
	// (the packages' manifests' linux.flatpakModules), each in YAML.
	flatpakModules []string
	// debDepends, debRecommends, rpmRequires and rpmRecommends are the
	// system packages the .deb and the .rpm depend on for the app's
	// packages (their manifests' linux.deb, linux.debRecommends,
	// linux.rpm and linux.rpmRecommends).
	debDepends, debRecommends, rpmRequires, rpmRecommends []string
	// What the app's native packages' Swift needs (their Info.plist and
	// entitlements, nativeNeeds): Info.plist's keys, and what each of
	// their entitlements is for.
	nativePlist     map[string]any
	entitlementsFor map[string]string
	// nativeModules are the app's native packages' Gradle modules: each
	// one's name and directory.
	nativeModules [][2]string
	// aars are the AARs the build depends on instead of the host's
	// modules and the std and sdk packages' (androidPrebuilt); nil: their
	// modules are built with the app.
	aars *androidAARs
	// Linux is what a Linux build says of the app, in its desktop file
	// and its AppStream metainfo (what software centers and Flathub show).
	Linux struct {
		// Summary is a line saying what the app does; Description, a few
		// paragraphs (separated by blank lines).
		Summary     string `json:"summary"`
		Description string `json:"description"`
		// Categories are the desktop file's (freedesktop.org's main
		// categories: Utility, Office, Development, Network...; default
		// Utility), and Keywords more words a launcher finds it by.
		Categories []string `json:"categories"`
		Keywords   []string `json:"keywords"`
		// License is the app's, as an SPDX expression (MIT, GPL-3.0-or-later,
		// LicenseRef-proprietary); Developer, its developer's name;
		// Homepage, its web page.
		License   string `json:"license"`
		Developer string `json:"developer"`
		Homepage  string `json:"homepage"`
		// FlatpakRuntime is the GNOME runtime's version the Flatpak manifest
		// asks for (default: a recent one; 46 or later).
		FlatpakRuntime string `json:"flatpakRuntime"`
		// Packages are what a build hands the app out in: "tar.gz",
		// "deb", "rpm", "appimage" and "flatpak" (the manifest); default
		// all of them (--package overrides it).
		Packages []string `json:"packages"`
		// Maintainer is who the .deb and .rpm say maintains them, "Name
		// <email>" (default: Developer).
		Maintainer string `json:"maintainer"`
		// DebDepends and RPMRequires are more packages the .deb and the
		// .rpm depend on, in each one's syntax ("libfoo1 (>= 1.2)", "a |
		// b"; "foo >= 1.2", "(a or b)"), beyond GTK's, libadwaita's and
		// what the app's packages need.
		DebDepends  []string `json:"debDepends"`
		RPMRequires []string `json:"rpmRequires"`
	} `json:"linux"`
	// Windows is what a Windows build's installer and MSIX say of the app.
	Windows struct {
		// Packages are what a build hands the app out in: "zip",
		// "installer" (a per-user setup .exe) and "msix" (for the
		// Microsoft Store and sideloading); default zip and installer
		// (--package overrides it).
		Packages []string `json:"packages"`
		// Publisher is who makes the app, as Installed apps and the
		// Store show it (default: the app's name); Homepage, its web page.
		Publisher string `json:"publisher"`
		Homepage  string `json:"homepage"`
		// DesktopShortcut: the installer puts a shortcut on the desktop
		// too, beside the Start menu's (default false).
		DesktopShortcut bool `json:"desktopShortcut"`
		// IdentityName and PublisherID are the MSIX package's identity:
		// its name (default: the app's ID) and its publisher, the subject
		// of the certificate it's signed with ("CN=Example Inc., O=Example
		// Inc., C=US"; default "CN=" and Publisher). For the Store, Partner
		// Center's Package/Identity/Name and Package/Identity/Publisher.
		IdentityName string `json:"identityName"`
		PublisherID  string `json:"publisherId"`
	} `json:"windows"`
	// Bluetooth is what an app of platform/bluetooth asks of the
	// platforms beyond what importing the package gives (docs/bluetooth.md).
	Bluetooth struct {
		// Background are the roles the app keeps in the background on
		// iOS: "central" (the bluetooth-central mode, with state
		// restoration) and "peripheral" (bluetooth-peripheral), and with
		// them platform/bluetooth's launch class. Android's background scan
		// is the package's, for an app that calls backgroundScan.
		Background []string `json:"background"`
		// Location says the app finds where the user is from its scans
		// (beacons): Android 12's BLUETOOTH_SCAN without neverForLocation,
		// and the location permission that then needs. Without it, scans
		// say they don't, and Android leaves some beacons' advertisements
		// out of them.
		Location bool `json:"location"`
	} `json:"bluetooth"`
	// Health is what an app of platform/health asks of Android's Health
	// Connect: the kinds it reads and writes, its privacy policy (health.go).
	Health healthConfig `json:"health"`
	// Car is what an app of platform/car is on a car's screen: its
	// category, which CarPlay and Android Auto show it by (car.go).
	Car carConfig `json:"car"`
	// carScene: the app has CarPlay's scene (carExtras); carAndroid, its
	// Android Auto category ("media": a media browser), "" none.
	carScene   bool
	carAndroid string
	// Ads is what an app of platform/ads gives Google Mobile Ads: its
	// AdMob app IDs, more ad networks' SKAdNetwork identifiers (ads.go).
	Ads adsConfig `json:"ads"`
	// Analytics is what an app of platform/analytics says of Google
	// Analytics beyond its Firebase files: the web stream the web and
	// the desktops send to, and what collection starts as (analytics.go).
	Analytics analyticsConfig `json:"analytics"`
	// Web is what a web build needs (cmd/asuka/web.go).
	Web struct {
		// VapidPublicKey is the app server's VAPID public key (base64url, an
		// uncompressed P-256 point), for platform/push's Web Push: the browser
		// subscribes with it, and the server signs with its private key
		// (asuka/webpush: GenerateKeys makes a pair).
		VapidPublicKey string `json:"vapidPublicKey"`
		// Base is the path the site is served at ("/app/" for
		// https://example.com/app/; default "/"): the page's <base href>,
		// so that its files are found from any route's path, and under
		// which a Router's hrefs are the page's addresses.
		Base string `json:"base"`
	} `json:"web"`
	Android struct {
		// DynamicColor takes the colors from the user's wallpaper where
		// the system has them (Android 12 and later), as Material apps do;
		// the accent is the seed elsewhere.
		DynamicColor bool     `json:"dynamicColor"`
		Permissions  []string `json:"permissions"`
		// The upload key; its passwords come from ASUKA_KEYSTORE_PASSWORD
		// and ASUKA_KEY_PASSWORD (default: the keystore's).
		Keystore string `json:"keystore"`
		KeyAlias string `json:"keyAlias"`
		// GoogleServices is the Firebase project's google-services.json,
		// for platform/push (Firebase Cloud Messaging) and platform/analytics.
		GoogleServices string `json:"googleServices"`
		// MapsKey is a Google Maps Platform API key (Maps SDK for Android),
		// for platform/maps.
		MapsKey string `json:"mapsKey"`
		// ServiceAccount is the JSON key of the Google Cloud service
		// account asuka publish uploads with (ASUKA_PLAY_SERVICE_ACCOUNT, the
		// path or the JSON, overrides it).
		ServiceAccount string `json:"serviceAccount"`
	} `json:"android"`
}

var (
	idPart   = regexp.MustCompile(`[^A-Za-z0-9]+`)
	hexColor = regexp.MustCompile(`^#?([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$`)
	scheme   = regexp.MustCompile(`^[a-zA-Z][a-zA-Z0-9+.-]*$`)
	domain   = regexp.MustCompile(`^[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?)+$`)
)

// configFile is an app's manifest, in its directory.
const configFile = "asuka.toml"

// loadConfig reads dir/asuka.toml, filling in the defaults.
func loadConfig(dir string) (*appConfig, error) {
	c := &appConfig{}
	data, err := os.ReadFile(filepath.Join(dir, configFile))
	if err == nil {
		if err := toml.Unmarshal(data, c); err != nil {
			var te *toml.Error
			if errors.As(err, &te) {
				return nil, fmt.Errorf("%s:%v", configFile, err) // asuka.toml:3:9: what
			}
			return nil, fmt.Errorf("%s: %v", configFile, err)
		}
	} else if !os.IsNotExist(err) {
		return nil, err
	}
	c.useI18nDefaults(dir)
	base := filepath.Base(dir)
	if c.Name == "" {
		c.Name = base
	}
	if c.ID == "" {
		c.ID = "dev.asuka." + strings.Trim(strings.ToLower(idPart.ReplaceAllString(base, "-")), "-")
	}
	if c.Version == "" {
		c.Version = "1.0"
	}
	if c.Build <= 0 {
		c.Build = 1
	}
	if len(c.Orientations) == 0 {
		c.Orientations = []string{"portrait", "landscape"}
	}
	for _, o := range c.Orientations {
		if o != "portrait" && o != "landscape" {
			return nil, fmt.Errorf("asuka.toml: orientation %q (want portrait or landscape)", o)
		}
	}
	if c.Accent != "" {
		if !hexColor.MatchString(c.Accent) {
			return nil, fmt.Errorf("asuka.toml: accent %q (want #rrggbb or #rrggbbaa)", c.Accent)
		}
	}
	if c.AccentDark != "" && !hexColor.MatchString(c.AccentDark) {
		return nil, fmt.Errorf("asuka.toml: accentDark %q (want #rrggbb or #rrggbbaa)", c.AccentDark)
	}
	if c.Scheme != "" && !scheme.MatchString(c.Scheme) {
		return nil, fmt.Errorf("asuka.toml: scheme %q (want letters, digits, + - .; a letter first)", c.Scheme)
	}
	for _, l := range c.Links {
		if !domain.MatchString(l) {
			return nil, fmt.Errorf("asuka.toml: link domain %q (want a host name, as example.com)", l)
		}
	}
	for _, r := range c.Bluetooth.Background {
		if r != "central" && r != "peripheral" {
			return nil, fmt.Errorf("asuka.toml: bluetooth background %q (want central or peripheral)", r)
		}
	}
	if err := c.C.Check(); err != nil {
		return nil, fmt.Errorf("asuka.toml: %v", err)
	}
	if err := c.checkFileTypes(dir); err != nil {
		return nil, fmt.Errorf("asuka.toml: %v", err)
	}
	if err := c.checkWidgets(); err != nil {
		return nil, fmt.Errorf("asuka.toml: %v", err)
	}
	if err := c.ShareTarget.check(); err != nil {
		return nil, fmt.Errorf("asuka.toml: shareTarget: %v", err)
	}
	if err := c.checkTV(dir); err != nil {
		return nil, err
	}
	if err := c.NFC.check(); err != nil {
		return nil, fmt.Errorf("asuka.toml: nfc: %v", err)
	}
	if err := c.Health.check(); err != nil {
		return nil, fmt.Errorf("asuka.toml: health: %v", err)
	}
	if err := c.Car.check(); err != nil {
		return nil, fmt.Errorf("asuka.toml: car: %v", err)
	}
	if err := c.Ads.check(); err != nil {
		return nil, fmt.Errorf("asuka.toml: ads: %v", err)
	}
	if err := c.Analytics.check(); err != nil {
		return nil, fmt.Errorf("asuka.toml: analytics: %v", err)
	}
	if _, err := c.updates(); err != nil {
		return nil, err
	}
	if c.Icon != "" && !filepath.IsAbs(c.Icon) {
		c.Icon = filepath.Join(dir, c.Icon)
	}
	if len(c.AlternateIcons) > 0 && c.Icon == "" {
		return nil, errors.New("asuka.toml: alternateIcons: the app needs its own icon too (icon)")
	}
	for name, p := range c.AlternateIcons {
		if !alternateIconName.MatchString(name) {
			return nil, fmt.Errorf("asuka.toml: alternateIcons: %q isn't a name an icon can have: letters and digits, starting with a letter", name)
		}
		if !filepath.IsAbs(p) {
			c.AlternateIcons[name] = filepath.Join(dir, p)
		}
	}
	if c.Android.Keystore != "" && !filepath.IsAbs(c.Android.Keystore) {
		c.Android.Keystore = filepath.Join(dir, c.Android.Keystore)
	}
	if k := &c.IOS.AppStoreConnect.Key; *k != "" && !filepath.IsAbs(*k) && !strings.Contains(*k, "-----BEGIN") {
		*k = filepath.Join(dir, *k)
	}
	if c.Android.ServiceAccount != "" && !filepath.IsAbs(c.Android.ServiceAccount) {
		c.Android.ServiceAccount = filepath.Join(dir, c.Android.ServiceAccount)
	}
	if c.Android.GoogleServices != "" && !filepath.IsAbs(c.Android.GoogleServices) {
		c.Android.GoogleServices = filepath.Join(dir, c.Android.GoogleServices)
	}
	if c.IOS.GoogleServices != "" && !filepath.IsAbs(c.IOS.GoogleServices) {
		c.IOS.GoogleServices = filepath.Join(dir, c.IOS.GoogleServices)
	}
	if sp := &c.Splash; sp.Image != "" || sp.Background != "" {
		if sp.Image != "" && !filepath.IsAbs(sp.Image) {
			sp.Image = filepath.Join(dir, sp.Image)
		}
		if sp.Size <= 0 {
			sp.Size = 120
		}
		sp.Size = min(sp.Size, 160)
		sp.colored = sp.Background != ""
		if sp.Background == "" {
			sp.Background = "#ffffff"
		}
		if sp.DarkBackground == "" {
			sp.DarkBackground = sp.Background
		}
		for _, b := range []string{sp.Background, sp.DarkBackground} {
			if !hexColor.MatchString(b) || len(strings.TrimPrefix(b, "#")) != 6 {
				return nil, fmt.Errorf("asuka.toml: splash background %q (want #rrggbb)", b)
			}
		}
	}
	return c, nil
}

// urlTypes is Info.plist's CFBundleURLTypes for the scheme, and on iOS
// the share extension's (shareScheme).
func (c *appConfig) urlTypes(ios bool) []any {
	if c == nil {
		return nil
	}
	var schemes []string
	if c.Scheme != "" {
		schemes = append(schemes, c.Scheme)
	}
	if ios && c.ShareTarget.on {
		schemes = append(schemes, c.shareScheme())
	}
	if len(schemes) == 0 {
		return nil
	}
	return []any{map[string]any{"CFBundleURLName": c.ID, "CFBundleURLSchemes": schemes}}
}

// associatedDomains is the entitlement for the links (simulator: in the
// developer mode, which needs no CDN's copy of the association file).
func (c *appConfig) associatedDomains(simulator bool) []string {
	var out []string
	for _, l := range c.Links {
		d := "applinks:" + l
		if simulator {
			d += "?mode=developer"
		}
		out = append(out, d)
	}
	return out
}

// androidManifest is the app's manifest, merged over the host's: its
// permissions, and the intent filters for its scheme and links. A debug
// build's also lets asuka dev serve over plain HTTP.
// collectedData is a kind of data in a privacy manifest.
type collectedData struct {
	// Type is Apple's name without its prefix: "EmailAddress" is
	// NSPrivacyCollectedDataTypeEmailAddress.
	Type string `json:"type"`
	// Linked: tied to the user's identity. Tracking: used to track them.
	Linked   bool `json:"linked"`
	Tracking bool `json:"tracking"`
	// Purposes are Apple's, without their prefix: "AppFunctionality".
	Purposes []string `json:"purposes"`
}

// payPkg is platform/pay, whose Apple Pay merchant IDs are the app's
// asuka.toml's.
const payPkg = "platform/pay"

// payEntitlement gives an app that imports platform/pay (deps; nil:
// any app) Apple Pay's entitlement, with asuka.toml's merchant IDs.
func (c *appConfig) payEntitlement(deps []string) {
	if len(c.ApplePay.MerchantIDs) == 0 || deps != nil && !slices.Contains(deps, payPkg) {
		return
	}
	if c.entitlements == nil {
		c.entitlements = map[string]any{}
	}
	c.entitlements["com.apple.developer.in-app-payments"] = c.ApplePay.MerchantIDs
}

// bluetoothPackage is platform/bluetooth, whose needs asuka.toml's
// "bluetooth" adds to (bluetoothExtras).
const bluetoothPackage = "platform/bluetooth"

// permissionsFor adds what the app's own asuka.toml asks of the
// packages deps (the program's imports, by import path) beyond their
// manifests (depManifests): Apple Pay's merchant IDs, the nfc,
// bluetooth, health, ads and car tables. nil deps: every package's (the player runs any app).
// Run after depManifests, whose permissions these change.
func (c *appConfig) permissionsFor(deps []string) {
	c.payEntitlement(deps)
	c.addNFC(deps)
	c.healthExtras(deps)
	c.adsExtras(deps)
	c.analyticsExtras(deps)
	c.carExtras(deps)
	if deps == nil || slices.Contains(deps, bluetoothPackage) {
		c.bluetoothExtras()
	}
}

// addDerived adds the needs the packages' manifests ask the CLI to make
// of the app's own asuka.toml (ios.derived: iCloud's, temporary full
// accuracy's, the share sheet's), and notes the host's parts that have
// more of the app (a Notification Service Extension for push's
// rewrites, the widget extension's Live Activities).
func (c *appConfig) addDerived() {
	c.addICloud(c.derives("icloudKeyValue"), c.derives("icloudDocuments"))
	c.locationPrecise = c.derives("locationPrecise")
	c.shareSheet = c.derives("shareSheet")
	c.pushRewrite = slices.Contains(c.hostFlags, "ASUKA_NOTIFICATION_SERVICE")
	c.liveActivity = slices.Contains(c.hostFlags, "ASUKA_LIVEACTIVITY")
}

// addPermission adds an Android permission ("name", "name:28" up to an
// API level, "name|flag" with usesPermissionFlags), once per name: a
// permission without a maximum wins over one with, as one without flags
// does over one with (the flags narrow what it's for).
func (c *appConfig) addPermission(p string) {
	parse := func(p string) (name string, max int, flags string) {
		p, flags, _ = strings.Cut(p, "|")
		name, m, _ := strings.Cut(p, ":")
		max, _ = strconv.Atoi(m) // 0: every level
		return
	}
	name, max, flags := parse(p)
	for i, q := range c.Android.Permissions {
		qn, qmax, qflags := parse(q)
		if qn != name {
			continue
		}
		if max != 0 && (qmax == 0 || qmax > max) {
			max = qmax
		}
		if qflags == "" {
			flags = ""
		}
		merged := name
		if max != 0 {
			merged += ":" + strconv.Itoa(max)
		}
		if flags != "" {
			merged += "|" + flags
		}
		c.Android.Permissions[i] = merged
		return
	}
	c.Android.Permissions = append(c.Android.Permissions, p)
}

// bluetoothExtras are what asuka.toml's bluetooth asks for, for an app
// of platform/bluetooth: the background modes, the location from scans.
func (c *appConfig) bluetoothExtras() {
	if c.Bluetooth.Location {
		// Scans that find where the user is: no neverForLocation, and the
		// location permission, which Android then wants granted to scan
		// (Request asks for it).
		for i, p := range c.Android.Permissions {
			if name, _, _ := strings.Cut(p, "|"); name == "android.permission.BLUETOOTH_SCAN" {
				c.Android.Permissions[i] = name
			}
		}
		c.addPermission("android.permission.ACCESS_FINE_LOCATION")
		c.addPermission("android.permission.ACCESS_COARSE_LOCATION")
		if c.IOS.Usage == nil {
			c.IOS.Usage = map[string]string{}
		}
		if c.IOS.Usage["NSLocationWhenInUseUsageDescription"] == "" {
			c.IOS.Usage["NSLocationWhenInUseUsageDescription"] = "To find the Bluetooth beacons near you."
		}
	}
	addMode := func(m string) {
		if c.plistArrays == nil {
			c.plistArrays = map[string][]string{}
		}
		if !slices.Contains(c.plistArrays["UIBackgroundModes"], m) {
			c.plistArrays["UIBackgroundModes"] = append(c.plistArrays["UIBackgroundModes"], m)
		}
	}
	for _, r := range c.Bluetooth.Background {
		switch r {
		case "central":
			addMode("bluetooth-central")
		case "peripheral":
			addMode("bluetooth-peripheral")
		}
	}
	// With a mode (these, the app's ios.backgroundModes', a package's), the
	// managers have their restore identifiers: iOS starts the app again
	// for what it kept, and the package's launch class makes them before
	// launch ends (sdk/bluetooth's swift/Bluetooth.swift).
	modes := append(slices.Clone(c.IOS.BackgroundModes), c.plistArrays["UIBackgroundModes"]...)
	if slices.Contains(modes, "bluetooth-central") || slices.Contains(modes, "bluetooth-peripheral") {
		if c.plistArrays == nil {
			c.plistArrays = map[string][]string{}
		}
		if !slices.Contains(c.plistArrays["AsukaLaunch"], bluetoothLaunch) {
			c.plistArrays["AsukaLaunch"] = append(c.plistArrays["AsukaLaunch"], bluetoothLaunch)
		}
	}
}

// bluetoothLaunch is platform/bluetooth's launch class on iOS (its manifest's
// launch, for an app that calls backgroundScan).
const bluetoothLaunch = "AsukaBluetoothLaunch"

// androidFeatures adds the host's optional modules the app's elements
// need (asuka-video, for a Video, as iOS's ASUKA_VIDEO), or (player:
// asuka dev's, which runs whatever the next save shows) all of them.
func (c *appConfig) androidFeatures(app *asukaApp, player bool) {
	if (player || app.refs("", "Video")) && !slices.Contains(c.androidModules, "video") {
		c.androidModules = append(c.androidModules, "video")
	}
}

// gradleProps are the Gradle properties naming the app's optional
// modules, and its native packages' modules (name=directory, ;-separated).
func (c *appConfig) gradleProps() []string {
	props := []string{"-PasukaModules=" + strings.Join(c.androidModules, ",")}
	var ms []string
	for _, m := range c.nativeModules {
		if c.aars == nil || !c.aars.pkgs[m[0]] {
			ms = append(ms, m[0]+"="+m[1])
		}
	}
	if len(ms) > 0 {
		props = append(props, "-PasukaNativeModules="+strings.Join(ms, ";"))
	}
	if c.aars != nil {
		// The host's and the packages' AARs, from a local repository.
		props = append(props, "-PasukaPrebuiltRepo="+c.aars.repo, "-PasukaPrebuiltHost="+c.aars.host,
			"-PasukaPrebuiltDeps="+strings.Join(c.aars.deps, ","))
	}
	return props
}

func (c *appConfig) androidManifest(debug bool) string {
	var b strings.Builder
	// With alternate icons the launcher's entries are activity aliases,
	// one an icon (androidIconAliases): the activity's own leaves.
	aliases := len(c.AlternateIcons) > 0
	if aliases {
		b.WriteString("<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" xmlns:tools=\"http://schemas.android.com/tools\">\n")
	} else {
		b.WriteString("<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\">\n")
	}
	for _, p := range c.Android.Permissions {
		p, flags, _ := strings.Cut(p, "|")
		name, max, _ := strings.Cut(p, ":")
		if max != "" {
			max = fmt.Sprintf(` android:maxSdkVersion="%s"`, html.EscapeString(max))
		}
		if flags != "" {
			max += fmt.Sprintf(` android:usesPermissionFlags="%s"`, html.EscapeString(flags))
		}
		fmt.Fprintf(&b, "    <uses-permission android:name=\"%s\"%s />\n", html.EscapeString(name), max)
	}
	for _, x := range c.androidTop {
		b.WriteString("    " + x + "\n")
	}
	b.WriteString(c.androidTVFeatures())
	b.WriteString("    <application" + c.androidTVBanner())
	if c.androidHasLocales() {
		b.WriteString(" android:localeConfig=\"@xml/asuka_locales_config\"")
	}
	// The host's network security: the system's authorities and Let's
	// Encrypt's roots, which Android 7.0 lacks; asuka dev's player also
	// loads the app over cleartext HTTP.
	if debug {
		b.WriteString(" android:usesCleartextTraffic=\"true\" android:networkSecurityConfig=\"@xml/asuka_network_security_debug\"")
	} else {
		b.WriteString(" android:networkSecurityConfig=\"@xml/asuka_network_security\"")
	}
	b.WriteString(">\n")
	// The Maps SDK's key (platform/maps, platform/maps), which it reads from the manifest.
	if c.Android.MapsKey != "" && c.mapsKey {
		fmt.Fprintf(&b, "        <meta-data android:name=\"com.google.android.geo.API_KEY\" android:value=\"%s\" />\n", html.EscapeString(c.Android.MapsKey))
	}
	// The splash's theme (androidRes), which AsukaActivity leaves as it starts.
	theme := ""
	if c.splashOn() {
		theme = ` android:theme="@style/Theme.Asuka.Splash"`
	}
	fmt.Fprintf(&b, "        <activity android:name=\"dev.asuka.AsukaActivity\" android:exported=\"true\"%s>\n", theme)
	// asuka.toml's shortcuts: the launcher's (androidShortcutRes), on
	// the aliases when they're the launcher's entries.
	if len(c.Shortcuts) > 0 && !aliases {
		b.WriteString("            <meta-data android:name=\"android.app.shortcuts\" android:resource=\"@xml/asuka_shortcuts\" />\n")
	}
	if aliases {
		b.WriteString(`            <intent-filter tools:node="remove">
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
`)
	} else if c.TV.Android {
		// The TV launcher's entry (Android TV, Google TV).
		b.WriteString(androidTVLauncher)
	}
	filter := func(verify bool, data string) {
		v := ""
		if verify {
			v = " android:autoVerify=\"true\""
		}
		fmt.Fprintf(&b, `            <intent-filter%s>
                <action android:name="android.intent.action.VIEW" />
                <category android:name="android.intent.category.DEFAULT" />
                <category android:name="android.intent.category.BROWSABLE" />
                %s
            </intent-filter>
`, v, data)
	}
	if c.Scheme != "" {
		filter(false, fmt.Sprintf(`<data android:scheme="%s" />`, html.EscapeString(c.Scheme)))
	}
	for _, l := range c.Links {
		filter(true, fmt.Sprintf(`<data android:scheme="https" android:host="%s" />`, html.EscapeString(l)))
	}
	if c.ShareTarget.on && !c.shareSheet {
		b.WriteString(c.ShareTarget.intentFilters())
	}
	// asuka.toml's fileTypes: opened from a file manager, or a mail's
	// attachment (AsukaActivity copies a content:// URI's file).
	b.WriteString(fileTypeFilters(c.fileTypes()))
	// platform/nfc's tags that open the app (asuka.toml's nfc).
	b.WriteString(c.nfcFilters())
	b.WriteString("        </activity>\n")
	if aliases {
		b.WriteString(c.androidIconAliases())
	}
	// A share sheet (platform/sharesheet): the shares go to its Activity.
	if c.ShareTarget.on && c.shareSheet {
		b.WriteString("        <activity android:name=\"dev.asuka.AsukaShareActivity\" android:exported=\"true\">\n")
		b.WriteString(c.ShareTarget.intentFilters())
		b.WriteString("        </activity>\n")
	}
	for _, x := range c.androidApp {
		b.WriteString("        " + x + "\n")
	}
	// The packages' launch code: a provider, made as the process starts.
	if len(c.androidLaunch) > 0 {
		b.WriteString("        <provider android:name=\"dev.asuka.AsukaLauncher\" android:authorities=\"${applicationId}.asuka.launch\" android:exported=\"false\" />\n")
		fmt.Fprintf(&b, "        <meta-data android:name=\"dev.asuka.launch\" android:value=\"%s\" />\n", html.EscapeString(strings.Join(c.androidLaunch, ",")))
	}
	b.WriteString(c.androidWidgetReceivers())
	b.WriteString("    </application>\n</manifest>\n")
	return b.String()
}

// iosDevices are Info.plist's UIDeviceFamily: iPhone, and iPad unless
// the app is iPhone only.
func (c *appConfig) iosDevices() []int {
	if c != nil && c.IOS.IPhoneOnly {
		return []int{1}
	}
	return []int{1, 2}
}

// ipadOrientations are Info.plist's UISupportedInterfaceOrientations~ipad:
// all four, whatever the app's orientations, since multitasking (Split
// View, Slide Over, Stage Manager) needs them; the app's window is any
// shape there anyway. nil for an iPhone-only app.
func (c *appConfig) ipadOrientations() []string {
	if c != nil && c.IOS.IPhoneOnly {
		return nil
	}
	return []string{"UIInterfaceOrientationPortrait", "UIInterfaceOrientationPortraitUpsideDown",
		"UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight"}
}

func (c *appConfig) allows(o string) bool {
	for _, x := range c.Orientations {
		if x == o {
			return true
		}
	}
	return false
}

// alternateIconName is what an alternate icon may be called: it names an
// Android activity alias, an iOS icon set and a file on the desktops.
var alternateIconName = regexp.MustCompile(`^[A-Za-z][A-Za-z0-9]*$`)

// alternateIconNames are asuka.toml's alternateIcons' names, sorted.
func (c *appConfig) alternateIconNames() []string {
	names := make([]string, 0, len(c.AlternateIcons))
	for n := range c.AlternateIcons {
		names = append(names, n)
	}
	slices.Sort(names)
	return names
}

// androidIconAliases are the launcher's entries of an app with alternate
// icons: an activity alias of AsukaActivity for each icon, the app's
// (dev.asuka.AppIcon) enabled, each other one's (dev.asuka.AppIcon.<name>)
// not. platform/appicon enables the one the app shows; the launcher shows
// the enabled one's icon.
func (c *appConfig) androidIconAliases() string {
	var b strings.Builder
	for _, name := range append([]string{""}, c.alternateIconNames()...) {
		comp, icon, enabled := "dev.asuka.AppIcon", "@mipmap/ic_launcher", true
		if name != "" {
			comp, icon, enabled = comp+"."+name, "@mipmap/"+androidIconRes(name), false
		}
		fmt.Fprintf(&b, "        <activity-alias android:name=\"%s\" android:targetActivity=\"dev.asuka.AsukaActivity\" android:exported=\"true\" android:enabled=\"%t\" android:icon=\"%s\">\n", comp, enabled, icon)
		b.WriteString(`            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
`)
		if c.TV.Android {
			b.WriteString(androidTVLauncher)
		}
		if len(c.Shortcuts) > 0 {
			b.WriteString("            <meta-data android:name=\"android.app.shortcuts\" android:resource=\"@xml/asuka_shortcuts\" />\n")
		}
		b.WriteString("        </activity-alias>\n")
	}
	return b.String()
}
