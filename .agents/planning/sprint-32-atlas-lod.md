# Sprint 32 — The atlas LOD

## Decision

The far view and the Planetarium draw the planet as the atlas stores it.
Every block they show is the block the game generates, in the right cell of
the right region's grid, cut where the atlas cuts it. The LOD is a way of
seeing the atlas at less detail, never a different picture of the planet.

This replaces `far::Globe` (a cube-sphere quadtree whose cells sample the
land at their centres). It drew the right heights on the wrong grid: rows ran
along cube faces, the poles showed the cube's corners, and nothing about the
seams could be inspected.

The Planetarium is the tool for tuning the planet: seams and regions now,
geology, interior and weather later. So the bar is professional grade:
block-exact at full detail, from the surface to the core.

Not this sprint: 32-bit engine coordinates (the ±31,000 limit caps the
radius at about 17,000 blocks); the bridge and the menu (sprint 31,
phases 4–5); several bodies.

## Phases

Each phase ends playable and tested.

### 1. One answer to "what block is this cell"

The map generator works out an atlas node inline (`MapgenTerra::atlasNode`):
the column at the node's place, its block at the cell's layer. Pulled out
into one pure piece of the generator, `terra::AtlasLand`, which takes a cell
and gives a node. The map generator and the LOD both call it, so the LOD
can't disagree with the world.
- Gate: `check:atlas-land` emerges real areas (segments, a cap, across
  seams, down to the core) and compares every node with `AtlasLand`. It must
  be exact.

Done 2026-10-28: `terra::AtlasLand` (`mapgen/atlas_land.h`), with a
per-thread `Reader` that keeps the last column; `MapgenTerra` fills and lights
through it; `far::Generator::land()` owns one; `core.terra.land(minp, maxp)`
for the check. `check:atlas-land`: 633,792 nodes in six areas (a segment's
middle, its edge and seam, a cap, the cap circle, by the core, the core),
every one the same. Those areas had no trees, so planting isn't exercised
there. atlas-world, atlas-debug, far and terragen still pass.

### 2. Tiles are pieces of the atlas

Each region's grid (24 segments, 2 caps, the core) is an octree of tiles.
A tile is (region, level, i, j, k): 32³ cells, each 2^level atlas cells.
- Level 0 is the game's cells exactly, including which cut cells exist. A
  coarser cell is made from the finer ones by a fixed rule (solid if most
  are; its block that of the top exposed one).
- Meshes are the exposed faces, put on the planet by the region's own chart
  (`terragen::Atlas::at`), so rounding and trimmed seams show as they are.
- Normally only tiles holding the surface are wanted, so the cost is as now.
- `far::Tiles` chooses among them. The in-game far view and the Planetarium
  both draw them; `far::Globe` goes.
- Gate: a level-0 tile is `AtlasLand` cell for cell (test); Cody plays the
  far view and looks at the seams up close in the Planetarium.

Built 2026-10-28, surface first (a heightfield per region, the column
mesher as it was): `far::Tiling` (`far/tiling.h`) replaces `far::Globe`. Its
roots are the 24 segments and 2 caps, each a quadtree over its own grid; a
tile cell is the atlas cell at the surface layer (a coarse one's middle fine
cell), built as the generator builds it (the column at its place, noise at
its stored node); a cell the atlas has none of is space, with no cliff drawn
toward it. Points come from the region's own chart. Tiles whose square lies
outside their grid aren't chosen. Tests: the tiling is the atlas (roots,
cells, points at cell centres); a level-0 tile's ground and top block are
`AtlasLand`'s, cell for cell, and its gaps are exactly the atlas's missing
cells. Headless: 736 tiles, built in about 4 s from orbit. The octree in
depth comes with the cutaway (phase 3), which is what needs it.

### 3. Cutaway

A clip plane (and a wedge) through the body. Tiles it crosses are wanted
whatever their depth, and meshed with the faces on the cut, so any block of
the mantle or core can be zoomed into at full detail.

### 4. Geometry overlays

Colourings of the cells the atlas already describes: region; seam kind; cut
cells; trimmed cells and slivers; overlap (two grids' cells over one place).
A picked cell shows its address: region, i, j, k, retained share, and its
neighbours across the seam.

### 5. Performance

Tile build times, tile counts and GPU memory logged; at 1080p, 60 fps.
A loading indicator from `Tiles::pending()`.

## Outcome

(filled in at the end)
