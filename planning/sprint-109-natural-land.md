# Sprint 109: natural land

The land from the drainage (sprint 108) is right locally but reads as made:
ridges and hills run in regular parallel lines across whole ranges and
lowlands, and some ranges are cut into straight-edged blocks.

## What phase 0 found

- **The gully filter isn't it.** Turning `World::gullies` off leaves the land
  unchanged where it's striped: with the drainage land on, it's not what
  shapes those slopes.
- **Window seams.** `Channels::read` makes the land the lowest hillside over
  the channels in a window round the place's lattice cell (two rings of grid
  cells for the trunk, ±2 lattice cells for finer levels). Where two windows
  meet, a channel can be in one and not the other, and the ground jumps: a
  232-block cliff a block wide on TF20.Y3gyMzlqNHY at 100k radius
  (u 0.47006, v 0.84752). The lattice is laid in x, y, z, so the seams are
  straight, parallel lines across the land: the rectangles, and very likely
  the stripes.
- **Measure.** `jumps`: pairs of land columns a block apart that differ by
  more than 8 / 32 / 100 blocks, from random places (scratch tool, to move
  into `scripts/terragen-check/`). TF20.Y3gyMzlqNHY (100k): 32 / 1 / 0 of
  18 558; TF20.dGVycmE: 4 / 1 / 0 of 14 510.
- **Also seen:** a range's high ground scattered with ponds (tarns) far
  denser than real land carries.

## Built

- **P1, seamless land.** Trunk windows gather every run within reach by
  distance (`m_trunk_ball`, from the runs' measured wander); finer levels'
  windows ±3 lattices round the coarser-bent place; each channel's hillside
  fades out (`FADE_TO`) before its window ends. Ponds' bowls and rims only in
  their own valley (perched ponds walled off the valley below, and scattered
  tarns over high ground). TF20.Y3gyMzlqNHY at 100k: worst seam 38 → 13,
  steep walls 60 → 5; the 232-block cliff gone.
- **P2, bend and vary.** What looked made was the trunk: every grid run, a
  cell apart, cut a full valley, straight from centre to centre, so a range's
  flank was a comb of parallel spurs (seen by colouring the land by which
  level owns it). Runs draining under `TRUNK_LEAST` (3 cells) aren't trunk
  valleys (the finer levels shape them; kept where the trunk's the finest
  read, a far tile's); undrawn trunk valleys swing in one to three bends
  (`VALLEY_SWING`); hillsides' steepness varies ±30% by place (`VARY`); level
  1 thins in clumps as finer levels did.
- **P3, the ground decides.** `Channels::hardness`: a province's strength,
  blended between cells, ±30% by beds and joints (`ROCK_VARY`). Drainage
  denser on soft and wet ground (`DENSE_SOFT`, `DENSE_WET`): level-2 heads
  52% of points on basins against 36% in orogens (43–44% both before);
  hillsides steeper on locally harder rock; joins dearer into harder rock
  (`DEFLECT`, the province's strength: the varying field per candidate cost
  12% more a column). Ponds fade out (`POND_OWN`) as their channel stops
  being the lowest, rather than switching: the switch left one-block
  pillars and pits (worst 43 blocks), and most of what P1 counted as stream
  banks were these. Only the trunk's valleys drown at the coast: a finer
  valley's inlet is narrower than a far tile reads, and its coast flipped.
  Seams 37 → 21, worst 27 → 15; 97 µs a column.
- **P4, gentle land, ponds, banks.** Ponds by area, not a share of every
  channel (which stacked them level on level): tarns only on the level nearest
  a cirque's size (`CIRQUE`), `TARNS` a km² of its lattice by ice; pools only
  where a channel has a floodplain (`POOLS`). Where ice worked, tarns 18.8 →
  0.51 a km²; elsewhere pools 4.8 → 0.42. Gentle land rolls ±5 blocks over
  ~600 (`ROLL_*`), clear of beds and ponds. Banks: hard rock's coast lift
  (`World::ground`) raised the land but not the beds the water's set on, so
  every stream in hard rock ran in a slot; it now fades in as the land rises a
  cliff over its bed. A stream crossing another's fan or a pond's sill sinks
  (`STREAM_SINKS`). Banks over 2 blocks 37% → 8% of water edges. The last
  cliffs were the fade's first blocks (65 536 blocks over half a lattice: a
  gentle valley's channel still lowest past its reach climbed it sheer); it's
  now a ramp, its square to `FADE_SLOPE` a block a block, `FADE_TO` only in its
  last tenth. Steps over 8 blocks 73 → 20, over 32 3 → 0, worst 61 → 12; a
  range's p90 slope 19.5° → 26.6° (steeper ramps stood as walls round heads).
  Measured with `seams` and `ponds` (scratch) on TF20.Y3gyMzlqNHY at 100k.
- **P5, branching to the finest.** A level of side valleys more
  (`FINEST_BLOCKS` 48 → 24, `MOST_LEVELS` 7): 46 blocks apart at 100k, 31 on
  the default world, where hillsides were planes. What a channel drains is
  counted over the levels as they were (`AREA_FINEST_BLOCKS`), so the coarser
  keep their slopes. Phase 4's hard-rock lift and the rolls fade by
  `Read::clear` (the least of each channel's hillside and rise over its bed),
  not the land over the owner's bed, which jumped where owners changed: steps
  over 4 blocks 75 → 9 before the new level, 16 with it; over 8, 4; worst 19.
  A fine level's fade ramp is at least `FADE_LEAST` high (its band is a few
  blocks). A range close up's p99 slope 59° → 68°; 10% more a column, a 250k
  world's first run levels 33 → 37 s. The gully filter isn't read under the
  drainage land: the network carries it all. Horton ratio still 3–5.
- **P6, ranges to the grid, lock in.** Finer channels climb from the trunk by
  their own slope with nothing holding them to the grid, so where no trunk
  runs a whole range stood over the grid's heights. Ground over the grid's
  surface by e now stands e·E/(E + e) over it (`OVER_GRID` E = 300 blocks,
  `Channels::toGrid`, before the beds), one rising map for land, beds and
  water, so seams and banks are untouched (steps over 4, 14; over 32, 0;
  worst 19; banks 24 of 304). At 100k: the most-rising belt's top 455 → 321
  (the noise land 253), the peak region's median 615 → 607 (537), its p90
  slope 31° → 24.5°. E = 150 brought the belt's top to 274, at the cost of
  the peak's slopes (p90 21°). Tried, not kept: capping each channel's head
  at the grid (the peak region came out mesas, and lowered channels won past
  their windows: steps over 32 0 → 11, worst 75). Golden re-pinned
  (1517453345); `check:land` passes, 40/40 tests, six far worlds at 100%; a
  headless boot is clean.
- **Tried, not kept:** varying rock strength and adding older relief in the
  grid's erosion: runs meeting rose 16% → 18% at most, every planet's rivers
  changed for it.
- **Measures** (scratch, to move into `scripts/terragen-check/`): `seams`
  walks transects and counts steps over 4 blocks in a tenth of a block;
  `owners` and `LEVELS=1` regions show which level owns the land. A
  regularity number (structure-tensor coherence) didn't separate natural from
  combed land and was dropped: repetition needs a spectral measure.
- **Open:** summits come down (P6) but a broad range's median still stands
  over the grid's (607 against the noise land's 537): bringing it down
  without flattening slopes means valleys cut under the grid, which the
  drawn rivers and lakes stand on; columns 77 → 91 µs.

## Checks made quick

The land's checks had grown to ~4½ minutes a change. `bun check:land` runs
check:terragen, its unit tests and check:far's six worlds side by side once
the engine's built (~1 min warm, ~1¾ after a terragen change); `--quick`, the
probe and the default world alone (~25 s). Under it: `testChannels` read every
run's profile on one thread (50 → 9 s, on every core), `testCoarse`'s columns
likewise (12 → 2 s); terragen's standalone tools build from kept objects
(`scripts/terragen-tool.sh`: 18 → 2 s after a change); kept grids are named by
a hash of what grids are built from, not the engine's build, so a change to
how columns are read (channels, soil, cover) keeps them; check:far's worlds run
together. Found on the way, both real: the emerge threads were marked active
only after all had started, so a block generated meanwhile ran its
`on_generated` with no mapgen (`get_mapgen_object` refused: an area never
generated, under load); and two processes writing one kept grid shared a
temporary file name.

- **Open:** a world's first read of its runs' levels after a generation change
  (`World::profileRuns`) is slow on large planets: 29 s at 250k radius, 14 s at
  1087f2009. A fresh read builds ~16 500 channel nodes (≈1 µs each) as each
  level's window asks for its neighbours' coarser nodes; P1's ±3 windows
  (343 lattice cells to 125) doubled it. Players pay it once per world per
  build; the checks after every change.

## Phases

1. **Seamless land.** Every channel that could be the lowest at a place is
   in its window, and a channel's pull fades out before its window ends, so
   the land is continuous by construction. Target: no pairs over 32, and the
   rectangles gone.
2. **Bend and vary.** Channels bend more than a quarter of their lattice, and
   the lattice's spacing and jitter vary by region, so neighbouring valleys
   fan and converge rather than run parallel.
3. **Ground decides the texture.** Valley spacing and depth by rock, rain and
   slope; hard rock deflects channels.
4. **Gentle land rolls.** Lumps and hollows at hill scale under gentle land;
   tarns only where ice worked, at a believable density.
5. **Branching to the finest scale.** The network carries the small side
   valleys; the gully filter only textures the last kilometre.
6. **Lock in.** Golden re-pinned under TF20, `check:terragen`, `check:far`,
   headless boot, record here.
