# check:interact: drop merging fails under load, now and then

Split from testing-atlas-sea-missing (jobs bot, 2026-11-26). The testing owner saw `check:interact`
fail once in 16 runs, under load, on "three drops of dirt lying together become one of 6 (found 2)".
Drop merging (or the check's wait for it) looks timing-dependent. Reproduce by running
`cd apps/game && bun check:interact` in a loop while the machine is busy (a build running).
