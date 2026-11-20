# Sprint 30 — The atlas in the engine

## Decision

The planet's blocks are laid out by `terragen::Atlas` (planet.paper.md):
bands of latitude rings, shells in depth, gnomonic polar caps, a Cartesian
core. This replaces the cube-sphere net (`planet::Planet`, `terragen::Sphere`)
and the sprint 29 transition-band plan. Old worlds stay on the cube-sphere
mapping until an explicit migration. New worlds record `atlas-v1` and the
atlas parameters in their map metadata.

The lab (`bun lab:atlas`) showed what we needed to know: blocks stay within
tolerance from the surface to the core, and every irregularity sits on a
seam that follows the planet's own frame. The rest has to be found in the
engine.

## The model we keep

The engine already works this way: a planet is a set of grids that are
plain Luanti inside. Only crossing between them is special, and that
crossing has one owner. Today the grids are six cube faces with
`step` / `halo` / `carry` / `turn` at their edges. Under the atlas:

- **Inside a band of rings with the same count, the integer neighbour is
  the true neighbour**: east is the next column, north is the same column
  in the next row, up is the next layer. Only these steps are special:
  - **ring wrap**: east of the last column is column 0;
  - **T-junction**: a row or layer whose ring has a different count;
  - **shell floor** and **cap edge** (cut cells);
  - **core boundary**.
- **Objects live in grid coordinates** (region, fractional i/j/k), as they
  do on a face now. Crossing a seam carries them through the atlas: grid →
  planet point → `locate` → the new grid.
- **The world is drawn in true 3D.** Mesh vertices come from the atlas
  geometry, relative to a per-mapblock anchor computed in doubles. That
  retires the sphere lens and its float-precision tricks, which sprint 28
  spent a long time on (the egg-shaped orbit, the corner skew).

## Phases

Each phase ends playable and tested. Nothing in a later phase is started
before the earlier phase's gate passes.

### 1. Lock the geometry (no engine change)

- `TestAtlas` in the engine unit tests, with the lab's measures as
  invariants (below). Golden values for one small and one Kerbin-sized
  planet, so any change to the mapping is a visible diff.
- Choose the layout: latitude or cylinder bands. Cylinder halves the worst
  east–west stretch at depth. Latitude has simpler storage (a band is a
  rectangle through the whole shell). **Decision needed** (see below).
- Version the mapping: `atlas-v1` plus the parameters, serialised and
  read back exactly.

Gate: `--test-module TestAtlas` passes; `bun check:atlas` (the lab CLI's
survey for the supported sizes) matches the goldens.

**Done 2026-10-27.** `bun check:atlas` builds the engine, runs `TestAtlas`
(12 cases: goldens, mapping code round trip and rejection, bad params,
band widths, shell heights, band and cap shape, locate/centre, cells hold
their points, neighbour symmetry with junctions and shell floors actually
crossed, ring closure, rail jogs), then compares the lab sweep for 7 sizes
× 2 layouts against `scripts/atlas-check/golden.json`. Writing it found and
fixed:

- **Cylinder inner bound.** Cylinders reached slightly inside their
  tolerance (the wrong one of two bounds was taken). The goldens are
  recorded after the fix.
- **Angle measurement.** `acos` floored gravity at 1.5e-6°. It's now
  `atan2(|a×b|, a·b)`, and gravity reads 1e-11° (moon) to 4e-9° (Kerbin).
- **Band skew isn't zero; it scales as 1/Rs².** A ring's block is a hair
  trapezoidal on the curve: 1e-5° at 3 km, 0.015° in a 50-block shell by
  the core, 1e-8° at 600 km. The invariant is now `max(1e-6°, 200/Rs²)`.
- **Exact neighbours.** Band-to-band neighbours (ring, row, layer, shell
  floor) are computed in integers, so the thinnest T-junction overlap
  counts. Sampling remains only for cap and core crossings.

Layout decided: **cylinder** (the mapping code records it; latitude stays
supported and tested).

### 2. Storage: the atlas in Luanti's map

`planet::Atlas` (the successor to `planet::Planet`) lays each shell into
the engine's 16-bit map:

- A shell is one rectangle: **x = column** (0 to the shell's largest ring),
  **z = row**, **y = layer**. A ring with fewer columns leaves the rest of
  its row void. That void is never generated, so it costs nothing.
- Caps are squares beside it. A map block of seam nodes separates regions,
  as the net's `GAP` does today.
- `node ↔ cell` is a bijection, tested exhaustively on a small planet.

This fits planets up to about **9.8 km radius** (the equator ring has to
fit in 61,808 nodes). The lab measures 98% regular blocks at 10 km, so this
is a real planet to prove everything on. Bigger planets wait for phase 6.

Gate: a headless world on a 9 km atlas planet generates. Every generated
node maps to a cell and back. `terra_column` reads terragen by the cell's
latitude and longitude directly; there's no equirectangular detour.

**Done 2026-10-27.**

- **`planet::Atlas`** (`src/planet/atlas.*`, no engine types) lays out the
  regions: one band rectangle shared by every shell (shells sit at their
  own layers), cap squares beside it, and the core square only when depth
  reaches it. Any planet that doesn't fit is refused, not clipped.
- **A world opts in** with `planet_atlas = <mapping code>` in
  `map_meta.txt` (`MapgenParams::planet_atlas`). The server refuses a world
  with both `planet_half` and `planet_atlas`, an unreadable code, a code
  changed while mods load, or regions outside the generator's edges.
- **Generation.** `MapgenTerra` writes seam (`mapgen_seam`, which
  `tf_planet` aliases) where no cell is stored, and stone in the core.
  Everything else comes from `terra::Columns`, which reads terragen at the
  storage column's latitude and longitude via `planet::Atlas::place`.
- **Lua.** `core.planet.atlas()` and `core.planet.atlas_cell(pos)`. Atlas
  worlds spawn on equatorial land.
- **Tests.** `TestPlanetAtlas` (5 cases): fits and refusals (largest
  world, 9 km, to the core, 10 km refused), regions apart, an exhaustive
  node ↔ cell pairing over a small planet's whole footprint at every kind
  of layer, every locatable cell stored, and a column's place matching its
  cell. `bun check:atlas-world` generates the default world headless at
  1:1 (radius 1,304) and checks all 119,814 nodes in five seam-heavy boxes:
  0 failures, real ground and air at the equator, land found to spawn on.

Corrections to the plan:

- **Not 9 km.** Terragen's world models are at most 16,384 blocks round,
  and today's planets map them 1:1, so a 1:1 atlas planet has a radius of
  at most 2,608. The check uses 1,304 (the default world). `TestPlanetAtlas`
  proves storage up to 9 km. Bigger planets at full detail need a
  finer world model: that's terragen's question, not the atlas's.
- **Terrain by storage column.** A storage column is generated from its
  surface cell's place. Below a cylinder seam the ring count can differ, so
  terrain there is off by up to half a block. Phase 3 (stepping by cell)
  removes it.
- **The menu still makes cube worlds.** An atlas world can't be drawn as a
  planet until phase 4, so only checks make one for now.

Regression: TestAtlas, TestPlanetAtlas, TestPlanet, TestPlanetMap,
TestPlanetMesh, TestFar, TestTerragen and TestSphere pass; `check:atlas`,
`check:planet` and `check:terragen` pass.

### 3. Neighbours: step, light, liquids, collision

Generalise the engine's one seam owner (`Map::see` / `step`, and
`step_on_planet` in `voxelalgorithms.cpp`, `collision.cpp`) from "one
neighbour, maybe turned" to the atlas's answer:

- **Ring wrap** and **storage cuts**: one neighbour, no turn.
- **T-junction**: one to three neighbours. Light and liquids go to all of
  them, weighted by shared face area. Collision tests against each.
- **Shell floor**: the same, vertically.
- **Cap edge**: cut cells are solid or empty like any other cell. Their
  shape is the full cube for now (exact cut geometry is phase 7).
- **Meshing v1**: never cull a face across a seam. Solid-to-solid seam
  faces are drawn but stay buried. That's correct, and costs a little only
  along seams.

Gate: a port of `bun check:planet`, with the walker, water and sand crossing
every seam kind: wrap, T-junction (north and south), shell floor (up and
down), cap edge at longitudes 0° and 45°, and into and out of the core.

**Gate passed 2026-10-27:** `bun check:atlas-seams`, 441 checks, 0 fails.
Walkers cross the ring wrap, a band seam going north and one going south,
and into the cap at 0° and at 45°. They stay on the floor, never sit in
seam, never jump on the planet, and end up past the seam. Water poured
before each of those seams flows past it. Sand dropped through a shell's
floor lands in the shell below.

**Storage v2 (a phase 2 flaw, fixed first).** Phase 2 put every shell in one
rectangle with x = column. Across a T-junction a plain `+z` step then
reached the same column index of a ring with another count: a real cell,
up to ~1% of a ring away. That silently broke the net's rule that code
stepping without `Map::next` meets seam, never a wrong block. Now every
uniform grid has its own region, walled by a map block of seam:

- latitude band × shell, or cylinder × shell, split north and south unless
  it spans the equator;
- each cap per shell;
- shells stacked in y a gap apart, the surface shell keeping y = layer;
- regions packed in shelves.

`TestPlanetAtlas::testPlainStepsNeverLie` holds the rule, and it found a
real bug on its first run: sampled cap neighbours missed a sliver's own
grid.

**Crossing** (`planet::Atlas`). Each cell's chart is its grid continued
(band: longitude, latitude, radius; cap: its plane; core: x, y, z).

- `step(from, axis, sign)`: the cell straight through the face's middle,
  for light and liquids. Cut cap and core cells take it even when a sliver
  of a plain neighbour exists, so adjacency is symmetric: a liquid flows
  into a cap at 45°.
- `see(from, p)` / `seeAt`: the node at p in from's grid carried on, and
  where its centre really is. Collision places boxes there, so a seam's
  geometry is exact, not snapped. The true centre is the cell's own chart
  origin: the corners' average sits ~1e-4 inside a curved cell, which made a
  0.18 mm ledge that stopped walkers.
- `carry(was, now)`: a mover that left its grid, re-placed in the one it's
  in, with its motion turned.
- `point`: where a storage position is on the planet.
- `moverNode`: a mover is in the node 1 cm above its feet. Collision and
  carry both use it; mixing it with the engine's own rounding ping-ponged
  walkers between the grids above and below a slanted cylinder seam.

`Map::next`, `Map::see` and `Map::seeAll` route through these. Collision
gathers every cell over a seam (`seeAll`), each at its true offset, and
reports the node really hit. Light steps through `Map::next` wherever a
seam is (not only at map block edges). Entities carry. The turn convention
matches `planet::Planet` and is pinned to real geometry by
`testTurnsFollowTheGround` (it caught a sign flip).

Fixed in the library on the way:

- **Cell existence is exact.** A cap or core cell exists iff its nearest
  point is in the territory, so a sliver no sample hits is still stored.
  `retained` floors at 1/128. Goldens: only `cap.minRetained` moved, 0 →
  1/128, as intended.
- **Generation by node.** The generator builds each node from its own
  place, because a storage column holds several shells. That also removes
  phase 2's half-block terrain offset under cylinder seams.

New Lua: `core.planet.atlas()` (regions), `atlas_cell`, `atlas_place`,
`atlas_node(lat, lon, layer)`, `atlas_point(pos)`.

Tests: `TestPlanetAtlas`, 10 cases, about 27 s:

- fits and refusals;
- regions apart;
- nodes and cells pair;
- plain steps never lie;
- every cell stored;
- place is the cell;
- steps cross to true neighbours;
- seeing carries the grid (the named point lies in the cell seen);
- carry keeps the point (within 2 × `UP`);
- turns follow the ground.

**Open after phase 3** (none blocks phase 4; the first is the next to fix):

1. **Sunlight across shells is wrong.** Luanti lights straight down storage
   columns, and below a shell's floor the storage is a gap and then another
   shell, which is somewhere else on the planet. Light *spreading* crosses
   seams correctly. Sunlight needs its own rule: a cell is sunlit if what's
   physically above it, across shells, lets light through.
2. **The active area round players** (`serverenvironment.cpp`) still wraps
   by the cube's faces. Across an atlas seam, the far side's blocks aren't
   activated for a player near it. The headless check force-loads instead.
3. **Pointing and raycasts** use `Map::see`'s node, not its offset: off by
   up to half a block at seams (phase 5).
4. **Decorations** (trees) are clipped by seam at a region's edge or a
   shell floor (the generator builds a chunk in storage).
5. **The client knows no atlas**: no prediction across seams, flat
   drawing. That's phase 4.
6. `TestPlanetAtlas` is slow (~27 s), mostly sampled cap and core
   neighbours.

### 4. Rendering in true 3D

- Mapblock meshes are built from `atlas.corners`, relative to the block's
  anchor in doubles. The anchor's offset from the camera is applied in
  doubles each frame (the engine's camera offset, generalised).
- The camera's up is radial. It already is.
- The far view (`far/`) samples terrain by latitude and longitude and draws
  the planet's true shape. The cube-face code paths go.

Gate: sprint 28's orbital roundness oracle (silhouette ratio < 1.02)
passes without a lens. The user captures seam fixtures in-client.

**Built 2026-10-27, awaiting the in-client look.** The eye, the player and
what's round them stay in the eye's grid. Every map block is drawn with
the affine map from its region's grid into the eye's carried on
(`planet::Atlas::frame` / `view`). A block's frame is computed once; per
frame only the eye's is.

- **Exact for its contents.** The gaps between regions are a block wide,
  so a block holds one region's cells at most.
- **Off by about 16²/8R at its corners:** 0.02 nodes on the default world.

Pieces:

- `client/atlas_view.*` (per-block matrices).
- `ClientMap`: culls and draws by the transformed place; atlas blocks
  aren't merged; the shadow pass does the same.
- `TOCLIENT_ATLAS` (0x69) sends the mapping on join. The client forces
  `client_mesh_chunk` 1, since merged meshes could span regions.
- Block sending walks the eye's grid carried over seams (`seeAt`).
- The local player carries (the view turns by the exact angle; the turn it
  hands the camera is now in degrees, cube included).
- The movement anticheat measures on the planet.
- Spawn goes through the atlas (`tf_planet.spawn_position`, checked by
  `check:atlas-world`).
- New worlds are atlas planets with `planet_atlas_worlds = true`
  (`TEMP`, default off).

Also fixed:

- **The south cap's grid was mirrored** (its j ran along +z with up −y).
  It's now along −z; `TestAtlas::testGridsTurnAlike` holds every grid
  right-handed.
- **`step` looked a whole block past a face.** Where the far grid is
  turned, that cell needn't touch the face. It now looks just past the
  face's middle.
- **The chart lives once**, in `terragen::Atlas::at`. Corners and sampled
  neighbours use the true curved faces.

`TestAtlasView` (3 cases) holds the matrices: own blocks drawn in place,
over a seam a step on, a turned grid drawn turned. A transposed matrix
fails all three.

Not yet for atlas worlds (all deliberate, all next):

- The far view (no horizon packet: `TEMP`).
- Entities in another region are drawn at their storage positions.
- Sunlight across shells.
- The sky and sun are the flat world's.
- The active area round players.
- Pointing across seams.
- Above the top layer (256) is seam: an invisible ceiling.

### 5. Play at the seams

- Pointing, placing and digging go through `locate` (ray against cell
  geometry), never integer stepping.
- The east–west rail curves gently; the north–south rail jogs at T-junctions
  and turns entering the cap. The probe asserts the lab's numbers.
- Entities and players carry across every seam with bounded position
  change. That's the sprint 28 matrix, re-pointed at atlas seams.

### 6. Scale: Kerbin and up

Bigger planets need more than 16-bit nodes. Options, to be decided with
evidence from phases 2–5:

- **32-bit node positions**: mapblock keys, database encoding, network
  protocol. Mechanical, but wide.
- **Regions as separate maps**: each band or segment keyed by region id
  and local 16-bit position.

### 7. Exact seam geometry

Polyhedral cut cells at cap edges and the core, sliver merging, partial
faces at T-junctions (cull exactly), and relaxing jogs over several rows.

## Invariants under test (`TestAtlas`)

| Invariant | How |
|---|---|
| Every band block's width is within `1 ± tolerance/2` at its ring centre | every band, every shell, analytically |
| Height and north–south length within `1 ± radialTolerance/2` | every shell |
| Band skew < 1e-6°, gravity 0 | sampled cells, from corners |
| Cap shrink ≤ `1 − cos²θcap`, skew ≤ 0.25° | cap edge cells |
| `locate(centre(c)) == c` | random cells, all regions |
| Watertight: any point locates to exactly one cell | random points against the lab's slice |
| Neighbours are symmetric: B across A's face f ⇔ A across B's opposite face | random cells, every seam kind |
| Ring closure: east from the last column is column 0 | every band |
| Rail: north–south jog ≤ 0.5 block | every band boundary |
| Mapping version round-trips | serialise, read back, compare |

## Decisions needed before phase 2

1. **Layout.** Cylinder bands look better under the cut. Their cost: a
   band isn't a fixed rectangle through a shell, so storage relies on the
   ragged-row rectangle above (which handles both layouts the same way).
   Recommendation: **cylinder**.
2. **Default tolerances.** Band 1%, shell 1%, cap at 85°. These still need
   judging by eye in-game.
3. **Polar crowding.** Band height near the pole falls as `1/tan φ`. That's
   geometry, not a choice. The cap can't simply grow: its own shrink is
   `1 − cos²θ`, so a 1% budget allows about 5.7°. Options: accept dense
   seams from 80° to 84°; let tolerance loosen toward the pole; or ring
   the cap with a few gnomonic patches. Can be decided in phase 5.
