package main

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/egoist/asuka/internal/cachedir"
	"github.com/egoist/asuka/internal/ccache"

	"github.com/egoist/asuka/internal/appbuild"
	"github.com/egoist/asuka/internal/appconfig"
	"github.com/egoist/asuka/internal/filetype"
	"github.com/egoist/asuka/lang/bytecode"
	"github.com/egoist/asuka/lang/check"
	"github.com/egoist/asuka/lang/diag"
	"github.com/egoist/asuka/lang/syntax"
	"github.com/egoist/asuka/lang/target"
)

// asukaApp is an app: its directory of .asuka files, its asuka.toml, its
// dependencies (appbuild.Project) and what its sources import and name,
// which decide what each platform's build needs.
type asukaApp struct {
	dir      string // the app's directory
	name     string // its base name
	bundleID string
	cfg      *appConfig // asuka.toml
	proj     *appbuild.Project
	// platform is the platform it's loaded for (asuka dev's, a build's):
	// what uses and the config it decides (permissions) are of.
	platform target.Platform
	uses     *appbuild.Uses
	// c is the C the app's extern declarations call, as of the last
	// compile (nil: none), which a player is built with.
	c *bytecode.C
}

// cKey names the C c a player is built with: its glue (the extern
// declarations) and its projects. A change to their C files is the
// watcher's.
func cKey(c *bytecode.C) string {
	if c == nil {
		return ""
	}
	h := sha256.New()
	h.Write(c.Glue)
	for _, d := range c.Dirs {
		h.Write([]byte("\x00" + d))
	}
	for _, d := range c.Std {
		h.Write([]byte("\x01" + d))
	}
	return hex.EncodeToString(h.Sum(nil))
}

// displayName is the name under the app's icon.
func (a *asukaApp) displayName() string {
	if a.cfg != nil && a.cfg.Name != "" {
		return a.cfg.Name
	}
	return a.name
}

// deps are the packages the program imports, all the way down, by
// import path (platform/location, @acme/charts).
func (a *asukaApp) deps() []string { return a.used().ImportList() }

// imports reports whether the program imports pkg (platform/location).
func (a *asukaApp) imports(pkg string) bool { return a.used().Import(pkg) }

// refs reports whether the program refers to pkg's member (pkg "": the
// prelude's, as Video), from outside pkg: the app's code or a
// dependency's.
func (a *asukaApp) refs(pkg, member string) bool { return a.used().Ref(pkg, member) }

// used is what the program imports and refers to (nothing, for an app
// whose sources weren't read).
func (a *asukaApp) used() *appbuild.Uses {
	if a.uses == nil {
		return &appbuild.Uses{}
	}
	return a.uses
}

// loadApp reads the app in dir: its asuka.toml, its dependencies
// (installed from asuka.lock when it has any), and what its sources
// import and name, from which its permissions and its platforms'
// features follow.
//
// platform is the --platform it's for (ios, mac...): its packages' files
// for that platform are the ones whose uses count.
func loadApp(dir, platform string) (*asukaApp, error) {
	proj, err := appbuild.Load(dir)
	if err != nil {
		return nil, err
	}
	if proj.File != "" {
		return nil, fmt.Errorf("%s is a file: asuka dev and asuka build take an app's directory (asuka run runs one file)", dir)
	}
	cfg, err := loadConfig(proj.Dir)
	if err != nil {
		return nil, err
	}
	// The catalogs: the compiler checks them (and tr's calls of them),
	// as each compile does.
	app := &asukaApp{dir: proj.Dir, name: filepath.Base(proj.Dir), bundleID: cfg.ID, cfg: cfg, proj: proj, platform: langPlatform(platform)}
	app.refresh()
	warnDevLang(cfg)
	warnShareSheet(cfg)
	return app, nil
}

// refresh takes again what the program uses, from its compile (the one
// asuka dev just did for a save, or one it makes now, which a build's
// compile then reuses: Project.Compile), and what that needs of
// asuka.toml's (permissions, entitlements).
func (a *asukaApp) refresh() {
	if a.proj == nil {
		a.proj = &appbuild.Project{Dir: a.dir}
	}
	a.uses = a.proj.Uses(a.platform)
	deps := a.deps()
	if err := a.cfg.depManifests(a.packageDirs(), a.uses); err != nil {
		fmt.Fprintln(os.Stderr, "asuka:", err)
	}
	a.cfg.permissionsFor(deps)
	a.cfg.addDerived()
	a.cfg.displayName = a.displayName()
}

// compile compiles the app for platform to its module (its packages'
// files for platform, @platform's branches), printing its diagnostics;
// an error says it didn't compile. Its test blocks are left out (and the
// test builtins with them, which only asuka test's fake host has): this
// is the module a player loads and a release compiles to C.
func (a *asukaApp) compile(platform target.Platform) (*bytecode.Module, error) {
	m, _, err := a.compileShown(platform, nil)
	if m != nil {
		a.c = m.C
	}
	return m, err
}

// compileShown is compile, printing only the diagnostics shown doesn't
// have, and returning them all (asuka dev's for each player's platform:
// what two platforms report alike is said once). It leaves a.c as it is.
func (a *asukaApp) compileShown(platform target.Platform, shown map[string]bool) (*bytecode.Module, []check.Diagnostic, error) {
	m, diags, err := a.proj.Compile(platform)
	for _, d := range diags {
		if !shown[d.String()] {
			fmt.Fprintln(os.Stderr, relDiag(a.dir, d).Format(diag.Error, ""))
		}
	}
	if err != nil {
		return nil, diags, err
	}
	if m == nil {
		return nil, diags, fmt.Errorf("%d %s", len(diags), plural(len(diags), "error", "errors"))
	}
	if m.App < 0 {
		return nil, diags, fmt.Errorf("%s has no app block (app { Root() }) to run", a.dir)
	}
	return appbuild.StripTests(m), diags, nil
}

// relDiag is d with its files relative to dir, when they're in it.
func relDiag(dir string, d check.Diagnostic) check.Diagnostic {
	for _, pos := range []*syntax.Position{&d.Pos, &d.End} {
		if !filepath.IsAbs(pos.Filename) {
			continue
		}
		if r, err := filepath.Rel(dir, pos.Filename); err == nil && !strings.HasPrefix(r, "..") {
			pos.Filename = r
		}
	}
	return d
}

// hostConfig is what the desktop host reads of the app as it starts
// (internal/appconfig's File): asuka.toml's, and asuka dev's --lang and
// --dir for its runs. target is a release's (darwin-arm64,
// windows-amd64...; "": asuka dev's player), and updates says whether
// it updates itself (asuka.toml's updates; not from the Mac App Store).
func (a *asukaApp) hostConfig(platform, target string, updates bool) appconfig.Config {
	c := appconfig.Config{
		Name:              a.displayName(),
		ID:                a.cfg.ID,
		Version:           a.cfg.Version,
		Build:             fmt.Sprint(a.cfg.Build),
		Accent:            a.cfg.Accent,
		AccentDark:        a.cfg.AccentDark,
		Scheme:            a.cfg.Scheme,
		Lang:              devLang,
		Direction:         devDirection,
		Background:        a.cfg.Background,
		KeepRunning:       a.cfg.KeepRunning,
		MultipleInstances: a.cfg.SingleInstance != nil && !*a.cfg.SingleInstance,
	}
	if u, err := a.cfg.updates(); err == nil && u != nil && target != "" && updates {
		c.UpdateFeed, c.UpdateKey, c.UpdateTarget = u.feed(target), strings.TrimSpace(u.PublicKey), target
	}
	if platform == platformWindows && target != "" {
		// The executable's alternate icons (alternateIconID), which the
		// player's hasn't.
		c.AlternateIcons = strings.Join(a.cfg.alternateIconNames(), ",")
	}
	if platform == platformWindows {
		if types := a.cfg.fileTypes(); len(types) > 0 {
			// A release's icons are its executable's resources; the
			// player's has none.
			ids := map[int]int{}
			for i, t := range types {
				if t.icon != "" && target != "" {
					ids[i] = fileTypeIconID(i)
				}
			}
			c.FileTypes = filetype.Encode(windowsFileTypes(types, ids))
		}
	}
	return c
}

// workDir is the app's directory for platform's builds in the user's
// cache.
func workDir(app *asukaApp, platform string) (string, error) {
	sum := sha256.Sum256([]byte(app.dir))
	dir := filepath.Join(cachedir.Root(), app.name+"-"+hex.EncodeToString(sum[:])[:8], platform)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return "", err
	}
	// Used now: the build cache evicts what was used least lately (an
	// app's work directory whole).
	ccache.Touch(filepath.Dir(dir))
	return dir, nil
}

// packageDirs are the app's dependencies' directories and the standard
// library's packages it imports (platform/network), by package ID: what
// brings native code and asks things of the app (asuka.toml's [ios] and
// [android]).
func (a *asukaApp) packageDirs() map[string]string {
	out := map[string]string{}
	if a.proj == nil {
		return out
	}
	for id, d := range a.proj.Deps {
		out[id] = d
	}
	for id, d := range a.proj.Std(a.platform) {
		out[id] = d
	}
	return out
}
