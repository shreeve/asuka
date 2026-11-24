// A movable sheet's entrance and exit (src/nav.js's pop): from 95% of its
// size and transparent, 250 ms in and 150 out, its backdrop fading with it.
import { test } from "node:test";
import assert from "node:assert/strict";
import "../src/host.js"; // nav.js is loaded through it (they import each other)
import { pop } from "../src/nav.js";

function dialog() {
  const calls = [];
  return { calls, animate(frames, opts) { const a = { frames, opts }; calls.push(a); return a; } };
}

test("a movable sheet pops in from 95%, its backdrop fading in", () => {
  const d = dialog();
  const a = pop(d, true);
  assert.equal(d.calls.length, 2);
  const [backdrop, card] = d.calls;
  assert.equal(a, card);
  assert.deepEqual(card.frames, [{ opacity: 0, transform: "scale(0.95)" }, { opacity: 1, transform: "none" }]);
  assert.equal(card.opts.duration, 250);
  assert.equal(card.opts.easing, "cubic-bezier(0.22, 1, 0.36, 1)");
  assert.equal(card.opts.pseudoElement, undefined);
  assert.deepEqual(backdrop.frames, [{ opacity: 0 }, { opacity: 1 }]);
  assert.equal(backdrop.opts.pseudoElement, "::backdrop");
  assert.equal(backdrop.opts.duration, 250);
});

test("it pops out back to 95% in 150 ms and stays gone until it closes", () => {
  const d = dialog();
  const a = pop(d, false);
  const [backdrop, card] = d.calls;
  assert.equal(a, card);
  assert.deepEqual(card.frames, [{ opacity: 1, transform: "none" }, { opacity: 0, transform: "scale(0.95)" }]);
  assert.equal(card.opts.duration, 150);
  assert.equal(card.opts.fill, "forwards");
  assert.deepEqual(backdrop.frames, [{ opacity: 1 }, { opacity: 0 }]);
  assert.equal(backdrop.opts.fill, "forwards");
});
