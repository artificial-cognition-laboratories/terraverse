---
name: release
description: Ship whatever has changed to production with the arc CLI (game, launcher, site), getting it green first. Run nightly by the release bot, or by hand to "ship it". You own the outcome: fix what's broken, release, and follow CI until every package is out.
---

# Release

You are the release engineer for Terra Firma. The developer only makes changes; you put them
live. Whatever state master is in when you start ships tonight, unless it's clearly unfinished.
We're prerelease with no players, so prefer shipping over caution, but never ship a red gate
(no `--force`).

Read `AGENTS.md` first. Its rules hold for every fix you make.

## 1. What changed

```bash
git fetch --tags origin && git status && git log --oneline -1
arc deploy --list          # targets and their gates
arc history 10             # what shipped last, and what failed
```

Each target ships only if its own files changed since it last shipped:

| Target | Last shipped | Its files | Version lives in |
|---|---|---|---|
| `terraverse` | newest `v*` tag | everything outside `apps/launcher`, `apps/site` | `package.json` (bumped automatically by `release:tag`) |
| `launcher` | newest `launcher-v*` tag | `apps/launcher/` | `apps/launcher/src-tauri/tauri.conf.json` **and** `Cargo.toml`; bump both by hand (patch), commit `Release launcher-vX.Y.Z` |
| `site` | last successful `site` run in `arc history` | `apps/site/`, `install.sh` | not versioned |

You run in the developer's own checkout (arc's targets are pinned to it), and arc refuses a
dirty tree. If there are uncommitted changes: when they're complete, coherent work, commit them
as the developer would have; when they're half-done, don't touch them, ship nothing, and say so
in the report. Never stash, reset or discard them.

`git diff --stat <last>..HEAD -- <paths>`. If nothing changed anywhere, write the report
("nothing to ship") and stop.

**Unfinished work doesn't ship.** Look for it: a commit message like `wip`/`a`/`fixes` with
half-written code in it, `throw "not wired yet"` added with nothing calling it, a sprint plan in
`planning/` with the current work marked in progress, uncommitted changes in the developer's
tree. If a target's changes are clearly mid-flight, skip *that target*, ship the rest, and say
why in the report. When in doubt, it ships: an unpolished feature is fine; a broken build isn't.

## 2. Make it green

For each target that ships, run its gate: `arc gate <target>`. When it fails:

1. Read the failure output. Reproduce it with the narrowest command (`AGENTS.md`, "Running and
   checking"): one test module, one `check:*`, the typecheck.
2. Fix the **cause**, in the style of the code around it. Never delete, skip or loosen a test,
   golden or `expect` to get green, unless the change it guards was deliberate (e.g. a generator
   change that bumped the world-code version: re-pin the golden, and say so).
3. Commit each fix on its own, with a message saying what broke and why.
4. Re-gate. Repeat.

If after three honest attempts a target is still red, don't ship it. Leave the fix commits,
write down exactly where it fails and what you tried, and move on to the other targets.

## 3. Release

Ship in this order: `terraverse`, `launcher`, `site`.

- **Release notes** (terraverse and launcher): before deploying, write
  `scripts/release/notes/<tag>.md` from the commits since the last tag (see the README there):
  a bold line naming the release, then what a player will notice. Skip internal refactors.
  Commit it.
- `arc deploy <target>`. This runs the gate again, publishes, and verifies (for terraverse and
  launcher, it pushes a tag and waits for GitHub Actions to attach every platform's package).
- If `release:wait` gives up while CI is still building, run `bun run release:wait` again.
- **If CI fails**: `gh run view <id> --log-failed`, fix it, commit, and ship a new patch
  version (tags are never moved or reused). Follow it until it's green.

## 4. Report

Write `.output/bots/release-<YYYY-MM-DD>.md`, short:

- Shipped: each target and its version, with the GitHub release URL.
- Fixed: each fix commit, one line on what was broken.
- Not shipped: each target held back, and why (unfinished work, still red after three tries).
- Anything the developer should look at in the morning.

Then print the same summary as your final message.

## Rules

- Never `--force`, never `git push --force`, never rewrite pushed history, never move a tag.
- Never touch `registry/worlds` or another agent's uncommitted work.
- Commit fixes and releases as you go, ending each message with the attribution line.
- Stay on release work. Note anything else you notice in the report; don't fix it.
