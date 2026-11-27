# UI check flaky locally on Linux (testing)

Found 2026-11-26 by the CI owner while checking the macOS pump didn't change Linux.
On master (335a08a7) and with the change, `TERRAVERSE_HOME=$(mktemp -d) .output/bin/terraverse --ui-check`
alternated between pass and fail on this machine, with no code change between runs:
- "UICHECK the page answered 16 inputs (fewer than 20): the sweep never reached it"
- once "input to screen, 95th percentile: 80 ms (budget 34)"
Run it 5 times in a row to reproduce. Either the sweep's timing or the budget depends on how loaded the machine is.

## Agent notes (2026-11-26, jobs bot)

Reproduced: 5 of 5 runs failed at 4137fa3ac, mostly on answers (11–16), once on p95. The machine
was busy (other sessions' builds and tests), and `/tmp` hit its quota mid-run. Each run's mktemp home
is ~16 MB under /tmp, so use a home under `.output/` (it must be an absolute path).

Tried: a sweep twice as fast (sin(t·5.4), sin(t·7.4+1) in ui_check.cpp). Answers went 15, 69, 40,
117, 16 over five runs, and every run still failed (p95 48 ms twice, and other budgets). Reverted.
"Answered" is a paint following a move within 250 ms (latency.cpp), so the count depends on
how many hover targets the sweep happens to cross on whatever screen is up, and on load. With ~12
samples, p95 is close to the worst one.

What would unblock it: a call on what the check is for locally. Either run it only on an idle
machine (CI), or make the sweep deterministic: move between the page's own hover targets (rows and
buttons, e.g. sent by the page) instead of a Lissajous, so every move should paint and there are
enough samples for a percentile. That's a redesign of the check, not a night's fix.
