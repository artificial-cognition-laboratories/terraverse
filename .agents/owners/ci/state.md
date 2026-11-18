# CI: state

## Map
- `.github/workflows/release.yml`: the game on Linux (ubuntu-22.04, gcc-12, sccache) and Windows
  (clang-cl, vcpkg x64-windows, sccache). Each: UI build, terra (gen:check, tsc, bun test, compile),
  engine, unit tests (TestOnline, TestRelease, TestTerragen: Linux through `scripts/test.sh`, Windows
  through cmd with `TF_TEST_SKIP` from `scripts/test/skips.sh`, so `scripts/test/skip.txt` applies to
  both), package, smoke boot. Publish only on a tag (a dispatch keeps an artifact, nothing else). Push
  filter: apps/game, apps/cli, packages/types, assets, library, cmake, CMakeLists, vcpkg.json,
  .gitignore, scripts/release, stage-runtime, the workflow. Warm jobs: Linux 10.0 min, Windows 14.6 min;
  Windows cold 36.4 min.
- `.github/workflows/launcher.yml`: the launcher (Tauri/Rust). Warm: Linux 3.9 min, Windows 3.3 min.
- `.github/workflows/macos.yml`: **dispatch only**, the macOS bring-up. macos-15 arm64, vcpkg arm64-osx
  static (cache ~73 MB), CEF macosarm64, sccache. `scripts/release/configure-macos.sh`, build `-k 0`,
  `scripts/release/build-errors.sh` (errors by file and undefined symbols, in the job summary), stage,
  then the three unit modules. **Green** since run 44412675690. Warm 3.6 min (configure 35 s, build 2 min,
  tests 41 s); cold 30 min (vcpkg 19 min, engine 11 min).
- `scripts/ci/status.ts` (`bun ci:status [--since date]`): what's running, each workflow's latest
  master run and its failing step, every Scoreboard number. ~20 s, read-only.
- macOS state of the code: builds and links with vcpkg static; curl and FreeType link through their
  targets (`CURL_LINK`, `FREETYPE_LINK` in apps/game/src/CMakeLists.txt), OpenAL is the system framework
  (configure-macos.sh). The shell is **not wired**: `Shell` throws "not wired on macOS yet", `main`
  skips `runHelper` on Apple. So: tests and `--server` can run, the client can't.
  Still non-portable: `scripts/test.sh` with no module args uses `mapfile` (bash 4; macOS has 3.2);
  vcpkg's openal-soft is still built on osx though unused (drop it with `"platform": "!osx"` the next
  time vcpkg.json changes anyway: changing it now makes Windows rebuild vcpkg cold, ~36 min);
  vcpkg builds for macOS 15 while the engine targets 13.0 (ld warns; needs an overlay triplet with
  VCPKG_OSX_DEPLOYMENT_TARGET 13.0 before shipping).

## Numbers
How measured: `bun ci:status --since <last session>`. Cold build = a successful job over 2x its
kind's median (the API doesn't say whether caches hit).

| Date | Master green rate | Time to signal (median) | Warm build (job) | Cold build | Release reliability | Runner min/day |
|---|---|---|---|---|---|---|
| 2026-11-17 | 3/7 = 43% | not measured | release ~18 min wall | unknown | not measured | not measured |
| 2026-11-18 | 9/13 = 69% (since 11-17 21:00; the last 4 release runs green in a row) | 9.6 min (11 pushes) | release linux 10.0, windows 14.6; launcher 3.9 / 3.3; macos 3.6 min | release windows 36.4 min; macos 30 min | 2/2 (v0.1.4, launcher-v0.1.2) | 305 (324 min over 1.1 days, includes tonight's bring-up) |

## Plan
1. **Master green.** Done when: master green rate is 100% over 5 consecutive master runs.
   - [x] 2026-11-17: the Windows launcher test.
   - [x] 2026-11-18: terra's gitignored `server/build/`; release.yml not running on apps/cli; terra's
     Windows tests and build; trace.cpp's cxxabi.h on clang-cl; sprint 116's two red terragen tests
     skipped by name (job terragen-sprint-116-red-tests). Streak: 4 green release runs (98e5318a,
     5eb0a8b2, b0c630ae, e5c7e9bd).
   - [ ] Hold it to 5.
2. **(current, the developer's direction) macOS online.** Done when: release.yml builds, tests,
   packages and smoke-boots macOS arm64 on master pushes, and a tag publishes a macOS package.
   - [x] Dispatch-only macos.yml (2026-11-18).
   - [x] Compiles (sidecar SOCK_CLOEXEC, st_mtimespec), links (curl/FreeType targets, system OpenAL),
     unit tests pass (shell guarded), 2026-11-18.
   - [ ] Headless server boot on the runner: `scripts/release/smoke-server.sh` shared by all three
     platforms (moves the inline YAML boot checks into a script too: milestone 6). Then add macOS to
     release.yml as a **non-publishing** job on master pushes (it can't publish until it packages), so
     every push keeps macOS compiling; macos.yml goes.
   - [ ] vcpkg overlay triplet: arm64-osx with VCPKG_OSX_DEPLOYMENT_TARGET 13.0 (cold vcpkg once).
   - [ ] The app: `.app` bundle with CEF's framework and its helper apps (`<name> Helper.app`,
     GPU/Renderer/Plugin variants), `CefScopedLibraryLoader` LoadInMain in main and LoadInHelper in a
     helper executable, then drop the Shell guard. Verify by a client start on the runner (screenshot).
   - [ ] `scripts/release/package-macos.sh` (ad-hoc signed; Developer ID + notarisation waits on the
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
- Homebrew dependencies for macOS: not tried, decided against (dylibs to bundle and relink, built for
  the runner's macOS). vcpkg static works.
- `CURL_LIBRARIES` instead of `CURL_LIBRARY`: empty where curl's own CMake config is used (Arch); the
  target is what every setup defines.

## Proposals
- (none)

## Direction

Over the next few sessions we want to get everything setup properly to allow macos. So choose chunks of work to do. getting the repo itself in line and setting up the appropriate ci. you are tasked with the full thing. within a few sessions we should have macos online. permission to make the changes you need to get this working and online. keep track of this work and your plan until the job is complete.

## Log
- 2026-11-17: First session. Fixed red master: the launcher test `leaves_the_appimage_behind` assumed ':'-separated path lists and failed on Windows; it is now `#[cfg(unix)]`. Baseline green rate: 43%.
- 2026-11-18: Master red since 05ec8cb6, fixed through five layers (gitignored terra sources, release's path filter, terra on Windows, cxxabi.h, sprint 116's red tests skipped by name); 4 green in a row since. CI's unit tests honour skip.txt. `bun ci:status` (milestone 3 done). macOS from nothing to green on a dispatch run: compiles, links, unit tests pass, warm 3.6 min. Green rate 43% → 69%.
