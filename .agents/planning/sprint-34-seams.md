# Sprint 34 — Seams you can build across

## Decision

Where two grids meet there is **one true cut**. The planet's regions
partition it exactly (watertight, sprint 30's invariant), so every cell's
solid is its grid's box *cut by its region's territory*. The union of those
solids is the planet with no gap and no overlap, by construction. Drawing,
collision, pointing and placing all use that one solid. Nothing is
stretched or blended: a cut cell keeps its grid's texture, just trimmed.

Today a seam cell is drawn, collided and pointed at as its *whole* box
(meshing never culls across a seam), so neighbours across the seam overlap
and the nearer one steals the pointer. Where boxes don't reach the
boundary, sky shows through (the dotted black lines). Both are the same
bug: the cut isn't applied.

What Cody saw (2026-10-28):

- East–west seams (sand, around (-22090, 2, -30717)): blocks overlap; the
  pointer picks the left block when looking at the right one; a hairline
  gap; building works but fights you.
- Cap seams at the pole (snow, around (-2910, 3836)): the harshest. A
  dotted gap along every seam, and T-junctions.
- Walking across a gap was blocked, then the server crashed (`tf_sky`:
  "the position is seam").
- The far view and the near world don't meet: holes where they join near
  the horizon, and they move at different speeds as the eye moves
  (parallax: the two are placed from different origins or radii).

## Phases

### 0. The two crashes

- `tf_sky` asks for the sun at the player's raw position. A mover's cell is
  the one 1 cm above its feet (`moverNode`), and a position can sit in seam
  storage for a step before `carry`. Every "where is this player" question
  goes through one Lua call that applies the mover rule, so seam is never
  asked about. If the mover's own cell is seam, that is the real bug, and it
  stays an error.
- `planetarium.hide` arrived with no handler (the page's unmount ran when no
  Session existed). The page should only ask a host that exists: the shell
  says which hosts are up, or the verbs are registered for the window's
  life. Not a silent no-op.

### 1. The seam oracle (measure first)

A headless check, `check:seams`, run over every seam kind (ring wrap, band
to band east–west, T-junctions north and south, band to cap, the pole, a
shell floor) and Cody's two places as fixtures:

- **Coverage.** Sample points across a slab either side of the seam: each
  must lie in exactly one drawn solid (no gap, no overlap). Reported as gap
  and overlap volume per metre of seam.
- **Pointing.** Rays at known cells from both sides must hit that cell and
  that face.
- **Far meets near.** For a cell at the near world's edge, its far-tile
  vertex and its mesh vertex in scene space, from several eyes moving
  across the planet: equal within 1e-3 nodes. That separates the parallax
  bug from the holes.

It fails today, and that's the baseline. Every later phase moves a number
to 0.

### 2. The cut solid

- `planet::Atlas::solid(cell)`: the cell's box clipped by its territory's
  boundary surfaces, as a small convex polyhedron in its grid (planes
  locally; the boundaries are cones, cylinders and planes, flat to 1e-4 over
  a block). It is empty for nothing stored, and a whole box away from seams
  (the fast path: nearly every cell).
- **Meshing.** A seam cell's faces are clipped to its solid, plus its cut
  face. The cut face is drawn unless what's across it is opaque everywhere
  it overlaps (`seeAll`). Two opaque cells meet on one shared surface, so
  nothing is drawn there and nothing z-fights.
- **Slivers.** A cut cell smaller than about an eighth of a block can't be
  pointed at usefully. It's merged into the neighbour across (the neighbour
  is drawn and collided reaching the boundary). This is the only place
  geometry extends, and it is local.

### 3. Collision and pointing on the solid

- Collision sweeps the mover's box against each nearby cell's solid at its
  true offset (`seeAll` already places them). Box against convex polyhedron,
  separating axes. Whole boxes keep the engine's AABB path.
- Pointing (`locate`, phase 5 of sprint 30): the ray clipped against
  solids, giving the cell really hit and its face. The selection box is the
  solid's outline.
- **Placing.** The new block goes in the cell just past the hit face:
  `locate(hit + ε·normal)`. Within a grid that's the usual neighbour. On a
  cut face it's the cell across the seam, in the other grid, turned the way
  that grid is. Digging takes the cell hit.
- Server and client agree: the client predicts with the same solid (it
  already has the atlas), and the server checks the place is a real cell
  next to the pointed one.

### 4. The far view meets the near world

- Whatever phase 1 finds causes the parallax (the far tiles placed from the
  eye's frame vs the blocks from their charts, or the sea radius vs the
  chart's): one placement for both.
- The far view's cut follows what the near world has actually drawn (the
  loaded blocks), not a radius, with skirts under the join. Holes at the
  horizon go to 0 in the oracle.

### 5. The look (Cody, in client)

Walk and build across an east–west seam and across the pole's seams:
no gaps, no overlaps, the pointer picks what you look at, a block placed on
a seam goes where you'd expect, and the horizon joins the near world.

## Open decisions

1. **Cut, not stretch** (recommended). A cut cell keeps its grid's texture,
   trimmed, and the seam stays visible as a line where the grids turn.
   Stretching cells to meet hides the line but distorts every block along
   every seam.
2. **The pole.** The cap seams are the harshest because bands crowd toward
   the cap (sprint 30, decision 3). The cut makes them correct, not pretty.
   Making them gentler (gnomonic patches round the cap, or a looser
   tolerance there) changes the atlas and every world. Recommended: after
   this sprint, with the oracle's numbers.
3. **Slivers' threshold**: an eighth of a block, judged in client.

## Built (2026-10-28), for Cody to try

- **The cut** (`planet::Atlas::cut`): a plane for each edge of the territory
  that crosses a cell (`terragen::Atlas::edges`: a segment's meridians, a
  cap's circle, the core's sphere), each fitted to the corners. One plane
  failed at the cap's corners, where two edges meet in one cell (0.25% of
  points round cuts in no cell); a plane per edge is exact there.
  `TestPlanetAtlas::testCutsPartitionThePlanet`: 0 overlaps; gaps 0 on the
  small planet, 0.015% on the default one (1 point in 6,466, at the cap).
- **Drawing**: a cut cell's triangles clipped by its planes; its cut faces
  drawn on them where what's across isn't solid (`drawCutFace`), so two
  solids meet on nothing. What each face lies against is found once
  (`Atlas::cutsIn`); the mesh reads only the nodes there.
- **Collision**: the cut as 4×4 columns of boxes. **Pointing**: the ray
  against the cut solid (`Environment::seamCells`); placing on a cut face
  goes to the other grid's cell just past it. **Selection**: the cell's
  own outline, cut and placed as its mesh is (`AtlasView::outline`).
- **Speed**: the atlas's geometry for a block (rim, cuts, chart) is found
  on its own thread (`MeshUpdateQueue::Preparer`); the main thread's
  share of a seam block's mesh went from ~4 ms to 0.25 ms at the cap.
- **Far meets near**: the mask is each drawn block's true footprint, and
  seam blocks don't mask (their square reaches past their cells).
- **Blocks sent**: `findAt` hops from a seam block to its own grid's
  neighbours that reach into the box (tested in the box's frame). The
  render oracle's unsent blocks: 6 → 0; holes 18 → 9.
- **The planet viewer** is the window's (`Shell::planetarium`), served by the
  menu or the game: `planetarium.hide` always has a host.

Open:

- `TestAtlasRender` budget: the server's first choice of blocks at the
  cap's edge is 2.2 s (1.7 s before; budget 1.5 s). Cached after, but a
  hitch on a first visit. Next: the same off-thread treatment server-side.
- `check:atlas-dig`: one dig of 41 runs ~1 s late (budget 0.05 s). Not
  shown to be new (server lag peaks of ~1 s were in the log before the
  sprint); needs a baseline.
- The oracle's 9 holes and 8 phantoms left are whole cells in interior
  blocks (the cut doesn't touch them): the oracle's flat eye-grid truth
  against the drawn curve, to check.
- Cut faces are textured by projection onto the face they most face;
  a grass side's orientation on a slanted cut face isn't matched.

## Outcome

(filled in at the end)
