# CI: state

## Map
- `.github/workflows/release.yml`: the game on Linux and Windows, with sccache, vcpkg and CEF caches, unit tests, packaging, a smoke boot, and signing and publishing on a tag. Master pushes run it to warm the caches. Warm master run took about 18 min (2026-11-17, the 19:13 run). The 11-12 and 11-14 runs took 15 to 90 min and were often red.
- `.github/workflows/launcher.yml`: the launcher (Tauri/Rust), the same shape, 3.5 to 12 min. "Test the backend" runs `cargo test --lib` on Windows too.
- Upstream Luanti workflows (docker_image, cpp_lint, ...) are still present and were red on the 2026-10-24 pushes. Not looked at yet (prune or fix).
- No test-only fast tier on push yet. Not measured yet: cold vs warm per platform, cache hit rates.

## Numbers
| Date | Master green rate (push runs) | Time to signal | Warm/cold build | Release reliability | Runner min/day |
|---|---|---|---|---|---|
| 2026-11-17 | 3/7 = 43% (11-12 to 11-17; 3 superseded cancels excluded) | not measured | release warm about 18 min wall (1 sample); cold unknown | not measured | not measured |

## Plan
1. **(current) Master green.** Done when: master green rate is 100% over the next 5 master runs.
   - [x] Fix the Windows launcher test (2026-11-17).
   - [ ] Diagnose the 11-12 and 11-14 release.yml failures, and confirm today's green holds.
   - [ ] Decide on the upstream Luanti workflows (delete them if unused).
2. **One-command CI status** (`bun ci` over `gh`). Done when: the script reports every Scoreboard number.
3. **Fast tier first.** Done when: the median time to signal is under 10 min.
4. **Path filters** so launcher, site and docs pushes skip the engine builds. Done when: launcher-only pushes run 0 engine builds.
5. **Move inline YAML steps into scripts.** Done when: no `run:` block in a workflow holds multi-line logic.

## Tried
(none)

## Proposals
(none)

## Direction

Over the next few sessions we want to get everything setup properly to allow macos. So choose chunks of work to do. getting the repo itself in line and setting up the appropriate ci. you are tasked with the full thing. within a few sessions we should have macos online. permission to make the changes you need to get this working and online. keep track of this work and your plan until the job is complete.

## Log
- 2026-11-17: First session. Fixed red master: the launcher test `leaves_the_appimage_behind` assumed ':'-separated path lists and failed on Windows; it is now `#[cfg(unix)]`. Baseline green rate: 43%.

