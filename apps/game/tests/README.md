# The game's tests

| Folder | What | Run by |
|---|---|---|
| `unit/` | engine unit modules (`class TestX : public TestBase`), one process each | `run/engine.sh` |
| `engine/` | Catch2 `TEST_CASE`s, run together as the suite `Catch` | `run/engine.sh` |
| `library/` | the kit every pure library's tests share (doctest, `library_test()`) | `run/library.sh` |
| `../library/<area>/<lib>/tests/` | a pure library's own tests, its own small binary | `run/library.sh` |
| `checks/` | `check:*` harnesses: a headless server, the engine sandbox, a real world | `bun check:<name>` |
| `probe/` | the probes those harnesses drive (`luanti --probe <name>`) | `checks/*.sh` |
| `run/` | the runners, the skip list, and `select/`: which suites a change reaches | |

**Tiers.** `bun check` before a commit (hot library tests, every engine module, the UI's
types); `bun check --changed` the same for only what you changed; `bun run test` (from
`apps/game`) the full tier, which the release gate runs; `bun test:slow` the slow cases alone.
The full tier builds once, then runs the libraries and the engine's modules side by side (the
libraries' report prints after the engine's). It's CPU-bound: about half its core time is
`terragen.soil.slow` reading an Earth-sized planet.

**What a change runs.** `run/select` reads the last build's depfiles: a changed file runs every
suite compiled against its unit (its library, or its folder). Data, build files and the runners
run wide. `bun run/select/main.ts --changed` says what would run and why, and lists changed code
that no suite is built against.

**Adding a test.**
- A pure library: a `.cpp` in its `tests/`, listed in its `library_test(… SOURCES …)`. Cases in
  `* doctest::test_suite("slow")` run in the slow tier only.
- The engine: `unit/test_<thing>.cpp` with a `TestBase` subclass and a static instance (copy
  `unit/test_far.cpp`), listed in `unit/CMakeLists.txt`. Selection finds it from what it includes.
- A test that fails because the code is wrong is skipped by name in `run/skip.txt`, citing a job
  in `.agents/jobs/`, never left red or deleted.
