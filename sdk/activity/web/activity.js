// platform/activity on the web: the document's own listeners, capturing
// and passive, so they see each event before the page's views do and
// never hold, stop nor prevent it: pointerdown (a mouse's button, a
// finger or a pen touching down), keydown, wheel, and input (a text edit,
// a software keyboard's and an input method's too). Times are Date.now()'s:
// the page's clock goes on while the computer sleeps.

let last = Date.now(); // when the last input was, in milliseconds
let told = 0; // when the listeners last heard of one
const listeners = new Set();
let watching = false;

function input() {
  const t = Date.now();
  last = t;
  if (listeners.size === 0 || t - told < 1000) return;
  told = t;
  for (const l of [...listeners]) l();
}

// Once for the page: every watcher (asuka dev's reloads make new ones)
// reads the same.
function watch() {
  if (watching || typeof document === "undefined") return;
  watching = true;
  const o = { capture: true, passive: true };
  for (const e of ["pointerdown", "keydown", "wheel", "input"]) document.addEventListener(e, input, o);
}

export function makeWatcher() {
  watch();
  return {
    idle: () => Math.max(Date.now() - last, 0),
    async watch(signal, input) {
      listeners.add(input);
      try {
        await new Promise((resolve) => signal.addEventListener("abort", resolve, { once: true }));
      } finally {
        listeners.delete(input);
      }
    },
  };
}
