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
- Release scripts (manifest.ts, gate.sh) read engine sources by path: a move breaks them silently,
  only a tag or the gate would notice. A dispatch run exercises manifest.ts (publish's first step).
- `.github/workflows/launcher.yml`: the launcher (Tauri/Rust). Warm: Linux 3.9 min, Windows 3.3 min.
- `.github/workflows/macos.yml`: **dispatch only**, the macOS bring-up. macos-15 arm64, vcpkg arm64-osx
  static (cache ~73 MB), CEF macosarm64, sccache. `apps/game/scripts/release/configure-macos.sh`, build `-k 0`,
  `apps/game/scripts/release/build-errors.sh` (errors by file and undefined symbols, in the job summary), stage,
  then the three unit modules. **Green** since run 44412675690. Warm 3.6 min (configure 35 s, build 2 min,
  tests 41 s); cold 30 min (vcpkg 19 min, engine 11 min).
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
| 2026-11-21 | 7/8 = 88% (11-20 23:06 to 11-21 19:34; every finished run green since 451c28bb; the one red is a 2 s cancel during my own burst) | 11.0 min (7 pushes) | release linux 6.9, windows 11.0, macos 5.8 (branch); launcher 3.6 / 3.5 | none in window | 2/2 (v0.1.10, launcher-v0.1.4) | 190 (15 runs over 1.0 day) |
| 2026-11-20 | 5/15 = 33% before tonight (the hierarchy move: 2c006128 onward red until 9c412f43) | 10.6 min (12 pushes) | release linux 10.2, windows 12.7; launcher 2.7 / 3.4 | release windows 60 min (vcpkg cold after the move) | 3/5 (v0.1.7, v0.1.8 failed on Windows) | 235 (398 min over 1.7 days) |
| 2026-11-17 | 3/7 = 43% | not measured | release ~18 min wall | unknown | not measured | not measured |
| 2026-11-19 | 3/5 = 60% (since 11-18 22:00; 7f82097d cancelled unsuperseded, 7321a457 red Windows terra, abd83088 red Linux Package + Windows terra) | not measured (GitHub API down) | not measured | not measured | 1/2 (v0.1.5 failed, v0.1.6 ok; launcher-v0.1.3 ok) | not measured |
| 2026-11-18 | 9/13 = 69% (since 11-17 21:00; the last 4 release runs green in a row) | 9.6 min (11 pushes) | release linux 10.0, windows 14.6; launcher 3.9 / 3.3; macos 3.6 min | release windows 36.4 min; macos 30 min | 2/2 (v0.1.4, launcher-v0.1.2) | 305 (324 min over 1.1 days, includes tonight's bring-up) |

## Plan
0. **(tonight, the developer's direction, before macOS) Official mods publish with each release.**
   Why: every new world pins the official mods (`registry/mods/*`, today `@terraverse/materials`
   and `@terraverse/commands`) at the exact versions the game ships, in its own server folder
   (`shell/bridge/world_server.cpp`, `<world>/server/package.json`). Adding any registry mod to a
   world runs `terra install`, which resolves those pins **on the registry**. So a released game
   whose official mod versions aren't published breaks "add a mod" for every new world.
   Done when: a tag publishes every official mod whose version isn't on the registry yet, before
   the game's packages go up, and a dispatch run proves it with `--dry-run`.
   - [ ] `apps/game/scripts/release/publish-mods.ts` (no logic in YAML): for each folder in
     `registry/mods/`, read its `package.json` name and version; ask the registry
     (`GET {ARCLABS}/api/terrafirma/registry/packages/<name>`) whether that version exists.
     Published: skip it (versions are immutable). Not published: `terra publish <folder> --json`.
     `--dry-run` runs `terra publish --dry-run` instead and publishes nothing. Fails loudly on
     any refusal: a release must not go out pinning a version the registry doesn't have.
   - [ ] Same content, same version: a mod whose files changed since its version was published
     must have its version bumped, or worlds made by different builds would pin one version with
     two meanings. The script detects it (compare against the published tarball's contents, or
     record the commit each version was published from: your call, keep it simple) and fails
     with "bump <name>". The release bot does the bump (its skill says so).
   - [ ] release.yml: a job that runs it on a tag, after packaging and the smoke boot are green and
     **before** the game's packages publish. Linux only: the mods are TypeScript, one publish is
     enough. Dispatch and master pushes run it with `--dry-run` (a master push must never publish).
   - [ ] Credential. `terra publish` reads a signed-in account from `~/.config/terra/account.json`
     (`apps/cli/src/terra/registry/account.ts`); it has no token from the environment. Add one
     (e.g. `TERRA_TOKEN` read by `Account`, taking precedence over the file: the smallest change,
     in apps/cli, with a test) and use a GitHub secret for it. The account must be a member of the
     `terraverse` org on ArcLabs (the developer owns it). **Needs the developer:** the secret
     itself; ask under Questions for the developer, and until it exists, ship everything else with
     the publish step running `--dry-run` and saying so in the job summary.
   - [ ] `bun check:registry` already exercises publish against staging (`../arclabs`): reuse its
     helpers rather than writing a second registry client.

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
   - [ ] Hold it to 5. Green again from 9c412f43 (2026-11-20 23:05), after 9 red pushes on the move.
2. **(current, the developer's direction) macOS online.** Done when: release.yml builds, tests,
   packages and smoke-boots macOS arm64 on master pushes, and a tag publishes a macOS package.
   - [x] Dispatch-only macos.yml (2026-11-18).
   - [x] Compiles (sidecar SOCK_CLOEXEC, st_mtimespec), links (curl/FreeType targets, system OpenAL),
     unit tests pass (shell guarded), 2026-11-18.
   - [x] 2026-11-20: `apps/game/scripts/release/smoke-server.sh`, Linux and Windows on it (2d28a08f).
   - [x] 2026-11-20: macos.yml builds UI and terra and smoke-boots `.output/bin/luanti` (run 44836357243,
     green, 22 min: a world boots headless on macOS).
   - [x] 2026-11-21: macOS is a non-publishing job in release.yml on every push and tag (38b395f5);
     macos.yml deleted. Branch run 44950944032: macOS 5.8 min warm.
   - [ ] **Next:** the `.app` (below), since the shell is what stands between macOS and a playable client.
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
- (none)

## Direction

2026-11-21 (the developer): publish the official mods with every release, tonight, before going on with macOS. Milestone 0 above has the why and the shape.

Over the next few sessions we want to get everything setup properly to allow macos. So choose chunks of work to do. getting the repo itself in line and setting up the appropriate ci. you are tasked with the full thing. within a few sessions we should have macos online. permission to make the changes you need to get this working and online. keep track of this work and your plan until the job is complete.

## Log
- 2026-11-21: Official mods publish with each release (milestone 0, the developer's direction): terra reads TERRA_TOKEN (556ceb2b), publish-mods.ts and the mods job, which publish needs (91e750e4); a changed, unbumped mod fails a tag. macOS moved into release.yml, non-publishing (38b395f5), macos.yml gone. ci:status miscounted superseded cancels (a07919e6). Branch runs 44949973289 and 44950944032 green on every platform, packages within 2 KB of master's. Milestone 1 held (green streak 7). Green rate 33% to 88%.
- 2026-11-20: The hierarchy move broke master in 8 places (terra, launcher, ops.h gen, two publish key paths, terragen on gcc-12 and clang-cl, winresource.rc, the deleted official-mods server folder, manifest.ts, gate.sh); fixed one CI round at a time, green at 9c412f43. Smoke boot is one script for all platforms (2d28a08f). Cache keys fixed (stale since the move) with an ABI-safe vcpkg fallback; macOS boots a world headless in CI (milestone 2 step 3). My first commit swept in a staged deletion (restored b6d9f331). Green rate 60% → 33% over the window (all red from the move), now green.
- 2026-11-19: Master red again (sprint 119 + registry). Fixed Linux Package (builtin/ gone), verified by local package + smoke boot. Windows terra failure undiagnosed: GitHub's API unreachable all session; 00c128d6's run not watched. Green rate 69% → 60%.
- 2026-11-17: First session. Fixed red master: the launcher test `leaves_the_appimage_behind` assumed ':'-separated path lists and failed on Windows; it is now `#[cfg(unix)]`. Baseline green rate: 43%.
- 2026-11-18: Master red since 05ec8cb6, fixed through five layers (gitignored terra sources, release's path filter, terra on Windows, cxxabi.h, sprint 116's red tests skipped by name); 4 green in a row since. CI's unit tests honour skip.txt. `bun ci:status` (milestone 3 done). macOS from nothing to green on a dispatch run: compiles, links, unit tests pass, warm 3.6 min. Green rate 43% → 69%.
