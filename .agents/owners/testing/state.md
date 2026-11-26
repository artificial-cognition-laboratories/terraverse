# Testing: state

## Map (2026-11-26)

Entry points (`apps/game/package.json`; root `bun check`, `bun test:hot`, `bun test:game` forward):

| Command | Runner | What | Wall (this machine) |
|---|---|---|---|
| `bun test:hot [lib]` | `tests/run/library.sh hot` → ctest | pure libraries' quick doctest cases (terragen: 9) | **5 s** |
| `bun test:slow [lib]` | `library.sh slow` | goldens, Earth-sized worlds (terragen: 4) | **114 s** |
| `bun run test:engine [Module…]` | `tests/run/engine.sh` | 89 engine suites (88 modules + Catch) in parallel, skips from `skip.txt` | **54 s** idle alone; 86 s beside the libraries |
| `bun check` | `tests/run/check.sh` | hot + engine + UI typecheck: the **fast tier** | **69 s** idle (2026-11-25) |
| `bun check --changed [base]` | `test.sh --hot --changed` | the fast tier for what the change reaches | seconds to ≈ 80 s |
| `bun run test` (apps/game) | `tests/run/test.sh` | library all + engine: the **full tier**; **the gate runs it** since 2026-11-21 | **133 s** idle (2026-11-25): soil.slow is the critical path (88 s alone, 131 s sharing cores; the machine is CPU-bound, ≈ 1100 CPU-s) |
| `bun run test <system\|path\|Module>` / `--changed [base]` | `test.sh` → `select/` | what the change reaches | physics 16 s |

- **Selection** (`tests/run/select/`, TS, 0.3 s): reads depfiles of every `<target>.dir` in
  `.output/build` (luanti, `<lib>_tests`; stale layouts skipped by missing source, newest wins).
  Unit = a pure library whole (folder under `apps/game/library` with its own CMakeLists), else
  the file's folder (a library's `tools/` goes with its parent); test files are their own unit. Effects table in `select/index.ts`: `tests/run/*.sh` → every engine module;
  `library.sh` → libraries; a data file (`assets/`, `game/`, shaders, textures) → the suites
  reaching units whose code names its basename in a string literal, else its folder's name
  (`select/loaders.ts`), else every engine module; a CMakeLists → suites under its folder, or none if its changed lines
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
- check:* harnesses: `apps/game/tests/checks/*.sh` (~25). **None runs in the gate or CI.** The
  probe checks share `checks/lib/probe.sh` (build, throwaway world, server, verdict) since
  2026-11-26: matter, interact, atlas-world/land/seam/debug/dig, window, bodies (9).
  `select` names the checks a change reaches (`run/select/checks.ts`, path prefixes), stderr only.
  Timings on clean master after the build (2026-11-26): matter 7 s, interact 8 s, atlas-world 4 s,
  atlas-land 4 s, atlas-seam 21 s, atlas-debug 4 s, atlas-dig 24 s, window 65 s, bodies 5 s
  (≈ 140 s all, sequential). **Red on master:** atlas-world, atlas-seam, bodies (job
  `testing-atlas-sea-missing`); interact flaked 1 in 16 under load. Not yet timed: terragen, land,
  far, wiki, moves, mods, dev, serve (display/network), ui, registry.
- Clean measurements: the shared tree may hold the developer's WIP (2026-11-26: a sidecar protocol
  bump made every probe refuse). Use a worktree: `git worktree add /tmp/tv-master HEAD`, `cmake -S
  apps/game -B .output/build -DRUN_IN_PLACE=TRUE -DCMAKE_BUILD_TYPE=Release` (CEF download ≈ 10 min,
  configure 170 s), build -j6, `bun install`, `bun run stage`, `cd apps/cli && bun run build`.
- `apps/game/tests/README.md`: layout, tiers, how to add a test.
- Profiling: `perf` unusable from the tool shell, gdb attach blocked; use timers.
- **Coverage by system** (2026-11-25, `select` over each folder of AGENTS.md's table, at today's
  paths): every row has a unit suite or a check. Thin ones: `shell/` (TestSystem + typecheck; its
  `host/` only by `check:ui`, which needs a display), `sidecar/` (only `check:mods`), `planetarium/`
  (TestPlanetarium; `globe/` none), `probe/` (is the checks). `milkyway/` → TestMilkyway and
  `views/figure/` → TestFigure, added 2026-11-25.
- Where the full tier's CPU goes: Streams' cold reads at Earth-ish sizes. soil.slow 638 CPU-s (first
  read per place 400 of it); TestBodies 31 of 32 s is 400 home-body reads at 628 320 round. Job
  `terragen-streams-cold-read-earth` (terragen's code, not ours).
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
| 2026-11-24 | not re-timed (game running) | not re-timed | **median 0 of 99** over last 100; **86 of 99** over the 34 touching code or data (`--measure` prints both now) | not audited | 0 |
| 2026-11-25 | `bun check` **69 s** | `bun run test` **133 s** idle | median 0 of 102 over last 20; 12.5 of 102 over the 6 touching code or data | **2 → 0** (milkyway, views/figure: now TestMilkyway, TestFigure) | 0 |
| 2026-11-26 | not re-timed (developer's game running) | not re-timed | not re-measured | 0 | 0 (3 probe checks red on master, not yet tied to a release) |

Own numbers:

| Date | Engine wall | Library slow wall | Skipped on master | Selection recall |
|---|---|---|---|---|
| 2026-11-17 | 326 s | (inside TestTerragen) | 8 | — |
| 2026-11-19 | 227 s | (inside TestTerragen) | 6 | — |
| 2026-11-20 | 47 s | 439 → 114 s | 3 | — |
| 2026-11-21 | 72–78 s loaded | 144 s loaded | 3 | 56/56 |
| 2026-11-25 | 54 s idle (86 s beside libs) | 131 s beside engine | 2 | — |

Code-touching commits over the last 300 (older ones' paths mapped): median 81 of 94, because
data (palette.json in most visual commits), `src/shared` and the runners run every engine module.

## Plan

1. ~~Unit tests one way in, ≤ 120 s.~~ Met 2026-11-20.
2. ~~Selection by system~~ (done 2026-11-21). ~~Data by loader~~ landed 2026-11-24 (cuts.json
   86 → 1 suite, palette.json 86 → 5, textures 86 → 8) but the code-touching median stayed 86:
   what's wide is `src/shared/base` (gameparams.h, porting.cpp reach 79/44 suites, honestly)
   and pre-re-sort paths that no longer exist (unknown → wide). The < 40 target was the wrong
   lever; dropped. Re-measure over 100 once ~60 post-re-sort commits exist.
3. **Full tier ≤ 120 s** (blocked, 3 sessions). Done when `bun run test` is ≤ 120 s wall idle
   (133 s 2026-11-25). The work left is cold Streams reads (job `terragen-streams-cold-read-earth`),
   terragen's code. Not retrying here; re-time when that job lands. Our own lever if it stalls: none
   that doesn't check less (the machine is CPU-bound).
4. **(current) Time and fold the check:* harnesses.** Done when every check:* has a timing in Map,
   shared server-boot/world-staging lives in one helper used by ≥ 4 harnesses, and select maps paths
   to checks. (b) and (c) done 2026-11-26 (9 harnesses on lib/probe.sh; checks.ts). Left: (a) time
   terragen, land, far, wiki, moves, mods; fold far.sh/terragen.sh onto the helper.
6. **Probe checks in the gate.** Done when the gate runs every headless probe check that's green
   (≈ 2.5 min sequential; parallel by port, less). Steps: a `bun run test:checks` running them in
   parallel with a summary; a red one skipped by name citing its job, as skip.txt does; then the
   proposal to the gate (CI's boundary for the workflow). Motive: atlas-world/seam/bodies broke
   unseen because nothing runs them.
5. ~~Coverage audit~~: systems without tests 0 (2026-11-25). Keep it there: a new folder in
   AGENTS.md's table gets a row in Map's coverage line. Deepen the thin ones (`planetarium/globe`,
   `shell/host`) when work reaches them.

## Tried

- Running wall-clock-budget modules alone after the parallel batch: still load-flaky; budgets
  live in `check:budgets`.
- perf/gdb sampling from the agent shell: see Map; use timers.
- Selection by include *closure* (folder graph or unit graph over all depfiles): every test
  reaches all 138 folders / 781 units; the engine's include graph is one tangle. Selection uses
  one level (the unit of each file a test compiles against) instead.
- Data by loader (2026-11-24): naming a file's folder too (`"world"`, `"matter"`) matches far too
  much; folder names are only a fallback when nothing names the file.
- Recall script lives in /tmp only (it needs the rename map from the two re-sort commits:
  `git show --name-status -M e7c5d319a 2c0061283`, chained). Rebuild it if recall needs
  re-measuring; worth committing as `select/main.ts --recall` if used again.

- Fewer reads in soil.slow or TestBodies (2026-11-25): the cost is the first read at each new
  place, not the count, and fewer places checks less. Not a lever.

## Proposals

- (none open; milestone 6 will need one: the gate runs the probe checks)

## Log

- 2026-11-26: every probe check timed on a clean master build (worktree; the shared tree's WIP
  broke the sidecar). 3 red on master, unseen because nothing runs them: job
  `testing-atlas-sea-missing` (not the `perf` commit: reverted, still red). 9 harnesses onto
  `checks/lib/probe.sh` (≈ 180 lines of copies → one 80-line helper), same verdicts before/after,
  no-verdict path checked. `select` names the check:* a change reaches. Scoreboard: no number moved
  (escaped bugs 0: the breaks are unreleased-unknown, not found after a release).

- 2026-11-25: coverage audit: systems without tests 2 → 0 (TestMilkyway, TestFigure, each proven
  by deliberate breaks). Library tools/ select with their library (milkyway/tools 87 → 1 suite).
  Idle timings: fast 69 s, full 133 s. Milestone 3 blocked on terragen's streams (3 sessions):
  switched to the audit; TestBodies' 31 s added to that job.
- 2026-11-24: data files select by the code naming them (`select/loaders.ts`); `--measure`
  reports the code-or-data median. Typical-change median unchanged (86 of 99 over code-touching
  commits): shared/ headers dominate. Game running: no timings, no engine build.
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
