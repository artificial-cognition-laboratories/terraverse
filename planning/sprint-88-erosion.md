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

**Pass 2: the model, and rivers that meet their water.**
- Uplift: each round the belts push land up (`UPLIFT` × belt), so ranges
  stand where uplift and erosion balance.
- Hillslope creep (`CREEP`, κ ∇²h between land cells, explicit), conserving:
  remainders under a unit of height are carried round to round.
- `testSediment` now balances worn + uplifted + hollows = to the deep, exactly.
- Sediment feeds the ground: `alluvium(sediment)` (full at ten blocks) adds
  silt and clay to the soil (`World::soil`) and richness to fertility.
- Lakes and rivers read one shared warp (a third of a cell in waves two
  cells long, plus a sixteenth in waves half a cell long; steeper warps fold
  and a wide river broke into pools), so rivers run into their lakes; runs
  between two lake cells are under the lake.
- Meanders by the river's own width: each run swings to one side and back
  in loops about eleven widths long, a whole number to the run (zero at both
  ends, so runs join), never steeper than they fold.
- `render … sediment`.
- Pits: the priority flood plus `keepRealLakes` already routes water through
  every basin to its spill point; Cordonnier et al.'s method is the same
  outcome, faster. Not needed at 64 002 cells (the grid takes ~0.4 s).

**Pass 3: placers, streams, the reference.**
- Placers by process (`grid.cpp`): the rivers carry heavy grains worn out
  of the belts' cores (belt over 0.3) beside their load; they settle
  `HEAVY` (2) times as readily, so they gather where the rivers first slow
  below the mountains. `Grid::placer` (0..1) is how rich what was laid is,
  outside the source rock. `Strata::placer` is now that richness (was a
  flag for mountain provinces, i.e. at the source); placer gravel lies in
  river and lake beds and, more thinly, on floodplains, by richness. Rare:
  rich gravel on 2–6% of the land. `render … sediment` shows it in red.
- Streams (`Grid::stream_threshold`): four times the rivers' share of the
  land carries a stream, along the same drainage, one to five blocks wide
  on any planet; a river widens from a stream's width to its own over its
  first few times the threshold (the channel never jumps). Tested: several
  times as many stream cells as river cells, all on land.
- **The reference.** fastscapelib wouldn't install here (no wheel for this
  Python; uv broken), so we checked against what its model predicts: at
  steady state, channel slope ∝ area^−0.5. Ours is −0.10 to −0.16
  (`scripts/terragen-check/concavity.cpp`); more rounds don't change it
  (24, 48, 96: the same, and every lake silts up). The reason: our land is
  the noise worn for a while, not land built by uplift — uplift is only on
  the belts' narrow crests, where streams start. Real profiles need broad
  uplift through time: **sprint C (plates with history)**, with this as
  its check.

Not done: streams finer than the grid's own drainage (every cell's stream
is still a run between centres; a hillside inside one cell has none).

