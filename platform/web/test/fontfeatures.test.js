// asuka.toml's web.fontFeatures as the page's font-feature-settings
// (src/host.js, fontFeatures): the tags quoted, anything else left out.
import { test } from "node:test";
import assert from "node:assert/strict";
import { fontFeatures } from "../src/host.js";

test("OpenType tags become font-feature-settings; anything else is left out", () => {
  assert.equal(fontFeatures(["cv11", "ss01", "tnum"]), '"cv11", "ss01", "tnum"');
  assert.equal(fontFeatures(["tnum", "x\";}", "tabular"]), '"tnum"');
  assert.equal(fontFeatures([]), "");
});
