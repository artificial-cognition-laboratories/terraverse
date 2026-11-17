# Unit tests that assert wall-clock budgets fail when the machine is busy

Testing owner's own job (2026-11-17). Several unit tests assert a time budget in milliseconds:

- `TestSandbox.testWorldBudget` (test_sandbox.cpp:490): a step of 10 000 awake blocks under its budget; 51 ms measured.
- `TestSandbox.testBudgetPicksBusiest` (test_sandbox.cpp:700): a budgeted step under 30 ms; 35–67 ms measured.
- `TestAtlasRender.testWithinBudget` (also in testing-atlas-render-fails.md).

They passed in a sequential `--run-unittests` on a quiet machine and fail with load average ~3 (other builds and agents share this machine), even run alone. So they fail for load, not for the code, and a red run tells nobody what broke.

Fix (testing owner): split performance budgets from correctness. Correctness half stays in the unit tier (e.g. the budget picks the busiest blocks, counted, not timed); the timing half moves to a perf tier that measures CPU time (`clock_gettime(CLOCK_THREAD_CPUTIME_ID)`) rather than wall time, or runs only on an idle machine. Until then both TestSandbox tests are skipped by name in `scripts/test/skip.txt`.

Reproduce: run anything CPU-heavy, then `./.output/bin/luanti --test-module TestSandbox`.
