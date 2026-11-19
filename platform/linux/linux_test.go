// The Linux host's tests. They build apps (an Asuka source each) into the
// executable an app ships, the C host and the core with the app compiled
// to C, and drive them as a user does (ASUKA_SCRIPT), reading back what
// they showed (ASUKA_DUMP, ASUKA_SNAPSHOT). They run on Linux with a
// display and a session bus, as the CI's container runs them:
//
//	xvfb-run -a dbus-run-session go test ./platform/linux
//
// Elsewhere (or without a display) they skip. test/fakes.c gives the
// session bus what a desktop has (a notification server, a
// StatusNotifierWatcher), so notifications and the tray are driven too.
package linux_test

import (
	"bufio"
	"bytes"
	"fmt"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
	"time"

	"github.com/egoist/asuka/internal/appbuild"
	"github.com/egoist/asuka/internal/cbuild"
	"github.com/egoist/asuka/lang/compile"
)

func onLinux(t *testing.T) {
	t.Helper()
	if runtime.GOOS != "linux" {
		t.Skip("the Linux host's tests run on Linux (xvfb-run -a dbus-run-session go test ./platform/linux)")
	}
	if os.Getenv("DISPLAY") == "" && os.Getenv("WAYLAND_DISPLAY") == "" {
		t.Skip("no display: xvfb-run -a dbus-run-session go test ./platform/linux")
	}
	if os.Getenv("DBUS_SESSION_BUS_ADDRESS") == "" {
		t.Skip("no session bus: dbus-run-session go test ./platform/linux")
	}
}

func target() cbuild.Target { return cbuild.Target{OS: "linux", Arch: runtime.GOARCH} }

// buildApp compiles src (an Asuka file with its app) to C and builds the
// app's executable, as asuka build does, at exe.
func buildApp(t *testing.T, src, exe string) {
	t.Helper()
	res := compile.Files(map[string][]byte{"app.asuka": []byte(src)}, &compile.Options{Platform: "linux"})
	if err := res.Err(); err != nil {
		t.Fatalf("the app doesn't compile:\n%v", err)
	}
	if err := os.MkdirAll(filepath.Dir(exe), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := appbuild.LinuxExe(target(), appbuild.StripTests(res.Module), nil, exe); err != nil {
		t.Fatal(err)
	}
}

func testdata(t *testing.T, name string) string {
	t.Helper()
	b, err := os.ReadFile(filepath.Join("testdata", name))
	if err != nil {
		t.Fatal(err)
	}
	return string(b)
}

// runEnv is the environment the apps under test run in: their own data,
// config and cache directories (HOME too), so a run leaves nothing of the
// user's touched, and GTK's settings as a fresh session has them.
func runEnv(t *testing.T, dir string) []string {
	env := os.Environ()
	for _, kv := range [][2]string{
		{"HOME", filepath.Join(dir, "home")},
		{"XDG_DATA_HOME", filepath.Join(dir, "home", "data")},
		{"XDG_CONFIG_HOME", filepath.Join(dir, "home", "config")},
		{"XDG_CACHE_HOME", filepath.Join(dir, "home", "cache")},
	} {
		if err := os.MkdirAll(kv[1], 0o755); err != nil {
			t.Fatal(err)
		}
		env = append(env, kv[0]+"="+kv[1])
	}
	env = append(env, "GSETTINGS_BACKEND=memory", "WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1")
	return env
}

// script runs exe with ASUKA_SCRIPT's steps, a snap:<name> each dumped,
// and returns the dumps by name ("end" the last).
func script(t *testing.T, dir, exe string, steps ...string) map[string]string {
	t.Helper()
	return scriptEnv(t, dir, exe, nil, steps...)
}

// scriptEnv is script with more of the environment.
func scriptEnv(t *testing.T, dir, exe string, env []string, steps ...string) map[string]string {
	t.Helper()
	var s, snaps []string
	for _, st := range steps {
		if name, ok := strings.CutPrefix(st, "snap:"); ok {
			snaps = append(snaps, name)
			st = "snap:" + filepath.Join(dir, name+".png")
		}
		s = append(s, st)
	}
	cmd := exec.Command(exe)
	cmd.Dir = dir
	cmd.Env = append(runEnv(t, dir), "ASUKA_SCRIPT="+strings.Join(s, ";"), "ASUKA_DUMP=1", "ASUKA_SNAPSHOT="+filepath.Join(dir, "end.png"))
	cmd.Env = append(cmd.Env, env...)
	var out bytes.Buffer
	cmd.Stdout, cmd.Stderr = &out, &out
	if err := cmd.Start(); err != nil {
		t.Fatal(err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	select {
	case err := <-done:
		if err != nil {
			t.Fatalf("%s: %v\n%s", exe, err, out.String())
		}
	case <-time.After(2 * time.Minute):
		cmd.Process.Kill()
		t.Fatalf("%s didn't end in 2 minutes\n%s", exe, out.String())
	}
	dumps := map[string]string{}
	for _, name := range append(snaps, "end") {
		b, err := os.ReadFile(filepath.Join(dir, name+".png.txt"))
		if err != nil {
			t.Fatalf("no dump %s: %v\n%s", name, err, out.String())
		}
		dumps[name] = string(b)
	}
	if o := out.String(); strings.Contains(o, "script: ") {
		t.Errorf("the script missed a step:\n%s", o)
	}
	return dumps
}

// shows checks that a dump shows each of want and none of not (a view
// hidden, or not mapped, isn't shown).
func shows(t *testing.T, dump, where string, want []string, not ...string) {
	t.Helper()
	for _, s := range want {
		if !strings.Contains(dump, fmt.Sprintf("%q", s)) {
			t.Errorf("%s: %q isn't shown:\n%s", where, s, dump)
		}
	}
	for _, s := range not {
		for _, l := range strings.Split(dump, "\n") {
			if strings.Contains(l, fmt.Sprintf("%q", s)) && !strings.Contains(l, " hidden") && !strings.Contains(l, " unmapped") {
				t.Errorf("%s: %q is shown:\n%s", where, s, dump)
				break
			}
		}
	}
}

const counter = `
component Counter {
  state count = 0
  Column {
    gap: 8; padding: 16
    Text("Count: {count}")
    Button("Add") { on press { count += 1 } }
  }
}
app { Counter() }
`

// The Counter as an app ships: compiled to C into the host's executable,
// Add pressed (GtkButton's clicked), the text set again, laid out in the
// window, and the window written.
func TestCounter(t *testing.T) {
	onLinux(t)
	dir := t.TempDir()
	exe := filepath.Join(dir, "app")
	buildApp(t, counter, exe)
	d := script(t, dir, exe, "tap:Add", "tap:Add")
	shows(t, d["end"], "after two taps", []string{"Count: 2"}, "Count: 0")
	if st, err := os.Stat(filepath.Join(dir, "end.png")); err != nil || st.Size() == 0 {
		t.Fatalf("no snapshot: %v", err)
	}
}

// Navigation (testdata/nav.asuka): tabs, a list, a sheet's form typed in
// and saved, back.
func TestNavigation(t *testing.T) {
	onLinux(t)
	dir := t.TempDir()
	exe := filepath.Join(dir, "app")
	buildApp(t, testdata(t, "nav.asuka"), exe)
	d := script(t, dir, exe,
		"snap:inbox",
		"tap:Add a note", "snap:sheet",
		"tap:Note", "type:hello", "tap:Save", "snap:saved",
		"tap:Settings", "snap:settings",
		"tap:Inbox", "snap:back")
	shows(t, d["inbox"], "the inbox", []string{"Message 30", "Message 29", "No note yet."})
	if !strings.Contains(d["sheet"], " Sheet [") {
		t.Errorf("no sheet shows:\n%s", d["sheet"])
	}
	shows(t, d["saved"], "saved", []string{"Note: hello"})
	if strings.Contains(d["saved"], " Sheet [") {
		t.Errorf("the sheet stayed:\n%s", d["saved"])
	}
	shows(t, d["settings"], "settings", []string{"Notifications"}, "Note: hello")
	shows(t, d["back"], "the inbox again", []string{"Note: hello"}, "Notifications")
}

// Rich text (testdata/rich.asuka): a RichText's spans and paragraphs, a
// rich TextField's styled text in and the user's typing out, and the
// core's text replacing it.
func TestRichText(t *testing.T) {
	onLinux(t)
	dir := t.TempDir()
	exe := filepath.Join(dir, "app")
	buildApp(t, testdata(t, "rich.asuka"), exe)
	d := script(t, dir, exe, "snap:start", "tap:Notes", "type:X", "snap:typed", "tap:Reset", "snap:reset")
	shows(t, d["start"], "the start", []string{"Read the docs first.\nSecond line.", "Hello world", "Spans: 2"})
	if !strings.Contains(d["typed"], `XHello world"`) && !strings.Contains(d["typed"], `"Hello worldX"`) {
		t.Errorf("the typing isn't in the field:\n%s", d["typed"])
	}
	shows(t, d["reset"], "reset", []string{"Plain", "Spans: 1"})
}

// Overlays and pickers (testdata/overlays.asuka): a split view's
// sidebar, a Picker's choice, a Popover opened and closed from inside it.
func TestOverlays(t *testing.T) {
	onLinux(t)
	dir := t.TempDir()
	exe := filepath.Join(dir, "app")
	buildApp(t, testdata(t, "overlays.asuka"), exe)
	d := script(t, dir, exe,
		"snap:start",
		"tap:Banana", "snap:picked",
		"tap:Info", "snap:popover",
		"tap:Done", "snap:closed",
		"tap:Drafts", "snap:drafts")
	shows(t, d["start"], "the inbox", []string{"No fruit", "Closed"})
	shows(t, d["picked"], "picked", []string{"Fruit 1"}, "No fruit")
	shows(t, d["popover"], "the popover", []string{"Open"}, "Closed")
	shows(t, d["closed"], "closed", []string{"Closed"}, "Open")
	shows(t, d["drafts"], "drafts", []string{"Draft list"}, "Fruit 1")
}

// A Picker takes its font: one in a 30-point font is taller and wider
// than the same Picker in the theme's.
func TestPickerFont(t *testing.T) {
	onLinux(t)
	dir := t.TempDir()
	exe := filepath.Join(dir, "app")
	buildApp(t, `
component Pickers {
  Column {
    gap: 8; padding: 16; align: .start
    Picker(["One", "Two"]) { selected: 0 }
    Picker(["One", "Two"]) { selected: 0; font: Font(size: 30) }
  }
}
app { Pickers() }
`, exe)
	d := script(t, dir, exe, "snap:start")
	var p []dumpNode
	for _, n := range parseDump(t, d["start"]) {
		if n.kind == "Picker" {
			p = append(p, n)
		}
	}
	if len(p) != 2 {
		t.Fatalf("not two pickers:\n%s", d["start"])
	}
	if p[1].h < p[0].h+10 || p[1].w <= p[0].w {
		t.Errorf("the 30-point picker (%vx%v) isn't larger than the theme's (%vx%v):\n%s", p[1].w, p[1].h, p[0].w, p[0].h, d["start"])
	}
}

// A WebView (testdata/web.asuka): WebKitGTK's page posting to the app,
// loaded, and a Video that fails.
func TestWebView(t *testing.T) {
	onLinux(t)
	dir := t.TempDir()
	exe := filepath.Join(dir, "app")
	buildApp(t, testdata(t, "web.asuka"), exe)
	d := script(t, dir, exe, "wait:6s", "snap:web")
	shows(t, d["web"], "the page's", []string{"Got: hi from the page", "Loaded."}, "Nothing yet.")
}

// Keys (testdata/keys.asuka): a view's Keys, the main menu's shortcuts.
func TestKeys(t *testing.T) {
	onLinux(t)
	dir := t.TempDir()
	exe := filepath.Join(dir, "app")
	buildApp(t, testdata(t, "keys.asuka"), exe)
	d := script(t, dir, exe, "snap:start", "key:ctrl+k", "snap:after")
	t.Logf("%s", d["after"])
	if d["start"] == d["after"] {
		t.Errorf("ctrl+k changed nothing:\n%s", d["after"])
	}
}

// fakes starts test/fakes.c's notification server and
// StatusNotifierWatcher on the session bus, and returns its log's path.
func fakes(t *testing.T) string {
	t.Helper()
	dir := t.TempDir()
	exe := filepath.Join(dir, "fakes")
	src, err := filepath.Abs("src")
	if err != nil {
		t.Fatal(err)
	}
	cc := os.Getenv("CC")
	if cc == "" {
		cc = "cc"
	}
	desk, err := filepath.Abs(filepath.Join("..", "desktop"))
	if err != nil {
		t.Fatal(err)
	}
	core, err := filepath.Abs(filepath.Join("..", "..", "core", "include"))
	if err != nil {
		t.Fatal(err)
	}
	cmd := exec.Command(cc, "-std=c17", "-D_GNU_SOURCE", "-I"+src, "-I"+filepath.Join(desk, "include"), "-I"+core,
		filepath.Join(src, "syms.c"), filepath.Join(desk, "src", "linux_syms.c"), filepath.Join("test", "fakes.c"), "-ldl", "-lpthread", "-o", exe)
	if out, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("building the fakes: %v\n%s", err, out)
	}
	log := filepath.Join(dir, "log")
	f := exec.Command(exe, log)
	var out bytes.Buffer
	f.Stdout, f.Stderr = &out, &out
	if err := f.Start(); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { f.Process.Kill(); f.Wait() })
	for deadline := time.Now().Add(20 * time.Second); ; {
		if b, _ := os.ReadFile(log); bytes.HasPrefix(b, []byte("ready")) {
			return log
		}
		if time.Now().After(deadline) {
			t.Fatalf("the fakes didn't start:\n%s", out.String())
		}
		time.Sleep(50 * time.Millisecond)
	}
}

func logLines(t *testing.T, path string) []string {
	t.Helper()
	f, err := os.Open(path)
	if err != nil {
		t.Fatal(err)
	}
	defer f.Close()
	var out []string
	sc := bufio.NewScanner(f)
	for sc.Scan() {
		out = append(out, sc.Text())
	}
	return out
}

// The host's modules (testdata/modules.asuka): platform/host's call, a
// module the host hasn't, platform/apps' ranking, a tray icon registered
// with the watcher, and the updater: info, a download over HTTP with its
// progress, and an install that swaps the app's own prefix while it runs.
func TestModules(t *testing.T) {
	onLinux(t)
	log := fakes(t)
	// What the update downloads: 300 KB, its length known.
	body := bytes.Repeat([]byte("asuka "), 50000)
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	srv := &http.Server{Handler: http.HandlerFunc(func(rw http.ResponseWriter, r *http.Request) {
		if r.Header.Get("User-Agent") != "asuka-updater" {
			http.Error(rw, "User-Agent "+r.Header.Get("User-Agent"), http.StatusBadRequest)
			return
		}
		http.ServeContent(rw, r, "new.txt", time.Time{}, bytes.NewReader(body))
	})}
	go srv.Serve(ln)
	defer srv.Close()
	url := fmt.Sprintf("http://%s/new.txt", ln.Addr())
	// The installed prefix: bin/app, share/app/old.txt (both listed) and
	// share/keep.txt (no list names it: the user's). The update, staged in
	// the work directory (share/app; not .asuka-update-*, which the host
	// removes as it starts): bin/app and share/app/new.txt (downloaded).
	dir := t.TempDir()
	exe := filepath.Join(dir, "bin", "app")
	buildApp(t, strings.ReplaceAll(testdata(t, "modules.asuka"), "MODULES_URL", url), exe)
	write := func(name, s string) {
		p := filepath.Join(dir, filepath.FromSlash(name))
		if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(p, []byte(s), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	write("share/app/.asuka-files", "bin/app\nshare/app/old.txt\n")
	write("share/app/old.txt", "old")
	write("share/keep.txt", "keep")
	staged := filepath.Join(dir, "share", "app", "staged-test")
	if err := os.MkdirAll(filepath.Join(staged, "share", "app"), 0o755); err != nil {
		t.Fatal(err)
	}
	b, err := os.ReadFile(exe)
	if err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(filepath.Join(staged, "bin"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(staged, "bin", "app"), b, 0o755); err != nil {
		t.Fatal(err)
	}
	d := script(t, dir, exe, "wait:6s", "snap:done")
	dump := d["done"]
	t.Logf("%s", dump)
	shows(t, dump, "the modules", []string{
		"supported: 1",
		"host: platform/push isn't supported on Linux",
		"missing: unsupported: nope.x isn't on Linux",
		"ranked: Visual Studio Code|Visual Studio Code,Xcode",
		"info: false notConfigured prefix linux-" + runtime.GOARCH,
		"download: 200 300000/300000 [] polls>1:true",
		"install: installed",
	})
	got, err := os.ReadFile(filepath.Join(dir, "share", "app", "new.txt"))
	if err != nil || !bytes.Equal(got, body) {
		t.Errorf("new.txt isn't the download (%d bytes, %v)", len(got), err)
	}
	if _, err := os.Stat(filepath.Join(dir, "share", "app", "old.txt")); !os.IsNotExist(err) {
		t.Errorf("old.txt, which the update no longer has, stayed: %v", err)
	}
	if _, err := os.Stat(filepath.Join(dir, "share", "keep.txt")); err != nil {
		t.Errorf("keep.txt, which no list names, went: %v", err)
	}
	if list, _ := os.ReadFile(filepath.Join(dir, "share", "app", ".asuka-files")); string(list) != "bin/app\nshare/app/new.txt\n" {
		t.Errorf(".asuka-files = %q", list)
	}
	if _, err := os.Stat(staged); !os.IsNotExist(err) {
		t.Errorf("the staged update stayed: %v", err)
	}
	registered := false
	for _, l := range logLines(t, log) {
		registered = registered || strings.HasPrefix(l, "register :")
	}
	if !registered {
		t.Errorf("the tray icon didn't register with the watcher: %q", logLines(t, log))
	}
}

// More of the modules (testdata/services.asuka): a notification shown by
// the session's notification server and listed, a global hot key (an
// X11 grab under Xvfb), recent documents, start at login, the launcher's
// progress, badge and attention, what Linux hasn't.
func TestServices(t *testing.T) {
	onLinux(t)
	log := fakes(t)
	dir := t.TempDir()
	doc := filepath.Join(dir, "doc.txt")
	if err := os.WriteFile(doc, []byte("doc"), 0o644); err != nil {
		t.Fatal(err)
	}
	exe := filepath.Join(dir, "app")
	buildApp(t, strings.ReplaceAll(testdata(t, "services.asuka"), "SERVICES_FILE", doc), exe)
	d := script(t, dir, exe, "wait:5s", "snap:done")
	dump := d["done"]
	t.Logf("%s", dump)
	shows(t, dump, "the services", []string{
		"toast: enabled:1 shown:1 Hello after:0",
		"hotkey: registered",
		"recent: true []",
		"login: 100",
		"dock: attention 4",
		"contacts: 1 not allowed",
		"widget: not available: widgets are iOS's and Android's",
	})
	notified := false
	for _, l := range logLines(t, log) {
		notified = notified || strings.Contains(l, "notify ") && strings.Contains(l, "Hello From the test")
	}
	if !notified {
		t.Errorf("the notification server wasn't asked: %q", logLines(t, log))
	}
	if _, err := os.Stat(filepath.Join(dir, "home", "config", "autostart")); err != nil {
		t.Errorf("login: no autostart folder: %v", err)
	}
}

// What the host draws itself (testdata/draw.asuka): images from the app's
// assets, a Shape, a Canvas, a Shader, rounded, turned and faded views.
func TestDrawing(t *testing.T) {
	onLinux(t)
	dir := t.TempDir()
	exe := filepath.Join(dir, "app")
	buildApp(t, testdata(t, "draw.asuka"), exe)
	writeAssets(t, dir)
	d := script(t, dir, exe, "wait:1500ms", "snap:drawn")
	t.Logf("%s", d["drawn"])
	shows(t, d["drawn"], "drawn", []string{"Loaded 64x48, failed"})
	for _, kind := range []string{" Shape [", " Canvas [", " Shader [", " Image ["} {
		if !strings.Contains(d["drawn"], kind) {
			t.Errorf("no%s in:\n%s", strings.TrimSuffix(kind, " ["), d["drawn"])
		}
	}
}

// A window reopens where it was left (docs/navigation.md, "Routes"): the
// app navigates to a message and quits; launched again, its router
// starts at the message (asuka_set_start, from asuka-routes.json), with
// home under it. Automation keeps no routes unless ASUKA_ROUTES=1 says
// so: without it, the app starts at its initial route.
func TestRouteReopens(t *testing.T) {
	onLinux(t)
	dir := t.TempDir()
	exe := filepath.Join(dir, "app")
	buildApp(t, testdata(t, "routes.asuka"), exe)
	data := filepath.Join(dir, "data")
	keep := []string{"ASUKA_DATA=" + data, "ASUKA_ROUTES=1"}
	first := scriptEnv(t, dir, exe, keep, "wait:1s", "tap:Open 7", "wait:1s", "snap:first")
	shows(t, first["first"], "the first run", []string{"Showing message 7"}, "The home screen")
	if b, _ := os.ReadFile(filepath.Join(data, "asuka-routes.json")); !strings.Contains(string(b), "/home/7") {
		t.Fatalf("asuka-routes.json:\n%s", b)
	}
	// Automation alone keeps nothing: the initial route.
	fresh := scriptEnv(t, dir, exe, []string{"ASUKA_DATA=" + data}, "wait:1s", "snap:fresh")
	shows(t, fresh["fresh"], "an automated run", []string{"The home screen"}, "Showing message 7")
	again := scriptEnv(t, dir, exe, keep, "wait:1s", "snap:again", "back", "wait:1s", "snap:back")
	shows(t, again["again"], "launched again", []string{"Showing message 7"}, "The home screen")
	shows(t, again["back"], "back", []string{"The home screen"}, "Showing message 7")
}

// Numbers in a locale with a decimal comma (en_DK's, installed where
// Ubuntu's English is): the app's still read and print "2.5", as
// everywhere, while GTK speaks the user's language.
func TestNumbersInLocale(t *testing.T) {
	onLinux(t)
	if out, err := exec.Command("locale", "-a").Output(); err != nil || !strings.Contains(string(out), "en_DK.utf8") {
		t.Skip("no en_DK.utf8 locale (locale -a)")
	}
	dir := t.TempDir()
	exe := filepath.Join(dir, "app")
	buildApp(t, `
component Numbers {
  Column {
    Text("read {"2.5".toFloat() ?? -1}")
    Text("fixed {2.5.fixed(2)}")
    Text("sum {0.1 + 0.2}")
  }
}
app { Numbers() }
`, exe)
	d := scriptEnv(t, dir, exe, []string{"LC_ALL=en_DK.utf8", "LANG=en_DK.utf8"})
	shows(t, d["end"], "in en_DK", []string{"read 2.5", "fixed 2.50", "sum 0.30000000000000004"})
}
