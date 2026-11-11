# Sprint 112: the surface, by process

Finish the land before plants: plants and the biomes that come from them will
read climate, soil and landform, so what's wrong in those is baked into them.
Every step here is a generation change (world code TF22, not yet released).

## Order

1. **Cold and dry water.** Ice sheets feed no rivers; salt pans sized by water. *(done, below)*
2. **Rivers by regime.** Braided, meandering or straight by slope, load and bank
   soil (sprint 111's soil gives bank strength); point bars inside bends, cut banks
   outside, not sand on every bank; stream density by rock and rain (limestone
   sinks its streams, shale and clay drain densely); drainage patterns by geology
   (dendritic, trellis in folded beds, radial off volcanoes, rectangular on jointed rock).
   *(braiding and cut banks done, below; density and patterns by rock still to do)*
3. **Ice carving.** Cirques, U-valleys, arêtes; fjords where carved valleys meet the sea.
4. **Ranges as carved valleys.** Ridges are what's left between valleys, not mounds.
5. **Coasts.** Beaches from sand supply and wave energy; dunes from sand, dryness
   and the climate's winds; cliffs.
6. **Volcanoes by magma.** Shields and lava plains (basalt), steep cones
   (andesite), calderas; worn by age.

## 1. Cold and dry water (done)

- **Meltwater only** (`grid.cpp`, `melted`): the final drainage and lakes count
  rain × the share that melts: all of it where the yearly mean at the cell's height is
  over −5 °C, none under −15 °C (an ice sheet, which flows away as ice). Erosion
  is unchanged. TF22.dGVycmE: river cells 3815 → 3222, all lost at high latitude;
  a cold world 3917 → 1474.
- **Salt pans by the wet years** (`keepRealLakes`): a closed basin's lake floods up
  until evaporation balances its inflow (as before); its pan is where the flood
  carries on to `PAN_WET` (3) times the inflow, never past the rim, and the rest
  of the floor is dry ground. Was: the whole floor up to 3 blocks over the lake.
  TF22.YmZ2bmVmZzQ: pan cells 259 → 144, the largest 190 → 81.
- A closed basin with almost nothing coming in keeps its lowest floor as pan,
  so every land cell's water still ends somewhere (`testWaterDrainsToSea`).
- Test: `testColdAndDryWater`.

**The grid cache's key was missing every `.cpp`.** `terragen/CMakeLists.txt` set
`terragen_SRCS` in the parent scope only, so the hash that names kept grids
(`terragen_grid_sources.cpp`) covered only the headers: a change to `grid.cpp`
reused the old grids, in the game and in the tests alike. It's set in both scopes
now (20 sources hashed). Tests run before this against grid changes read stale grids.

## 2. Rivers by regime (part)

- **Braided rivers** (`braids()` in soil.cpp; `RunShape::braided`): a drawn run
  braids where its slope passes Leopold and Wolman's line for its flood flow,
  drawn ten times higher (the grid's runs fall over a cell, steeper than a
  reach; at the textbook line half of every world's rivers braided), and at a
  third of the slope under glacial meltwater. A braided run is `BRAID_WIDER` (3)
  times as wide and swings a quarter as far; its bars are islands at the water's
  level, value noise laid along the run (`BRAID_LONG` 4 times longer than wide).
  About one run in nine braids (TF22.dGVycmE: 886 of 8007).
- **Cut banks**: `Ground::river_bend` carries the side of the bend. Soil lays the
  bank's coarse deposit only on the inside (and along a braided river); outside,
  the bank is the floodplain's silt it cuts into. No sand line down both banks.
- Golden re-pinned: 3302989925.

Stream density by rock and rain was already there (channels.cpp DENSE_SOFT,
DENSE_WET), and so was trellis drainage on folded beds (TRELLIS). Tried and
dropped: rectangular drainage on jointed rock (a join cost off two joint
bearings, shield and highland): even at 12× it moved nothing visible, since
what's seen as water there is the grid's runs, and joints only steer the
finer dry valleys. Limestone sinking its streams waits for karst.

## 3. Ice carving (part)

Troughs (U-valleys), tarns at cirques and drowned valleys were already there
(sprint 109 P6). Two changes:

- **Valleys under ice sheets came back.** Step 1 had zeroed `drained` under ice,
  and the channels carve valleys by `drained`: glaciated land lost its valleys.
  Now `Grid::runoff` (meltwater) decides where rivers are drawn (`river()`,
  `stream()`, `drawn()`), and `drained` (all of it) still shapes the land.
- **Fjords reach further.** `World::m_sea_cells` (steps to the sea); an ice-cut
  trunk valley drowns up to `FJORD_CELLS` (4) cells in, over land to
  `FJORD_BAND_M` (2000 m), rather than one cell over 600 m.
- **The far view and narrow inlets.** A far tile wider than an inlet averaged the
  land and sank the whole tile with the valley in it: a 4096-block tile drew a
  hillside as sea. Now a far tile drowns valleys only when it's no wider than an
  inlet (`INLET_WIDE`, 320 blocks: a trough's floor at its widest), so tiles near
  enough draw fjords and farther ones leave them out, as they leave out narrow
  valleys. `testCoarse` now says that: a far tile never invents sea; at 4096
  blocks it may leave out an inlet (1 of 400 columns does); at 256 blocks it
  agrees exactly.
- **Cirques** (`channels.cpp`, `Shape::cirque_r`): on the level whose channels are
  a cirque's size (`m_tarn_level`, the tarns'), a channel head the ice worked is a
  bowl: a near-level floor from `CIRQUE_FROM` (55%) up the channel, `CIRQUE_SHARE`
  of its length across (24..220 blocks, by the ice), under a headwall
  `CIRQUE_WALL` (45°) steep. No wider than the channel climbs, so gentle cold
  ground gets none. Arêtes aren't built as such: where bowls meet, their walls do.
- The default world (`TF22.dGVycmE`) is too small for channels below the grid, so
  it gets no troughs, cirques or fjords; they show on larger planets. Even at
  Earth's size the ranges round them are smooth mounds: step 4.
- Golden unchanged (1719416001): its region has none of these.



## 4. Ranges as carved valleys (part 1)

The smallest planet (16 384 round, the default) had no drainage land at all:
its grid cells are 36 blocks, and the channels' finest level stopped at 24
blocks (half a cell, 18, was under it). Its ranges were the grid's heights
between cells, a smooth dome under noise and gullies, ~100 blocks high, the
rivers stopping at its foot.

- **`FINEST_BLOCKS` 24 → 12**: the smallest planet gets one level of channels,
  18 blocks apart, and the drainage land (sprint 108) shapes it: the dome becomes
  a crest with side valleys cut down both flanks. Larger planets gain a finer level
  of side valleys too. Columns on the smallest planet 3.5 → 5.3 µs (`perf:gen`
  SIZES=16384); making it is unchanged (0.9 s cold).
- **Beds and rolls by the planet's height scale** (`m_small`, `TUNED_SIZE`):
  tuned on 100 000-round planets, they're scaled down on smaller ones, whose
  heights are all less (blocksPerMetre). Larger planets unchanged.
- `testSoil`'s steep against flat now reads the ground's own slope (four blocks
  either way), soil made where it lies (not what water laid), and away from the
  ice's till: by the grid's slope, with channels on the smallest planet, it
  compared the wrong columns.
- Golden re-pinned: 4140243162.

Still to do in 4: the flanks are low and the side valleys too regular (a comb);
spurs and ridges branching from the crest; at Earth's size the ranges are still
smooth mounds (sprint 110's OVER_GRID cap and the hillsides' slope).

## 4. Ranges as carved valleys (part 2: peaks)

A transect of the smallest planet's range: the grid stood at ~90 blocks, the
ground at 55–85 under it, hillsides rising 10–20 blocks between valleys. The
cause: a planet's heights shrink as √ of its size and its breadth as all of it,
so its land is exaggerated (×49 on the smallest), but hillsides stood at the
Earth's critical slope (0.55) on every planet. Ranges rose like exaggerated land
and were grooved by Earth's hillsides: a broad rise, no peaks.

- **Hillsides as steep as the land is exaggerated** (`m_steep`, `STEEP_MOST` 4):
  `critical()` × (1 + (1/metre − 1) × how fast it's rising), at most 4×. A plain's
  hills stay as they were; a rising range's stand up to 4× steeper (≈66° at
  most). The Earth's size is unchanged (relief in 10 km, median 1761 m).
- **Peaks free of the grid's mean** (`RANGE_OVER` 6): in a rising range ground over
  the grid bends toward it 6× further off. Alone it changed nothing (the
  hillsides never reached the lid); with steeper hillsides the ridges stand over
  the grid's height and the valleys under it.
- Transect after: ground swings 84–123 across the crest about the grid's 89; the
  range's top 104 → 140.
- Golden re-pinned: 1580458716.

## 4. Ranges as carved valleys (part 3: rivers through ranges)

No large river crossed a rising belt on any world measured: erosion started
on the land with its ranges whole, and the drainage went round them.

- **Ranges rise under rivers already running** (`grid.cpp` `RISING`,
  `RISE_ROUNDS`): each land cell's height times its belt (not a plateau's) is
  taken off the land erosion starts on and given back over the first 8 rounds.
  Counted as uplift, so the sediment's balance holds.
- **Big rivers keep pace** (`KEEPS_PACE`, 20 cells of rain, about three times a
  river's least): such a cell doesn't rise with the range, and what stands over
  it is the gorge's walls. By wearing alone the grid's rounds were too coarse (a
  river wears a quarter of its fall a round, the range rose a tenth of its height;
  it pooled and spilled aside), and the rise alone changed nothing.
- Rivers in rising belts +20–40%, the largest 4 → 10–16× a river's least on most
  worlds; on the planet map ranges are broken by river gaps where they were walls.
  Peaks as high as before.
- Placers ~10% fewer, the richest 0.21–0.23 (was 0.22–0.26): `testSediment`'s
  "rich" line is now a fifth (it was a quarter, one world just over it).
- **Valleys along a rising range** (`channels.cpp` `STRIKE`): the trellis pull of
  fold belts (joins across the belt's contours cost more) now in any rising
  range at half strength: its thrust sheets are hard and soft by turns. A modest
  effect in renders. Tried first at the grid's scale (soft bands wearing faster
  along the contours): a range is only a few cells across there, and it changed
  nothing; dropped.
- Golden re-pinned: 900378371.

## 5. Coasts (beaches, dunes; cliffs as they were)

Cliffs on hard coasts were already there (sprint 97, `hardness()`, `CLIFF_M`).

- **Winds kept** (`Climate::wind`, `Grid::wind`): the two seasons' mean surface
  wind per cell, as a vector in the planet's frame.
- **Waves** (`Grid::surf`, grid.cpp `surf()`): on a coast's land cells, 0..1:
  the fetch (sea cells `FETCH` (8) spacings upwind, by the wind against the
  sea's median) and `SWELL` (35%) by how open the cell is to the sea. Bays and
  lee shores are calm, open windward coasts rough (p10 0.06, p90 0.53).
- **Beaches by waves**: the strand rises `BEACH_TOP` + `BERM` (4) × surf blocks
  (a storm berm on open coasts). A sheltered shore (`CALM`, 0.12) where a
  river's silt reaches is mud, not sand: the land's own biome at the water.
  About 25–30% of the low shore is sandy beach on the worlds measured (the
  Earth's: ~31%, Luijendijk 2018).
- **Dunes** (`World::dunes`, `Ground::dune`): ridges across each grid cell's
  wind, blended between the three cells round a place; a gentle stoss
  (`DUNE_STOSS` 80% of the wave) and a steep lee, crests swayed by noise.
  Blocks, not metres (sand's own physics): `DUNE_WAVE` 96, up to `DUNE_HIGH` 10.
  Ergs in the desert biome only (steppe's grass holds its sand), where it's
  dry past −wet 0.3, on sand rivers laid or soft rock (basins, rifts) in
  noise patches; shields stay stony desert. About 40% of the desert is erg.
  Foredunes behind beaches with surf over `DUNE_SURF` (0.35), up to 8 blocks
  over the strand. A far tile wider than half a wave lays the field's mean.
  Mapgen makes a dune its top block all through (sand, not sandstone).
  A dune a river, lake or bank later covers loses the flag.
- `GRID_SIZE` 1208 → 1304. Golden unchanged (900378371): its region has no
  coast or desert. Test: `testCoasts`.

## 6. Volcanoes by magma

- **Shape by magma** (`coneProfile`): basalt's runny lava builds shields,
  broad and convex (1 − x², hotspots, `wide` 12 → 16); andesite's sticky lava
  steep concave stratocones ((1 − x)², arcs); rift fissures between
  ((1 − x)^1.5, `wide` 6 → 10).
- **Calderas** (`Volcano::caldera`): `CALDERAS` (25%) of arc volcanoes' tops
  fell in: a third of the radius across, a flat floor under walls
  `CALDERA_WALL` of it thick, a quarter as deep as wide. A young hotspot's
  summit is a pit crater over its chamber.
- **Age** (`Volcano::age`, 0..`VOLCANO_AGES` − 1): arcs and rifts get one at
  random, hotspots their chain's; only the youngest erupt. Older is lower
  (arcs to 5/8), its crater filled, its flanks cut by radial ravines: a
  noise read by the way from the vent alone, `RAVINES_BLOCKS` (160) apart
  round the mountain, up to `RAVINE_DEPTH` (half) of the flank's height at
  the oldest, none at the summit; left out of far tiles wider than a ravine.
  Lid worlds' giants and paterae unchanged.
- Not done: lava plains (flood basalts) round shields and rifts, crater lakes.
- Golden re-pinned: 1178319120. `testVolcanoes` checks calderas, ages, and
  that only the young erupt.

## Follow-ups from the in-game shots

- **The sandstone "pillars" were its texture.** All three Good Vibes sandstone
  faces are dressed masonry (bricks with carved columns), so every exposed bed
  read as rows of posts. `tf_world:sandstone` (the rock) is now the
  sedimentary rocks' grain (limestone's) tinted sand; the masonry is
  `tf_world:sandstone_bricks` (4 from 4 sandstone, in the building kit).
  Palette re-baked. A scan for one-column spikes on a 100 000-round planet
  found 15 in 106 000 columns: the ground itself wasn't it.
- **Crags** (`World::crags`): two octaves of noise `CRAG_WAVE` (24) blocks
  across, up to `CRAG_HIGH` (3) blocks either way, on hillsides from slope 0.3,
  full at 0.8; left out of far tiles.
- **Bare steep desert** (`Ground::bare`): a desert hillside past sand's repose
  (`SAND_STEEPEST` 0.6) shows its filler (sandstone) on top. No desert on the
  worlds measured is that steep yet (they're plains), so it's latent.
- Golden re-pinned: 3674840680.

## Lava plains, crater lakes, karst

- **Lava plains** (`floods()`, `lavaReach()`, `Volcanic::plain`): a young
  basalt volcano (hotspot or rift, age 0–1) spreads flows past its slopes to
  `PLAINS_REACH` (3) of its radius: `PLAINS_LOBES` (10) lobes round it (noise
  read by the way from the vent), thinning to their ends, up to a quarter of
  its height thick (3..16 blocks; half once quiet). The youngest's is bare
  (Rock biome). Its lava-field province (basalt) and the grid's reach lists
  reach as far. ~1% of land on the worlds measured.
- **Crater lakes** (`Volcanic::pool`): a quiet crater or caldera at least
  `POOL_LEAST` (4) deep where wet > −0.2 fills to `POOL_UNDER` (30%) of its
  depth under the rim. ~40% of quiet volcanoes on land.
- **Karst** (`World::karst`, `dolines`): limestone platforms in humid
  (wet > 0.15) basins, in noise patches; their beds mostly limestone
  (75–95% of the ground's top rock there, was mostly shale); streams under
  `KARST_STREAM` (6 blocks) wide sink (dry valleys), the grid's rivers keep
  running; dolines `DOLINE_WAVE` (48) blocks apart up to 6 deep. 1–9% of land.
- `World::volcanic` returns a `Volcanic`; `karst()` is public.
- Golden re-pinned: 3597410694. Test: `testCraterLakesAndKarst`.
