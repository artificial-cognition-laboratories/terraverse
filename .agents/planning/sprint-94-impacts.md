# Sprint 94 — Impacts

Every body is hit, all through its history. Where nothing wears or
recycles the surface (no water, no plates, little volcanism), the craters
stay: crater density is a surface's age. The Moon's face is mostly this.

## The model

**Population.** Craters of every size from half the body's radius down to
a few blocks, in classes a factor of two apart. Each class covers the same
share of the surface (cumulative counts fall as D⁻², near the saturation
of old highlands), so it's scale-free: the same rule on any body. Each
crater has an age; younger ones are crisp, older ones softened.

**What keeps them** (`Processes::craters`, 0..1, from the body card, never
its name): water wears them (a four-hundredth survives: Earth keeps about 190), plates recycle the
crust (a third), volcanism resurfaces (a quarter). Air burns up the small
impactors: none smaller than 100 m × the air's pressure in bar.

**Shapes** (Pike 1977; Melosh 1989), in the body's metres (blocks by
`blocksPerMetre`), with gravity:
- simple bowls below the transition diameter, 3 km / g (Earth ~3 km, the
  Moon ~18 km): depth 0.2 D, rim 0.036 D;
- complex above it: shallower (depth ∝ D^0.3), a flat floor, a central
  peak;
- basins above 12× the transition: a ring outside the rim;
- an ejecta blanket falling as (r / R)⁻³ to a diameter out.

**Where.** Craters are hashed on a 3D lattice per class (a candidate per
cell, kept in a thin shell about the sphere so the density is even), so a
column finds its own without a list. Craters wider than two grid cells
are also stamped into the grid (the preview and planetarium see them, and
sprint 95's maria fill them) and indexed per cell; a column reads them
exactly and takes the grid's sampled stamp back out.

## Checks

- The Moon: craters at every size; most of its surface within one.
- Earth: a few, worn.
- Deterministic (golden re-pinned; TF9).

## Outcome

- `terragen/craters.*`: `Craters` hashes a candidate per lattice cell per
  class (classes [D, 2D) across, from R/4 down to 8 blocks, cells 2D wide,
  each class covering 15% of the surface × what the body keeps), kept only
  in a shell a cell thick so they lie evenly; across by a D⁻² draw, worn by
  age (depth and rim to a quarter; halved again on a wet body). Shapes:
  bowls below 3 km / g (depth 0.2 D, rim 0.036 D), complex above (depth ∝
  D^0.3 and rim ∝ D^0.4 from the transition's, a flat floor, a central
  peak), basins past 12× with a ring at 1.6 R; ejecta as (r / R)⁻³ to a
  diameter out. Craters add to the height.
- `Processes`: `craters` (the share kept: water ÷400, plates ÷3, volcanism
  ÷4), `smallest_crater` (100 m a bar of air), `gravity`; water also wears
  away all under 2 km.
- The grid (`grid.cpp`): craters wider than two cells are stamped into
  `base` (`crater_stamp`) before the climate, so rivers and the preview
  meet them, and indexed per cell (`crater_start` / `crater_list`). A
  column (`World::craterBlocks`) reads those exactly, less the stamp
  between cells, and finds the rest itself (`crater_from` on).
- The default Moon (50k home): 15 249 blocks round, about 52 000 craters
  in the grid's classes alone; Earth keeps about 150, mostly small and
  worn. `render` shades dry bodies grey; `render <code>@<body>` took the
  home's radius in fixed point (a moon 65 536 times too big): fixed.
- A column costs 5.9–7.4 µs (was 5.6): the hashed search.

Not yet: rays and bright young ejecta (a material, with the Moon's rock in
sprint 95), craters on the ground's rock (strata) and regolith depth.

Golden re-pinned (3559267434); TF9.

**Run:** `TestTerragen` 31/31, `TestBodies` 5/5 (the Moon's craters: many,
wide ones in the grid, a pit with a rim; Earth's a twentieth of them at
most), `TestScale`, `TestSphere`, `TestPlanetAtlas`, `TestFar`,
`TestFluid`, `TestPlanetWindow`, `bun check:terragen` 39/39, `check:far`
100%, `check:bodies`. Not run: the game, the planetarium.
