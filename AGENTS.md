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
7. **Open and moddable.** Native code supplies mechanisms, mods (TypeScript) supply policy. Official
   content uses only public APIs, so the API stays honest. With every gameplay mod
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
| UI | Menus, HUD, inventory, wiki with native scenes, planetarium |

**Not there yet:** trees (being rewritten as species), creatures, boats, aircraft, swimming
by the body, breaking and structural strength, electricity and networks, crafting by operations,
carrying and routes (see Making and moving), and a survival loop
threading these together. Known gaps belong in `TODO.md`.

## Repo map

```
apps/game/src/      the Luanti fork, plus our systems in their own folders (below)
apps/game/ui/       the Vue app: menus, HUD, inventory, wiki, gallery
apps/game/server/   the game's own server folder: the official TypeScript mods every world runs
apps/game/game/     the game's baked data: palette.json, moves.json, vehicles.json, ground.json
apps/cli/           terra: the CLI, and the sidecar that runs TypeScript mods (sprint 119)
packages/types/     @terraverse/types: the TypeScript mod API's globals, as implemented
assets/             shipped static files; assets/matter/ holds elements, substances, reactions and thermo data
registry/           shaders, textures; registry/worlds is user data, never clean it up
registry/mods/      the game's own TypeScript mods (@terraverse/*), run by apps/game/server
library/            vendored dependencies (library/jolt: MIT; irr is upstream, leave alone)
scripts/            monorepo only: the bots, CI status
apps/game/scripts/  the game's dev loop, build, staging, bakes (bake/), release packaging (release/)
apps/game/tests/    engine unit modules (unit/), probes, the runners (run/) and whole-engine checks (checks/);
                    a library's own tests and tools sit in its folder (library/world/terragen/tests, tools/)
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
| `flora/`, `sprig/` | Plants grown from their site (pure, fixed point); small plants as the map keeps them |
| `physics/` | Jolt: rigid bodies, the player's movement (`mover`, `walk`, `reach`), the figure's pose |
| `piece/`, `workshop/` | Machines: parts and joints, compiled into physics |
| `orbit/` | Flight under a planet's gravity and air |
| `fluid/`, `weather/` | Atmosphere layer on a sphere, and live weather |
| `online/` | STUN, hole punching, UPnP, ticket gate, joining |
| `client/body`, `figure`, `liquids`, `shallow.h` | Our client parts: the player's body and fine water |
| `sidecar/` | TypeScript mods: spawns `terra runtime`, a window each step, answers its calls |
| `probe/` | The checks' probes: a headless server driven by `--probe <name>` (`apps/game/tests/checks/*.sh`) |

Mods are TypeScript, and only TypeScript: there's no Lua in the engine (sprint 119,
`planning/sprint-119-ts-mods.md`). The API is
designed in `apps/site/content/docs/mods/`; `terra` prepares a server folder into `.terra/`, and
the engine runs its mods in a sidecar process.
The wire's ops live in `apps/cli/src/runtime/wire/ops.ts`: change them there and run `bun run gen`
in apps/cli, which writes `sidecar/ops.h`.

The game's own TypeScript mods live in `registry/mods/` (`@terraverse/materials`, `@terraverse/commands`)
and run through `apps/game/server`, which staging prepares into `.output/server`. The engine
runs that folder in every world not given another. Their blocks register beside the engine's
(`server/game/registry`), so the world's matter, machines and the catalogue see them.
A block's weight, strength, grip and transparency come from its substance (`assets/matter`).

There's no game yet: only the skeleton (sprint 119). What's per
player or per tick is the engine's (`server/`: the sky, gravity by height, spawn, seam and travel,
the weather's readout, snow and ice, `player.cpp`: what a player's given, creative, sprinting, the
map, and `matter.cpp`: every block's matter registered and run). The generator's own blocks,
biomes and tree species are data (`assets/world/*.json`, `core:` names), and the matter's own
blocks come from its table: `server/game/registry` registers them all, reading each definition as
`server/game/defs` does (an unknown field is refused). Digging, placing, drops and what falls are
`server/game/interact` (a dug block gives itself, TEMP until breaking is tool × material); an entity
off the grid is one of a few native kinds (`server/game/kinds`: a machine, a dropped item, a falling
block). The weather finds the snow and ice it makes by
alias (`weather_snow`, `weather_ice`), as the generator finds rock (`mapgen_*`).

**Who may do what** is native (`server/access`): no passwords, everyone is their ArcLabs account.
A world played alone lets its one player in at once (`AUTH_MECHANISM_LOCAL`, 127.0.0.1 only), a
hosted one admits by ticket, and any other server takes no one. Permissions are `prefix:name`
(the engine's `core:fly`, a mod's `commands:kick`); roles (`player`, `builder`, `admin`) are sets
of them, kept in `<world>/access.json`, and the world's owner is always its admin. **Chat** is
native too (`Server::handleChat`, `server/game/commands`): a line is broadcast, and `/word` goes to the
engine's admin commands (`/travel`, `/river`, `/rivers`) or a TypeScript mod's.

`apps/game/scripts/stage-runtime.sh` mirrors shipped resources into `.output/`, which the run-in-place
engine uses as its share directory.

## How we differ from Luanti

We've diverged hard from Luanti and don't track upstream. Our systems live in our own
folders; where they reach into engine files, change those files as the design needs. Find them with
`git log` or by grepping for the system's name; the main divergences:

| Replaced | With | Hook points |
|---|---|---|
| Formspecs, Lua main menu, native HUD | Vue in Chromium (CEF), drawn over every frame | `main.cpp`, `clientlauncher`, `game.cpp`, `hud.cpp`, `inputhandler.cpp` |
| Map generators | `terra`, a planet on the atlas | `mapgen.cpp`, `emerge` |
| Movement | A Jolt character on the client, placed on the server | `localplayer`, `player_sao`, `content_cao` |
| Liquids | The world's matter: fills, virtual pipes, fine water | `server.cpp`, `client.cpp`, map format 31 |
| Whole-node ground | Cut, smoothed and loose ground at sixteenths | `map`, `content_mapblock`, `mesh_generator_thread` |
| Sky, clouds | Space, seen from where you stand | `game.cpp` |
| Entities that move by script | Machines as rigid bodies | `entity_sao`, `activeobject.h`, `server.cpp` |
| Auth, privileges, chat | ArcLabs tickets, roles and permissions (`server/access`), native chat | `server.cpp`, `serverpackethandler.cpp`, `network/mtp` |

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
  them, a worker, a machine or the player, is policy in a mod. Away from players a route runs
  Scheduled, conserving cargo and time, and fails honestly. The economy is sites joined by routes.
- **First slice:** sticks and stones as items, a bench with a few forms, a pick and a chisel giving
  rubble and cut blocks, placing cut blocks; then a waterwheel saw for the automation half. Prove
  that loop before creatures or rails.

**Every primitive has four parts:** properties on definitions, sparse state the engine owns,
mod verbs (query, inject, subscribe; a mod never polls blocks), and native couplings to other
primitives. A thing is native if it's per block, per tick or across many blocks, if the client
must predict it, or if it's a mechanism. A rule is a mod's.

**Physics.** Jolt, double precision, deterministic. Player and vehicles are predicted on their
client and corrected by the server. Everything simulated runs Live near players, then Settled,
Scheduled or Frozen further out; handovers conserve matter, energy and time.

**Content is data.** One declaration says what something is, and the framework derives the rest.
Variants live in tables, never `if` chains across files. Biomes: terragen decides where, the
data says what it's made of (`assets/world/biomes.json`); the generator refuses to start if a biome is missing. World settings go
in `world.mt` as `tf_*` keys, and the engine accepts only known keys.

**The wiki.** Wiki pages are native scenes running the real code, never animations, and every
scene is checked by `check:wiki` or `check:moves`. Worlds scenes replay
terragen's real erosion, baked by `bun palette`.

## The shell and UI

One Chromium page lives for the whole session, rendered offscreen and drawn over every frame.
Each file in `shell/` owns one concern.

**The bridge.** `ui/src/bridge/types.ts` is the source of truth; **change both sides together.**
- Intents (page → engine) are verbs registered with `shell.on`. An unanswered intent fails
  loudly. Verbs throw `Refused("…")` for an expected no.
- Slow verbs use `shell.onAsync`: a worker thread, so they must be pure.
- Messages (engine → page) carry a `kind`, so never name a payload field `kind`.
- The engine talks to the page with `Server::setUi(player, key, value)`, and answers its events
  in `terraUiEvent` (`server/player.cpp`); a TypeScript mod's go through the sidecar.
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

The root has the monorepo's commands (`bun dev`, `bun check`, `bun test:hot`, `bun test:game`); everything
else below is the game's, run from `apps/game` (`cd apps/game && bun check:wiki`).

```bash
bun dev                 # engine, Vite, game against the dev server (hot reload); --go enters a world; --profile two adds a second player
bun components          # the component gallery in a browser, mock engine, no game
bun run build           # engine + production UI
bash apps/game/scripts/build.sh -j8              # engine only, waits its turn if another build is running (after cmake -S . -B .output/build -DRUN_IN_PLACE=TRUE)
bun run stage           # refresh resources in the runtime tree
cd apps/game/ui && bun run typecheck
./.output/bin/luanti --server --gameid terrafirma --world <tmp dir> --port <n>   # headless boot: catches content and start-up errors
bun test:hot [library]   # the pure libraries' quick tests (each library's tests/, as library/world/terragen/tests): their own small binaries, seconds; the loop to keep running
bun test:slow [library]  # their slow ones: goldens, Earth-sized worlds
bun check                # before a commit: test:hot, every engine module, the UI's types
bun check --changed [base]   # the same, only what your uncommitted changes (or everything since base) reach
bun run test             # (apps/game) the full tier, as the gate runs it: every library's tests hot and slow, every engine module
bun run test <system|path|Module>...  # what covers far, physics, library/world/terragen, a file; --changed [base] as above
bun apps/game/tests/run/select/main.ts --changed   # just say what would run and why, and which changed files no suite is built against
./.output/bin/luanti --test-module TestFar                                       # one engine test module; --run-unittests for all
bash apps/game/tests/run/engine.sh [Module...]  # every engine unit test module in parallel, slowest listed first; skips (apps/game/tests/run/skip.txt) cite their job

bun check:land          # after a generation change: check:terragen, its unit tests and check:far side by side (~1 min); --quick: probe and default world only (~25 s)
bun check:terragen      # probed world == standalone terragen, plus unit tests
bun check:wiki          # every wiki scene in the engine sandbox
bun check:moves         # every Movement, Vehicles and Flow scene through the real code
bun check:matter        # a real fire on a live server
bun check:far           # far view against generated chunks
bun check:mods          # TypeScript mods: terra's unit tests, terra prepare, then the sidecar in a headless server
bun check:dev           # terra dev on a new mod: reloads, a content restart, a broken save, Ctrl-C
bun check:registry      # terra against the ArcLabs staging backend (../arclabs: arc u): login, publish, refusals, install, a headless boot
terral <args>           # terra from apps/cli source, against .output/bin's engine; the sidecar runs from source too (TERRA_CLI)
bun perf:gen            # where making a world spends its time, by step, cold and warm at 50k and 100k (SIZES=, CODE=)
bun palette             # re-bake palette.json after changing blocks, textures or species
bun check:palette       # fails if palette.json is stale
bun pose-sheet          # the figure's moves as PNGs
.output/bin/luanti --planetarium TF9.dGVycmE                                     # the planet viewer alone

bun ci:status           # CI now: what's running, what's red on master and at which step, and the CI Scoreboard (--since <date>)
arc gate terraverse     # build, the full tier (bun run test), UI and a headless world
arc deploy terraverse   # package.json version → tag → CI attaches every platform's package
arc deploy launcher     # apps/launcher, on its own release track (version in tauri.conf.json)
arc deploy site         # apps/site and install.sh to Cloud Run
bun package:linux <version>   # the portable Linux package from the current build
```

To tune by eye, render a code's grid to a PNG:
`apps/game/library/world/terragen/tools/tool.sh /tmp/render apps/game/library/world/terragen/tools/render.cpp && /tmp/render TF9.dGVycmE out.png [scale] [relief|plates|provinces|rain|season|biomes|fertility|currents|sediment]`
(`<code>@<body>` draws another body.)

**How agents verify:** build the engine, typecheck and build the UI, boot headless for start-up errors,
run the check for what changed, and use the gallery for UI. Agents may run the engine, the game
client and the render tools to see their work or capture images from it. Say
plainly what was and wasn't run. Commit only when asked.

## Code standards

**`STANDARDS.md` is the standard:** the ladder (value function → bag → leaf → module), the
failure states that move code up it, and the invariants. Read it before writing code. It applies in
full to new code; code being worked on climbs as it's touched; stable code waits. Offenders you
find are raised in your response, first. Below is how it's spelled here.

Here, specifically: expected refusals are `Refused` and `WorldCodeError`, shown to the player;
the seams are the bridge, file I/O, world settings, the wire and inventory slots; unwired things
also say so in the UI (a Notice).

### C++

The file limit is **~400 lines** of `.cpp`. The engine builds as C++17 (the shell as C++20).

| Rung | Spelled |
|---|---|
| Value function | A free function in an anonymous `namespace {}`: declared at the top of the `.cpp`, defined below the class it serves |
| Bag | A `namespace` (`namespace results { … }`) in the `.cpp`, or in a header when two files need it |
| Leaf | `class X` with a nested `struct Opts`, `explicit X(Opts)`, a few public verbs, private members. One `.h`, one `.cpp` |
| Module | A folder with one public header, its door. The orchestrator's public members are its leaves; its initializer list is the dependency graph, in declaration order; its constructor body is empty. Internals sit in the `.cpp`, a `detail` namespace, or behind pimpl |
| Manager | A `std::unique_ptr<T> current_`. Build the candidate (may throw, nothing changed), then `current_ = std::move(candidate)`; the old one's destructor is its disposal |
| Boot root | `static std::unique_ptr<X> boot(Opts)` doing the work and throwing a structured error, over a private constructor that only wires |

```cpp
// the door: world_matter.h
class WorldMatter {
public:
	struct Opts { Ground &ground; const Catalogue &catalogue; };
	explicit WorldMatter(Opts o);

	Heat heat;            // leaves, declared in the order they're built
	Reactions reactions;
	Flow flow;
};

// world_matter.cpp: the graph, and nothing else
WorldMatter::WorldMatter(Opts o) :
	heat({.ground = o.ground}),
	reactions({.catalogue = o.catalogue, .heat = heat}),
	flow({.ground = o.ground, .heat = heat})
{}
```

Ownership follows the tree: a parent owns its children by value (or `unique_ptr` when
swapped or optional); a child is handed its siblings as references through `Opts` and never owns
them. A reference or raw pointer never owns.

Opts are built with **designated initializers** (`{.ground = ground}`), so the wiring names both
sides. They're C++20: GCC accepts them in our C++17 files, but MSVC refuses them below
`/std:c++20`, so the Windows build breaks. Moving the engine to C++20 is the first step of this
standard; until it lands, new `Opts` are written as above only where the file already
builds as C++20 (the shell), and elsewhere positionally with each field named in a comment.

Failure states only C++ has:

| You see | Move |
|---|---|
| `new` or `delete` | A member, or `std::make_unique`. Ownership follows the tree |
| An owning raw pointer | `std::unique_ptr` |
| An out-parameter (`void f(T &result)`) | Return the value, or a struct of them |
| A `bool` argument (`draw(true, false)`) | An `enum class`, or a field of `Opts` |
| Inheritance to share code | Composition: a member. Inheritance is for an interface with several real implementations |
| A mutable `static` or global (`g_settings` and kin) | A member of the noun that owns it, passed down |
| A macro beyond include guards and platform switches | `constexpr`, a function or a template |
| `reinterpret_cast`, or a `static_cast` quieting a mismatch | Fix whichever shape is wrong |
| A `switch` on a kind repeated across files | A `constexpr` table indexed by an `enum class` |
| A header including what only its `.cpp` needs | Move the include down; forward-declare; pimpl if the private side is heavy |

**Luanti's code** is legacy: god classes (`Game` in `client/game.cpp`, `Server`, `Client`),
globals, positional parameters. Its size alone is never a reason to refactor it. Touching it is:

| You see | Move |
|---|---|
| About to add a method or member to a god class | Don't. The concern becomes its own module in our folder; the god class gets a member and one call into it |
| About to edit a block of logic inside a god class | If it's in the way, extract that concern first, then change it in its new home |
| Our system reaching into an engine file | A hook point only: the logic lives in our folder |

The god files only shrink. No reference module exists yet in C++; `sandbox/world` is the first
candidate (its `Ground` is already an `Opts` of dependencies).

### TypeScript

`apps/game/ui`, `apps/cli`, `registry/mods`, `packages/types`: the arclabs form. Every noun is a
capitalized factory, `X(opts)` returning a handle, its type `ReturnType<typeof X>`; a module is
a folder with one `index.ts`; wiring is `key: value`, never shorthand, and every child is built
above the `return`, which only assigns. Vue components are the UI's leaves, composables its
nouns (`ui/src/kit` for patterns). The file limit is ~300 lines.

## Licensing

- The engine is LGPL, so engine changes stay public. The game's own code and art are our choice.
- **Never build on Mineclonia or VoxeLibre** (GPL). GPL code is never copied.
- Vendored code keeps its licence beside it.
- Assets need a redistributable licence and an entry in `assets/CREDITS.md` before they're committed.
  Default art is Good Vibes (Acaitart, CC BY 4.0). Nothing resembling Minecraft characters or branding.
# Standards

How code is shaped, in any language. This file is the shared core: the ladder every piece of
code climbs, the failure states that move it up, and the invariants that hold everywhere. Each
repo's AGENTS.md binds it to a language: how each rung is spelled, the language's own failure
states, and the reference files to copy.

One idea, applied recursively: **nouns own verbs, parents own children, and dependencies flow
down.** A reader learns a system by reading its root and descending only where they need to.

## The ladder

Code sits on one rung. When a trigger fires it climbs one rung: that is the only move, and no
rung is skipped.

| Rung | What it is | Climbs when |
|---|---|---|
| **0. Value function** | Pure: values in, a value out. No state, no I/O | It needs state or I/O → **2**. More than three in a file → **1** |
| **1. Bag** | A named group of related value functions | It needs shared state or I/O → **2**. It grows a second theme → two bags |
| **2. Leaf** | One noun owning one concern, with a small verb surface | It builds children of its own, passes the file limit, or its sentence needs an "and" → **3** |
| **3. Module** | A folder with one door; behind it an orchestrator wiring leaves, doing no logic | Its children need each other → redraw. It grows a second domain → sibling modules under a parent |

**A leaf is finished when its responsibility fits one sentence with no "and".** An orchestrator
builds its children, wires them and exposes a shape; it contains nothing else.

Two special forms of a leaf or module:

- **Manager.** It owns something replaceable (a connection, a loaded mod, a compiled machine).
  Callers never hold the inner thing; verbs go through `current`. A swap builds the new one first,
  then drops the old: overlap, never a gap, and a failed candidate leaves nothing changed.
- **Boot root.** Building it does work that can fail. That work lives in one `boot` that returns a
  whole object or a structured error, never a half-built one. Everything else constructs by wiring
  only.

## Failure states

Every state of the code has one next action. Each of these can be seen in a diff or counted.

| You see | What's broken | Move |
|---|---|---|
| More than three loose functions in a file | Rung 0 has overflowed | Group them into a bag |
| A loose function doing I/O or touching state | A verb with no noun | Find the noun that owns it, or make one |
| A file over the language's limit | Several concerns under one name | Name each in a sentence; each becomes a leaf; the file becomes a module |
| `if`, a loop, `try` or arithmetic in an orchestrator | A root doing work | Move the logic into the child it belongs to |
| More than three positional parameters, or two of one type side by side | Unpackaged dependencies | One `Opts` with named fields |
| A dependency reached through a global or singleton | Dependencies not flowing down | The parent passes it in through `Opts` |
| A leaf importing a sibling | A sideways reach | The parent passes it in; a concern two siblings need belongs to the parent |
| Two children that can't be built without each other | A wrong boundary | Redraw it. Thunks that break the cycle hide the signal |
| The same branch on a kind in two places | Variants as branches | A table keyed by the kind, read by every decision |
| Callers holding something that gets swapped | A missing manager | Put it behind its noun as `current` |
| Construction doing I/O, or able to half-fail | Work at construction | A boot root; construction wires only |
| A module's insides reached from outside | A second door | Promote it to the door deliberately, or redraw the boundary |
| A helper above the thing it serves | Precedence inverted | The primary thing first, its privates below |
| The same block of logic in two siblings | A shared concern duplicated | Promote it to one leaf both receive |
| A call A must make before B, by convention | Temporal coupling | A returns B |

## Invariants

Correctness, not style. Breaking one in new code is a bug.

1. **Fail loudly.** No swallowed errors, no catch-and-continue, no fallback value hiding something
   that must exist. Invalid state throws, everywhere.
2. **Validate at boundaries, trust inside.** Files, the network, processes, user input and
   settings are checked once, at the seam, into a strict shape. Past the seam nothing re-checks.
3. **Make invalid states unrepresentable.** If a function can receive bad input, its signature is
   wrong.
4. **No casts to silence a mismatch.** If two shapes disagree, one is wrong: fix it.
5. **Errors are structured**, thrown where they're detected, and an expected refusal reaches the
   person as a reason.
6. **Unwired things say so.** A stub throws "not wired yet"; it never returns empty pretending to
   work.
7. **No unmarked bypasses.** A workaround carries `TEMP:` with a date and a reason, or isn't written.
8. **Data over branches.** Variants live in tables.
9. **Caches are keyed by what actually changes them.** If the real key is too dear, don't cache.
10. **Comments say why**, at the density of the surrounding code.

## Naming and shape

- **Hierarchy over prefixes.** `world.matter.heat.step()`, not `stepWorldMatterHeat()`. One word
  at each level; the path is the context.
- **The surface is designed, not accumulated.** A top-level API reads like the product. If it
  needs a comment to say what it does, the name or the shape is wrong.
- **Construction reads as the graph.** Every child is built in dependency order before the shape
  is exposed, and wiring names both sides (`http: http`, `.http = http`): the wiring is the
  content of an orchestrator, never noise.

## Moving a codebase toward this

The standard applies in full to new code. Existing code climbs as it's worked on:

- **New code** is written on the right rung from the start.
- **Code being actively changed** is brought to standard when the time is right or it's getting in
  the way: the concern being touched is moved up first, then changed.
- **Stable code** is left alone until work reaches it. A violation in code nobody is changing is
  a note, not a task.
- **Never grow an offender.** A file already over the limit, or a class already doing ten things,
  gets no new member. The new concern becomes its own leaf or module, and the offender keeps one
  call into it.

## For agents

- Before answering about code you've read, check it against the failure states. **Raise what you
  find in your response**, leading with it: what's wrong, which rule it breaks, and whether to fix
  it now or carry on. Never bury it at the end, never save it for later.
- Small things in code you're touching (a name, a loose helper, a dead include) are fixed as you go.
  Structural things (a boundary, an orchestrator doing work, a god file growing) are raised.
- Say "stop, this needs fixing first" when the foundation is unsound for what's about to be built
  on it.
- Leave every file you touch at the same rung or higher.

**Why it's stricter than it feels.** The cheapest change, one more function in a file that already
has some, is invisible when it happens and compounds. An orphan function is a precedent: the next
person, or agent, copies the shape they find. The default must be the right shape even when one
instance would be fine.

