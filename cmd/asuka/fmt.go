package main

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"strings"

	"github.com/egoist/asuka/lang/syntax"
)

// cmdFmt formats Asuka source (.asuka files) in its canonical form, as
// gofmt does Go's: the files and directories named (a directory's .asuka
// files, its subdirectories' too), or standard input without any.
func cmdFmt(args []string) error {
	var write, list, indented bool
	fs := newFlagSet("fmt", "[paths...]")
	fs.about = "Formats Asuka source: each file named, every .asuka file in each directory named (and in\n" +
		"their subdirectories), or standard input when none is. Prints the result unless -w or -l;\n" +
		"a file with syntax errors is left as it is, its errors reported as file:line:col: message."
	fs.Bool(&write, "write", 'w', "write the result to the file instead of printing it")
	fs.Bool(&list, "list", 'l', "list the files whose formatting differs")
	fs.Bool(&indented, "indented", 'i', "write blocks by indentation where they can be (rfcs/0018), braces where they can't")
	if err := fs.parse(args); err != nil {
		return err
	}
	return runFmt(fs.Args(), write, list, indented, os.Stdin, os.Stdout, os.Stderr)
}

// errFmt says that some files had syntax errors, which runFmt printed.
var errFmt = errors.New("fmt: some files have syntax errors")

func runFmt(paths []string, write, list, indented bool, stdin io.Reader, stdout, stderr io.Writer) error {
	if len(paths) == 0 {
		if write {
			return errors.New("fmt: -w needs files to write: name them, or . for this directory")
		}
		src, err := io.ReadAll(stdin)
		if err != nil {
			return err
		}
		return fmtReport(formatSource("<standard input>", src, nil, false, list, indented, stdout), stderr)
	}
	failed := false
	for _, path := range paths {
		info, err := os.Stat(path)
		if err != nil {
			return err
		}
		if !info.IsDir() {
			if err := fmtReport(formatFile(path, info.Mode(), write, list, indented, stdout), stderr); errors.Is(err, errFmt) {
				failed = true
			} else if err != nil {
				return err
			}
			continue
		}
		err = filepath.WalkDir(path, func(p string, d fs.DirEntry, err error) error {
			if err != nil {
				return err
			}
			name := d.Name()
			if d.IsDir() {
				if p != path && (strings.HasPrefix(name, ".") || name == "node_modules") {
					return filepath.SkipDir
				}
				return nil
			}
			if !strings.HasSuffix(name, ".asuka") || strings.HasPrefix(name, ".") {
				return nil
			}
			info, err := d.Info()
			if err != nil {
				return err
			}
			if err := fmtReport(formatFile(p, info.Mode(), write, list, indented, stdout), stderr); errors.Is(err, errFmt) {
				failed = true
				return nil
			} else {
				return err
			}
		})
		if err != nil {
			return err
		}
	}
	if failed {
		return errFmt
	}
	return nil
}

// fmtReport prints a file's syntax errors, one per line, and returns
// errFmt for them; other errors it returns as they are.
func fmtReport(err error, stderr io.Writer) error {
	var list syntax.ErrorList
	if errors.As(err, &list) {
		for _, e := range list {
			fmt.Fprintln(stderr, e)
		}
		return errFmt
	}
	return err
}

func formatFile(path string, mode fs.FileMode, write, list, indented bool, stdout io.Writer) error {
	src, err := os.ReadFile(path)
	if err != nil {
		return err
	}
	return formatSource(path, src, func(out []byte) error {
		return writeKeeping(path, src, out, mode.Perm())
	}, write, list, indented, stdout)
}

// writeKeeping writes out over path, whose contents were src, as gofmt
// -w does: in place, so a symlink, a hard link, the file's owner and
// mode stay what they were, with src first copied beside it, put back
// if the write fails, removed once it's done. A write that fails
// halfway (a full disk) leaves the file as it was, or its backup.
func writeKeeping(path string, src, out []byte, perm fs.FileMode) error {
	bak, err := backup(path, src, perm)
	if err != nil {
		return err
	}
	if err := os.WriteFile(path, out, perm); err != nil {
		if rerr := os.WriteFile(path, src, perm); rerr != nil {
			return fmt.Errorf("%w (the file as it was is in %s)", err, bak)
		}
		os.Remove(bak)
		return err
	}
	return os.Remove(bak)
}

// backup writes src to a new file beside path, named after it.
func backup(path string, src []byte, perm fs.FileMode) (string, error) {
	// The directory of the file a symlink names, so the backup is where
	// the write goes.
	dir := filepath.Dir(path)
	if real, err := filepath.EvalSymlinks(path); err == nil {
		dir = filepath.Dir(real)
	}
	f, err := os.CreateTemp(dir, filepath.Base(path)+".*.bak")
	if err != nil {
		return "", err
	}
	name := f.Name()
	_, err = f.Write(src)
	if err == nil {
		err = f.Chmod(perm)
	}
	if cerr := f.Close(); err == nil {
		err = cerr
	}
	if err != nil {
		os.Remove(name)
		return "", err
	}
	return name, nil
}

// formatSource formats one file's source: lists its name if it changes
// and list is set, writes it back with save if write is set, prints it
// otherwise.
func formatSource(name string, src []byte, save func([]byte) error, write, list, indented bool, stdout io.Writer) error {
	format := syntax.Format
	if indented {
		format = syntax.FormatIndented
	}
	out, err := format(name, src)
	if err != nil {
		return err
	}
	changed := !bytes.Equal(src, out)
	if list && changed {
		fmt.Fprintln(stdout, name)
	}
	if write {
		if changed {
			return save(out)
		}
		return nil
	}
	if !list {
		_, err = stdout.Write(out)
	}
	return err
}
