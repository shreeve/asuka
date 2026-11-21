//go:build e2e

package e2e

import (
	"encoding/json"
	"fmt"
	"testing"
)

// TestPickerWidthWeb runs e2e/testdata/picker-width in headless Chrome: a
// Picker sized to its items is as wide as its widest item, in a column
// that doesn't stretch it as in a row, not as narrow as its padding.
func TestPickerWidthWeb(t *testing.T) {
	browser := chrome()
	if browser == "" {
		t.Skip("no Chrome (ASUKA_CHROME)")
	}
	pageURL := webServer(t, "e2e/testdata/picker-width")
	c := launchCDP(t, browser)
	var created struct{ TargetID string }
	if err := c.call("", "Target.createTarget", map[string]any{"url": "about:blank"}, &created, ""); err != nil {
		t.Fatal(err)
	}
	page := c.attach(t, created.TargetID)
	var res json.RawMessage
	if err := c.call(page, "Page.navigate", map[string]any{"url": pageURL}, &res, ""); err != nil {
		t.Fatal(err)
	}
	s := &webScreen{t: t, url: pageURL}
	showsLine(t, s, "ready")

	var widths string
	var col, row, item float64
	until(t, func() bool {
		widths = c.eval(t, page, `(() => {
			const ps = [...document.querySelectorAll("select")];
			if (ps.length !== 2) return "";
			const span = document.createElement("span");
			span.style.font = getComputedStyle(ps[0]).font;
			span.textContent = "Couple pack for two";
			document.body.append(span);
			const item = span.getBoundingClientRect().width;
			span.remove();
			return ps.map((p) => p.getBoundingClientRect().width).concat(item).map(Math.round).join(" ");
		})()`)
		if widths == "" {
			return false
		}
		fmt.Sscan(widths, &col, &row, &item)
		return col > 0 && row > 0
	}, func() string { return "no pickers: " + widths })
	if col < item || row < item {
		t.Errorf("pickers %v (in a column) and %v (in a row) wide, narrower than their widest item (%v)", col, row, item)
	}
	if col != row {
		t.Errorf("a picker in a column is %v wide, in a row %v: want the same", col, row)
	}
}
