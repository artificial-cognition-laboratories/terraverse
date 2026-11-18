# Sprint 116: Land by the book

The terrain has been grown by trial: a hand-rolled erosion, then a workaround
for each thing it got wrong, then constants tuned by eye. It's spent many
sprints and still doesn't converge. This sprint replaces it with published,
tested algorithms, each checked against a reference model, and deletes what
they make unnecessary. Nothing already built is kept for its own sake, and old
worlds will break (accepted).

**The aim:** the most plausible planet generator we can make, fast enough to
rerun while a player moves the land settings. Correct first, then fast.

## What the oracle found

`bash scripts/terragen-oracle.sh <code> out.png` runs fastscapelib (GPL-3: a
local reference only, fetched into `.output/oracle`, never shipped or copied)
on the grid's own cells, from the same land, uplift and erodibility. On
TF24.dGVycmE:

- **River profiles are wrong.** Stream power says slope ∝ area^−0.5 (the
  reference gives −0.57); terragen gives **−0.25**.
- **Drainage disagrees more than height does.** Heights after 12 rounds match
  (r = 0.986), but only 45% of cells drain to the same neighbour and 42% of
  river cells are shared: the random-walk receivers and the fill's jitter.
- **The mountains aren't a balance.** Run to equilibrium, the current uplift
  and erodibility wear the land to a 2.4 km top and 95% lowland. Today's ranges
  stand because erosion stops after 12 rounds, leaving the history's heights.

## Decisions

1. **Mountains are an equilibrium.** Where the plates push land up, it stands
   where uplift balances erosion, so a range's height and shape come from its
   plates' speed and its climate. Old land wears down for the time since it
   last rose. The plates' history supplies uplift *rates* and crust, never
   final heights.
2. **Every algorithm has a reference.** A paper, and where one exists a
   permissively licensed implementation (Landlab, MIT) or the oracle to diff
   against. GPL code is read for its API at most, never copied.
3. **Fixed point stays.** Determinism is non-negotiable. The ports are written
   in our fixed point, and the oracle checks them in floats.
4. **`Grid` is the contract.** Its fields (`eroded`, `filled`, `receiver`,
   `drained`, `runoff`, `lake`, `lake_level`, `sediment`, `placer`) are what
   the far view, mapgen, rivers, weather, flora and the menu read. Phases 1–4
   replace what fills them, not their meaning, so the rest of the engine
   doesn't churn while the core is rebuilt. A field is renamed or removed only
   when its meaning really changes.
5. **Correct, then fast.** No performance work until phase 7, except not
   making it gratuitously slow.
6. **Nothing is committed until every phase is done** (the release bot ships
   master nightly, and the land between phases is worse than either end).
   One version bump (TF25) and one rebake of the goldens then. Tests a phase
   leaves failing are listed under it and pass again by the phase that owns
   what they check.

## Phases

Each phase has a gate. It isn't done until the gate passes and what it makes
redundant is deleted.

### Phase 0: the oracle (done)
`scripts/terragen-check/oracle.cpp`: fastscapelib on our icosphere through a
small grid adapter, compared with terragen on hypsometry, concavity, heights,
receivers and rivers, plus a stacked PNG.
- **Next:** make its comparison a check with thresholds (`bun check:oracle`),
  on three or four codes with different land settings, not just the default.

### Phase 1: the erosion core
A new `terragen/erosion.*`, one concern: given land, uplift, erodibility and
rain, evolve it.
- **Routing:** steepest descent on the sphere's cells (no wander, no jitter).
- **Depressions:** Cordonnier, Bovy & Braun 2019: the basin graph and its
  minimum spanning tree route every pit to its outlet without filling (the
  same method as the oracle's `mst_sink_resolver`).
- **Stream power:** implicit, Braun & Willett 2013, m = ½, n = 1, rain-weighted
  area, spatially variable K.
- **Hillslopes:** linear diffusion, implicit or stably sub-stepped.
- **Gate:** with the oracle's parameters, the same receivers on ≥ 98% of land
  cells and heights within a block or two of RMS; concavity −0.5 ± 0.1.
- **Cut:** `fill` + `jitter`, `receivers(…, wander)`, `wear()` in
  history.cpp, the round loop in `build()`, `KEEPS_PACE`, `RISING` /
  `RISE_ROUNDS`, the explicit creep and its crumbs.

**Outcome.** `terragen/drainage.*` (steepest descent, Cordonnier's basin tree,
carving) and `terragen/erosion.*` (implicit stream power, conservative creep),
used by `build()` and the history. `bun check:oracle` runs the core and
fastscapelib in lockstep on four worlds with water: after 12 rounds 98% of
receivers agree and heights are ~9 m apart; after 200, heights within ~12 m
(r > 0.999) and concavity within 0.05 (−0.57 against −0.56 on the default).
Calls made on the way:
- **Lakes are base level** (`Lakes::Base`): a river wears down to a lake's
  surface, and nothing under a lake wears. fastscapelib wears toward the bed
  and erodes depressions' floors; the oracle runs the core in its convention
  (`Lakes::Bed`) so the two solve the same equations.
- **Coasts drain to the sea's surface**, not its floor, so how deep the sea
  is never picks which way a coast drains.
- **A pass shared by several edges** (a divide's high cell) spills to the
  side that falls furthest. fastscapelib chooses otherwise; on a dry body,
  closed basins everywhere, the two wander apart, so the gate is held on
  worlds with water.
- The gate's receivers are held early (98% at 12 rounds): fixed point
  against floats flips near-ties on flat lowland, and over hundreds of rounds
  the runs drift there while heights stay together.
- `fill` keeps its epsilon (Barnes's priority-flood) for the finished water
  surface; only its jitter went.

What it exposed (tests skipped in `scripts/test/skip.txt`, each until the
phase that rebuilds what it checks):
- **Lakes and salt pans** mostly came from the sinking-basin noise landing
  on plains the old erosion had left dead flat. Real profiles have a gradient
  the 5-block sag doesn't close, so the default world's lakes fell from 210
  cells to 115 and TF24.YmZ2bmVmZzQ's from 78 to none, pans with them
  (`testColdAndDryWater`). Phase 4.
- **Ranges are walls again**: without the rising-under-rivers workaround the
  belts stand whole from the first round, and big rivers go round them.
  Plateaus no longer read far smoother than ranges (`testRanges`). Phase 2.
- **The far view's coarse reader invents sea** on one fresh coastal plain
  (column 17 blocks up, the 256-block tile −7; `testCoarse`). Phase 6.

### Phase 2: uplift and crust
Decision 1 made real: the history yields an uplift-rate field and a crust
(continental or oceanic, thickness), and the erosion core runs to
equilibrium where it's rising.
- **Reference:** Cortial, Peytavie, Galin & Guérin 2019 (*Procedural
  Tectonic Planets*) for collision and subduction uplift and its reach into
  each plate; Cordonnier et al. 2016 and 2023 for uplift-driven terrain and
  for steering it with settings.
- **Isostasy:** thick crust floats high (plateaus, continental platforms) as
  a base level, not a post-hoc remap.
- **Tune:** uplift and K so that on an Earth-like code belts reach 5–8 km,
  most land stands under 500 m, and the land's hypsometry falls within
  Earth's. Wetter belts stand lower, as they should.
- **Gate:** hypsometry and peak heights in range on the test codes; relief
  tracks uplift; Land, Mountains and Continents settings still mean what
  they say.
- **Cut:** `PLATFORM_RISE`, the shield wearing (`SHIELD_M`, `SHIELD_WORN`),
  the plateau levelling passes, the sea-floor smoothing passes where isostasy
  covers them, `UPLIFT` as a per-round fudge.

**Outcome.** `bash scripts/terragen-tool.sh /tmp/h scripts/terragen-check/hypsometry.cpp`
reports a code's land in Earth's bands (`<code>:mountains=1` moves a setting).
The default world against the Earth:

| | <200 m | <500 | <1k | <2k | <3k | >3k | mean | top cell |
|---|---|---|---|---|---|---|---|---|
| Earth | 28% | 24 | 21 | 16 | 6 | 5 | 840 m | ~5.5 km |
| TF24.dGVycmE, before | 34 | 36 | 19 | 6 | 2 | 3 | 544 | 4.4 km |
| TF24.dGVycmE, now | 28 | 24 | 20 | 16 | 7 | 5 | 831 | 5.6 km |

The three other test worlds: means 730–830 m, tops 4.7–5.3 km. Mountains 0 →
1: mean 389 → 1252 m, top 4.0 → 7.1 km. Humidity −1 → 1: mean 976 → 672 m
(rain wears). What made it:
- **Rising land starts from its platform** and the belts (the history's
  recent uplift) push it up for 300 rounds, each `STEP` (4) of the old:
  uplift, erodibility and creep all scale, so where they balance doesn't
  move. The belts are within ~1 m a round of balance by the end.
- **Isostasy** (`rebound`, erosion.h): land rises back 0.82 of what's worn
  off it, over three rounds of neighbour averaging. It's what keeps old land
  standing through the long run.
- **The glacial buzzsaw**: erodibility rises to 5× where the yearly mean is
  12 °C under freezing. Without it a dry, fast belt stood a massif 8.8 km
  high (cells' mean); with it ranges top out near the Earth's.
- **The Mountains setting** scales uplift (×0.5..1.5) as well as the
  history's push: the belt is capped, so alone it hardly moved the land.
- The platform and shield remaps are gone: without them the hypsometry is
  the same and shields stand as low and smooth (median 225 m, steps of 45 m
  to a neighbour, against 218 and 43). The plateau levelling stays: it's
  thick crust floating high, which isostasy of erosion doesn't give.
- A dry body's ancient erosion keeps its old pace (its rounds aren't STEP).
- Making a grid takes ~2.8 s (was ~0.9): phase 7.

Found on the way:
- Concavity of the land straight out of erosion is 0.42–0.59; the finished
  land reads 0.6–1.2, raised by the sunk basins and filled hollows after it
  (phase 4).
- Range flanks drain by parallel transverse valleys a cell apart: real
  ranges have regularly spaced ones (Hovius 1996), but a cell apart is the
  grid's limit (phase 6).
- `testSoilByWater` already fails at HEAD (12 floodplain samples, needs 20),
  before this sprint; floodplains are phases 3 and 6.

### Phase 3: sediment
- **Reference:** SPACE (Shobe, Tucker & Barnhart 2017; Landlab's
  `SpaceLargeScaleEroder`, MIT) for bedrock and sediment together: rivers
  carry what they can and lay down the rest, building floodplains, fans,
  deltas and the shelf. Yuan et al. 2019 for the implicit form.
- **Placers** stay: the heavy-grain share of sediment flux, settling first.
- **Gate:** mass balance exact (uplifted = eroded + stored + to the deep);
  sediment where it should be on renders (basins, river mouths, piedmonts).
- **Cut:** `CARRY`, the per-round lake silting, the hand-rolled load pass.

**Outcome.** Stream power still wears the land implicitly (stable at the
long rounds; SPACE's own solver is explicit in slope); what it wears comes
off a cell's loose sediment first, and the load is carried heads first and
laid by the ξ–q law (Davy & Lague 2009): a cell passes on Qs / (1 + G·a/q)
(Landlab's SpaceLargeScaleEroder, MIT), G = `DEPOSIT` 0.25. A lake takes all
that reaches it till it's full (no more silting by an eighth); the sea
builds its shelf. `Grid::sediment` is the layer as it is now, worn as well
as laid (it was every grain ever laid: after 300 rounds, 16 blocks on every
cell), and placers the heavy share of it.
- Splitting wear and carry, a steep gully lays part of its load each round
  and wears it the next. Off the rivers that's load in transit, not
  alluvium, and isn't counted; on a river it's the bed and floodplain it
  always floors its valley with, and is. 18% of the land carries some, 2% a
  block or more.
- Deposition steepens beds by about (1 + G), so uplift came down (11/2000 ×
  STEP) and the buzzsaw up (×9): default 32/22/18/15/7/7%, mean 860 m, top
  cell 6.8 km.
- Phase 2's creep scaled with STEP, reaching ~10 cells over the run: it had
  been wearing plateaus down 1.2 km. It now reaches about a cell, as it did.
- Tried and dropped: scaling the buzzsaw by snowfall (a glacier needs snow,
  so dry plateaus would stay unglaciated). The dry belts lost their cap and
  stood 12 km.
- Tests: soil, coasts, sediment balance, placers and soil by water pass.
  `testRanges` fails: rivers from the sea now cut kilometre-deep gorges back
  into plateaus, leaving 3 cells of interior on its world, rougher than the
  ranges. A real plateau's interior stands because it drains inward (Tibet
  is endorheic and dry): phase 4's water balance.

### Phase 4: lakes, water balance and ice
The basin graph from phase 1 already knows every lake's spill level.
- Which basins hold water is the water balance (inflow against
  evaporation): keep the policy from `keepRealLakes`, rebuilt on the basin
  graph. Closed basins become salt lakes or pans by climate.
- Climate and land in a loop: orographic rain depends on the land, erosion
  on the rain. Iterate a couple of times, and measure whether it converges.
- Glacial erosion where it's cold: a simple ice-flux K (U-shaped troughs,
  fjords, overdeepened lakes) replacing the scour noise, if the literature
  gives one cheap enough; otherwise keep scour as is.
- **Gate:** lakes match the water balance; `rivers/` and `server/lakes.cpp`
  run unchanged on the new grid.
- **Cut:** `keepRealLakes` as a separate routing, `SINK` / basin noise if
  closed basins now arise from tectonics and climate.

**Progress.**
- `route()` takes a water balance (`Balance`): from the basin tree's leaves
  in, each basin's lake fills from its pit, its open water giving back what
  evaporates; one that gives back all that reaches it before the pass is
  closed, its pit where its water ends. Runoff is rain by the Turc–Pike
  curve (a Budyko curve: ET = P / √(1 + (P/E)²)). The erosion runs on it
  every round: a closed lake is its basin's base level and keeps all its
  sediment.
- Without anything renewing them, closed basins don't last: 38 at the
  run's start, none by round 50 (filled, or cut into from outside). The
  sinking-basin noise is now a subsidence rate through the run
  (`BASIN_SINK`, 5% of `BASIN_DEPTH` a round), not a sag after it.
- The finished water comes from the same routing and balance
  (`standWater`): lakes where water stands a block deep, shallower hollows
  filled, closed basins' wet-year floors as pans, `filled` the surface with
  a hair of fall along each way. `keepRealLakes` (and its size rules) and
  the priority-flood `fill` are gone.
- Lake cells: default 16, TF24.YmZ2bmVmZzQ 35, the supercontinent test world
  2628 (6% of its land), TF24.Y3gyMzlqNHY none (one pan). More subsidence
  gives more lakes but sinks the lowlands (at 8%: 54 lakes on the default,
  39% of land under 200 m, against Earth's 28%).

**Open.**
- Plateaus: rivers from the sea still cut their interiors (`testRanges`).
  The balance closes basins that are dry, but the climate gives the
  plateaus 0.66 of typical rain, and cold lakes give back little: Tibet's
  dryness (a rain shadow, strong evaporation at altitude) isn't in the
  climate. The climate–land loop (below) is where to look.
- The climate–land loop, and glacial erosion beyond the buzzsaw.
- The finished land's concavity reads 0.8–0.9: flat filled basins.

### Phase 5: hand-over to the world
Everything that reads `Grid` checked on the new land: mapgen, the far view,
the planetarium, flora, soil, rivers, weather and the menu preview.
- **Gate:** `bun check:land`, `check:far`, `check:wiki` (Worlds scenes
  replay the new erosion: `bun palette`), the goldens rebaked, and walking
  the default world in the game.

### Phase 6: below the grid
The grid's cells are 36–250 blocks across, so everything finer is
procedural. `channels.cpp` and the landform half of `world.cpp` (~3000 lines)
are the other half of the trial and error. Split by job:
- **Streams that carry water** stay a real network: trunk valleys follow the
  grid's receivers; finer channels branch under them (Génevaux et al. 2013),
  sized by hydraulic geometry, with slope from the stream-power steady state.
  Kept small: network, discharge, width, slope.
- **Hillsides that look eroded** come from an erosion-filter amplifier: one
  pure function of the grid's slope and flow direction that carves dendritic
  gullies and ridges at every scale (runevision's erosion filter and Phacelle
  noise: reimplemented from the description; the published code is partly
  CC BY-NC-SA). Seamless by construction, so the window and fade-band
  machinery goes.
- Fans, troughs, washes and cirques return only as profiles on the remaining
  channels, if renders show they're missed.
- **Prove first:** prototype the filter in `render.cpp` / a region render
  against today's land before deleting anything.
- **Gate:** region renders and walking in the game look at least as good;
  seamless across windows (`check:far`); per-column cost no worse.
- **Cut:** most of `channels.cpp`; `gullies`, `folds`, `crags`, the run
  shaping and profiles in `world.cpp`.

**Prototype.** `PROTO=1 SIZE=628320 region <code> out.png belt 60 512`
(scripts/terragen-check/region.cpp, `amplify`): the grid's surface,
smoothed over half a cell, gullied by the filter, in floats on the
region's lattice, set beside the game's columns. What it took so far:
- Depth from the grid's own slope was far too little: a cell's average
  slope is gentle even in a range; the relief is inside it. Stream power at
  its balance says a drainage L across falls k_s · cell / L, so each octave
  adds about RELIEF · k_s · cell (k_s = S·√A from the grid's cell and its
  receiver), held where its wave would pass a hillside's critical gradient.
- The octaves' own grooves steer the next but mustn't deepen it: compounded
  over a dozen, heights reached millions of blocks.
- Gullied down to a few blocks it's fractal noise; stopped at a hillside's
  length (400 m) it reads as dissected country.
- `gridHeight` is linear over the grid's triangles: its facets show, and
  the gradient that steers the gullies jumps at their edges. The real thing
  needs a smooth base, point by point.
- Smaller octaves get less, × (L / the top's)^0.35: the big valleys
  organise the small, as drainage does. Held at a hillside gradient of
  0.35, the range's slopes are p50 27°, p90 44° (real ranges: 25–30°);
  RELIEF 0.6 (at 0.5 cap the RELIEF hardly mattered: the cap held).
Open: coastal lowland dissected nearly as hard as the range (k_s blurred
over half a cell doesn't fall off fast enough).

**Built in.** `terragen/amplify.*`: the filter in fixed point, a pure
function of the place. Its lattices are cubes in three dimensions (no seam
on the sphere), each wave measured across the slope in the place's own
tangent plane; the grid's surface and k_s smooth between cells by a
Wendland kernel over the cells within 1.2 spacings (no facets, and a slope
everywhere). 42 µs a column (the drainage land's: 250). Seen in the
planetarium: dissected ranges, snow on their cores, gullied flanks.

**The default, and the cuts.** Every body with water stands on it; a dry
body's land and the sea floor keep their noise. Gone:
- `TERRAGEN_LAND` and the drainage land (sprint 108): `channels.cpp`
  2086 → 422 lines. It keeps the lattice of small streams, their bends and
  the valleys they cut (`Read`: relief, cut, the stream); trunk pieces,
  shapes, paths, wandering, ponds, fans, cirques, troughs, washes, beds,
  rolls, the grid pull and the window and fade machinery went.
- `world.cpp`: gullies, alpine crests, crags, fold ridges in the land (the
  folds still bed the strata), drowned valleys and fjords, the slope-read
  biomes (snow off steep ground, rock above the trees), the valley swing.
- `check:streams` (it scored the drainage land's streams).
- Soil reads the cover's slope, the channels' relief, and till wherever
  glaciers ran (`glacial()`), no longer the drainage land's.
Missed, to come back on the amplifier if renders say so: rias and fjords at
the coast, rock and snow by local slope, scree.

### Phase 7: speed
Aim: a settings change shows its land in about a second.
- **Steady state directly.** At equilibrium with n = 1 the land is a single
  upstream pass over the receivers (h = h_down + (U/K)·A^−½·d: Cordonnier et
  al. 2016), iterated only until the receivers stop changing. That may
  replace thousands of rounds in the belts. Old land still needs its time.
- Coarse to fine: solve on a coarser icosphere, then refine.
- Rebuild only what a setting touches: climate alone shouldn't redo plates.
- `bun perf:gen` before and after each change.
- **Gate:** the oracle still passes; interactive preview time measured on a
  low-end and a fast machine.

## Not in this sprint
- A new plate simulation (Cortial's full model). Phase 2 takes uplift from
  the history we have, and that's revisited only if the uplift field is the
  weak link.
- Coasts and waves as erosion, karst, wind-blown landforms.
- Anything at block scale finer than phase 6's network and filter.

## References
- Braun & Willett 2013, *A very efficient O(n), implicit and parallel method
  to solve the stream power equation*.
- Cordonnier, Bovy & Braun 2019, *A versatile, linear complexity algorithm
  for flow routing in topographies with depressions*.
- Cordonnier et al. 2016, *Large scale terrain generation from tectonic
  uplift and fluvial erosion*; 2023, *Large-scale terrain authoring through
  interactive erosion simulation*.
- Cortial et al. 2019, *Procedural Tectonic Planets*.
- Shobe, Tucker & Barnhart 2017, *The SPACE 1.0 model*; Yuan et al. 2019.
- Génevaux et al. 2013, *Terrain generation using procedural models based on
  hydrology*.
- Code: fastscapelib (GPL-3, oracle only), Landlab (MIT).
