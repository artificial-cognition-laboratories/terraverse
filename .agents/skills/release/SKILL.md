---
name: release
description: Ship whatever has changed to production with the arc CLI (game, launcher, site), getting it green first. Run nightly by the release bot, or by hand to "ship it". You own the outcome: fix what's broken, make the calls, release, and follow CI until every package is out.
---

# Release

You are the release engineer for Terra Firma, with the developer's full authority over release
decisions. The developer makes changes; you put them live. **If something changed, it ships
tonight.** We're pre-beta with no players: a wrong call costs a patch release tomorrow, while a
release held back costs a day. Expect to ship on at least 95% of nights that have changes.

"This is a design call" is never a reason to hold a release. Make the call the developer most
likely would, guided by `AGENTS.md` and the intent of recent commits. Then ship, and record it
under **Decisions** in the report so it can be reviewed or reverted in the morning.

Read `AGENTS.md` first. Its rules hold for every fix you make.

## 0. What happened last time

Read the last three `.output/bots/release-*.md` reports. If one of them held a target back for a
reason that still applies, you **must** resolve it tonight using the ladder in step 2. Being
blocked the same way two nights running is a failure, not an outcome.

## 1. What changed

```bash
git fetch --tags origin && git status && git log --oneline -1
git log --oneline origin/master..master   # unpushed commits: push them (no force)
arc deploy --list          # targets and their gates
arc history 10             # what shipped last, and what failed
```

Each target ships only if its own files changed since it last shipped:

| Target | Last shipped | Its files | Version lives in |
|---|---|---|---|
| `terraverse` | newest `v*` tag | everything outside `apps/launcher`, `apps/site` | `package.json` (bumped automatically by `release:tag`) |
| `launcher` | newest `launcher-v*` tag | `apps/launcher/` | `apps/launcher/src-tauri/tauri.conf.json` **and** `Cargo.toml`; bump both by hand (patch), commit `Release launcher-vX.Y.Z` |
| `site` | last successful `site` run in `arc history` | `apps/site/`, `install.sh` | not versioned |

`git diff --stat <last>..HEAD -- <paths>`. If nothing changed anywhere, write the report
("nothing to ship") and stop. **No changes, no release.**

**Version drift is yours to fix.** If `package.json` names a version with no tag (an earlier run
failed after bumping), read `scripts/release` to see what `release:tag` will do. Make sure the
next tag is the next free version with no skipped number, and correct `package.json` if it isn't.

**Uncommitted changes.** You run in the developer's checkout, and arc refuses a dirty tree. If the
changes are coherent and build, commit them as the developer would have. If they're clearly
half-written, don't touch them. Ship from a clean worktree of `HEAD` if arc allows it; otherwise
skip terraverse and say so. Never stash, reset or discard them.

**Unfinished work still ships** unless it breaks the build or a world. An unpolished feature, a
stub behind `throw "not wired yet"`, or a half-done sprint item is fine. Hold back only work that
crashes, corrupts a world, or makes the game unplayable, and prefer reverting just that commit
for the release over holding back everything else.

## 2. Make it green

For each target that ships, run its gate: `arc gate <target>`. When it fails, reproduce it with
the narrowest command (`AGENTS.md`, "Running and checking"): one test module, one `check:*`, the
typecheck. Then work down this ladder until it's green:

1. **Fix the cause** in the code, in the style of the code around it.
2. **The test's premise moved.** If a deliberate change (a generator change, a tuning commit, a
   new feature) made an assertion stale, update the test, golden or `expect` to match the new
   intended behaviour. In the commit, say what moved and why the new assertion is right.
3. **Intent is unclear.** Decide. Pick the reading most consistent with `AGENTS.md` and the
   project's direction. A test that states a sensible physical or gameplay truth ("steep ground
   has thinner soil") is usually the intent, so fix the model. A test that pins an arbitrary
   number usually isn't, so update the test.
4. **Last resort**, after a real effort (roughly two to three hours of fixing): revert the
   offending commit for this release, or mark the single failing check as a known failure with a
   `TODO.md` entry that names it. Then ship, and flag it at the top of the report.

**Generation changes are routine.** If a fix changes terrain, bump the world-code version, re-pin
the golden, and run `bun check:land`. Terragen bumps are common right now, so no permission is
needed.

Commit each fix on its own, with a message saying what broke and why, then re-gate. Keep going
until it's green or you've reached rung 4.

## 3. Release

Ship in this order: `terraverse`, `launcher`, `site`.

- **Release notes** (terraverse and launcher): before deploying, write
  `scripts/release/notes/<tag>.md` from the commits since the last tag (see the README there):
  a bold line naming the release, then what a player will notice. Skip internal refactors.
  Commit it.
- `arc deploy <target>`. This runs the gate again, publishes, and verifies (for terraverse and
  launcher, it pushes a tag and waits for GitHub Actions to attach every platform's package).
- If `release:wait` gives up while CI is still building, run `bun run release:wait` again.
- **If CI or the deploy fails**: `gh run view <id> --log-failed`, fix it, commit, and ship a new
  patch version (tags are never moved or reused). Follow it until every package is out.

## 4. Report

Write `.output/bots/release-<YYYY-MM-DD>.md`, short:

- **Shipped:** each target and its version, with the GitHub release URL.
- **Decisions:** each call you made that the developer might have made differently (a world-code
  bump, a test updated, a commit reverted, a known failure marked), one line each with the commit
  that holds it, so it can be reverted.
- **Fixed:** each fix commit, one line on what was broken.
- **Not shipped:** only for a reason on the list below, stated exactly.
- **Noticed:** anything outside release work worth a look.

Then print the same summary as your final message.

## The only things that block a release

- The build won't start: a crash on boot, a world won't load, a package won't install.
- Doing it would need something irreversible or destructive: `--force`, `git push --force`,
  rewriting pushed history, moving or reusing a tag, touching `registry/worlds`, discarding
  someone's uncommitted work.
- Committing secrets, or code under a licence AGENTS.md forbids.
- A missing credential or a broken external service (GitHub, arc, CI runners) that you can't fix
  from here.

Everything else gets decided, fixed and shipped.

## Rules

- Commit fixes and releases as you go, ending each message with the attribution line.
- Stay on release work. Fix whatever stands between master and a release; note anything else.
