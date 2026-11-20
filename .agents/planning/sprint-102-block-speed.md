# Sprint 102: blocks arrive

Terrain near the player took minutes. Measured on a headless server, 100k+
radius world (`chunksize = 1`, so a generation job is one 16³ block).

| ms a block | surface | air | view-like stack |
|---|---|---|---|
| before | 42.8 | 41.7 | (rock alone: 7.4) |
| lighting walks y innermost | 8.8 | 8.5 | |
| reader keeps 8192 columns | 6.0 | 6.0 | 3.95 |

Causes: `lightAtlas` walked x innermost past a one-column cache, rebuilding a
column per open node; each block built its columns twice, and again for the
block above. Also: the 2 s "nothing to send" pause no longer outlasts moving or
turning; new worlds start at radius 100k (`NEW_WORLD_SIZE`; `DEFAULTS.size`
keeps its meaning in codes).

Second pass, once the far view took 30 s to reach the horizon:

- A far cell 4 km from its neighbour cost 160 µs against 9 beside it: every
  channel level and crater class searched afresh, narrower than the cell.
  `World::Coarse` leaves those out for far tiles (9 µs). Level-0 tiles and
  generated blocks are unchanged; `testCoarse` bounds what moves.
- Far view builds on half the machine (was 2 threads of 8).
- Grids kept on disk (`terragen/grid_file.cpp`, `<cache>/grids`, named for the
  engine build; any rebuild starts afresh). A golden run read back from disk passes.
- Trees gone: not planted, not drawn far off, no crown growing. They come back
  as species with traits. Registered species, `Cell::trunk/canopy` and the
  protocol's list remain until then.
- `terra` runs 4 emerge threads: 1.6 ms a block of wall time for a stack.
- Flying only passes through unloaded ground above any possible ground (25 km
  at the planet's scale); lower it waits, as walking does.

Third pass (the speed pass):

- `terragen/parallel.*`: fixed parts, joined in order; a world's hash is the same
  on 8 cores and 1. History, climate and the grid's per-cell loops run on it.
- Stages kept by exactly what they read (`historyKept`, `climateKept`): for one
  seed, 0.77 s a full build, 0.14 s for a change of Rivers (0.26 s Temperature, Humidity).
- `fill` floods from coasts only; heights sort as packed keys (`lowestFirst`);
  insolation asked twice of a place answers from memory.
- Mapgen objects `shore`, `terra_cover`: found natively, so Lua no longer copies
  48³ a block. Lock time 1.09 → 0.31 ms a block; on forest land 2.1 ms a surface
  block, 1.5 ms across a stack (4 threads).
- Profiler: "Far view: tile built".

Not measured in play: time to the horizon, block sending.
