# CI: state

## Map
- `.github/workflows/release.yml`: the game on Linux (ubuntu-22.04, gcc-12, sccache) and Windows
  (clang-cl, vcpkg x64-windows, sccache). Each: UI build, terra (gen:check, tsc, bun test, compile),
  engine, unit tests (TestOnline, TestRelease, TestTerragen: Linux through `apps/game/tests/run/engine.sh`, Windows
  through cmd with `TF_TEST_SKIP` from `apps/game/tests/run/skips.sh`, so `apps/game/tests/run/skip.txt` applies to
  both), package, smoke boot. Publish only on a tag (a dispatch keeps an artifact, nothing else). Push
  filter: apps/game, apps/cli, packages/types, assets, library, cmake, CMakeLists, vcpkg.json,
  .gitignore, apps/game/scripts/release, stage-runtime, the workflow. Smoke boot: both platforms call
  `apps/game/scripts/release/smoke-server.sh <exe> <version>` (bash 3.2-portable; stops once listening
  with mods loaded). Warm jobs: Linux 5.5 min, Windows 11.0 min (run 44834312695); Windows cold 60 min (11-20, vcpkg after the move).
- Since the hierarchy move (2c006128) terragen and its tests are their own targets, configured before
  src/CMakeLists.txt sets the engine's MSVC flags: Windows-wide options (/EHsc, /MD, compiler-rt
  builtins, clang-cl required) live at the top of apps/game/CMakeLists.txt.
- Cache keys hash apps/game/vcpkg.json and apps/game/cmake/Modules/CEF.cmake (were stale after the
  move, hashing nothing); vcpkg restore falls back to any older cache of its triplet (ABI-hashed, safe).
- Official mods: release.yml's `mods` job (beside the builds; `publish` needs it) runs
  `apps/game/scripts/release/publish-mods.ts`. Per `registry/mods/*`: a version the registry has must
  match the repo byte for byte (published tarball vs terra's pack list) or it fails "bump"; a version
  it lacks goes up by `terra publish`. Tag: publishes, signed in by the `TERRA_TOKEN` secret (terra
  reads it since 556ceb2b; **the secret doesn't exist yet**, so a tag fails only if a mod needs
  publishing). Dispatch: `--dry-run` (fails as a tag would). Master push: `--dry-run --warn-unbumped`.
  ~1 s locally, ~40 s as a job. Today both mods are at 0.1.0 on production, identical to the repo.
- Release scripts (manifest.ts, gate.sh) read engine sources by path: a move breaks them silently,
  only a tag or the gate would notice. A dispatch run exercises manifest.ts (publish's first step).
- `.github/workflows/launcher.yml`: the launcher (Tauri/Rust). Warm: Linux 3.9 min, Windows 3.3 min.
- macOS: release.yml's `macos` job (since 2026-11-21; macos.yml deleted), on every push, dispatch and
  tag, **not published** (`publish` doesn't need it, so a red macOS never holds a release). macos-15
  arm64, vcpkg arm64-osx static (cache ~73 MB), CEF macosarm64, sccache.
  `apps/game/scripts/release/configure-macos.sh`, build `-k 0`, `apps/game/scripts/release/build-errors.sh`
  (errors in the job summary), stage, terra's tests, the three unit modules, smoke boot of the
  run-in-place build. The repo is public: macOS minutes cost nothing, but count in runner-min/day.
- `scripts/ci/status.ts` (`bun ci:status [--since date]`): what's running, each workflow's latest
  master run and its failing step, every Scoreboard number. ~20 s, read-only.
- macOS state of the code: builds and links with vcpkg static; curl and FreeType link through their
  targets (`CURL_LINK`, `FREETYPE_LINK` in apps/game/src/CMakeLists.txt), OpenAL is the system framework
  (configure-macos.sh). The shell is **not wired**: `Shell` throws "not wired on macOS yet", `main`
  skips `runHelper` on Apple. So: tests and `--server` can run, the client can't.
  Still non-portable: `apps/game/tests/run/engine.sh` with no module args uses `mapfile` (bash 4; macOS has 3.2);
  vcpkg's openal-soft is still built on osx though unused (drop it with `"platform": "!osx"` the next
  time vcpkg.json changes anyway: changing it now makes Windows rebuild vcpkg cold, ~36 min);
  vcpkg builds for macOS 15 while the engine targets 13.0 (ld warns; needs an overlay triplet with
  VCPKG_OSX_DEPLOYMENT_TARGET 13.0 before shipping).

## Numbers
How measured: `bun ci:status --since <last session>`. Cold build = a successful job over 2x its
kind's median (the API doesn't say whether caches hit).

| Date | Master green rate | Time to signal (median) | Warm build (job) | Cold build | Release reliability | Runner min/day |
|---|---|---|---|---|---|---|
| 2026-11-20 | 5/15 = 33% before tonight (the hierarchy move: 2c006128 onward red until 9c412f43) | 10.6 min (12 pushes) | release linux 10.2, windows 12.7; launcher 2.7 / 3.4 | release windows 60 min (vcpkg cold after the move) | 3/5 (v0.1.7, v0.1.8 failed on Windows) | 235 (398 min over 1.7 days) |
| 2026-11-17 | 3/7 = 43% | not measured | release ~18 min wall | unknown | not measured | not measured |
| 2026-11-19 | 3/5 = 60% (since 11-18 22:00; 7f82097d cancelled unsuperseded, 7321a457 red Windows terra, abd83088 red Linux Package + Windows terra) | not measured (GitHub API down) | not measured | not measured | 1/2 (v0.1.5 failed, v0.1.6 ok; launcher-v0.1.3 ok) | not measured |
| 2026-11-18 | 9/13 = 69% (since 11-17 21:00; the last 4 release runs green in a row) | 9.6 min (11 pushes) | release linux 10.0, windows 14.6; launcher 3.9 / 3.3; macos 3.6 min | release windows 36.4 min; macos 30 min | 2/2 (v0.1.4, launcher-v0.1.2) | 305 (324 min over 1.1 days, includes tonight's bring-up) |

## Plan
0. **Official mods publish with each release.** Done 2026-11-21 (the developer's direction):
   556ceb2b (TERRA_TOKEN in terra), 91e750e4 (publish-mods.ts and the mods job); dispatch proof on
   branch ci/publish-mods, run 44949973289. Left: the `TERRA_TOKEN` secret (Questions for the
   developer). `bun check:registry` wasn't reused: the script imports terra's own Registry, Api and
   Pack from apps/cli/src, so there is still one registry client.

1. **Master green.** Done when: master green rate is 100% over 5 consecutive master runs.
   - [x] 2026-11-17: the Windows launcher test.
   - [x] 2026-11-18: terra's gitignored `server/build/`; release.yml not running on apps/cli; terra's
     Windows tests and build; trace.cpp's cxxabi.h on clang-cl; sprint 116's two red terragen tests
     skipped by name (job terragen-sprint-116-red-tests). Streak: 4 green release runs (98e5318a,
     5eb0a8b2, b0c630ae, e5c7e9bd).
   - [x] 2026-11-19: packages no longer copy the deleted builtin/ (00c128d6).
   - [x] 2026-11-19's Windows terra red: fixed upstream (9df8bf9b, tar and drive letters).
   - [x] 2026-11-20: the hierarchy move (2c006128) left terra, the launcher, ops.h's generator and
     both publish steps' update-key.pub on old paths (54519f82); a staged deletion swept into that
     commit broke the UI build, restored in b6d9f331.
   - [x] Held: every finished master run since 451c28bb (2026-11-20 23:22) green, 7 in a row by
     2026-11-21. One cancel (eb735aac, 2 s in, during my own push/dispatch burst) still counts red.
   Done 2026-11-21; the number stays on the Scoreboard.
2. **(current from next session, the developer's direction) macOS online.** Done when: release.yml builds, tests,
   packages and smoke-boots macOS arm64 on master pushes, and a tag publishes a macOS package.
   - [x] Dispatch-only macos.yml (2026-11-18).
   - [x] Compiles (sidecar SOCK_CLOEXEC, st_mtimespec), links (curl/FreeType targets, system OpenAL),
     unit tests pass (shell guarded), 2026-11-18.
   - [x] 2026-11-20: `apps/game/scripts/release/smoke-server.sh`, Linux and Windows on it (2d28a08f).
   - [x] 2026-11-20: macos.yml builds UI and terra and smoke-boots `.output/bin/luanti` (run 44836357243,
     green, 22 min: a world boots headless on macOS).
   - [ ] **Next:** macOS joins release.yml as a non-publishing job on master pushes (build, unit tests,
     smoke on the run-in-place build); macos.yml goes. Adds ~10 macOS runner-min per push (10x billed:
     check the minutes number after). (moves the inline YAML boot checks into a script too: milestone 6). Then add macOS to
     release.yml as a **non-publishing** job on master pushes (it can't publish until it packages), so
     every push keeps macOS compiling; macos.yml goes.
   - [ ] vcpkg overlay triplet: arm64-osx with VCPKG_OSX_DEPLOYMENT_TARGET 13.0 (cold vcpkg once).
   - [ ] The app: `.app` bundle with CEF's framework and its helper apps (`<name> Helper.app`,
     GPU/Renderer/Plugin variants), `CefScopedLibraryLoader` LoadInMain in main and LoadInHelper in a
     helper executable, then drop the Shell guard. Verify by a client start on the runner (screenshot).
   - [ ] `apps/game/scripts/release/package-macos.sh` (ad-hoc signed; Developer ID + notarisation waits on the
     developer's answer in the charter), smoke test, publish in release.yml: release-path change, needs
     the charter's dispatch proof.
   - [ ] Launcher (Tauri) on macOS in launcher.yml.
3. **One-command CI status.** Done 2026-11-18: `bun ci:status`.
4. **Fast tier first.** Done when: median time to signal under 10 min with the whole unit suite in CI.
   Blocked on: TestFar and TestScale red on master (job testing-far-scale-red); the full suite takes
   47 s locally, so running all of it on Linux early is cheap once they're fixed or skipped by name.
5. **Path filters.** Done when: launcher-only pushes run 0 engine builds (looks true already; verify).
6. **Move inline YAML steps into scripts.** Done when: no `run:` block holds multi-line logic (the
   smoke tests and the publish steps are the big ones).

## Tried
- `git commit` with no paths while someone has staged changes: sweeps them in (2026-11-20, broke the UI
  build). Always `git commit -- <paths>`.
- Homebrew dependencies for macOS: not tried, decided against (dylibs to bundle and relink, built for
  the runner's macOS). vcpkg static works.
- `CURL_LIBRARIES` instead of `CURL_LIBRARY`: empty where curl's own CMake config is used (Arch); the
  target is what every setup defines.

## Proposals
- (none). Not a proposal but the developer's to do: create the `TERRA_TOKEN` secret (charter question, 2026-11-21).

## Direction

2026-11-21 (the developer): publish the official mods with every release, tonight, before going on with macOS. Milestone 0 above has the why and the shape.

Over the next few sessions we want to get everything setup properly to allow macos. So choose chunks of work to do. getting the repo itself in line and setting up the appropriate ci. you are tasked with the full thing. within a few sessions we should have macos online. permission to make the changes you need to get this working and online. keep track of this work and your plan until the job is complete.

## Log
- 2026-11-20: The hierarchy move broke master in 8 places (terra, launcher, ops.h gen, two publish key paths, terragen on gcc-12 and clang-cl, winresource.rc, the deleted official-mods server folder, manifest.ts, gate.sh); fixed one CI round at a time, green at 9c412f43. Smoke boot is one script for all platforms (2d28a08f). Cache keys fixed (stale since the move) with an ABI-safe vcpkg fallback; macOS boots a world headless in CI (milestone 2 step 3). My first commit swept in a staged deletion (restored b6d9f331). Green rate 60% → 33% over the window (all red from the move), now green.
- 2026-11-19: Master red again (sprint 119 + registry). Fixed Linux Package (builtin/ gone), verified by local package + smoke boot. Windows terra failure undiagnosed: GitHub's API unreachable all session; 00c128d6's run not watched. Green rate 69% → 60%.
- 2026-11-17: First session. Fixed red master: the launcher test `leaves_the_appimage_behind` assumed ':'-separated path lists and failed on Windows; it is now `#[cfg(unix)]`. Baseline green rate: 43%.
- 2026-11-18: Master red since 05ec8cb6, fixed through five layers (gitignored terra sources, release's path filter, terra on Windows, cxxabi.h, sprint 116's red tests skipped by name); 4 green in a row since. CI's unit tests honour skip.txt. `bun ci:status` (milestone 3 done). macOS from nothing to green on a dispatch run: compiles, links, unit tests pass, warm 3.6 min. Green rate 43% → 69%.
