//go:build e2e

package e2e

import (
	"html"
	"net/http"
	"net/http/httptest"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

// TestStdWebViews runs the standard library's and the platform SDK's web
// view tests: each {std,sdk}/<name>/tests/*_view_test.js, a module the page imports (served
// from the checkout, so it imports its package's web/ as an app does),
// in headless Chrome, which prints the page once the test wrote its
// result into #result: "PASS ..." or "FAIL" and why.
func TestStdWebViews(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	r := root(t)
	tests, _ := filepath.Glob(filepath.Join(r, "std", "*", "tests", "*_view_test.js"))
	sdk, _ := filepath.Glob(filepath.Join(r, "sdk", "*", "tests", "*_view_test.js"))
	tests = append(tests, sdk...)
	if len(tests) == 0 {
		t.Skip("no package has web view tests ({std,sdk}/*/tests/*_view_test.js)")
	}
	files := http.FileServer(http.Dir(r))
	for _, test := range tests {
		rel, _ := filepath.Rel(r, test)
		t.Run(strings.TrimSuffix(filepath.ToSlash(rel), ".js"), func(t *testing.T) {
			mux := http.NewServeMux()
			mux.HandleFunc("/test.html", func(w http.ResponseWriter, req *http.Request) {
				w.Header().Set("Content-Type", "text/html")
				w.Write([]byte(`<!doctype html><meta charset="utf-8"><body><pre id="result">RUNNING</pre>` +
					`<script type="module" src="/` + filepath.ToSlash(rel) + `"></script>`))
			})
			mux.Handle("/", files)
			srv := httptest.NewServer(mux)
			defer srv.Close()
			out, err := exec.Command(browser, "--headless=new", "--no-first-run", "--no-default-browser-check", "--disable-gpu",
				"--user-data-dir="+t.TempDir(), "--virtual-time-budget=20000", "--dump-dom", srv.URL+"/test.html").Output()
			if err != nil {
				t.Fatalf("%v\n%s", err, out)
			}
			m := regexp.MustCompile(`(?s)<pre id="result">(.*?)</pre>`).FindStringSubmatch(string(out))
			if m == nil {
				t.Fatalf("no result in:\n%s", out)
			}
			result := html.UnescapeString(m[1])
			t.Log(result)
			if !strings.HasPrefix(result, "PASS") {
				t.Fatal(result)
			}
		})
	}
}
