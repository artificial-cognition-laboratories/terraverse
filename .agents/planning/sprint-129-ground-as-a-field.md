# Sprint 129: ground as a field

Sprint 128 gave the land its height to a sixteenth (`terragen::Ground::frac`, `ground::Rises`), but
`ground::Ground` still shapes only a column's top block, and its clusters split at a two-block
step. So wherever the land falls more than a block a column, the top is smooth and the sides are
whole cubes: boxy crests on steep sand, cliffs stepped a metre at a time down and along. The
heights are fine; the shape rule is a block at a time. This sprint draws natural ground as one
field.

## Decided (2026-11-27)

1. **One field for natural ground.** A corner's height is the average of the fine heights of the
   four columns there, with no clusters, where all four are natural (undisturbed, their rises
   kept). Ground that's been dug or built on keeps today's rule (clusters, cliffs at two blocks).
2. **Every node the field passes through is cut**, not just the top: a node is filled up to the
   field where the field is below its top. Nothing new is stored; the field comes from the rises.
   A cut node reads as its material where at least half is there (`ground::seen`), as now.
3. **Loose ground stands no steeper than its material holds.** At generation, ground steeper than
   its loose top can stand (sand ~34°, soil and gravel steeper) shows what's under it: terragen's
   `bare` extended by material, a function of the column, nothing simulated.
4. **Rock follows the field too**, steep: a cliff stays a cliff, drawn as a steep plane at
   sixteenths, not stairs.
5. **The first dig keeps the drawn shape**, as the top block's does today (`Top::shape`).

## Phases

1. **Loose ground as a field.** `ground::Ground` gives a node's cut from the field (corners for a
   column, which nodes of it the field crosses); the mesher draws every crossed node, collision and
   the machines' ground read box them. Fixes the sand crests and the grass lip.
2. **Settling by steepness.** terragen's per-material angle of repose: too steep, the loose top
   gives way to filler or rock. Test: no generated loose node steeper than its material's angle.
3. **Rock follows the field.** The cliff faces cut at sixteenths, down and along.
4. **Checks.** TestGround (a field across a six-block drop agrees at every corner; a crossed node
   is cut, an uncrossed one whole), `atlas-land`, `check:land`, `check:moves`, `check:far`, the
   game: the sand ridges and the cliff from the sprint's screenshots.

## Progress

- **Phase 1 built (2026-11-27), uncommitted.** `Rises` keeps every natural top (a rise of 0 too;
  `get` → optional: none is dug or built). `corner()`: all four natural → the fine heights' plain
  average, clamped to [mine − REACH, mine + 1] blocks. `Top` gains `surface` (where a top's drawn),
  `below` (a slice's depth) and `open` (loose blocks under the top the field may run into, never
  rock), with `build` and `slice(k)`; `Ground::top` answers for a loose block under natural ground
  with its slice. Mesher: tops only where `surface`, a slice lit from its column's top
  (`getSmoothLightFrame(lift)`). Test `testNaturalGroundIsOneField`.
- **Phase 3 built (2026-11-27), uncommitted, before phase 2.** Natural rock follows the field:
  the generator keeps every walkable top's rise; a natural solid (`Column::natural`, its rise
  kept) joins the average and is cut, and slices run through rock as through loose blocks. Built
  rock (no rise) keeps its block. Test `testNaturalRockFollowsTheField`; atlas-dig passes.
- Known: the low side of a corner rises at most a block over its top, so drops over two blocks
  still meet in walls; phase 2 (loose ground no steeper than it holds) is what removes them.

## Costs to measure

- Meshing and collision on steep ground: several cut nodes a column instead of one. The debugger's
  Performance tab on a mountain, before and after.
- Digging the first cut of a node deep in a slope: the shape it keeps.

## Open

- A cell's field is two flat triangles: tall cliffs may show facets. If so, terragen's height at
  the cell's middle as a fifth point.
- Where natural ground meets disturbed ground: which rule a corner takes when its columns are mixed.
- Sprint 128's leftovers: the far view at sixteenths, rises across atlas seams, the TF25 bump.
