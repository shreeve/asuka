# RFCs

Proposals for changes big enough to want a design before code: what
they'd change, why, how, and what's still open. DESIGN.md stays the
source of truth for what Asuka *is*; an RFC that's accepted and built
moves its design there, and the RFC keeps the reasoning.

Each RFC is a file `NNNN-short-name.md` with a header:

- **Status**: Draft, Accepted, Built (with the commit or PR), Rejected,
  or Withdrawn.
- **Created**, and the RFCs or PRs it relates to.

## Index

| RFC | Title | Status |
|---|---|---|
| [0001](0001-keep-native-views-across-reloads.md) | Keep the player's native views across reloads | Draft |
| [0002](0002-restore-navigation-across-reloads.md) | Restore navigation across reloads | Draft |
| [0003](0003-in-process-code-patching.md) | In-process code patching and an interpreted app package | Rejected |
| [0004](0004-asuka-test.md) | asuka test: the same UI tests on the platforms' real views | Draft |
| [0005](0005-devtools.md) | Devtools: debugging, inspecting and profiling an app | Draft |
| [0006](0006-over-the-air-updates.md) | Over-the-air updates on phones | Built |
| [0007](0007-c-core-and-asuka-language.md) | A C core and the Asuka language | Accepted |
| [0008](0008-no-go-at-run-time-and-c-ffi.md) | No Go at run time, and C from Asuka | Accepted |
| [0009](0009-styles.md) | Composable styles | Built |
| [0010](0010-headless-components.md) | Headless components | Built |
| [0011](0011-everyday-language.md) | The everyday language: `mut fn`, type tests, extensions, iteration, workers | Built |
| [0012](0012-reactivity-said-and-checked.md) | What the UI follows, said and checked: following states, `peek`, effects' dependencies, keys | Built |
| [0013](0013-per-path-tracking.md) | Per-path tracking: reads follow their path, writes notify theirs | Built |
| [0014](0014-loads-hold-what-they-show.md) | Loads hold what they show: `.pending` only before the first answer, `loading` | Built |
| [0015](0015-diagnostics-with-codes.md) | Diagnostics with codes, for people and agents: one table, `asuka explain`, run-time detectors, `AGENTS.md` | Built |
| [0016](0016-cleanup.md) | `cleanup { }`: what undoes an effect's run, or goes with its owner | Built |
| [0017](0017-actions-and-grouped-loads.md) | Changes shown at once, and loads that land together: `optimistic`, `reload f`, `load (a(), b())` | Built |
| [0018](0018-indentation-blocks.md) | Blocks by indentation: a deeper line opens a block, braces stay for one line | Draft |

## Background: hot reload today

`asuka dev` (DESIGN.md, docs/development.md) rebuilds the app's Go code
on a save and runs it again: in a player on a phone, in its own window
on a desktop, in the page on the web. The next run starts behind the
last (`internal/devstate`'s `Wait`), and the app's signals come back by
names read from the source (`signal.KeepPlaces`). What a reload still
loses:

- Native view state: scroll positions, focus and the caret, a playing
  video, a web view's page, a map's camera, the keyboard. The player
  clears the last session's views and the new run mounts from scratch
  (0001).
- Navigation: screens are closures, so a reload starts at the root
  screen (0002).

Both came out of a survey of recent hot reload work (React Fast
Refresh, Flutter's stateful hot reload on the web, Compose Hot Reload,
Dioxus's Subsecond, InjectionNext), in which keeping the existing UI
tree, rather than tearing it down, is what makes a reload feel
seamless. 0003 records why patching the running process, the other
direction that survey points to, doesn't fit Asuka.
