# Testing: state

## Map (2026-11-20, after the developer's re-sort into apps/game/{src,library,tests,scripts})

Entry points (`apps/game/package.json`; root `bun check`, `bun test:hot`, `bun test:game` forward):

| Command | Runner | What | Wall (this machine) |
|---|---|---|---|
| `bun test:hot [lib]` | `tests/run/library.sh hot` → ctest | pure libraries' quick doctest cases (terragen: 9) | **5 s** |
| `bun test:slow [lib]` | `library.sh slow` | goldens, Earth-sized worlds (terragen: 4) | **114 s** (was 439 s; soil.slow 439 → 87 s) |
| `bun run test:engine` | `tests/run/engine.sh` | 81 engine unit modules in parallel + Catch, skips from `tests/run/skip.txt` | **47 s** |
| `bun check` | `tests/run/check.sh` | hot + engine + UI typecheck: the **fast tier** | ≈ 55 s + typecheck |
| `bun run test` (apps/game) | library all + engine | the **full tier** (the gate runs engine.sh) | ≈ 160 s |

- Library tests: `apps/game/library/<area>/<lib>/tests/*.cpp`, doctest, suites labelled hot/slow
  (`* doctest::test_suite("slow")`), one binary per library (`.output/tests/terragen_tests`),
  ctest entries per source file. `TEST_NOTES=1` prints each test's `note()` line.
  Heaviest slow: terragen.soil.slow 87 s (100 river windows × 400 `World::ground` reads at
  Earth size, ~6 ms each, now on every core), ranges.slow 19 s.
- Engine unit modules: `apps/game/tests/unit/` (legacy Luanti framework + ours). 3 tests
  skipped by name (skip.txt, each cites a job). Budgets held by `bun check:budgets`.
- check:* harnesses: `apps/game/tests/checks/*.sh` (land, terragen, oracle, wiki, budgets,
  moves, matter, interact, far, mods, dev, registry, palette, atlas-*, bodies, window, ui,
  perf-gen). Not timed yet.
- Profiling here: `perf` needs `-e cpu-clock:u` (paranoid 2) and still wrote empty files from
  the tool shell; gdb attach is blocked (ptrace_scope 1). Instrument with timers instead.

**UI tests**: `check:ui` exists (not yet looked at). **Tests on push**: CI owner's.

## Numbers

Scoreboard (charter definitions):

| Date | Fast tier | Full tier | Suites/typical change | Systems w/o tests | Escaped bugs |
|---|---|---|---|---|---|
| 2026-11-17 | none defined | gate ≈ build + ~5 min tests + UI | no selection | not audited | 0 |
| 2026-11-19 | none defined (all modules 227 s) | gate ≈ build + 227 s + UI | no selection | not audited | 0 |
| 2026-11-20 | `bun check` ≈ 55 s + typecheck (hot 5 s, engine 47 s) | `bun run test` ≈ 160 s (slow 114 + engine 47; was ≈ 490) | no selection: all | not audited | 0 |

Own numbers:

| Date | Engine modules wall | Library slow wall | Failing tests on master |
|---|---|---|---|
| 2026-11-17 | 326 s | (inside TestTerragen) | 8 skipped |
| 2026-11-19 | 227 s | (inside TestTerragen) | 6 skipped |
| 2026-11-20 | 47 s (terragen moved out by the developer) | 439 → 114 s | 3 skipped |

## Plan

1. ~~Unit tests one way in, ≤ 120 s.~~ Met 2026-11-20: fast tier ≈ 55 s, full ≈ 160 s.
2. **(current) Full tier ≤ 120 s.** Done when `bun run test` (apps/game) is ≤ 120 s wall.
   Steps: ranges.slow (19 s) and the next slowest engine modules (re-list with engine.sh);
   soil.slow is now bounded by cores (465 s CPU): next lever is fewer `ground` reads that
   don't land in a bed or floodplain (most of the 40 000 are discarded), or a cheaper read.
3. **Selection by system.** Done when the median "suites run for a typical change" over the
   last 20 commits is measured and < half of all suites. Steps: one table folder → suites
   (`tests/run/systems.tsv`), `bun run test <system>` and `--changed`.
4. **Time and fold the check:* harnesses.** Done when every check:* has a timing in Map and
   shared server-boot/world-staging lives in one helper used by ≥ 4 harnesses.
5. **Coverage audit.** Done when "systems without tests" is counted from AGENTS.md's folder
   table and is 0 for player-visible systems.

## Tried

- Running wall-clock-budget modules alone after the parallel batch: still load-flaky; budgets
  now live in `check:budgets` (someone else's).
- perf/gdb sampling from the agent shell: see Map; use timers.

## Proposals

- (none open)

## Log

- 2026-11-20: map rebuilt after the re-sort. soil.slow runs its 100 river windows on every
  core (results bit-identical, sums joined in river order): slow tier 439 → 114 s, full tier
  ≈ 490 → 160 s. Found SoilByWater's steep-bed check never runs (0 samples): job
  `testing-soil-steep-beds-never-sampled`.
- 2026-11-19: the gate runs every unit module (approved proposal). TestFarPalette load flake
  fixed. `bun run test` 227 s.
- 2026-11-17: first session. Mapped suites; 6 failing tests → jobs and named skips; added
  `bun run test` (parallel, per-module timings). 579 s → 326 s.
