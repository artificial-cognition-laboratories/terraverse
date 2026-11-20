# Sprint 43 — The ground has shape

## Decision

The world's small scale starts here (the large is the system, sprints
39–42). Its rule: a blueprint is free, making it real costs a process;
materials are loose (pour, settle, smooth: dirt, sand, gravel, snow, clay)
or solid (keep the shape they're cut to: wood, stone, metal). Every shape
is a block's 16³ of one material; the block stays the world's unit.

This sprint is the first half of that: undisturbed loose ground is smooth
to a sixteenth, and you walk over it without stepping. Nothing is stored,
nothing is editable. Seamless ascent moves to sprint 44; sprint 42's steps
4–6 are parked.

1. **The rule.** Natural loose ground is a height field: each column
   corner (where four columns meet) has a height, worked out from those
   columns' surfaces alone, to a sixteenth. A drop of more than one block
   between them is a cliff and stays one (the corner has a height for
   each side). A function of the corner only, so neighbours always agree:
   across map blocks and across the atlas's tile seams. One noun, used by
   the client's mesh and collision and by the server, from block data both
   have.
2. **Drawn.** The near world draws a loose surface as faces through its
   corners: lower than a block where the ground falls away, filling into
   the air above where it rises. Lit and textured as the block; plants on
   it stand on it.
3. **Walked on.** Feet meet the surface the mesh draws (the same corners):
   up and down a slope without a step or a jitter, the player and every
   moving thing.
4. **Proved.** The rule: flat stays flat, a step is a ramp, two blocks a
   cliff, corners agree across a block and a seam, the same input the
   same answer. Headless: a player walks over a generated hill and to the
   shore without jumping and never leaves the surface. Cody: walks it.

Loose is a property of what the block is (its `loose` group, set where
its matter is grains), not a list of names. Stone isn't smoothed this
sprint: it is solid.

Not this sprint: water where it meets a slope (it needs its own attention),
editing, stored shapes, loose amounts in the inventory, parts, the focus
editor, vehicles, the far view.

## Step 1, the rule (2026-10-29)

- `ground::Ground` (`src/ground/`): a loose block with nothing walkable
  over it gives its `Top`: four corner heights and, on each side, the
  neighbour's surface there (a wall from it up to ours where ours is
  higher), in sixteenths. Read through any `get(v3s16) → MapNode` (a
  voxel manipulator, a map); nothing stored.
- A corner's height is `ground::corner`: the columns there clustered
  (from the lowest, a new cluster where one is more than a block above
  the cluster's lowest), the column's cluster averaged to a sixteenth;
  never under the column's surface where one there is higher, or solid at
  its height (the face hidden against it stays hidden). Clusters span a
  block at most, so a top is only lowered within its cell, and only rises
  into the cell above.
- Columns are looked for 3 blocks up and down (`REACH`): a cluster chain
  of four spans 3, so nothing further can change a corner. A neighbour 2
  or more lower is given at its lowest (the wall reaches into its ground,
  not short of it).
- `loose = 1` on dirt, grass, dry grass, tundra, soil, sand, snow, gravel,
  placer gravel, clay. Stone, ice, ores, sandstone: solid.

Proved: TestGround (5): flat stays flat; a step is a ramp met halfway
from both sides; two blocks a cliff whose wall reaches the low ground;
dirt beside stone stays full there though its corner averages lower; on a
rough field of dirt and stone (2,000+ edges), every column's account of
each neighbour's edge is that neighbour's own, and none falls under its
surface beside a higher or solid column. With the pin off,
`testSolidKeepsItsShape` and `testNeighboursAgree` fail.

Not yet: the atlas's tile seams. The rule reads neighbours by map
position; across a seam the mesh's neighbour is the seam's (step 2 reads
them as the mesh does).

## Step 2, drawn (2026-10-29)

- The near world's mesher (`MapblockMeshGenerator::drawGround`): a
  smoothed block draws its top as two triangles through its corners (split
  0–3, as `Top::height`) and, each side, a wall from the neighbour's
  surface up to its own where its own is higher (a triangle where they
  cross); its bottom is the cube's. Textured and lit as the cube's faces
  (smooth lighting through the node's frame); the top's normal is its
  slope's. An atlas cut cell keeps its cube.
- It reads the world through the mesh's own voxel area, a map block of
  margin round it with the seam's halo written in: across a tile seam the
  ground reads the neighbours the mesh draws against.
- A plant (anything not walkable) on smoothed ground stands on its
  surface at its middle.
- A node's change remeshes every block the shape reads it from: two
  columns across, `REACH + 1` up and down (was: the leading edge).
- `ContentFeatures::loose`, from the group where a definition's
  registered, on the server (`set`) and on the client (`deSerialize`):
  one `deriveFromGroups` for it and `floats` (the client's copy of
  `floats` was its own line). Before it, the client read the group by
  name for every solid node in every mesh; and the first cut of it was
  `set`'s alone, which the client never calls: the client would never
  have smoothed.

Proved: TestMapblockMeshGenerator `testSmoothedGroundHasNoHoles` (new): a
block of rough dirt and stone, meshed; 1,000 rays straight down meet the
surface the rule gives, exactly; 1,000 slanting 30° reach the mesh
through no ground. With the walls not drawn it fails. TestGround
`testTheClientKnowsLoose` (new): definitions sent and read back keep
`loose`; it fails with the client's old line. TestGround (6),
TestMapblockMeshGenerator (5), TestAtlasView, TestCollision pass.
TestAtlasRender as before: 5 holes (its nodes aren't loose: untouched)
and `testWithinBudget` over on the choosing time, which isn't the mesh.

Not yet: walking on it (step 3): until then you stand on the cubes, so
above a lowered slope and in a raised one by up to a block. Pointing and
selection are the cube's.

## Voxels, and walked on (2026-10-29)

Cody, seeing step 2: always voxels, not slopes: sixteenth blocks along the
slope's line.

- `Top::cells`: 16 × 16 columns a sixteenth across, each the field at its
  middle to the nearest sixteenth. `Top::height` is the column under a
  point (what's drawn and stood on); `Top::field` the plane.
- Drawn: each row's tops in runs of one height; a riser where two columns
  side by side differ; at the block's sides a wall from where the
  neighbour stands no lower than (`Top::beside`: the line between its
  corners less `EDGE`, 2: its columns stand within a sixteenth and a half
  of it) up to each column. A riser's texture runs from its own top edge,
  as a cube side's from its top: each step shows the side's top (grass).
- Walked on (step 3): collision takes a smoothed block's `Top::boxes` (a
  box from its base to each run of columns) for its cube. Everything that
  moves by `collisionMoveSimple` stands on it, the local player and the
  server's entities alike; a sixteenth is far under a step (0.6).

Proved: TestGround `testWalksUpASteppedHill` (new): a player-sized mover
through the player's collision walks up a hill rising a whole block every
three, to the top, never inside the ground, feet on the columns under it;
stood on as cubes it stops at the first step. TestMapblockMeshGenerator
`testSmoothedGroundHasNoHoles` on the voxels (1,000 rays each way: the
voxel tops have more triangles to test), failing with the block's side
walls or with the risers between columns not drawn. TestGround (7),
TestMapblockMeshGenerator, TestCollision, TestAtlasView, TestFar,
TestFarPalette pass; check:atlas-world passes.

Not yet: across an atlas seam, collision reads a cell through the seam as
its cube (`add_area_node_boxes`' seam path), though it's drawn smoothed.
Pointing and the selection box are the cube's.

## Holes (2026-10-29)

Cody: smooth, walks up and down; rare holes, sky through them at a block's
corner, the soil's side showing.

Two ways the ground leaned against what's drawn as a full block (its face
against it hidden):

- A column not loaded yet read as nothing there: the ground beside it
  leaned down to it. And a block's arrival remeshed only its six face
  neighbours unless smooth lighting was on without performance tradeoffs,
  though the shape reads across edges and corners: the mesh that leaned
  could stay. Now a column not all known is a wall (nothing beside it
  lowered), and an arrival remeshes all 26.
- An atlas's cut cell keeps its cube, but the ground beside it took it for
  dirt and leaned with it. Now `Ground::top` takes which cells keep their
  cube (the mesh's cuts; collision asks the atlas): solid to the rule.

Proved: TestGround `testUnknownAndCubesAreWalls` (new: beside an unknown
column, and beside a cut cell, the corners stay up where they'd lean);
each half fails without its fix. TestGround (8), TestMapblockMeshGenerator,
TestCollision, TestAtlasView pass. Not proved: the holes Cody saw were
these (not reproduced headless); on screen (Cody).

## Open

- Digging or building next to smoothed ground: a block placed into air
  that holds raised ground takes its place (for now).
- The build: a one-file change took ~2 min (115 s), nearly all of it the
  link-time optimisation's relink. Off in `.output/build`'s cache
  (`ENABLE_LTO=OFF`): a one-file change is now ~4 s. The release gate
  (`scripts/release/gate.sh`) configures the same directory and doesn't
  set it, so releases are built without it too until it does.
