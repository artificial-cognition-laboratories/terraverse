# UI check flaky locally on Linux (testing)

Found 2026-11-26 by the CI owner while checking the macOS pump didn't change Linux.
On master (335a08a7) and with the change, `TERRAVERSE_HOME=$(mktemp -d) .output/bin/terraverse --ui-check`
alternated between pass and fail on this machine, with no code change between runs:
- "UICHECK the page answered 16 inputs (fewer than 20): the sweep never reached it"
- once "input to screen, 95th percentile: 80 ms (budget 34)"
Run it 5 times in a row to reproduce. Either the sweep's timing or the budget depends on how loaded the machine is.
