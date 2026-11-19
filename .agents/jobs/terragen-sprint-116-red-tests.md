# TestTerragen: testRanges and testDeterminismGolden fail on master since sprint 116

Found by the CI owner, 2026-11-18: the release run on f99e48d5 (run 44397404612) failed its Linux
unit tests. Both fail at 0f12048e4 "Land by the book" and reproduce locally:

    ./.output/bin/luanti --test-module TestTerragen
    Test assertion failed: grid->eroded[i] > ratio(6, 10)     [FAIL] testRanges
    Test assertion failed: h == 3643851153u                    [FAIL] testDeterminismGolden

Neither is a surprise to the sprint: `planning/sprint-116-land-by-the-book.md` lists `testRanges` as
failing until phase 4's plateaus (rivers from the sea cut their interiors), and says the goldens are
rebaked once, with the TF25 bump, when every phase is done. But the sprint was committed part-way, so
master's release runs went red on them.

Both are skipped by name in `scripts/test/skip.txt` (CI's unit tests read it). Remove the two lines
when the sprint rebakes the golden (TF25) and the plateaus stand again.

Note for the sprint: until TF25, generation changed under TF24 without a version bump, so a TF24 code
no longer means what it did (AGENTS.md: "A code must never silently change meaning"). Old worlds
breaking was accepted in the plan; the code's meaning changing isn't covered there.

## Agent notes (2026-11-18, jobs bot)

Left open; no code changed. The job is a tracker: both tests are already skipped by name, and the
lines come out when the sprint rebakes the golden at TF25 (phase 5) and plateaus stand again
(phase 4's open item, `planning/sprint-116-land-by-the-book.md`). Rebaking the golden or bumping
TF25 tonight would cut across the sprint's own plan ("one version bump and one rebake then"), so
that's the developer's/sprint's call, not a fix. The note on TF24 changing meaning silently also
belongs to that call: the remedy is the TF25 bump itself, so it gets no separate action.
Unblocks when sprint 116's phases 4–5 land.
