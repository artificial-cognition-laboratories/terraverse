# Sprint 85 — Ground detail at a distance

Cut and smoothed ground (sculpted builds, smoothed hills, sprint 84's ruts)
was meshed in sixteenths out to the view's edge: grainy far off, and most
of the triangles in the scene. Far off it's now drawn in coarser steps.

## Plan

- Each mesh is made at a detail chosen by its distance from the camera:
  sixteenths within `ground_detail_near` (12 nodes), quarters within
  `ground_detail_far` (32), whole-node steps past it.
- Coarser ground is drawn from the same shapes, coarsened: a cut node's
  shape filled per g³ where at least half of it is; a smoothed top's
  columns at their g² mean, to the nearest g. The mesh's quads merge runs
  of one height or fill, so ground in steps of g is far fewer quads.
- Moving a node, the client weighs every block's detail again and makes
  again (not urgent) those whose ring the camera crossed.

## Outcome

- `MeshMakeData::m_detail` (1, 4, 16: sixteenths a drawn step), set by
  `MeshUpdateQueue::detailAt` from the camera (`setCamera`) and the two
  settings; `MeshUpdateResult::detail` carries it back.
- `coarsen` (shape and `ground::Top`) in `content_mapblock.cpp`, used by
  `drawShape` and `drawGround` when detail > 1.
- `Client::meshDetail`: each block's detail as made (`m_detail`); the
  camera moved a node, those whose detail changed are made again.

**Run:** engine build; full unit tests: everything passes but
`TestAtlasRender` (fails the same with the committed mesher: not this
change) and `TestSandbox`'s `testBudgetPicksBusiest` (a timing budget in
the world's matter, not touched here); Lab check 3/3.

**Not run:** the game client: how far the rings should be, the look of
whole-node steps far off, and cracks where two details meet are unseen.

**Not done:** skirts where two details meet (a coarse block's edge can
miss its finer neighbour's by under a step); the near ring doesn't
follow the view range.

## Second pass: performance

Seen: FPS dipping to 26–38 moving over churned sand; Water look 6–15 ms
while moving, Water flat bursts, non-rendering spikes to 150 ms.

- **Ground changes batched** (`Client::flushGround`): a piles write and
  the server's shapes each queued, per node, every block its smoothing
  reaches (urgent), and flagged the water's ground, per node. Now nodes
  gather a frame and each block is made again once; the water, the
  body's ground and the mesh readers are told once per block.
- **Shapes the client already has** (it predicted them) aren't set or
  made again (`handleCommand_Shape`).
- **The prediction writes at 10 Hz**, not 20 (`Piles::PREDICT`).
- **Dry water tiles aren't rescanned** when their ground changes, only
  marked stale and rescanned when water wakes them (`Tile::stale`):
  churned sand rescanned every dry fine tile round it (Water look).
- **Ring crossings make six meshes a frame**, nearest first
  (`m_detail_pending`), not a shell at once.

**Not done: digging and placing.** The ramp round a dug hole (and the
sand rising round a placed block) is the ground's smoothing: a loose node
takes its shape from the columns round it. Freezing the neighbours as
they were before the change (and letting piles slump them) needs a
whole cut shape kept (`shape::Shapes` drops whole ones, so a full
neighbour is smoothed again): a decision for the next pass.

## Third pass: lag on the ground

Seen: lag mostly while standing or walking on sand.

- **The physics' ground is built again at most 4 times a second** while
  ground churns (`Client::flushBodyGround`). A block's ground is a convex
  hull per sloped node; each piles write rebuilt the blocks underfoot up
  to 10 times a second.
- **Feet aren't predicted on the client** (wheels still are): walking
  read a new region every couple of nodes. Footprints come from the
  server.
- Timers: `Physics: ground built [us]`, `Client: piles read/write [us]`,
  `Server: piles read/write [us]` (summed per profiler period).

## Fourth pass: steps, digging and building

Seen: a spike in `Time non-rendering` with each step; footprints laggy
with feet left to the server.

- **Ground hulls cached** (`physics::World` `hulls`): a block's ground is
  a convex hull per sloped node, built again each time the block's
  ground changed. Each node's hull (or that it has none) is kept by its
  slices, in 32nds from its centre: smoothed sand is a few shapes many
  times over, so a rebuild is mostly lookups.
- **Feet predicted again** on the client (the lag came back as laggy
  footprints; the step spikes look like the ground rebuild).
- **Digging and building** (`tf_world/ground.lua`,
  `core.matter.ground_changed`, `Piles::changed`): the loose ground round
  a dug or placed node is frozen as it stood just before
  (`Map::groundTopAs`: smoothed as if the old node were still there) as
  cut nodes kept exactly, whole too (`Map::keepShape`,
  `Shapes::keep`); a dug hole's floor is kept flat. Then its region is
  stirred round the change: sand slides in, soil (`pile_yield` 16) and
  clay (24) hold a node-high wall.
- **Protocol 88:** `TOCLIENT_SHAPE`'s shape is kept as sent (a whole one
  too); its flag is the node uncut.
- `TestPile::testHoleSlumps`: a node dug from a sand floor: frozen, sand
  slides in, everything sleeps; the count moves by under an eighth of a
  node, the smoothed sand just past the writes reshaping to its new
  neighbours (computed, never stored).

## Fifth pass: what the profiler showed

The server's page, walking: piles read 3–13 ms and write 2–7 ms a 3 s
period (cheap); the physics' ground built 120–420 ms a period (most of a
frame each second); on sand, `SendBlocks(): Collect list` 436 ms and a
208 ms step.

- **The ground's map read** (`workshop::groundBoxes`): whether a node and
  each of its six neighbours is plain and whole was asked of each, each
  time computing a loose node's smoothing (reading the columns round
  it): about 28 000 smoothings a block of sand or soil, each rebuild, on
  the client and the server. Each node is weighed once a rebuild now,
  and loose ground under something walkable (not cut) is whole without
  reading any columns.
- **The settle resend** marked the region's blocks unsent, which starts
  every client's sending scan over its whole view range. Now the server
  sends the shape of each loose node a region wrote (`Region::moved`);
  a client skips those it already has.
- **The prediction only reshapes** the ground's own nodes again (never
  adds or removes one), so the server's shapes always put it right.
- Timers: `Physics: ground read` and `Physics: ground built` (read
  included), summed per period.

## Sixth pass

Profiler (3 s pages, walking on sand): physics ground built 172 ms, of
which the map read 135 ms; client mesh making 161 ms (mesh thread);
piles 1–14 ms; 4.4 M vertices and ~1200 draw calls a frame.

- **Ground read at a block's edges:** the buried-ground shortcut only
  covered a block's inside; its six faces (about 1350 nodes) still
  computed each node's smoothing. Every node now asks `whole` first.
- **The old ramp for a frame:** a loose node added or removed on the
  client was remeshed at once, before its shape (sent after it)
  arrived: drawn smoothed in between. Now it's made again with its
  shape, once, at the frame's end (`Client::addNode`/`removeNode`,
  `m_ground_blocks`).
- **Trails on firm ground, and the jolt:** a foot presses ~18 kPa, and
  soil gave a sixteenth per 30, so each step sank a sixteenth (and the
  ground under the player was rebuilt, dropping them). Bearing is now
  soil 60, gravel 80, clay 50 kPa a sixteenth: wheels rut them, feet
  mark only sand and snow.

Next lever, seen here: 4.4 M vertices a frame (Draw scene).

## Seventh pass: vertices

Page 4 on the beach: `renderMap(SOLID)` 4.4–4.9 M vertices a frame in
~280 draw calls, about 16 000 a map block of mostly flat sand. Smoothed
ground (`drawGround`) drew its tops as runs along x only: a flat node 16
strips, a slope a quad per sixteenth both ways; risers and the walls at
a node's sides a quad per sixteenth. Coarsening far off didn't help,
still 16 rows. Now tops are the largest rectangles of one height
(greedy, as `drawShape`), risers runs of one step along their edge,
walls runs of one height. `testSmoothedGroundHasNoHoles` passes (rays
from above find no hole in rough meshed ground).

## Outcome

Ground far off is drawn coarser, and the ground's costs found by the
profiler are gone: on the beach the near world went from 4.4–4.9 M to
~420 k vertices a frame, and walking on sand is near a steady 60 fps
(user's profiler, after the seventh pass).

- Detail rings (`ground_detail_near` 12, `_far` 32): sixteenths, quarters,
  whole-node steps (`MeshMakeData::m_detail`, `coarsen`), remade a few a
  frame as the camera crosses them.
- Smoothed ground meshed greedily (tops as rectangles, risers and walls
  as runs): most of a block's vertices were its ground's strips.
- The physics' ground: each node weighed once, buried loose ground whole
  without reading columns, hulls cached by shape; rebuilt at most 4 Hz on
  the client.
- Ground changes batched a frame; shapes the client has skipped; dry
  water tiles stale till water comes; a loose node and its shape remade
  together (no flash of the old ramp).
- Digging and building freeze the loose ground round the change as it
  stood (`Map::keepShape`, a whole cut kept; `Map::groundTopAs`) and stir
  it: sand slumps in, soil and clay hold a wall (`tf_world/ground.lua`,
  `core.matter.ground_changed`). Protocol 88.
- Feet mark sand and snow only (soil, gravel, clay bear a foot).

**Run:** engine build; unit tests (all pass but `TestAtlasRender`, which
fails the same on the committed mesher, and a `TestSandbox` timing budget
under load); Lab check 3/3; headless boot. **Seen** by the user.

**Not done:** skirts where two details meet; the far view drawing the
whole planet past the horizon (next).
