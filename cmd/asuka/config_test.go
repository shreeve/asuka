package main

import (
	"fmt"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"testing"

	"github.com/egoist/asuka/internal/appbuild"
)

func TestAccent(t *testing.T) {
	dir := t.TempDir()
	write := func(s string) {
		if err := os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(s), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	write("accent = \"#7c3aed\"\n\n[android]\ndynamicColor = true")
	c, err := loadConfig(dir)
	if err != nil {
		t.Fatal(err)
	}
	if c.Accent != "#7c3aed" || !c.Android.DynamicColor {
		t.Fatalf("accent %q, dynamic color %v", c.Accent, c.Android.DynamicColor)
	}
	// A background app says so to the desktop hosts.
	write(`background = true`)
	if c, err := loadConfig(dir); err != nil || !(&asukaApp{cfg: c}).hostConfig(platformMac, "", false).Background {
		t.Fatalf("background: %v, %v", c, err)
	}
	write(`accent = "purple"`)
	if _, err := loadConfig(dir); err == nil {
		t.Fatal("a bad accent was taken")
	}
}

// accentDark is the accent in dark mode, taken in the accent's form and
// written to the desktop hosts' settings beside it; without one, the
// settings carry none and the accent serves both modes.
func TestAccentDark(t *testing.T) {
	dir := t.TempDir()
	write := func(s string) {
		if err := os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(s), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	write("accent = \"#2563eb\"\naccentDark = \"#93c5fd\"")
	c, err := loadConfig(dir)
	if err != nil {
		t.Fatal(err)
	}
	if c.AccentDark != "#93c5fd" {
		t.Fatalf("accentDark %q", c.AccentDark)
	}
	if h := (&asukaApp{cfg: c}).hostConfig(platformMac, "", false); h.Accent != "#2563eb" || h.AccentDark != "#93c5fd" {
		t.Fatalf("host settings: accent %q, accentDark %q", h.Accent, h.AccentDark)
	}
	write(`accent = "#2563eb"`)
	if c, err := loadConfig(dir); err != nil || (&asukaApp{cfg: c}).hostConfig(platformMac, "", false).AccentDark != "" {
		t.Fatalf("no accentDark: %v, %v", c, err)
	}
}

func TestLinks(t *testing.T) {
	dir := t.TempDir()
	os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(`id = "com.example.notes"
scheme = "notes"
links = ["notes.example.com"]

[android]
permissions = ["android.permission.CAMERA"]`), 0o644)
	c, err := loadConfig(dir)
	if err != nil {
		t.Fatal(err)
	}
	m := c.androidManifest(true)
	for _, want := range []string{`android.permission.CAMERA`, `android:scheme="notes"`, `android:autoVerify="true"`,
		`android:host="notes.example.com"`, `usesCleartextTraffic`} {
		if !strings.Contains(m, want) {
			t.Errorf("manifest lacks %s:\n%s", want, m)
		}
	}
	if strings.Contains(c.androidManifest(false), "Cleartext") {
		t.Error("a release manifest allows cleartext")
	}
	if d := c.associatedDomains(true); len(d) != 1 || d[0] != "applinks:notes.example.com?mode=developer" {
		t.Errorf("domains %v", d)
	}
	os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(`scheme = "my app"`), 0o644)
	if _, err := loadConfig(dir); err == nil {
		t.Error("a bad scheme was taken")
	}
}

// shareTarget: on iOS, the share extension's scheme beside the app's
// own, and the extension's Info.plist, which takes the app's name,
// version and build keys; on Android, the ACTION_SEND filter.
func TestShareTarget(t *testing.T) {
	c := &appConfig{ID: "com.example.dl", Scheme: "dl", ShareTarget: textShares}
	got := plist(map[string]any{"CFBundleURLTypes": c.urlTypes(true)})
	if !strings.Contains(got, "<string>dl</string>\n\t\t\t\t<string>asukashare.com.example.dl</string>") {
		t.Errorf("iOS URL types:\n%s", got)
	}
	if t2 := c.urlTypes(false); strings.Contains(plist(map[string]any{"T": t2}), "asukashare") {
		t.Error("macOS has the share scheme")
	}
	if (&appConfig{ID: "x"}).urlTypes(true) != nil {
		t.Error("URL types with no scheme")
	}
	info := c.shareInfo(map[string]any{"CFBundleName": "DL", "CFBundleVersion": "7", "DTSDKName": "iphoneos27.1",
		"CFBundleIdentifier": "com.example.dl", "UILaunchScreen": map[string]any{}})
	if info["CFBundleIdentifier"] != "com.example.dl.share" || info["CFBundleName"] != "DL" || info["CFBundleVersion"] != "7" ||
		info["DTSDKName"] != "iphoneos27.1" || info["AsukaShareScheme"] != "asukashare.com.example.dl" || info["UILaunchScreen"] != nil ||
		info["CFBundlePackageType"] != "XPC!" {
		t.Errorf("extension's Info.plist: %v", info)
	}
	if m := c.androidManifest(false); !strings.Contains(m, "android.intent.action.SEND\"") || !strings.Contains(m, `android:mimeType="text/plain"`) ||
		strings.Contains(m, "SEND_MULTIPLE") {
		t.Errorf("Android's filters:\n%s", m)
	}
	rule := info["NSExtension"].(map[string]any)["NSExtensionAttributes"].(map[string]any)["NSExtensionActivationRule"].(map[string]any)
	if len(rule) != 2 || rule["NSExtensionActivationSupportsText"] != true || rule["NSExtensionActivationSupportsWebURLWithMaxCount"] != 1 {
		t.Errorf("activation rule %v", rule)
	}
	if c.appGroup() != "" || info["AsukaAppGroup"] != nil {
		t.Error("an App Group for texts")
	}
}

// shareTarget is true, false or what the app takes: texts, files by
// MIME type, and how many at once.
func TestShareTargetConfig(t *testing.T) {
	dir := t.TempDir()
	load := func(js string) (*appConfig, error) {
		os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte("id = \"com.example.pics\"\nshareTarget = "+js), 0o644)
		return loadConfig(dir)
	}
	for js, want := range map[string]shareTarget{
		"false": {},
		"true":  textShares,
		`{types = ["Image/*", "video/*"], max = 5}`: {on: true, Types: []string{"image/*", "video/*"}, Max: 5},
		`{}`: textShares,
	} {
		c, err := load(js)
		if err != nil || c.ShareTarget.on != want.on || !slices.Equal(c.ShareTarget.Types, want.Types) || c.ShareTarget.Max != want.Max {
			t.Errorf("%s: %+v, %v", js, c.ShareTarget, err)
		}
	}
	for _, js := range []string{`{types = ["pictures"]}`, `{max = -1}`, `"yes"`} {
		if _, err := load(js); err == nil {
			t.Errorf("%s was taken", js)
		}
	}

	// Images and videos, five at once: Apple's keys, an App Group, and
	// both of Android's actions.
	c, _ := load(`{types = ["text", "image/*", "video/*"], max = 5}`)
	rule := c.shareInfo(nil)["NSExtension"].(map[string]any)["NSExtensionAttributes"].(map[string]any)["NSExtensionActivationRule"].(map[string]any)
	if rule["NSExtensionActivationSupportsImageWithMaxCount"] != 5 || rule["NSExtensionActivationSupportsMovieWithMaxCount"] != 5 ||
		rule["NSExtensionActivationSupportsText"] != true {
		t.Errorf("rule %v", rule)
	}
	if g := c.appGroup(); g != "group.com.example.pics" || c.shareInfo(nil)["AsukaAppGroup"] != g {
		t.Errorf("group %q", g)
	}
	m := c.androidManifest(false)
	single, multiple, _ := strings.Cut(m, "SEND_MULTIPLE")
	if !strings.Contains(single, `android:mimeType="text/plain"`) || !strings.Contains(single, `android:mimeType="image/*"`) ||
		!strings.Contains(multiple, `android:mimeType="video/*"`) || strings.Contains(multiple, "text/plain") {
		t.Errorf("Android's filters:\n%s", m)
	}

	// A type Apple has no key for: a predicate on the attachments' types.
	c, _ = load(`{types = ["audio/*", "application/pdf", "application/x-thing"]}`)
	want := `SUBQUERY(extensionItems, $e, SUBQUERY($e.attachments, $a, ANY $a.registeredTypeIdentifiers UTI-CONFORMS-TO "public.audio" OR ` +
		`ANY $a.registeredTypeIdentifiers UTI-CONFORMS-TO "com.adobe.pdf" OR ANY $a.registeredTypeIdentifiers UTI-CONFORMS-TO "public.data").@count >= 1).@count >= 1`
	if got := c.ShareTarget.activationRule(); got != want {
		t.Errorf("predicate\n%v\nwant\n%v", got, want)
	}
	if strings.Contains(c.androidManifest(false), "SEND_MULTIPLE") {
		t.Error("SEND_MULTIPLE for one file")
	}
}

// What an app's imports need of the platforms: their manifests'
// permissions and purpose strings; the host's optional modules only for
// what the host still does (a Video's player, push, periodic), the std
// packages' own libraries being their Gradle modules'.
func TestFeaturesFromImports(t *testing.T) {
	var c appConfig
	importStd(t, &c, "ios", "scan", "camera")
	if got := strings.Join(c.gradleProps(), " "); got != "-PasukaModules=" {
		t.Errorf("modules: %s", got)
	}
	if !slices.Contains(c.Android.Permissions, "android.permission.CAMERA") || slices.Contains(c.Android.Permissions, "android.permission.POST_NOTIFICATIONS") {
		t.Errorf("permissions: %v", c.Android.Permissions)
	}
	var pics appConfig
	importStd(t, &pics, "ios", "vision", "photos")
	if got := strings.Join(pics.gradleProps(), " "); got != "-PasukaModules=" {
		t.Errorf("modules: %s", got)
	}
	if !slices.Contains(pics.Android.Permissions, "android.permission.READ_MEDIA_IMAGES") || pics.IOS.Usage["NSPhotoLibraryUsageDescription"] == "" {
		t.Errorf("photos: %v %v", pics.Android.Permissions, pics.IOS.Usage)
	}
	var none appConfig
	importStd(t, &none, "ios")
	if got := strings.Join(none.gradleProps(), " "); got != "-PasukaModules=" {
		t.Errorf("no modules: %s", got)
	}

	// Video's player: in an app whose sources name Video, and in asuka
	// dev's player, which shows whatever the next save does.
	for _, x := range []struct {
		refs   map[string]bool
		player bool
		want   string
	}{
		{map[string]bool{"Video": true}, false, "-PasukaModules=video"},
		{map[string]bool{"Text": true}, false, "-PasukaModules="},
		{nil, true, "-PasukaModules=video"},
	} {
		var v appConfig
		app := &asukaApp{cfg: &v, uses: &appbuild.Uses{Refs: x.refs}}
		v.androidFeatures(app, x.player)
		v.androidFeatures(app, x.player)
		if got := strings.Join(v.gradleProps(), " "); got != x.want {
			t.Errorf("%v, player %v: %s, want %s", x.refs, x.player, got, x.want)
		}
	}
}

// An app that imports platform/pay gets Apple Pay's entitlement with
// asuka.toml's merchant IDs (none without them), and no host module
// (Google Pay's library, API and callbacks service are platform/pay's
// Gradle module's); one that doesn't, neither.
func TestPayFeature(t *testing.T) {
	dir := t.TempDir()
	os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte("[applePay]\nmerchantIds = [\"merchant.dev.asuka.shop\"]"), 0o644)
	c, err := loadConfig(dir)
	if err != nil {
		t.Fatal(err)
	}
	importStd(t, c, "", "pay")
	if got := strings.Join(c.gradleProps(), " "); got != "-PasukaModules=" {
		t.Errorf("modules: %s", got)
	}
	if ids, _ := c.entitlements["com.apple.developer.in-app-payments"].([]string); !slices.Equal(ids, []string{"merchant.dev.asuka.shop"}) {
		t.Errorf("entitlement: %v", c.entitlements)
	}
	var without appConfig
	without.ApplePay.MerchantIDs = []string{"merchant.dev.asuka.shop"}
	importStd(t, &without, "")
	if _, ok := without.entitlements["com.apple.developer.in-app-payments"]; ok || len(without.androidModules) != 0 {
		t.Errorf("an app without the package: %v", without.entitlements)
	}
	var noIDs appConfig
	importStd(t, &noIDs, "", "pay")
	if _, ok := noIDs.entitlements["com.apple.developer.in-app-payments"]; ok {
		t.Errorf("no merchant IDs: %v", noIDs.entitlements)
	}
}

func TestIPad(t *testing.T) {
	dir := t.TempDir()
	os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(`orientations = ["portrait"]`), 0o644)
	c, err := loadConfig(dir)
	if err != nil {
		t.Fatal(err)
	}
	// An iPad app, in every orientation whatever the iPhone's: Split
	// View and Stage Manager need them.
	if d := c.iosDevices(); !slices.Equal(d, []int{1, 2}) {
		t.Errorf("devices %v", d)
	}
	if o := c.ipadOrientations(); len(o) != 4 {
		t.Errorf("iPad orientations %v", o)
	}
	os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte("[ios]\niphoneOnly = true"), 0o644)
	if c, err = loadConfig(dir); err != nil {
		t.Fatal(err)
	}
	if d := c.iosDevices(); !slices.Equal(d, []int{1}) || c.ipadOrientations() != nil {
		t.Errorf("iPhone only: devices %v, iPad orientations %v", d, c.ipadOrientations())
	}
}

// An app with its own share sheet (platform/sharesheet): Android's share
// filters are its Activity's; iOS's extension is the host's sheet, with
// the app's ID, and an App Group for the data even for texts alone; the
// app's keychain group for both.
func TestShareSheetConfig(t *testing.T) {
	c := &appConfig{ID: "com.example.pins", ShareTarget: textShares, shareSheet: true}
	m := c.androidManifest(false)
	main, sheet, ok := strings.Cut(m, `dev.asuka.AsukaShareActivity`)
	if !ok || strings.Contains(main, "action.SEND") || !strings.Contains(sheet, "action.SEND") || !strings.Contains(sheet, `android:exported="true"`) {
		t.Errorf("manifest:\n%s", m)
	}
	info := c.shareInfo(nil)
	if info["NSExtension"].(map[string]any)["NSExtensionPrincipalClass"] != "AsukaShareSheetController" ||
		info["AsukaShareSheet"] != true || info["AsukaAppID"] != "com.example.pins" || info["AsukaAppGroup"] != "group.com.example.pins" {
		t.Errorf("extension's Info.plist: %v", info)
	}
	if g := c.keychainGroup("TEAM123456"); g != "TEAM123456.com.example.pins" {
		t.Errorf("keychain group %q", g)
	}
	c.shareSheet = false
	if c.appGroup() != "" || c.keychainGroup("T") != "" || c.sharePrincipal() != "AsukaShareViewController" {
		t.Error("no sheet: a group, or the sheet's class")
	}
}

// A push rewrite (platform/push's rewrite) gives the app a Notification
// Service Extension: the shared layout, the App Group and keychain group
// it shares, its own bundle ID; Android's manifest says so to the host.
func TestPushRewriteConfig(t *testing.T) {
	c := &appConfig{ID: "com.example.chat"}
	importStd(t, c, "ios", "push.register")
	if c.pushRewrite || c.sharedLayout() || c.appGroup() != "" || c.keychainGroup("TEAM") != "" {
		t.Fatal("no rewrite: one executable, no group")
	}
	if strings.Contains(c.androidManifest(false), "dev.asuka.push.rewrite") {
		t.Fatal("no rewrite: no meta-data")
	}
	c = &appConfig{ID: "com.example.chat"}
	importStd(t, c, "ios", "push.rewrite")
	if !c.pushRewrite {
		t.Fatal("push.rewrite: no rewrite")
	}
	if !c.sharedLayout() || c.appGroup() != "group.com.example.chat" || c.keychainGroup("TEAM") != "TEAM.com.example.chat" {
		t.Fatalf("rewrite: %v %q %q", c.sharedLayout(), c.appGroup(), c.keychainGroup("TEAM"))
	}
	info := c.notificationInfo(map[string]any{"CFBundleVersion": "7", "DTSDKName": "iphoneos", "Other": 1})
	ext := info["NSExtension"].(map[string]any)
	if info["CFBundleIdentifier"] != "com.example.chat.notification" || ext["NSExtensionPointIdentifier"] != "com.apple.usernotifications.service" ||
		ext["NSExtensionPrincipalClass"] != "AsukaNotificationService" || info["AsukaAppGroup"] != "group.com.example.chat" ||
		info["AsukaNotificationService"] != true || info["CFBundleVersion"] != "7" || info["DTSDKName"] != "iphoneos" || info["Other"] != nil {
		t.Fatalf("extension Info.plist %v", info)
	}
	if !strings.Contains(c.androidManifest(false), `<meta-data android:name="dev.asuka.push.rewrite" android:value="true" />`) {
		t.Fatal("rewrite: no meta-data")
	}
}

// asuka.toml's errors say where: a TOML one its line and column, a value
// Asuka can't take its key.
func TestConfigErrors(t *testing.T) {
	dir := t.TempDir()
	for doc, want := range map[string]string{
		"name = \"Notes\"\nbuild = \"7\"":     `asuka.toml:2:9: build: want an integer, not a string`,
		"[ios]\nteam = \"A\"\n[ios]":          `asuka.toml:3:2: table ios is defined twice`,
		"accent = \"red\"":                    `asuka.toml: accent "red" (want #rrggbb or #rrggbbaa)`,
		"accentDark = \"navy\"":               `asuka.toml: accentDark "navy" (want #rrggbb or #rrggbbaa)`,
		"[web.colors]\nlabelDark = \"gray\"":  `asuka.toml: web.colors.labelDark "gray" (want #rrggbb or #rrggbbaa)`,
		"shareTarget = \"yes\"":               `asuka.toml: shareTarget: want true, or a table of types and max`,
		"[[widgets]]\nkind = \"a\"\nsize = 1": ``,
	} {
		os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(doc), 0o644)
		_, err := loadConfig(dir)
		if got := fmt.Sprint(err); want == "" && err != nil || want != "" && got != want {
			t.Errorf("%q: got %v, want %s", doc, err, want)
		}
	}
}

// [web.colors] is the web page's palette: a style sheet after the host's
// with each color set, light and dark (a color with no dark one in both),
// and the browser's theme color from the background; with none, the page
// is the host's alone.
func TestWebColors(t *testing.T) {
	dir := t.TempDir()
	os.WriteFile(filepath.Join(dir, "asuka.toml"), []byte(`[web.colors]
label = "#16181d"
labelDark = "#e8e9eb"
separator = "dfe1e5"
background = "#ffffff"
backgroundDark = "#101114"
cardDark = "#1b1d21"
`), 0o644)
	c, err := loadConfig(dir)
	if err != nil {
		t.Fatal(err)
	}
	want := ".asuka { --asuka-label: #16181d; --asuka-background: #ffffff; --asuka-separator: #dfe1e5; }\n" +
		".asuka.asuka-dark { --asuka-label: #e8e9eb; --asuka-background: #101114; --asuka-separator: #dfe1e5; --asuka-card: #1b1d21; }\n"
	if got := c.Web.Colors.css(); got != want {
		t.Fatalf("css:\n%s\nwant:\n%s", got, want)
	}
	page := webIndex(webPage{Name: "Notes", Lang: "en", Host: "asuka/x", Core: "asuka.wasm", Colors: c.Web.Colors})
	if i, j := strings.Index(page, `<link rel="stylesheet"`), strings.Index(page, "<style>\n"+want+"</style>"); j < 0 || j < i {
		t.Fatalf("no style sheet after the host's:\n%s", page)
	}
	if !strings.Contains(page, `<meta name="theme-color" content="#ffffff" media="(prefers-color-scheme: light)">`) ||
		!strings.Contains(page, `<meta name="theme-color" content="#101114" media="(prefers-color-scheme: dark)">`) {
		t.Fatalf("theme colors:\n%s", page)
	}
	if page := webIndex(webPage{Name: "Notes", Lang: "en", Host: "asuka/x", Core: "asuka.wasm"}); strings.Contains(page, "<style>") ||
		!strings.Contains(page, `content="#121212" media="(prefers-color-scheme: dark)"`) {
		t.Fatalf("no colors:\n%s", page)
	}
}
