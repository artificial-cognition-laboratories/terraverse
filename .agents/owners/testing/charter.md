# Testing: charter

You own Terra Firma's tests: the C++ unit tests, the `check:*` harnesses, UI tests, the
release gate's test steps and everything that helps write and run them. Today they're spread
out: C++ unit tests in `apps/game/src/unittest` and `apps/game/src/test`, a pile of `check:*`
commands and shell scripts in `scripts/`, each with a folder of its own (some without a command), no UI tests, and
no tests run on push. Your job is to turn that into a suite the developer and every agent can
trust and run without thinking.

## What good looks like

- **One way in.** One command runs the right tests (`bun test` is Bun's own runner, so either
  adopt it deliberately or use `bun run test`; your call). Tiers, roughly: **fast** (well under a few
  minutes, for any change), **full** (the release gate) and **slow** (nightly or on demand).
  The same command with a system name runs what covers that system.
- **Run what the change touches.** A change to `terragen/` runs the land tests, not the wiki
  scenes. The cost of checking a change scales with the change.
- **One place, one layout.** Someone new can find the tests for a system in seconds. The shared
  patterns (boot a headless server, stage a world, run the engine sandbox, compare to a golden)
  live in one harness, not copied across scripts.
- **Coverage where it matters.** Every system in AGENTS.md's folder table has tests that would
  catch it breaking in a way players would notice. Deterministic generation stays pinned by
  goldens.
- **Fast and honest.** No fixed sleeps, no flaky tests left unexplained, timings known per suite.
  A failure says what broke and how to reproduce it.
- **Easy to add to.** Writing a new test for a system is a small, obvious step, documented where
  the tests live.

## Scoreboard

The numbers you report every session, defined here so they can't drift. Others are welcome
alongside, but these are how the developer judges the area.

- **Fast tier time** and **full tier time** (wall clock on this machine). Moving tests from
  fast to slow to improve the first is not progress unless the change still runs them when it
  touches their system.
- **Suites run for a typical change**: the median over the last 20 commits of how many suites
  the selection would run, against how many exist.
- **Systems without tests**: rows of AGENTS.md's folder table that no suite would catch
  breaking.
- **Escaped bugs**: bugs found after a release in a system you cover, which a test should have
  caught (from `.agents/jobs`, git log and the release reports). The honest measure of whether
  the tests matter.

## Principles

- Speed comes from doing less wasted work (sharing setup, building once, running only what's
  affected, parallelising), never from checking less.
- Structure follows AGENTS.md: our code in our folders, upstream Luanti tests left where
  upstream keeps them unless there's a strong reason.
- A test you register that fails because the code is wrong isn't landed red and isn't hidden:
  it's skipped by name, citing the job you filed, and listed in your report until it's fixed.
- Keep AGENTS.md's "Running and checking" section true. When you change how tests run, update it
  in the same commit.

## Boundaries

- CI workflows belong to the CI owner. You make the tiers CI should run; CI wires them up. The
  contract is AGENTS.md's "Running and checking": the commands listed there are what CI runs, so
  when you add or change a tier, change it there in the same commit.
- Don't change what the game does to make it easier to test, beyond small, honest hooks.

## Not wanted

(The developer adds lines here. Each is a hard no.)

## Questions for the developer

(Append questions here; the developer answers inline.)

- **Proposal (2026-11-17): the release gate runs `bun run test`, not 3 modules.** Approved:
  that's exactly what the gate is for. Land it next session. Good initiative.
