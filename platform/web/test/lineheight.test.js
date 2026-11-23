// A Font's lineHeight on the web (src/text.js, fontOf): points as a
// multiple of the size, so it scales with the text; 0 keeps the style's.
import { test } from "node:test";
import assert from "node:assert/strict";
import { fontOf } from "../src/text.js";

const h = { fontFiles: () => [] };

test("a font's line height in points is its multiple of the size", () => {
  assert.equal(fontOf(h, { style: 0, size: 11, weight: 0, family: "", lineHeight: 14 })[2], 14 / 11);
  assert.equal(fontOf(h, { style: 0, size: 15, weight: 0, family: "", lineHeight: 20 })[2], 20 / 15);
});

test("no line height keeps the text style's", () => {
  assert.equal(fontOf(h, { style: 0, size: 13, weight: 0, family: "", lineHeight: 0 })[2], 1.375);
  assert.equal(fontOf(h, { style: 0, size: 13, weight: 0, family: "" })[2], 1.375);
});
