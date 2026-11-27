package ir

import (
	"bytes"
	"fmt"
	"hash/fnv"
	"os"
	"strings"

	bc "github.com/egoist/asuka/lang/bytecode"
	"github.com/egoist/asuka/lang/check"
	"github.com/egoist/asuka/lang/diag"
	"github.com/egoist/asuka/lang/syntax"
)

// A Lowerer lowers a checked program.
type Lowerer struct {
	lastNode syntax.Node // the last node a function lowered (fb.at), for an error in one that has none
	noFuse   bool        // seqfuse.go's loops are off
	prog     *check.Program
	info     *check.Info
	fset     *syntax.FileSet
	out      *Program
	mod      *bc.Module

	strs      map[string]int // a string → its index in the module's
	funcs     map[string]int // a function's instantiation → its index
	shapes    map[string]int // a type → its shape
	shapeType map[int]check.Type
	globals   map[*check.Global]int
	envShape  map[string]int
	memberIdx map[*check.Member]int
	// reloadKeys: each function loads and reloads name, its key in the
	// core's registry of loads (rfcs/0017): from 1, by declaration, so a
	// generic's instantiations and a store's instances share theirs.
	reloadKeys map[*check.Function]int
	enumNames  map[string]int // an int enum → the global holding its case names
	errIDs     map[int]int    // an error's shape → its type id
	// What the shapes' method tables hold for interfaces (implement):
	// the conversions made, which types each interface's values may be,
	// and the interfaces whose values another's are seen as (a type
	// parameter's, instantiated with an interface), by typeKey.
	implemented map[string]bool
	impls       map[string][]*check.Named
	flows       map[string][]*check.Named
	methodFrom  map[int]map[string]string // a shape's methods: the interface whose provided one it is
	work        []func()
	errs        []check.Diagnostic

	opts           *Options
	pendingStrings []pendingStrings
	consts         map[string]int // a hoisted literal (and its type) → its global
	pendingConsts  []pendingConst
	pendingNative  []pendingNative
	pendingExtern  []pendingExtern
	stringsFn      *fb
	catalog        int // the app's catalog's global, or -1
	// styleMemo: a style's local (check.StyleLocal) whose register, in
	// the application being lowered, holds a memo (style.go).
	styleMemo map[*check.Var]bool
	// variantIDs are the declared variants' ids (style.go's variantID).
	variantIDs map[*check.VariantDef]int64
	// cov counts statements' runs, with Options.Cover (cover.go).
	cov *coverer
	// ev is what the debugger's expressions lowered after the module
	// (eval.go): nil until the first.
	ev *evalState
}

// Options say how to lower.
type Options struct {
	// Accent, AccentFlags and AccentDark are the app's accent color (the
	// wire's Color, 0: the platform's), its flags and its color in dark
	// mode (0: Accent in both), which the app's init gives the core
	// (ui.set_accent).
	Accent      uint64
	AccentFlags uint8
	AccentDark  uint64
	// Lang and Direction force the language and the direction the app
	// shows for this run (asuka dev's --lang and --dir): the init says so
	// first (i18n.force), so that every platform's player follows.
	Lang, Direction string
	// Version is the project's (asuka.toml's version): std/flag's
	// --version.
	Version string
	// Platform is the platform the program is compiled for: what
	// @platform is, which decides the branches it folds (fold.go).
	// "": target.Default.
	Platform string
	// Debug is what @debug is: a development build's (asuka dev, run,
	// test), not a release's. It decides the branches it folds, as
	// @platform does.
	Debug bool
	// NoFuse: Seq pipelines and arrays' closure methods are the
	// runtime's calls, not loops (seqfuse.go).
	NoFuse bool
	// Cover names the files whose statements count their runs, for
	// asuka test --cover (cover.go); none: nothing is counted.
	Cover []string
	// Preview, when set, is the module's app: a preview's build, which
	// asuka dev shows in place of the app (preview.go).
	Preview *check.Preview
	// PreviewTests makes each of the root package's previews a test that
	// compares what it shows with its snapshot (asuka test --previews).
	PreviewTests bool
	// Locals records the names of each function's parameters and
	// locals, and the registers that hold them, for the debugger
	// (Func.Locals, bytecode.Local): a build the optimizer won't change
	// (asuka dev's).
	Locals bool
}

// Lower lowers prog (checked without errors) to a program.
func Lower(prog *check.Program, opts *Options) (*Program, []check.Diagnostic) {
	if opts == nil {
		opts = &Options{}
	}
	l := &Lowerer{
		prog: prog, info: prog.Info, fset: prog.Fset,
		mod:       &bc.Module{App: -1, Main: -1},
		funcs:     map[string]int{},
		shapes:    map[string]int{},
		shapeType: map[int]check.Type{},
		globals:   map[*check.Global]int{},
		envShape:  map[string]int{},
		memberIdx: map[*check.Member]int{},
		enumNames: map[string]int{},
		errIDs:    map[int]int{},

		implemented: map[string]bool{},
		impls:       map[string][]*check.Named{},
		flows:       map[string][]*check.Named{},
		methodFrom:  map[int]map[string]string{},
		catalog:     -1,
	}
	l.out = &Program{Mod: l.mod, l: l}
	l.opts = opts
	// ASUKA_OPT_SKIP=fuse: Seq pipelines and arrays' closure methods
	// lowered as the runtime's calls, not as loops (seqfuse.go): what the
	// tests compare the loops with.
	l.noFuse = opts.NoFuse || strings.Contains(os.Getenv("ASUKA_OPT_SKIP"), "fuse")
	if len(opts.Cover) > 0 {
		l.cov = newCoverer(l, opts.Cover)
	}
	l.lower()
	if l.cov != nil {
		l.cov.branches(l)
		l.out.Cover = l.cov.out
	}
	return l.out, l.errs
}

// errorf reports what the lowering can't do (yet) with the program.
func (l *Lowerer) errorf(n syntax.Node, code *diag.Code, format string, args ...any) {
	var pos syntax.Position
	if n != nil {
		pos = l.fset.Position(n.Range().Start)
	}
	l.errs = append(l.errs, check.NewDiagnostic(pos, pos, code, format, args...))
}

func (l *Lowerer) lower() {
	// The init function first: globals in dependency order.
	init := l.newFunc("init", "", 0, bc.KVoid)
	l.mod.Init = uint32(init.idx)
	l.initBody(init)
	root := l.prog.Root
	if pv := l.opts.Preview; pv != nil {
		l.lowerPreviewApp(pv)
	} else if root != nil && root.App != nil {
		app := l.newFunc("app", l.fileOf(root.App), 0, bc.KRef)
		l.mod.App = int32(app.idx)
		app.ctx = l.info.Ctxs[root.App]
		app.lowerApp(root.App)
	}
	if root != nil {
		for _, t := range root.Tests {
			l.lowerTest(t)
		}
		if l.opts.PreviewTests {
			for _, pv := range root.Previews {
				l.lowerPreviewTest(pv)
			}
		}
		if m := l.prog.Main; m != nil {
			l.mod.Main = int32(l.funcIndex(m, nil, nil))
		}
	}
	// What the constants and the native errors' table use (an error's
	// message method) is lowered in turn.
	for {
		for len(l.work) > 0 {
			w := l.work[0]
			l.work = l.work[1:]
			w()
		}
		if len(l.pendingConsts) > 0 {
			l.finishConsts()
			continue
		}
		if len(l.pendingNative) > 0 {
			l.finishNativeErrors()
			continue
		}
		break
	}
	l.finishExternTemplates()
	l.finishStrings()
}

func (l *Lowerer) fileOf(n syntax.Node) string {
	if n == nil {
		return ""
	}
	if f := l.fset.File(n.Range().Start); f != nil {
		return f.Name()
	}
	return ""
}

func (l *Lowerer) line(n syntax.Node) int32 {
	if n == nil {
		return 0
	}
	return int32(l.fset.Position(n.Range().Start).Line)
}

// newFunc adds a function and returns its builder.
func (l *Lowerer) newFunc(name, file string, nparams int, ret bc.Kind) *fb {
	f := &Func{Name: name, File: file, NParams: nparams, Ret: ret}
	l.out.Funcs = append(l.out.Funcs, f)
	return &fb{l: l, f: f, idx: len(l.out.Funcs) - 1, vars: map[*check.Var]int{}, unboxed: map[*check.Var]bool{}, free: map[bc.Kind][]int{},
		blockFree: map[string][]int{}, memberRegs: map[*check.Member]int{}, envReg: -1, ownerReg: -1, nullReg: -1}
}

// ---------------------------------------------------------------------
// Types

// arrKind is an array of t's elements' kind with how they're stored, as
// NEWA and arr.map take it (core/src/rt/rt.h's AK_ARR_KIND): a sized
// integer below 64 bits packed at its width, its ak_elem in bits 4 to 7.
func (l *Lowerer) arrKind(t check.Type) int {
	k := int(l.kind(t))
	if check.IsSized(t) {
		// ak_int_elem: i8 i16 i32 (i64) u8 u16 u32 (u64), by IntCode.
		k |= [8]int{2, 3, 4, 0, 5, 6, 7, 0}[check.IntCode(t)] << 4
	}
	return k
}

// kind is how values of t are stored.
func (l *Lowerer) kind(t check.Type) bc.Kind {
	switch x := t.(type) {
	case *check.Basic:
		switch x {
		case check.Int, check.Duration, check.Color, check.Length, check.Pointer, check.Char:
			return bc.KInt
		case check.I8, check.I16, check.I32, check.U8, check.U16, check.U32, check.U64:
			return bc.KInt // canonical: sign- or zero-extended, a u64's bits
		case check.Float:
			return bc.KFloat
		case check.Bool:
			return bc.KBool
		case check.Void, check.Never:
			return bc.KVoid
		}
		return bc.KRef
	case *check.Named:
		if x.Obj.Kind == check.EnumType && !x.Obj.RefEnum() {
			return bc.KInt
		}
	}
	return bc.KRef
}

// typeKey is a concrete type's name, unique in the program.
func (l *Lowerer) typeKey(t check.Type) string {
	switch x := t.(type) {
	case *check.Named:
		var b strings.Builder
		if p := x.Obj.Pkg; p != nil && p.Path != "" && !p.Prelude {
			b.WriteString(p.Path)
			b.WriteByte('.')
		}
		b.WriteString(x.Obj.Name())
		if len(x.Args) > 0 {
			b.WriteByte('<')
			for i, a := range x.Args {
				if i > 0 {
					b.WriteByte(',')
				}
				b.WriteString(l.typeKey(a))
			}
			b.WriteByte('>')
		}
		return b.String()
	case *check.Array:
		return "[" + l.typeKey(x.Elem) + "]"
	case *check.Map:
		return "[" + l.typeKey(x.Key) + ":" + l.typeKey(x.Elem) + "]"
	case *check.Set:
		return "Set<" + l.typeKey(x.Elem) + ">"
	case *check.Optional:
		return l.typeKey(x.Elem) + "?"
	case *check.Tuple:
		var parts []string
		for _, e := range x.Elems {
			parts = append(parts, l.typeKey(e))
		}
		return "(" + strings.Join(parts, ",") + ")"
	}
	if t == nil {
		return "?"
	}
	return t.String()
}

// isErrorStruct reports a struct declared : Error, which is stored as an
// enum of one case (see shape).
func isErrorStruct(t check.Type) bool {
	n, ok := t.(*check.Named)
	return ok && n.Obj.Kind == check.StructType && n.Obj.IsError
}

// caseBase is the index of an enum's first case in its shape: errors
// have a case 0 no value has, so that an error's ECASE is never 0 (a
// runtime error, a struct, has 0) and catch can tell them apart.
func caseBase(t check.Type) int {
	if n, ok := t.(*check.Named); ok && n.Obj.IsError {
		return 1
	}
	return 0
}

// shape returns the shape of a struct, a tuple, an enum stored as a
// reference, or a store; -1 for other types.
func (l *Lowerer) shape(t check.Type) int {
	key := l.typeKey(t)
	if i, ok := l.shapes[key]; ok {
		return i
	}
	switch x := t.(type) {
	case *check.Tuple:
		i := l.addShape(key, t)
		var fs []bc.Field
		for j, e := range x.Elems {
			fs = append(fs, l.field(itoa(j), e))
		}
		l.mod.Shapes[i].Fields = fs
		return i
	case *check.Named:
		s := x.Subst()
		switch x.Obj.Kind {
		case check.StructType:
			i := l.addShape(key, t)
			var fs []bc.Field
			for _, f := range x.Obj.Fields {
				fs = append(fs, l.field(f.Name, s.Apply(f.Type)))
			}
			if x.Obj.IsError {
				l.mod.Shapes[i].Cases = []bc.Case{{Name: "_"}, {Name: x.Obj.Name(), Fields: fs}}
				l.mod.Shapes[i].IsError = true
				l.errorMethods(i, x)
			} else {
				l.mod.Shapes[i].Fields = fs
			}
			l.ownHooks(i, x)
			return i
		case check.EnumType:
			if !x.Obj.RefEnum() {
				return -1
			}
			i := l.addShape(key, t)
			var cs []bc.Case
			if x.Obj.IsError {
				cs = append(cs, bc.Case{Name: "_"})
			}
			for _, k := range x.Obj.Cases {
				c := bc.Case{Name: k.Name}
				for j, p := range k.Params {
					name := k.Labels[j]
					if name == "" {
						name = itoa(j)
					}
					c.Fields = append(c.Fields, l.field(name, s.Apply(p)))
				}
				cs = append(cs, c)
			}
			l.mod.Shapes[i].Cases = cs
			l.mod.Shapes[i].IsError = x.Obj.IsError
			if x.Obj.IsError {
				l.errorMethods(i, x)
			}
			l.ownHooks(i, x)
			return i
		case check.StoreType:
			return l.envShapeOf(x.Obj, x)
		}
	}
	return -1
}

func (l *Lowerer) addShape(key string, t check.Type) int {
	l.mod.Shapes = append(l.mod.Shapes, bc.ShapeDef{Name: key})
	i := len(l.mod.Shapes) - 1
	l.shapes[key] = i
	l.shapeType[i] = t
	return i
}

// field is a shape's field: its name, with its JSON descriptor after a
// NUL and, after another, how the language writes it when that's
// otherwise (a duration, a color: showDesc; empty when it isn't, so the
// loader keeps the name as it is), its kind and its shape.
func (l *Lowerer) field(name string, t check.Type) bc.Field {
	d := l.desc(t)
	name += "\x00" + d + "\x00"
	if sd := l.showDesc(t); sd != d {
		name += sd
	}
	return bc.Field{Name: name, Kind: l.kind(t), Shape: int32(l.shape(check.Unopt(t)))}
}

// showDesc is t's JSON descriptor (rt/json.h), with a duration said as
// one ("d") and a color as one ("c") where they're not a struct's or an
// enum's (whose fields' names say theirs: field): how the language
// writes its values (core/src/vm/literal.c, a snapshot's test.show).
func (l *Lowerer) showDesc(t check.Type) string {
	switch x := t.(type) {
	case *check.Basic:
		switch x {
		case check.Duration:
			return "d"
		case check.Color:
			return "c"
		}
	case *check.Optional:
		return "?" + l.showDesc(x.Elem)
	case *check.Array:
		return "[" + l.showDesc(x.Elem)
	case *check.Set:
		return "<" + l.showDesc(x.Elem)
	case *check.Map:
		if x.Key != check.String {
			return "m" + l.showDesc(x.Key) + l.showDesc(x.Elem)
		}
		return "{" + l.showDesc(x.Elem)
	}
	return l.desc(t)
}

// desc is t's JSON descriptor (core/src/rt/json.h).
func (l *Lowerer) desc(t check.Type) string {
	switch x := t.(type) {
	case *check.Basic:
		switch x {
		case check.Int, check.Duration, check.Color, check.Length, check.Pointer:
			return "i"
		case check.Char:
			return "r" // a string of one code point
		case check.I8, check.I16, check.I32, check.U8, check.U16, check.U32, check.U64:
			// n and the type's code: n0 an i8 … n7 a u64.
			return "n" + string(rune('0'+check.IntCode(x)))
		case check.Float:
			return "f"
		case check.Bool:
			return "b"
		case check.String:
			return "s"
		case check.Bytes:
			return "y"
		}
		return "-"
	case *check.Optional:
		return "?" + l.desc(x.Elem)
	case *check.Array:
		return "[" + l.desc(x.Elem)
	case *check.Map:
		if x.Key != check.String {
			// Printed only: JSON's objects are from strings.
			return "m" + l.desc(x.Key) + l.desc(x.Elem)
		}
		return "{" + l.desc(x.Elem)
	case *check.Set:
		return "<" + l.desc(x.Elem)
	case *check.Tuple:
		// Printed only: (1,"a"), its elements by its shape's descriptors.
		l.shape(t)
		return "("
	case *check.Named:
		switch x.Obj.Kind {
		case check.StructType:
			if x.Obj.IsError {
				return "-"
			}
			l.shape(t)
			key := l.typeKey(t)
			if key == "std/bigint.BigInt" {
				// JSON and print read and write it with std/bigint's C
				// (json.c's ak_link_bigint): a program that only decodes
				// one links that C too.
				l.builtin("bigint.bigText")
			}
			return "S" + key + ";"
		case check.EnumType:
			if x.Obj.RefEnum() {
				l.shape(t)
				if check.IsJSONValue(t) {
					return "J" + l.typeKey(t) + ";" // any JSON, as it is
				}
				return "E" + l.typeKey(t) + ";"
			}
			l.intEnumShape(x)
			return "e" + l.typeKey(t) + ";"
		}
	}
	return "-"
}

// intEnumShape is the shape JSON names a payloadless enum's values by.
func (l *Lowerer) intEnumShape(x *check.Named) int {
	key := l.typeKey(x)
	if i, ok := l.shapes[key]; ok {
		return i
	}
	i := l.addShape(key, x)
	for _, k := range x.Obj.Cases {
		l.mod.Shapes[i].Cases = append(l.mod.Shapes[i].Cases, bc.Case{Name: k.Name})
	}
	l.cmpHook(i, x) // its compare, which orders a B-tree's keys of it
	return i
}

// errorMethods gives an error's shape the methods catch and .message
// call through CALLI: __etype (its type's id) and message.
func (l *Lowerer) errorMethods(shape int, t *check.Named) {
	id := len(l.errIDs) + 1
	l.errIDs[shape] = id
	if l.mod.Shapes[shape].Methods == nil {
		l.mod.Shapes[shape].Methods = map[string]uint32{}
	}
	et := l.newFunc(l.typeKey(t)+".__etype", "", 1, bc.KInt)
	et.f.Kinds = []bc.Kind{bc.KRef}
	r := et.newReg(bc.KInt)
	et.emit(bc.Byname["KSMALL"], r, id)
	et.emit(op("RET"), r)
	l.mod.Shapes[shape].Methods["__etype"] = uint32(et.idx)
	l.mod.Shapes[shape].Methods["message"] = uint32(l.messageFunc(t))
}

// messageFunc is an error type's message: its message field, its
// message method, or the error as the language writes it: its type's
// name, its case's and its payload (error.text, core/src/vm/literal.c),
// built as it's read. An enum none of whose cases has a payload, and a
// struct of no fields, is its names alone (an array of them, frozen).
func (l *Lowerer) messageFunc(t *check.Named) int {
	key := "message|" + l.typeKey(t)
	if i, ok := l.funcs[key]; ok {
		return i
	}
	if m := t.Obj.Methods["message"]; m != nil && m.Sig != nil && len(m.Sig.Params) == 0 && m.Sig.Result == check.String {
		i := l.funcIndex(m, nil, t)
		l.funcs[key] = i
		return i
	}
	b := l.newFunc(l.typeKey(t)+".message", "", 1, bc.KRef)
	l.funcs[key] = b.idx
	b.f.Kinds = []bc.Kind{bc.KRef}
	out := b.newReg(bc.KRef)
	if t.Obj.Kind == check.StructType {
		if f := t.Obj.Field("message"); f != nil && f.Type == check.String {
			b.emit(op("GETP"), out, 0, f.Index)
			b.emit(op("RET"), out)
			return b.idx
		}
		if len(t.Obj.Fields) == 0 {
			b.emit(op("KSTR"), out, l.str(t.Obj.Name()))
		} else {
			b.callBTo("error.text", out, 0)
		}
		b.emit(op("RET"), out)
		return b.idx
	}
	for _, k := range t.Obj.Cases {
		if len(k.Params) > 0 {
			// Type.case(payload)
			b.callBTo("error.text", out, 0)
			b.emit(op("RET"), out)
			return b.idx
		}
	}
	// Type.case
	c := b.newReg(bc.KInt)
	b.emit(op("ECASE"), c, 0)
	names := l.frozenStrings(func() []string {
		ns := []string{t.Obj.Name()}
		for _, k := range t.Obj.Cases {
			ns = append(ns, t.Obj.Name()+"."+k.Name)
		}
		return ns
	}(), "message."+l.typeKey(t))
	arr := b.newReg(bc.KRef)
	b.emit(op("GGET"), arr, names, int(bc.KRef))
	b.emit(op("IDX"), out, arr, c)
	b.emit(op("RET"), out)
	return b.idx
}

// frozenStrings is a global holding an array of strings, built in init
// and frozen.
func (l *Lowerer) frozenStrings(ss []string, key string) int {
	if g, ok := l.enumNames[key]; ok {
		return g
	}
	g := len(l.mod.Globals)
	l.mod.Globals = append(l.mod.Globals, bc.KRef)
	l.enumNames[key] = g
	l.pendingStrings = append(l.pendingStrings, pendingStrings{g, ss})
	return g
}

type pendingConst struct {
	global int
	e      syntax.Expr
	subst  check.Subst
	zero   check.Type // e nil: t's zero value (emptyConst)
}

type pendingStrings struct {
	global int
	ss     []string
}

// str interns s in the module's strings: by an index of them, as a
// program may have many.
func (l *Lowerer) str(s string) int {
	if l.strs == nil {
		l.strs = make(map[string]int, len(l.mod.Strings))
		for i, x := range l.mod.Strings {
			if _, ok := l.strs[x]; !ok {
				l.strs[x] = i
			}
		}
	}
	if i, ok := l.strs[s]; ok {
		return i
	}
	l.mod.Strings = append(l.mod.Strings, s)
	i := len(l.mod.Strings) - 1
	l.strs[s] = i
	return i
}

func (l *Lowerer) builtin(name string) int { return int(l.mod.Builtin(name)) }

func op(name string) bc.Op {
	o, ok := bc.Byname[name]
	if !ok {
		panic("ir: no opcode " + name)
	}
	return o
}

// ---------------------------------------------------------------------
// Functions

// funcKey names an instantiation of f.
func (l *Lowerer) funcKey(f *check.Function, targs []check.Type, recv check.Type) string {
	var b strings.Builder
	fmt.Fprintf(&b, "%p", f)
	if recv != nil {
		b.WriteString("|" + l.typeKey(recv))
	}
	for _, t := range targs {
		b.WriteString("|" + l.typeKey(t))
	}
	return b.String()
}

// funcIndex returns the index of f's instantiation with targs (and its
// receiver's type), lowering it once.
func (l *Lowerer) funcIndex(f *check.Function, targs []check.Type, recv check.Type) int {
	key := l.funcKey(f, targs, recv)
	if i, ok := l.funcs[key]; ok {
		return i
	}
	name := f.Name()
	if f.Recv != nil {
		if recv != nil {
			name = l.typeKey(recv) + "." + name
		} else {
			name = f.Recv.Name() + "." + name
		}
	} else if f.IsExtMethod() && recv != nil {
		name = l.typeKey(recv) + "." + name
	}
	if len(targs) > 0 {
		var ts []string
		for _, t := range targs {
			ts = append(ts, l.typeKey(t))
		}
		name += "<" + strings.Join(ts, ",") + ">"
	}
	s := providedSubst(f, targs, recv)
	if s == nil && f.IsExtMethod() {
		// An extension's: its type parameters are the receiver's.
		s = check.ExtSubst(f, recv)
	}
	if s == nil {
		s = check.Subst{}
		if n, ok := recv.(*check.Named); ok {
			for k, v := range n.Subst() {
				s[k] = v
			}
		}
		for i, tp := range f.TypeParams {
			if i < len(targs) {
				s[tp] = targs[i]
			}
		}
	}
	nparams := len(f.Sig.Params)
	if f.Recv != nil || f.IsExtMethod() {
		nparams++
	}
	b := l.newFunc(name, l.fileOf(f.Decl), nparams, l.kind(s.Apply(f.Sig.Result)))
	b.subst = s
	l.funcs[key] = b.idx
	l.work = append(l.work, func() { b.lowerFunc(f, recv) })
	return b.idx
}

// ---------------------------------------------------------------------
// Globals and init

// global returns a package-level value's slot.
func (l *Lowerer) global(g *check.Global) int {
	if i, ok := l.globals[g]; ok {
		return i
	}
	i := len(l.mod.Globals)
	k := bc.KRef
	if g.Kind == check.ConstGlobal || g.Kind == check.VarGlobal {
		k = l.kind(g.Type)
	}
	l.mod.Globals = append(l.mod.Globals, k)
	l.globals[g] = i
	return i
}

// initBody makes the module's init: each package's state, memos and
// constants, dependencies first.
func (l *Lowerer) initBody(b *fb) {
	// The case-name tables the lowering asks for as it goes are made by
	// a function init calls first; its body is written at the end.
	l.stringsFn = l.newFunc("init.strings", "", 0, bc.KVoid)
	// A preview's settings first: the language it shows is what the
	// catalog and the memos read.
	l.previewInit(b)
	// The catalog first: what init's memos say may read the language.
	if l.opts.Lang != "" || l.opts.Direction != "" {
		mark := b.mark()
		b.callBTo("i18n.force", bc.None, b.kstr(l.opts.Lang), b.kstr(l.opts.Direction))
		b.release(mark)
	}
	l.initCatalog(b)
	b.emit(op("CALL"), bc.None, l.stringsFn.idx, 0, 0)
	done := map[*check.Global]bool{}
	var visit func(g *check.Global)
	// In a spawn's worker (the web's: an instance of the core that runs
	// spawn's work and nothing else), only the file's values a spawn can
	// use are made: not state, memos, or what isn't a value (a database, a
	// native object), which the checker keeps out of spawn, nor what's
	// made from them. A file's database isn't opened again in each worker.
	mainOnly := map[*check.Global]bool{}
	var isMainOnly func(g *check.Global) bool
	isMainOnly = func(g *check.Global) bool {
		if v, ok := mainOnly[g]; ok {
			return v
		}
		mainOnly[g] = false // a cycle is the checker's error
		// A file's var a spawn never reaches (the checker's), but one a
		// let is made from is made in a worker as it is on the main
		// thread, so the let is the same there.
		v := g.Kind != check.ConstGlobal && g.Kind != check.VarGlobal || !check.Sendable(g.Type)
		for _, d := range g.Deps() {
			v = v || isMainOnly(d)
		}
		mainOnly[g] = v
		return v
	}
	worker := -1
	visit = func(g *check.Global) {
		if done[g] {
			return
		}
		done[g] = true
		for _, d := range g.Deps() {
			visit(d)
		}
		if g.Value == nil || !isMainOnly(g) {
			l.initGlobal(b, g)
			return
		}
		if worker < 0 {
			worker = b.local(bc.KBool)
			b.callBTo("async.in_worker", worker)
		}
		skip := b.f.NewLabel()
		b.emitJump(op("JT"), worker, skip)
		l.initGlobal(b, g)
		b.f.Place(skip)
	}
	// An app sets its accent first, always (0 when it has none), so a
	// player that ran another app drops that one's.
	if root := l.prog.Root; root != nil && root.App != nil {
		c, f, d := b.konst(int64(l.opts.Accent)), b.konst(int64(l.opts.AccentFlags)), b.konst(int64(l.opts.AccentDark))
		b.callB("ui.set_accent", bc.KVoid, c, f, d)
	}
	for _, p := range l.prog.Packages {
		for _, g := range p.Globals {
			visit(g)
		}
	}
	b.emit(op("RET0"))
}

// finishStrings writes init.strings: the frozen string tables (enums'
// case names) the lowering asked for.
// finishConsts makes the hoisted literals, in init.strings.
func (l *Lowerer) finishConsts() {
	b := l.stringsFn
	for len(l.pendingConsts) > 0 {
		p := l.pendingConsts[0]
		l.pendingConsts = l.pendingConsts[1:]
		mark := b.mark()
		b.subst = p.subst
		b.hoisting = true
		var v int
		if p.e == nil {
			v = b.zeroValue(p.zero)
		} else {
			v = b.expr(p.e, -1)
		}
		b.hoisting = false
		v = b.freeze(v, bc.KRef)
		b.emit(op("GSET"), p.global, v, int(bc.KRef))
		b.release(mark)
	}
}

func (l *Lowerer) finishStrings() {
	l.writeStrings(l.pendingStrings)
	l.stringsFn.emit(op("RET0"))
}

// writeStrings writes ps's tables into init.strings (l.stringsFn).
func (l *Lowerer) writeStrings(ps []pendingStrings) {
	b := l.stringsFn
	for _, p := range ps {
		mark := b.mark()
		// literalChunk of them from a block, the rest added one by one: a
		// table of any length (an enum's 70000 cases' names) takes as many
		// registers as one of literalChunk.
		first, rest := p.ss, []string(nil)
		if len(first) > literalChunk {
			first, rest = first[:literalChunk], first[literalChunk:]
		}
		blk := b.regs(repeatKind(bc.KRef, len(first)))
		for i, s := range first {
			b.emit(op("KSTR"), blk+i, l.str(s))
		}
		arr := b.temp(bc.KRef)
		b.emit(op("NEWA"), arr, int(bc.KRef), bc.NoShape, blk, len(first))
		b.releaseRegs(blk, repeatKind(bc.KRef, len(first)))
		for _, s := range rest {
			m := b.mark()
			b.appendReg(arr, b.kstr(s), "arr.push")
			b.release(m)
		}
		arr = b.freeze(arr, bc.KRef)
		b.emit(op("GSET"), p.global, arr, int(bc.KRef))
		b.release(mark)
	}
}

func repeatKind(k bc.Kind, n int) []bc.Kind {
	ks := make([]bc.Kind, n)
	for i := range ks {
		ks[i] = k
	}
	return ks
}

func (l *Lowerer) initGlobal(b *fb, g *check.Global) {
	if g.Value == nil {
		return
	}
	slot := l.global(g)
	b.ctx = g.Ctx
	b.line = l.line(g.Value)
	mark := b.mark()
	switch g.Kind {
	case check.StateGlobal:
		cell := b.temp(bc.KRef)
		b.state(cell, g.Decl, g.Value, g.Ctx, g.Type, g.Follows, g.Self, l.str(l.stateKey(g)))
		b.emit(op("GSET"), slot, cell, int(bc.KRef))
	case check.MemoGlobal:
		clos := b.thunk(g.Ctx, g.Value, g.Type)
		m := b.temp(bc.KRef)
		b.emit(op("MEMO"), m, clos, int(l.kind(g.Type)))
		b.emit(op("GSET"), slot, m, int(bc.KRef))
	case check.VarGlobal:
		// Not frozen: it's changed in place, as a local is.
		k := l.kind(g.Type)
		b.emit(op("GSET"), slot, b.valueReg(b.value(g.Value, -1), k), int(k))
	default:
		k := l.kind(g.Type)
		v := b.valueReg(b.value(g.Value, -1), k)
		if k == bc.KRef && !isObject(g.Type) {
			v = b.freeze(v, k)
		}
		b.emit(op("GSET"), slot, v, int(k))
	}
	b.release(mark)
}

func (l *Lowerer) stateKey(g *check.Global) string {
	p := g.Pkg.Path
	if p == "" {
		p = "main"
	}
	return p + "." + g.Name() + initKey(g.Value)
}

// initKey is what a state's key (CELLK's) says of its initial value: a
// hash of its expression as the formatter prints it ("#1a2b3c4d"), so
// that a reload starts a state whose initializer was edited again from
// the new one (React Fast Refresh does likewise for useState's), and
// keeps one whose initializer was only laid out differently.
func initKey(e syntax.Expr) string {
	if e == nil {
		return ""
	}
	var b bytes.Buffer
	syntax.Fprint(&b, nil, e)
	h := fnv.New32a()
	h.Write(b.Bytes())
	return fmt.Sprintf("#%08x", h.Sum32())
}

// ---------------------------------------------------------------------
// Tests

func (l *Lowerer) lowerTest(t *check.Test) {
	b, entry := l.newTest(t)
	l.mod.Tests = append(l.mod.Tests, entry)
	l.work = append(l.work, func() {
		b.f.Flags = b.f.Flags&bc.FSized | bc.FThrows
		if t.Ctx.Awaits {
			b.f.Flags |= bc.FAsync
		}
		b.testParams(t)
		// A user error that leaves the test fails it with its message:
		// rethrown as a runtime error, whose message the runner reads.
		start, end, target := b.f.NewLabel(), b.f.NewLabel(), b.f.NewLabel()
		b.f.Place(start)
		b.line = l.line(t.Node())
		b.testBody(t)
		b.f.Place(end)
		b.emit(op("RET0"))
		b.f.Place(target)
		e := b.newReg(bc.KRef)
		b.emit(op("CATCH"), e)
		user := b.f.NewLabel()
		c := b.newReg(bc.KInt)
		b.emit(op("ECASE"), c, e)
		b.emitJump(op("JT"), c, user)
		b.emit(op("THROW"), e)
		b.f.Place(user)
		m := b.newReg(bc.KRef)
		b.emit(op("CALLI"), m, l.str("message"), e, 1)
		blk := b.newReg(bc.KRef)
		b.emit(op("MOVR"), blk, m)
		b.emit(op("CALLB"), e, l.builtin("error"), blk, 1)
		b.emit(op("THROW"), e)
		b.f.Handlers = append(b.f.Handlers, Handler{Start: start, End: end, Target: target, Rethrows: true})
	})
}

// method is struct or enum n's method name: its own, or the extension's
// that gives it the method where it has an interface (check.ExtMethod).
func (l *Lowerer) method(n *check.Named, name string) *check.Function {
	if f := n.Obj.Methods[name]; f != nil {
		return f
	}
	return l.prog.ExtMethod(n, name)
}

// providedSubst is what an interface's provided method f is compiled
// with for recv, a type that has the interface (targs: the interface's
// type arguments): self is a recv. nil when f isn't one.
func providedSubst(f *check.Function, targs []check.Type, recv check.Type) check.Subst {
	if f.Recv == nil || f.Recv.Kind != check.InterfaceType || f.Recv.Iface == nil {
		return nil
	}
	s := check.Subst{f.Recv.Iface.Self: recv}
	for i, tp := range f.Recv.TypeParams {
		if i < len(targs) {
			s[tp] = targs[i]
		}
	}
	return s
}

// methodFor is the function n (a struct or an enum) answers interface
// in's method m with: its own (or an extension's), or the interface's provided one compiled
// for n when n has none of its signature (provided); -1 when it has
// neither.
func (l *Lowerer) methodFor(n, in *check.Named, m *check.IfaceMethod) (idx int, provided bool) {
	f := l.method(n, m.Name)
	if m.Provided == nil {
		if f == nil {
			return -1, false
		}
		return l.funcIndex(f, nil, n), false
	}
	if f != nil && f.Sig != nil && check.Identical(n.Subst().Apply(f.Sig.Type()), in.Subst().Apply(m.Sig.Type())) {
		return l.funcIndex(f, nil, n), false
	}
	return l.funcIndex(m.Provided, in.Args, n), true
}

// implement fills the method table of t's shape with what interface
// iface calls through CALLI: its methods (a provided one the type has
// none of compiled for it), and getters of its fields. A value of
// another interface converted to iface (a type parameter's) is a flow:
// each type the first's values may be gets iface's methods too. at is
// where the conversion is, for its errors.
func (l *Lowerer) implement(t, iface check.Type, at syntax.Node) {
	in, ok := iface.(*check.Named)
	if !ok || in.Obj.Iface == nil {
		return
	}
	if from, ok := t.(*check.Named); ok && from.Obj.Kind == check.InterfaceType {
		if !check.Identical(from, in) {
			l.flow(from, in, at)
		}
		return
	}
	n, ok := t.(*check.Named)
	if !ok {
		return
	}
	if n.Obj.Kind == check.EnumType && !n.Obj.RefEnum() {
		// A type parameter's, of an enum without payloads: an int.
		l.errorf(at, diag.NotSupported, "%s is stored as an int: declare the interface (enum %s: %s) to use it as one", n, n.Obj.Name(), in.Obj.Name())
		return
	}
	key := l.typeKey(n) + "|" + l.typeKey(in)
	if l.implemented[key] {
		return
	}
	l.implemented[key] = true
	shape := l.shape(n)
	if shape < 0 {
		return
	}
	if l.mod.Shapes[shape].Methods == nil {
		l.mod.Shapes[shape].Methods = map[string]uint32{}
	}
	for _, m := range in.Obj.Iface.Methods {
		f, provided := l.methodFor(n, in, m)
		if f < 0 {
			continue
		}
		sd := &l.mod.Shapes[shape]
		from := ""
		if provided {
			from = in.Obj.Name()
		}
		if old, done := sd.Methods[m.Name]; done {
			if int(old) != f {
				l.methodConflict(n, m.Name, l.methodFrom[shape][m.Name], from, in, at)
			}
			continue
		}
		sd.Methods[m.Name] = uint32(f)
		if from != "" {
			if l.methodFrom[shape] == nil {
				l.methodFrom[shape] = map[string]string{}
			}
			l.methodFrom[shape][m.Name] = from
		}
	}
	sd := &l.mod.Shapes[shape]
	for _, fl := range in.Obj.Iface.Fields {
		if _, done := sd.Methods[fl.Name]; done {
			continue
		}
		if f := l.method(n, fl.Name); f != nil && n.Obj.Kind == check.EnumType {
			sd.Methods[fl.Name] = uint32(l.funcIndex(f, nil, n))
			continue
		}
		f := n.Obj.Field(fl.Name)
		if f == nil {
			continue
		}
		ft := n.Subst().Apply(f.Type)
		g := l.newFunc(l.typeKey(n)+"."+fl.Name, "", 1, l.kind(ft))
		g.f.Kinds = []bc.Kind{bc.KRef}
		r := g.getField(0, n, f, -1)
		g.emit(op("RET"), r)
		sd.Methods[fl.Name] = uint32(g.idx)
	}
	ik := l.typeKey(in)
	l.impls[ik] = append(l.impls[ik], n)
	for _, to := range l.flows[ik] {
		l.implement(n, to, at)
	}
}

// flow says that values of interface from are seen as to's (a type
// parameter's, of one, called with to's methods): each type from's
// values may be, now and later, gets to's methods.
func (l *Lowerer) flow(from, to *check.Named, at syntax.Node) {
	fk := l.typeKey(from)
	for _, t := range l.flows[fk] {
		if check.Identical(t, to) {
			return
		}
	}
	l.flows[fk] = append(l.flows[fk], to)
	for _, n := range append([]*check.Named(nil), l.impls[fk]...) {
		l.implement(n, to, at)
	}
}

// methodConflict reports n's method name, which two interfaces it's
// converted to give it differently (a shape has one table: CALLI finds
// a method by its name): before and now are the interfaces whose
// provided ones they are ("" for n's own).
func (l *Lowerer) methodConflict(n *check.Named, name, before, now string, in *check.Named, at syntax.Node) {
	if before != "" && now != "" {
		l.errorf(at, diag.ExtensionConflict, "%s gets %s() from both %s and %s: declare %s's own %s()", n, name, before, now, n.Obj.Name(), name)
		return
	}
	l.errorf(at, diag.ExtensionConflict, "%s's own %s isn't of %s's %s()'s signature, and both are called through its interfaces: rename one", n, name, in.Obj.Name(), name)
}

// hoist returns a constant literal (an array, a map, a tuple, a struct or
// a case made of literals) from the global init builds it in once,
// frozen; ok false when e isn't one.
func (b *fb) hoist(e syntax.Expr, dst int) (int, bool) {
	l := b.l
	if b.hoisting || b == l.stringsFn || b.idx == int(l.mod.Init) || !l.constLiteral(e, true) {
		return 0, false
	}
	t := b.rawType(e)
	if t == nil || check.HasTypeParams(t) {
		return 0, false
	}
	key := fmt.Sprintf("%p|%s", e, l.typeKey(t))
	if l.consts == nil {
		l.consts = map[string]int{}
	}
	g, ok := l.consts[key]
	if !ok {
		g = len(l.mod.Globals)
		l.mod.Globals = append(l.mod.Globals, bc.KRef)
		l.consts[key] = g
		l.pendingConsts = append(l.pendingConsts, pendingConst{global: g, e: e, subst: b.subst})
	}
	r := b.out(dst, bc.KRef)
	b.emit(op("GGET"), r, g, int(bc.KRef))
	return r, true
}

// emptyConst is a frozen zero value of t (concrete: an empty array), a
// global init.strings makes once: what a builtin reads its elements'
// kind and shape from (a B-tree's entries, Extra "entryProto") without
// one made each call.
func (b *fb) emptyConst(t check.Type) int {
	l := b.l
	key := "zero|" + l.typeKey(t)
	if l.consts == nil {
		l.consts = map[string]int{}
	}
	g, ok := l.consts[key]
	if !ok {
		g = len(l.mod.Globals)
		l.mod.Globals = append(l.mod.Globals, bc.KRef)
		l.consts[key] = g
		l.pendingConsts = append(l.pendingConsts, pendingConst{global: g, zero: t})
	}
	r := b.temp(bc.KRef)
	b.emit(op("GGET"), r, g, int(bc.KRef))
	return r
}

// constLiteral reports a value made of literals only (top: a container
// of them, which is worth hoisting): an array, a set or a map of them
// (empty too), a tuple, a struct (of its defaults too) or a case.
func (l *Lowerer) constLiteral(e syntax.Expr, top bool) bool {
	switch x := e.(type) {
	case *syntax.ParenExpr:
		return l.constLiteral(x.X, top)
	case *syntax.BasicLit, *syntax.BoolLit, *syntax.NoneLit:
		return !top
	case *syntax.StringLit:
		return !top && len(x.Interps) == 0
	case *syntax.UnaryExpr:
		return !top && x.Op == syntax.SUB && l.constLiteral(x.X, false)
	case *syntax.ImplicitMember, *syntax.MemberExpr:
		if top {
			return false
		}
		sel := l.info.Selections[e]
		return sel != nil && (sel.Kind == check.SelCase || sel.Kind == check.SelColor || sel.Kind == check.SelIntConst || sel.Kind == check.SelLengthAuto)
	case *syntax.ArrayExpr:
		for _, el := range x.Elems {
			if !l.constLiteral(el, false) {
				return false
			}
		}
		return true
	case *syntax.MapExpr:
		for _, en := range x.Entries {
			if !l.constLiteral(en.Key, false) || !l.constLiteral(en.Value, false) {
				return false
			}
		}
		return true
	case *syntax.TupleExpr:
		for _, el := range x.Elems {
			if !l.constLiteral(el, false) {
				return false
			}
		}
		return len(x.Elems) > 0
	case *syntax.CallExpr:
		call := l.info.Calls[x]
		if call == nil || call.Kind != check.CallStruct && call.Kind != check.CallCase {
			return false
		}
		if n, ok := call.Type.Obj.Type.(*check.Named); ok && n.Obj.IsError {
			return false
		}
		for i, a := range call.Args {
			if a == nil {
				if d := call.Params[i].Default; d != nil && !l.constLiteral(d, false) {
					return false
				}
				continue
			}
			if !l.constLiteral(a, false) {
				return false
			}
		}
		return call.Kind == check.CallStruct || len(call.Args) > 0
	}
	return false
}

// isObject reports a type whose values are the runtime's objects, not
// values: a Ref (a cell), a Path (a builder), a Graphics. A constant of
// one isn't frozen: it changes.
func isObject(t check.Type) bool {
	switch check.Unopt(t) {
	case check.Ref, check.Path, check.Graphics:
		return true
	}
	return false
}
