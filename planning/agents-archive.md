# Terra Firma — Agent Brief

Terra Firma is an open-source survival voxel game: Minecraft-like, but built
around a world that is *simulated* rather than sprinkled with noise, and
survival that can be tuned anywhere from arcade to realistic. The aim is a
rich, complete base game, with a carefully chosen progression from the stone
age to the stars (see "The game"), built as built-ins that can each be
switched off. It is also a
world for AI agents to live in (ArcLabs' Axon labs plug in later).

It is a fork of the Luanti engine (C++, LGPL-2.1+). We keep Luanti's fast
engine — rendering, meshing, networking, map storage — and replace its shell:
all 2D UI is a Vue app drawn by an embedded Chromium, and world generation is
our own model.

Read this whole file before changing anything. It records decisions; if a
change contradicts one, raise it rather than drift.

---

## Repo map

```
apps/game/src/    Luanti fork and our native game systems. Touch upstream lightly; see "Engine policy".
apps/game/src/shell/  OURS — the web UI host: Chromium (CEF), bridge, menus, screens, state stream
apps/game/src/terragen/ OURS — the world model: a pure, fixed-point function of the world code (no engine deps)
scripts/assets/   texture import: Good Vibes at a pinned revision, plus derived rocks and ores
apps/game/src/matter/ OURS — elements and substances, and the state they're in at any temperature and pressure
assets/matter/    OURS — the building blocks as data: elements.json (all 118), substances.json (the modelled ones)
apps/game/src/air/  OURS — a body's atmosphere as light meets it: optics from its card, sky tables (pure)
apps/game/src/far/  OURS — the far view: terrain beyond the loaded blocks to the horizon
apps/game/src/physics/ OURS — rigid bodies on Jolt: assemblies, hinges, ground, tiers, gravity and air as a field (pure; the workshop compiles machines into it)
apps/game/src/orbit/ OURS — flight under a planet's gravity and air; the field in the atlas's grids (orbit::Field)
apps/game/src/online/ OURS — playing with friends: STUN, endpoints, hole punching, UPnP, ticket gate, join
apps/game/src/mapgen/mapgen_terra.* OURS — the `terra` map generator
apps/game/src/builtin/ upstream engine Lua bootstrap, staged as .output/builtin; not optional game mods
apps/game/ui/    OURS — the Vue app (Vite + TypeScript): menus, HUD, in-game screens, gallery
apps/game/game/  OURS — the game, in Lua: blocks, items, recipes, biomes, trees, survival systems
registry/mods/clientmods/ upstream client mods, staged separately as .output/clientmods
registry/shaders/ and registry/textures/  shipped rendering resources
assets/           shipped static files: fonts, matter data, music, misc, translations
library/          vendored dependencies and the first-party UI (pending further organization); library/jolt is Jolt Physics v5.4.0 (MIT)
.output/          ignored build, runtime staging, caches, accounts and local user data
cmake/Modules/CEF.cmake   fetches the pinned CEF build
scripts/dev.ts    the dev loop behind `bun dev`
scripts/terragen-check.sh  generate a world headless and check it against standalone terragen
planning/         sprint plans and their outcomes
registry/games/devtest/, library/irr/, library/lua/  upstream; leave alone unless asked
```

`scripts/stage-runtime.sh` mirrors the app and shipped resources into `.output/`, which
the run-in-place engine uses as its share directory. Saved worlds are user
data; do not treat `registry/worlds/` as a content pack or delete it during
cleanup. That directory still needs a deliberate migration.

In the design notes below, shorter `src/`, `ui/`, and `games/terrafirma/`
references describe paths inside the game product or its staged runtime;
their source locations are now `apps/game/src/`, `apps/game/ui/`, and
`apps/game/game/` respectively.

## Running and checking

```bash
bun dev                 # rebuild engine, start Vite, launch the game against the dev server (hot reload in-game)
bun dev --go            # same, straight into a world
bun dev --profile two   # a second game beside the first, as another player (own sign-in and Chromium cache; shares the UI server)
bun components          # the component gallery in a browser — every UI component, mock engine, no game
bun run build           # engine + production UI (.output/ui/dist, served in-game as terra://ui/)

cmake --build .output/build -j8          # engine only (after cmake -S . -B .output/build -DRUN_IN_PLACE=TRUE)
bun run stage                            # refresh resources copied to the runtime tree
cd apps/game/ui && bun run typecheck      # UI types
./.output/bin/luanti --server --gameid terrafirma --world <tmp dir> --port <n>   # headless boot: catches Lua errors
./.output/bin/luanti --test-module TestTerragen   # terragen unit tests (--run-unittests for the whole engine)
./.output/bin/luanti --test-module TestPhysics    # physics: resting, rolling, hinges, pushing, determinism
bun check:terragen      # world generation end to end: probed world == standalone terragen, + unit tests
bun pose-sheet          # the figure's moves as contact sheets (.output/pose-sheet/*.png), no game needed
bun check:lab           # every Lab scenario built headless; each `expect` checked
bun check:wiki          # every wiki demo (reactions + Lab) run in the engine sandbox; reactions must make their products
bun check:moves         # every wiki Movement and Vehicles scene (game/moves.json, vehicles.json) played through the real code; each must do what it says
bun check:matter        # a clay fire pit lit on a live server: the world's matter burns, flames, records reactions, saves
bun check:far           # the far view against the world: generated chunks vs core.terra.surface, column by column
bun palette             # export the game's baked palette (apps/game/game/palette.json) after changing blocks, textures or species
bun check:palette       # fails if the committed palette is out of date with the game's content
.output/bin/luanti --planetarium TF9.dGVycmE   # the planet viewer alone, on a world code (in game: F7)
arc gate release        # what a release needs, locally: build, tests, UI, a headless world (repo.config.ts)
arc deploy release      # package.json "version" → tag v<version> → CI builds and attaches the Linux package (.github/workflows/release.yml)
bun package:linux <version>  # the portable Linux folder + .tar.zst from the current build (scripts/release)
# tuning by eye: render a code's grid (relief, lakes, rivers) to a PNG and look at it
g++ -std=c++17 -O2 -Iapps/game/src apps/game/src/terragen/*.cpp scripts/terragen-check/render.cpp -o /tmp/render && /tmp/render TF9.dGVycmE out.png [scale] [relief|plates|provinces|rain|season|biomes|fertility|currents|sediment]
```

**How agents verify:** build the engine, typecheck and build the UI, boot the
game headless for Lua errors, run the terragen check after any generation
change, run the Lab check after any mechanic change, and use the gallery for
UI. **Do not launch the
game client or screenshot the user's desktop** — hand it over and the user
runs it and shares screenshots. Say plainly what was and wasn't run.

---

## Engine policy

- Our code lives in our folders. Upstream files get **small hooks** guarded by
  `#if USE_SHELL` (and later `USE_TERRAGEN`), so upstream merges stay cheap.
  Current hooks: `main.cpp` (CEF helper entry, `--shell-url`), `clientlauncher`
  (owns the Shell, swaps the main menu), `game.cpp` (Session lifetime, Esc /
  inventory keys, per-frame update), three draw sites (`game.cpp`,
  `guiEngine.cpp`, `renderingengine.cpp`), `inputhandler.cpp`, `hud.cpp` and
  `render/plain.cpp` (native HUD parts off), `defaultsettings.cpp`, CMake;
  `mapgen.cpp/.h` register the `terra` generator (enum + name + factory);
  `game.cpp` creates and updates the far view (`far_view`); `server.cpp`
  sends `TOCLIENT_HORIZON` on join; `clientmap.h` lists the blocks it
  draws (`forEachDrawn`). Physics (sprint 57): `luaentity_sao` has `setDriven`
  (the workshop's physics moves it) and `sendParts` (`AO_CMD_SET_PARTS`,
  `activeobject.h`); `server.cpp` steps the workshop before the environment
  and tells it map edits; `content_cao` draws a machine's rigid groups as
  one scene node each. The body (sprint 58): `localplayer` hands walking to
  `client/body` (`applyControl`, `move`); `client.cpp` tells it map
  changes; `content_cao` draws visual `"body"` (`client/figure`) and poses a
  carried object in the camera's frame (`applyPosed`). Orbit (sprint 59):
  `server.cpp` owns the field (`m_field`) and gives it to the workshop;
  `player_sao` skips the movement anticheat for `fly`; `localplayer`
  turns the view when the body crosses a seam (`turnWithGround`); `keys`/
  `inputhandler`/`defaultsettings` add `BRAKE` (`keymap_brake`); `gameui`
  shows the orbit on F3. The sky (sprint 60): `game.cpp` makes
  `planetarium::Sky` before the far view and updates it with the date. Online (sprint 24): `gameparams.h` carries
  `online::Launch`; `clientlauncher` turns the menu's choice into hosting or
  joining; `game.cpp` makes `online::Host` around the server socket, joins
  from a STUN-mapped socket, pauses a hosted world only when alone;
  `server.cpp`/`serverpackethandler.cpp` admit by `AUTH_MECHANISM_TICKET`
  through `online::Gate`; `client.cpp` answers it and connects from a given
  socket; `network/mtp` hands datagrams that aren't the game's (STUN answers)
  to `IConnection::setStrayHandler`. Loose ground (sprints 84–85):
  `server.cpp` steps `pile::Piles` with what presses the ground
  (`pressGround`) and sends a settled region's shapes; `client.cpp`
  predicts its own presses, gathers a frame's ground changes into one
  remesh a block (`flushGround`), and remeshes blocks whose detail ring
  the camera crossed (`meshDetail`); `mesh_generator_thread` gives each
  mesh its detail; `content_mapblock` coarsens far ground and meshes
  smoothed ground greedily; `map` keeps a whole cut (`keepShape`).
- Upstream C++ keeps upstream style. Our C++ (`src/shell`, `src/terragen`)
  follows "Code standards" below within C++ idiom: one concern per class,
  pimpl to keep CEF out of engine headers, RAII.
- Old UI code (main menu Lua, formspecs) is deleted only once its replacement
  is proven. The non-shell build is not a supported target going forward.
- Irrlicht here has **no `irr::` namespace** (`video::`, `gui::` are top level).
- CEF headers need C++20; our shell sources are compiled `-std=c++20`, set at
  the target in `src/CMakeLists.txt` (source properties are directory-scoped).

## The shell (src/shell)

One Chromium page lives for the whole session, rendered offscreen and drawn
over every frame. Each file owns one concern:

| File | Owns |
|---|---|
| `shell.*` | the composition root: CEF lifetime, page, `frame()`, `onEvent()`, `on/off/send` |
| `app.*` | process-level CEF hooks; the helper-process entry (`runHelper` first thing in `main`) |
| `client.*` | CEF's view of the browser: paint → Pixels, size, console/load errors to the log |
| `pixels.*` / `surface.*` | dirty-rect unpremultiply (LUT) → Irrlicht texture → drawn last |
| `input.*` | mouse to the page where it painted something opaque (or while a press is held); keyboard only when the page asks |
| `ime.*` | hidden IME element so SDL delivers typed text |
| `bridge.*` | intents in (JSON, registered verbs), messages out |
| `scheme.*` | `terra://ui/` (built UI) and `terra://item/` (icons) |
| `options.*` | the settings the page may read/change, with ranges |
| `menu.*` / `worlds.*` | the main menu loop and world list/create/delete; `worldgen.preview` (terragen fields for the create screen) |
| `session.*` | one running game: owns `state`, `items`, `screens`, `actions` and the in-game verbs |
| `state.*` | per-frame read of the local player; sends sections that changed **by value** |
| `items.*` | item label + icon (blocks rendered as cubes, read back, written as PNG) |
| `screens.*` | page screens backed by an invisible `GUIModalMenu` (real pause/cursor/Esc semantics); table of screens |
| `actions.*` | inventory move/craft/drop → the engine's own inventory actions |
| `account.*` | the ArcLabs account: device-flow sign-in, the saved key, `account` messages |
| `social.*` | friends, presence (heartbeat) and invites from ArcLabs, `social` messages |

Settings: `shell_url` (default `terra://ui/index.html`), `shell_gpu` (Chromium
GPU raster; set false if a driver misbehaves), `game_id` (default `terrafirma`),
`arclabs_url` (the ArcLabs API accounts sign in to; `--arclabs-url` overrides it,
and `bun dev` passes the local stack, `http://localhost:3099`, unless
`ARCLABS_URL` says otherwise). Each API keeps its own saved sign-in.

### The bridge contract

`ui/src/bridge/types.ts` is the source of truth for every intent and message.
**Change both sides together.**

- **Intents** (page → engine) are answered by verbs registered with
  `shell.on(kind, fn)` by whoever owns that screen, and removed with `off` when
  it hands over. An intent nobody answers fails loudly in the page.
  Always on: `ready`, `capture`, `settings.get/set`, `open`, `account.*`, `social.*`.
  Menu: `worlds.*`, `play`, `lab.list`, `lab.enter`, `quit`, `worldgen.preview`. Session: `screen.open`,
  `screen.close`, `game.leave`, `game.quit`, `inventory.move/craft/drop`, `ui.send`.
- A verb throws `Refused("…")` for an expected no (shown to the player);
  anything else is logged as an engine error.
- **Slow verbs are async** (`shell.onAsync`): they run on the bridge's worker
  thread, one at a time, and answer on the main thread — the menu keeps
  drawing. They must be thread-safe (pure). `worldgen.preview` is one.
- **Messages** (engine → page): `mode`, `error`, `items`, `account`, `social`, `state` (sections:
  `hud`, `player`, `hotbar`, `inventory`), `screen`, `ui`. Messages sent before the
  page is ready are queued; `mode` is resent after every reload.
- **The UI channel: game Lua ↔ page.** `player:set_ui(key, value)` sends a
  JSON value to that player's page (`TOCLIENT_UI`; the page keeps the latest
  per key, null clears); the page's `ui.send { name, payload }` reaches
  `core.register_on_ui_event(function(player, name, payload))`
  (`TOSERVER_UI`). Keys and events are typed in `UiValues` / `UiEvents`. A
  screen can require a key (the Lab screen needs `lab`). This is how game
  systems feed the HUD.
- **The server is the authority.** The page never edits game state; it sends
  intents and renders what comes back. No optimistic updates.

## The UI (ui/)

```
ui/src/
  bridge/     contract types, Bridge(), useBridge(), keyboard capture (focus-driven)
  game/       store (all engine state, reactive), HUD, pause screen, inventory/
  menu/       main menu: home, worlds, create/ (steps Start · World · Rules), settings wrapper
  system/     the planetary system on the page: causes only (model.ts), cards from the engine (preview.ts), tree, card, history
  settings/   SettingsPanel — the one settings UI, used by menu and pause
  kit/        the component kit — import from "../kit"
  worldgen/   world config + codes (incl. star/tilt/year), sun.ts (the solar model),
              fields.ts (engine preview fields, overlays, painting), previewer (asks the
              engine), map/globe/star views
  gallery/    `bun components`: pages, mock engine
  theme.css   tokens: colours, fonts, the lighting model
```

- **Build every screen from the kit** (Button, Panel, Slider, Toggle, Field,
  Tabs, Segmented, Choice, IconTile, Section, Notice). New visual patterns become kit
  components, and get a gallery specimen.
- **One light, top-left.** Never hand-write a bevel: use `--raised` /
  `--sunk` (and `--ledge`); layer accents first:
  `box-shadow: inset 0 -4px 0 var(--accent), var(--raised)`.
- Hard edges (`--b: 2px`), no radius. Accent `rgb(0 148 210)` and the `»`
  chevron are the only brand marks. Fonts are bundled (Silkscreen display,
  JetBrains Mono body) — never load from the network.
- Labels never wrap in buttons; panels never scroll sideways; grid children
  get `min-width: 0`.
- The store holds `null` for anything the engine hasn't sent — render nothing
  rather than invent a value.
- Every component must render in the gallery against the mock engine
  (`gallery/mock.ts`). If it can't, its boundary is wrong. Keep the mock's
  intent table complete (it is typed against the contract). The mock answers
  `worldgen.preview` with a JavaScript stand-in (`gallery/mockWorld.ts`) —
  same shape, not terragen's numbers.
- Performance: the page is re-uploaded to the GPU when it repaints, so avoid
  continuous full-screen animation; prefer stepped motion; heavy compute goes
  in a Web Worker.

## The game (games/terrafirma)

Lua mods. `tf_world` has blocks, water, biomes, trees (`trees.lua`), placeholder
recipes and a placeholder starter kit, with Good Vibes textures (see
`assets/CREDITS.md`). `tf_sky` keeps each player's solar sky. `tf_lab` is the
Lab (below).

Biomes: `biomes.lua` registers one Lua biome per terragen class
(`terragen/world.cpp` `BIOME_NAMES`) — terragen decides where, Lua says what
it's made of; the `terra` generator refuses to start if one is missing.

**The Lab** (`tf_lab`, sprints 4–5): a page of scenarios, one mechanic each,
**each in its own tiny world**. The Lab page (`ui/src/lab`) lists every
scenario. Choosing one builds and enters its world (`lab.enter { id }` →
`<user>/lab`, `tf_lab = <id>` in the map settings), and the page stays put:
the game draws through the middle of it, with the scenario's controls
below. Choosing another leaves that world and enters the next. **A scenario
is data plus behaviour:**
- data, in the owning mod's `lab.json`: id, title, description, size,
  optional world and camera, and controls (`button`, `toggle`, `slider`,
  `choice`). The menu reads it before any game Lua runs (`src/shell/lab.*`).
- behaviour, in Lua: `tf_lab.load()` then
  `tf_lab.behaviour(id, { build, control?, enter?, expect? })`. Loading fails
  if the two disagree.

In the world, the camera orbits the scenario while the Lab view is open
(`set_ui "camera"`, run on the client by `shell/director.*`). Step in
closes the view, L reopens it. Restart and time speed
(`core.set_time_speed`) are always there. `bun check:lab` runs every
scenario in its own headless world and checks its `expect`. Systems that
need terragen ask `core.terra.enabled()` first: scenario worlds aren't terra
worlds unless they say so.

World settings are written at creation into the world's `world.mt`:
`creative_mode`, `enable_damage` (copied into the engine's settings when the
menu picks the world, as Luanti's own menu did), and one `tf_*` key per
survival rule; the world code goes into `map_meta.txt` as `mgterra_code`. Systems read their key when they are built. The engine
only accepts those keys (see `worlds.cpp`).

---

## The game

Where we're going. Every content and system decision is checked against this
section; if a change pulls away from it, raise it.

### What we fix in Minecraft

Minecraft's first hours are magic; after that it's a hodge-podge that ends
quickly. The world is scenery (where you settle barely matters). Progression
is one ladder of materials with bigger numbers. Its systems don't feed each
other. Nothing rewards building a town, a stable or a smithy beyond looks.
Distance costs nothing. Mods fill the gaps in pieces, hacking around an engine
they don't own. We own ours, so the pieces can be designed as one game.

What we take from the mod community:
- **TerraFirmaCraft:** the world is the challenge. Geology places ore, seasons
  make storage matter, metallurgy is a real process. Gate on knowledge and the
  world, not grind.
- **GregTech:** a legible tier spine makes a long game coherent. Not its
  tedium: steps must be decisions, never padding.
- **Create:** machines are physical and visible, not humming GUI boxes. Its
  gap is that nothing needs them; ours must be needed.
- **Factorio:** logistics *is* a game once moving things has a cost.
- **Farming, animal and colony mods:** depth from time and care (soil,
  seasons, breeding), and towns that produce and need things.

### Pillars

1. **The world is the game.** Every resource lives where it makes sense:
   ore by geology, crops by climate, animals by biome, power by rivers and
   wind. You can't get everything at home, which creates exploration,
   transport, trade and towns.
2. **Builds exist because they're needed.** A stable because horses sicken in
   winter; a smithy because a forge needs a chimney, an anvil and a quench
   side by side; a granary because food rots. Nothing is built only for looks
   (though everything can look good).
3. **Systems feed each other.** Every output is another system's input. No
   pillar of play stands alone.
4. **Towns are engines.** Settlers (later Axon agents) come for food,
   shelter and work; towns specialise, produce and trade. Later ages need an
   economy, not one player.
5. **Distance is real.** No ender chests. Roads, carts, rails, boats, then
   trains, aircraft and rockets.
6. **Finishing means adventuring.** The last stages need the extremes (poles,
   deep caves, deserts, peaks), then the moon, other planets and other stars.

### Progression: ages

Each age needs the one before it *and* a new place in the world, so progress
pulls you outward.

- **Stone:** knapping, fire, shelter, foraging, the first winter.
- **Bronze:** copper and tin from different places (the first expedition);
  kilns, pottery, storage, early farming.
- **Iron:** bloomery, smithing; animals at work (ploughs, carts, horses).
- **Mechanical:** water wheels on real rivers, windmills where it's windy,
  shafts and gears; the first factories.
- **Industrial:** steam, electricity, rails; logistics becomes the game.
- **Modern:** electronics, automation, chemistry, flight.
- **Space:** rockets, orbits, the moon and planets with resources Earth
  lacks; eventually FTL to other stars, each a new world code.

### Automation is required by kind, not quantity

Tech must be hard without automation but never a grind. Gate tiers on what
hands *can't* do, never on "collect 10,000 of X":
- **rate:** more air, heat or power than a person can supply;
- **continuity:** processes that can't stop (steel, annealing);
- **precision:** tolerances only machines reach; precise machines make more
  precise machines, and that ladder is the tech spine;
- **environment:** clean, sealed, vacuum, extreme heat or cold;
- **scale of energy:** more than one engine gives, hence networks.

Each tier asks for a new capability, not a bigger pile. Recipes stay
learnable without a wiki.

### Mechanics

The physics we build, roughly in the order the ages need them:

1. **Heat**, the keystone: temperatures, conduction, insulation, fuel
   grades, melting and burning points. Feeds survival (hearths, body
   temperature), kilns and metallurgy, and later engines. First.
2. **Mechanical power:** torque and speed from rivers, wind, animals, then
   steam. Early industry has to sit on a river.
3. **Fluids and pressure:** water, steam, oil and molten metal in pipes;
   aqueducts, irrigation, casting; boilers that fail if neglected.
4. **Biology over time:** soil nutrients, rotation, seasons, disease;
   animal genetics, feed, shelter, health.
5. **Logistics:** weight and bulk; carts, roads, rails, boats, cranes.
6. **Structural load:** supports, timbered mines, real bridges. Gentle by
   default.
7. **Atmosphere:** thin air at altitude, bad air in mines (ventilation),
   vacuum in space (sealed habitats, suits).
8. **Electricity:** voltage, current, losses over distance.
9. **Chemistry:** ratios and byproducts (slag, fractions, pollution) that
   force chains.
10. **Orbital mechanics:** Kepler orbits on rails, delta-v, launch windows.
    Space is a simulation, not blocks: ships hand over between planet layers
    and the orbital sim; the sky shows its bodies.
11. **People:** settlers with needs and work.

**The three-question test.** A mechanic earns its place only if it answers
all three: *what does the world provide?* (place matters), *what can't hands
do?* (automation is necessary), *what does it feed?* (one game, not toys).

**Simulate where the effect lives.** Heat, fluids and load are sparse
simulations that run only where something is happening (inside a furnace,
along a pipe network), never a field over the whole world.

**Simulate at the cheapest honest tier** (`planning/simulation-scales.md`).
Everything simulated runs Live (stepped in full) near players, Settled
(compiled into rates measured from the Live model) or Scheduled (asleep
until its next event) further out, or Frozen at rest. Handovers conserve
matter, energy and time; outcomes don't depend on whether anyone watched.
Motion can't compile: far-off bodies are at rest or ride a known path. A deep atmosphere
and weather simulation is a long-term idea, at the coarse grid's scale, not
now.

### Engine primitives (directional)

Ambitious on purpose: this is where we land, built step by step. Each step
ships with the content that needs it.

**Native supplies mechanisms, Lua supplies policy.** The engine models the
physics and assumes no content: it knows resistance, ignition points and
composition, never "copper wire", "coal" or "bronze". Lua builds every piece
of content and every rule. Where a thing goes:
1. per block, per tick, or across many blocks → native;
2. the client must know or predict it → native;
3. a mechanism ("temperature here") is native; a rule ("freezing hurts")
   is Lua;
4. anything every mod would want, framework or not → a native `core.*` API.

**Every primitive has the same four parts:** *properties* on node and item
definitions (any mod declares them, so all blocks take part); *state* the
engine owns, sparse and saved with the map; *Lua verbs* to query, inject
(sources and parameters at runtime) and subscribe to threshold events (Lua
never polls blocks); and native *couplings* between primitives (current
heats wires, burning releases heat, heat raises steam pressure).

**Decision fidelity.** A law is modelled only as deep as it changes what a
player decides or builds. Every law must be predictable without a wiki
(TerraFirmaCraft's depth with none of its opacity), local and sparse, and
budgeted per tick, degrading gracefully.

**Built (sprint 7, `planning/sprint-07-matter.md`):** the substance table,
the first piece of matter. `src/matter` loads `matter/*.json` and gives
each substance's state (solid, liquid, gas, supercritical) at any
temperature and pressure, from its triple and critical points, its 1 atm
boiling or sublimation point and the slope of its melting line. The
main menu's **Matter** screen shows it: a periodic table, structures in
3D, and a phase diagram to drag across, with reference worlds marked.

The catalogue:
1. **Environment fields**, pure and unstored: temperature (climate, season,
   time, altitude, depth), humidity, wind, pressure, sunlight, later rain.
   `core.env.at(pos)`. Trees, crops, windmills and bodies read them. A later
   weather simulation only changes how they're computed.
2. **Heat:** conductivity, capacity, emissivity, thresholds; sparse
   conduction with radiation approximated; hot things glow by black-body
   colour.
3. **States and phases:** substances declare small phase tables (melting
   and boiling over pressure, latent heats). Items and stacks carry
   temperature and phase: a hot ingot cools in your hand.
4. **Rooms:** sealed volumes found by flood fill, each with pressure,
   temperature and gas composition (ideal gas law); openings flow by
   pressure. Fires use oxygen, mines need ventilation, boilers build
   pressure, habitats need airlocks. Per room, never gas per block.
5. **Matter: substances → mixtures → reactions** (see "Matter: one chain
   from the flask to the planet" below). Burning, smelting, alloying,
   curing (concrete strength rising over time), and explosions as fast
   gas-producing reactions spiking room pressure into structural failure.
6. **Networks:** one graph solver with kinds as data: rotation (torque,
   speed), fluid (pressure, flow), electric (voltage, current, I²R heat).
   Content declares conductors and ports; Lua injects parameters
   (`core.network.set`), reads results and handles events (`on_overload`).
7. **Structure:** strength and density; local support checks on place and
   dig; Lua decides what a failure does; strictness per world.
8. **Motion:** rigid bodies via **Jolt Physics** (MIT): block assemblies
   that move and settle, vehicles from physical parts (mass from blocks,
   wheels with suspension, engines with torque curves, hulls as sealed
   rooms so leaks sink ships, wings with lift and drag), gravity as a
   field, drag from air pressure, buoyancy. Needs client authority for
   drivers.
9. **Composite blocks:** any block can be refined into 8³ (maybe 16³)
   micro-voxels, stored only where someone builds them; physics reads the
   micro shape (mass, displacement, drag, lift). Never subdividing the
   whole world.
10. **Shaped terrain:** the generator places slopes, wedges and corners
    where the ground steps, so land is walkable and drivable; players grade
    roads.
11. **Space:** orbits on rails, the sky as its window; later FTL.

### Matter: one chain from the flask to the planet

Everything physical derives from one table and one primitive.

- **Substances** (built, sprint 7): `matter/*.json`, every element's
  identity and the modelled substances with their properties. The engine
  gives a substance's state at any temperature and pressure.
- **Mixtures** (built, sprint 8, `planning/sprint-08-reactions.md`): amounts of substances, plus a temperature and a
  pressure or volume. A mixture's properties come from what's in it, which
  is what makes alloys, composites, solutions, brines and air possible.
  Each step it asks each substance's state, runs the reactions whose
  conditions are met at their rates, and balances energy (reaction heats,
  latent heats). **One primitive at every scale:** a crucible's contents and
  a room's air (seconds to minutes), a lake, an ocean or a planet's
  atmosphere (millions of years). It is the "reservoir" of the modelling
  boundary (`planning/systems-and-bodies.md`).
- **Reactions** (built, sprint 8; the Matter screen's **Reactor** runs them live): `matter/reactions.json`. Each has reactants,
  products, heat released or absorbed, a rate that rises with temperature
  from an activation energy (Arrhenius), and requirements: a state
  (dissolved in liquid water), a catalyst, or an energy input (electricity,
  light). **Element balance is checked when the file loads**, so no reaction
  makes or destroys atoms. Combustion, smelting, electrolysis, rusting,
  calcining, curing, photosynthesis.
- **In the world:** blocks declare what they're made of, and the heat
  primitive plus the engine's states do the rest. Ice melts above its
  melting point at the local pressure, lava cools to basalt, water boils
  cooler on a mountain. Containers and items carry mixtures (copper and tin
  at temperature react into bronze); rooms are gas mixtures (a fire burns
  the room's oxygen).
- **Geology:** the body model is rebuilt on mixtures and reactions over
  geological time. Outgassing chemistry (SO₂ with water makes acid
  clouds), weathering as a reaction (the carbon cycle), and ores from
  chemistry: evaporites (salt left as a sea dries), hydrothermal sulfides,
  placers, and banded iron from life's oxygen rusting the oceans' iron.
  Strata record the history.
- **Life:** metabolism as reactions (photosynthesis, chemosynthesis,
  methanogenesis), emerging where a solvent, an energy source and the
  elements it needs exist. Biomass is a reservoir; the oxygen rise is
  chemistry, not a script. Evolution later (traits, adaptation). Crops and
  animals have chemical and environmental needs, so farming falls out of
  the same rules.
- **The boundary holds:** substances, amounts and rates. No atoms,
  molecular dynamics or fluid simulation. Realistic in outcome, cheap in
  computation.

**Order:** reactions and mixtures (with a menu view to watch them) → heat
and block materials in the world → rooms → the body model on mixtures
(chemistry, weathering, strata, ores) → life as metabolism. The substance
library grows with each step, by what that step needs.

**Geology uses the same ideas at world creation**, deterministic and fixed
point on the coarse grid: ores form by process. Hydrothermal veins near old
intrusions, placers downstream of them in our rivers, coal in buried swamp
basins, oil under sedimentary domes, diamonds in kimberlite pipes from the
deep, salt from dried seas. Mods declare ores by formation process;
terragen supplies the process fields. Prospecting means reading the land.

**Depth:** caves hold the yearly mean temperature; below, a geothermal
gradient (cooling and ventilation, then suits); rock hardens and caverns
need support; a molten deep world far below with exotic minerals, reached
with actively cooled suits; geothermal power late.

**Not doing:** per-block gas or fluid dynamics, finite-element stress,
molecular chemistry, subdividing the whole world.

**Order:** environment fields + properties and states on definitions →
heat and rooms → reactions → geology → structure → networks → composite
blocks and shaped terrain → rigid bodies and vehicles → space.

### Built-ins and content as data

Everyone gets the world; everything above it is a built-in that can be
switched off.

1. **Platform (always on):** engine, terragen and the `terra` generator,
   `core.terra`, the UI channels, and the content registry.
2. **Framework mods:** how content works: materials and tiers, processes,
   crafting, survival needs, heat, power, logistics.
3. **Content packs:** the stuff, an age or a domain at a time (`tf_stone`,
   `tf_bronze`, `tf_farming`, `tf_animals`, ...).

With every `tf_*` gameplay mod off, you're left with a clean world on our
generator to build your own game on. Our packs use only public APIs, so the
API stays honest.

**Adding content is data, not code.** One declaration says what something is,
and the framework derives tools, recipes, tinted textures, icons and world
placement:

```lua
tf.material("bronze", { tier = 2, alloy = { copper = 0.88, tin = 0.12 }, melts = 950, tools = true })
tf.ore("cassiterite", { yields = "tin", rock = { "granite" }, depth = { -60, 20 }, deposit = "vein" })
```

Per-kind behaviour lives in tables, never `if` chains across files. Ores
belong to rock types and rock types come from terragen (geology, phase 5), so
prospecting is a skill.

**Difficulty:** the default is tuned between Minecraft and TerraFirmaCraft
(around Hardy); the rules presets reach either end.

---

## Decided design

### World

- **A planet**, 2k–16k blocks round the equator (radius size / 2π), laid
  on the atlas (`planet::Atlas`). No edges: the poles are places like any
  other (sprint 86), and latitude is real: poles, equator, seasons that
  make sense.
- Generated from a **world code**: `TF2.<seed>.<settings>` — the seed plus only
  the land settings that differ from defaults, versioned. (TF1 was sprint 1's
  generator; it is refused, not reinterpreted.) The code carries the
  **land only**; rules travel separately. A code must never silently change
  meaning: a generator change reproduces old codes or refuses them.
- The seed field accepts plain text or a full code. Share copies the code.

### Generation (terragen)

**Built (sprint 1, `planning/sprint-01-terragen.md`):** phases 0–1. terragen
is a pure fixed-point function of the code (value noise periodic east–west
by lattice wrap, CORDIC trig, the solar model); `mg_name = terra` builds
terrain, climate (yearly mean) and biomes from it; `worlds.create` writes
`mg_name`, `mgterra_code` and the engine seed into `map_meta.txt`; the create
screen's preview comes from terragen via `worldgen.preview`. A unit-test
**golden** pins the output — changing generation for an existing code means a
new code version, not editing the golden.

**Built (sprint 2, `planning/sprint-02-water.md`):** phase 2, water. A 256²
coarse grid per code (`terragen/grid.*`, cached and shared across threads):
sea placed by quantile so land matches the Land setting exactly; priority-
flood filling; implicit stream-power erosion; flow and rain-weighted
accumulation; rivers as the top share of drained land (the Rivers setting
picks 0.4–4%); lakes only for basins of real size. `World::ground(x, z)`
gives each column its terrain and water (sea / lake / river) — carved
channels, banks that hold rivers in, lake floors and rims. The preview sends
lake flags and river segments; the map and globe draw them. Water invariants
(drains to the sea, flows downhill, land share) are unit-tested.

**Built (sprint 6, `planning/sprint-06-bodies.md`):** the **body model**
(`terragen/body.*`): every body in a system is a short history stepped from
formation to the system's age. From causes (star type, age and wind; orbit,
mass, where it formed, volatiles, day, life) come its card (kind, air and
gases, temperature, field, seas, plates, tides, composition, gravity) and
its story. Fixed point (`log2`/`exp2`/`pow` in `fixed.*`), calibrated so our
own solar system comes out as itself (unit-tested). Served by
`system.preview`; the create screen's System tab holds causes only
(`ui/src/system/`). Direction: `planning/systems-and-bodies.md`.

**Online (sprint 23, `planning/sprint-23-online.md`):** players are ArcLabs
accounts. Terra Firma shares only identity with ArcLabs (the device flow,
client `terrafirma`, a key scoped `terrafirma:play`); ArcLabs auth replaces
Luanti's for online play, singleplayer needs no account. Built so far: sign-in
(`src/shell/account.*`) and friends, presence and invites (`src/shell/social.*`,
`ui/src/social`; backend `platform/terrafirma`), and joining
(`planning/sprint-24-join.md`, `src/online`): a signed-in player's world is
hosted for friends (STUN, UPnP, hole punching on the server's socket), Join
gets a single-use ticket from ArcLabs, and the host's server admits by ticket
(protocol 58). No relay yet: pairs that can't be punched through are told so.

**Built (sprint 128, `planning/arc-movement.md`):** one date and the mod menu. The server sends its day count (protocol 96), the body's clock runs on it unbroken, and `core.terra.set_hour` sets a planet's own time. World controls are data (`tf_player/controls.lua`, `tf_controls.page`), with live movement numbers (`physics/tuning.h`) and debug views (settings `view_*`).

**Built (sprint 127, `planning/arc-movement.md`):** falling down and getting up (`physics::Fallen`, `Body::knockDown`). Protocol 95.

**Built (sprint 126, `planning/arc-movement.md`):** balance: bad landings stagger, step-ups no longer jump, a shin caught at a run trips.

**Built (sprint 125, `planning/arc-movement.md`):** kicks by angle of attack, climbs over corners (`physics::overCorner`), side vaults and Kongs as throws about the hands.

**Built (sprint 124, `planning/arc-movement.md`):** wall kicks as a push (`physics::WallPush`); hands kept on the edge and out of the head; shimmy by the body's facing.

**Built (sprint 123, `planning/arc-movement.md`):** climbs by force (`physics::Haul`): caught, pulled, let go, leg up, driven off; how it arrives decides how long that takes.

**Built (sprint 122, `planning/arc-movement.md`):** the mass: hung, the body swings about its hands; on its feet it leans
into what it's doing; legs in the air go by how it's rising and take turns.

**Built (sprint 121, `planning/arc-movement.md`):** moves chosen by velocity (`physics::moveFor`, table `MOVES`), short hops,
and `bun pose-sheet`: every move rendered to a PNG from the game's own pose code (`physics/skeleton`).

**Built (sprint 120, `planning/arc-movement.md`):** contact and flow: hands by IK on edges (`armTo`,
`Grip`, `HandOverHand`), reaching for what's in reach; moves chain, keep their speed, and go by how high the body is by then.

**Built (sprint 119, `planning/arc-movement.md`):** wall runs and wall jumps (`findWall`, `runsAlong`,
`wallKick`), side vaults; vaults hold the speed they came in at. Protocol 94.

**Built (sprint 118, `planning/arc-movement.md`):** reach (`physics/reach`): Space vaults a
low wall on the move; in the air it mantles or hangs by height; A/D shimmy, Space pulls up, Ctrl lets go. Protocol 93.

**Built (sprint 117, `planning/arc-movement.md`):** slides, landing rolls, and momentum: kept
in the air, built by a clean line, lost in sharp turns and hard landings (`physics/walk`). Protocol 92.

**Built (sprint 116, `planning/arc-movement.md`):** stances. Hips face the way the body
goes; crouch and prone with smaller capsules (`physics::Stance`, `World::resize`), protocol 91.
Keys: Shift runs and thrusts, Ctrl crouches, Z lies down, C the camera, N the cursor, F gets up.

**Built (sprint 115, `planning/arc-movement.md`):** the body, first of the movement
arc. The figure is 13 blocks with knees, elbows, a waist and block hands, posed by a pure
rule (`physics/pose`): feet by inverse kinematics, the rest on springs between key poses.
F5 gains over the shoulder (`CAMERA_MODE_SHOULDER`); first person keeps the body, not the head.

**Built (sprints 113–114, `planning/arc-atmosphere.md`):** the rim and eclipses
(`planetarium/rim`, family shadows in `planet_lit_shader`); daylight's colour from the air
(`air::sunTint`, `solar::tint`), and dynamic shadows from the brighter star (`Sky::light`).

**Built (sprints 111–112, `planning/arc-atmosphere.md`):** the air in the game's sky
(`planetarium/air`: a picture per star, made on a worker, looked up per pixel) and
bodies lit by their star (`planetarium/lit`).

**Built (sprint 110, `planning/arc-atmosphere.md`):** air optics (`src/air`): a card's
gases, particles and star as scattering and absorption, in tables after Hillaire 2020.
Nothing draws it yet; `scripts/air-check/sky.cpp` renders a sheet of skies to a PNG.

**Built (sprint 109, `planning/arc-systems.md`):** binary stars (`Companion`, in the
code's params; style Binary's from the seed). The card's light is both stars', the sky
draws both, and the light level follows the brighter one's height (`solar::second`).
Unstable orbits are refused. Shadows and sunset colours aren't built. TF16.

**Built (sprint 108, `planning/arc-systems.md`):** double planets. A moon whose
balance point with its planet is outside the planet is its partner (`partnerOf`);
planets swing round their balance points (`System::balance`), a planet can hold to
its partner, and pairs too close are refused. A body big in the sky is drawn from its
own tiles with a night side (`Sky::drawBaked`). TF15.

**Built (sprint 107, `planning/arc-systems.md`):** spin. Free by default; a lock
or resonance is chosen per body (`Spin`, in the code's body flags) and sets its day.
A world facing its star has its climate's bands round the line to the star
(`Cells(axis)`, `Processes::facing_star`), longitude 0 under it. `System::clock` is a
body's time against the engine's, sent with the sky (`SkySolar::turns`, `at`;
protocol 90). A body's winds follow its day. TF14.

**Built (sprint 106, `planning/arc-systems.md`):** land on every body. A body's
own settings travel with it in the code (`SystemBody::land`, `WorldCode::setLand`);
the code's own settings are the start body's only. A grid is kept by what decides
its land (`WorldCode::keepGrid`), so an edit elsewhere rebuilds nothing. `check:far`
also runs on the Moon. Body seeds use `hashOf`, not `std::hash`. TF13.

**Built (sprint 105, `planning/arc-systems.md`):** the home follows its card.
`WorldCode::decode` fills the start body's `Processes` from its card
(`processesOf`), so every reader of a code plays the same world; a giant is
refused. Climate is pushed by `Processes::warming` (the card's kelvin against
Earth's) with the Temperature setting a nudge; `Star::warmth` is gone. The start
is any solid body (the `playable` flag); its `year()` is its planet's. Other
bodies take their own tilt and warming. TF12.

**Built (sprint 104, `planning/arc-systems.md`):** the system in the code.
The seed and a style lay out a system (`SystemSpec::generate`, fixed point; style
Home is the one fixed system, what a settings-free code means); an edited system
travels as the code's fourth part (`SystemSpec::read`, at most `MOST_BODIES`).
The home's grid is kept by `WorldCode::grid_key`, so moving a moon doesn't
rebuild it. TF11.

**Built (sprint 103, `planning/arc-systems.md`):** stars by spectral class,
O to M (`terragen::STARS`), with mass, metallicity, age and wind in the code.
A proper mass–luminosity fit (`luminosity`), lifetimes (`lifetime`; a system
can't outlive its star), metal from the
star. The home's climate still reads the class's `warmth` until sprint 105. TF10.

**Built (sprint 102, `planning/sprint-102-block-speed.md`):** blocks arrive.
The land's reader keeps its columns and lighting asks them in order; far tiles
leave out detail narrower than their cells (`World::Coarse`); grids are kept on
disk per engine build (`terragen/grid_file.cpp`); `terra` has 4 emerge threads.
Grid stages run on fixed parts (`terragen/parallel.*`, so the same on any core
count) and are reused by what they read. Lua gets a chunk's shore and cover from
native (`get_mapgen_object`: `shore`, `terra_cover`). Trees are gone until
they're rewritten as species with traits.

**Built (sprint 101, `planning/sprint-101-ranges.md`):** ranges by what
built them. Crust keeps the squeezing it went through (`History::Cell::over`,
`under`); `Tectonics` turns it into fold belts, plateaus and oldness. Fold
ridges are contours of `front`, with trellis drainage and water gaps
(`World::folds`, `Channels::build`).

**Built (sprint 100, `planning/sprint-100-channels.md`):** river-built
detail. Below the grid's drainage tree, levels of hashed points each join the
nearest channel a level up (`terragen/channels.*`): real branching (Horton's
ratio tested), valleys and divides in place of noise wherever water ever ran,
and streams in them where it's wet enough. Airless bodies keep their noise.

**Built (sprint 99, `planning/sprint-99-lids.md`):** rock lids by their
interiors (`terragen/lid.*`, from the card). Long plumes under a thick mantle:
a low thin side, one rise, giant shields, canyons. Seas long gone
(`ancient_water`): worn valleys, rust, polar caps. Eruptions: buried plains,
lone thrust blocks, paterae with lava lakes, sulfur, no craters to speak of.

**Built (sprint 98, `planning/sprint-98-ice.md`):** small and icy bodies.
A crater overflow under 0.09 g made spikes tens of km high (fixed); dry bodies
have no waterline; an ice crust (`Processes::stiff`, `flexing`, from the card)
is ice underfoot, with sagging relief, few craters, double ridges and chaos
terrain as hard as tides flex it (`World::flexed`).

**Built (sprint 97, `planning/sprint-97-polish.md`):** polish. Coasts by
their rock (`World::hardness`): cliffs on hard coasts, beaches only as the
strand; point bars inside river bends and cut banks outside; sandy lake
shores; liquids in the world's matter at their body's pull.

**Built (sprint 96, `planning/sprint-96-moon-in-play.md`):** the Moon
in play. Each body's gravity, air and gases come from its card
(`terragen::Surroundings`, `planetOf`): `orbit::Field` takes a planet per
atlas body, and the world's matter runs one air per body
(`WorldMatter::Opts::airs`, `Ground::air`), so fire starves in vacuum.
Mare shores follow the land; wrinkle ridges, rilles, domes and crater rays
on the maria. The game sky draws no orbits.

**Built (sprint 95, `planning/sprint-95-maria.md`):** maria and
lunar materials. A lid that was volcanic floods its basins on the thin
side of its crust (`terragen/maria.*`, `Processes::flooding` and
`young` from when the card's volcanoes fell silent): level pools that
spill, burying the older craters, with few young ones on them. Highland
(breccia over anorthosite) and Mare (basalt) provinces; ilmenite, KREEP
and polar ice; regolith biomes on airless ground.

**Built (sprint 94, `planning/sprint-94-impacts.md`):** impacts.
Craters on every body (`terragen/craters.*`), hashed per size class from
R/4 down to 8 blocks, kept by what the body doesn't wear away
(`Processes::craters`: water, plates, volcanism; air stops small ones);
bowls, complex craters with peaks, ringed basins, ejecta, by gravity.
Wide ones are stamped into the grid; a column finds the rest.

**Built (sprint 93, `planning/sprint-93-scale.md`):** scale. Heights
in metres: √(R / R⊕) blocks a metre (`blocksPerMetre`, `heightScale`;
an Earth-sized world is a block a metre, smaller ones exaggerated), so
the land grows with the planet. Detail is a spectrum from a cell down to
16 blocks (ridged in rising ranges); volcanoes in metres; bodies get the
home's sky.

**Built (sprint 92, `planning/sprint-92-body-settings.md`):** body
settings, the first of the Moon arc (`planning/arc-moon.md`). A body's
processes come from its card, never its name (`WorldCode::processes`:
plates or a stagnant lid, water, life, bare ground): a lid is one plate
that doesn't move; a dry body doesn't rain, wear, or hold lakes and rivers.
`render <code>@<body>` draws any body.

**Built (sprint 91, `planning/sprint-91-polish.md`):** polish. Arcs
rise from volcanic cones carried with the crust (island chains), plate
edges wander, slivers are folded into the plate round them; ice is each
cell's own and cold interiors are colder than their latitude (sea ice
follows coasts); hot deserts from an 18 °C year; glacial scour and tarns
make lakes; placers come only from range cores (`VEIN_CORE`).

**Built (sprint 90, `planning/sprint-90-climate.md`):** an
energy-balance climate on the sphere. Temperature from sunlight absorbed
against heat radiated and carried poleward (zonal bands, solved exactly),
with ice that brightens a cold world further; each cell about its band by
its albedo, the currents and continentality (fierce seasons inland). The
moisture march, winds and currents run on the grid's own cells; the
latitude–longitude climate grid and `heatAt` are gone. A column's heat is
`Grid::heat` + `Grid::heat_gain` × the season's extra sunlight − the lapse
rate. The star and Temperature setting are forcings. The plates' history
is on the body's History timeline. TF9.

**Built (sprint 89, `planning/sprint-89-plates.md`):** plates with a
history. The land comes out of 32 steps of plate motion
(`terragen/history.cpp`, `History`): crust handed cell to cell as each
plate turns, ocean sinking under the other side (arcs, which grow into
land), continents colliding into broad ranges, new floor where plates part
(tearing continents into rift margins), worn by rivers between steps.
Height comes from the crust (ocean floor deepens with age); the Land share
is still exact. `Tectonics` is the plates today, with each cell's `formed`
and `age`; provinces read them. River profiles in rising ranges are near
steady state (`concavity.cpp`). TF8.

**Built (sprint 88, `planning/sprint-88-erosion.md`):** land worn by
water. Each erosion round the belts are pushed up, rivers cut (implicit
stream power) and carry what they wore down the receivers, laying it down
where they slow (floodplains, fans, filled lakes, a shelf at the coast;
`Grid::sediment`, balanced exactly, `testSediment`), and hillslopes creep.
Heavy grains from the belts' cores settle first: placers below the
mountains (`Grid::placer`, `Strata::placer`). Sediment makes silty soil and
fertile ground. Streams (four times the rivers' share, one to five blocks
wide on any planet) and rivers meander by their own width; lakes and rivers
share one warp, so outlines aren't the cells' triangles; coasts wander and
the fine detail fades at the waterline. River profiles weren't yet those of
uplift-built land (`scripts/terragen-check/concavity.cpp`): sprint 89.

**Built (sprint 87, `planning/sprint-87-precision.md`):** places to the
block. A column is asked by a `terragen::Place` (30 fractional bits) on
every planet up to Earth's; the radius and cell spacing are 64-bit. TF7.

**Built (sprint 86, `planning/sprint-86-sphere.md`):** the world on
the sphere. terragen is a function of a place on the planet (a fixed-point
direction, `terragen::Dir`), never of block (x, z): its grid is an
icosphere (`terragen/sphere.*`, 64 002 cells, built in integers), noise is
3D at the place, plates turn about their own poles, and block noise is read
at the block's point in the planet (`World::solid`). The atlas hands each
cell its direction (`planet::Atlas::place`); a terra world is always a
planet. No polar wall, no seam at ±180°. `core.terra.place(pos)`. TF6,
protocol 89.

**Built (sprint 85, `planning/sprint-85-ground-detail.md`):** ground
detail and its costs. Cut and smoothed ground is meshed coarser far off
(`ground_detail_near` / `_far`: sixteenths, quarters, whole-node steps;
`MeshMakeData::m_detail`) and smoothed ground greedily (a flat node one
quad, not 16 strips). The physics' ground weighs each node once and
caches hulls by shape. A dug or placed node freezes the loose ground round
it as it stood (`Map::keepShape`: a whole cut kept, never smoothed) and
lets it slide (`core.matter.ground_changed`, `tf_world/ground.lua`):
sand slumps in, soil holds its wall. Protocol 88.

**Built (sprint 84, `planning/sprint-84-loose-ground.md`):** loose
ground moves (`src/pile`). Wheels and feet press it into ruts with
ridges, and what stands steeper than it rests at slides down, in
sixteenth columns by integers (`pile::Field`), run by the server round
what presses (`pile::Piles`, `Server::pressGround`) and written as cut
shapes; the client predicts its own presses and is corrected. Materials
are node groups (`pile_repose`, `pile_yield`, `pile_bearing`,
`pile_give`).

**Built (sprint 83, `planning/sprint-83-planetarium.md`):** the
planetarium, sharpened. Overlays are painted natively per tile vertex
from the world model (`planetarium/paint.*`), as sharp as the land and on
every body; the other bodies are drawn from their own tiles
(`Planetarium::m_baked`); the star is a body to fly to and go round
(`planetarium.fly { body: "star" }`); orbits are ribbons
(`planetarium/ribbon.*`); the game sky uses the game camera's FOV.

**Built (sprint 82, `planning/sprint-82-fine-water.md`):** fine water.
Near the camera the client runs each wet node as 16×16 columns of shallow
water (waves, sloshing, wakes from what moves), held to the server's
amount per node, drawn as voxels on the sixteenth (`client/liquids.*`).
Poured liquid falls as sixteenth-cube drops that splash in
(`core.matter.stream`, `TOCLIENT_STREAM`, protocol 87). The sea is drawn
by the layer (group `liquid_layer`), meeting the shore; old shores are
filled on load.

**Built (sprint 81, `planning/sprint-81-liquid-layer.md`):** the liquid
layer. Liquid lies in a node's open space as a fill (`src/fill`, map
format 31, `TOCLIENT_FILL`, protocol 86), beside the node: water in a pit
cut in the sand, the sand staying sand. The world's matter runs it against
the open sixteenths (`Map::solidShape`: cut or smoothed ground); the
client draws it on its own (`client/liquids.*`), gliding, never remeshing
map blocks. Generation fills the shore; `core.matter.set_fill/get_fill`.

**Built (sprint 80, `planning/sprint-80-flow.md`):** flow. Liquids fall
and run by virtual pipes, powders heap at their angle of repose, open
surfaces evaporate (a torch over spirit lights it), all in the world's
matter (`sandbox/pouring.cpp`); levels drawn in sixty-fourths on levelled
nodes. Still pools sleep keeping their own mixture (`Held`); past the
budget the busiest blocks step first. The world's water is the matter's,
not Luanti's. `core.matter.pour`, `core.matter.disturb`; the pour key (X,
protocol 85). Matter saves are version 4.

**Built (sprint 79, `planning/sprint-79-matter.md`):** matter
foundations. Formation enthalpies (NASA) and saturation curves (CoolProp)
baked offline into `assets/matter/thermo.json` (`scripts/matter-data`);
reaction heats computed from them. 76 substances, 46 reactions (acids,
alkali metals, thermite, gunpowder, fuels). Solubility and colour as data.
Still mixtures aren't run; 10 000 awake blocks in 22 ms a simulated
second. `core.matter.add`, a crucible (vessel), scoop, blowtorch; "set
upright"; wheels spin the right way between poses. Protocol 84.

**Built (sprint 78, `planning/sprint-78-drive.md`):** drive, first
pass. An engine is a power and a top rpm (`_machine.power`, kW; `rpm`),
geared by Jolt with shifts at its revs; fittings weigh their `_machine.mass`,
not a block of matter. Carried parts aim at a part's whole box and line up
to the sixteenth (flush or centred when near). Protocol 84.

**Built (sprint 77, `planning/sprint-77-assemble.md`):** assembly. A
copy's plug keeps the face it hung by (`Machine::plugFace`), so it goes
back on flush and square with no turning by hand; hold left on a
workpiece part (tools away) to pull it off with what hangs from it
(`Machine::pull`, `Workshop::pull`), carried while held, let go on a face
to put it on; the ground round a benched workpiece isn't dug; parts sit in
the creative catalogue. Fixed a crash carrying what had just been grabbed.
Protocol 83.

**Built (sprint 76, `planning/sprint-76-parts.md`):** parts you can
see and carry. A sprung joint's hub is its own body, riding its wheel's
suspension (`physics::Hub`); a copy keeps the joint it hung by
(`Machine::Plug`); starter parts (`workshop/starters.*`: a wheel, sprung
corners that steer and are driven) in the inventory's Parts tab. Springs
rest where they're built (preloaded). Protocol 82.

**Built (sprint 75, `planning/sprint-75-suspension.md`):** suspension
and structure. The sprung joint: a wheel turning on its hub rides its
spring (travel and firmness picked on the joint's menu; ride height is
where it's built; TFM5, protocol 81). Frame, Truss and Beam shapes (a
chassis in one drag, its beams the size thick); Round on the rail;
right-click stopping a stroke no longer opens the menu.

**Built (sprint 74, `planning/sprint-74-parts-library.md`):** copy,
paste, keep. Ctrl+C copies the part pointed at and what hangs from it
(`Machine::extract`), Ctrl+V carries another till the next click, Ctrl+S
saves it to the parts library, which is every world's now
(`<user>/parts`; TOSERVER_CLIPBOARD, protocol 80). Building materials
(`tf_world/materials.lua`: timber, aluminium, steel, rubber, canvas,
glass); a wheel grips by its tread's material (`treadGrip`).

**Built (sprint 73, `planning/sprint-73-vehicles.md`):** vehicles built,
not guessed. Wheel joints that steer, are driven, or both (`piece::Joint`
kinds, picked on the joint's menu); only those steer or take the engines'
torque. Seats and engines are fittings: a block with a role is added whole,
facing the way its fitter looks (facedir), and the seat's front is ahead
(`Assembly::facing`; `Drive::ahead` gone, protocol 79). The driver sits
lower and the view turns with the machine.

**Built (sprint 72, `planning/sprint-72-joints.md`):** F and Tab are
separate (F only benches; Esc leaves the workpiece be); delete asks twice
and isn't offered on a lone part; the workpiece's joints and sockets are
marked and picked by looking (right-click: their kind, or remove). The
client rebuilds a block's ground with its neighbours', compiles its copy of
a machine from the object's current text (a wheel just made was missing, so
it sank while dragged), and logs where its grabbed copy parts from the
server's (`World::describe`). A stale pointed object crashed the punch.

**Built (sprint 71, `planning/sprint-71-feel.md`):** feel. A grip sits
on the voxel clicked and on the body it's on (a wheel's turns with it;
pulled along the rim it spins the wheel, the rest pushes the car at the hub).
Wheel groups glide by their middles on the client (they cut through the
ground). Right-click any object for its menu in the world, drawn as the
page's buttons (`Hud::Tile`): sit, work on, carry; on the workpiece its
joints (hovered: shown where they'd be) and its part's edit, detach, delete.
The joint tool and the page's part menu are gone.

**Built (sprint 70, `planning/sprint-70-build.md`):** one build mode.
Tab in and out (X and C as ways in are gone; C frees the cursor); Add makes
objects, and Alt held adds into the world. The rail is Add, Cut, Paint and
Joint (R steps through them); the shapes are on a bar on the hotbar, picked
with 1–6. Select is gone. A workpiece has a front
(`Machine::front`, TFM4, protocol 78), and mirroring follows it.

**Built (sprint 69, `planning/sprint-69-drive.md`):** drive. Seat and
motor blocks (a material's `role`, `torque`); right-click a machine with
a seat to drive it (W/S, A/D steer the front axle, Space brakes, Shift
gets up; `TOSERVER_DRIVE`, protocol 77), the car predicted on the
driver's client. Fixed: what's in someone's hands kept its ground (it
fell through after two seconds); machines are pointed at by their
voxels; the grab's hand stops at the ground, and its grip is drawn.

**Built (sprint 68, `planning/sprint-68-grab.md`):** grab. Left-click
held on an object pulls the point pressed toward the hand as hard as a
person (weight decides: lifted, tipped, or barely moved; many can grab
one), predicted in the client's physics and corrected toward the
server's (`World::grab`, `World::correct`, `TOSERVER_OBJECT_GRAB`,
protocol 76). Alt-click carries whole. Alt is the build layer: Alt+1–6
shapes, Alt+= steps the mirror plane, which is now kept once chosen.

**Built (sprint 67, `planning/sprint-67-solid.md`):** solid. Wheels have
backstops in the root's shape (a tipped car rests on them, a wheel built
into the ground lifts out); machine bodies are swept; standing on an object
isn't a push; pushes land at the centre of mass's height; a placed player
moves through a server step's physics steps a share at a time.

**Built (sprint 66, `planning/sprint-66-rolling.md`):** rolling. Round
parts on level free axles off a machine's root are vehicle wheels (Jolt's
vehicle constraint: cast tyres, suspension, axle friction), their bodies
following; sliced slopes are their hulls to what moves; pushes land where
they're given and don't lift (creative pushes anything); the part label
gives the machine's weight; wheels glide on screen with the body.

**Built (sprint 65, `planning/sprint-65-parts.md`):** parts. Round
shapes are drawn from their middle; drawing on a socket grows a part
joined there (a wheel in one stroke); carried parts meet at their shape's
middle. The rail is Add, Cut, Paint, Select (Shift pulls). A workpiece's
parts are clicked in the world: select, Shift-click for the joint between
two, right-click for the part's menu (sockets, edit, detach, delete);
the page's part card is gone. Protocol 75.

**Built (sprint 64, `planning/sprint-64-fit.md`):** fitting. What's
carried is drawn where it'd fit (a socket or a face); Q/E spin it about
the joint, F flips its side, R the next axis (join and plug carry the spin,
protocol 74). Shift-click a socket for another of the last thing plugged
in (creative). Parts light up as they're looked at. Inventory is I, drop
Backspace.

**Built (sprint 63, `planning/sprint-63-joints.md`):** joints: fixed,
rotary (free, an axle) and hinge (between stops); a part carried onto a
turning socket meets it with its thinnest side (a wheel stands up); R turns
what's carried; a face's joint menu is drawn in the world on the face
(right-click on the workpiece). Protocol 73.

**Built (sprint 62, `planning/sprint-62-create.md`):** build mode's two
ways in: X shapes the world, C creates objects (the first shape drawn is
the workpiece; F on another object switches to it). N is gone. Cut nodes
let the light above them through when meshed.

**Built (sprint 61, `planning/sprint-61-workbench.md`):** the workbench,
played. Pushing works again: the body sends what it presses on objects
(`TOSERVER_OBJECT_PUSH`, protocol 72) and the server pushes them. Making
an object no longer crashes. Parts can be drawn on the ground while a
workpiece is on the bench. The drawing cursor and the footprint step are
fixed.

**Built (sprint 60, `planning/sprint-60-sky.md`):** the sky is space.
On a planet the stock sky, clouds and drawn weather are off; the
Planetarium's system is drawn behind the far view from where the player
is (`planetarium::Sky`), and the full-screen Planetarium draws the
player's orbit (`orbit::Flight::path`). The air comes back later.

**Built (sprint 59, `planning/sprint-59-orbit.md`):** gravity as a field.
`orbit::Field` gives the planet's pull, spin and air at any place in the
map's grid axes, and carries a velocity from place to place (and across
seams) as the same motion, so things fall around the planet. Machines
and the player weigh and drag by it; creative fliers thrust (Ctrl, B
brakes) to orbit; new worlds' sky reaches a radius up.

**Built (sprint 58, `planning/sprint-58-body.md`):** the body. The player
is a Jolt character with inertia (`physics/walk`), run on the client's own
physics world (`client/body`) and placed on the server's; riding what
moves, pushing as hard as a person; carrying is a spring against weight;
a jointed figure posed by movement. Luanti's movement only flies, swims
and climbs.

**Built (sprint 57, `planning/sprint-57-rolling.md`):** physics. Machines
are rigid bodies on Jolt Physics (`src/physics`, double precision,
deterministic): each rigid group of parts a body weighed by its blocks'
materials (`core.physics.material`, from `core.matter.density`), hinges as
hinges, round hinged parts rolling as cylinders, players pushing, Live and
Frozen tiers. The workshop compiles and drives them (`workshop/motion`).

**Built (sprint 21, `planning/sprint-21-flight.md`):** flight. One
planet card (`terragen/planet.h`: a tenth of Earth's size, ten times its
spin, Earth's pull and air) read by climate, weather and flight; the world's
blocks are a map of it. `src/orbit` flies
anything under gravity, air and thrust (`core.orbit`). Space is a place:
above the 30 km boundary everything rides in bubbles, cells of the map past
the polar wall (`World::spaceFrom`) each falling on an orbit, where you
float, build stations and meet other bubbles (`tf_flight/space.lua`); the
creative flight pack flew over the blocks and in space. The wiki's
Flight section is Newton's cannon (`orbit.coast`). The pack and
`check:flight` went with the cube net (2026-10-28, sprint 31); flight
returns on the system's orbits.

**Built (sprint 19, `planning/sprint-19-families.md`):** material
families. A block is a family plus a composition in its `param2`, coloured
by a palette. Soil is a point on the texture triangle with organic matter
(198 points; terragen's `soilPoint`/`soilIndex`, chosen per column by parent
rock, weathering, floodplains, slope and cold), named by USDA texture class.
Rocks are 16-step blends toward a geological neighbour. The world's matter
reads families through `core.matter.register_family`. Heat-made liquids pour
and spill.

**Built (sprint 18, `planning/sprint-18-creative.md`):** the inventory as
Minecraft's (`ui/src/game/inventory`: `carry.ts` holds every click and drag
rule; the creative and survival panels; a fixed frame with icon tabs and an
always-visible search; E or Esc closes it), Minecraft's keys (Ctrl or
double-tap W sprints, double-tap jump flies, F3 debug, F5 camera), a block
of every substance (`tf_matter/substances.lua`), a heat gun, and a probe
(a HUD card).

**Built (sprint 20, `planning/sprint-20-horizon.md`):** the horizon. The
far view is the world itself at a lower resolution, as blocks. The
generator decides every block with one rule, `terra::Columns::node`
(`src/mapgen/terra_column.*`); tree species are engine data
(`core.terra.register_species`, chosen by `core.terra.species`;
`src/mapgen/terra_species.*`). The server sends both to the client
(`TOCLIENT_HORIZON`, protocol 56), which builds tiles of block columns
with the same code (`src/far`): trees regrown with treegen as the world
grows them, meshed as block faces in each block's texture colours and lit
as the near world is. It's a quadtree of tiles, finer near the near
world's edge, and it draws exactly where the near world's map blocks
don't (`ClientMap::forEachDrawn`). The curve is drawn, not stored
(R = 8 × world size), with haze from the seam out. `core.terra.surface(pos)`
gives a column as the generator makes it, without generating it.
`bun check:far` compares the two. Settings ▸ Horizon.

**Built (sprint 17, `planning/sprint-17-world-as-sandbox.md`):** the world
is the sandbox. `src/sandbox/physics.*` holds the rules between blocks, and
both the wiki's `Sandbox` and the world's `WorldMatter` use them. Blocks
declare `_matter` (a substance or rock, its share of the volume, its state,
whether it's that matter's form); `tf_matter` registers them and runs
`core.matter`. Awake blocks exist only around activity, and asleep air is
the room. Wood breaks down into charcoal and fuel gas that burns as the
flame, clay walls insulate, lava cools to basalt, ice melts. There's a fire
drill (lights what's light enough: thermal mass) and a bellows; hot items
stay hot; discovery greys unseen reactions in the in-game wiki.
`bun check:matter` lights a real fire on a live server.

**Built (sprint 16, `planning/sprint-16-wiki.md`):** the wiki
(`ui/src/wiki`, key G in game where the world's rules allow it): Matter,
Reactions, Reactor and Lab, each with a list, a live view and the facts.
Its demos run in the **sandbox engine** (`src/sandbox`): a grid of
`matter::Mixture` blocks with conduction, gases that mix, rise and are drawn
in, falling liquids, and heat carried with matter. That's the same physics
as the game, and it's meant to become the world's block physics. Mixture
phase changes are now gradual: matter holds at its transition temperature
while it changes. Every reaction's demo is generated from its data;
`bun check:wiki` runs them all in the engine. `tf_player` grants fly, fast
and noclip in creative worlds and adds sprint.

**Built (sprint 15, `planning/sprint-15-weather-on-the-world.md`):** the
weather acts on the world. `tf_weather/world.lua` lays snow where it falls
and melts it, and freezes and thaws open water, all by one rule over
`core.terra.weather` (`tf_weather.weather_on`), run by ABMs around players.

**Built (sprint 14, `planning/sprint-14-ocean.md`):** ocean currents in the
climate. Wind-driven gyres warm the sea off east coasts and chill it off
west coasts (reversed in the subpolar band). The heat reaches the coasts
and cold water dries the air above it, which makes coastal deserts. Shown in
the Ocean currents overlay. TF5.

**Built (sprint 13, `planning/sprint-13-climate.md`):** the climate makes
the land. terragen solves the long-run climate (`terragen/climate.*`: the
same circulation as the weather, two seasons, moisture marched along the
mean winds, rain where air rises), and rain drives erosion, rivers, lakes
and biomes. Biomes are a niche table of heat × wetness × seasonality,
including monsoon (summer rain) and mediterranean (winter rain). Fertility
comes from rock supply, river silt and leaching. The live weather's rain is
anchored to this climate. TF4.

**Built (sprint 12, `planning/sprint-12-weather.md`):** weather as a
general **fluid layer on a spinning sphere** (`src/fluid`): the body's
numbers in (size, gravity, spin, tilt, sunlight, the air's gases from the
matter library, whichever substance condenses there), circulation by rule
(Held–Hou Hadley reach, Rhines jets: the bands follow the spin), heat,
vapour and ash moved on a lon–lat grid, rain wherever air rises and cools,
storms as a list, events as threshold data. Earth's instance
(`src/weather`) is built from terragen's ground and anchored to its climate
(a sample's temperature is terragen's for that column plus the layer's
departure); live state saved as `<world>/weather.bin`, replayed exactly
from the last save. `core.terra.weather(pos)`; `tf_weather` shows clouds,
rain, snow and ash; the HUD strip; the create screen's Weather overlay
(`weather.preview`, a settled year).

**Built (sprint 11, `planning/sprint-11-alloys.md`):** early craft
chemistry as data. Alloys are substances made by reactions that need their
metals molten (bronze, pewter, cast iron, steel by blowing air through cast
iron); wood burns in air or chars to charcoal short of it; sand and soda ash
make glass; lime slakes and sets back to limestone as mortar. Metals carry a
Mohs `hardness`. Reactor presets for each.

**Built (sprint 10, `planning/sprint-10-vegetation.md`):** vegetation.
terragen decides how much can grow on each column and where trees stand
(`World::cover`); `core.terra.cover(minp, maxp)` serves a chunk; species are
content in `tf_world/vegetation.lua` (niche → shape), so mods add species
without engine changes.

**Built (sprint 9, `planning/sprint-09-geology.md`):** geology. Plates on
the grid (boundaries classified per plate pair: converging, spreading,
sliding); mountains follow their collisions (`belt`), with rifts, ridges and
trenches (`relief`); volcanoes on arcs, rifts and hot-spot chains, active ones
with level lava craters; provinces (shield, orogen, arc, rift, basin,
volcanic, ocean floor); folded strata; ores by process; caves by rock (karst,
lava tubes, rare caverns). Rock and ore per block come from terragen
(`World::strata/rock/ore/cave`, 3D fixed noise `value3`); the game supplies
blocks by alias (`mapgen_rock_*`, `mapgen_ore_*`, `mapgen_placer`,
`mapgen_lava_source`). Codes are **TF3**. `core.terra.geology(pos)`;
plates and geology overlays; body interiors (`BodyCard.layers`).

A deterministic **coarse simulation** of the whole finite world, computed at
creation (seconds) and cached in the world folder, then sampled per chunk with
fine detail. One implementation serves the engine *and* the menu preview
(`worldgen.preview` replaces the JS mock).

- **Determinism is a hard requirement:** integer / fixed-point noise for
  anything that decides terrain; floats only for visual-only detail. Same code
  → same world on every machine.
- **Tileable** east–west from the start.
- Takes a **world kind** input (earthlike now; moon and planets later).
- Phases: 0 foundation (module, `mg_name = terra`, coarse grid, engine
  preview) → 1 shape (continents, mountains, coast) → 2 water (depression
  fill, flow accumulation, rivers, stream-power erosion) → 3 climate (from the
  sun model: latitude, altitude, prevailing wind, rain shadows) → 4 vegetation
  (moisture, temperature, slope, altitude → species and density; treelines;
  riparian forest; L-system trees placed by terragen) → 5 geology (strata;
  ores, oil, coal, placer gold from rock history — prospecting is learnable)
  → caves.
- Libraries are fine where they don't threaten determinism.

### The sun

Axial tilt, year length, day length, sun strength (star type), and optionally
orbit shape are **world settings**. One small shared solar model gives, for any
latitude and date, sun elevation, day length and insolation. It drives
terragen's climate, in-game seasons (temperature, snow, leaves, crops, day
length, polar day/night) and the sky. The engine's sky (`sky.cpp`, day/night
ratio) is changed to take its sun path from the model; the server sends the
date. The create menu shows the planet lit by the real sun for a chosen date.

**Built (sprint 3):** `set_sky{ solar = { latitude, declination } }` (a
trailing field of `TOCLIENT_SET_SKY`). The client places the sun, moon and
stars from the real time of day, latitude and declination, and feeds
everything that colours and lights the world (sky colours, glow, light ratio,
shadows) the **apparent time**: the stock time at which the stock sun stands
as high as the real one (`client/solar.*`). `tf_sky` keeps each player's
solar sky current. The engine's clock (time of day and its speed) is
unchanged; time is the same at every longitude.

### Survival

Rules are data, from arcade to realistic (`menu/create/rules.ts`, presets
Arcade / Classic / Hardy / Realistic): hunger, healing, illness, thirst, body
temperature, stamina, difficulty, keep-inventory, day length, weather,
seasons. Creative and survival are both first-class. **Body temperature is the
first system** — it exercises sun → climate → rule → Lua system → state stream
→ HUD. Game Lua will feed the HUD via `player:set_ui(key, value)` (planned).

### Later

- **Planets:** a planet is another finite world (its own code, kind, rules),
  living as a separate high layer of the same map. The moon first, once Earth
  is good. Orbits are numbers driving phases, tides, eclipses and the sky.
  Other planets hold resources Earth doesn't; other stars (FTL) are new world
  codes.
- **Weather:** a coarse atmosphere on the terragen grid (heat, pressure,
  moisture, wind, rain shadows), with extreme events (storms, blizzards,
  droughts, floods through the river network) spawned where conditions allow.
  Deferred.
- **Multiplayer** is mostly free (Luanti is always client+server). Anything we
  add to the protocol means our client and server only — bump the protocol so
  mismatches are refused cleanly.

---

## Code standards (our code)

- **Fail loudly.** No swallowed errors, no `?? fallback` hiding a value that
  must exist, no silent defaults. Invalid state throws. Expected refusals are
  structured (`Refused`, `WorldCodeError`) and shown to the player.
- **Validate at boundaries, trust inside.** The bridge, file I/O, world
  settings and inventory slots are checked where they enter.
- **No `as` casts** to silence a type mismatch — fix the shape or add the
  honest primitive (typed switches over discriminated unions).
- **Data over branches.** Variants live in tables that every decision reads
  (screens, options, presets, tabs, world settings whitelist).
- **One concern per module**; factories/composables in TS, classes in C++.
  Orchestrators wire; leaves work.
- **Comments say why**, at the density of the surrounding code.
- **Unwired things say so** — in code (throw "not wired yet") and in the UI
  (Notice: "preview only", "saved for later"). Never pretend.

## Licensing

- The engine is LGPL: our engine changes stay public. The game's own code and
  art can be whatever we choose.
- **Never build on Mineclonia/VoxeLibre** (GPL) for the product.
- Vendored code keeps its licence file beside it (Jolt Physics: MIT, `library/jolt/LICENSE`).
- **Assets** (images, music, sounds) need a redistributable licence (CC0, CC-BY,
  OFL…) and an entry in `assets/CREDITS.md` before they're committed. Nothing
  resembling Minecraft characters or branding.
- Fonts are OFL (bundled via @fontsource).

## Status

Done: CEF shell, bridge, main menu (home, worlds, create in three steps —
Start (enough to play), World (body tree · viewer · inspector for the
selected body, Earth's land as its Surface), Rules — relief globe and map, world codes, name generator, settings),
HUD (hotbar, hearts, breath, crosshair), item icons, pause screen, inventory
(crafting, move/drop, shift-move), component gallery, trees, and **terragen
phases 0–2**: worlds are generated from their code — land, climate, rivers,
lakes, carved valleys — and the preview is the world.

The create viewer has the system list (Sun ▸ Earth ▸ Moon), overlays
(terrain, biomes, elevation, temperature, moisture), a date scrubber and
star / tilt / year settings, all drawn from terragen (the mock now serves
only the gallery).

**The sun on the live world (sprint 3, `planning/sprint-03-seasons.md`):**
a calendar from the engine's clock, the sky following the real sun per
player, and `core.terra` for game Lua.

**The Lab (sprint 4, `planning/sprint-04-lab.md`):** scenarios, the panel,
the UI channel, `bun check:lab`. Its first run caught water that had never
been a liquid, and lakes with stepped surfaces.

Next: the game. First `planning/game-design.md`, which turns "The game" into
the ages, their unlocks and the first content tables. Then the framework
skeleton (content registry, materials and tiers) and a stone-age vertical
slice: knapping, fire and **heat**, shelter, foraging, and body temperature on
`core.terra.climate` (needs the `player:set_ui` HUD channel). A first winter
you can lose. Geology (strata, ores) follows, because bronze needs it.
Known items: very large basins become high sediment plains (fine for now);
small streams below grid resolution don't exist yet.

Queued UI work: make the shared Settings screen production-grade. Expose the
relevant engine settings in clear pages (video, graphics, audio, controls,
accessibility, and advanced as needed), with graphics presets, keybinding
edit/reset/conflict handling, and any missing kit components represented in
the gallery. Keep the page and validated engine settings bridge in sync.

Default art: `tf_world` uses selected Good Vibes textures (Acaitart, CC BY 4.0)
with source revision and attribution in `assets/CREDITS.md`. The user still
needs to review the look in-game and share screenshots for any colour or water
tuning. Add new blocks in the same visual style and record new asset sources.
## Game Philosophy

**Terra Firma is an open-world sandbox about discovering, understanding, and mastering a living, physically coherent universe.**

The goal is not to recreate Minecraft with more realism, nor to build a scientific simulator for its own sake. It is to create a rich, accessible, and consistently enjoyable world where meaningful gameplay emerges from the interaction of understandable systems rather than arbitrary recipes, progression gates, and scripted objectives.

### Core Principles

**1. Emergence over prescription**

Build a relatively small set of powerful, interacting primitives from which complex behaviour can emerge. Chemistry, materials, geology, climate, ecology, and physics should provide consistent foundations for gameplay. Prefer general rules over special-case mechanics.

**2. Realism in service of gameplay**

Simulate enough reality to create interesting possibilities, constraints, and decisions—not unnecessary chores. Approximation is encouraged where it preserves meaningful relationships. Greater simulation complexity must justify itself through gameplay value.

**3. Knowledge is progression**

Players should be rewarded for curiosity, experimentation, and understanding the world. Real-world intuition should generally produce sensible results. Rather than memorizing arbitrary recipes, players discover properties, processes, and techniques through observation and experimentation. Failures should be understandable and informative.

**4. Problems create purpose**

The environment should naturally create reasons to build. Climate motivates shelter, distance motivates transport, scarcity motivates exploration, and resource processing motivates industry. Roads, farms, workshops, machines, and settlements should become useful solutions rather than optional decoration.

**5. Mastery eliminates repetition**

Early tasks may require manual effort, but players should progressively develop tools, machinery, infrastructure, and automation that solve recurring problems. Progression should expand capabilities and ambitions rather than simply introduce more grinding.

**6. Freedom through construction**

Players should be able to invent solutions, not merely assemble predefined objects. Explore variable-resolution construction, material shaping, reusable components, mechanical assemblies, and blueprints. Balance approachable LEGO-like building with optional engineering depth.

**7. A living, persistent universe**

Worlds should evolve independently of the player. Planetary conditions shape weather, oceans, terrain, resources, and ecosystems. Ecologies may grow, compete, adapt, and recover. Simulations should operate at appropriate spatial and temporal scales without requiring every organism or process to be tracked individually.

**8. Progression without a prescribed path**

From primitive survival to industry, civilization, and interplanetary exploration, progression should emerge through capabilities and dependencies rather than rigid technology trees. Support multiple solutions, alternative starting ages, and different play styles, from Arcade to Realistic.

**9. Scientific foundations, playful possibilities**

The universe should behave coherently without being constrained to a perfect recreation of reality. Encourage wonder, discovery, experimentation, unusual creatures, unexpected interactions, and ambitious engineering. Grounded systems should enable creativity, not suppress it.

**10. Open, moddable, and extensible**

Terra Firma should provide a strong, independently playable baseline rather than attempt to contain every possible game. Simulation primitives belong in the engine; gameplay and mods consume and interact with them through stable interfaces. Modding, self-hosting, and community creativity are first-class concerns.

### Guiding Design Questions

When introducing a system or mechanic, ask:

- **What new possibilities or meaningful decisions does this create?**
- Can players understand its behaviour through observation and experimentation?
- Does it interact naturally with existing systems rather than require isolated special cases?
- Does it encourage invention, exploration, construction, or discovery?
- Can players eventually automate away its repetitive aspects?
- Is it enjoyable at an appropriate level of abstraction?

### Long-Term Direction

Build a world in which a player can begin with nothing, establish shelter, discover materials, develop tools, construct machinery, automate industry, build a civilization, and eventually explore other worlds—all within a coherent set of underlying rules.

Potential directions include emergent metallurgy and chemistry, functional construction and vehicles, evolving ecosystems, environmental engineering, planetary exploration, and physically grounded orbital mechanics.

These are possibilities, not mandatory features or a fixed development roadmap.

**The measure of success is not simulation fidelity or the number of implemented systems. It is whether players continually discover interesting things to understand, problems worth solving, and ambitions worth pursuing.**
