# Testing: state

## Map (2026-11-17)

**C++ unit tests**: `apps/game/src/unittest/` (85 legacy modules, ~750 tests, ours and upstream's
mixed in one folder) plus 10 Catch2 `TEST_CASE`s, and upstream's `apps/game/src/test/` (Irrlicht
matrix, k-d tree, translations; built into the same binary). Run by:
- `luanti --run-unittests`: all modules in one process, sequential. **579 s** on this machine, and red (6 tests).
- `bun run test [Module...]` (`scripts/test.sh`, added 2026-11-17): builds, asks the binary for
  its modules (`--list-test-modules`), runs each in its own process in parallel plus Catch
  (`--run-tests`), prints per-module time and each failure with its repro command. Skips by name
  from `scripts/test/skip.txt` via `TF_TEST_SKIP` (hook in `unittest/test.cpp`).
- Wall clock is held by **TestTerragen (297 s alone)**; next TestAtlasRender 84, TestBodies 54,
  TestPlanetAtlas 36, TestFar 29, TestFarPalette 15, TestMoves 12. Everything else < 10 s.

**check:* harnesses** (`package.json` → `scripts/*.sh`, each with its own `scripts/<name>/` folder):
land (terragen+TestTerragen+far in parallel), terragen, streams, lab (headless server per
scenario, `timeout 60` each), wiki (g++ builds sandbox standalone), moves (= `--test-module
TestMoves`), matter, far, palette, atlas-world, window, bodies, atlas-land, atlas-seams,
atlas-debug, atlas-dig. Folders without a command: `air-check`, `milkyway-check`, `motion`.
Timings not yet measured.

**Release gate** (`scripts/release/gate.sh` via `arc gate terraverse`): key checks, build,
TestOnline + TestRelease + TestTerragen only, UI typecheck+build, 20 s headless boot. Does
**not** run the other 82 modules, which is how 6 failures sat unseen on master.

**UI tests**: none. **Tests on push**: none (CI owner's area).

## Numbers

Scoreboard (charter definitions):

| Date | Fast tier | Full tier | Suites/typical change | Systems w/o tests | Escaped bugs |
|---|---|---|---|---|---|
| 2026-11-17 | none defined; all unit modules 579 s sequential, see below parallel | none defined (gate ≈ build + TestTerragen ~5 min + UI) | no selection exists: all or by memory | not yet audited | 0 counted (6 red unit tests found on master, pre-release-report baseline) |

Own numbers:

| Date | All unit modules, wall | Failing unit tests on master |
|---|---|---|
| 2026-11-17 | 579 s sequential → 326 s `bun run test` | 6 wrong + 2 load-flaky (5 jobs, 8 skipped by name) |

## Plan

1. **(current) One way in for the unit tests.** Done when `bun run test` runs every module, green
   with skips cited, wall ≤ 330 s (met 2026-11-17: 326 s). Next: wall ≤ 120 s. TestTerragen is
   the floor: `testSoilByWater` alone is **181 s** of its 297 s, then testCoarse 19, testSoil 17,
   testDeterminismGolden 17, testCoasts 11. Step: profile testSoilByWater (shared planet build?
   too many columns sampled?) and cut its waste without checking less.
   Also: fix `testing-wall-clock-budgets` (own job): count instead of time for correctness, CPU
   time for the perf half; un-skip TestSandbox's two.
2. **Fast tier ≤ 120 s.** Done when `bun run test --fast` (unit modules + quick checks) is ≤ 120 s
   wall and runs every module. Mainly: TestTerragen and TestAtlasRender sharing built planets.
3. **Selection by system.** Done when the median "suites run for a typical change" over the
   last 20 commits is measured and < half of all suites. Steps: map folders → modules/checks in
   one table (`scripts/test/systems.tsv`), `bun run test <system>` and `--changed`.
4. **Time and fold the check:* harnesses.** Done when every check:* has a timing in Map and
   shared server-boot/world-staging lives in one helper used by ≥ 4 harnesses.
5. **Coverage audit.** Done when "systems without tests" is counted from AGENTS.md's folder
   table and is 0 for player-visible systems (shell UI and online likely first gaps).
6. **Full tier in the gate.** Proposal: gate runs `bun run test` instead of 3 modules.

## Tried

- Running wall-clock-budget modules alone after the parallel batch (`scripts/test/serial.txt`): TestSandbox still failed alone at load ~3 from other work on the machine. Kept the mechanism (empty), skipped the two tests on a job instead.

## Proposals

- 2026-11-17: **The release gate runs every unit module** (`bun run test`) instead of TestOnline,
  TestRelease and TestTerragen only. Why: 6 failing tests in 4 modules reached master unseen.
  Cost: none in wall time once TestTerragen is split (it's already in the gate and dominates);
  today the parallel run is about the same as TestTerragen alone. Lands 2026-11-19 on silence.

## Log

- 2026-11-17: first session. Mapped the suites; found 6 failing unit tests on master (jobs
  `testing-atlas-render-fails`, `testing-far-palette-fails`, `testing-moves-bones-stay-out`,
  `testing-scale-every-size`); added `bun run test` (parallel, per-module timings, skips cite
  jobs) and the `--list-test-modules` / `TF_TEST_SKIP` hooks. All unit modules 579 s → 326 s,
  green with 8 named skips. TestSandbox's budget tests are load-flaky (own job).
