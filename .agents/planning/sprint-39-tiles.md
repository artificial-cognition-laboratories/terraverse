# Sprint 39 — Tiles: the groundwork for a world size slider

## Decision

Planets from 50k radius to the Earth's (6.371M). The engine's map (16-bit
positions, 61,808 a side) stops being the world and becomes a window:

- **Tiles.** Every region's grid (24 segments, 2 caps, the core) is stored
  in tiles of 4,096 × 4,096 cells, each in a slot of the map (15 × 15
  slots, a map block of seam between). Crossing tiles is crossing a seam
  with no turn: the atlas's crossing (`step`, `see`, the rim) already
  goes through it.
- **The crust.** A tile keeps 30,000 layers under the sea and the sky
  above (the map's ±30,912). Below is seam: the planet's floor. The core
  is stored only where the crust reaches it (radius ≲ 40k); for larger
  planets it waits for a later sprint.
- **Paging.** Tiles get slots as they're needed, recorded with the world;
  the window full fails loud. Any radius lays out.
- **Sizes.** The world code's size table grows to the Earth; the creator's
  Start page gets a slider.

Old worlds don't matter (Cody: "dud worlds"): the code's size table and
defaults change freely.

## Measured (`--run-unittests --test-module TestScale`)

| Radius | World model build | A column | Land relief (40°N) | Atlas |
|---:|---:|---:|---:|---|
| 2,607 | 209 ms | 0.49 µs | −32..38 | fits |
| 10k | (grid: 256², size-free) | 0.47 µs | −33..37 | resident: no |
| 50k | | 0.48 µs | −33..38 | needs paging |
| 250k | | 0.45 µs | −33..38 | needs paging |
| 1M | | 0.45 µs | −32..36 | needs paging |
| 6.371M | | 0.43 µs | −32..36 | needs paging |

- The generator costs the same at every size: its model is a 256² grid
  over the whole planet, a column a lookup.
- **Its land doesn't grow with the planet.** Relief is ~70 blocks at every
  radius, and its features scale with the planet (the detail noise's
  wavelength is size / 64): a 1:1 Earth is today's world magnified, its
  hills hundreds of km wide and 30 m tall. Relief in metres and features
  in km are their own piece of work (the geology sprint), not storage.
- Resident (every tile at once): up to radius ~5,500 (204 of 225 slots;
  tiles at a grid's edges are partly empty). Before tiles: ~10k. Paging
  lifts it.

## Built (2026-10-28)

- `planet::Atlas`: tiles (`Tile`, `tiles()`; `TILE`, `SLOT`, `SLOTS`,
  `CRUST`). A node's tile by its slot (O(1), was a scan of every region);
  a cell's tile by its key (O(1)). Lua: `core.planet.atlas().tiles`
  (was `regions`; the probe scripts follow).
- `interior(block)`: a block is plain only inside its tile, the block and
  a step round it (a tile's edge is a seam).
- The rim across a tile's edge is the cell straight across (the grid
  goes on; the corner samples over a seam picked up its diagonal
  neighbours).
- The render oracle (`TestAtlasRender`) only calls a face a phantom when
  it isn't one: solid one side, air the other. A ray clipping a step's
  edge meets a true face where the march saw only air. All six phantoms
  (five from before this sprint) were that.
- Fixed: the engine read a world code at defaults as 8,192 round, the
  page as 16,384 (`DEFAULT_SIZE`); both 16,384 now.

Open:

- `TestAtlasRender`: 5 holes (4 before tiles). They are the view's own
  error at seams (a block drawn flat: its ground 0.14–0.35 of a node off
  at 4–18 nodes on the test's 1,304-radius planet), now at tile edges too.
  The error shrinks as 1 / radius. The budget at the cap (2.3 s) is
  sprint 34's, still open.

## Paging (2026-10-28)

The window is the atlas's: which tile each of the 225 slots holds, shared by
every copy of the atlas (the server's, the generator's) and changed in one
place, the server's pager. Clients follow it by packet.

- **The atlas's window** (`planet::Atlas`): `put(slot, tile)`, `window()`,
  `slotOf`, `windowCode()` / `setWindow()`, `generation()`. A change makes a
  new layout (each slot's tile, placed); the last 64 are kept, so what a
  mesh thread found a moment ago stays valid. What's found about blocks
  (rims, cuts, steps) is per layout, so a change finds it all again.
  `Atlas(params)` starts empty; `Atlas::resident(params)` holds every tile
  (tests, small planets).
- **A tile sits in its slot as in its square of the grid**: a map block is
  the same 16 cells wherever its tile is.
- **Spots.** A node's place on the planet whatever slot holds it: its
  region's grid at a fixed origin in a 32-bit plane. Generation (rock,
  caves, ores; the far view's look of a column) is seeded by spot, not by
  where the window put it; so is where a block is saved.
- **Saved by spot** (`MapDatabasePlanet`): the sqlite map table already
  keys by three integer columns; a planet's blocks go in by their spot in
  blocks (`MapDatabase::saveBlockAt`; other backends throw "not wired").
  Blocks of no tile (the seam between slots) are never kept: they're the
  seam the generator makes. Listing every block throws (not wired: they
  aren't all in the map).
- **The pager** (`planet::Pager`, no engine dependencies, its engine work
  as hooks): every second the tiles within 1,024 blocks of each player;
  `core.planet.atlas_node` the tiles round the cell it's asked for. Into an
  empty slot; else the tile needed longest ago, past 30 s, once its blocks
  are out of the map (saved first; a slot with blocks in use is skipped).
  All needed within 30 s: it throws. A new world small enough to fit
  (radius ≲ 5.5k) has every tile in from the start and never pages.
- **The server**: the window is saved to `<world>/planet_window` on every
  change and read at start (map_meta is read-only by then); a slot that
  changes unloads its blocks (`Map::unloadArea`), every client forgets
  what it was sent there (`ClientInterface::forgetArea`), and gets
  `TOCLIENT_WINDOW` (protocol 63; channel 2, before the new tile's blocks).
  The client drops its blocks in that slot and puts the tile.
- **Players** are kept where they are on the planet, not in the map: each
  second their cell is written to their meta (`planet_cell`); on joining,
  they're put at that cell, its tiles in the window, whatever slot.
- **Objects** saved in a block are moved with it: a loaded block's stored
  objects from another slot are shifted by the slots' difference.
- **Spawn** without pinning tiles: `core.planet.spawn_level(lat, lon)` asks
  the generator at a place; `tf_planet.spawn_position` only makes a node
  for the one it picks.
- **Sizes**: `SIZES` (`terragen/code.cpp`, `ui/src/worldgen/config.ts`) is
  radii 2.6k, 10k, 25k, 50k, 100k, 250k, 500k, 1M, 2M, 3.2M, 6.371M (the
  code stores the index; old codes are void). Start / Quick start has a
  "Planet size" slider with the radius, the Earth fraction and the walk
  round; the Land page's size control is gone. The world model's block
  coordinates are 32-bit now (`terra::equirect`, `Columns::onWorld`): they
  wrapped past 65,536 round.

Proved:

- `TestPlanetWindow` (new): the pager fills, takes out the oldest past its
  hold, skips a slot in use, throws when full, keeps the tiles round a
  player; a block saved in one slot loads in another once its tile moves,
  and seam isn't kept.
- `TestPlanetAtlas`: a tile moved between slots is the same cells at the
  same spots; window codes round-trip; a step off one tile lands on its
  neighbour in a far slot; tilesNear finds a corner's four.
- `check:window` (new, headless, ~70 s): a 250k-radius planet; the spawn
  found; a mark made there; 125 places elsewhere fill the window and the
  spawn's tile leaves; asked for again, the same cell, the mark kept.
- Every headless check passes but `check:atlas-dig` (one dig 0.95 s late;
  as at the last commit). The checks now make the default world
  consistently (16,384 round: radius 2,607.59; they were sized for 8,192).
  `check:far` picks its areas by place (`atlas_node`), so it reads the same
  on any storage, and skips columns that are no place (seam).
- Against a build of the last commit (a worktree): `check:atlas-land` and
  `check:far` passed there and pass here.

Fixed on the way:

- `far::Generator::column` (core.terra.surface) seeded a column by
  storage, the generator by spot.
- `core.planet.spawn_level` answers as `core.get_spawn_level` (one above
  the ground), and the debug sphere's level.
- `TestTerragen`: the code's size 1 is 62,832 round now; the golden world
  is that one (2496371879).

Open:

- Water: right by the equator in a segment's middle, a 96² area shows a
  lake 3 blocks above what the far view draws (1,050 columns). The
  generator's water there is node for node what `core.terra.land` says
  (`check:atlas-land`'s box at the same place), so it's most likely water
  flowing and renewing after generation; not proven.
- Positions mods keep (beds, homes, mod storage) are the map's: a tile
  moved to another slot leaves them pointing at the old one.
- Ores and decorations the engine places (`oremgr`, `decomgr`) are seeded
  by the map chunk's storage position: the same tile made in another slot
  differs there (saved blocks don't; only blocks made for the first time).
- Paging is proven headless; not with a client walking across tiles, or
  the planetarium drawing an Earth-sized planet.
- The land doesn't grow with the planet (see Measured): a big world is a
  flat one until relief is in metres.

## First play (2026-10-28)

- Booted on joining (Ashen Mesa, radius 1M): `tf_sky` asked `core.terra.sun`
  at layer 0 under the player. The spawn was on a segment's edge column,
  which narrows with depth (cells only above layer ~30 there): layer 0 was
  seam. It asks where the player stands now.
- "No land on the equator" (Frost Vale, 2M): that planet has none. The spawn
  searches out from the equator in 5° bands to 60°, on the half degree (off
  the equator's tile-edge row and the segments' 15° edges).
- A blank world (Drift Hollow, 2M): no error in the log. Its spawn was on
  the map's own edge (z −30,912, a tile's edge row in slot row 0); spawns
  now land inside a tile. Not reproduced, so not proven the cause.
- `check:window`'s far places now keep a quarter turn from the spawn (they
  covered its tile once it moved).
