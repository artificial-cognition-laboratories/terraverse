# Sprint 120: land with a history

Every place on a planet gets a history, and its land comes from that history. Variety
should emerge from the history, not be painted on. Africa is the reference: shields,
sagging basins, swells, rifts and escarpments, each looking different. Today one look,
foothills, covers every continent.

## Where we are (read 2026-11-20, TF24.dGVycmE)

- **Continents start as one blob each.** `history()` (`grid/history.cpp`) makes every
  continental cell `Formed::Craton`. Its height is a level plain from the smooth
  `landmass` field, which gives smooth coastlines and a uniform interior from step 0.
- **There are few plates, with simple edges.** There are 6 + continents plates, each
  given to its nearest centre through one warp of 0.3. The result is long, gently bent
  boundaries.
- **Provinces are labels, not causes.** `provinces()` (`grid/geology.cpp`) names cells
  after the fact. On the default world almost all land is Shield (the provinces render).
  A province changes fertility and rock but hardly changes relief. `Amplifier` (column
  relief) is RELIEF · k_s · cell in every octave, everywhere.
- **Ranges fill areas.** Collision raises everything within COLLIDE_REACH = 9 cells, so
  ranges are country-sized blobs.
- **What we keep:** the Cortial-style plate history, the implicit stream-power erosion
  (Braun & Willett, `grid/erosion.cpp`), climate, the river network, and the geology
  columns.

## The idea

A cell's history is a record of what happened to it: when it formed, what kind of crust
it is, each time it was squeezed, stretched, buried or uplifted, and how long it has worn
since. Three properties are derived from that record and drive the land:

| Property | From | Drives |
|---|---|---|
| uplift (now and recent) | collisions, rifts, swells, rebound | how high the land stands |
| strength (erodibility k) | rock type and age | how sharp or soft its relief is |
| age since last uplift | the event record | how worn it is: peaks, hills, or a peneplain |

The stream-power erosion then runs on varied uplift and k, rather than one k on one plain.
That is where variety emerges: hard old cratons stay flat, soft basins fill, young belts
grow sharp ranges, and escarpments retreat.

## Phases

1. **Provinces from the history** (see it first). Record events per cell in `History`:
   crust kind, events, and their ages. Derive provinces from them, adding new kinds:
   - craton interior;
   - platform cover;
   - intracratonic basin;
   - swell or dome;
   - rift and failed rift;
   - young and old orogen;
   - foreland basin;
   - passive margin;
   - active margin;
   - large igneous province;
   - suture.

   Measure each province's land share on 4 codes. Target: no province above 35% of land,
   and at least 6 present on every Earth-like world.
2. **Erosion driven by the history.** Give k, uplift and age per cell to `streamPower`
   in `build()`, and let the column relief (`Amplifier`) take its amplitude, wavelength
   and grain from the province and rock, instead of one RELIEF.
   - Measure local relief per province (p50 and p90 of the height range in 10 km).
     Target: shields and basins under 100 m, old orogens 300–800 m, young ones over 1500 m.
   - This replaces the sprint 116 red test on 10 km relief.
3. **Ranges as belts.** A collision raises a narrow belt (2–3 cells) along its front, with
   ridges running along the boundary. Add a foreland basin on the lower plate and a
   plateau behind only for long, fast collisions. Belts are measured by aspect ratio:
   length over width above 5.
4. **Plates and seams.** Use more and uneven plates, grown from scattered seeds at
   different rates, and keep the old boundaries inside continents as sutures. Later
   rifts and ranges prefer to form on sutures.
5. **Coasts by process.** Make the coastline in the history, not from one field:
   - continents break up along rifts, leaving gulfs and peninsulas;
   - the sea drowns river valleys (rias) on sinking margins;
   - fjords where the land was glaciated and high;
   - headlands on hard rock and bays on soft;
   - deltas where rivers carry heavy sediment.

   Target: a coastline fractal dimension of 1.2–1.3 at continent scale, measured on
   the grid.
6. **A landform catalogue as data.** Each named landform is a row: the province, rock
   and climate it forms in, and how the column builds it. Examples: inselbergs,
   mesas and buttes, cuestas, karst, cones and calderas, rift lakes, alluvial fans,
   dunes, badlands, moraines and drumlins.

### Phase 1, built

- `History::Cell::past` (`Past`, geology.h) records a cell's history:
  - steps since a plume last lifted it, and since it was last stretched;
  - whether a suture runs through it, whether a plume flooded it with basalt, and
    whether flat beds cover it.
- `grid/past.*` holds the intraplate history:
  - Continents start as 48 sphere-wide terranes. Their seams are sutures inside 2-cell
    old belts, and half of them carry a platform cover.
  - 8–14 fixed mantle plumes. Each lasts 10–40 steps. A plume's head floods basalt and
    opens three rift arms (the arms that don't open become failed rifts). Its swell holds
    the land up while it lasts, and land carried off it becomes a sag basin.
- `grid/setting.*` holds `Setting`, 18 kinds, each with its province in `SETTINGS`.
  `provinces()` now reads it. It is shown by `render … settings`, which prints each
  setting's share of land, and tested by the "Settings" test.
- Heights and rivers are unchanged (the same river count on every code). Rock, fertility
  and vegetation follow the new provinces.
- Measured on 5 codes:
  - the largest share is 32.6% (arc, on the archipelago);
  - 11–13 settings per world;
  - swells and flood basalts vary by world (0–3%), as plumes do.
- **Open for later phases:**
  - Arcs and young belts together cover a third of the land (Earth's is about 15%).
    Phase 3 narrows them.
  - The hotspot volcanoes (`volcanoes()`) don't yet use the plumes.
  - The code version stays TF24 until the sprint lands. Bump it then.

### Phase 2, built (erosion by setting)

- Each row of `SETTINGS` says how its land is made: `still` (its share of the slow rise
  young crust gets), `rise` (its own rise: a swell, a margin's shoulder), `sink` (its
  sag: sags, forelands, failed rifts), `hard` (how readily it wears) and `worn` (how far
  it was levelled before the history: toward the broad lie of the land, over 12 rounds
  of averaging).
- `build()` reads the settings before erosion (from the base heights) and labels them
  again from the eroded heights after. The noise-placed basins are gone: settings sink
  instead.
- Measured on TF24.dGVycmE (median height, then median / p90 drop to the lowest
  neighbour). Before → after:

  | Setting | Before | After |
  |---|---|---|
  | platform | 150 m, 39/256 | 89 m, 12/94 |
  | sag | 119 m, 78/467 | 30 m, 8/117 |
  | craton | 485 m, 127/465 | 498 m, 112/479 |
  | swell | 599 m, 152/333 | 1330 m, 271/643 |
  | old belt | 232 m, 74/422 | 391 m, 204/638 |
  | young belt | 1008 m, 399/1473 | ~same |
  | arc | 1238 m, 522/1384 | ~same |

  The ordering now follows the history. Cratons still keep relief from the starting
  continents, which the next phases' plate edges should change.
- Lakes: default 16 → 312 cells (about 2% of land, Earth's share); the supercontinent
  2628 → 1994; the archipelago 44 → 0 (its sags sink to the sea and drain).
  `LakesAreFlat` now needs lakes across the worlds, not on each.
- Fixed in `route()`: a closed basin whose pass the lake downstream rises over is
  drowned in that lake. Before, two touching lakes could stand at different levels.
  More lakes exposed it.
- Test changes:
  - `ColdAndDryWater` now checks that frozen ground feeds no runoff of its own (its
    comment's rule). A river from warmer ground may cross a frozen sag.
  - The caves test compares limestone with all other rock, not granite alone.

### Phase 3, built (ranges as belts)

- The history's uplift reaches 4 cells into the upper plate (was 9), 2 into the lower
  (was 5), and 4 for arcs (was 7, peaking 2 in).
- Settings:
  - A belt is now 2 cells in from a converging edge on the upper side and 1 on the side
    going under (was 4 on both).
  - Only ranges that rose within 8 steps (120 Myr) are young belts; older ones are old
    belts (was 24 steps).
- Erosion's `UPLIFT` is raised from 11/2000 to 15/2000. Narrower belts held less high
  ground, and this restores it.
- Noise-placed sags are rarer (over 0.40, was 0.30).
- The shape measure is the scratch probe `belts.cpp`. Each connected young belt or arc
  on land gives its width as 4 · cells / edges outside (hexagons) and its length as
  cells over width. On 4 codes:

  | | Before | After |
  |---|---|---|
  | Width | about 6.5 cells | 3.2–4.0 cells |
  | Length over width (median) | about 7 | 8–12.5 |
  | Share of land | 27–38% | 13–22% |

- Plateaus cover 0.6–2% of land. Land over 2 km covers 5–15% (Earth about 11%).
- Fixed in `World` (the column): the fine hillside detail is no longer added under a
  stream's water. On steep ground it dug the bed up to 17 blocks under the water
  (caught by `ranges.cpp`'s stream check).
- Still to do: ridges don't yet run along a belt (the ridge noise is isotropic); fold
  trains do follow the front.

### Phase 4, built (plates and seams)

- 9 + continents plates (was 6 + continents).
- Plates grow from their centres, each at its own pace (0.3 to 1), by Dijkstra over
  ground whose roughness varies ±60% (`past::grow`). Small plates come out compact and
  edges wander. A plate's spin is cut to 35–100% by its size, so small plates don't
  trail their own new floor round the world in strips.
- First tried: a power diagram (nearest centre less a reach). Small plates came out as
  crescents and strips round big ones.
- Continents go to plates a terrane at a time (`past::terranes`, split out of
  `inherit`), so at the start every plate edge on land is an old seam, and continents
  part along them.
- Measured with the scratch probe `plates.cpp`, on 4 codes:
  - Plate sizes before: 3–22%, fairly even. After: one or two of 12–26%, a middle
    group of 4–13%, and 3–5 micro-plates under 2%.
  - Plate edges on land that are old seams today: 24–28% before, 24–37% after.
- Land is reshuffled. The default world (TF24.dGVycmE) is now a low old-continent
  world: 81% of its land under 500 m, 2% over 2 km. The others have 40–57% under
  500 m and 9–16% over 2 km. That's variety by seed, kept as is.
- Test changes:
  - The soil test compares organic matter for soils made where they lie (`!laid`).
    Warm swamps now lay peat, as the Congo's do.
  - The streams patch in `ranges.cpp` takes the most uplifted land a stream crosses.
    The most uplifted cell was a dry island peak.

### Phase 5, built (coasts by process)

- First, `history.cpp` (427 lines) was split: the plates' motion each step (crust
  handed between cells, gaps opened into floor, slivers tidied) is `grid/drift.*`.
  Renders and stats were byte-identical on 4 codes before and after.
- `grid/coast.*` cuts the coasts on the base heights, after the climate and before the
  rivers (`build()` has one call to it). Two processes:
  - **The sea comes in** over low ground it reaches, where the setting lets it
    (`SettingRow::sea`): forelands, sags, rifts, failed rifts and passive margins fully,
    platforms half, below 200 m. This makes gulfs and inland seas (the Persian Gulf,
    Hudson Bay, the Red Sea).
  - **Waves wear** coastal cells weaker than they are strong (Sapoval et al. 2004,
    damped erosion). Strength comes from the cell's own rock, its terrane's, the lie of
    the rock, its setting's `hard`, and its height. A cell's waves scale with how much
    sea lies within 4 cells, so a bay shelters itself and the wearing stops (in under
    10 rounds). The Coast setting sets their strength.
  - The sea's level then moves so the land's share stays exact (by rank, so equal
    heights at the shore split).
- Measured with the scratch probe `coast.cpp`: box counting over the coast cells of
  landmasses of 600+ cells, in boxes of 2 to 16 cells. A smooth shape scores 1.05 (the
  hexagons' staircase).

  | Code | Before | After |
  |---|---|---|
  | TF24.dGVycmE (low, old) | 1.14 | 1.28 |
  | TF24.YmZ2bmVmZzQ | 1.19 | 1.34 |
  | TF24.YWJj (arcs) | 1.12 | 1.15 |
  | TF24.a3U3eWd1Y2c (high) | 1.06 | 1.09 |
  | supercontinent | 1.12 | 1.15 |
  | archipelago | | 1.18 |

  Low, old worlds reach the target; worlds whose coasts are arcs and young belts stay
  smooth. That matches Earth, where Chile's and Japan's ocean sides are straight and the
  Atlantic's drowned margins are not. That's variety, kept as is.
- Not done at grid scale: rias and fjords (5–50 km wide, under a cell) belong to the
  column, in phase 6's catalogue. Deltas need the erosion to build land at a river's
  mouth, inside `build()`'s erosion loop (`grid.cpp`, 938 lines). That waits until
  `grid.cpp` is split.

### Phase 6, built (the landform catalogue)

- First, `grid.cpp` (943 lines, `build()` alone about 500) was split by stage, each its
  own file with one call from `build()`:
  - `crust.*`: the base land from the history, and the impacts;
  - `wear.*`: the glacial scour and the erosion rounds;
  - `water.*`: evaporation, runoff, standing water and the river threshold;
  - `fertility.*`;
  - `surf()` moved into `coast.*`.

  `grid.cpp` is now 260 lines. Renders and stats were byte-identical on 4 codes
  (relief, settings, rain, sediment) against the pre-split source.
- `column/landform/` (a module; its door is `landform.h`) holds the `LANDFORMS`
  table. Each row gives the settings the landform forms in, its rock (hardness),
  rain and warmth bands, whether it needs limestone, its shape, its wave and height in
  blocks, and its share of the ground. One builder (`Landforms`) reads every row.
  `World` owns one and adds its height in `surface()`, off the streams.
- Sizes are in blocks, as the dunes' are. A planet is scaled down by R / R⊕ across but
  only √ of that up, so at a body's own metres an inselberg would be a block wide.
- Seven rows, built from six shapes (dome, cone, tower, table, scarp, ridged):

  | Landform | Forms in | Climate |
  |---|---|---|
  | inselbergs | craton, swell, old belt, suture; hard rock | seasonal, over 10 °C |
  | mesas and buttes | platform, flood basalt, sag, failed rift | dry |
  | cuestas | platform, margin, foreland | any |
  | tower karst | limestone platforms and basins | wet, over 18 °C |
  | badlands | foreland, sag, rift; soft rock | dry |
  | cinder cones | rift, plume swell, flood basalt | any |
  | drumlins | low old ground | cold now (under ice then) |

- Measured with the scratch probes `forms.cpp` (share of land per row) and
  `formimg.cpp` (each row's own height at its strongest site). Each row covers 0–5%
  of land per world. Tower karst forms on one of the 4 codes, because it needs wet,
  hot limestone. The "Landforms" test checks that no row covers more than a sixth of a
  world, and that all but 2 rows form across the Earth-like codes.
- Not yet in the table: the dunes, dolines, folds, canyons and deltas still in
  `world.cpp` (1863 lines). They move in one at a time, and `world.cpp` shrinks as
  they do. Drumlins aren't yet aligned to the ice's flow, and cuestas form rings (a
  basin's rim) rather than following a dip direction.

Every phase changes generation: bump the code version, update the goldens, and run
`bun check:land`.

## References

- Cortial et al. 2019, *Procedural Tectonic Planets* (the history we already follow).
- Braun & Willett 2013, the implicit stream-power scheme (FastScape), already in use.
- Cordonnier et al. 2016, *Large Scale Terrain Generation from Tectonic Uplift and Fluvial
  Erosion*: uplift and erosion as the source of relief.
- Burke & Gunnell 2008, *The African Erosion Surface*: Africa's basin-and-swell landscape.
- Mandelbrot 1967 and Richardson: the coastline dimension used as the target.
