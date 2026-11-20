# Sprint 44 — Cutting a block

## Decision

The second half of sprint 43's rule: every shape is a block's 16³ of one
material, and now a player makes one. A block can be cut to the sixteenth:
the shape is stored with it, sent, drawn, stood on, and pointed at; in
creative, a chisel takes a sixteenth off or puts one back. Seamless ascent
moves again (sprint 46); bulk tools with shapes are sprint 45; blueprints
after.

1. **Stored.** A node may carry a shape: which of its 16³ sixteenths its
   material fills. Kept by its map block (sparse: only cut nodes), saved
   and sent with it (map format 30), dropped when the node becomes
   something else. A full shape is no shape; an empty one is air.
2. **Drawn and stood on.** The near world draws a cut node's sixteenths
   (faces only where open), its neighbours draw their faces toward it;
   collision stands on its sixteenths.
3. **Pointed at.** The pointing ray hits the sixteenths (and smoothed
   ground's columns), not the cube: it knows which sixteenth and which face.
4. **Cut.** A chisel in creative: a click takes the pointed sixteenth off,
   the other puts one back on the face pointed at. The server decides
   (reach, the node's still there) and tells everyone who has the block.
5. **Focused.** A key brings the camera round one block, the world dimmed,
   its grid shown, the chisel's tools round the screen's edge (the shell).
6. **Proved.** A shape survives disk and the network; a cut node's mesh has
   no holes and hides nothing it shouldn't; you stand on a half-cut block;
   the ray names the sixteenth it hits; a cut made on the server reaches a
   client's mesh. Cody: cuts a block.

Not this sprint: survival tools and what materials do when cut, loose
material in shapes (a cut loose block keeps its shape for now), bulk
shapes, blueprints, objects off the grid.

## Steps 1–4, stored, drawn, pointed at, cut (2026-10-29)

- `shape::Shape` (`src/shape/`): a node's 16³ as bits; `boxes` (runs along
  x merged along z), 512 bytes on the wire. `shape::Shapes`: a map block's
  cut nodes (`MapBlock::m_shapes`), saved and sent after node metadata in
  map format 30 (format 29 blocks have none). Protocol 65.
- A shape is its material's: `Map::setNode` and `MapBlock::copyFrom` drop
  it where the content changes. `Map::setShape` keeps one (a whole one is
  none; an empty one is refused: that's the node removed) and raises
  `MEET_SHAPE`, so the server sends it in order with the node's own
  changes (`TOCLIENT_SHAPE` after `TOCLIENT_ADDNODE`).
- Drawn: `MapblockMeshGenerator::drawShape`, each open face of a sixteenth
  merged into rectangles a plane at a time, textured as the cube's faces;
  a whole neighbour draws its face toward a cut node. The shapes reach the
  mesh thread copied on the main thread (`QueuedMeshUpdate::shapes`).
  Smoothed ground treats a cut node as a cube, and draws its sides down to
  its base against one.
- `Map::getShapedBoxes`: a cut node's sixteenths or smoothed ground's
  columns: collision stands on them, the pointing ray hits them
  (`Map::getPointedBoxes`). Pointing at shaped ground now hits its
  columns, not the cube; the outline is round what's there.
- The chisel (X, creative): the selection is the sixteenth pointed at; dig
  takes it off, place puts one on the face (into the open: of the pointed
  node's material). `TOSERVER_CUT`; the server checks interact, creative,
  reach (12) and that it's a block, and logs what it refuses.

Proved: TestShape (4): boxes are exactly the shape; a shape survives disk
and the network, none from format 29; it goes with its material (set,
voxel manipulator), a whole one isn't kept; the ray's sixteenth and the
one across a side. TestMapblockMeshGenerator `testCutNodeIsItsSurface`: a
cut node's mesh is exactly its open faces' area, alone and against a cube,
and a cube draws its whole face toward it. Each fails with its piece
removed (the neighbour's face, the cube hiding a side, the format).
TestMapBlock, TestGround, TestCollision, TestAtlasView pass.

Not yet: the focus view (step 5). Cutting smoothed ground starts from its
cube (it jumps to a block, less the sixteenth). Over an atlas seam, the
ray and collision still read cells as cubes. No hold-to-repeat.

## Cut from the slope (2026-10-29)

Cody: chiselling sloped ground restored the full block first.

- `Map::settleGround`: smoothed ground cut the first time becomes cut nodes
  of the shape it had (`Top::shape`): its own cell, and what rose into the
  cell above (a node of its material there). The server settles the node
  cut and the one under it (the chisel may be in the risen part, which
  the one below draws), then cuts.
- `shape::hit` gives the sixteenth in whichever node it lies (smoothed
  ground's boxes rise into the cell above).
- How the rule reads a cut node (`ground::seen`): its material where at
  least half is there, else open; ground under a cut node isn't smoothed.
  So a settle leaves its neighbours' shape as it was.

Proved: TestGround `testFirstCutKeepsTheShape` (new): along a slope, each
node settled stands the same, nothing it had is left to start from a cube,
and the ground two round is unchanged. Fails with the settle from the cube,
and with cut nodes read as their material whole.
