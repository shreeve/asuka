package mactest

import (
	"strings"
	"testing"

	"github.com/egoist/asuka/lang/compile"
	"github.com/egoist/asuka/lang/target"
)

// reloadThrowSrc throws one of the app's own errors, an enum case with a
// payload: freeing it reads its shape, which is the module's. label names
// the button, so a test can tell which module is running.
func reloadThrowSrc(label string) string {
	return `
		enum Oops: Error {
		  failed(code: int)
		}

		component App {
		  state n = 0
		  state out = ""
		  Column {
		    Text("out: {out}")
		    Button("` + label + `") {
		      on press {
		        n += 1
		        do {
		          throw Oops.failed(code: n)
		        } catch e {
		          out = "caught {n}"
		        }
		      }
		    }
		  }
		}
		app { App() }
	`
}

// An error thrown before a hot reload, of a type of the module the reload
// replaces, is let go before that module: asuka dev's player keeps the
// last error thrown for its trace, and the next one thrown after the
// reload released it, reading the freed module's shape.
func TestReloadAfterThrownError(t *testing.T) {
	a := Run(t, reloadThrowSrc("Throw"))
	a.Do("tap Throw")
	a.UntilText("out: caught 1")

	res := compile.Files(map[string][]byte{"app.asuka": []byte(Dedent(reloadThrowSrc("Throw again")))}, &compile.Options{Platform: target.MacOS})
	if err := res.Err(); err != nil {
		t.Fatalf("the reloaded app doesn't compile:\n%v", err)
	}
	a.srv.Send(target.MacOS, res.Module.Encode())
	a.Until(func(tr *Tree) bool { return tr.Find("Button", "Throw again") != nil })

	a.Do("tap Throw again")
	a.UntilText("out: caught 2")
	if a.Exited() {
		t.Fatalf("the player exited after a throw following the reload:\n%s", a.Logs())
	}
	if strings.Contains(a.Logs(), "error:") {
		t.Errorf("the player reported an error:\n%s", a.Logs())
	}
}
