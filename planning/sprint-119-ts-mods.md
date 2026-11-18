# Sprint 119: TypeScript mods

Lua is replaced by a TypeScript mod API. The API is designed docs-first in
`apps/site/content/docs/mods/`: those pages are the spec, and this sprint makes them true.

## The design

- **`apps/cli` is the `terra` command** (Bun, compiled with `bun build --compile`, shipped
  beside the engine from day one). It is the only thing that understands server and mod
  folders: package.json, bun.lock, TypeScript, bundling.
- **`.terra/` is the contract.** `terra prepare` turns a server folder into `.terra/`: a
  bundle per mod and half, the load order and a manifest. The engine reads `.terra/` and
  `save/`, never package.json or TypeScript.
- **Mods run in a sidecar.** The engine spawns `terra runtime <folder>`, with one socket on
  fd 3. A crash in a mod is a restarted process, never a dead server, and reloading is
  restarting it.
- **Step windows.** Mod code only touches the world inside a window the engine opens and
  waits on: each server step sends one `step` (joins, timers, events). The sidecar runs the
  handlers, lets their promises settle, and answers `idle`. Until then the engine answers its
  calls. So `world.get` is synchronous, and a `get` always sees the `set` before it, because one
  ordered channel carries both. A decision (a guard, a block's `on.place`) is a window of its
  own. Outside a window, a call waits for the next step.
- **Wire.** Frames are `u32 length` + MessagePack `[op, id, payload]`. Ops are declared once,
  in `apps/cli/src/runtime/wire/ops.ts`, and `bun run ops` (in apps/cli) writes
  `apps/game/src/sidecar/ops.h` from it.
- **Content at boot.** The sidecar runs every mod's server half and sends one content
  table. The engine makes node definitions from it, before registration closes. On reload,
  the same table is behaviour-only: the world never pauses. A different one needs a restart.
- **Lua runs alongside** until the `tf_*` mods are ported, then goes.

## Stages

1. **The sidecar runs.** `@terraverse/types` with a first slice of the globals.
   `terra prepare`, `terra host` and the hidden `terra runtime`. The engine's
   `sidecar/`: spawning, the handshake, the content table, step windows, the watchdog.
   Blocks with a texture, `players.on("join" | "leave")`, `player.tell`, `game.announce` and
   `time.after`, `every` and `wait`. Proved by `bun check:mods`.
2. **The surface.** World reads and writes (`get`, `set`, `find`, `fill`). Block handlers
   with `Refused`, and guards. Commands. Roles and permissions. `mod.settings` from
   `server.config.ts`, stores in `save/store`, schedules.
3. **`terra dev`.** Watch, re-prepare, swap `.terra/` atomically, restart the sidecar.
   The same content table keeps the world running; a different one is a fast restart.
4. **Folders.** `terra init`, `install` and `uninstall` (the shipped binary as bun),
   `server.config.ts` worlds to `save/worlds`, `.terra/types`, message and store validators,
   and the game's own hosting calling `terra prepare`.
5. **Client halves** in the CEF page, sandboxed per mod: client bundles sent on join,
   panels, messages.
6. **Port `tf_*`**, with matter on blocks and reactions' `on`.
7. **Remove Lua.** Windows named pipes before the first Windows release with mods.

## Stage 1: the sidecar runs

### Built

- **`apps/cli`: `terra`**, compiled to `.output/bin/terra` (`bun run build`, part of the root
  `bun run build`, built in CI on both platforms, shipped in the Linux package). Output through
  `@arcforge/arcline`.
  - **`terra prepare [folder]`** installs if package.json is newer than bun.lock (the binary
    is bun too, via `BUN_BE_BUN`), finds the mods (dependencies with a `mod.config.ts`), checks
    prefixes don't collide and mods only build on mods the server runs, orders them by
    dependency, and bundles each `server/main.ts` to `.terra/build/<prefix>/server.js` with
    inline source maps. Writes `.terra/manifest.json`.
  - **`terra host [folder] [--port n]`** prepares, then runs the engine beside it on
    `save/worlds/main` with `--server-folder`.
  - **`terra runtime <folder>`**, hidden: the sidecar.
- **A mod imported by another mod is the loaded one.** Bun's compiled binary doesn't resolve bare
  packages from disk, so prepare turns `import … from "@you/kilns"` into the module the sidecar
  already loaded (`globalThis.__terraMods`), never a bundled copy. Only by its name: a subpath is
  refused, as is importing a mod you don't depend on.
- **Local mods are used where they are.** Bun copies `file:` dependencies into node_modules; prepare
  reads `mods/<name>` itself, so an edit is seen without reinstalling.
- **The sidecar** (`apps/cli/src/runtime`): reads fd 3 with blocking reads (about 60–140 µs a
  window, two event loop turns included). `content.blocks.create` (texture, matter accepted and
  not yet simulated), `game.players.on("join" | "leave")`, `list`, `get`, `player.tell`,
  `game.announce`, `game.time.now`, `after`, `every` and `wait` in game time. Everything else the
  docs show throws `NOT_WIRED`. Console output and failures go to the server's log under the
  mod's name, kept across awaits by AsyncLocalStorage. A throwing handler is reported and stays
  registered.
- **The engine's `sidecar/`**: posix_spawn with a socketpair on fd 3, a small MessagePack codec,
  the content handshake (60 s to load), a window per server step after the environment steps,
  under its lock. A window over 50 ms is reported; one hung for 10 s is killed. A dead sidecar is
  restarted, seeded with who's here (no joins replayed), its load-time timers counting from its
  first window. Five deaths in a minute leave mods off. Windows: refuses with "not wired".
- **Content into the engine**: each block an item and a node before registration closes, its
  textures as media named `<prefix>--<file>`. TEMP: every block digs like stone (cracky 3)
  until matter decides it.
- **`@terraverse/types`** (`packages/types`): the globals that are implemented, nothing more.
- **`bun check:mods`**: the fixture in `scripts/mods-check/server` typechecks against the types,
  prepares, and runs headless: load order and shared exports, blocks, timers, a throwing handler
  with its TypeScript line, awaits across windows, a crash and restart, and no sidecar left
  behind.

### The CLI's shape

Rebuilt to the ArcLabs standard (`../arclabs/AGENTS.md`, Coding Standards): capitalized
factories returning handles, orchestrators that only wire, leaves that own one concern, one
`index.ts` per module as its only door. Three roots, one per runtime:

- **`Terra({ exec })`** (`src/terra`): the work, printing nothing. `projects` finds and opens
  projects of every kind, as in Axon: `KINDS` is a table of what makes each kind different
  (its config file, whether it installs, whether it runs mods), so a new kind (texture packs,
  shaders, skins) is a row, not branches. `Project` is the same surface for every kind (frame,
  prepare), plus `server`, the server kind's extension (mods, build, manifest, worlds).
  `projects.server(dir)` refuses any other kind and narrows `prepare()` to a
  `PreparedServer`, the only thing `engine.host` takes. `Frame` owns the `.terra/` layout.
- **`Runtime({ manifest, wire })`** (`src/runtime`): the sidecar. Channel (windows, calls,
  logs), Running (which mod, across awaits), Api (handlers, content: blocks and media; game:
  players and time), Loader, Globals (installed, and restored), Lifecycle. The wire is
  injected: `MemoryWire` plays the engine in memory.
- **`Cli({ exec, renderer? })`** (`src/cli`): presentation only. Presentation, Boundary (a
  thrown error → arcline's errorReport and an exit code), `parseArgs`, Commands (one leaf per
  command).
- **`src/errors`**: one map of every failure terra can name; `err("CODE", { detail })`. The
  runtime's codes are the ones mods catch.

Tests live in `apps/cli/tests/<module>/` (cli, terra, runtime), each importing the leaf or root
it tests: args, Cli output, kinds, Mods, projects and preparing both kinds, Time, Players,
Blocks, the whole Runtime on a MemoryWire. `check:mods` runs them first; CI runs them with tsc.

## Stage 1b: folders, worlds, dev

### Built

- **Globals, no imports.** `packages/types/*.d.ts` is built into terra (`bun run gen` writes
  `globals.gen.ts`; `gen:check` in CI). Prepare writes `.terra/types/`: the globals each part
  of a project uses, and one tsconfig per part (KINDS' `types`): `root` (configs and shared
  folders: `defineServer`, `defineMod`), `server/` (server globals), `client/`. Each part's own
  `tsconfig.json` extends its frame, so the nearest one is the right one. A server's prepare
  types its local mods too.
- **`terra server init` / `terra mod init <name> [--scope @you]`**: scaffolds from KINDS'
  `scaffold` (pure templates), refuses a folder that isn't empty, then prepares. A new server's
  system is a fresh code from the game's own encoder (`apps/game/ui/src/worldgen/config.ts`, now
  free of Vue), so it's never a stale version.
- **`server.config.ts` read and checked**: title, system, worlds, settings, access.
- **`ConfigEditor`**: sets or removes a value in a config by AST (@babel/parser) and splices
  bytes, keeping the author's layout and comments. On every project as `project.config`.
- **The universe from its system.** World making moved out of the shell into
  `server/universe.{h,cpp}`. The engine's `--world-code` makes the world on first boot and
  refuses one made from another system. The save is `save/worlds/` (TEMP: one engine world for
  every body, players inside it).
- **Hot reload.** The engine's sidecar checks `.terra/manifest.json` once a second and reloads
  on a new build. Content that differs from what the universe started with stops the server
  with `Exit.restart` (75); `terra dev` and `terra host` start it again. A reload clears mods
  left off after crashing.
- **`terra dev [folder] [--port n]`**, headless: a server as itself, a mod in a scratch universe
  in its `.terra/dev/`. Prepare, run, and on each save prepare again; config changes restart; a
  broken build is shown with file and line and the last build keeps running; Ctrl-C stops it.
  The whole engine log in `.terra/logs/engine.log`. `bun check:dev` drives it all.

### Found on the way

- **Bun's recursive `fs.watch` crashes** (segfault, 1.4.0) while `bun install` fills a
  node_modules beneath it. Watch walks the project and watches each folder on its own instead.
- **A universe with no content** looked like one not loaded yet: a reload adopted new blocks
  silently. The sidecar now knows when its first content came.
- **Per-project names can't be global yet.** A mod imported by another is type-checked under the
  importer's globals, so `OwnName` and `TextureName` stay broad (the runtime checks them) until
  mods ship declarations.
- **One `check:dev` run saw the engine stop with exit 0** on a content change, once, not again
  in eleven runs; the check keeps its folder when it fails, to catch it.

## Stage 1c: the game, settings, PATH

### Built

- **`terra dev` opens the game.** The universe is made in `main()` before either mode (the
  game reads its world before its server starts); the game's own server gets the server
  folder, so its sidecar runs and reloads as the headless one does. A content restart exits
  the game with `Exit.restart`, and dev opens it again. The dev game is its own profile
  (`terra-dev`): Chromium locks its cache to one game, and the player's may be open. It's
  stopped with SIGTERM: Chromium takes SIGINT for itself. `--headless` is the old way.
- **`--fresh`** runs on `.terra/fresh/`, wiped every run: never the server's own save.
- **`--in <server>`** runs a mod inside a copy of a real server: its mods, its config, a fresh
  copy of its save. Scratch universes live in terra's cache (`Places`), never inside the mod:
  bun copies a `file:` dependency whole, and copied the scratch server into itself.
- **`terra <ref>`**: a server folder (`terra .`) in the game, on its own save, restarting on a
  content change; a world code as a clean universe with no mods, kept with the game's worlds
  (the engine places it). A published server says it needs the registry.
- **Flags refuse what a command doesn't take**; `--fresh` and `--headless` are switches.
- **Settings.** Declared in mod.config.ts (number with range, step, unit; boolean; choice;
  string with pattern), chosen in server.config.ts, checked and defaulted by prepare, carried
  in the manifest. `mod` is lexical: each bundle binds its own (`MOD_OF`, a banner), so a
  mod's code reads its own settings whoever calls it. Typed: `mod.settings` from the mod's
  declarations (and those of the mods it builds on, whose source its frame checks), and a
  server's `settings` from its mods'.
- **PATH.** The launcher links `~/.terraverse/bin/terra` to the version it launches;
  `install.sh` writes `terra` and `terraverse` shims: alone the launcher, with a command that
  CLI. An installed engine is started through its `terraverse` wrapper (its libraries).

### Found on the way

- **`mod` resolved by who's running was wrong**: hello's `greeting()`, called by greeter, read
  greeter's settings. The check caught it; `mod` is per bundle now.
- **The sidecar died first on Ctrl-C**: the whole process group gets SIGINT, and Bun exits on
  it. The sidecar ignores it; the engine stops it in order.

### Not doing (yet)

- **`access`** (`join`, `roles`) is read and checked, not enforced. "friends" means the ArcLabs
  ticket gate the game's own hosting uses: a dedicated server would register with ArcLabs and
  run that gate, and checking it needs a second signed-in account.
- **`worlds`** is read and checked; it's used once `game.worlds` is (stage 2).
- **Windows**: the launcher copies terra.exe into place; the sidecar still refuses there.

### Not verified

- A player joining and being told something: the path is there (join and leave events, `tell`),
  but nothing has joined a server in a check yet.

### Not doing (yet)

- `terra .` and `terra <code>` in a window: the paths `terra dev` uses, not opened by hand.
- No typechecking in `terra prepare`: the editor checks; prepare bundles.
- `install`, `uninstall`, `--in`, `--fresh`, PATH.
- Windows: the sidecar needs named pipes; `terra` is built there, the engine refuses.
