# Home: where Terraverse keeps things

One folder holds everything Terraverse keeps on a machine, laid out the same by every program
that reads it: the engine (`src/shared/platform/home`), terra (`apps/cli/src/terra/home`) and
the launcher (`apps/launcher/src-tauri/src/home.rs`). Modelled on `~/.axon`. Change this file
first, then all three together.

## The root

| Build | Root |
|---|---|
| Installed (launcher, release engine, compiled terra) | `~/.terraverse` (Windows `%APPDATA%\Terraverse`) |
| From source (`.output` engine, `terral`, `bun dev`, a debug launcher) | `~/.terraverse-dev` (Windows `%APPDATA%\Terraverse-dev`) |
| Tests, or anything told to | `TERRAVERSE_HOME` (absolute) |

Installed and source builds never share state: working on one can never quietly repoint the
other, and a source build can never damage a player's worlds. Which is which is decided when
it's built (the engine's `TERRAVERSE_RELEASE`, terra's `--define`, the launcher's debug
assertions), never at run time.

## The layout

```
<root>/
  settings.conf              the game's settings: this machine's, every profile shares them
  launcher.json              the launcher's settings
  versions/<v>/              installed builds: the program only, read-only
  launcher/  bin/            the launcher's AppImage, and the `terra` shims on PATH
  profiles/
    local/                   playing signed out
    @<handle>/               one per ArcLabs account, named by its handle
      profile.json           { "name", "handle" (null for local), "created" }
      account.json           the game's sign-in (account-<host>.json for another backend)
      terra.json             terra's sign-in (terra-<host>.json likewise)
      worlds/  servers/      its worlds, and server folders it hosts
      mods/                  mods it's developing: found by default, shown in Community
      packages/              mods installed from the registry
      textures/ parts/ backups/ screenshots/ mod_data/   the engine's per-player files
  cache/                     rebuildable: delete it and nothing is lost
    active-profile.json      { "profile": "@cody" }
    launcher-versions.json
    engine/                  media, terrain grids, far tiles, the galaxy
    shell-<profile>/         Chromium's cache, one per running profile
    dev/                     terra dev's scratch servers
    logs/  crashes/  traces/  perf-reports/
  running/                   live processes' locks and state
```

## Profiles

- **Which one runs:** `--profile <name>` (the engine), then `TERRAVERSE_PROFILE`, then
  `cache/active-profile.json`, then `local`. A name is `local`, `@handle`, or letters, digits,
  `_` and `-` (a test player: `two`).
- **Signing in** (in the game or with `terra login`) makes `@<handle>` the active profile and
  keeps the sign-in there. The first time an account signs in on a machine and it has no
  profile yet, it adopts `local` as it is (renamed), so nothing played signed out is
  stranded; `local` starts fresh after.
- **Signing out** forgets the key and makes `local` active. The `@handle` profile stays, with
  its worlds, for the next sign-in.
- The engine switches profile in the menu without restarting: only the profile's folder
  changes, and everything reads it when it needs it. Chromium's cache is the one thing fixed for
  a process (`cache/shell-<profile>`, by the profile it started as).

## Moving in

Once, the first time a build of the new layout starts with no `profiles/`, it moves what the
old layout left in the root into `profiles/local/` (worlds, servers, the account, textures,
parts, backups, screenshots, mod_data), `minetest.conf` to `settings.conf`, and the rest
(media, logs, traces) into `cache/`. A source build also moves in what `.output/` held. A
`MOVED.md` is left wherever things were taken from, saying where they went.
