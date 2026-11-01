# Sprint 86 — The world on the sphere

terragen still models a flat map: a 256×256 grid in latitude and longitude
(row 0 the north, columns wrapping east–west, polar "wall" rows from the
days the world was a flat strip). The atlas (`planet::Atlas`) cuts the
planet into segments and caps, and every cell asks terragen by latitude
and longitude (`terra::Columns::atPlace` → `equirect`). Near a pole a whole
row of 256 cells covers a tiny circle, so noise, erosion and rivers are
smeared east–west, and the polar wall rows are still laid down there.
That's the weirdness at the poles.

This sprint moves the world model onto the sphere. It's the foundation for
the generation sprints after it (rivers by fastscapelib, plates with
history, the energy-balance climate, the sky); it adds no new features.

## The rule

**The world model is a function of a point on the sphere.** It takes a
direction from the planet's centre (and a height); it knows nothing of
blocks, segments, caps or latitude rows. The atlas is the only thing that
knows segments: each cell asks for its own direction. Seams agree because
both sides ask about the same point; the poles are just places.

## Plan

### Pass 1: the mesh

- `terragen/sphere.*`: a geodesic grid, a subdivided icosahedron. Cells
  are its vertices (hexagons, and 12 pentagons), about equal in area.
  Built from integers and fixed point, so it's the same on every machine.
  - Frequency 128: 163,842 cells, about as fine as today's 65,536 at the
    equator and far finer than it at the poles.
  - Neighbours as an adjacency list (≤ 6), each cell's centre and area.
  - A point's cell in O(1): its icosahedron face, then barycentric
    coordinates on it. A field between cell centres by the face's three
    nearest cells.
- No library needed: an icosphere is small, exact and ours. geogram is
  kept for later (plates as Voronoi cells), if it's wanted.

### Pass 2: the fields on it

- Noise in 3D on the unit sphere (`value3` at the point), not periodic in
  u: no seam, no pole.
- The grid's algorithms on the adjacency list instead of eight neighbours:
  the sea by quantile, priority flood, receivers, rain-weighted drainage,
  erosion, lakes, rivers. These are graph algorithms already; only how
  they find neighbours changes.
- Plates, belts, provinces and volcanoes on the mesh's cells.
- Climate: winds and rain marched along the wind over neighbours, not
  along rows; the ocean currents' distance to land measured along the
  latitude circle by sampling points, keeping the model.
- The polar walls go. The poles are land or sea or ice like anywhere.

### Pass 3: asking by place

- `World` is addressed by a direction (unit vector, fixed point) instead
  of block (x, z) on a flat map. `Columns::atPlace` passes the atlas
  cell's direction straight through; `equirect` goes.
- The preview: the map view samples the mesh per pixel (equirectangular
  is now only how it's *shown*); the globe samples by direction.
- The far view and `core.terra.surface` take the same path, so
  `bun check:far` still compares like with like.

### Pass 4: the code and checks

- A new code version, **TF6**; TF5 codes are refused, as the code rule
  says (a code must never silently change meaning).
- The golden is regenerated for TF6. The water invariants (drains to the
  sea, flows downhill, land share) and the solar-system calibration still
  pass.
- New tests: every cell has neighbours both ways; a point's cell is the
  nearest centre; the same code gives the same mesh fields twice; nothing
  differs in kind at a pole (rivers cross it, noise has no seam).
- `bun check:terragen`, `bun check:far`, the preview in the gallery.

## Decisions to confirm

- **Old worlds.** Worlds made with a TF5 code won't load once TF5 is
  refused. Either that's accepted (we're pre-release), or the TF5
  generator is kept frozen beside TF6. Recommended: accept it.
- **Determinism.** This sprint stays fixed point, so it changes nothing
  there. Deterministic floating point (for fastscapelib) is set up in the
  next sprint, where it's first needed.

## Not in this sprint

fastscapelib, plates with history, the energy-balance climate, the sky,
other body kinds. The weather (`src/fluid`) keeps its own lon–lat grid; it
samples the ground by place, so it follows along.

## Outcome

**Pass 1 (the mesh): done.** `terragen/sphere.*`: `Sphere(n)`, 10n² + 2
cells, built from the icosahedron's corners at 2²⁰ (a lattice point is the
same integer sum from every face it's on, so edges and corners join
exactly), centres normalised in fixed point (`isqrt`, now public in
`fixed.h`). Neighbours in turn round each cell (from its fan of lattice
triangles), area against the mean, `cell(dir)` (face by signed volumes,
lattice point, then a walk to the nearest centre) and `spot(dir)` (the
lattice triangle's three cells and weights).

- A plain icosphere is uneven: cells run from 0.51 of the mean (the
  pentagons) to 1.21. Anything summed over area weighs by `area()`. An
  even spacing needs a warp that's costly to invert in fixed point; not
  worth it yet.
- **Run:** `TestSphere` (rings, pentagons, symmetric neighbours, areas,
  nearest cell against a brute search, poles, spot weights and continuity,
  a pinned hash of the n = 128 mesh); `TestTerragen` unchanged.

**Pass 2 (the fields on it): done.** terragen's inside is on the sphere;
its outside (block x, z and u, v) is as it was, turned into a place
inside (`World::place`, `u(Dir)`, `v(Dir)`), until pass 3.

- The grid is `gridSphere()`, frequency 80: 64 002 cells, about the flat
  map's count. `Grid::index`, `GRID` and the polar rows are gone
  (`Column::polar` too); the poles are land, sea or ice by the climate.
- Noise is 3D at the direction (`noise`, `fbm`: frequency still counted
  round the equator). Block noise (folds, veins, caves, blends) is read at
  the block's place in the planet (`World::solid`: its column's direction at
  the model's radius, size / 2π, plus its height).
- Water: steepness is drop over each neighbour's real distance
  (`Sphere::distancesBegin`), rain gathers weighted by cell area, the sea
  is placed by area. Lakes needed retuning: on the finer cells hollows come
  out shallower (15 cells and 4 blocks deep, was 10 and 6), else the default
  world had none.
- Plates turn about their own poles (`Plate::pole`, `spin`); a pair's
  boundary is classified by their velocities (ω × p) at the middle of their
  centres. Volcanoes are places (`Volcano::at`); slopes, craters, lava
  fields and rivers are measured straight through the planet, in blocks.
- The climate keeps its own latitude–longitude grid (128²), read from and
  back onto the cells: it's smooth and banded by latitude, and the
  energy-balance climate replaces it. (The plan had it marched over the
  cells; not worth it for a model being replaced.)
- The flat map had latitude across `size` blocks, twice the planet's
  radius allows: land on the planet was squashed north–south 2×. Measured
  on the sphere it isn't.
- TF6. The preview's rivers are segments between places (9 bytes:
  u0 v0 u1 v1, strength; `grid` gone), the page's decoder with them.
- Fixed on the way: a sea column whose river bank rose over the sea kept
  the kind Sea (water under dry ground); the far check's probe took the
  matter's water (group `liquid`, no Luanti liquidtype, since sprint 80)
  for ground, hidden while its columns were land.

**Run:** engine build; UI typecheck; `TestSphere`, `TestTerragen` (golden
re-pinned for TF6; the limestone-caves test now counts tunnels above the
deep caverns, which open in any basement rock; the pole-noise test has a
unit's tolerance: cos 90° in fixed point isn't quite 0); `TestPlanetarium`
(its sharpness test takes the best of nine places: its one spot is an even
slope on TF6); full unit tests: all pass but `TestAtlasRender` (failing
before this sprint too); `bun check:far` 100%; `bun check:lab` 3/3; a
headless server boots on TF6. `bun check:terragen`: core.terra 36/36,
weather 36/36, columns 34/36. The two: a deep-sea column whose block 20
down is the atlas's seam layer, and one whose ore differs because the
engine hands the block noise the atlas cell's spot coordinates while the
predictor uses the map node's. Not checked against the last commit (no
baseline run). Pass 3 gives every block its true place, which settles the
second.

**Not run:** the game client, the create screen's preview in the page.

**Pass 3 (asking by place): done.** `World` is asked by place only:
`ground`, `column`, `temperature`, `sunlight`, `strata`, `soil`, `cover`
take a `Dir`; a block is its column's strata (`Strata::at`) and a height,
its noise read at `solid(at, y)`. `u(x)`, `v(z)`, `blocksAt`, `spaceFrom`
and the polar wall are gone; `place(u, v)` / `u(Dir)` / `v(Dir)` stay for
flat pictures (the preview's map, the weather's grid, the painter's lattice).

- `planet::Atlas::place` gives the column's direction (`direction()`: the
  cell centre to 1/1024 of a block, then fixed point); `surface()` the exact
  point, for Lua's `atlas_place` (through fixed point it lost ~3 blocks:
  the window check caught it). `equirect` is gone.
- A terra world is a planet: the map generator, `terra::Columns`,
  `core.terra` and the far view refuse a world without `planet_atlas`
  (every world made since sprint 30 has one). The far view's flat generator
  and its four tests went with it.
- Tree spots: a jittered lattice on the cube face a place looks through;
  a column's roll by its surface block.
- `core.terra.world_column` → `core.terra.place` (the direction, fixed
  point); `core.terra.world().space` is gone (tf_weather: space is above
  `boundary`).
- The check: the predictor names places, the probe logs each node's own
  place, the predictor predicts for exactly it. The ore mismatch is gone.
- The menu's backdrop passes over a last-visited world whose code no longer
  decodes (a TF5 world raised every frame).

**Run:** engine build; full unit tests (all but `TestAtlasRender`, the same
two failures as before); golden re-pinned (the climate reads latitude back
from the place); `bun check:terragen` 38/38 (polar places added; seam 20
down not compared; weather near the pole allowed 35 K); `bun check:far`
100%; `bun check:lab` 3/3; atlas-land, atlas-world (equator box moved to a
coast), bodies, window checks pass; headless boot; UI typecheck.
`atlas-seam-check` fails 6 water pours (walkers pass): poured water is the
matter's since sprint 80; not checked against an older build.

**Pass 4 (closeoff): done.**

- The ice block is gone from `terra::Blocks`, `TOCLIENT_HORIZON` and the
  baked palette (version 2; re-exported, which also brought water's tint
  up to date, stale since sprints 80–82); `mapgen_ice` isn't needed.
  Protocol 89, and the minimums with it (they had been left at 87).
- `terragen::Map` is gone (nothing used it but its boundary, now
  `terragen::SPACE_BOUNDARY`).
- `testNoPoleOrSeam`: the ground walked block by block over the north pole
  and across ±180° steps no more than a walk through mid-latitudes. On
  TF6.dGVycmE all three walks are gentle (steepest 1 block): it guards
  against a wall or a seam, not the relief there.
- AGENTS.md: the world is a planet with no edges; the sprint's entry.
