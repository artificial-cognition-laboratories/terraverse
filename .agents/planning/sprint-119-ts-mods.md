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
7. **Remove Lua.**

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

## Stage 2a: the sky in the engine, and the first official mod

### Sorting tf_* (against the API in apps/site/content/docs/mods)

Most of the Lua is mechanism, which the docs put in the engine (no tick event, nothing that
reads engine state back and forth); the policy that's left mostly waits on items. Into the
engine: the sky, gravity by altitude, the weather readout, snow and ice, matter's running and
effects, movement, seams, loose ground, and the natural blocks (rock, soil, ore, flora) as
`core:` content. Ported: building materials (done), then `/travel` and felling once
`game.worlds` is wired. Waiting on design: items and inventory (tf_craft, fire tools, pouring),
biomes. Decisions, not ports: `_machine.friction` (now matter's), `set_timeofday` in the Lab
(`time.skip`/`pause`/`step`), the Lab's server-driven camera, the mod menu's live world settings.

### Built

- **`terral`** (`apps/cli/bin/terral`, linked into `~/.local/bin`): terra from source against
  `.output/bin`'s engine. `TERRA_ENGINE` says where the engine is; `TERRA_CLI` tells the engine
  which terra to spawn, so the sidecar runs from source too.
- **The sky is the engine's** (`server/sky.cpp`): tf_sky's loop, the same thresholds, from
  `terraSun` (shared with `core.terra.sun`). tf_sky is gone; its Lab scene moved to tf_world.
- **`@terraverse/materials`** (`registry/mods/materials`): flint, timber, aluminium, steel, rubber,
  canvas, glass, each only matter and share. tf_world's `materials.lua` is gone; its names are
  aliased (`tf_world:steel` → `materials:steel`) and the engine's starters, showcase, moves and
  vehicles use the new ones.
- **The game is a server folder** (`apps/game/server`): `@terraverse/game`, depending on the
  official mods. Staging prepares it and stages `.output/server`; the engine runs it in every
  world it isn't given another folder for; packages carry it.
- **`.terra/` is self-contained**: each mod's assets are copied beside its bundle, and the
  manifest's paths are relative, so a prepared folder runs wherever it's moved.
- **Matter on TypeScript blocks is real**: the sidecar starts between Lua's mod scripts and
  their `on_mods_loaded`, and its blocks register through `core.register_mod_block`
  (`builtin/game/sidecar.lua`, TEMP until Lua goes). tf_matter, machines, the catalogue and the
  palette bake see them as their own.
- **Substances say how strong and grippy they are** (`strength`, `friction`, `transparent` in
  `substances.json`, parsed and in `core.matter.substances()`). tf_matter takes a block's
  strength from its substance by its share, after `_machine` and before dig groups; a
  transparent substance's block is drawn as glass.
- **Wiki scenes find a mod's media** (`prefix--file.png`) in the staged server folder.
- **`check:mods`** prepares and type-checks `apps/game/server` and every mod in `registry/mods`.

### Found on the way

- Lua's `on_mods_loaded` ran before the sidecar started, so tf_matter never saw a TypeScript
  block's matter: `loadMods` takes a step between the scripts and that callback.
- A manifest of absolute paths couldn't be staged or shipped.

### Windows

- **The sidecar runs on Windows** over a named pipe. The engine makes it (one instance, local
  clients only), starts `terra.exe runtime <folder> --pipe <name>` with no window and only its
  output handles inherited, and checks the client that connects is that process. Its end is
  overlapped, so reads wait with the same deadlines; before the sidecar connects the engine
  watches its process too, so a sidecar that dies loading says why. `terra runtime` opens the
  pipe by name; elsewhere it's still fd 3 of a socket pair.
- `sidecar.cpp` is one implementation over a per-platform `Child` (spawn, send, receive with
  a timeout, exited); framing, windows and the watchdog are shared.
- Windows worlds run the game's folder like any other; the engine finds `terra.exe` beside it.
- **Verified:** Linux only (build, check:mods, check:dev): nothing here has Windows headers.
  The Windows code is checked by the release workflow, whose package smoke test now fails
  unless a world logs the game's blocks loading. Bun opening `\\.\pipe\…` with `openSync`
  and reading it with `readSync` is the part most likely to need a fix there.

### Native: flight, planet and weather

Mechanisms out of Lua, as the sky went (`tf_flight`, `tf_planet` and `tf_weather` are gone):

- **Gravity by height** (`server/gravity.cpp`): R² / (R + h)², set when it changes by 1%. Only
  gravity: speed and jump stay whatever tunes them (tf_flight's also reset those, undoing
  sprinting).
- **The planet** (`server/planet.cpp`): the spawn on land nearest the equator (in
  `findSpawnPos`, so new players and respawns alike), one in seam moved to it at join and each
  step, and travel between bodies (held still on the page's screen while the land there is
  made, polled rather than called back). `core.planet.spawn_position` and `core.planet.travel`
  expose them. The seam and the atlas debug paints are `core:` blocks in `builtin/game/planet.lua`,
  with `/travel` beside them as an admin's, like `/teleport`. Old names are aliases.
- **The weather** (`server/weather.cpp`): each player's readout for the page, and snow, ice and
  thaw as the engine's own active block modifiers, through `terraWeather` (shared with
  `core.terra.weather`, as `terraSun` is). The blocks stay the game's (`tf_world:snow`,
  `tf_world:ice`), found by `weather_snow` and `weather_ice` aliases; `core.terra.weather_act`
  is for the check. `/river` and `/rivers` are the engine's (`builtin/game/rivers.lua`).

### Commands: `@terraverse/commands`

- **`game.commands`** as designed: paths, typed args (string, number, integer, boolean,
  player, position; the last string takes the rest of the line), a permission checked before
  `run`, `Refused` as the reply, `COMMAND_TAKEN`. The engine knows only each command's first
  word (`commands.set`, the whole set each time it changes, and always in the first window,
  so a restarted sidecar's replace its predecessor's); every line typed under one goes to the
  next window as an event, and the runtime finds the path, parses, checks and runs it.
- **The chat side** is builtin's (`core.set_mod_commands` in `builtin/game/sidecar.lua`, TEMP
  until Lua goes): a mod's command shadows builtin's of the same name while it's there, and
  answers "the mods are off" if the sidecar is.
- **`player.can` and `player.kick`.** TEMP: admins (the engine's `server` privilege) hold every
  permission and nobody else any, until roles are granted permissions in the setup.
- **`@terraverse/commands`**: `/msg`, `/players`, `/kick` (`commands:kick`), `/announce`
  (`commands:announce`), replacing builtin's `/msg` and `/kick`. In the game's server folder.
- **Protocol 2.** `check:mods` types a command from the console, wrong and right, before and
  after the sidecar's forced restart.
- **Not verified:** a connected player typing one, and the permission check against a real
  player's privileges: the console path and the runtime's tests cover the rest.

### The skeleton: cut to what's built

There's no game yet, so half-built game systems went rather than be ported:

- **`tf_craft`** (making: forms, processes, stations, carry weight), its making screen, the carry
  meter, and pressing F at a station.
- **The Lab** (`tf_lab`, every scenario, `shell/lab.*`, its worlds, the L key, the camera
  director it drove, `check:lab`).
- **`tf_matter`'s hand tools** (fire drill, bellows, heat gun, blowtorch, blast gun, flamethrower,
  probe, scoop), kindling, charcoal and fired clay, pouring (the X key; bit 10 of the player's
  keys is unused now), and the probe card. Kept: the matter mechanism, substance blocks, the
  crucible (the wiki's chemistry scenes), and the flame (burning air's light). `check:matter`
  lights leaves for tinder.
- **The flight HUD** (nothing sent it since the flight pack) and **the world controls panel**
  (its time, player, travel, world, items, tuning and view pages). `view_*` and `tune_*` stay
  engine settings, set in the config.
- **`tf_player` is native** (`server/player.cpp`): the body visual, the inventory's layout, the
  hotbar, the creative catalogue's take and clear, sprinting, the map (the body, what's explored,
  where they are, Go there), and the wiki rule. Creative is the world's, no longer switched per
  player: it grants fly, fast, noclip, creative and give (`getPlayerEffectivePrivs`), digs
  anything at once (the client's dig time, the server's anti-cheat), and can't be hurt.
  **Verified** by the real client joining a creative planet world with a probe: body, inventory,
  hotbar, privileges, immortality, no health bar, the explored map saved, gravity by height.
  **Not verified:** sprinting, instant digging and the catalogue, which need keys pressed.

### Matter native, the world as data: no game Lua left

- **Blocks carry their matter natively.** `ContentFeatures` holds a block's `_matter`, `_family`
  and `_machine` (server only, never sent); `server/matter.cpp` registers every block with the
  world's matter and with machines once the mods have loaded (`terraRegisterMatter`), as
  tf_matter's `on_mods_loaded` did. The world's matter left `script/lua_api/l_matter.cpp` for
  `server/matter.cpp`; the Lua API is a thin layer over it. **Verified:** the palette, baked from
  the registered matter, came out unchanged.
- **The matter runs natively** (`terraStepMatter`): stepping, the weather into its cubes, flames
  (with a flame node's light), smoke and sparks, burning in heat, what each player saw react, and
  saving every minute and at shutdown. Digging and placing wake what's beside them. Dropped, as
  half-built rules: hot carried items, and naming a family block's item by its composition.
- **The world is data** (`assets/world/*.json`, staged as `world/`): every block as its
  definition (rocks with their blends and the ores precomputed), soils as their parents and
  stoniness (computed at load from terragen's points), the biomes, the tree species, and the
  generator's and old names as aliases. Dumped from the Lua by running it under a stub, so
  nothing was retyped. Names are `core:` (aliases keep `tf_world:` and `tf_matter:` worlds).
  `builtin/game/world.lua` and `matter.lua` register them through Lua, TEMP while digging and
  placing look blocks up in `core.registered_nodes`; `core.terra.data` hands builtin the files,
  which mod security keeps it from reading.
- **Native:** the shore's fills (`mapgenShore` on the emerge thread, filled on the server's next
  step) and loose ground keeping its shape where a block's dug or built (`groundChanged`).
  Builtin keeps the hand, drops landing on the ground, and the machine entity (`core:piece`,
  `tf_world:piece` too: entities have no aliases).
- **Dropped:** felling, the starter kit, crafts, sandstone bricks.
- The game's textures are the engine's (`registry/textures/base/pack`, Good Vibes' licence
  beside them). **Credits gap:** `matter_flame`, `_smoke`, `_steam` and `_spark` never had an
  entry, and their history doesn't say where they came from.

### Removing Lua: the dead parts first

What's left of Lua is Luanti's stock plumbing the engine still calls, not game logic: content
registration (data passed through), digging and placing (`item.lua`: the one real piece), dropped
items and falling blocks, password auth and privileges, the stock chat commands, `core.after`.
The order: dead code, then native auth and privileges, a native content registry with minimal
digging, the Lua-probe checks as C++ tests, and last the engine's `m_script` hooks, `src/script/`,
`builtin/` and LuaJIT.

- **Gone:** the Lua main menu (`GUIEngine`, `builtin/mainmenu`, `fstk`; the non-shell build now
  says it has no menu), client-side Lua and SSCSM (`ClientScripting`, `builtin/client`, the
  `sscsm_test0` preview, the client's mod VFS, the `.` local command key), the pause menu's Lua
  (its Settings button: the shell has its own), the Lua profiler, devtest and the upstream client
  mods. Their settings went with them.
- **Protocol 104:** `TOCLIENT_CSM_RESTRICTION_FLAGS` is gone.
- **Found on the way:** builtin registered biomes in the unit tests' mock server, which has no
  generator, so TestSAO and TestMoveAction crashed since the world became data:
  `core.terra.generates()` says whether there's one. TestScriptApi loads builtin as `test`.

### Auth, privileges and chat, native

The design was already in the docs (`server/setup.md`, `api/game/players.md`): everyone is their
ArcLabs account, roles hold permissions, a grant lasts while they're here.

- **Joining:** a world played alone offers `AUTH_MECHANISM_LOCAL` and accepts at once (its server
  binds 127.0.0.1); a hosted one offers `TICKET`; any other server refuses ("this server takes no
  one"). SRP, legacy passwords, sudo mode, password changes and login/register are gone, client and
  server (protocol 105), with the auth database in every backend, `--migrate-auth`, `auth.lua`,
  `core.auth`, the password helpers and their settings.
- **Access** (`server/access`): permissions are `prefix:name`, the engine's `core:` (`core:fly`,
  `core:interact`; the client checks those names now). Roles: `player` (everyone: interact, shout),
  `builder` (creative, fly, fast, noclip, give), `admin` (anything, mods' permissions too). The owner
  (`singleplayer`, or the host's account) is always admin; others' roles are kept in
  `<world>/access.json`, which fails loudly if it's malformed. Grants end when they leave. The
  sidecar's `player.can` asks it, so the TEMP "admin is `server`" is gone. TestAccess.
- **Chat:** a line is broadcast as `<name> line`; `/word` runs the engine's command (`server/commands`:
  `/travel`, `/river`, `/rivers`, ported from Lua) or a TypeScript mod's, and `/help` lists both. Join
  and leave messages are the engine's. `chat.lua`, `chatcommands.lua`, `privileges.lua`, the help
  formspecs and `core.set_mod_commands` are gone. The checks' probes type commands with
  `core.console_command` (TEMP, until they're C++ tests).
- **Not yet:** a way to give someone a role in play (`player.roles.add` in the API is designed, not
  wired), `join` (`anyone`, `friends`, a list), and mods declaring their permissions.

### No Lua: content, digging and the checks, native

The last of Lua went: `src/script/`, `builtin/`, the vendored Lua, LuaJIT and every hook the
engine called into (protocol 106). What it still did is native:

- **Content** (`server/registry`, `server/defs`): the hand, the world's blocks and soils, a block of
  every substance (solid, powder, liquid, puddle), the flame and crucible, the seam and the atlas's
  debug paints, aliases, biomes and tree species, all from `assets/world/*.json` and the matter
  table, then the TypeScript mods' blocks. A definition is read by the fields content uses and any
  other is refused. Where the world and the matter both define a block (`core:water_puddle`) the
  world's wins: under Lua the substance's had quietly overwritten its shallow water.
- **Digging and placing** (`server/interact`): a dug block falls where it was as items (its
  `drop`, or itself with its colour); placing goes into what gives way or beside it, facing the
  way it's put down where it `_rotates`; tools wear. TEMP: a dug block gives itself whatever digs
  it, until breaking is tool × material. In creative what's in hand never runs out (it did only
  for rotating blocks under Lua).
- **What falls** (`interact::settle`): sand and gravel over nothing fall, a flower whose ground goes
  comes off as items, after a dig, a placing, a punch, or a liquid moving.
- **Entities** (`server/kinds`): a machine's carrier (`core:piece`), a dropped item (picked up by a
  punch, merging with others lying close, gone after `item_entity_ttl`) and a falling block, under
  the names worlds saved them by; an entity of no kind is kept inert. Items and falling blocks saved
  by Lua are read back by their item or block name.
- **Death**: the client's own death screen (TOCLIENT_DEATHSCREEN_LEGACY), and the server respawns
  them when they choose (TOSERVER_RESPAWN_LEGACY).
- **Inventories** answer moves by their rules alone; dropping throws the stack in front of them.
  Node timers, node forms and formspec replies have nothing to answer them, and say so in the log.
- **The checks' probes** (`src/probe`, `--probe <name>`): the eleven Lua probes as C++, printing
  the lines their scripts already read; `check:interact` is new, digging, placing, dropping and
  falling on a live server. TestLua, TestScriptApi and TestModStorageDatabase went with what they
  tested; TestMoveAction and TestSAO keep what isn't Lua's.
- **Gone with Lua**: mod storage (and its database backends), mod IPC, mod channels (their packets
  null in 106), async and emerge Lua environments, Luanti's mod specs and configuration
  (`content/mods`), `--migrate-mod-storage`, `--insecure-no-sandbox`, `secure.*_mods`, the builtin
  locale. The Community screen keeps texture packs, worlds and seeds; its Lua mods tab went, and a
  world's page no longer counts mods. The debugger's Mods tab lists the server folder's TypeScript
  mods (`.terra/manifest.json`). `LuaEntitySAO` is `EntitySAO`.
- **Checks still red, as before this**: check:bodies (no land found on the moon), check:atlas-world
  (no solid at the equator's sea level), check:atlas-seams (water poured past a seam stays air:
  liquids are the matter's fills now, and the check still expects Luanti's flowing water).

### Not doing (yet)

- **`world` and `block` arguments** say they aren't wired: there's no `game.worlds` yet.
- **Declaring permissions in mod.config.ts**: a permission is only named, `prefix:name`.
- **The rest of builtin's commands** (`/ban`, `/time`, `/grant`, `/teleport`…) wait for the API
  they need: bans, `time.skip`, roles, `player.place`.
- **Digging**: every TypeScript block digs like stone (`cracky = 3`) until matter decides it, and
  every block gives itself until breaking is tool × material.
- **Canvas is wood** at a third of a block until there's a fibre substance.
- **A server folder of your own** doesn't get the official mods: it lists them, once the
  registry can install them.
- **Strength by substance** also reaches Lua blocks of those substances (planks, substance
  blocks): checked by check:moves and TestMachine, not tuned by hand.
