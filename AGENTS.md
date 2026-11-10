# Terra Firma

An open-source voxel universe, packaged as a game. Minecraft's insight is that it's a
universe people make games in; we take that further: real planets, physics and matter
underneath, and survival, racing, stealth or Hunger Games as things played on top.
We supply the universe and first-class official games. The community supplies most of
the content.

It's a fork of the **Luanti** engine (C++, LGPL-2.1+). We keep Luanti's rendering,
meshing, networking and map storage, and replace its shell, world generation, movement
and much of its simulation.

This file is the map and the rules. If a change contradicts a decision here, raise it
rather than drift. History is elsewhere: `planning/` holds each sprint's plan and arc,
and `planning/agents-archive.md` the old long version of this file, with every sprint's
"Built" entry.

## What the game is

**The test for anything we add:** what possibilities or decisions does it create, can
players work it out by watching and trying, and does it connect to what's already here?
The measure is whether players keep finding things to understand, problems worth solving
and ambitions worth pursuing, not simulation fidelity or feature count.

1. **Emergence over prescription.** A few powerful primitives, not special cases. Water
   boils away on the Moon because of what water is, not because a rule says so.
2. **The world is the game.** Everything lives where it makes sense: ore by geology, crops
   by climate, power by rivers and wind. Distance is real, so no teleporting chests.
3. **Knowledge is progression.** Real intuition mostly works, failures explain themselves,
   and recipes are learnable without a wiki.
4. **Problems create purpose.** Builds exist because they're needed, and systems feed each
   other. Automation is required by kind (rate, continuity, precision, environment, scale
   of energy), never by quantity.
5. **Realism serves play.** A law is modelled only as deep as it changes what a player
   decides. Readable in seconds, deep if you dig. Simulate where the effect lives, at the
   cheapest honest tier (`planning/simulation-scales.md`).
6. **Freedom through construction.** Players invent solutions from materials, shapes and
   joints. Most of the creative menu should eventually be shapes and mixtures, not items.
7. **Open and moddable.** Native code supplies mechanisms, Lua supplies policy. Official
   content uses only public APIs, so the API stays honest. With every `tf_*` gameplay mod
   off you have a clean world to build your own game on.

**The direction, not a fixed roadmap:** stone, bronze, iron, mechanical, industrial, modern,
space, each pulling the player to new places. Possible business: managed hosting, then a
mod and map registry. Nothing is built for that yet.

## What exists

| Area | State |
|---|---|
| Worlds | Planets from a world code: plates, erosion, climate, rivers, geology, ores, craters, whole star systems |
| Matter | Substances, mixtures, reactions, heat, flow and air, run in the world and the wiki alike; sealed rooms still need a design pass |
| Body | Physical player: stances, parkour, climbing, falling |
| Machines | Parts, joints, suspension, engines, seats, driven cars, on Jolt |
| Space | Gravity as a field, orbits, a real sky, flying to orbit in creative |
| Online | ArcLabs accounts, friends, hosted worlds by hole punching (no relay yet) |
| UI | Menus, HUD, inventory, wiki with native scenes, Lab, planetarium |

**Not there yet:** trees (being rewritten as species), creatures, boats, aircraft, swimming
by the body, breaking and structural strength, electricity and networks, crafting by operations,
carrying and routes (see Making and moving), and a survival loop
threading these together. Known gaps belong in `TODO.md`.

## Repo map

```
apps/game/src/      the Luanti fork, plus our systems in their own folders (below)
apps/game/ui/       the Vue app: menus, HUD, inventory, wiki, gallery
apps/game/game/     the game in Lua (mods/tf_*), and baked data: palette.json, moves.json, vehicles.json, ground.json
assets/             shipped static files; assets/matter/ holds elements, substances, reactions and thermo data
registry/           shaders, textures, upstream client mods, devtest; registry/worlds is user data, never clean it up
library/            vendored dependencies (library/jolt: MIT; irr and lua are upstream, leave alone)
scripts/            dev loop, staging, palette bake, the check:* harnesses, release packaging
planning/           sprint plans, arcs and design notes
cmake/Modules/CEF.cmake   fetches the pinned CEF build
.output/            ignored: build, runtime staging, caches, accounts
```

Our folders in `apps/game/src/`:

| Folder | Owns |
|---|---|
| `shell/` | The web UI host: embedded Chromium, bridge, screens, state stream, wiki scenes |
| `terragen/` | The world model: a pure, fixed-point function of the world code, no engine deps |
| `mapgen/mapgen_terra.*`, `mapgen/terra_*` | The `terra` generator: blocks from terragen |
| `planet/` | The atlas: voxel grids laid round a sphere |
| `far/` | The far view to the horizon, and the palette bake |
| `planetarium/`, `air/`, `milkyway/` | The planet viewer and game sky, atmosphere optics, the galaxy |
| `matter/` | Substances, mixtures, reactions |
| `sandbox/` | Block physics of matter: `WorldMatter` runs in the world, `Stage` in wiki scenes |
| `fill/`, `pile/`, `ground/`, `shape/`, `sculpt/` | Liquid in a node's open space, loose ground, cut and smoothed ground, shapes |
| `physics/` | Jolt: rigid bodies, the player's movement (`mover`, `walk`, `reach`), the figure's pose |
| `piece/`, `workshop/` | Machines: parts and joints, compiled into physics |
| `orbit/` | Flight under a planet's gravity and air |
| `fluid/`, `weather/` | Atmosphere layer on a sphere, and live weather |
| `online/` | STUN, hole punching, UPnP, ticket gate, joining |
| `client/body`, `figure`, `liquids`, `shallow.h` | Our client parts: the player's body and fine water |

Lua mods: `tf_world` (blocks, biomes, materials), `tf_matter`, `tf_player`, `tf_sky`,
`tf_weather`, `tf_planet`, `tf_flight`, `tf_lab`, `tf_craft` (making: forms, processes, stations).

`scripts/stage-runtime.sh` mirrors shipped resources into `.output/`, which the run-in-place
engine uses as its share directory.

## How we differ from Luanti

Our code lives in our folders. Upstream files get **small hooks** guarded by `#if USE_SHELL`,
so merges stay cheap. Upstream C++ keeps upstream style. Find the hooks with
`git log` or by grepping for the system's name; the main divergences:

| Replaced | With | Hook points |
|---|---|---|
| Formspecs, Lua main menu, native HUD | Vue in Chromium (CEF), drawn over every frame | `main.cpp`, `clientlauncher`, `game.cpp`, `hud.cpp`, `inputhandler.cpp` |
| Map generators | `terra`, a planet on the atlas | `mapgen.cpp`, `emerge` |
| Movement | A Jolt character on the client, placed on the server | `localplayer`, `player_sao`, `content_cao` |
| Liquids | The world's matter: fills, virtual pipes, fine water | `server.cpp`, `client.cpp`, map format 31 |
| Whole-node ground | Cut, smoothed and loose ground at sixteenths | `map`, `content_mapblock`, `mesh_generator_thread` |
| Sky, clouds | Space, seen from where you stand | `game.cpp` |
| Entities that move by script | Machines as rigid bodies | `luaentity_sao`, `activeobject.h`, `server.cpp` |
| Auth | ArcLabs tickets for online play | `server.cpp`, `serverpackethandler.cpp`, `network/mtp` |

- Stock Luanti peers are refused. **Bump the protocol** (`network/networkprotocol.cpp`) whenever
  it changes, so mismatches fail cleanly.
- The non-shell build isn't a supported target. Irrlicht here has **no `irr::` namespace**.
- CEF needs C++20: shell sources are compiled `-std=c++20`, set on the target.

## Core mechanics

**World code.** `TF<n>.<seed>.<settings>`: seed, only the land settings that differ from
default, and the system. It carries land, not rules. **Generation is deterministic:** fixed
point for anything that decides terrain, floats only for visuals, the same on any machine. A
code must never silently change meaning: a generator change bumps the version, and a unit-test
golden pins it. Run `bun check:terragen` after any generation change.

**Planets.** terragen is a function of a place on the sphere, never of block (x, z). A 64 002
cell icosphere grid holds plates, erosion, climate, rivers and geology. A column is sampled from
it with fine detail. A body's processes come from its card (star, orbit, mass, air), never its
name. The atlas lays square grids round the sphere, 2k–16k blocks round.

**Matter.** One chain: substances (`assets/matter/*.json`) → mixtures → reactions (element
balance checked on load, rates by Arrhenius, heats from thermo data). Blocks declare `_matter`,
and heat and state do the rest. The same primitive serves a crucible, a room and a sea. Native
never knows "coal" or "bronze". **Not doing:** per-block gas dynamics, finite-element stress,
molecular chemistry, subdividing the whole world.

**Making and moving** (the direction for crafting and logistics; none of it is built yet):
- **Hand-sized is magical, bigger is built.** If it fits in a hand it's an item from a catalogue;
  anything bigger (carts, cranes, mills, buildings, engines) is engineered from blocks, pieces and
  joints. No sub-voxel modelling of tools, edges or lashings.
- **An item is form × material.** The form (axe, saw, tongs, rope, nails, axle, plank) is
  predefined and gives the affordances; the material, from `assets/matter`, gives the properties
  (hardness, density, toughness, melting point). A flint, bronze and steel axe are one form, their
  differences derived, never authored. Quality beyond that isn't modelled yet.
- **Things and stock.** *Things* are catalogue forms, used in the hand or built into machines as
  fittings (an axle becomes a joint, rope a rope joint). *Stock* (cut block, slab, beam, plank,
  brick) is material in building sizes and becomes voxels when placed. *Loose* matter (soil,
  rubble, sawdust, ore) lies in piles. Everything has real mass; stacks are limited by weight and
  volume, never a count.
- **Matter is simulated, shape is chosen.** Smelting, mixing, firing and cooking stay real, in the
  world. Making a form is a *process* (knap, carve, saw, cast, forge, throw, twist, haft): a
  station, inputs and time. A process needs a tool, a station or heat and works on a material
  *property* (brittle, ductile, fibrous, plastic), never a name; what can't be made is offered
  greyed, with the reason ("needs a saw", "too cold to work"), so there's no recipe to learn.
- **The tool decides which side it comes out.** Breaking is tool × material → output: granite by a
  pick is rubble, by a chisel a cut block; an oak by an axe is a log, a log at a saw is planks and
  sawdust. Mass is conserved across the line, and waste always lands somewhere as loose matter.
- **One process table for hands and machines.** A bench saw is you; a sawmill is the same process
  driven by a shaft; later a worker runs it. Crafting by hand starts things, and machines exist
  for rate and continuity, never to skip a recipe.
- **Workplaces collapse labour; distance stays real.** A station is a physical place: press F
  and what's on and around it (worktop, rack, the chest beside it) is what you work with, never
  a global inventory. Tools hung on it unlock and quicken its processes; a few work in the hand,
  slower. Stations: bench, forge and anvil, kiln, wheel. The bench makes hand-sized things;
  large things are made in the world, by processes on what's there.
- **Building is easy, distance is what costs.** Some magic is needed or nothing gets built: blocks
  and trees are ordinary blocks, broken into the inventory and placed from it, as in Minecraft. The
  inventory holds amounts by mass (metric: "4.5 kg of dirt", the world's sixteenth voxels its grain),
  never block counts. Weight never refuses a pickup; it slows you, down to a crawl, so dragging ore
  home is possible and slow. Hauling is built because it's faster: sled, handcart, animals in
  harness, boats, rails. The first cart must come early. Each era is a jump in what you can move, and
  big geological deposits make a route worth engineering. Physical felled trees and winched logs are
  later spectacle, never something the economy depends on.
- **Do it once, then hand it over.** Automation is two primitives: a *pattern* (a build or a run
  of processes recorded at a workplace) and a *route* (a journey recorded between workplaces). Whoever runs
  them, a worker, a machine or the player, is policy in Lua. Away from players a route runs
  Scheduled, conserving cargo and time, and fails honestly. The economy is sites joined by routes.
- **First slice:** sticks and stones as items, a bench with a few forms, a pick and a chisel giving
  rubble and cut blocks, placing cut blocks; then a waterwheel saw for the automation half. Prove
  that loop before creatures or rails.

**Every primitive has four parts:** properties on definitions, sparse state the engine owns,
Lua verbs (query, inject, subscribe; Lua never polls blocks), and native couplings to other
primitives. A thing is native if it's per block, per tick or across many blocks, if the client
must predict it, or if it's a mechanism. A rule is Lua.

**Physics.** Jolt, double precision, deterministic. Player and vehicles are predicted on their
client and corrected by the server. Everything simulated runs Live near players, then Settled,
Scheduled or Frozen further out; handovers conserve matter, energy and time.

**Content is data.** One declaration says what something is, and the framework derives the rest.
Variants live in tables, never `if` chains across files. Biomes: terragen decides where, Lua
says what it's made of; the generator refuses to start if a biome is missing. World settings go
in `world.mt` as `tf_*` keys, and the engine accepts only known keys.

**The Lab and the wiki.** The Lab (`tf_lab`) is one tiny world per mechanic: data in `lab.json`,
behaviour in Lua, each with an `expect`. Wiki pages are native scenes running the real code, never
animations, and every scene is checked by `check:wiki` or `check:moves`. Worlds scenes replay
terragen's real erosion, baked by `bun palette`.

## The shell and UI

One Chromium page lives for the whole session, rendered offscreen and drawn over every frame.
Each file in `shell/` owns one concern.

**The bridge.** `ui/src/bridge/types.ts` is the source of truth; **change both sides together.**
- Intents (page → engine) are verbs registered with `shell.on`. An unanswered intent fails
  loudly. Verbs throw `Refused("…")` for an expected no.
- Slow verbs use `shell.onAsync`: a worker thread, so they must be pure.
- Messages (engine → page) carry a `kind`, so never name a payload field `kind`.
- Game Lua talks to the page with `player:set_ui(key, value)` and `core.register_on_ui_event`.
- **The server is the authority.** No optimistic updates. The store holds `null` for what the
  engine hasn't sent: render nothing rather than invent a value.
- The page gets the keyboard only while an editable element has focus (`bridge/keyboard.ts`).

**UI rules.**
- Build every screen from the kit (`ui/src/kit`). New patterns become kit components, with a gallery
  specimen. Everything must render in the gallery against `gallery/mock.ts`.
- One light, top-left: use `--raised`, `--sunk`, `--ledge`, never a hand-written bevel. Tokens are
  in `theme.css`. Hard edges, no radius. Accent `rgb(0 148 210)` and `»` are the only brand marks.
- Fonts are bundled (Silkscreen, JetBrains Mono). Labels never wrap in buttons, panels never scroll
  sideways, grid children get `min-width: 0`.
- The page re-uploads to the GPU on repaint: avoid continuous full-screen animation. Heavy compute
  goes in a Web Worker.

## Running and checking

```bash
bun dev                 # engine, Vite, game against the dev server (hot reload); --go enters a world; --profile two adds a second player
bun components          # the component gallery in a browser, mock engine, no game
bun run build           # engine + production UI
cmake --build .output/build -j8          # engine only (after cmake -S . -B .output/build -DRUN_IN_PLACE=TRUE)
bun run stage           # refresh resources in the runtime tree
cd apps/game/ui && bun run typecheck
./.output/bin/luanti --server --gameid terrafirma --world <tmp dir> --port <n>   # headless boot: catches Lua errors
./.output/bin/luanti --test-module TestTerragen                                  # one test module; --run-unittests for all

bun check:land          # after a generation change: check:terragen, its unit tests and check:far side by side (~1 min); --quick: probe and default world only (~25 s)
bun check:terragen      # probed world == standalone terragen, plus unit tests
bun check:lab           # every Lab scenario headless, each `expect` checked
bun check:wiki          # every wiki scene in the engine sandbox
bun check:moves         # every Movement, Vehicles and Flow scene through the real code
bun check:matter        # a real fire on a live server
bun check:far           # far view against generated chunks
bun perf:gen            # where making a world spends its time, by step, cold and warm at 50k and 100k (SIZES=, CODE=)
bun palette             # re-bake palette.json after changing blocks, textures or species
bun check:palette       # fails if palette.json is stale
bun pose-sheet          # the figure's moves as PNGs
.output/bin/luanti --planetarium TF9.dGVycmE                                     # the planet viewer alone

arc gate release        # build, tests, UI and a headless world
arc deploy release      # package.json version → tag → CI attaches the Linux package
bun package:linux <version>   # the portable Linux package from the current build
```

To tune by eye, render a code's grid to a PNG:
`scripts/terragen-tool.sh /tmp/render scripts/terragen-check/render.cpp && /tmp/render TF9.dGVycmE out.png [scale] [relief|plates|provinces|rain|season|biomes|fertility|currents|sediment]`
(`<code>@<body>` draws another body.)

**How agents verify:** build the engine, typecheck and build the UI, boot headless for Lua errors,
run the check for what changed, and use the gallery for UI. **Don't launch the game client or
screenshot the user's desktop:** hand it over, and the user runs it and shares screenshots. Say
plainly what was and wasn't run. Commit only when asked.

## Code standards

- **Fail loudly.** No swallowed errors, no `?? fallback` hiding a value that must exist. Expected
  refusals are structured (`Refused`, `WorldCodeError`) and shown to the player.
- **Validate at boundaries, trust inside:** bridge, file I/O, world settings, inventory slots.
- **No `as` casts** to silence a type mismatch. Fix the shape.
- **Data over branches.** Variants live in tables that every decision reads.
- **One concern per module:** factories and composables in TS; classes, RAII and pimpl in C++.
- **Comments say why**, at the density of the surrounding code.
- **Unwired things say so**, in code (throw "not wired yet") and in the UI (a Notice). Never pretend.

## Licensing

- The engine is LGPL, so engine changes stay public. The game's own code and art are our choice.
- **Never build on Mineclonia or VoxeLibre** (GPL). GPL code is never copied.
- Vendored code keeps its licence beside it.
- Assets need a redistributable licence and an entry in `assets/CREDITS.md` before they're committed.
  Default art is Good Vibes (Acaitart, CC BY 4.0). Nothing resembling Minecraft characters or branding.
