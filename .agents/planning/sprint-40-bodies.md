# Sprint 40 — Bodies: many worlds, one window

## Decision

A world is its system, not one planet: every planet and moon big enough to
stand on can be visited, all at once, by different players. Getting
between them is `/travel <body>` behind a loading screen this sprint; the
seamless way (leave, fly, arrive) is sprint 41's, built on this.

- **One window, many atlases.** The map's 225 slots each hold a tile of
  some body: `(body, region, ti, tj)`. Each body has its own atlas (its
  shape), generator and storage. The window is lifted out of
  `planet::Atlas` into its own noun, the server's one copy shared by every
  reader, as today.
- **Which bodies.** The world's system (`terragen::System::forWorld`), at
  one scale: the playable body's blocks per km. A body is visitable when
  it has a surface (not a giant, not the star) and a radius of at least
  the smallest world size (2.6k blocks): on a small world its moon is
  too small and isn't. Asking for one that isn't throws.
- **Places, not positions.** `{ body, cell }` is where something is on a
  world: players, beds, homes, mod storage. Map positions mean nothing
  once two bodies share the window.
- **Not this sprint.** Bodies that look like what they are (an airless
  moon, an ice world): every body is made by today's generator at its
  radius with its own seed, an Earth-like world of its size, and says so
  (`// TEMP:` where it's chosen). Sky per body; other bodies in the game
  sky; small bodies (km-sized: their own handling); space.

## Steps

1. **The window as its own noun, one body.** `planet::Window` (slots,
   generations, layouts, its code) apart from the atlas (one body's shape,
   node ↔ cell within a tile). Every reader goes through the window to a
   body. Nothing visible changes: every check passes as before.
2. **Tiles name their body.** `TileId { body, region, ti, tj }`, the
   window's code with bodies, cells found through the slot's body.
3. **A generator and a database per body.** Mapgen asks the slot's body;
   `<world>/bodies/<id>/map.sqlite`; ore and decoration seeds by body and
   spot (closes sprint 39's open item).
4. **The client.** The system once on joining (with the window, on one
   channel: closes the ATLAS/WINDOW ordering risk); the window names
   bodies; a client builds a body's atlas when a tile of it first comes.
5. **Places.** `core.planet` places; players saved by place; beds, homes
   and mod storage moved onto it.
6. **Travel.** `/travel <body> [lat lon]`: the tiles there paged in, the
   player held behind a loading screen until they arrive, then put there.

## Proof

- Unit: two bodies in one window; a tile of each in neighbouring slots
  stay their own (a step off one never lands on the other); window codes
  with bodies round-trip.
- `check:bodies` (headless): two players, one on the playable body and
  one on its moon; a mark on each; both survive the window paging; a
  player who travels and comes back finds theirs.
- Every check that passed at sprint 39's end passes.
- Cody: `/travel moon`, walk, `/travel` home.

## Built (2026-10-28)

Steps 1 and 2 as one: the window stays the atlas's, which now holds every
body (a separate window noun would have duplicated the node layer, which
is the map's, not a body's).

- `planet::Atlas(bodies)`: each body's shape (`body(b)`, `bodies()`;
  `HOME` = 0, the world's playable body, always first). `atlas()` is gone:
  every caller names its body.
- A cell, a tile, a spot and a frame carry their body
  (`Atlas::Cell(body, cell)`; `TileId` and `Spot` take every part, so a
  tile without its body doesn't compile). `bodyAt(node)`. The window code
  is `tiles-v2` (`slot:body,region,ti,tj`); `planet_cell` is
  `body region i j k …`. `view()` throws for a block of another body.
- `Pager::keep` takes points with their body; `all()` fits every body's
  tiles or none.
- Each land reader serves one body: `terra::Columns(…, atlas, body)`,
  `AtlasLand`, `far::Generator`, `far::Tiles`, `far::View`. Asked for
  another body's node, they throw.
- Protocol 64: `TOCLIENT_ATLAS` carries every body's code and goes on the
  window's channel (closes sprint 39's ordering risk); `TOCLIENT_WINDOW`
  names each tile's body.
- Still the home body's, marked `// TEMP:`: the map generator's land and
  `core.terra.surface` (step 3), the game's far view (step 4), the server's
  sky and orbit.
- Worlds from before this (a `tiles-v1` window, the old `planet_cell`)
  are refused at start, as sprint 39's were.

Proved: TestPlanetAtlas, TestPlanetWindow, TestPlanetMap, TestAtlasView,
TestFar, TestFarPalette, TestScale pass; TestAtlasRender as at sprint 39
(5 holes, the cap's budget). check:window, atlas-world, atlas-land,
atlas-debug, atlas-seams, far pass. `check:atlas` is deleted: it built
the atlas lab, gone since sprint 31 (TestAtlas still runs as a unit test).

## Step 3 (2026-10-28)

- `terragen::bodyWorlds(home code, home atlas)` (`terragen/worlds.h`): the
  home first, then every body of the world code's system that isn't a
  giant and is at least `SMALLEST_BODY` (2,607) round at the home's blocks
  per km. Each is a world code of its own: the home's at its size, its text
  and seed the home's `@id` (the world model's grid is cached by text: two
  bodies never share one). `// TEMP:` every body is an Earth-like world of
  its size until sprint 41.
- At the Earth's size: Earth, Cinder, Vesper, Moon, Aster, Pyre, Nacre,
  Hale, Morrow (9; the Jovian is a giant). At the smallest size, the home
  alone.
- The server lays its atlas from them (planet_atlas the home's,
  mgterra_code the rest); the map generator builds a body's land (world,
  columns, cells, reader) the first time a chunk of it is made, and picks
  each column's body by its slot. A chunk is seeded by its body and spot.
  (No mod registers ores or decorations yet: sprint 39's open item about
  their seeding has nothing in it today.)
- Storage: `MapDatabasePlanet` keeps each body's blocks in its own
  database, opened when first needed: the home's is the world's own
  `map.sqlite`, another's `bodies/<id>/map.sqlite`. A read-only backend
  covers the home only.

Proved: TestBodies (new): which bodies at the smallest and the Earth's
size, at the home's scale, unique ids, texts and seeds; two bodies' same
square are two tiles in two slots, their cells, spots and frames their own,
a view across them refused, the window code round-trips; a block of the
moon saved in its database and the home's at the same spot in the home's.
check:window, atlas-world, atlas-land, atlas-debug, atlas-seams, far pass
(atlas-seams' count of places varies run to run, 373–505: its movers are
timed).

## Step 4 (2026-10-29)

- The game's far view is the body the player stands on: made again when
  their feet are on another body's tile (kept while they're in seam).
- `far::Generator` builds a body's world from its own code
  (`terragen::bodyWorlds` from the horizon's, the home's), so another
  body's horizon is its land, not the home's at its size.
- `core.terra.surface` and `core.terra.land` ask the body of each column
  (a generator per body, built when first asked; an area over several
  slots reads each column from its own body).
- Fixed on the way: a block was drawn where its chart puts it on its body,
  in the eye's space, whatever body it was: a moon tile in the next slot
  would have been drawn over the home. `AtlasView::transform` draws only
  the eye's body's blocks (the server only sends those: its shells are the
  eye's grid carried on, which never leaves its body).

Proved: TestFar `testABodysOwnLand` (the moon's generator is the moon's
world, and refuses a home column); TestAtlasView
`testAnotherBodyIsntDrawn` (the same tile of two bodies side by side: the
eye's drawn, the other's not). TestFar, TestFarPalette, TestAtlasView,
TestBodies pass; check:far, atlas-land, window, atlas-world pass.

## Step 5 (2026-10-29)

- Places: `core.planet.place(pos)` → `{ body, region, i, j, k, x, y, z }`
  (the body by its id, the cell, the offset from its node's centre), and
  `core.planet.pos(place)` → the position it is at now, its tiles put in
  the window. `core.planet.bodies()` (id, name, radius; the home first),
  `core.planet.body_at(pos)`. `atlas_node`'s body, `atlas_cell`'s and the
  window's tiles' are ids. Players' `planet_cell` names their body by id.
- What mods keep: no mod keeps a position (no beds or homes yet; the map's
  explored record is by the world model's column, the home's). So there's
  nothing to move onto places today; what comes next keeps places.
- The world model by body: `core.terra.sun`, `climate`, `geology`,
  `cover` and `species` ask the world of the body under the position
  (each column its own in an area); `calendar` and `world` are the
  world's (the home's). `core.terra.weather` on another body is still,
  dry air at its climate's temperature, `atmosphere = false` (`// TEMP:`
  only the home's air moves, until sprint 41). Before, all of them
  answered the home's at the same latitude wherever the player stood.
- The map widget is the home's (the page draws the world code's planet):
  on another body it's taken away and comes back on the home (`// TEMP:`
  until the page draws bodies).
- `check:bodies` (new, headless, ~70 s): a 250k home and its moon; land
  found on the moon; its sun, surface and air its own; a mark on each kept
  as a place; every tile of both pushed out of the window (the moon's all
  gone); both marks found again by their places, in other slots; the
  moon's blocks in `bodies/moon/map.sqlite`.

Proved: check:bodies, window, atlas-world, atlas-land, far pass; TestBodies,
TestPlanetAtlas, TestPlanetWindow, TestFar, TestAtlasView pass.

## Step 6 (2026-10-29)

- `/travel <body> [<latitude> <longitude>]` (privilege `teleport`;
  `tf_planet/travel.lua`, `tf_planet.travel`): land on the body at the
  place, or the nearest within 30° in rings a degree apart, or (no place)
  where a spawn there would be. The player is held (no speed, jump or
  gravity) behind the page's travel screen (`travel` ui value,
  `TravelScreen.vue`) while 32 nodes round the landing are made, put
  there, and let go 1.5 s later.
- Spawn levels by body: `core.planet.spawn_level(lat, lon[, body])`,
  `tf_planet.spawn_position([body])`; `MapgenTerra::spawnLevelAt(body, …)`
  (the home's TEMP gone).
- Fixed on the way: `spawnLevelAt` is asked from the main thread of the
  emerge thread's generator, which now builds a body's land when first
  needed: that build is under a lock (two threads could have built one).
- Fixed: the menu's Planetarium crashed its far view (`the atlas holds 1
  bodies, the world code's system 9`): it lays out the home alone, and the
  home's world code is the horizon's, no list needed (step 4 asked for the
  whole list for every body). TestFar covers a home-only atlas on a world
  with a moon.

Proved: check:bodies (now with the moon's landing: on the moon, by its own
spawn levels), window, atlas-world pass; TestFar passes. Not proved: a
client travelling (Cody).

Open:

- The screen drops on a timer (1.5 s after arriving), not when the client
  has the blocks round it: on a slow machine it may lift onto blocks still
  coming. The honest end is the client saying so.

## The map, on every body (2026-10-29)

- One key for a map's world: the world code and, in game, a body
  (`{ code, body }`). `worldgen.preview` and `planetarium.show` take it:
  the session finds the body's world from the client's atlas
  (`terragen::bodyWorlds`); the menu's are codes alone. The Planetarium
  draws any body from its tiles (`Opts::body`; "playable" in it is now the
  tiled body), the rest of the system round it. A key (`showWorld`) shows
  the body the player's on.
- tf_player: the map is of the body the player's on: its size, radius,
  name, and what they've explored of it (`tf_explored@<id>`; the home's
  keeps `tf_explored`); `map_position` names its body; `bodies` lists every
  body for the page; "Go there" goes to the body shown. The away TEMP is
  gone. `core.planet.bodies()` gives each body's `size`.
- The page: the minimap and flat map are keyed by world and body
  (`mapKey`). The map screen is the whole window: the globe over all of
  it, the bodies along the top (the player's marked), out to the whole
  system and back, Find me (back to the player's body), the flat map of the
  player's body with its overlays.

Proved: TestBodies `testAPreviewOfABody` (the moon's preview is the moon's
world, the same each time, not the home's); check:bodies passes; the page
typechecks. Not proved: the map in game (Cody).
