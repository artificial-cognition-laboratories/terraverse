# Choosing a view's blocks near the cap's edge takes 2 s of CPU (budget 1.5 s)

Found by the jobs bot, 2026-11-17, while moving the unit tests' time budgets to CPU time
(testing-wall-clock-budgets). `TestAtlasRender.testWithinBudget`'s eye at lat 84.6, lon 10, 3 over the
ground (the cap's edge) spends ~2125 ms of its own thread's CPU choosing the blocks the server sends
(`Atlas::blocksAt` over every slot in range), against a 1500 ms budget, on a quiet machine. The other
three eyes take 68–536 ms. So it's the code, not load: a player arriving near the cap's edge waits
that long for the server to pick what to send.

Reproduce: `bun check:budgets` (or `TF_BUDGETS=1 ATLAS_BUDGET=1 ./.output/bin/luanti --test-module TestAtlasRender`).
Unknown whether it regressed or was always so: planet/ hasn't changed since e7bdd1d87 (Nov 4).
Profile `blocksAt` there (cut planes at the cap, `prepare`?) and bring it under the budget, or argue the
budget up in the test with a reason.

Profile (perf, `ATLAS_BUDGET=1 --test-module TestAtlasRender`, all four eyes): hypot 14%,
`terragen::Atlas::at` 11%, `Atlas::retained` 8.5%, `Atlas::inside` 7.3%, `Atlas::grid` 1.7%,
`planet::Atlas::cell` 1.5%, `cellsAround` 1.3%. So it's geometry asked again and again per slot
(`blocksAt` for every slot in range, each locating cells through `at`/`inside`), not one hot loop:
caching a slot's cells or locating them once per block would be the first thing to try.
