# Sprint 88 — Land made by uplift and erosion

Today the land is noise shaped by the plates, worn once by an implicit
stream-power step. Real land is the balance of two processes over millions
of years: rock pushed up where plates meet, and water and gravity wearing
it down, carrying it off and laying it down again. This sprint runs that
balance over the whole planet at world creation.

## Decisions (agreed)

- **Our own erosion, in fixed point,** from the papers, on our icosphere.
  Clients rebuild the land from the code, so it must be identical on every
  machine; fixed point gives that for free, deterministic floating point
  doesn't. fastscapelib (BSD-3) is the reference we check our results
  against, never a dependency.
- **Places to the block first** (sprint 87).
- **Small streams are carved into the block terrain**; finer shaping with
  sub-voxels later.

## The model

Each step, on every cell:

1. **Uplift** U from the plates: up where they converge (the belts), down
   where they rift, slow sag in basins. Today's plates for now; sprint C
   (plates with history) makes U change through time.
2. **Rivers cut** by stream power, E = K · A^m · S^n, solved implicitly
   down the receivers in one pass (Braun & Willett 2013): stable at any
   step, O(n).
3. **Sediment is carried and laid down** (Yuan et al. 2019): what a river
   can't carry where it slows is deposited, which makes floodplains, fans
   and deltas. Erosion and deposition balance: nothing is made or lost.
4. **Hillslopes creep** by diffusion (κ ∇²h), which rounds ridges.
5. **Pits and lakes** are routed properly (Cordonnier et al. 2019): water
   fills a basin to its spill point and carries on, so lakes sit in real
   basins and rivers don't stop in holes.
6. **The sea** is a fixed level: rivers end there, sediment builds out
   from the coast.

Run to a steady state (uplift ≈ erosion) or a set age; the code's settings
(rivers, roughness) set K, m and κ.

## What comes out

- Height, drainage area, receivers (the river network), lakes and their
  levels: what the grid has now, made properly.
- **Sediment thickness** per cell, which feeds:
  - soil depth and texture (alluvium along rivers, thin soil on ridges);
  - placers: gold and tin downstream of eroded plutons;
  - basins for coal and salt.
- Small streams: carved into the block terrain below grid resolution,
  following the fine slope.

## Checks

- Invariants: everything drains to the sea or a lake; rivers flow downhill;
  the land share holds; sediment balances (eroded = deposited + to sea).
- A golden for the result; a timing test (world creation within a few
  seconds; the preview usable).
- By eye: renders of relief, rivers and sediment, beside fastscapelib's on
  the same uplift and parameters.

## Not in this sprint

Plates with history (sprint C), the energy-balance climate (D), the sky
(E). Glaciers. A finer grid where it matters (planned, after the streams
carved into blocks show what's missing).

## Outcome so far

**Pass 1: coasts, lake outlines, sediment** (from the 100k planetarium shots:
dithered coasts, lakes in the cells' polygons).
- Sediment (`grid.cpp`): each erosion round carries what it wore away down
  the receivers; a stream keeps `CARRY · F · drop` and lays down the rest
  (never above its lowest donor), lakes silt up to half their depth, the sea
  builds a shelf to just under its level, the rest goes to the deep
  (`Grid::sediment`, `to_deep`; hollows filled as before are counted apart,
  `hollows`). `testSediment`: worn = laid + to the deep, exactly.
- Coasts (`World::surface`): the coastline wanders by cell-scale noise (5
  octaves, 3 blocks), and the fine detail fades to nothing at the waterline
  (at most half the height over or under it), so land and sea no longer
  flip column by column.
- Lakes: the mask is read at a place warped by up to a cell, and its edge
  wanders by finer noise: bays, inlets and islands, not triangles.
- Golden re-pinned (TF7 is unreleased).

Still to do: uplift through time, hillslope diffusion, pits routed
(Cordonnier), sediment feeding soil and placers, streams below the grid.
