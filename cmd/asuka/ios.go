package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"html"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"slices"
	"sort"
	"strings"
	"time"

	"github.com/egoist/asuka/internal/appbuild"
	"github.com/egoist/asuka/internal/cbuild"
	"github.com/egoist/asuka/lang/bytecode"
	iosHost "github.com/egoist/asuka/platform/ios"
)

// iOS: the Swift host (platform/ios, compiled once and cached) linked
// with the C core (libasuka.a) into an .app. A release's core has the
// app compiled into it (asuka_run); asuka dev's is the player, the core
// without an app, which loads the bytecode asuka dev sends it and again
// on each save, keeping its state (asuka_dev_connect, ASUKA_DEV in its
// environment): it's built and installed once, and again only when the
// app's settings or native code change.

const iosMin = "16.0"

type iosToolchain struct {
	sdk, sdkName, clang, target, goarch string
	release                             bool // optimized, without the debug automation
	verbose                             bool
	// packages is what the app's Swift packages bring besides the host's
	// object, as the last buildHost built them (nil without any).
	packages *swiftPackageOutput
	// cLink is what the app's C ([c]'s libraries and frameworks) links,
	// as the last buildCore built it: the executable's link adds it.
	cLink []string
}

func newIOSToolchain(verbose bool) (*iosToolchain, error) {
	sdk, err := output("xcrun", "--sdk", "iphonesimulator", "--show-sdk-path")
	if err != nil {
		return nil, fmt.Errorf("no iOS simulator SDK (install Xcode): %v", err)
	}
	clang, err := output("xcrun", "--sdk", "iphonesimulator", "--find", "clang")
	if err != nil {
		return nil, err
	}
	// Apple silicon's simulator: the Macs Asuka develops on.
	arch, goarch := "arm64", "arm64"
	return &iosToolchain{sdk: sdk, sdkName: "iphonesimulator", clang: clang, target: arch + "-apple-ios" + iosMin + "-simulator", goarch: goarch, verbose: verbose}, nil
}

func output(name string, args ...string) (string, error) {
	out, err := exec.Command(name, args...).Output()
	if err != nil {
		if ee, ok := err.(*exec.ExitError); ok {
			return "", fmt.Errorf("%s: %v: %s", name, err, bytes.TrimSpace(ee.Stderr))
		}
		return "", err
	}
	return strings.TrimSpace(string(out)), nil
}

func (t *iosToolchain) run(dir string, env []string, name string, args ...string) error {
	cmd := exec.Command(name, args...)
	cmd.Dir = dir
	cmd.Env = append(os.Environ(), env...)
	if t.verbose {
		fmt.Fprintln(os.Stderr, "+", strings.Join(append(env, append([]string{name}, args...)...), " "))
	}
	out, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("%s failed:\n%s", name, filterLinker(out))
	}
	return nil
}

// filterLinker drops the linker's noise about SDK versions.
func filterLinker(out []byte) string {
	var keep []string
	for _, l := range strings.Split(string(out), "\n") {
		if strings.Contains(l, "Wincompatible-sysroot") || strings.HasPrefix(l, "ld: warning:") {
			continue
		}
		keep = append(keep, l)
	}
	return strings.TrimSpace(strings.Join(keep, "\n"))
}

// buildHost builds the host's Swift with the app's native packages'
// (swiftBuild: cached modules, or prebuilt ones) into one object.
func (t *iosToolchain) buildHost(work string, app *asukaApp, features []string) (string, error) {
	var natives []nativePackage
	if app != nil {
		// The app's native packages' Swift; what they need goes in its
		// Info.plist.
		ns, err := nativeFor(app, "ios")
		if err != nil {
			return "", err
		}
		if len(ns) > 0 {
			if err := app.cfg.nativeNeeds(ns, "ios"); err != nil {
				return "", err
			}
		}
		natives = ns
	}
	return t.buildHostPackage(work, natives, features)
}

// iosHostVariant names the host's variant a release prebuilds for
// features: base (none), all (a player's), or "" for another.
func iosHostVariant(features []string) string {
	if len(features) == 0 {
		return "base"
	}
	if f := slices.Compact(sortedCopy(features)); slices.Equal(f, allFeatures()) {
		return "all"
	}
	return ""
}

// buildHostPackage builds the host's Swift with the app's native
// packages' (native_swift.go) into one object, as buildHost's swiftc
// build is: the packages' objects linked together. What else they bring
// (static libraries, frameworks, resource bundles) is t.packages, which
// linkApp links and embedPackages puts in the app.
func (t *iosToolchain) buildHostPackage(work string, natives []nativePackage, features []string) (string, error) {
	host, err := swiftSources(iosHost.Files, "Sources/AsukaHost", "Sources/AsukaOps", "App")
	if err != nil {
		return "", err
	}
	cAsuka, err := filesUnder(iosHost.Files, "Sources/CAsuka")
	if err != nil {
		return "", err
	}
	b := &swiftBuild{dir: filepath.Join(work, "swift-"+t.sdkName), platform: "ios", sdkName: t.sdkName,
		arch: macSwiftArch(t.goarch), release: t.release, defines: features, variant: iosHostVariant(features),
		host: host, cAsuka: cAsuka, natives: natives, run: t.run}
	out, err := b.build()
	if err != nil {
		return "", err
	}
	obj := filepath.Join(b.dir, "AsukaApp-linked.o")
	if err := os.MkdirAll(b.dir, 0o755); err != nil {
		return "", err
	}
	args := append([]string{"--sdk", t.sdkName, "clang", "-target", t.target, "-r", "-o", obj}, out.objects...)
	for _, a := range out.forceLoad {
		args = append(args, "-Wl,-force_load,"+a)
	}
	if err := t.run(work, nil, "xcrun", args...); err != nil {
		return "", err
	}
	out.forceLoad = nil // in obj now
	t.packages = out
	return obj, nil
}

// elementFlags are the parts of the iOS host that link a framework of
// their own for one of the core's elements (WebKit, AVKit, Metal), by the
// prelude's element: loading a framework costs at launch whether or not
// it's used (WebKit and AVKit's together, about a second on the
// simulator), so a release's host is built with only those its app
// shows. The host's modules' flags are the packages' to say (their
// manifests' ios.hostFlags, a package's or a member's: depManifests);
// a package with Swift of its own (platform/camera, platform/location...)
// brings it, and its frameworks, to the apps that import it, and needs
// no flag. asuka dev's player has them all.
var elementFlags = map[string]string{
	"WebView": "ASUKA_WEBVIEW",
	"Video":   "ASUKA_VIDEO",
	"Shader":  "ASUKA_SHADER",
}

// allFeatures is every flag: the player runs any app.
func allFeatures() []string {
	out := stdHostFlags()
	for _, f := range elementFlags {
		if !slices.Contains(out, f) {
			out = append(out, f)
		}
	}
	sort.Strings(out)
	return out
}

// usedFeatures are the flags the app's host is built with: the elements
// it shows, and what its packages' manifests (and their members' the
// program refers to) ask for.
func usedFeatures(app *asukaApp, cfg *appConfig) []string {
	var used []string
	for _, el := range manifestKeys(elementFlags) {
		if app.refs("", el) {
			used = append(used, elementFlags[el])
		}
	}
	for _, f := range cfg.hostFlags {
		if !slices.Contains(used, f) {
			used = append(used, f)
		}
	}
	sort.Strings(used)
	return used
}

// playerFeatures are every feature, for asuka dev's player, which runs
// whatever the app's next save calls; what they need of the app
// (entitlements, background modes) is only what the app uses' (which a
// device's development profile must allow).
func playerFeatures(app *asukaApp, cfg *appConfig) []string {
	usedFeatures(app, cfg)
	return allFeatures()
}

// buildCore builds the C core for the SDK's target into work: with m
// compiled in (a release), or without an app (m nil: asuka dev's
// player), with the C the app's externs call (m's, or for a player c,
// the last compile's).
func (t *iosToolchain) buildCore(work string, m *bytecode.Module, c *bytecode.C) (string, error) {
	target := cbuild.Target{OS: "ios", Arch: "arm64"}
	if t.sdkName == "iphonesimulator" {
		target = cbuild.Target{OS: "ios-simulator", Arch: t.goarch}
	}
	if m != nil {
		c = m.C
	}
	// A static library links nothing: the executable links what its C
	// needs.
	f, err := appbuild.Foreign(c, target)
	if err != nil {
		return "", err
	}
	t.cLink = nil
	if f != nil {
		t.cLink = f.Link
	}
	lib := filepath.Join(work, "libasuka.a")
	if m != nil {
		return lib, appbuild.Lib(target, cbuild.Static, m, lib)
	}
	return lib, appbuild.PlayerLib(target, cbuild.Static, c, lib)
}

// embedPackages puts the frameworks of the app's Swift packages in
// bundle's Frameworks, and their resource bundles in it; signFrameworks
// signs the frameworks.
func (t *iosToolchain) embedPackages(bundle string) error {
	return t.packages.embed(filepath.Join(bundle, "Frameworks"), bundle)
}

func (t *iosToolchain) link(app *asukaApp, work, hostObj, lib string) (string, error) {
	bundle := filepath.Join(work, app.name+".app")
	os.RemoveAll(bundle)
	if err := os.MkdirAll(bundle, 0o755); err != nil {
		return "", err
	}
	ents, err := simulatorEntitlements(work, app.cfg)
	if err != nil {
		return "", err
	}
	code, err := t.linkApp(work, bundle, app.cfg, hostObj, lib, ents)
	if err != nil {
		return "", err
	}
	if err := t.embedPackages(bundle); err != nil {
		return "", err
	}
	if err := copyDir(filepath.Join(app.dir, "assets"), filepath.Join(bundle, "assets")); err != nil {
		return "", err
	}
	if err := app.cfg.copyStoreKit(bundle); err != nil {
		return "", err
	}
	urls := ""
	if t := app.cfg.urlTypes(true); t != nil {
		schemes := t[0].(map[string]any)["CFBundleURLSchemes"].([]string)
		for i := range schemes {
			schemes[i] = html.EscapeString(schemes[i])
		}
		urls = fmt.Sprintf("<key>CFBundleURLTypes</key><array><dict><key>CFBundleURLName</key><string>%s</string>"+
			"<key>CFBundleURLSchemes</key><array><string>%s</string></array></dict></array>\n",
			html.EscapeString(app.bundleID), strings.Join(schemes, "</string><string>"))
	}
	// The purpose strings: without one, iOS ends an app that asks for what it protects.
	keys := make([]string, 0, len(app.cfg.IOS.Usage))
	for k := range app.cfg.IOS.Usage {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, k := range keys {
		urls += fmt.Sprintf("<key>%s</key><string>%s</string>\n", html.EscapeString(k), html.EscapeString(app.cfg.IOS.Usage[k]))
	}
	for _, k := range app.cfg.iosTrue {
		urls += fmt.Sprintf("<key>%s</key><true/>\n", html.EscapeString(k))
	}
	urls += plistKeys(app.cfg.plistExtras())
	if g := app.cfg.appGroup(); g != "" {
		urls += fmt.Sprintf("<key>AsukaAppGroup</key><string>%s</string>\n", html.EscapeString(g))
	}
	if app.cfg.ShareTarget.on && app.cfg.shareSheet {
		urls += "<key>AsukaShareSheet</key><true/>\n"
	}
	if app.cfg.pushRewrite {
		urls += "<key>AsukaNotificationService</key><true/>\n"
	}

	devices := ""
	for _, d := range app.cfg.iosDevices() {
		devices += fmt.Sprintf("<integer>%d</integer>", d)
	}
	if o := app.cfg.ipadOrientations(); o != nil {
		urls += "<key>UISupportedInterfaceOrientations~ipad</key><array><string>" + strings.Join(o, "</string><string>") + "</string></array>\n"
	}
	urls += app.cfg.plistLocalizationKeys()
	plist := fmt.Sprintf(infoPlist, app.bundleID, app.displayName(), app.displayName(), iosMin, devices, urls)
	if err := os.WriteFile(filepath.Join(bundle, "Info.plist"), []byte(plist), 0o644); err != nil {
		return "", err
	}
	if err := app.cfg.writeLprojs(bundle); err != nil {
		return "", err
	}
	// Its icon too, so the player shows on the home screen as the app does.
	if app.cfg.Icon != "" || app.cfg.splashOn() {
		if err := t.compileAssets(app.cfg, work, bundle, filepath.Join(bundle, "Info.plist"), app.cfg.Icon != ""); err != nil {
			return "", err
		}
	}
	// Signed as Xcode signs for the simulator (simulatorIdentity).
	sign := simulatorIdentity(app.cfg.IOS.Team)
	if sign == "-" && len(app.cfg.Shortcuts) > 0 {
		fmt.Println("asuka: no Apple Development identity in the keychain: the app is signed ad hoc, and its shortcuts' App Intents won't run (Xcode › Settings › Accounts makes one)")
	}
	if app.cfg.ShareTarget.on {
		exe, err := t.buildShare(work, app.cfg, code)
		if err != nil {
			return "", err
		}
		appex, err := embedShare(bundle, exe, app.cfg.shareInfo(map[string]any{
			"CFBundleName": app.displayName(), "CFBundleDisplayName": app.displayName(), "CFBundleShortVersionString": "1.0", "CFBundleVersion": "1", "MinimumOSVersion": iosMin,
			"CFBundleSupportedPlatforms": []string{"iPhoneSimulator"}, "UIDeviceFamily": app.cfg.iosDevices(),
		}))
		if err != nil {
			return "", err
		}
		if err := t.run(work, nil, "codesign", "--force", "--sign", sign, appex); err != nil {
			return "", err
		}
	}
	if app.cfg.pushRewrite {
		exe, err := t.buildNotification(work, app.cfg, code)
		if err != nil {
			return "", err
		}
		appex, err := embedNotification(bundle, exe, app.cfg.notificationInfo(map[string]any{
			"CFBundleName": app.displayName(), "CFBundleDisplayName": app.displayName(), "CFBundleShortVersionString": "1.0", "CFBundleVersion": "1", "MinimumOSVersion": iosMin,
			"CFBundleSupportedPlatforms": []string{"iPhoneSimulator"}, "UIDeviceFamily": app.cfg.iosDevices(),
		}))
		if err != nil {
			return "", err
		}
		if err := t.run(work, nil, "codesign", "--force", "--sign", sign, appex); err != nil {
			return "", err
		}
	}
	if app.cfg.widgetExtension() {
		exe, err := t.buildWidgets(work, app.cfg)
		if err != nil {
			return "", err
		}
		appex, err := embedWidgets(bundle, exe, app.cfg.widgetInfo(map[string]any{
			"CFBundleName": app.displayName(), "CFBundleDisplayName": app.displayName(), "CFBundleShortVersionString": "1.0", "CFBundleVersion": "1", "MinimumOSVersion": iosMin,
			"CFBundleSupportedPlatforms": []string{"iPhoneSimulator"}, "UIDeviceFamily": app.cfg.iosDevices(),
		}))
		if err != nil {
			return "", err
		}
		if err := app.cfg.writeLprojs(appex); err != nil {
			return "", err
		}
		if err := t.run(work, nil, "codesign", "--force", "--sign", sign, appex); err != nil {
			return "", err
		}
	}
	if err := t.signFrameworks(work, bundle, sign); err != nil {
		return "", err
	}
	return bundle, t.run(work, nil, "codesign", "--force", "--sign", sign, bundle)
}

// simulatorEntitlements are the linker flags that give a simulator build
// the entitlements the keychain wants (an app identifier and its access
// group; without them SecItemAdd fails with errSecMissingEntitlement, and
// platform/secure with it). As Xcode does: in the binary's __entitlements
// section, for an ad hoc signature can't carry them.
func simulatorEntitlements(work string, cfg *appConfig) ([]string, error) {
	id := "ASUKASIMULA." + cfg.ID // a team ID's shape: ten characters
	ent := filepath.Join(work, "simulator.entitlements")
	m := map[string]any{
		"application-identifier": id,
		"keychain-access-groups": []string{id},
	}
	if ds := cfg.associatedDomains(true); len(ds) > 0 {
		m["com.apple.developer.associated-domains"] = ds
	}
	if g := cfg.appGroup(); g != "" {
		m["com.apple.security.application-groups"] = []string{g}
	}
	for k, v := range cfg.entitlements {
		m[k] = iCloudTeam(v, firstOf(cfg.IOS.Team, "ASUKASIMULA"))
	}
	if cfg.IOS.StoreKit != "" {
		// storekitd takes a StoreKit configuration only for an app
		// installed for development, as Xcode's debug builds are.
		m["get-task-allow"] = true
	}
	text := plist(m)
	if err := os.WriteFile(ent, []byte(text), 0o644); err != nil {
		return nil, err
	}
	return []string{"-Xlinker", "-sectcreate", "-Xlinker", "__TEXT", "-Xlinker", "__entitlements", "-Xlinker", ent}, nil
}

const infoPlist = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>%s</string>
<key>CFBundleExecutable</key><string>AsukaApp</string>
<key>CFBundleName</key><string>%s</string>
<key>CFBundleDisplayName</key><string>%s</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>1.0</string>
<key>CFBundleVersion</key><string>1</string>
<key>MinimumOSVersion</key><string>%s</string>
<key>CFBundleSupportedPlatforms</key><array><string>iPhoneSimulator</string></array>
<key>UIDeviceFamily</key><array>%s</array>
<key>CADisableMinimumFrameDurationOnPhone</key><true/>
<key>NSAppTransportSecurity</key><dict><key>NSAllowsLocalNetworking</key><true/></dict>
<key>UIApplicationSceneManifest</key><dict><key>UIApplicationSupportsMultipleScenes</key><false/></dict>
<key>UISupportedInterfaceOrientations</key><array><string>UIInterfaceOrientationPortrait</string><string>UIInterfaceOrientationLandscapeLeft</string><string>UIInterfaceOrientationLandscapeRight</string></array>
%s</dict></plist>
`

type simDevice struct {
	Name        string `json:"name"`
	UDID        string `json:"udid"`
	State       string `json:"state"`
	IsAvailable bool   `json:"isAvailable"`
	runtime     string
}

// showSimulator shows the simulator udid names: in Simulator.app (Xcode
// 26 and before), or in Xcode 27's Device Hub, which has its window
// (Xcode 27 has no Simulator.app), on that device.
func showSimulator(udid string) {
	if exec.Command("open", "-a", "Simulator").Run() == nil {
		return
	}
	if exec.Command("open", "devices://device/open?id="+udid).Run() == nil {
		return
	}
	exec.Command("open", "-a", "DeviceHub").Run()
}

// pickSimulator returns the device named want, or the booted iPhone, or
// the newest available iPhone, booting it if needed.
func pickSimulator(want string) (*simDevice, error) {
	out, err := exec.Command("xcrun", "simctl", "list", "devices", "available", "-j").Output()
	if err != nil {
		return nil, fmt.Errorf("simctl: %v (install Xcode and an iOS simulator runtime)", err)
	}
	var list struct {
		Devices map[string][]simDevice `json:"devices"`
	}
	if err := json.Unmarshal(out, &list); err != nil {
		return nil, err
	}
	var all []*simDevice
	runtimes := make([]string, 0, len(list.Devices))
	for rt := range list.Devices {
		if strings.Contains(rt, "iOS") {
			runtimes = append(runtimes, rt)
		}
	}
	sort.Sort(sort.Reverse(sort.StringSlice(runtimes)))
	for _, rt := range runtimes {
		for i := range list.Devices[rt] {
			d := list.Devices[rt][i]
			d.runtime = rt
			if d.IsAvailable {
				all = append(all, &d)
			}
		}
	}
	var pick *simDevice
	for _, d := range all {
		switch {
		case want != "" && (d.UDID == want || d.Name == want):
			pick = d
		case want == "" && (d.State == "Booted" || d.State == "Booting") && strings.HasPrefix(d.Name, "iPhone") && pick == nil:
			pick = d // one that's up, or coming up
		}
	}
	if pick == nil && want == "" {
		for _, d := range all {
			if strings.HasPrefix(d.Name, "iPhone") && strings.Contains(d.Name, "Pro") && !strings.Contains(d.Name, "Max") {
				pick = d
				break
			}
		}
		if pick == nil {
			for _, d := range all {
				if strings.HasPrefix(d.Name, "iPhone") {
					pick = d
					break
				}
			}
		}
	}
	if pick == nil {
		if want != "" {
			return nil, fmt.Errorf("no simulator %q (see xcrun simctl list devices)", want)
		}
		return nil, errors.New("no iPhone simulator; add one in Xcode")
	}
	if pick.State != "Booted" {
		fmt.Printf("asuka: booting %s\n", pick.Name)
		start := time.Now()
		defer func() { fmt.Printf("asuka: %s booted in %.1fs\n", pick.Name, time.Since(start).Seconds()) }()
		// One already booting (Simulator.app's, a script's) is waited for.
		if pick.State != "Booting" {
			if out, err := exec.Command("xcrun", "simctl", "boot", pick.UDID).CombinedOutput(); err != nil && !bytes.Contains(out, []byte("current state: Boot")) {
				return nil, fmt.Errorf("simctl boot: %s", bytes.TrimSpace(out))
			}
		}
		exec.Command("xcrun", "simctl", "bootstatus", pick.UDID, "-b").Run()
		showSimulator(pick.UDID) // shows it, when installed
	}
	return pick, nil
}

func devIOS(dir string, o *devOptions, stop <-chan os.Signal) error {
	app, err := loadApp(dir, platformIOS)
	if err != nil {
		return err
	}
	// An iPhone or iPad --device names (ios_device.go).
	if d, err := findIOSDevice(o.device); err != nil {
		return err
	} else if d != nil {
		return devIOSDevice(app, o, d, stop)
	}
	tc, err := newIOSToolchain(o.verbose)
	if err != nil {
		return err
	}
	work, err := workDir(app, "ios")
	if err != nil {
		return err
	}
	sim, err := pickSimulator(o.device)
	if err != nil {
		return err
	}
	fmt.Printf("asuka: %s on %s (%s)\n", app.name, sim.Name, sim.UDID)
	// The simulator shares this Mac's network: the player reaches asuka
	// dev on its loopback.
	loop, err := newDevLoop(app, "127.0.0.1:0")
	if err != nil {
		return err
	}
	return loop.run(simulatorPlayer(app, tc, work, sim, loop), stop, o.once)
}

// simulatorPlayer is the player on the simulator sim: built, installed
// when it changed, launched connected to the loop.
func simulatorPlayer(app *asukaApp, tc *iosToolchain, work string, sim *simDevice, loop *devLoop) devPlayer {
	installed := ""
	launch := func() error {
		start := time.Now()
		bundle, err := tc.buildPlayer(app, work)
		if err != nil {
			return err
		}
		if sum := dirDigest(bundle); sum != installed {
			if err := tc.run(work, nil, "xcrun", "simctl", "install", sim.UDID, bundle); err != nil {
				return err
			}
			installed = sum
			fmt.Printf("asuka: the player built and installed in %.1fs\n", time.Since(start).Seconds())
		}
		// Its environment: SIMCTL_CHILD_ names are the app's.
		env := append(app.cfg.storeKitLaunchEnv(), fmt.Sprintf("SIMCTL_CHILD_ASUKA_DEV=127.0.0.1:%d", loop.port()))
		return tc.run(work, env, "xcrun", "simctl", "launch", "--terminate-running-process", sim.UDID, app.bundleID)
	}
	return devPlayer{
		launch: launch,
		stop:   func() { exec.Command("xcrun", "simctl", "terminate", sim.UDID, app.bundleID).Run() },
	}
}

// buildPlayer builds asuka dev's player for the simulator: the app's
// .app (its Info.plist, icon, entitlements, native packages' Swift), with
// every feature of the host and the core without an app.
func (t *iosToolchain) buildPlayer(app *asukaApp, work string) (string, error) {
	if err := generateNatives(app); err != nil {
		return "", err
	}
	lib, err := t.buildCore(work, nil, app.c)
	if err != nil {
		return "", err
	}
	hostObj, err := t.buildHost(work, app, playerFeatures(app, app.cfg))
	if err != nil {
		return "", err
	}
	return t.link(app, work, hostObj, lib)
}

// copyDir copies the files under src (if it exists) to dst.
func copyDir(src, dst string) error {
	if _, err := os.Stat(src); err != nil {
		return nil
	}
	return filepath.WalkDir(src, func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		rel, _ := filepath.Rel(src, path)
		out := filepath.Join(dst, rel)
		if d.IsDir() {
			return os.MkdirAll(out, 0o755)
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		return os.WriteFile(out, data, 0o644)
	})
}
