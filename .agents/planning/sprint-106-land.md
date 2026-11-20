# Sprint 106 — The land

A pass over the shape of the land, tuned by eye in the planetarium and by
`scripts/terragen-check/region.cpp` (a region at block scale, hillshaded, with
hypsometry and slopes; `BIOMES=1` for the ground's biomes). Uncommitted, and
the world code still says TF16: see Housekeeping.

## Done

- **Seeing it.** The planetarium shades relief: each tile's top carries the
  land's slope (`far/mesh.cpp`, the normal put on the planet per vertex in
  `Tiling::place`), lit from a map's north-west light and the star, slopes
  three times steeper than they are; contours on the Elevation overlay (50
  blocks, every fifth bold). The far view's shader ignores normals.
- **Continents are platforms.** Above the median land height crust rises at
  35% of its old rate (`PLATFORM_RISE`): low flat interiors, ranges on them.
- **Gullies** (`World::gullies`): an erosion filter, waves down the slope
  from a cell to ~40 blocks, each octave down the slope the coarser left, so
  drainage branches. On rising ground and plateaus, none on plains. Fixed on
  the way: a slope a few thousandths steep rounded to nothing when squared in
  a fix, snapping the waves' way across a block on: long straight steps along
  the grid (now 64-bit); waves cancelling at lattice planes flipped sign.
- **Rivers by rain**: a river where 4–24 cells' rain has gathered (the
  Rivers setting), not a share of the land. The one-block straight streams
  are gone; meanders swing by the run's grade; the inside of a bend between
  runs is rounded.
- **Valleys cut by flow** (`Channels::Read::cut`): each sub-grid channel cuts
  a valley as deep as √ of its rain, 6 blocks on a plain to 45 in a range,
  and the wetter ones carry streams on the floor (2–5 wide). Channels bend
  by noise at their own scale.
- **Lakes**: shores where the ground dips under the level (drowned valleys,
  bays, islands), reaching a cell past the outline.
- **Basins**: interiors sag after erosion (`BASIN_*`); a closed basin keeps
  a lake only as big as evaporation balances (`evaporation`), else a salt
  lake or dry pan, everything draining to it (`Grid::pan`).
- **Coasts**: on hard rock the coast is ragged (read off by noise), soft
  beds wear back between harder; a sandy coast stays smooth.
- **Landforms**: three biomes appended (indices kept): salt flat (salt
  crust), marsh (mud, half pools), floodplain. Protocol 98.
- **Fold ridges** in patches, bending, splitting, lower; and only in a belt's
  outer six cells (the foreland: the Jura, the Valley and Ridge), fading with
  its age (they'd covered 40% of the land, ribbing whole continents).
- **Rifts** open in basins strung along the boundary, not one trough.
- **The far view's gullies** leave out waves under two of its cells (they beat
  against the atlas grid in diagonal stripes, `World::Coarse`).
- **Plates that wander**: the finished history read through a smooth warp
  (5 cells deep, bends ~30 cells long, `history.cpp` WANDER): coasts, rifts
  and sutures bend instead of running in ruler lines. (Deeper than a bend's
  length over 2π and it folds, scattering plates into each other.)
- **Salt pans follow the ground**: a pan holds the level its water stood at
  (`Grid::lake_level`); its floor is worn flat to it, salt where the ground's
  under it, over any biome (pans lie near the sea's level, where it's beach).
  Marsh's edge jittered by noise.
- **Deltas** (`World::deltas`): a river of 10× a river's least flow over low
  land into the sea builds a fan 40 of its widths out (≤ 3 cells), lobed,
  marsh with pools, split into 3–6 distributaries from the apex. The fan's
  built before the rivers, so the river runs on over it.
- **Rivers**: each run bowed (Hermite) to leave and arrive along the river's
  way through each end: no corners where runs meet. No wider than a sixth of
  a cell (were 60%: slabs with straight banks whose ends didn't meet).
- **Alpine crests**: plateaus need more thickening (rare); a rising range's
  core over 1500 m is raised along the channels' own divides, sharpened (t²:
  flat floors, edged crests), so crests branch with the drainage. Gullies
  fade on the level (a summit drew a star of waves and a straight canyon).
- **Glacial valleys**: where it's under 1 °C a year now, valleys are U-shaped,
  60% deeper, at least 400 m.
- **Sea floor**: its detail read through a warp (the value noise's lattice
  showed as squares).
- **Ores**: flint nodules in the chalk's beds (`Ore::Flint`, appended: protocol
  99); clay under marshes and on floodplain river banks (`biomes.lua`).
- **Streams renumbered** off each other's: RIFTS 419, HISTORY_WARP 421, ALPINE
  431, FLINT 433, channels' BEND 379 (they'd shared volcanoes', hotspots',
  sediment's and folds').
- **Defaults**: 7 continents, 25% land (code.cpp and config.ts).

## Housekeeping (done)

- World code **TF17** (TF16 refused as an older build's); golden re-pinned.
- `testRanges` asks ridges cross the belt 5/4 as often as along it (they bend
  now); `testDecodeDefaults` 7 / 0.25.
- `grid.cpp` sediment: `lowest_donor - 1 - h` overflowed (INT32_MAX with no
  donor, under the sea): undefined, now 64 bits.
- `check:terragen`: the predictor knew no soil depth for the landform biomes;
  the probe skips things lying about (`lying`); sticks don't lie on sea ice.
- Passing: TestTerragen 40/40, TestFar, `check:terragen` 42/42, `check:far`,
  `check:palette`, headless boot, UI typecheck.
- Failing, not this sprint's: `TestFarPalette.testNothingInsideTheCut`,
  `TestAtlasRender` (its own sine ground: mesh holes, one view's choosing over
  budget), `TestMoves.testBonesStayOut`.

## Next, in order

1. **Fjords** from the grid: a glaciated coast's cells cut under the sea
   there, so the far view and the game agree (a column's cut is narrower
   than the far view reads: `testCoarse` caught it).
2. **Sixteenth-voxel terrain**: heights to a sixteenth from terragen down,
   the mapgen writing cut ground (the mesh smoothing goes). Touches the map
   format and the mesher: needs a design pass first.
3. **Flint's own texture** (drawn for now as the coal's, chalk-pale).
4. **Placers and bog iron** read off the new rivers and marsh.
5. **River widths on the default (small) planet**: the sixth-of-a-cell cap
   is ~6 blocks there; tune by eye.
6. Stream ids: ARC_CONES and KREEP still share 197 (older).
