package main

import (
	"bufio"
	"bytes"
	"compress/gzip"
	"crypto/ecdh"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"html"
	"image"
	"image/png"
	"io"
	"io/fs"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"runtime"
	"slices"
	"sort"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/egoist/asuka/internal/appbuild"
	"github.com/egoist/asuka/internal/cbuild"
	"github.com/egoist/asuka/internal/devserver"
	"github.com/egoist/asuka/internal/nativegen"
	"github.com/egoist/asuka/internal/qrcode"
	"github.com/egoist/asuka/lang/bytecode"
	"github.com/egoist/asuka/lang/target"
	webhost "github.com/egoist/asuka/platform/web"
)

// The web: the C core as WebAssembly (asuka.wasm, built by Zig) and the
// web host (platform/web: the DOM, in JavaScript), which the page loads
// together.
//
// asuka dev --platform web serves the app on this machine and opens it in
// the browser: the page runs the player (the core without an app) and
// loads the app's bytecode from asuka dev, and again on each save,
// keeping its state and views (asuka_dev_reload). The app's output and
// the page's errors are printed here. asuka build --platform web makes a
// static site: the core with the app compiled in, its files named by
// their content's digest (cache them for good; index.html no-cache),
// gzipped copies, a web app manifest and its icons, and the app's service
// worker (webWorker), which shows its notifications and pushes while no
// page is open.

const devWebPort = 8790

// webSource is the web host's files: the checkout's, when the CLI runs
// from one (so changing them reloads the page too), else the ones built
// into the CLI.
func webSource() (fs.FS, string) {
	if root := appbuild.Checkout(); root != "" {
		dir := filepath.Join(root, "platform", "web", "src")
		if _, err := os.Stat(filepath.Join(dir, "host.js")); err == nil {
			return os.DirFS(dir), dir
		}
	}
	sub, _ := fs.Sub(webhost.Src, "src")
	return sub, ""
}

// webSQLitePath is where a web build carries std/sqlite's module (its
// SQLite worker's, appbuild.SQLiteWasm) in the host's directory, as the
// page's sqlite option names it.
const webSQLitePath = "lib/asqlite.wasm"

// webSQLite is the web's SQLite module when c (the app's) imports
// std/sqlite, or "" (debug: asuka dev's).
func webSQLite(c *bytecode.C, debug bool) (string, error) {
	if !appbuild.UsesSQLite(c) {
		return "", nil
	}
	return appbuild.SQLiteWasm(debug)
}

// fontFiles are the app's font files (assets/fonts), which a FontFamily
// names by their names without the extension.
func fontFiles(app *asukaApp) []string {
	var out []string
	es, _ := os.ReadDir(filepath.Join(app.dir, "assets", "fonts"))
	for _, e := range es {
		switch strings.ToLower(path.Ext(e.Name())) {
		case ".ttf", ".otf", ".woff", ".woff2":
			out = append(out, e.Name())
		}
	}
	return out
}

type webPage struct {
	Name, Lang     string
	Base           string // the path the site is served at: its <base href>
	Host, Core     string // the URLs of the host's directory, the core's wasm
	Options        map[string]any
	Launch         [2]string // the splash's colors, light and dark: the page's until the first frame
	Icon, Manifest bool
	// Scripts are its native packages' generated JavaScript, loaded
	// before it starts: they register the packages' modules and views.
	Scripts []string
	// Colors is asuka.toml's [web.colors]: a style sheet after the
	// host's, and the browser's theme color from its background.
	Colors webColors
}

// launchColors are the splash's colors, light and dark (the light one
// for both without a dark one), when asuka.toml sets its background;
// none otherwise, so the page keeps the host's own while it loads.
func launchColors(c *appConfig) [2]string {
	if c == nil || !c.Splash.colored {
		return [2]string{}
	}
	return [2]string{c.Splash.Background, c.Splash.DarkBackground}
}

// launchStyle paints the page in the launch colors until the app's first
// frame (the host marks its container asuka-drawn then), so a page that
// is loading shows the app's color, not the host's background: "" for
// none. The colors are asuka.toml's, checked as #rrggbb.
func launchStyle(launch [2]string) string {
	if launch[0] == "" {
		return ""
	}
	const sel = "html:not(:has(.asuka-drawn)), .asuka-page:not(.asuka-drawn)"
	return fmt.Sprintf("<style>\n%s { background: %s; }\n@media (prefers-color-scheme: dark) { %s { background: %s; } }\n</style>\n", sel, launch[0], sel, launch[1])
}

// siteBase is a site's base path, with its slashes: "/", "/app/".
func siteBase(b string) string {
	b = strings.Trim(b, "/")
	if b == "" {
		return "/"
	}
	return "/" + b + "/"
}

// webIndex is the page that runs the app: its <base href> is the site's
// base, so it finds its files from any route's path (a Router's
// addresses, which the server answers with this page).
func webIndex(p webPage) string {
	opts := map[string]any{"core": p.Core, "assets": "assets"}
	for k, v := range p.Options {
		opts[k] = v
	}
	o, _ := json.Marshal(opts)
	// The browser's theme color: the page's background, light and dark.
	themeLight, themeDark := "#ffffff", "#121212" // the host's
	if bg := p.Colors.Background; bg != "" {
		themeLight, themeDark = cssHex(bg), cssHex(bg)
	}
	if bg := p.Colors.BackgroundDark; bg != "" {
		themeDark = cssHex(bg)
	}
	var b strings.Builder
	fmt.Fprintf(&b, `<!doctype html>
<html lang="%s">
<head>
<meta charset="utf-8">
<base href="%[3]s">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover, interactive-widget=resizes-content">
<meta name="color-scheme" content="light dark">
<meta name="theme-color" content="%[4]s" media="(prefers-color-scheme: light)">
<meta name="theme-color" content="%[5]s" media="(prefers-color-scheme: dark)">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="%[2]s">
<title>%[2]s</title>
`, html.EscapeString(p.Lang), html.EscapeString(p.Name), html.EscapeString(siteBase(p.Base)), themeLight, themeDark)
	if p.Manifest {
		b.WriteString(`<link rel="manifest" href="manifest.webmanifest">` + "\n")
	}
	if p.Icon {
		b.WriteString(`<link rel="icon" type="image/png" sizes="32x32" href="icons/favicon.png">
<link rel="apple-touch-icon" href="icons/apple-touch-icon.png">
`)
	}
	b.WriteString(launchStyle(p.Launch))
	fmt.Fprintf(&b, `<link rel="stylesheet" href="%[1]s/style.css">
<link rel="modulepreload" href="%[1]s/asuka.js">
<link rel="preload" href="%[2]s" as="fetch" type="application/wasm" crossorigin>
%[5]s</head>
<body>
<script type="module">
import * as asuka from "./%[1]s/asuka.js";
globalThis.asuka = asuka;
%[4]sasuka.start(%[3]s);
</script>
</body>
</html>
`, p.Host, p.Core, o, imports(p.Scripts), style(p.Colors.css()))
	return b.String()
}

// style is css in a <style> element ("" for none).
func style(css string) string {
	if css == "" {
		return ""
	}
	return "<style>\n" + css + "</style>\n"
}

// webScript is a native package's file among the host's
// (packages/<package>/<file>): its path there, its file here, and whether
// the page imports it (its generated JavaScript, which imports the rest).
type webScript struct {
	path, file string
	entry      bool
}

// nativeScripts are the files of the native packages' web/.
func nativeScripts(natives []nativePackage) ([]webScript, error) {
	var out []webScript
	for _, p := range natives {
		dir := filepath.Join(p.dir, "web")
		entry := nativegen.JSFile(&nativegen.Package{Name: p.name})
		err := filepath.WalkDir(dir, func(f string, d fs.DirEntry, err error) error {
			if err != nil || d.IsDir() {
				return err
			}
			if strings.HasSuffix(f, ".test.js") {
				return nil // the package's own tests (node --test), not the app's
			}
			r, _ := filepath.Rel(dir, f)
			r = filepath.ToSlash(r)
			out = append(out, webScript{path: "packages/" + nativegen.Sanitize(p.path) + "/" + r, file: f, entry: r == entry})
			return nil
		})
		if err != nil {
			return nil, err
		}
	}
	return out, nil
}

// scriptPaths are the paths, among the host's files, of the scripts the
// page imports.
func scriptPaths(scripts []webScript) []string {
	var out []string
	for _, sc := range scripts {
		if sc.entry {
			out = append(out, sc.path)
		}
	}
	return out
}

// imports are the page's imports of the app's scripts.
func imports(scripts []string) string {
	var b strings.Builder
	for _, s := range scripts {
		q, _ := json.Marshal("./" + s)
		fmt.Fprintf(&b, "await import(%s);\n", q)
	}
	return b.String()
}

// webWorker is the app's service worker: its settings (globalThis.asukaApp:
// name, title, assets, icon), then the host's notify-core.js and sw.js,
// as one classic script (platform/web/src/sw.js says what it does).
func webWorker(src fs.FS, settings map[string]any) ([]byte, error) {
	var b bytes.Buffer
	st, _ := json.Marshal(settings)
	b.WriteString("// The app's service worker: Asuka's (platform/web/src/sw.js), with the app's settings.\n")
	fmt.Fprintf(&b, "globalThis.asukaApp = %s;\n", st)
	for _, n := range []string{"notify-core.js", "sw.js"} {
		f, err := fs.ReadFile(src, n)
		if err != nil {
			return nil, fmt.Errorf("the web host's %s: %w", n, err)
		}
		b.Write(f)
		b.WriteString("\n")
	}
	return b.Bytes(), nil
}

// webVAPIDKey is asuka.toml's web.vapidPublicKey, checked: an uncompressed
// P-256 point in base64url (unpadded), as PushManager.subscribe takes it.
func webVAPIDKey(app *asukaApp) (string, error) {
	if app.cfg == nil || strings.TrimSpace(app.cfg.Web.VapidPublicKey) == "" {
		return "", nil
	}
	k := strings.TrimRight(strings.TrimSpace(app.cfg.Web.VapidPublicKey), "=")
	k = strings.NewReplacer("+", "-", "/", "_").Replace(k)
	b, err := base64.RawURLEncoding.DecodeString(k)
	if err == nil {
		_, err = ecdh.P256().NewPublicKey(b)
	}
	if err != nil || len(b) != 65 {
		return "", errors.New("asuka.toml: web.vapidPublicKey isn't a VAPID public key (an uncompressed P-256 point, 65 bytes, in base64url): github.com/egoist/asuka/webpush's GenerateKeys makes a pair")
	}
	return k, nil
}

func devWeb(dir string, o *devOptions, stop <-chan os.Signal) error {
	app, err := loadApp(dir, platformWeb)
	if err != nil {
		return err
	}
	work, err := workDir(app, "web")
	if err != nil {
		return err
	}
	start := time.Now()
	s, loop, err := newWebDev(app, work)
	if err != nil {
		return err
	}
	_, srcDir := webSource()
	s.stamp = strconv.FormatInt(time.Now().UnixNano(), 36)
	host := "127.0.0.1"
	if o.lan {
		host = ""
	}
	port := o.port
	if port == 0 {
		port = devWebPort
	}
	ln, err := net.Listen("tcp", net.JoinHostPort(host, strconv.Itoa(port)))
	if err != nil && o.port == 0 {
		ln, err = net.Listen("tcp", net.JoinHostPort(host, "0")) // taken: any free one
	}
	if err != nil {
		return err
	}
	go http.Serve(ln, s)
	url := fmt.Sprintf("http://localhost:%d/", ln.Addr().(*net.TCPAddr).Port)
	s.url = url
	fmt.Printf("asuka: %s at %s (the player built in %.1fs)\n", app.name, url, time.Since(start).Seconds())
	if o.lan {
		addrs := lanAddrs()
		for _, a := range addrs {
			fmt.Printf("asuka: on this network: http://%s:%d/\n", a, ln.Addr().(*net.TCPAddr).Port)
		}
		// The first one as a QR code: a phone's camera opens it.
		if len(addrs) > 0 {
			if c, err := qrcode.Encode(fmt.Sprintf("http://%s:%d/", addrs[0], ln.Addr().(*net.TCPAddr).Port), qrcode.Low); err == nil {
				fmt.Print(c.Terminal())
			}
		}
	}
	hostWatch := newWebWatcher(srcDir)
	var nativeWatch []*webWatcher // the native packages' JavaScript, wherever it is
	natives, _ := nativeFor(app, "web")
	for _, d := range nativeDirsOf(natives, "web") {
		nativeWatch = append(nativeWatch, newWebWatcher(d))
	}
	opened := false
	var stopChrome func()
	launch := func() error {
		if opened {
			// The app's settings, assets, scripts or C changed: the page
			// loads them again, and a player built again.
			if err := s.buildPlayer(); err != nil {
				return err
			}
			if err := s.findNatives(); err != nil {
				return err
			}
			s.stamp = strconv.FormatInt(time.Now().UnixNano(), 36)
			s.send("reload", "")
			return nil
		}
		opened = true
		if len(o.flows) > 0 {
			// asuka test --gui: a headless Chrome of its own.
			quit, err := headlessChrome(url)
			if err != nil {
				return err
			}
			stopChrome = quit
			return nil
		}
		if !o.noOpen {
			openBrowser(url)
		}
		return nil
	}
	// The host's own JavaScript (a checkout of Asuka) and the packages':
	// the page loads them again.
	go func() {
		w := newWatcher(append([]string{srcDir}, nativeDirsOf(natives, "web")...)...)
		for range w.wake {
			if hostWatch.changed() || slices.ContainsFunc(nativeWatch, (*webWatcher).changed) {
				s.stamp = strconv.FormatInt(time.Now().UnixNano(), 36)
				s.send("reload", "")
			}
		}
	}()
	loop.flows = o.flows
	return loop.run(devPlayer{launch: launch, stop: func() {
		if stopChrome != nil {
			stopChrome()
		}
	}}, stop, o.once)
}

// newWebDev makes asuka dev's web server for app: the player (the core
// without an app) built in work, and the loop that compiles the app and
// tells the pages of each new module (which they load in place), of what
// didn't compile and of when it compiles again.
func newWebDev(app *asukaApp, work string) (*devServer, *devLoop, error) {
	vapid, err := webVAPIDKey(app)
	if err != nil {
		return nil, nil, err
	}
	src, _ := webSource()
	s := &devServer{app: app, src: src, wasm: filepath.Join(work, "asuka.wasm"), vapid: vapid, clients: map[chan string]bool{}, pages: map[string]*webPerfPage{}}
	// The loop compiles the app first: the player has the C its externs
	// call.
	loop, err := newDevLoop(app, "127.0.0.1:0")
	if err != nil {
		return nil, nil, err
	}
	if err := s.buildPlayer(); err != nil {
		loop.close()
		return nil, nil, err
	}
	if err := s.findNatives(); err != nil {
		loop.close()
		return nil, nil, err
	}
	s.loop = loop
	loop.webLoop = true
	s.stamp = strconv.FormatInt(time.Now().UnixNano(), 36)
	s.moduleStamp = s.stamp
	loop.mu.Lock()
	loop.modules = append(loop.modules, func([]byte) {
		s.mu.Lock()
		s.moduleStamp = strconv.FormatInt(time.Now().UnixNano(), 36)
		stamp := s.moduleStamp
		s.mu.Unlock()
		s.send("module", stamp)
	})
	// The automation's commands go to the page by its events; it answers
	// at /__asuka/automated.
	loop.automation.send = func(seq uint32, commands string) bool {
		s.mu.Lock()
		n := len(s.clients)
		s.mu.Unlock()
		if n == 0 {
			return false
		}
		s.send("automate", strconv.FormatUint(uint64(seq), 10)+"\n"+commands)
		return true
	}
	// The inspector asks the pages for their views by their events; each
	// answers at /__asuka/tree.
	loop.inspector.askers = append(loop.inspector.askers, func() int {
		s.mu.Lock()
		n := len(s.clients)
		s.mu.Unlock()
		s.send("inspect", "")
		return n
	})
	// The performance tools reach the pages by their events too ("<page>
	// ...", * for every page); a page's recording comes back at
	// /__asuka/profile.
	loop.perfAll = append(loop.perfAll, perfAll{
		overlay: func(on bool) {
			s.mu.Lock()
			s.perf = on
			for _, p := range s.pages {
				p.overlay = on
			}
			s.mu.Unlock()
			s.send("perf", "* "+boolDigit(on))
		},
		start: func() {
			s.mu.Lock()
			for _, p := range s.pages {
				p.recording = true
			}
			s.mu.Unlock()
			s.send("profile", "* start 0")
		},
		stop: func() []string {
			s.mu.Lock()
			var keys []string
			for id, p := range s.pages {
				if p.recording {
					p.recording = false
					keys = append(keys, "web:"+id)
				}
			}
			s.mu.Unlock()
			s.send("profile", "* stop")
			return keys
		},
	})
	loop.restarts = append(loop.restarts, func() {
		s.mu.Lock()
		stamp := s.moduleStamp
		s.mu.Unlock()
		s.send("restart", stamp)
	})
	// What didn't compile: a failed frame's payload (core/src/abi/dev.h),
	// which the page gives the core (asuka_dev_frame).
	loop.failed = append(loop.failed, func(f devserver.Failure) {
		data := base64.StdEncoding.EncodeToString(f.Encode())
		s.mu.Lock()
		s.failed = data
		s.mu.Unlock()
		s.send("failed", data)
	})
	loop.fixed = append(loop.fixed, func() {
		s.mu.Lock()
		s.failed = ""
		s.mu.Unlock()
		s.send("fixed", "")
	})
	loop.mu.Unlock()
	return s, loop, nil
}

// buildPlayer builds the page's player: the core without an app, with
// the C the app's externs call as of the last compile.
func (s *devServer) buildPlayer() error {
	return appbuild.PlayerLib(cbuild.Target{OS: "wasm", Arch: "wasm32"}, cbuild.Module, s.app.c, s.wasm)
}

// devServer serves the app's page under asuka dev: the page, the host's
// files, the build, the app's assets, and its connection back (the
// reloads it waits for, the logs it sends).
type devServer struct {
	app     *asukaApp
	loop    *devLoop
	src     fs.FS
	sqlite  atomic.Pointer[string]      // webSQLite, as of the last build ("": none)
	scripts atomic.Pointer[[]webScript] // the native packages' scripts (nativeScripts), likewise
	wasm    string                      // the player: the core without an app
	vapid   string                      // asuka.toml's web.vapidPublicKey
	url     string
	stamp   string // the page's files', so the page never loads old ones
	// moduleStamp names the app's last module, so the page never loads an
	// old one.
	moduleStamp string
	failed      string // the build that failed, as its event's data ("": none)
	mu          sync.Mutex
	clients     map[chan string]bool
	// pages are the pages connected, by their id, with what the
	// performance tools last asked of each; perf: the overlay's on for
	// every page (the terminal's p).
	pages   map[string]*webPerfPage
	perf    bool
	nextID  int64
	waiting map[int64]chan struct {
		ID         int64
		Out, Error string
	}

	// The app's icon as icon last read it, and its file's path and time
	// (under mu).
	iconPath string
	iconTime time.Time
	iconImg  *image.NRGBA
}

// automate sends the page the debug automation's commands (the request's
// body, a line each: tap, type, submit, back, tab, dump, reload), as the
// GUI tests send them to the mobile hosts, and answers with what the page
// does: dump's lines, or the page's error (500). Bounded: a page that
// doesn't answer fails the request.
func (s *devServer) automate(w http.ResponseWriter, r *http.Request) {
	b, _ := io.ReadAll(r.Body)
	c := make(chan struct {
		ID         int64
		Out, Error string
	}, 1)
	s.mu.Lock()
	s.nextID++
	id := s.nextID
	if s.waiting == nil {
		s.waiting = map[int64]chan struct {
			ID         int64
			Out, Error string
		}{}
	}
	s.waiting[id] = c
	s.mu.Unlock()
	msg, _ := json.Marshal(map[string]any{"id": id, "commands": string(b)})
	s.send("automation", string(msg))
	select {
	case d := <-c:
		if d.Error != "" {
			http.Error(w, d.Error, http.StatusInternalServerError)
			return
		}
		io.WriteString(w, d.Out)
	case <-time.After(10 * time.Second):
		s.mu.Lock()
		delete(s.waiting, id)
		s.mu.Unlock()
		http.Error(w, "the page didn't answer", http.StatusGatewayTimeout)
	}
}

func (s *devServer) send(event, data string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	msg := "event: " + event + "\n"
	for _, line := range strings.Split(data, "\n") {
		msg += "data: " + line + "\n"
	}
	msg += "\n"
	for c := range s.clients {
		select {
		case c <- msg:
		default:
		}
	}
}

func (s *devServer) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	p := r.URL.Path
	w.Header().Set("Cache-Control", "no-store")
	switch {
	case p == "/" || p == "/index.html":
		s.index(w)
	case p == "/asuka.wasm":
		w.Header().Set("Content-Type", "application/wasm")
		http.ServeFile(w, r, s.wasm)
	case p == "/__asuka/app.akb":
		m := s.loop.webModule()
		if m == nil {
			http.Error(w, "the app hasn't compiled", http.StatusServiceUnavailable)
			return
		}
		w.Header().Set("Content-Type", "application/octet-stream")
		w.Write(m)
	case p == "/__asuka/reloaded" && r.Method == http.MethodPost:
		var rep struct {
			First, Restarted bool
			Kept             uint32
			Micros           uint32
		}
		if json.NewDecoder(r.Body).Decode(&rep) == nil {
			s.loop.report(nil, rep.First, rep.Restarted, rep.Kept, rep.Micros)
		}
	case p == "/sw.js":
		b, err := webWorker(s.src, map[string]any{"name": s.app.cfg.ID, "title": s.app.displayName(), "assets": "assets"})
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		w.Header().Set("Content-Type", contentType(p))
		w.Write(b)
	case strings.HasPrefix(p, "/asuka/packages/"):
		i := slices.IndexFunc(*s.scripts.Load(), func(sc webScript) bool { return "/asuka/"+sc.path == p })
		if i < 0 {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", contentType(p))
		http.ServeFile(w, r, (*s.scripts.Load())[i].file)
	case p == "/asuka/"+webSQLitePath:
		lib := *s.sqlite.Load()
		if lib == "" {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", contentType(p))
		http.ServeFile(w, r, lib)
	case strings.HasPrefix(p, "/asuka/"):
		b, err := fs.ReadFile(s.src, strings.TrimPrefix(p, "/asuka/"))
		if err != nil {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", contentType(p))
		w.Write(b)
	case strings.HasPrefix(p, "/web/") && path.Ext(p) == ".js":
		w.Header().Set("Content-Type", contentType(p))
		http.ServeFile(w, r, filepath.Join(s.app.dir, "web", path.Base(p)))
	case p == "/icons/favicon.png" || p == "/icons/apple-touch-icon.png":
		s.icon(w, r, p)
	case strings.HasPrefix(p, "/assets/"):
		http.ServeFile(w, r, filepath.Join(s.app.dir, filepath.FromSlash(strings.TrimPrefix(path.Clean(p), "/"))))
	case p == "/__asuka/events":
		s.events(w, r)
	case p == "/__asuka/automation" && r.Method == http.MethodPost:
		s.automate(w, r)
	case p == "/__asuka/automation/done" && r.Method == http.MethodPost:
		var d struct {
			ID         int64
			Out, Error string
		}
		if json.NewDecoder(r.Body).Decode(&d) == nil {
			s.mu.Lock()
			c := s.waiting[d.ID]
			delete(s.waiting, d.ID)
			s.mu.Unlock()
			if c != nil {
				c <- d
			}
		}
	case p == "/__asuka/automated" && r.Method == http.MethodPost:
		// The page's core's answers to the automation's commands.
		b, _ := io.ReadAll(io.LimitReader(r.Body, 16<<20))
		s.loop.automation.answeredFrame(b)
	case p == "/__asuka/tree" && r.Method == http.MethodPost:
		// A page's views (its core's AK_DEV_TREE), for the inspector: page
		// is the page's own id, name its browser.
		b, _ := io.ReadAll(io.LimitReader(r.Body, 64<<20))
		name := "Web"
		if n := r.URL.Query().Get("name"); n != "" {
			name += " (" + n + ")"
		}
		page := r.URL.Query().Get("page")
		s.loop.inspector.put("web:"+page, name, b, true, func(path string) {
			s.send("highlight", page+" "+path)
		}, s.pagePerf(page))
	case p == "/__asuka/profile" && r.Method == http.MethodPost:
		// A page's recording (its core's AK_DEV_PROFILE).
		b, _ := io.ReadAll(io.LimitReader(r.Body, 256<<20))
		name := "Web"
		if n := r.URL.Query().Get("name"); n != "" {
			name += " (" + n + ")"
		}
		s.loop.gotProfile("web:"+r.URL.Query().Get("page"), name, b)
	case p == "/__asuka/uncaught" && r.Method == http.MethodPost:
		// An error the app didn't catch (an uncaught frame's payload): it's
		// printed, and the answer is its source frame's payload.
		b, _ := io.ReadAll(r.Body)
		u, ok := devserver.DecodeUncaught(b)
		if !ok {
			http.Error(w, "a bad report", http.StatusBadRequest)
			return
		}
		source, details := s.loop.uncaught(u)
		w.Header().Set("Content-Type", "application/octet-stream")
		w.Write(devserver.EncodeSource(u.Seq, source, details))
	case p == "/__asuka/log" && r.Method == http.MethodPost:
		b, _ := io.ReadAll(r.Body)
		printLog(b)
	case r.Method == http.MethodGet && strings.Contains(r.Header.Get("Accept"), "text/html"):
		// A page's address no file has: a Router's route, which the app
		// shows (the browser's reload, a link pasted).
		s.index(w)
	default:
		http.NotFound(w, r)
	}
}

// icon serves the app's icon as the page's favicon (32) or iOS's home
// screen icon (180), made as a build makes them (webIcons): none without
// an icon in asuka.toml. The icon is read again only when its file
// changes, so loadIcon's warnings show once, not on each page load.
func (s *devServer) icon(w http.ResponseWriter, r *http.Request, p string) {
	path := s.app.cfg.Icon
	if path == "" {
		http.NotFound(w, r)
		return
	}
	fi, err := os.Stat(path)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	s.mu.Lock()
	img := s.iconImg
	if s.iconPath != path || !s.iconTime.Equal(fi.ModTime()) {
		if img, err = loadIcon(path); err == nil {
			s.iconPath, s.iconTime, s.iconImg = path, fi.ModTime(), img
		}
	}
	s.mu.Unlock()
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	px := 32
	if p == "/icons/apple-touch-icon.png" {
		px = 180
	}
	w.Header().Set("Content-Type", "image/png")
	png.Encode(w, resize(img, px))
}

// index serves the page that runs the app (at / and at any route's
// path: a Router's addresses).
func (s *devServer) index(w http.ResponseWriter) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	s.mu.Lock()
	module := "/__asuka/app.akb?" + s.moduleStamp
	s.mu.Unlock()
	opts := map[string]any{"name": s.app.cfg.ID, "title": s.app.displayName(), "dev": "/__asuka/", "module": module,
		"fonts": fontFiles(s.app), "version": s.app.cfg.Version, "build": s.app.cfg.Build, "serviceWorker": "sw.js"}
	if icons, err := webAlternateIcons(s.app.cfg); err == nil && icons != nil {
		opts["icons"] = icons
	}
	if s.vapid != "" {
		opts["vapidPublicKey"] = s.vapid
	}
	if st := s.app.cfg.analyticsSettings(platformWeb, true); st != nil {
		opts["settings"] = st
	}
	if *s.sqlite.Load() != "" {
		opts["sqlite"] = "asuka/" + webSQLitePath
	}
	io.WriteString(w, webIndex(webPage{
		Name: s.app.displayName(), Lang: devLangOr("en"), Launch: launchColors(s.app.cfg),
		Host: "asuka", Core: "asuka.wasm?" + s.stamp,
		Scripts: prefixed("asuka/", scriptPaths(*s.scripts.Load())), Icon: s.app.cfg.Icon != "",
		Options: opts,
		Colors:  s.app.cfg.Web.Colors,
	}))
}

// events is the page's connection for reloads (server-sent events).
func (s *devServer) events(w http.ResponseWriter, r *http.Request) {
	f, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "no streaming", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "text/event-stream")
	c := make(chan string, 8)
	page := r.URL.Query().Get("page")
	s.mu.Lock()
	s.clients[c] = true
	if page != "" {
		s.pages[page] = &webPerfPage{overlay: s.perf}
	}
	s.mu.Unlock()
	defer func() {
		s.mu.Lock()
		delete(s.clients, c)
		delete(s.pages, page)
		s.mu.Unlock()
	}()
	io.WriteString(w, ": asuka\n\n")
	// A page that comes while the build is broken hears so at once, and
	// while the overlay's on, that it is.
	s.mu.Lock()
	failed, perf := s.failed, s.perf
	s.mu.Unlock()
	if failed != "" {
		io.WriteString(w, "event: failed\ndata: "+failed+"\n\n")
	}
	if perf && page != "" {
		io.WriteString(w, "event: perf\ndata: "+page+" 1\n\n")
	}
	f.Flush()
	for {
		select {
		case <-r.Context().Done():
			return
		case m := <-c:
			io.WriteString(w, m)
			f.Flush()
		}
	}
}

// webPerfPage is what the performance tools last asked of a page.
type webPerfPage struct {
	overlay, recording bool
}

func boolDigit(b bool) string {
	if b {
		return "1"
	}
	return "0"
}

// pagePerf drives a page's performance tools by its events: no CPU
// samples (the web's core has no thread to sample with).
func (s *devServer) pagePerf(page string) *perfCtl {
	with := func(f func(p *webPerfPage)) {
		s.mu.Lock()
		if p := s.pages[page]; p != nil {
			f(p)
		}
		s.mu.Unlock()
	}
	return &perfCtl{
		overlay: func(on bool) {
			with(func(p *webPerfPage) { p.overlay = on })
			s.send("perf", page+" "+boolDigit(on))
		},
		start: func(bool) {
			with(func(p *webPerfPage) { p.recording = true })
			s.send("profile", page+" start 0")
		},
		stop: func() {
			with(func(p *webPerfPage) { p.recording = false })
			s.send("profile", page+" stop")
		},
		state: func() (overlay, recording bool) {
			with(func(p *webPerfPage) { overlay, recording = p.overlay, p.recording })
			return
		},
	}
}

// printLog prints what the page sent: runs of the app's output (marked
// \x01) and its errors (\x02).
func printLog(b []byte) {
	for len(b) > 0 {
		out := os.Stdout
		if b[0] == 2 {
			out = os.Stderr
		}
		b = b[1:]
		i := bytes.IndexAny(b, "\x01\x02")
		if i < 0 {
			i = len(b)
		}
		out.Write(b[:i])
		b = b[i:]
	}
}

func contentType(p string) string {
	switch path.Ext(p) {
	case ".js", ".mjs":
		return "text/javascript; charset=utf-8"
	case ".css":
		return "text/css; charset=utf-8"
	case ".wasm":
		return "application/wasm"
	case ".json", ".webmanifest":
		return "application/json"
	}
	return "application/octet-stream"
}

// webWatcher notices changes to JavaScript the page loads as it is, with
// no build: the host's own (a checkout of Asuka: src/ and its modules/),
// and the app's (web/*.js).
type webWatcher struct {
	dir, last string
}

func newWebWatcher(dir string) *webWatcher {
	w := &webWatcher{dir: dir}
	w.last = w.scan()
	return w
}

func (w *webWatcher) scan() string {
	if w.dir == "" {
		return ""
	}
	var b strings.Builder
	filepath.WalkDir(w.dir, func(p string, d fs.DirEntry, err error) error {
		if err != nil || d.IsDir() {
			return nil
		}
		if info, err := d.Info(); err == nil {
			fmt.Fprintf(&b, "%s%d;", p, info.ModTime().UnixNano())
		}
		return nil
	})
	return b.String()
}

func (w *webWatcher) changed() bool {
	s := w.scan()
	if s == w.last {
		return false
	}
	w.last = s
	return true
}

func openBrowser(url string) {
	var c *exec.Cmd
	switch runtime.GOOS {
	case "darwin":
		c = exec.Command("open", url)
	case "windows":
		c = exec.Command("rundll32", "url.dll,FileProtocolHandler", url)
	default:
		c = exec.Command("xdg-open", url)
	}
	c.Start()
}

func lanAddrs() []string {
	var out []string
	as, _ := net.InterfaceAddrs()
	for _, a := range as {
		if n, ok := a.(*net.IPNet); ok && n.IP.To4() != nil && !n.IP.IsLoopback() {
			out = append(out, n.IP.String())
		}
	}
	return out
}

func prefixed(prefix string, names []string) []string {
	out := make([]string, len(names))
	for i, n := range names {
		out[i] = prefix + n
	}
	return out
}

func devLangOr(def string) string {
	if devLang != "" {
		return devLang
	}
	return def
}

func buildWeb(app *asukaApp, o *buildOptions) error {
	out := filepath.Join(o.out, "web")
	if err := os.RemoveAll(out); err != nil {
		return err
	}
	if err := os.MkdirAll(out, 0o755); err != nil {
		return err
	}
	// The core with the app compiled in.
	m, err := app.compile(target.Web)
	if err != nil {
		return err
	}
	tmp := filepath.Join(out, ".asuka.wasm")
	if err := appbuild.Lib(cbuild.Target{OS: "wasm", Arch: "wasm32"}, cbuild.Module, m, tmp); err != nil {
		return err
	}
	defer os.Remove(tmp)
	wasm, err := os.ReadFile(tmp)
	if err != nil {
		return err
	}
	vapid, err := webVAPIDKey(app)
	if err != nil {
		return err
	}
	src, _ := webSource()
	// The host's files go in a directory named by their digest: they
	// import each other by name.
	files := map[string][]byte{}
	fs.WalkDir(src, ".", func(p string, d fs.DirEntry, err error) error {
		if err == nil && !d.IsDir() {
			files[p], _ = fs.ReadFile(src, p)
		}
		return nil
	})
	// SQLite's module, when the app imports std/sqlite, and the native
	// packages' scripts.
	sqlite, err := webSQLite(m.C, false)
	if err != nil {
		return err
	}
	if sqlite != "" {
		if files[webSQLitePath], err = os.ReadFile(sqlite); err != nil {
			return err
		}
	}
	natives, err := nativeFor(app, "web")
	if err != nil {
		return err
	}
	pscripts, err := nativeScripts(natives)
	if err != nil {
		return err
	}
	for _, sc := range pscripts {
		if files[sc.path], err = os.ReadFile(sc.file); err != nil {
			return err
		}
	}
	names := make([]string, 0, len(files))
	for n := range files {
		names = append(names, n)
	}
	sort.Strings(names)
	h := sha256.New()
	for _, n := range names {
		h.Write([]byte(n))
		h.Write(files[n])
	}
	hostDir := "asuka/" + hex.EncodeToString(h.Sum(nil))[:12]
	for _, n := range names {
		if err := writeWeb(filepath.Join(out, filepath.FromSlash(hostDir), n), files[n]); err != nil {
			return err
		}
	}
	sum := sha256.Sum256(wasm)
	wasmName := "asuka." + hex.EncodeToString(sum[:])[:12] + ".wasm"
	if err := writeWeb(filepath.Join(out, wasmName), wasm); err != nil {
		return err
	}
	if err := copyDir(filepath.Join(app.dir, "assets"), filepath.Join(out, "assets")); err != nil {
		return err
	}
	// The native packages' JavaScript, which the page imports (in the
	// host's directory, named by its digest).
	scripts := prefixed(hostDir+"/", scriptPaths(pscripts))
	name := app.displayName()
	icon := app.cfg.Icon != ""
	if icon {
		if err := webIcons(app.cfg.Icon, filepath.Join(out, "icons")); err != nil {
			return err
		}
	}
	manifest := map[string]any{
		"name": name, "short_name": name, "id": "./", "start_url": "./", "scope": "./",
		"display": "standalone", "background_color": "#ffffff", "theme_color": "#ffffff",
	}
	if app.cfg.Accent != "" {
		manifest["theme_color"] = app.cfg.Accent
	}
	if bg := app.cfg.Web.Colors.Background; bg != "" {
		manifest["background_color"] = cssHex(bg)
	}
	if icon {
		manifest["icons"] = []map[string]string{
			{"src": "icons/icon-192.png", "sizes": "192x192", "type": "image/png"},
			{"src": "icons/icon-512.png", "sizes": "512x512", "type": "image/png"},
			{"src": "icons/icon-512.png", "sizes": "512x512", "type": "image/png", "purpose": "maskable"},
		}
	}
	// asuka.toml's fileTypes: an installed app opens them (Chromium's
	// File Handling), the host reads them from the launch queue.
	if fh := webFileHandlers(app.cfg.fileTypes()); fh != nil {
		manifest["file_handlers"] = fh
	}
	mb, _ := json.MarshalIndent(manifest, "", "  ")
	if err := os.WriteFile(filepath.Join(out, "manifest.webmanifest"), mb, 0o644); err != nil {
		return err
	}
	// The app's service worker, beside index.html (its scope is the app's
	// directory), named by its digest too.
	settings := map[string]any{"name": app.cfg.ID, "title": name, "assets": "assets"}
	if icon {
		settings["icon"] = "icons/icon-192.png"
	}
	sw, err := webWorker(src, settings)
	if err != nil {
		return err
	}
	swSum := sha256.Sum256(sw)
	swName := "sw." + hex.EncodeToString(swSum[:])[:12] + ".js"
	if err := writeWeb(filepath.Join(out, swName), sw); err != nil {
		return err
	}
	opts := map[string]any{"name": app.cfg.ID, "title": name, "fonts": fontFiles(app), "version": app.cfg.Version, "build": app.cfg.Build, "serviceWorker": swName}
	icons, err := webAlternateIcons(app.cfg)
	if err != nil {
		return err
	}
	if icons != nil {
		opts["icons"] = icons
	}
	if icon {
		opts["icon"] = "icons/icon-192.png"
	}
	if vapid != "" {
		opts["vapidPublicKey"] = vapid
	}
	if st := app.cfg.analyticsSettings(platformWeb, false); st != nil {
		opts["settings"] = st
	}
	if sqlite != "" {
		opts["sqlite"] = hostDir + "/" + webSQLitePath
	}
	base := o.base
	if base == "" {
		base = app.cfg.Web.Base
	}
	base = siteBase(base)
	page := webIndex(webPage{
		Name: name, Lang: "en", Base: base, Host: hostDir, Core: wasmName, Launch: launchColors(app.cfg),
		Options:  opts,
		Icon:     icon,
		Manifest: true,
		Scripts:  scripts,
		Colors:   app.cfg.Web.Colors,
	})
	if err := os.WriteFile(filepath.Join(out, "index.html"), []byte(page), 0o644); err != nil {
		return err
	}
	// What platform/update reads for a newer version than the page's.
	if err := os.WriteFile(filepath.Join(out, "version.json"), versionFile(app), 0o644); err != nil {
		return err
	}
	if err := os.WriteFile(filepath.Join(out, "_headers"), []byte(webHeaders(base)), 0o644); err != nil {
		return err
	}
	// Every route's path is the app's page (a Router's addresses: a
	// reload, a link pasted): Netlify's rewrite (Cloudflare Pages does it
	// for a site with no 404.html; docs/web.md: the other hosts).
	if err := os.WriteFile(filepath.Join(out, "_redirects"), []byte(webRedirects(base)), 0o644); err != nil {
		return err
	}
	fmt.Printf("asuka: %s (%s of WebAssembly, %s gzipped)\n", out, size(len(wasm)), size(gzipSize(wasm)))
	fmt.Printf("asuka: a static site for %s: serve the folder there from any web server or CDN, index.html and manifest.webmanifest with no-cache, the rest for good (_headers says so for Cloudflare and Netlify), and index.html for the paths that have no file (a Router's routes: _redirects does it on Netlify; https://asuka.egoist.dev/docs/web for others)\n", base)
	return nil
}

// webHeaders is the site's _headers (Cloudflare's and Netlify's): the
// page and the manifest fetched again, what's named by its digest cached
// for good.
func webHeaders(base string) string {
	var b strings.Builder
	for _, h := range []struct{ path, header string }{
		{"index.html", "Cache-Control: no-cache"},
		{"manifest.webmanifest", "Cache-Control: no-cache"},
		{"version.json", "Cache-Control: no-cache"},
		{"asuka/*", "Cache-Control: public, max-age=31536000, immutable"},
		{"web/*", "Cache-Control: public, max-age=31536000, immutable"},
		{"sw.*.js", "Cache-Control: public, max-age=31536000, immutable"},
		{"*.wasm", "Cache-Control: public, max-age=31536000, immutable\n  Content-Type: application/wasm"},
	} {
		fmt.Fprintf(&b, "%s%s\n  %s\n", base, h.path, h.header)
	}
	return b.String()
}

// webRedirects is the site's _redirects (Netlify's): a path with no file
// is the app's page, which shows its route.
func webRedirects(base string) string {
	return base + "*  " + base + "index.html  200\n"
}

// writeWeb writes a file of the site, and a gzipped copy of what
// compresses (servers that serve precompressed files send it).
func writeWeb(p string, b []byte) error {
	if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
		return err
	}
	if err := os.WriteFile(p, b, 0o644); err != nil {
		return err
	}
	switch path.Ext(p) {
	case ".js", ".mjs", ".css", ".wasm", ".json":
		var z bytes.Buffer
		zw, _ := gzip.NewWriterLevel(&z, gzip.BestCompression)
		zw.Write(b)
		zw.Close()
		return os.WriteFile(p+".gz", z.Bytes(), 0o644)
	}
	return nil
}

func gzipSize(b []byte) int {
	var z bytes.Buffer
	zw, _ := gzip.NewWriterLevel(&z, gzip.BestCompression)
	zw.Write(b)
	zw.Close()
	return z.Len()
}

func size(n int) string {
	if n >= 1<<20 {
		return fmt.Sprintf("%.1f MB", float64(n)/(1<<20))
	}
	return fmt.Sprintf("%d KB", n>>10)
}

// webIcons makes the page's icons from the app's square icon: the
// manifest's (192 and 512), iOS's home screen icon (180) and the favicon.
func webIcons(src, dir string) error {
	img, err := loadIcon(src)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return err
	}
	for name, px := range map[string]int{"icon-192.png": 192, "icon-512.png": 512, "apple-touch-icon.png": 180, "favicon.png": 32} {
		f, err := os.Create(filepath.Join(dir, name))
		if err != nil {
			return err
		}
		w := bufio.NewWriter(f)
		err = png.Encode(w, resize(img, px))
		w.Flush()
		f.Close()
		if err != nil {
			return err
		}
	}
	return nil
}

// findNatives finds what the app's page loads besides the core again
// (its imports may have changed): SQLite's module, when it imports
// std/sqlite, and its native packages' scripts.
func (s *devServer) findNatives() error {
	sqlite, err := webSQLite(s.app.c, true)
	if err != nil {
		return err
	}
	natives, err := nativeFor(s.app, "web")
	if err != nil {
		return err
	}
	scripts, err := nativeScripts(natives)
	if err != nil {
		return err
	}
	s.sqlite.Store(&sqlite)
	s.scripts.Store(&scripts)
	return nil
}

// webAlternateIcons are asuka.toml's alternate icons as the page's icon
// shows them (platform/appicon): each a 64-pixel PNG, as a data: URL, by
// name. nil without any.
func webAlternateIcons(c *appConfig) (map[string]string, error) {
	if len(c.AlternateIcons) == 0 {
		return nil, nil
	}
	out := map[string]string{}
	for name, p := range c.AlternateIcons {
		img, err := loadIcon(p)
		if err != nil {
			return nil, fmt.Errorf("alternateIcons.%s: %w", name, err)
		}
		var b bytes.Buffer
		if err := png.Encode(&b, resize(img, 64)); err != nil {
			return nil, err
		}
		out[name] = "data:image/png;base64," + base64.StdEncoding.EncodeToString(b.Bytes())
	}
	return out, nil
}
