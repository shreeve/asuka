// platform/activity's web code (sdk/activity/web/activity.js): the
// document's capturing, passive listeners note the user's input; idle is
// the time since the last; watch hears inputs at most once a second, and
// stops as its task is cancelled: node --test platform/web.
import { test } from "node:test";
import assert from "node:assert/strict";
import * as native from "../src/native.js";

globalThis.asuka = { native };

let now = 1_000_000;
Date.now = () => now;
const added = {};
globalThis.document = {
  addEventListener(kind, f, o) { (added[kind] ??= []).push({ f, o }); },
  removeEventListener() {},
};
const { makeWatcher } = await import("../../../sdk/activity/web/activity.js");
await import("../../../sdk/activity/web/activity.asuka.js");

const fire = (kind) => { for (const l of added[kind] ?? []) l.f({ type: kind }); };

test("the document's listeners capture, passive, once for the page", () => {
  makeWatcher();
  makeWatcher();
  assert.deepEqual(Object.keys(added).sort(), ["input", "keydown", "pointerdown", "wheel"]);
  for (const ls of Object.values(added)) {
    assert.equal(ls.length, 1);
    assert.deepEqual(ls[0].o, { capture: true, passive: true });
  }
  assert.equal(added.pointermove, undefined); // a pointer moving isn't input
});

test("idle is the time since the last input, of any kind", () => {
  const w = makeWatcher();
  now += 5000;
  assert.equal(w.idle(), 5000);
  for (const kind of ["pointerdown", "keydown", "wheel", "input"]) {
    fire(kind);
    assert.equal(w.idle(), 0);
    now += 250;
    assert.equal(w.idle(), 250);
  }
});

test("watch hears inputs at most once a second, until cancelled", async () => {
  const w = makeWatcher();
  const stop = new AbortController();
  let heard = 0;
  now += 10_000;
  const done = w.watch(stop.signal, () => { heard += 1; });
  fire("pointerdown");
  assert.equal(heard, 1);
  now += 500;
  fire("keydown");
  assert.equal(heard, 1); // within the second
  assert.equal(w.idle(), 0); // but noted
  now += 600;
  fire("input");
  assert.equal(heard, 2);
  stop.abort();
  await done;
  now += 2000;
  fire("wheel");
  assert.equal(heard, 2);
});
