# Testing: state

## Map (2026-11-21)

Entry points (`apps/game/package.json`; root `bun check`, `bun test:hot`, `bun test:game` forward):

| Command | Runner | What | Wall (this machine) |
|---|---|---|---|
| `bun test:hot [lib]` | `tests/run/library.sh hot` → ctest | pure libraries' quick doctest cases (terragen: 9) | **5 s** |
| `bun test:slow [lib]` | `library.sh slow` | goldens, Earth-sized worlds (terragen: 4) | **114 s** |
| `bun run test:engine [Module…]` | `tests/run/engine.sh` | 81 engine suites (80 modules + Catch) in parallel, skips from `skip.txt` | **47 s** idle, 72–78 s with the game running |
| `bun check` | `tests/run/check.sh` | hot + engine + UI typecheck: the **fast tier** | ≈ 55 s + typecheck |
| `bun check --changed [base]` | `test.sh --hot --changed` | the fast tier for what the change reaches | seconds to ≈ 80 s |
| `bun run test` (apps/game) | `tests/run/test.sh` | library all + engine: the **full tier**; **the gate runs it** since 2026-11-21 | ≈ 160 s idle; 222 s loaded |
| `bun run test <system\|path\|Module>` / `--changed [base]` | `test.sh` → `select/` | what the change reaches | physics 16 s |

- **Selection** (`tests/run/select/`, TS, 0.3 s): reads depfiles of every `<target>.dir` in
  `.output/build` (luanti, `<lib>_tests`; stale layouts skipped by missing source, newest wins).
  Unit = a pure library whole (folder under `apps/game/library` with its own CMakeLists), else
  the file's folder; test files are their own unit. Effects table in `select/index.ts`: data
  (`assets/`, `game/`, `registry/shaders|textures`) and `tests/run/*.sh` → every engine module;
  `library.sh` → libraries; a CMakeLists → suites under its folder, or none if its changed lines
  only list sources; root/cmake build files → everything; non-code/md/other apps → none.
  `test.sh` builds before selecting unless `BUILT=1`. Prints why each suite runs and "no suite is
  built against X" for changed code with no suite.
- Recall check (scratch script, not committed: see Tried): over 1500 commits of history with
  paths mapped through the two re-sorts (e7c5d319a, 2c0061283), every test module edited
  alongside code is picked from the code alone: 56/56.
- Library tests: `apps/game/library/<area>/<lib>/tests/*.cpp`, doctest, hot/slow labels, one
  binary per library, objects in `.output/build/src/<lib>/CMakeFiles/<lib>_tests.dir`.
  Heaviest slow: terragen.soil.slow 87 s, ranges.slow 19 s.
- Engine unit modules: `apps/game/tests/unit/` + Catch in `tests/engine/`. 3 tests skipped by name.
- check:* harnesses: `apps/game/tests/checks/*.sh` (~25). Not timed, not selected yet.
- `apps/game/tests/README.md`: layout, tiers, how to add a test.
- Profiling: `perf` unusable from the tool shell, gdb attach blocked; use timers.
- **Compiled code no unit suite is built against** (select over all of src/library, 2026-11-21),
  by folder: shell/host 41 files, output/sound 24, planetarium/globe 10, server/app/mods/sidecar 9,
  views/game/shadows 8, render/pipeline 8, server/world/run/rules 7, planetarium/space 6,
  render/fonts 6, app/network 5, app/menus 5, world/sim/water 3, server/players 3, … (27 folders).
  Some are covered by check:* harnesses (sidecar by check:mods); that's milestone 5's input.

## Numbers

Scoreboard (charter definitions). "Suites" = unit suites: 81 engine + 13 library ctest entries = 94.

| Date | Fast tier | Full tier | Suites/typical change | Systems w/o tests | Escaped bugs |
|---|---|---|---|---|---|
| 2026-11-17 | none defined | gate ≈ build + ~5 min tests + UI | no selection | not audited | 0 |
| 2026-11-19 | none defined (all modules 227 s) | gate ≈ build + 227 s + UI | no selection | not audited | 0 |
| 2026-11-20 | `bun check` ≈ 55 s + typecheck | `bun run test` ≈ 160 s | no selection: all | not audited | 0 |
| 2026-11-21 | `bun check` not re-timed; `--changed` seconds–80 s | 222 s loaded (game running), ≈ 160 s idle est. | **median 0 of 94** over last 20 (13 touch no code); **6 of 94** over the 7 that touch code | not audited (27 folders of uncompiled-against code listed) | 0 |

Own numbers:

| Date | Engine wall | Library slow wall | Skipped on master | Selection recall |
|---|---|---|---|---|
| 2026-11-17 | 326 s | (inside TestTerragen) | 8 | — |
| 2026-11-19 | 227 s | (inside TestTerragen) | 6 | — |
| 2026-11-20 | 47 s | 439 → 114 s | 3 | — |
| 2026-11-21 | 72–78 s loaded | 144 s loaded | 3 | 56/56 |

Code-touching commits over the last 300 (older ones' paths mapped): median 81 of 94, because
data (palette.json in most visual commits), `src/shared` and the runners run every engine module.

## Plan

1. ~~Unit tests one way in, ≤ 120 s.~~ Met 2026-11-20.
2. ~~Selection by system~~ (milestone 3, done 2026-11-21: median 0 of 94 < 47). Next refinement,
   as a step of 3 below: **data by loader**. A data file runs the suites reaching the units whose
   code names it in a string literal (`"palette.json"` in far/bake/baked.cpp, bridge/wiki.cpp,
   sandbox/stage); a folder loaded by scanning (assets/matter) stays wide. Done when code-touching
   commits' median over the last 100 is < 40 of 94 with recall still 56/56.
3. **(current) Full tier ≤ 120 s.** Done when `bun run test` is ≤ 120 s wall on an idle machine.
   Steps: re-time idle; ranges.slow (19 s); soil.slow is core-bound (465 s CPU): fewer `ground`
   reads that don't land in a bed or floodplain, or a cheaper read.
4. **Time and fold the check:* harnesses.** Done when every check:* has a timing in Map, shared
   server-boot/world-staging lives in one helper used by ≥ 4 harnesses, and select maps paths to
   checks (so --changed can say which check:* to run).
5. **Coverage audit.** Done when "systems without tests" is counted from AGENTS.md's folder
   table and is 0 for player-visible systems. Input: Map's uncovered list. Note AGENTS.md's folder
   table predates the 2026-11-20 re-sort (`shell/`, `far/`… are now under src/client/…).

## Tried

- Running wall-clock-budget modules alone after the parallel batch: still load-flaky; budgets
  live in `check:budgets`.
- perf/gdb sampling from the agent shell: see Map; use timers.
- Selection by include *closure* (folder graph or unit graph over all depfiles): every test
  reaches all 138 folders / 781 units; the engine's include graph is one tangle. Selection uses
  one level (the unit of each file a test compiles against) instead.
- Recall script lives in /tmp only (it needs the rename map from the two re-sort commits:
  `git show --name-status -M e7c5d319a 2c0061283`, chained). Rebuild it if recall needs
  re-measuring; worth committing as `select/main.ts --recall` if used again.

## Proposals

- (none open)

## Log

- 2026-11-21: selection landed (`bun run test <system>`, `--changed`, `bun check --changed`):
  median 0 of 94 suites over the last 20 commits, 6 of 94 over code-touching ones, recall 56/56.
  The gate ran engine.sh only, so terragen's tests and goldens hadn't been gated since
  2026-11-20; it runs the full tier now. tests/README.md. The developer's game was running:
  timings loaded, no engine rebuild.
- 2026-11-20: soil.slow on every core: slow tier 439 → 114 s, full ≈ 490 → 160 s. Job
  `testing-soil-steep-beds-never-sampled`.
- 2026-11-19: the gate runs every unit module (approved proposal). TestFarPalette flake fixed.
- 2026-11-17: first session. Mapped suites; 6 failing tests → jobs and named skips; added
  `bun run test` (parallel, per-module timings). 579 s → 326 s.
