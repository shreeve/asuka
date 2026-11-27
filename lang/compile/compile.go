// Package compile is the Asuka compiler's driver: a package directory,
// the packages it imports (beside it, from the standard library), and
// the prelude, checked (lang/check) and lowered (lang/ir) into one
// bytecode module (lang/bytecode), with the diagnostics found.
package compile

import (
	"errors"
	"fmt"
	"os"
	pathpkg "path"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"

	"github.com/egoist/asuka/internal/cbuild"
	"github.com/egoist/asuka/internal/pkgs"
	"github.com/egoist/asuka/internal/toml"
	"github.com/egoist/asuka/lang/bytecode"
	"github.com/egoist/asuka/lang/cffi"
	"github.com/egoist/asuka/lang/check"
	"github.com/egoist/asuka/lang/diag"
	"github.com/egoist/asuka/lang/ir"
	"github.com/egoist/asuka/lang/syntax"
	"github.com/egoist/asuka/lang/target"
	"github.com/egoist/asuka/sdk"
	"github.com/egoist/asuka/std"
)

// Options say how to compile.
type Options struct {
	// Packages are packages given as sources rather than directories, by
	// import path (std/http stubs in tests): file name → source.
	Packages map[string]map[string][]byte
	// Resolve finds a dependency: the package that from (a dependency's
	// ID; "" for the root package) imports as path (@acme/charts). Dir
	// resolves through the project's asuka.lock when it's nil.
	Resolve func(from, path string) (id, dir string, err error)
	// Overlay holds files' unsaved contents by absolute path (an editor's
	// open documents): read instead of what's on disk, and a .asuka file
	// the disk doesn't have yet is part of its directory's package.
	Overlay map[string][]byte
	// CheckOnly stops after the checker (an editor's diagnostics): no
	// lowering, no Module.
	CheckOnly bool
	// Catalogs are the app's catalogs (tr). Dir finds them itself
	// (CatalogsOf) when they're nil and NoCatalogs isn't set.
	Catalogs   *Catalogs
	NoCatalogs bool
	// Lang and Direction are a language and a direction ("rtl", "ltr")
	// the module shows whatever the user's (asuka dev's --lang and --dir).
	Lang, Direction string
	// Project is the directory of the project the package is a target of
	// (asuka.toml's [bin.<name>]: a program in a directory of its own),
	// whose asuka.lock and catalogs it uses; "": the package's own.
	Project string
	// Platform is the platform the program is compiled for ("":
	// target.Default): what @platform is, and which of each package's
	// files are part of it (tray_linux.asuka only on Linux).
	Platform target.Platform
	// Debug compiles a development build (asuka dev, run and test):
	// what @debug is. Without it, @debug is false, as in a release.
	Debug bool
	// Cover counts the runs of the root package's statements (asuka
	// test --cover): Result.IR.Cover says where its counters are.
	Cover bool
	// NoOptimize leaves the lowered code as it is: no optimizer passes
	// (lang/ir's Optimize). A module that counts coverage is never
	// optimized (its counters are its statements'), and ASUKA_OPT=0 in
	// the environment turns them off too.
	NoOptimize bool
	// NoTests compiles a build, not a test build: the root package's test
	// files (name_test.asuka) and its test and bench blocks are left out,
	// unchecked, as they are of every package it imports. Without it, the
	// root package's are part of it (asuka test, asuka check, an
	// editor's); an imported package's never are.
	NoTests bool
	// Preview, when set, builds the preview it names as the module's app,
	// in place of the app block (asuka dev's, when it's asked to show
	// one): the package's other previews, and its tests and benches, are
	// left out, as a build's are, and its file is part of it even when
	// it's a test file.
	Preview *PreviewRef
	// PreviewTests makes each of the root package's previews a test of a
	// test build, which compares what it shows with its snapshot (asuka
	// test --previews).
	PreviewTests bool
	// NoFuse lowers Seq pipelines and arrays' closure methods as the
	// runtime's calls rather than as loops (lang/ir's seqfuse.go): what
	// the tests compare the loops with. ASUKA_OPT_SKIP=fuse too.
	NoFuse bool

	// disk keeps what the compilations of one DirPlatforms read: each
	// platform's reads the same directories and files.
	disk *diskCache
}

// diskCache is the directories listed and the files read, by path; the
// platforms' compilations share it at once.
type diskCache struct {
	mu    sync.Mutex
	dirs  map[string]dirListing
	files map[string]fileRead
}

type dirListing struct {
	entries []os.DirEntry
	err     error
}

type fileRead struct {
	src []byte
	err error
}

func (c *diskCache) readDir(dir string) ([]os.DirEntry, error) {
	if c == nil {
		return os.ReadDir(dir)
	}
	c.mu.Lock()
	r, ok := c.dirs[dir]
	c.mu.Unlock()
	if ok {
		return r.entries, r.err
	}
	es, err := os.ReadDir(dir)
	c.mu.Lock()
	c.dirs[dir] = dirListing{es, err}
	c.mu.Unlock()
	return es, err
}

// readFile is a file's contents, shared: the parser doesn't write them.
func (c *diskCache) readFile(path string) ([]byte, error) {
	if c == nil {
		return os.ReadFile(path)
	}
	c.mu.Lock()
	r, ok := c.files[path]
	c.mu.Unlock()
	if ok {
		return r.src, r.err
	}
	src, err := os.ReadFile(path)
	c.mu.Lock()
	c.files[path] = fileRead{src, err}
	c.mu.Unlock()
	return src, err
}

// Result is a compilation's outcome.
type Result struct {
	// Platform is the platform it compiled for.
	Platform target.Platform
	// PlatformFiles reports whether a package of the program (the root's,
	// an import's) has files for some platforms only: compiled for another
	// platform, the program has other files.
	PlatformFiles bool
	Module        *bytecode.Module // nil when there are errors
	Diags         []check.Diagnostic
	// Warnings are what's worth saying that doesn't stop the build: the
	// catalogs' (a translation's missing messages, ...).
	Warnings []check.Diagnostic
	Program  *check.Program
	Fset     *syntax.FileSet
	IR       *ir.Program
}

// Err is the diagnostics as an error, nil without any.
func (r *Result) Err() error {
	if len(r.Diags) == 0 {
		return nil
	}
	var b strings.Builder
	for i, d := range r.Diags {
		if i > 0 {
			b.WriteByte('\n')
		}
		b.WriteString(d.String())
	}
	return errors.New(b.String())
}

// a package to check: its files, parsed, and its imports, resolved.
type unit struct {
	path  string // its import path ("" for the root)
	id    string // a dependency's ID in asuka.lock ("" for the project's own)
	dir   string
	files []*syntax.SourceFile
	// others are its files for other platforms than the compile's
	// (tray_linux.asuka compiling for iOS), parsed for their names only:
	// a name a file every platform has declares, a platform's file can't.
	others  []*syntax.SourceFile
	imports map[string]*unit // by the path as written; nil: it failed (reported)
	std     map[string]bool
	pkg     *check.Package
	state   int // 0 new, 1 visiting, 2 done
}

type loader struct {
	fset  *syntax.FileSet
	opts  *Options
	units map[string]*unit // by directory (or overlay path)
	diags []check.Diagnostic
	// depDirs are the dependencies' directories, by ID.
	depDirs map[string]string
	// platformFiles: some package has files for some platforms only.
	platformFiles bool
	// twice are the declarations of names declared in two files a
	// platform has both of (dupNames), which the checker would report
	// again.
	twice map[syntax.Position]bool
	// preview is the preview Options.Preview names, once its file is
	// read (nil: none found).
	preview *syntax.PreviewDecl
}

// Dir compiles the package in dir.
func Dir(dir string, opts *Options) (*Result, error) {
	if opts == nil {
		opts = &Options{}
	}
	abs, err := filepath.Abs(dir)
	if err != nil {
		return nil, err
	}
	project := abs
	if opts.Project != "" {
		if project, err = filepath.Abs(opts.Project); err != nil {
			return nil, err
		}
	}
	if opts.Resolve == nil {
		o := *opts
		o.Resolve = pkgs.NewImports(project).Resolve
		opts = &o
	}
	if opts.Catalogs == nil && !opts.NoCatalogs {
		cs, err := CatalogsOf(project)
		if err != nil {
			return nil, err
		}
		o := *opts
		o.Catalogs = cs
		opts = &o
	}
	l := &loader{fset: syntax.NewFileSet(), opts: opts, units: map[string]*unit{}}
	// A package of the standard library's or the platform SDK's (its own
	// tests, asuka test std/term) is compiled as its import path, which
	// its native fns are the runtime's under.
	root, err := l.loadDir(carried(abs), "", abs)
	if err != nil {
		return nil, err
	}
	if len(root.files) == 0 {
		if len(root.others) > 0 {
			return nil, fmt.Errorf("no .asuka files in %s for %s (its files are other platforms')", dir, opts.Platform.Or().Title())
		}
		return nil, fmt.Errorf("no .asuka files in %s", dir)
	}
	return l.compile(root), nil
}

// carried is the import path of the standard library's or the platform
// SDK's package in dir (std/term), "" when dir isn't one of theirs: a
// directory with an asuka.toml in theirs (std.Packages', found without
// listing them all).
func carried(dir string) string {
	for _, t := range []struct {
		prefix string
		dir    func() (string, error)
	}{{std.Prefix, std.Dir}, {std.PlatformPrefix, sdk.Dir}} {
		d, err := t.dir()
		if err != nil {
			return ""
		}
		if abs, err := filepath.Abs(d); err != nil || abs != filepath.Dir(dir) {
			continue
		}
		if _, err := os.Stat(filepath.Join(dir, "asuka.toml")); err == nil {
			return t.prefix + filepath.Base(dir)
		}
	}
	return ""
}

// Files compiles one package made of files (name → source).
func Files(files map[string][]byte, opts *Options) *Result {
	if opts == nil {
		opts = &Options{}
	}
	l := &loader{fset: syntax.NewFileSet(), opts: opts, units: map[string]*unit{}}
	root := l.loadSources("", "", ".", files)
	return l.compile(root)
}

func (l *loader) loadDir(path, id, dir string) (*unit, error) {
	if u, ok := l.units[dir]; ok {
		return u, nil
	}
	files := map[string][]byte{}
	for p, src := range l.opts.Overlay {
		if filepath.Dir(p) == dir && strings.HasSuffix(p, ".asuka") {
			files[p] = src
		}
	}
	entries, err := l.opts.disk.readDir(dir)
	if err != nil && len(files) == 0 {
		return nil, err
	}
	for _, e := range entries {
		if e.IsDir() || !strings.HasSuffix(e.Name(), ".asuka") {
			continue
		}
		p := filepath.Join(dir, e.Name())
		if _, ok := files[p]; ok {
			continue
		}
		src, err := l.opts.disk.readFile(p)
		if err != nil {
			return nil, err
		}
		files[p] = src
	}
	u := l.loadSources(path, id, dir, files)
	return u, nil
}

func (l *loader) loadSources(path, id, dir string, files map[string][]byte) *unit {
	key := dir
	if path != "" && l.opts.Packages[path] != nil {
		key = "pkg:" + path
	}
	if u, ok := l.units[key]; ok {
		return u
	}
	// The first package loaded is the root: the one its tests are of.
	tests := len(l.units) == 0 && !l.opts.NoTests
	u := &unit{path: path, id: id, dir: dir, imports: map[string]*unit{}, std: map[string]bool{}}
	l.units[key] = u
	names := make([]string, 0, len(files))
	for n := range files {
		names = append(names, n)
	}
	sort.Strings(names)
	platform := l.opts.Platform.Or()
	root := len(l.units) == 1
	for _, n := range names {
		previewFile := root && l.opts.Preview.inFile(n, dir)
		if !tests && IsTestFile(n) && !previewFile {
			continue // a test build's only, and only its root package's
		}
		if ps, _ := target.FileSuffix(n); ps != nil {
			l.platformFiles = true
			if !target.Includes(n, platform) {
				// Another platform's: its names, not its errors (asuka
				// check checks it for its own).
				f, _ := syntax.ParseFile(syntax.NewFileSet(), n, files[n])
				u.others = append(u.others, f)
				continue
			}
		}
		f, errs := syntax.ParseFile(l.fset, n, files[n])
		for _, e := range errs {
			l.diags = append(l.diags, check.FromSyntax(e))
		}
		if !tests {
			var keep *syntax.PreviewDecl
			if previewFile {
				keep = l.opts.Preview.find(f)
				if keep != nil {
					l.preview = keep
				}
			}
			dropTests(f, keep)
		}
		u.files = append(u.files, f)
	}
	l.dupNames(u)
	for _, f := range u.files {
		for _, imp := range f.Imports {
			ip := imp.PathString()
			if _, ok := u.imports[ip]; ok || u.std[ip] {
				continue
			}
			if src := l.opts.Packages[ip]; src != nil {
				u.imports[ip] = l.loadSources(ip, "", "pkg:"+ip, src)
				continue
			}
			var dep *unit
			var err error
			switch {
			case imp.Relative:
				rel := strings.TrimPrefix(ip, "./") // ../x stays: a directory up
				d := filepath.Join(u.dir, filepath.FromSlash(rel))
				// Its path names it apart from another package's ./util.
				p := ip
				if base := u.path; base != "" {
					p = pathpkg.Join(base, rel)
				}
				dep, err = l.loadDir(p, u.id, d)
			case check.ProvidesStd(ip):
				u.std[ip] = true // the checker provides it
				continue
			case std.Reserved(ip):
				d, ok := std.Package(ip)
				if !ok {
					// The checker says what the standard library has.
					u.std[ip] = true
					continue
				}
				dep, err = l.loadDir(ip, "", d)
			case strings.HasPrefix(ip, "asuka/"):
				pos := l.fset.Position(imp.Start)
				l.diags = append(l.diags, check.NewDiagnostic(pos, l.fset.Position(imp.End), diag.UnknownPackage, "%s", retired(ip)))
				u.imports[ip] = nil // reported: the checker skips it
				continue
			default:
				if l.opts.Resolve == nil {
					err = fmt.Errorf("a dependency needs a project (asuka.toml)")
					break
				}
				var id, d string
				if id, d, err = l.opts.Resolve(u.id, ip); err == nil {
					if l.depDirs == nil {
						l.depDirs = map[string]string{}
					}
					l.depDirs[id] = d
					dep, err = l.loadDir(ip, id, d)
				}
			}
			if err != nil {
				pos := l.fset.Position(imp.Start)
				what := "can't import"
				if imp.Relative {
					what = "can't read package"
				}
				l.diags = append(l.diags, check.NewDiagnostic(pos, pos, diag.UnknownPackage, "%s %s: %v", what, ip, err))
				u.imports[ip] = nil // reported: the checker skips it
				continue
			}
			u.imports[ip] = dep
		}
	}
	return u
}

// IsTestFile reports whether a file is a test file (name_test.asuka,
// name_linux_test.asuka): part of its package in a test build of it
// only, not when it's built or imported.
func IsTestFile(name string) bool {
	return strings.HasSuffix(name, "_test.asuka")
}

// dropTests takes a file's test, bench and preview blocks out of it
// (but keep, a preview's build's preview): a build's packages have none,
// nor what only they use.
func dropTests(f *syntax.SourceFile, keep *syntax.PreviewDecl) {
	if f == nil {
		return
	}
	n := 0
	for _, d := range f.Decls {
		switch d.(type) {
		case *syntax.TestDecl, *syntax.BenchDecl:
			continue
		case *syntax.PreviewDecl:
			if d != syntax.Decl(keep) {
				continue
			}
		}
		f.Decls[n] = d
		n++
	}
	clear(f.Decls[n:])
	f.Decls = f.Decls[:n]
}

// retired says what an import of the retired asuka/ prefix is now: the
// standard library's std/<name>, the platform SDK's platform/<name>.
func retired(ip string) string {
	name := strings.TrimPrefix(ip, "asuka/")
	if name == "platform" {
		return ip + " is platform/host now"
	}
	for _, p := range []string{std.Prefix + name, std.PlatformPrefix + name} {
		if _, ok := std.Package(p); ok || check.ProvidesStd(p) {
			return ip + " is " + p + " now"
		}
	}
	return "no package " + ip + ": the asuka/ prefix is retired; the standard library is std/<name>, the platform SDK platform/<name> (https://asuka.egoist.dev/docs/stdlib)"
}

func (l *loader) compile(root *unit) *Result {
	c := check.NewChecker(l.fset)
	res := &Result{Fset: l.fset, Platform: l.opts.Platform.Or(), PlatformFiles: l.platformFiles}
	// The app's catalogs, before any package is checked: tr is theirs.
	dates := false
	for _, u := range l.units {
		dates = dates || u.std["std/i18n"] || u.imports["std/i18n"] != nil
	}
	if cs := l.opts.Catalogs; cs != nil {
		cat, errs, warns := cs.catalog(dates)
		l.diags = append(l.diags, errs...)
		res.Warnings = warns
		if cat == nil {
			cat = &check.Catalog{Broken: true, Dir: cs.Dir, Source: cs.Source}
		}
		c.SetCatalog(cat)
	} else if dates {
		cat, err := implicitCatalog()
		if err != nil {
			pos := syntax.Position{Filename: "std/i18n", Line: 1, Column: 1}
			l.diags = append(l.diags, check.NewDiagnostic(pos, pos, diag.InternalError, "%s", err.Error()))
		} else {
			c.SetCatalog(cat)
		}
	}
	var visit func(u *unit) bool
	visit = func(u *unit) bool {
		switch u.state {
		case 1:
			return false
		case 2:
			return true
		}
		u.state = 1
		imports := map[string]*check.Package{}
		for ip, dep := range u.imports {
			if dep == nil {
				imports[ip] = nil
				continue
			}
			if !visit(dep) {
				pos := l.fset.Position(u.files[0].Start)
				l.diags = append(l.diags, check.NewDiagnostic(pos, pos, diag.ImportCycle, "import cycle through %s: packages can't import each other", ip))
				imports[ip] = nil // reported: the checker skips it
				continue
			}
			imports[ip] = dep.pkg
		}
		name := "main"
		if u.path != "" {
			name = u.path[strings.LastIndexByte(u.path, '/')+1:]
		}
		u.pkg = c.CheckPackageIn(u.path, name, u.dir, u.files, imports)
		u.pkg.Dir = u.dir
		u.pkg.NativeClass = nativeClass(u, l.opts.disk)
		u.state = 2
		return true
	}
	visit(root)
	prog := c.Program(root.pkg)
	res.Program = prog
	res.Diags = append(append([]check.Diagnostic{}, l.diags...), l.withoutTwice(prog.Diags)...)
	res.Warnings = append(res.Warnings, prog.Warnings...)
	if len(res.Diags) > 0 || l.opts.CheckOnly {
		return res
	}
	accent, accentFlags, accentDark, err := appAccent(root.dir)
	if err == nil {
		err = checkWebColors(root.dir)
	}
	if err != nil {
		pos := syntax.Position{Filename: filepath.Join(root.dir, "asuka.toml"), Line: 1, Column: 1}
		res.Diags = append(res.Diags, check.NewDiagnostic(pos, pos, diag.InvalidProject, "%s", err.Error()))
		return res
	}
	vdir := root.dir
	if l.opts.Project != "" {
		vdir = l.opts.Project
	}
	var cover []string
	if l.opts.Cover {
		for _, f := range root.files {
			if f.File != nil {
				cover = append(cover, f.File.Name())
			}
		}
	}
	var preview *check.Preview
	if l.opts.Preview != nil {
		for _, pv := range root.pkg.Previews {
			if pv.Decl == l.preview {
				preview = pv
			}
		}
		if preview == nil {
			pos := syntax.Position{Filename: l.opts.Preview.File, Line: max(l.opts.Preview.Line, 1), Column: 1}
			res.Diags = append(res.Diags, check.NewDiagnostic(pos, pos, diag.UndefinedName, "no preview %q in %s", l.opts.Preview.Name, filepath.Base(l.opts.Preview.File)))
			return res
		}
	}
	optimize := !l.opts.NoOptimize && !l.opts.Cover && os.Getenv("ASUKA_OPT") != "0"
	// Code the optimizer leaves alone names its locals for the debugger
	// (asuka dev's): the optimizer's passes move values between
	// registers, so its code has none.
	p, diags := ir.Lower(prog, &ir.Options{Platform: string(l.opts.Platform.Or()), Debug: l.opts.Debug, Version: projectVersion(vdir), Lang: l.opts.Lang, Direction: l.opts.Direction, Accent: accent, AccentFlags: accentFlags, AccentDark: accentDark, Cover: cover, NoFuse: l.opts.NoFuse, Locals: !optimize,
		Preview: preview, PreviewTests: l.opts.PreviewTests})
	res.Diags = append(res.Diags, diags...)
	if len(res.Diags) > 0 {
		return res
	}
	res.IR = p
	if optimize {
		p.Optimize()
	}
	res.Module = p.Encode()
	if !optimize {
		// Its frames' locals are named: the debugger evaluates
		// expressions in them.
		res.Module.Evaluator = &Evaluator{res: res}
	}
	glue, err := cffi.Generate(prog)
	if err != nil {
		pos := syntax.Position{Filename: root.dir, Line: 1, Column: 1}
		res.Diags = append(res.Diags, check.NewDiagnostic(pos, pos, diag.DuplicateDeclaration, "%s", err.Error()))
		res.Module = nil
		return res
	}
	if stdC := l.stdCDirs(); glue != nil || len(stdC) > 0 {
		res.Module.C = &bytecode.C{Glue: glue, Std: stdC}
		if glue != nil {
			res.Module.C.Dirs = l.cDirs(root)
		}
	}
	return res
}

// stdCDirs are the directories of the std and sdk packages the program
// imports (all the way down) that have C of their own (their asuka.toml's
// [c]: std/sqlite's SQLite), sorted.
func (l *loader) stdCDirs() []string {
	var dirs []string
	for _, u := range l.units {
		if u.id == "" && std.Reserved(u.path) && u.pkg != nil && cbuild.HasC(u.dir) {
			dirs = append(dirs, u.dir)
		}
	}
	sort.Strings(dirs)
	return dirs
}

// cDirs are the projects whose asuka.toml [c] builds the C the program's
// externs call: the app's (its packages' and their relative imports'),
// then each dependency's with externs, by its ID.
func (l *loader) cDirs(root *unit) []string {
	dirs := []string{root.dir}
	seen := map[string]bool{root.dir: true}
	var ids []string
	for _, u := range l.units {
		if u.id == "" || u.pkg == nil || !hasExterns(u.pkg) {
			continue
		}
		if d := l.depDirs[u.id]; d != "" && !seen[d] {
			seen[d] = true
			ids = append(ids, u.id)
		}
	}
	sort.Strings(ids)
	for _, id := range ids {
		dirs = append(dirs, l.depDirs[id])
	}
	return dirs
}

func hasExterns(p *check.Package) bool {
	for _, f := range p.Funcs {
		if f.Extern != nil {
			return true
		}
	}
	return false
}

// projectVersion is asuka.toml's version in dir ("" for none).
func projectVersion(dir string) string {
	if dir == "" || strings.HasPrefix(dir, "pkg:") {
		return ""
	}
	b, err := os.ReadFile(filepath.Join(dir, "asuka.toml"))
	if err != nil {
		return ""
	}
	var m struct {
		Version string `json:"version"`
	}
	if toml.Unmarshal(b, &m) != nil {
		return ""
	}
	return m.Version
}

// appAccent is the app's accent color from its asuka.toml (accent =
// "#rrggbb" or "#rrggbbaa"), as the wire's Color (0: none, the
// platform's), its flags (1: android.dynamicColor, the wallpaper's
// colors where the system has them) and its color in dark mode
// (accentDark; 0: the accent in both). The module's init gives them to the
// core, which sends them to the host as the app mounts, so asuka dev's
// player shows each app's.
func appAccent(dir string) (accent uint64, flags uint8, dark uint64, err error) {
	if dir == "" || strings.HasPrefix(dir, "pkg:") {
		return 0, 0, 0, nil
	}
	b, err := os.ReadFile(filepath.Join(dir, "asuka.toml"))
	if err != nil {
		return 0, 0, 0, nil
	}
	var m struct {
		Accent     string `json:"accent"`
		AccentDark string `json:"accentDark"`
		Android    struct {
			DynamicColor bool `json:"dynamicColor"`
		} `json:"android"`
	}
	if toml.Unmarshal(b, &m) != nil {
		return 0, 0, 0, nil // the CLI reports the manifest's own errors
	}
	if m.Android.DynamicColor {
		flags = 1
	}
	if accent, err = wireColor("accent", m.Accent); err != nil {
		return 0, 0, 0, err
	}
	if dark, err = wireColor("accentDark", m.AccentDark); err != nil {
		return 0, 0, 0, err
	}
	return accent, flags, dark, nil
}

// wireColor is asuka.toml's color key ("#rrggbb" or "#rrggbbaa") as the
// wire's Color, ops.Color's RGBA (0 for "").
func wireColor(key, s string) (uint64, error) {
	if s == "" {
		return 0, nil
	}
	h := strings.TrimPrefix(s, "#")
	if len(h) == 6 {
		h += "ff"
	}
	v, err := strconv.ParseUint(h, 16, 32)
	if len(h) != 8 || err != nil {
		return 0, fmt.Errorf("asuka.toml: %s %q (want #rrggbb or #rrggbbaa)", key, s)
	}
	return 2<<32 | v, nil
}

// checkWebColors checks asuka.toml's [web.colors], the web host's
// palette, which the CLI writes into the page: each color is "#rrggbb"
// or "#rrggbbaa", as the accent is.
func checkWebColors(dir string) error {
	if dir == "" || strings.HasPrefix(dir, "pkg:") {
		return nil
	}
	b, err := os.ReadFile(filepath.Join(dir, "asuka.toml"))
	if err != nil {
		return nil
	}
	var m struct {
		Web struct {
			Colors map[string]string `json:"colors"`
		} `json:"web"`
	}
	if toml.Unmarshal(b, &m) != nil {
		return nil // the CLI reports the manifest's own errors
	}
	keys := make([]string, 0, len(m.Web.Colors))
	for k := range m.Web.Colors {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, k := range keys {
		v := m.Web.Colors[k]
		h := strings.TrimPrefix(v, "#")
		if _, err := strconv.ParseUint(h, 16, 32); (len(h) != 6 && len(h) != 8) || err != nil {
			return fmt.Errorf("asuka.toml: web.colors.%s %q (want #rrggbb or #rrggbbaa)", k, v)
		}
	}
	return nil
}

// nativeClass is what a package's platforms' code registers its native
// interfaces under, as internal/nativegen names it: asuka.toml's name,
// else its directory's.
func nativeClass(u *unit, disk *diskCache) string {
	if strings.HasPrefix(u.dir, "pkg:") {
		return u.path
	}
	if b, err := disk.readFile(filepath.Join(u.dir, "asuka.toml")); err == nil {
		var m struct {
			Name string `json:"name"`
		}
		if toml.Unmarshal(b, &m) == nil && m.Name != "" {
			return m.Name
		}
	}
	return filepath.Base(u.dir)
}
