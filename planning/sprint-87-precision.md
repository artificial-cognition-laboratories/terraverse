# Sprint 87 — Places to the block

A place on the planet (`terragen::Dir`) is a unit vector in fixed point,
16 fractional bits: it can tell apart points 1/65 536 of the radius apart.
At the default size (radius 2 607) that's 0.04 blocks; at 50k radius 0.8;
past ~65k radius it's coarser than a block, and neighbouring columns share
a place (measured along 2 000 blocks of the equator: 1 399 distinct places
at 100k radius, 620 at 250k, 26 at Earth's). The land comes out in
terraces: the lines left on a 250k planet.

Sprint 88 (erosion) reads places much more finely, so this goes first.

## The rule

**A column is asked by a `Place`: its direction to 30 fractional bits.**
What works at cell scale (the grid, plates, climate, lakes' outlines,
`between`) takes the place's coarse `Dir`, as now; what works at block scale
reads the fine place:

- the surface's fine detail noise (`World::surface`);
- distances in blocks: rivers' channels and banks, volcanoes' cones and
  craters, a column's crater distance in its strata;
- a block's point in the planet (`World::solid`), so block noise (rock
  folds, veins, caves, blends) and per-block hashes;
- tree spots and a column's roll (`World::cover`).

## Plan

- `terragen::Place { int32_t x, y, z; }` at 2³⁰ a unit, with `dir()` (the
  coarse `Dir`, rounded), made from a vector (`placeOf(x, y, z)`) and from
  latitude and longitude. Fixed-point helpers for it: chord, offsets in
  blocks, `noise` at a place.
- `World` takes a `Place` wherever it takes a `Dir` from outside;
  `World::place(u, v)` gives one.
- `planet::Atlas::place` gives a `Place` (the cell centre to 1/1024 of a
  block, then 30 bits); `core.terra.place` returns it.
- The check's predictor and probe pass `Place`s.
- **TF7**: every place shifts a little, so every world changes. TF6 is
  refused.

## Checks

- Distinct places per block: along 2 000 blocks of the equator at every
  size up to Earth's, 2 000 places (a unit test).
- `TestScale`: no step is a cliff, at every size (as now), and the land
  isn't terraced (a run of equal heights is no longer than the flat ground
  around it allows).
- Golden re-pinned for TF7; `bun check:terragen`, `check:far`, the atlas
  checks.

## Not in this sprint

Erosion (sprint 88). The grid's resolution.

## Outcome

- `terragen::Place` (`sphere.h`): 30 fractional bits, made from a point
  (`placeOf`), widened exactly from a `Dir`, `dir()` back; `chord` between
  places. `mulShift` (`fixed.h`): an exact 64 × 64 → shift for products
  past 64 bits (a fine place times a large planet's radius). `noise` and
  `fbm` at a place.
- `World` takes places; `blocksFine` turns a place's length into blocks.
  The cell-scale fields read `dir()`. `World::place(u, v)` is still made
  from 16-bit trig: coarse, fine for pictures (the preview, the painter,
  the weather's grid) and the tests' flat samples; the atlas's places are
  made from the point.
- The atlas, `terra::Columns`, the land reader, `core.terra.place` (2³⁰ a
  unit) and the check's predictor pass places.
- TF7; TF6 refused. A place widened from a `Dir` gives what the `Dir`
  did, so the golden (its samples are such places) is unchanged; worlds
  change through the atlas's finer places.
- Fixed on the way (sprint 86's follow-up): a radius past 32 767 blocks
  round overflowed a fix (craters 32 000 blocks high, lines across 10k,
  50k, 100k planets); the radius and cell spacing are 64-bit
  (`World::blocks`, `angle`), the noise's lattice coordinate no longer
  overflows on huge planets (exactly the same bits below that). The
  golden's own code was 62 832 round: re-pinned once for that.

**Run:** `TestSphere` (2 000 places in 2 000 blocks at every size to
Earth's), `TestScale` (steepest step at Earth's size 1 block, was 17),
`TestTerragen`.
