# Sprint 107 — Old land, real weather, living cover

Follows sprint 106 (the land). Three ideas carry it: most land is old and
flat, rain is carried by the wind, and vegetation is read off continuous
fields, never chosen by biome. Fjords and sixteenth-block terrain close it.
One world-code bump (TF18) covers every generation change; the golden is
re-pinned once at the end, and `bun check:terragen` runs after each phase.

## Phases

1. **Crust age.** Each plate region carries an age: cratons (shields, the
   oldest cores), platforms (sediment laid flat over them), young belts. Read
   from the plate history, warped like the rest. Render overlay `age`.
2. **Old land is flat.** Relief scales with age and uplift: cratons worn to
   gently rolling ground, platforms nearly level, ranges only where the crust
   is rising or was folded recently (old belts low and rounded: Appalachians).
   Gullies only on rising ground. The column's fine detail too, so on foot the
   plains aren't mounds. Target hypsometry: most land under ~500 m. Kills the
   ribbing in the elevation view.
3. **Rivers that hang together.**
   - Wide rivers: runs drawn as straight slabs meeting at corners (bends with
     sharp edges); the bowing has to carry the banks, not just the centre.
   - Tributaries meet the main river at its bank, every time.
   - Small channels are one network with the grid's rivers: they start at a
     source, run downhill, and end only in a bigger channel, a lake or the sea
     (now they wander, run straight and stop at random).
4. **Moisture carried by the wind.** A grid pass: winds by latitude (trades,
   westerlies, the equator's rising band), vapour picked up over sea, rained
   out where air rises (windward slopes, convergence), re-evaporated from wet
   land, lakes and forest, so interiors downwind of wet land stay wet (the
   Amazon). Two seasons, so monsoons and dry summers fall out. Rain shadows
   and dry ringed basins stay. Target: about a third of land dry.
5. **Soil water.** A wetness index from what drains to a place and how flat
   it is: riverside strips green through deserts, bogs in hollows, dry ridges.
6. **The fields.** Per column, continuous: moisture (rain over evaporation),
   warmth (growing season), seasonality, soil water, salt, soil texture and
   depth. Terragen's biomes become labels derived from them (map, wiki), and
   ground materials (sand, clay, peat, salt crust) are read off them.
7. **Vegetation structure.** Lua turns the fields into cover: tree cover,
   canopy height, undergrowth, ground cover, leaf type. Species declare
   tolerances (cold, drought, shade, waterlogging, salt) and compete for
   cover where they fit; no species knows a biome. The planetarium draws the
   cover. A wiki page with the life-zone diagram (moisture × warmth), live.
8. **Fjords** from the grid: glaciated coastal cells cut under the sea there,
   so the far view and the game agree.
9. **Sixteenth-block terrain.** Design pass first (map format, mesher, the
   mapgen writing cut ground), then build.

## Water that shapes the land (replaces phase 3's leftovers; with 5, 8, 9)

Today the land is made and the water painted on it: a river's level is the
grid's, lerped, and the ground raised into banks to hold it (streams on
embankments). Turned round (Génevaux et al. 2013, hydrology first): one
network, every scale, and the land fits it.

W1. **The planetarium draws the network**: water by flow, held to a visible
    width (a tile only showed water as wide as itself; most isn't).
W2. **Long profiles**: each channel's bed falls from source to mouth; the
    ground beside slopes down to it, never banked up to hold it.
W3. **Steps**: hard beds across a channel hold its profile up: falls,
    rapids, and a small lake behind the sill.
W4. **Drowned valleys**: valleys reaching the sea carry on under it (coves,
    rias, estuaries); fjords are the glacial case (phase 8).
W5. **Small lakes**: tarns and kettles (ice), oxbows (meanders), pools.
W6. **Springs and fine water**: where a valley head meets the water table
    (phase 5), a source the fine water runs from. Design pass first.
W7. **Sloped water surfaces**, with sixteenth-block terrain (phase 9).

## Not in it

The procedural trees themselves (phase 7 places stand-ins by tolerance until
they come), flint's texture, placers and bog iron (carried from 106).

## Done

- **Crust age** (`Tectonics::stable`, geology.cpp): 0..1, how long the crust
  has stood still: last rose 6–28 steps ago, nothing rising now, 4–12 cells
  off a converging or spreading edge; smoothed over 10 rings. Render overlay
  `age` (violet still, orange young); region pick `shield`.
- **Old land worn flat**:
  - a shield barely rises (`belt` 0.03 against a young land's 0.18), so the
    grid's erosion wears it down;
  - what stands over 300 m on it is 55% gone (`SHIELD_M`, `SHIELD_WORN`): old
    belts low and rounded;
  - its bumps 60% smaller (`SHIELD_SMOOTH`, world.cpp);
  - basins sag as before (read against a young land's rise, or the shields'
    basins sank into inland seas).
  Land under 500 m: 69 → 76% (default seed 496rukkb), 36 → 41% (yamvpshx, a
  seed of ranges), 58 → 60% (hxbx5we5). A shield at 628 320 blocks round:
  7 blocks of relief over 30 km.
- **A closed lake never stands over its basin's rim** (grid.cpp
  keepRealLakes): a huge flat basin's evaporating lake rose past its spill,
  stranding the water beside it (`testWaterDrainsToSea`).
- World code **TF18**; golden 3548419286; palette re-baked.
- **Rivers that hang together** (phase 3):
  - A run's line is found, not guessed: the nearest point on the bowed and
    swung line by a few Gauss–Newton steps from the run's own (straight
    across the run was further where the line crossed it steeply: the river
    swelled and pinched, and runs met in points).
  - Each drawn run's shape is worked out once (`World::RunShape`,
    `shapeRuns`), and `World::onRun` says where its line passes.
  - **Creeks**: a run with a quarter of a river's flow is drawn too
    (`World::drawn`, `CREEK_SHARE`), 2–5 blocks, its meanders sized as an
    8-block stream's at least (a creek's own were a sawtooth).
  - **Streams join up**: a channel carries water only if what it runs into
    does, down to a drawn run (`Channels::Node::wet`); a first level's foot is
    on the run's line (`onRun`); each level is bent with the coarser levels'
    bends and its own fades out at its foot, so it ends where it joins. The
    levels' own wander (stream 359) is gone: the channels are read in the
    rivers' warp alone. `testChannels` checks the water chain.
  - Cost: a column about 10% dearer (creeks).
- **Moisture by the wind** (phase 4): the march was already there (winds by
  latitude, the sea's vapour, rain where air rises, land giving back its
  rain, two seasons, monsoons). What was wrong:
  - **Beach was a third of the land**: any land under 150 m was sand, so the
    worn shields went yellow (the "deserts" in the planetarium). A beach is
    now only by the sea (`World::bySea`: the sea within a cell or so).
    496rukkb: beach 42 → 14% of land samples (ground() keeps it to the strand).
  - Rain fell in bands straight across land and sea. The land's summer heat
    now lifts the air three times as hard (`THERMAL` 5 → 12), so a continent
    breaks the dry belt (monsoons, wet east coasts); storms and fronts mix
    vapour over 8° (`EDDIES`, a blur each pass; the old mixing reached a
    cell); some rain falls from any air (`DRIZZLE` 0.5 → 1.5%).
  - Land, 496rukkb: desert 6 → 13% (Earth's hot deserts ~14%), rain's
    quartiles 0.22 / 5.96 of the median → 0.19 / 1.89.
  - Not done: lakes don't give vapour back (the climate runs before they
    exist); rivers through deserts are phase 5.
- The golden (TF18.YWJj…) didn't move with phases 3 or 4: it doesn't pin the
  rivers or the rain. Worth a look.
- **W1, the planetarium draws the network**: on a far tile (`World::Coarse`)
  a run's drawn once it drains `SHOWN` (16) cells of rain per (tile in
  cells)², never less than a river (creeks lie parallel on smooth slopes:
  a herringbone), at least a tile wide; a channel below the grid once its
  runs are `STREAM_TILES` (24) tiles long.
- **W2, long profiles** (`World::profileRuns`): the ground's sampled along
  each drawn run's line (9 points, `landAt` at `unwarped` places, in
  parallel); node levels a block under their ground and no higher than any
  run in (upstream first); each run falls between them, under its ground.
  Ground over a run's water is cut back to walls rising 1 in 2 from the bank
  (`VALLEY_WALL`). Rivers standing over dry ground within 6 blocks (block
  scale, 40 spots): 496rukkb 5.9% by 10.2 blocks → 3.1% by 1.4; 95xp92rj
  13.5% by 13.6 → 11.0% by 2.1 (what's left: streams on slopes). A world
  costs ~0.13 s more to build; TestTerragen 14 → 28 s.
- **W3, falls**: where a run's profile drops at least `FALL_LEAST` (4)
  blocks between points, no gentler than 1 in `FALL_GRADE` (25), over rock at
  least `FALL_HARD` (0.45, the province's `HARD`, less what's buried in
  sediment), the drop is one step at a lip (hashed, a quarter to three
  quarters along): level water above, a plunge pool a third of the fall deep
  below, the walls cut back to the lower water (a gorge). Soft rock or a
  gentler drop stays a ramp (rapids). h5r5hbmp: 7 of the 30 steepest spots
  fall 4+ blocks (up to 37). Profiles: a lake or the sea standing higher backs
  the water up to its level; never under the sea's. `testChannels` checks
  every profile falls and meets the next. Far tiles' rivers 0.8 of a tile wide.
