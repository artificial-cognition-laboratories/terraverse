# Sprint 110: where generation's time goes, and getting it back

Making a 100k world took ~30 s from New World to a planet on screen, and the
create screen's bar sat at 0 for most of it. First: see where the time goes,
by step, for the UI and for perf work alike; then take back what doesn't
depend on the river work still to come.

## Built

- **Steps** (`terragen/progress.h`): each slow piece of work is a named,
  timed, counted `Step` recorded against the world code; whoever's waiting
  reads `making(code)`. The engine logs a world's breakdown when it's made
  (`terragen: <code> made in …`) and when its coarse planet is there (`far:`).
- **`bun perf:gen`**: 50k and 100k, cold and warm, each run a line in
  `.output/perf/gen.jsonl`, shown beside the last like it.
- **What it found** (TF20.Y3gyMzlqNHY at 100k, cold): 12.5 s, of it the river
  levels (`World::profileRuns`) 11.3; plates 0.55, crust and climate 0.36,
  erosion 0.09. Opening a world: its system's layout 0.72 s, its coarse
  planet's 242 tiles 6.8 s, every time.
- **Far tiles kept** (`far/tilecache.h`): by the sources a tile's built from
  (`FAR_SOURCES`), the horizon and the body's place; a kept planet 6.8 s →
  5 ms, and the world model isn't made till a tile's missing. 821 KiB a
  world's coarse planet; every world's under 512 MiB, least recently used
  swept first.
- **Atlas layouts kept**: each body's regions' extents (≈250 000 of the
  projection's probes a body) by the layout's code, in the process and on
  disk: 0.72 s → 0.15 ms.
- **River levels kept by terragen's sources** (`TERRAGEN_SOURCES`), not the
  engine's build: a rebuild that doesn't touch terragen keeps them.
- **River levels read in place order** (a Morton curve through the run
  starts): each thread's channel nodes reused, 9.0 → 5.8 s. **Channel caches
  two-way** (`Ways`), the same memory: → 4.7 s. Twice the slots gave 5.1 s,
  four times 3.9 s at ~60 MB a thread: not kept. Nothing generated changed
  (the golden holds).
- The create screen already starts the world as its code settles (200 ms):
  nothing to add.

- **River levels from the network (TF21).** Each drawn run's level is a
  block under the network's own bed (`linkHeight`), not the finished land
  read at its start: that read was 4 929 full-detail reads on any size of
  planet, each dearer as the planet grew (0.4 ms at 50k, 4 ms at 200k). The
  land's already carved to the river, so it holds. Measured first: the land
  at a run start was within a block or two of the bed at nine runs in ten;
  the rest coasts (the shore's noise) and ridges over the river. Making a
  world (Islands, `TF21.Nnh2enl0em4.AAKAqoCAAP9A2U2ApoCAgICCVQ`): 50k 3.0 →
  1.0 s, 100k 9.8 → 1.1 s, 200k 21.5 → 0.9 s. Water along the drawn runs
  unchanged at 8 points a run; places more than 6 deep 0.04 → 0.06 %. The
  level cache (`.runs`) is gone: nothing slow left to keep.
- **The land wasn't deterministic within a process.** Each thread's channel
  caches marked entries by seed and size, so worlds sharing those (other
  settings, a world's prototype land) were handed each other's nodes, by
  whichever thread built them first: the golden came out three ways in twelve
  runs. Each `Channels` now has its own owner number, and `channelsAt`'s last
  read is keyed by it, not an address that's reused. Golden re-pinned.

- **The create screen's wait was the coarse planet** (traced: asked → view
  built 0.5 s, world 1.1 s, coarse planet 8.8 s at 250k). Its tiles filled
  each run's profile, seven full-detail reads of the land a run: 7.0 s of
  reads at 100k, 22.9 s at 250k. A run's points between its ends now come
  from its ends and the rock it runs over: concave, the drop gathered where
  a harder bed crosses (noise, `RIVER_BEDS`, on the province's hardness),
  so falls stand on hard rock and soft plains run smooth. 0.022 s for every
  run at any size. Water along the runs unchanged; deeper than 6, 0.06 →
  0.12 % (plunge pools). `World::landAt` is gone.
- **The golden reads rivers now**: its patches never fell on one, so the
  profile change passed it unseen. Twelve runs, 16 points each.
- **The loader lost running steps**: `beginMaking` cleared a code's record,
  steps already running with it (the tiles start theirs as the world's asked
  for), so the coarse planet showed waiting while it ran. Running steps are
  kept; finished ones from before are let go.

- **The coarse planet's own time** (timed headless: every root tile of
  `TF21.enRudHJwemY.AwKAqoCAAQCmWZmAgICAgICCVQ`, 128k cells, one thread):
  ground() was all of it, and `channelsAt` 89 %: the trunk level's list
  gathered every grid run within reach of its window, each cut into 12
  pieces (heights, ice, climate), and a run's within reach of many windows.
  A planet has more windows than a coarse planet reads cells, so caching the
  lists couldn't help (four and 32 ways tried: 189k → 128k builds, not
  kept); caching each run's pieces per thread, by cell (1024 slots, ~2 MB a
  thread), did: list builds 33.3 → 3.7 s, the roots 32.4 → 9.4 s, 254 → 73
  µs a cell. The land's the same (golden unchanged).

- **Runs out of reach skipped** in the trunk's scan: its window holds every
  run within reach of anywhere in it, so most are out of reach of any one
  place, and one without a stream, pond or fan changes nothing there. A box
  round its ends tells. 128k coarse reads 9.6 → 6.6 s; the land the same
  (golden, and the reads' sum). Not done: coarse reads leaving out the runs
  that drain little (`hollows`), 6.6 → 4.3 s, but the coarse land then
  stands ~5 blocks higher on average, its small valleys gone.

## Next

- A `loading` message and a stage list, so the create screen's bar moves
  from the first second; the same for entering a world.
- Instrument entering a world: spawn generation, meshing, media.
- Once rivers settle: coarser reads for the river levels, levels only for
  the basins needed.

### The planet from orbit, sketched first

Each root tile is first sketched (`far::Generator::sketch`): the land is read at every other
cell, as land twice as coarse, and the rest are copied. That's about a quarter of the reads.
All 242 roots are sketched before any is built properly, so the whole planet shows in a
fraction of the time and then sharpens. Sketches are never kept in the tile cache. In the
test world, a sketch takes 0.37 of a build's time and sits 1.2 blocks off its ground on
average (`TestFar::testSketchStandsInForTheTile`). The loader's "Seeing it from orbit" step
ends when the sketches are in.

## Rivers that meet (TF22)

The parallel rivers came from the drainage's shape, not the folds: on these worlds the land is
built from the channels, so their pattern is the land's.
- **The grid's rivers wander** (`receivers(surface, wander)` in `grid.cpp`): each cell drains to
  one of its lower neighbours, picked by a hash of the cell and the world's seed with odds as
  the slope squared, not always to the steepest. Also tried: a gentle noise swell to bend them
  (coherent, but too weak where they ran side by side, on steep plateau flanks), and unevenness
  in the land before erosion. Neither changed the parallel bands, which are short rivers down
  an escarpment, so both were dropped. On an even slope they used to run side by side for a continent; now two that
  meet are one from there on (Scheidegger's random walks). Erosion still uses the steepest, or
  plateaus get cut up from every edge (`testRanges`).
- **Each level branches** (`Channels::build`): a node may run into a lower node of its own
  level, anywhere along that node's length, when that costs less than joining the level above.
  (Joined head to head first, a level's water ran as a line with a corner at every head.) "Lower" is judged by the
  way to the level above alone (`down()`), so there are no loops. The fans, where every
  channel of a level ran to the bottom end of the one above, became trees. Their drainage is
  counted down the chain (`drains`).
- Meanders by rock: a run's swing is `MEANDER_SOFT` (1.2) to `MEANDER_HARD` (0.35) of what its
  grade gives, by `hardness()`. Channel streams grow up to `STREAM_WIDEST` 9 blocks (was 6).
- The coarse reads of the 100k default world: 6.6 s → 2.2 s.
- `testNoPoleOrSeam` now ignores steps across a shore: a coastal cliff on its walk isn't a seam.
- Seen with `scratchpad/wet/net.cpp`, which draws the network by level over the shaded land.
- **Width from source to mouth:** a grid run is never narrower than `Channels::streamWidth` for
  what it drains, so a stream grows on into its river instead of narrowing where it reaches a
  run. Streams are capped at 16 blocks (was 6).
- **Denser by default:** the Rivers setting maps to the river threshold as the cube of
  (1 − setting). The default 0.5 is now about 6.5 cells' rain (was 14); the ends are unchanged
  (24 and 4).
- `testNoPoleOrSeam` skips steps into rivers as well as across shores.
- Coarse reads: 2.3 s → 2.9 s (more rivers drawn).

## Scale (sprint 110, TF22)

- **Continents** go up to 16 (were 8). The default stays at 7: its byte is now 102.
- **River width** comes from what a river drains, not the planet's size (`World::riverWidth`):
  1.2 m × √(area in m²) at Earth's scale, which gives 1 m for 1 km², 40 m for 1 000 km² and
  2 km for the Amazon. It's taken × √(R⊕/R) in blocks, as heights are. Channels' streams use the
  same rule. Before, big rivers grew with the planet: 15 km across on half the Earth.
  Drawn runs on seed 63gf33qq:
  | Radius | Median | p90 | p99 | Widest |
  |---|---|---|---|---|
  | 100k | 31 | 91 | 250 | 250 blocks |
  | 3.2M | 178 | 516 | 1614 | 4444 blocks |
  | 6.37M | 251 | 728 | 2278 | 6270 blocks (metres) |
- **Measured, not yet fixed:** mountain relief at Earth size, in rising ranges, as the highest
  minus the lowest ground in a window: 45 m in 1 km, 167 m in 10 km, 352 m in 50 km. Real ranges
  are ~300–600 m, ~1.5–2.5 km and ~3 km. Peaks are about right (2.2 km median, 4 km highest).
- **Why big planets have few small rivers:** only `STREAM_LEVELS` (3) channel levels carry water,
  by index. At Earth size the finest wet level is a 12 km lattice, so streams start at ~150 km²
  drained. Real streams start at ~1 km². On the 100k world the same three levels go down to 375
  blocks.
- **Streams by real size:** a channel level carries water when its lattice is at least
  `STREAM_LEAST_M` (1.5 km across, as the Earth would have it) and `STREAM_LEAST_BLOCKS` (150)
  wide. The rain each needs runs from 0.3 at the coarsest wet level to 1.2 at the finest. The
  default planet keeps its three wet levels; an Earth-sized one now has six, down to a 1.5 km
  lattice (it had three, down to 12 km).
- **Ranges by real size:** the cap on how far ground stands over the grid's surface is
  `OVER_GRID_M` 2400 m, taken in blocks as heights are (300 blocks on the default planet, as
  before). Relief in rising ranges, highest minus lowest ground:
  | Planet, window | 1 km | 10 km | 50 km | Peaks (median / highest) |
  |---|---|---|---|---|
  | Earth, was | 45 | 167 | 352 | 2185 / 4036 m |
  | Earth, now | 245 | 850 | 1676 | 3124 / 5092 m |
  | Half-Earth, now | 119 | 566 | 831 | 2088 / 3566 blocks |
  | 100k, unchanged | 7 | 52 | 127 | 315 / 523 blocks |

  Pinned by `testRangesAtEarthsScale` (the median over 10 km must be above 500 m; it's 1006).
  Still a little under real ranges at 1 km: the finest channel level on an Earth is 750 m
  (`MOST_LEVELS` 7).
