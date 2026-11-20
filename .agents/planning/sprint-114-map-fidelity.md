# Sprint 114: Map fidelity

The planetarium should look as good from orbit as it does up close. Today it only
does if you zoom in first: a fine tile, once built, stays when you zoom back out,
and those frames look right. Ordinary frames look blocky from orbit, speckled when
the planet is small, and the rivers are swollen.

## Why

One decision causes all three: **a tile cell is one point sample, drawn as one
flat-coloured quad, and that grid is both the shape and the colour.**

- `planetarium/camera.cpp` lets a cell grow to `CELL = 6` pixels before its tile
  splits, so every frame is a field of 3–6 px blocks, each coloured from the
  column at its centre.
- Further out a cell stands for thousands of columns but shows one of them. That
  is aliasing: snow beside forest beside rock, chosen at random. A tiny planet
  needs the *mean* of what a pixel covers, not fewer samples.
- A point sample can't say "a tenth of this cell is river", so coarse tiles show
  a river by widening it to a whole cell (`World::Coarse` at the tile's stride).

## The design: shape and colour apart

1. **Geometry is coarse.** A tile's mesh only needs to carry the relief's silhouette.
   It splits on screen error (pixels the shape is off), not on cell size.
2. **Colour is a texture per tile**, about one texel per screen pixel (256² against
   32² vertex colours today).
3. **A texel is the mean of the ground under it.** Fine levels sample columns.
   Coarser ones are filtered down from their children, or from jittered samples
   per texel where no children exist yet. Then a tiny planet averages correctly.
4. **Rivers by coverage, at their true width.** terragen knows `river_dist` and
   `river_width`, so a texel is blued by the share of it the river covers. Rivers
   fade honestly with distance. Keeping big ones legible from orbit is a styling
   choice (weight by size, as an atlas does), for the next sprint, not the sampler.
5. **Tiles are cached on disk**, keyed by world code and generator version, so the
   coarse levels cost nothing the second time.

## Steps

1. **Measure.** `bun shots:map` renders a ladder of fixed shots (tiny planet, orbit,
   continent, region, close) with the shipping renderer. It renders the same views
   supersampled with cells under half a pixel as the reference, and scores each
   by RMSE. That gives before and after images and a number.
2. Tile colour textures, with separate split rules for texels and geometry.
3. Filtered colour for coarse levels (children, or jittered samples).
4. Rivers by coverage; the widening removed.
5. Persistent tile cache and a cost budget for building cold.

## Not this sprint

Cartographic styling: hillshade, labels, river weighting, coastlines.

## Risk

Filtering the coarse levels needs far more terragen samples per tile. If building
from children is too slow cold, the fallback is 4–16 jittered samples per texel on
the coarse levels, which still removes the aliasing.
