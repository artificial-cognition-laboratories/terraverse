# Sprint 42 — The planetarium draws right, at any size

## Decision

Sprint 41 made the system something to move round; this makes what's drawn
sound before the in-game sky and clouds are redone (they'll draw bodies
the same way). Seamless ascent moves to sprint 43. Sprint 41's step 4
(bodies in the game sky) is folded into step 5 here.

The planetarium and the game's far view are one LOD (`far::Tiles`): every
fix here is both's.

1. **Measured.** Holes are found headless (`TestFar testNoHolesFromAbove`),
   not from screenshots.
2. **No holes.** The seams between regions (segments, caps), and slits in
   the sea.
3. **The coarse levels average.** A coarse cell is its area's look, not
   its middle column's (the grain: one column a pixel, plants speckled).
4. **Smooth at any size.** Detail by size on screen, built nearest and
   most visible first within a budget a frame; the cost a frame doesn't
   grow with the planet.
5. **Every body the same LOD.** Seen far off, a body shows its land, low
   resolution; its coarsest level, baked, is the world builder's, the
   strip's icons and the game sky's.
6. **Light and lines.** The star lights the bodies (a day side, a soft
   terminator; no cast shadows). Orbits a few pixels wide.

Parked (2026-10-29, Cody): steps 4–6, for sprint 43's ground shape.

## Step 1–2, holes (2026-10-29)

Cody's account: the black lines are where the voxel grids wrapping the
sphere meet (the segments, like an orange's, and the cap at each pole),
which the game already handles and the LOD didn't; the black patches only
in the deep oceans.

- `TestFar testNoHolesFromAbove` (new): every tile of a level over the
  whole planet, meshed and put on it; 40,000 rays from above (half
  straight down, half slanting 40°), each must meet the mesh. Every block
  one grey, so faces merge wherever they can. Radius 1,304 at the roots
  and a level down, 250k at the roots. Before: 454 rays through holes at
  the roots, all on segments' edges.
- The seams: a coarse cell is no cell when its middle isn't, though up to
  half of it is, and the region across stops the same way: a gap up to a
  cell wide, widest on the coarsest tiles (a big planet seen from afar).
  A cell beside a seam now reaches half a cell into it, with a skirt hung
  from where it reaches (the region across may stand lower).
- The sea's slits: tops were merged into strips of up to 4 cells. On the
  planet a row bends, and a strip's straight edge left a slit beside the
  next row's shorter pieces. The sea floor under it is drawn darkened by
  depth, black in deep water: so slits showed only in the deep oceans
  (in shallows the floor through them is the sea's own colour). Strips
  are gone: every cell its own quad (up to 28% more triangles on an
  all-grey planet, less on a real one).
- `Tiling::place`: putting a tile's mesh on the planet is the tiling's
  (was private to the workers); the test uses it.

Proved: TestFar (9) passes; TestFarPalette, TestAtlasView, TestPlanetarium
pass; TestAtlasRender as at sprint 39 (5 holes, the cap's budget: the
near world's, not touched). Not proved: the slits by a failing test (the
grey test passed with strips straight down; its slanting rays came with
their removal); on screen (Cody).

Cody confirmed both on screen.

## Step 3, the grain (2026-10-29): tried, taken out

The tiles' tops drawn as pictures (ground and sea, no trees) at levels 0, 2
and 4 are smooth: the grain isn't the ground, it's the trees. A coarse
cell stands a crown where its middle column's roll falls under the
forest's density, so a forest from afar is one cell's tree, the next
one's ground.

Tried: a coarse cell's top mixed with its leaves by the forest's density,
crowns only where it's at least half closed. Not the look wanted (a
blurred wash), and the frame rate dropped (more crowns stood in dense
forest). Taken out: trees are as before. Kept: the two copies of the crown
rule are one (`Generator::plant`). Before that, 4 × 4 columns sampled per
coarse cell: each a world-model column (2.3 µs), a coarse tile 20 times
the cost (33–48 ms; level 0 is 2.3), for ground already smooth. Dropped.

Left for now (Cody). Seen on the way (terrain, not rendering): the ground
at level 0 has straight-edged rectangles where one biome's ground meets
another's, the world model's own output.

## The game's far view stops at the near world's range (2026-10-29)

Cody: the far view's trees stood among the near world's, round the
player, in game. The far view drew right up to the eye and hid itself by
a mask (the map-block columns the near world drew this frame, read by the
shader). With the mask shown in red (a debug setting, since removed) none
showed anywhere: the mask hid nothing. Why wasn't found; the mask is gone
instead, as Cody asked ("the far trees should only draw outside of the
normal render distance").

- The shader drops everything nearer the eye than the near world's range
  (`wanted_range`, the one the near world's draw list goes by).
- A tile all within it (its farthest point at the sea, and 256 blocks of
  relief) isn't drawn or built (`Tiles::inside`): no copy of the land
  under the player's feet.
- Gone with the mask: its texture and `updateMask`, `ClientMap::forEachDrawn`'s
  only use, the faces' layers in vertex alpha (solid, liquid, under a
  liquid), and the floor drawn under every sea for where the near world's
  water lay over it, `AtlasView::footprint` and `ClientMap::forEachDrawn`
  (the mask's alone).

Proved: TestFarPalette `testNothingInsideTheCut` (new: with a cut, no tile
all inside it is drawn and some reach across it; it fails with the skip
off); TestFar, TestFarPalette (6), TestAtlasView pass; check:far passes.
Not proved: in game (Cody). Expected there: where the near world hasn't
loaded a block inside its range yet, nothing is drawn (the sky shows) until
it comes.
