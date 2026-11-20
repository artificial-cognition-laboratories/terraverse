# The launcher

How Terraverse reaches players: a small launcher that installs and runs game
versions, on Linux and Windows. The game is free and open; releases are public.
Revenue comes later, from managed hosting of worlds.

## Decisions

- **A separate launcher, not versions inside the game.** One process can't hold
  two engine builds, and the thing that lets a player roll back must not break
  with a bad build. Switching version is quit, pick, relaunch.
- **A version picker, like Minecraft's.** Play launches the newest version (of
  the channels shown), or one pinned. No worlds in the launcher: choosing,
  creating and upgrading worlds live in the game. No boot profiles until
  something needs them (per-profile mod sets, perhaps).
- **Tauri.** The system webview (WebView2, WebKitGTK) and a small Rust backend,
  about 5–10 MB, with a signed self-updater, installers, single instance and
  deep links. The page is Vue on the game's kit, theme and fonts.
- **The account is the game's.** The same ArcLabs device sign-in, the same
  `Account` and `Social` types, the game's own `ProfileDropdown`, the same
  `account.json`. Signing in isn't needed to play (singleplayer works
  without); it brings friends and multiplayer.
- **Everyone sees every version**, betas included. "Betas count as latest" in
  Settings decides whether Play takes a beta when nothing's pinned.
- **Separate version numbers**: the launcher is `launcher-v*` and rarely moves.
- **No usage data.** Crash reports only, sent by the player.
- **Public builds**, archives on GitHub Releases described by a signed manifest.
- **ArcLabs** is the auth provider and takes crash reports. Terraverse's API
  endpoints may move out of ArcLabs later; auth stays.
- **Lives in `apps/launcher/`**, importing from `apps/game/ui/src` as `@game`.

## The page (done, against the mock)

`bun run ui` (browser, `?scenario=normal|fresh|offline|crash|failing|running`,
`#versions`, `#settings`) or `bun run dev:mock` (the Tauri window).

- **Rail:** Game, Versions; Settings and the account at its foot.
- **Game:** the art, the title as the main menu draws it, and floating at the
  bottom right the version picker (Latest or a pinned one) and one Play button
  that follows the version: Install & play, progress and Cancel, Play,
  Playing, Retry.
- **Versions:** the list by channel; one centred page per version (icon,
  name, meta, Play / Pin / Remove, release notes).
- **Settings:** beta builds, what the launcher does when the game starts
  (hide, close, stay), keep old versions, data folder and disk, report a
  problem, about.
- **Account:** the game's dropdown, from the rail's foot.
- **Crash:** a report dialog after an abnormal exit; the same by hand from
  Settings.

The contract is `src/backend/types.ts`; `mock.ts` fakes it, `tauri.ts` calls
Rust and says "not wired yet" for every command that isn't.

## The versions

Built: every GitHub release carrying a `manifest.json` signed by CI
(`scripts/release/manifest.ts`), verified against `scripts/release/update-key.pub`,
the key the game trusts. A pre-release is a beta; the release's text is its
notes. Nothing new was needed in CI or ArcLabs for Linux. What a manifest could
still carry (not yet: the launcher shows nothing for them):

```jsonc
{
  "versions": [                // newest first
    {
      "id": "0.3.0-beta.2",
      "channel": "beta",       // stable | beta
      "released": "2026-11-08",
      "notes": "…",            // paragraphs, **bold**, "- " lists
      "protocol": 49,          // multiplayer: who can join whom
      "launcher": "0.1.0",     // the least launcher that can run it
      "builds": {
        "linux-x86_64":   { "url": "…", "size": 0, "sha256": "…" },
        "windows-x86_64": { "url": "…", "size": 0, "sha256": "…" }
      }
    }
  ]
}
```

`protocol` and `launcher` are the additions; the rest is today's shape.

## On disk

`TERRAVERSE_HOME`: `~/.terraverse` on Linux, `%APPDATA%\Terraverse` on Windows.

```
launcher.json          settings: pinned, beta, on launch, keep old
launcher-versions.json the last versions fetched (verified then), for offline
versions/<v>/          read-only, complete or absent (unpacked beside, then renamed)
<the game's own>       worlds, settings, the ArcLabs key: shared by every version
logs/session-<UTC time>.log   each game session's output; its first line names the version
crashes/
```

The launcher and the game share the game's saved ArcLabs key, so signing in
either place signs in both, and nothing is handed over at launch.

# Getting it

- **Linux:** the download page's button copies
  `curl -fsSL https://terraverse.arclabs.it/install.sh | sh`
  (`scripts/release/install.sh`, tested by `install-check.sh`, run by
  `gate.sh`): the launcher's AppImage, checked against signed
  `launcher.json`, into `~/.terraverse/launcher/`, a menu entry, a
  `terraverse` command, then it opens. Under it, the AppImage and `.deb`
  by their fixed names on the `launcher` release.
- **Windows (phase 3):** `terraverse-setup.exe`, Tauri's NSIS installer: per
  user, no admin, Start menu, WebView2 if missing, updates itself silently.
  Signed before strangers see it.

# Toward production

Five workstreams. The first milestone is **Linux, end to end**: a tester
installs the launcher, it installs a real build from a real manifest, plays,
and a crash reaches ArcLabs. Windows is the second.

## 1. The launcher's backend (Rust): built, but for self-update

`src-tauri/src/`, a module per concern; `cargo test` (and `-- --ignored` for
the real releases). All seven are wired; 7 is proven by the first
`launcher-v*` tag (`.github/workflows/launcher.yml`).

1. **Local:** `listen` (Tauri events), `settings` (`launcher.json`),
   `installed` (`versions/`), `about` (with free disk), `openDataFolder`,
   `openUrl`. Single instance.
2. **Manifest:** fetch, verify the signature, cache; refuse a version whose
   `launcher` is newer than us ("update the launcher").
3. **Install:** streamed download with progress, resume, SHA-256, unpack
   beside then rename, `cancel`, `remove`, keep-old pruning.
4. **Launch:** run `versions/<v>/terraverse` with `TERRAVERSE_HOME`, capture
   its output to `logs/`, watch it, tell a crash from a quit by exit code,
   hide / close / stay as set, come back on exit.
5. **Account:** the device sign-in against ArcLabs, writing the game's key
   file; `access` (beta) and `social` from the same endpoints the game uses.
   One implementation of the protocol in Rust beside the game's in C++: keep
   them to the same endpoints and file, and test both against ArcLabs.
6. **Reports:** collect the session log, system info and a dump, upload to
   `POST /api/terrafirma/reports` (`{ session, version, platform, launcher,
   system, log, note, dump }`, the key as bearer if signed in). Until ArcLabs
   has it, a 404 tells the player where the log is kept.
7. **Self-update:** Tauri's updater, its own signing key, its own release tag
   (`launcher-v*`).

## 2. The game, launched

1. **User data audit:** nothing written beside the binary or into
   `versions/<v>/`; everything under `TERRAVERSE_HOME`.
2. **Version stamps:** built already (`release::openWorld`: `tf_saved_by`,
   a copy in `backups/`). Left: its refusal says "Run terraverse update",
   which should name the launcher when launched from it.
3. **Exit codes** that mean something (0 quit, a fixed one for a refused
   start), and **crash dumps** (Crashpad ships with CEF) to `crashes/`.
4. **Launched mode:** `--launcher` hides the menu's "run `terraverse update`"
   notice (the launcher does updates) and adds "Back to launcher" to Quit.
5. **The key file:** document its path and shape as a contract; no format
   change without the launcher.

## 3. Releases (CI)

1. Each `v*` tag: build Linux (exists) and Windows, upload archives, compute
   sizes and hashes, write and sign the manifest entry, publish the manifest.
2. Notes from the tag or a `notes/<v>.md`, so the launcher shows real ones.
3. Channels: `-beta.N` tags are beta, plain tags stable.
4. Yank: drop a version from the manifest without deleting its archive.
5. Launcher releases (`.github/workflows/launcher.yml`): `launcher-v*` builds
   the AppImage and deb, the updater's `latest.json`, and `launcher.json`
   (signed with the game's key) for `install.sh`. The rolling release
   `launcher` keeps fixed names for the download page. Windows' NSIS `.exe`
   is a second job, added with the Windows engine (phase 3).

## 4. Windows

1. The engine on CI: MSVC on `windows-latest`, vcpkg for dependencies, CEF's
   Windows build in `CEF.cmake`, LuaJIT.
2. Our code: Winsock and UPnP in `online/`, paths, any POSIX in our folders.
3. Determinism: `check:terragen` goldens on Windows; Jolt's
   `JPH_CROSS_PLATFORM_DETERMINISTIC`, and a Windows client against a Linux
   server.
4. Packaging: a zip the launcher unpacks; `terraverse.exe` as the entry.
5. Signing: Azure Trusted Signing for the launcher and the engine, before
   strangers see SmartScreen.
6. A Windows machine to debug GPU and CEF problems CI can't.

## 5. ArcLabs

1. Device sign-in: exists. Check it serves a second client (the launcher).
2. **Crash reports:** built in `../arclabs/apps/backend`: `POST
   /api/terrafirma/reports` (public, 20 an hour per caller), one row each in
   `terrafirma_reports`, dumps in the private `terraverse-reports` bucket,
   staff routes under `/api/staff/terrafirma/reports`. Left: the migration
   applied, the bucket made, and a staff page to read them.
3. The versions: GitHub Releases is enough (60 unauthenticated API calls an
   hour per address, one per check); ArcLabs can front it later.
4. A download page for the launcher.

## Open questions

- The key file is plain text under `TERRAVERSE_HOME`. Fine for a beta; the OS
  keychain later, for both the game and the launcher at once.
