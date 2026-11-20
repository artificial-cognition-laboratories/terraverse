# Sprint 100 — River-built detail

Land between the grid's cells was noise, which doesn't know where water goes,
and channels stopped at the grid. Now the drainage goes on below it
(`terragen/channels.*`).

## The network

The grid's tree (each cell to its receiver) is the trunk. Up to 6 levels of
points hashed on a lattice half as wide as the last, down to 48 blocks apart.
Each runs to the nearest point of the level above that isn't uphill of it, so
every level has four times the channels, half as long. Nearest-point segments
can't cross, except where the uphill test turned one away.

Measured on a patch at 100k radius: Strahler orders 1579, 304, 79, 18. About
4.5 per order; real networks are 3–5 (`testChannels`).

## The land

A place's relief is built from its distance to each level's channels, weighted
as real roughness falls (2^−0.8 per octave), centred and scaled to the noise it
replaces, so ranges stay the same height. Noise remains below the finest level.
Used only where water ever ran (seas now, or `ancient_water`), on land. Airless
and icy bodies, and sea floors, keep their noise.

## Water

The first three levels hold water where rain reaches 0.3, 0.7 or 1.2: one to
three blocks wide, a block into the ground at their own column. No level of
their own: one worked out at the channel's warped place disagreed with the
land there and etched trenches. So a stream follows the ground it lies in:
downhill as its valley is (a channel never joins uphill on the grid's land),
not checked block by block. Never through a cone.

## Performance

At 100k radius, on one machine, single runs (`ground()`, µs a column): block by
block 13.5 → about 15; every 16 blocks (far view) 14.6 → about 20. Costs: lists
of nodes per lattice cell, kept per thread; a direct-mapped node cache; 64-bit
squared distances; the warp shared with the rivers.

## Not done

- Trunk and channel lines are still straight between hashed points, under one
  warp. Stars form where many join one grid cell's corner.
- Width doesn't grow with a channel's own area: it goes by level.
- A stream's surface can step up a block where fine relief rises along it.
- A small world (the default 2 607 radius) has too small a cell for any level.
