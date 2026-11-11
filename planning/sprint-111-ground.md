# Sprint 111: the ground

The land's shape is in decent order (sprint 106–110). Now what it's made of.
The ground becomes a true mixture, made by the processes that make real
ground, so that where things are follows from how the land formed: clay on
floodplains, sand on bars, gravel in upland channels, gold and diamonds below
their sources. Knowing the land is how you find things.

## What exists (sprint 19)

- One soil block, `tf_world:soil`. Its `param2` is one of 198 points: sand and
  clay in tenths on the texture triangle, with silt making up the rest, at
  three organic levels. These are defined in `terragen/soil.cpp` and named by
  the USDA classes in `tf_world/soil.lua`.
- `World::soil` picks a point from four things:
  - province texture,
  - a warm/wet climate nudge,
  - a bank or alluvium nudge,
  - slope.
- Its matter is quartz plus kaolinite plus cellulose, 55% solid. Dug soil
  keeps its point.
- `fill/` holds liquid in a node's open space. That's what water in soil is.
- `rocks.json` gives each rock by its minerals. Rocks blend toward a
  neighbour over 16 `param2` steps.
- The render tool now has a `soil` mode, used for the baseline below. The
  baseline shows:
  - soil by province and climate only,
  - stair-step latitude bands,
  - no trace of the rivers.

## What's missing

1. **Grain size as part of matter.** A mixture knows quartz but not whether
   it's a boulder, sand or dust. Sorting, sieving, milling, placers and ore
   grade all rest on this.
2. **Gravel and stones** in the soil. The triangle has none.
3. **Mineral identity by parent rock.** Today all sand and silt is quartz.
   Basalt should give dark grains and iron, granite quartz and grus.
4. **Sorting by water.** Grain size by stream power, the drop where power
   falls, and floodplain fines with distance from the channel.
5. **A profile:** topsoil, subsoil, weathered rock (grus/saprolite), bedrock,
   with depths from weathering against slope.
6. **Water as state:** each soil holds what its texture lets it, wetted by
   rain and dried by sun.
7. **Looks:** grains and lumps drawn by composition, not just a tint.

## Decisions

- **True mixtures.** The inventory carries what was dug: shares by grain
  size, minerals, organic matter and water. Processes (sieve, wash, mill,
  fire) split and change it. Nothing is a named "clay item".
- **Composition in the data, a few blocks on screen.** One soil family with
  colour and grain from the palette. The block shows its make-up: fine and
  smooth for clay, granular for sand, lumps for gravel coloured by what
  they are.
- **Water is fill, not identity.** Mud is wet clay or silt. Treading,
  draining and drying all move real water.
- **Live soil water.** It's put in only as far as the weather needs now; the
  weather overhaul comes later.
- **Depletion waits for farming.**
- **Plants are prepared for, not built.** The soil exposes the axes a plant
  trait reads: water held, drainage, nutrients, depth, coarseness.

## Phases

1. **Grain size in matter.** A material is mineral amounts split across size
   bands (boulder, gravel, sand, silt, clay), plus organic matter and water.
   It's loaded from data, and mass is kept across splits.
   - Ground truth: how real rocks break down.
     - Granite: quartz stays sand, feldspar becomes clay, mica becomes silt.
     - Basalt: clay, plus iron oxides.
     - Limestone: dissolves.
   - Native supplies the mechanism; the tables are data.
2. **Soil from processes (terragen).** Replace `World::soil`'s nudges with:
   - weathering (parent minerals × climate → products, rate),
   - depth (production against slope),
   - hillslope fines into hollows,
   - **river sorting** (largest grain carried from flow × slope, deposits
     where it falls, channel → bank → floodplain by distance),
   - organic matter and peat.

   It's fixed-point and deterministic, needs a world-code bump and a golden,
   and is checked on the `soil` render.
3. **The profile in the generator.** The column's layers become blocks.
   Gravel bars, sand banks and floodplain clay appear in the world. The
   composition space grows to carry gravel and parent mineral.
   - `param2` is 8 bits, so the per-node point may need the matter
     system's state instead. This is the main open engineering question.
4. **Water in soil.** Rain wets ground through fills, sun and drainage dry
   it, and it's held by texture. Mud is what wet clay is (pile `yield` and
   `bearing` from water content).
5. **Rock and ore** (next sprint): host rock plus minerals by grain size;
   veins, magmatic, sedimentary; placers fed by phase 2's sorting.
6. **Karst** (after): limestone dissolved by rain along the water table:
   caves, sinkholes, springs, lost rivers.
7. **Plants by traits** (after): they read phase 2's axes.

## Checks to pin

- Floodplain soil has finer grains than the channel beside it, and the
  channel's grain gets finer downstream.
- Warm, wet granite land weathers to more clay than cold, dry granite land.
- Steep ground has thinner, stonier soil than hollows.
- Mass is conserved across every split (phase 1).

## Phase 1, built: grain size in matter

- **Data:**
  - `substances.json` minerals gain `weathers` with a `rate` in Goldich's
    order. Olivine is 1, then pyroxene, plagioclase, feldspar and mica;
    quartz, clay and hematite have none. Some also carry `into`/`per`:
    feldspar and plagioclase → ½ kaolinite, mica → 1½ kaolinite,
    pyrite → ½ hematite, magnetite → 1½ hematite. Calcite, olivine,
    pyroxene and apatite dissolve. `keep` (kg left a kg) is derived from
    the molar masses.
  - `rocks.json` rocks gain `grain`, their crystal or clast size in mm
    (granite 1.5, basalt 0.1, shale 0.004, gravel 20).
- **`matter/grains.{h,cpp}`:**
  - Size bands are stone, gravel, sand, silt and clay (USDA, plus stone
    over 64 mm).
  - `Grains` holds kg per substance per band, plus water.
  - Operations: `broken` (a rock at a band), `finer` (sieve or current),
    `share` (spadeful), `crush`, and `weather(extent, crystals)` (minerals
    alter into clay or dissolve; pieces fall apart to the crystal size).
    Every one keeps mass, and `weather` returns what was dissolved.
  - Queries: `texture` (triangle of the mineral fine earth), `stones`,
    `organic` (anything of carbon and hydrogen).
- **TestMatter `testGrains`:**
  - Granite weathered mildly is a sandy loam (sand > 0.6, clay 0.1–0.3).
    Deeply weathered, it's clay > 0.4 with quartz sand left.
  - Basalt makes more clay than granite.
  - Limestone loses over 90% to solution.
  - Young weathering is still mostly stone.
  - Every split keeps mass and water.
- **Not yet:** a Lua face (comes with digging in phase 3), and the
  fixed-point twin terragen will need in phase 2 (it reads the same data).

## Phase 2, built: soil from processes

- **`World::soil`** (`terragen/soil.cpp`) is rewritten. `Soil` gains
  `stones`, `depth` and `parent`.
  - **Weathering** of the column's own bedrock, as the closed form of
    `Grains::weather` in fixed point. Extent = 3.5 × 2^((T−15)/10) × rain ×
    time. Time is (½ + 2·stable), less where glaciers scraped, divided by
    (1 + 2·slope).
  - **Slopes:** fines wash off steep ground and ridges, and gather in
    hollows. Depth is 1–6 blocks by how far weathering has gone, thinner on
    slopes and ridges, 6 on what water laid.
  - **Water:** the nearest water is a grid river (`Ground` now carries
    `river_width/dist/level/slope/bar` from `ground()`) or a stream below
    the grid. Within a grid river's own channel, the river wins.
    - Beds take their band from Shields (D ≈ 13.5·depth·slope, depth =
      0.27·Q^0.4, in the planet's own metres). Gravel needs D ≥ 32 mm (the
      gravel–sand transition) and cobbles ≥ 128 mm.
    - Banks and bars are one band finer. Floodplains (within twice the
      river's depth of its level) are silt by the river, grading to clay.
    - Lakes lay clay, and ice leaves till.
    - Worlds without channels below the grid (cells under 48 blocks: the
      2k–16k atlas sizes) use the column's own river and its cell's banks.
  - **Organic matter** is plants over rot: rot doubles every 10 °C and
    nearly stops below freezing. Waterlogged clay flats and marsh make peat.
- **Data:**
  - Rocks gain `crumbles` (shale 4, sandstone and tuff 3, obsidian 0.5).
  - Kaolinite gains `grain` 0.001 mm: clay minerals come apart clay-sized.
  - Limestone carries 6% kaolinite, the residue that's terra rossa.
  - Terragen's mirror (`parentRock`) is held to the data by
    `testSoilMatchesMatter`.
- **Geology, for the soil to be honest:**
  - Sedimentary beds are mostly mud, as on Earth (shale 18 of 36 parts),
    shifting smoothly with climate (`Strata::sands/limes`).
  - The beds are level: the ground's height picks the bed, plus a regional
    depth. Before, one bed covered a whole world's surface.
  - Province edges wander half a cell, no longer the grid's straight edges.
- **Measured** (default world, 20 000 land columns):

  | Where | Sand | Silt | Clay | Stones |
  |---|---|---|---|---|
  | Hot and wet | | | 0.32–0.41 | 0.01 |
  | Temperate | 0.29 | 0.44 | 0.28 | |
  | Dry | | | 0.23 | 0.31 |
  | Lake floors | | | 0.60 (organic 0.29) | |
  | River beds | 0.86 | | | |

  On an Earth-sized planet, floodplain fines are 0.71 against 0.09 in the
  beds. Big rivers' beds are gravel on steeper reaches and sand on gentle
  ones.
- **Tests:**
  - TestTerragen: `testSoil` (climate, slope, lakes), `testSoilMatchesMatter`
    and `testSoilByWater` (Earth size). The golden is re-pinned at
    3928772032, the same on two runs.
  - TestMatter: limestone leaves clay.
  - `bun check:land` passes.
- **Cost:** strata plus soil take 2.2 µs a column over the ground's
  6.7–18.6 µs.
- **Known:**
  - Fold belts on big planets read one crest phase across the belt, so a
    belt is all sandstone with the belt fields' straight edges. That's
    geology's (sprint 101), to fix there.
  - The blocks still hold the 198-point triangle plus organic matter:
    stones, depth and parent rock reach the world in phase 3.

## Phase 3, built: the layers as blocks

- **Storage:** a soil block's name says what its grains came from and how
  stony it is. That's 4 parents × 4 stoniness classes, so 16 blocks, each
  keeping the 198-point fine earth in `param2`.
  - Parents (`terragen::SoilParent`): granitic (granite, gneiss, sandstone,
    tuff, obsidian, anorthosite), basaltic (basalt, andesite, breccia), limy
    (limestone), shaly (shale, slate).
  - Stoniness (`soilStony`): none under 15%, stony to 35%, gravelly to 60%,
    gravel above.
  - Names are `tf_world:soil_<parent>[_<stony>]`, each with a `mapgen_` alias.
    `tf_world:soil` (and `mapgen_soil`) means "this column's soil": the
    generator puts the kind in its place, and old worlds' soil reads as
    granitic.
  - Lua reads the kinds from the engine (`core.terra.soil_kinds`).
  - Matter per point: the parent rock as stones, its lasting minerals as
    sand and silt (quartz and feldspar; plagioclase and pyroxene; calcite and
    quartz; quartz), kaolinite clay (with hematite from basalt), and wood as
    the organic part.
  - Look: grey soil tinted by parent, with the palette's fine-earth colour.
    The stones are the parent rock's own texture through pebble masks
    (`tf_world_pebbles_1..3`, drawn by `palettes.py`), left untinted.
- **Layers** (`terragen::soilLayer`): the topsoil is the column's; the
  subsoil keeps ¼ of the organic matter (peat all of it) and +6% clay
  washed down; the last block over the rock is breaking rock (≥ 50%
  stones). What water or ice laid is one deposit, organic matter in its
  top only.
- **Generator:** where a biome's filler is the column's soil, terragen's
  depth replaces `depth_filler`. Riverbeds default to the column's soil:
  the sorted deposit, all of it, with no filler under it. Biomes with their
  own beds (marsh mud, salt crust, regolith) keep them, with their fillers.
- **Far view:** the kinds are in the block list, the baked palette (format
  3) and the horizon packet. Protocol 100, now also the least accepted
  (it was 89, so mismatched peers weren't refused).
- **Checks:**
  - `check:terragen` predicts the bed's soil kind and terragen's depth:
    43/43 columns agree.
  - `check:land`, `check:matter` and `check:palette` pass; the palette is
    re-baked.
  - Unit suite: all pass but TestAtlasRender, the same 2 failures as before
    phase 1.
  - TestFarPalette's cut test fails run alone (a tile 395 blocks off against
    a 400 allowance) and passes in the full suite. It's order-dependent and
    unexplained.
- **Known:** pebbles are flat-shaded blobs, worth an art pass. Desert,
  beach and sea-floor fillers are still the biomes' own blocks.

## Phase 4, part 1: soil on the globe

- **`World::regionalSoil()`:** the soil each grid cell's land mostly has, a
  field per share (sand, silt, clay, organic, stones) that `between` reads.
  It's from the grid alone: the cell centre's height, climate, rock, lake
  and river setting, with no streams below the grid. Made once, in
  parallel, on first ask: 20 ms at any planet size. Reading fine soil
  across a globe took 0.4 s at 192² and 320 s at Earth size.
  `World::soilWith` is the shared body; `soil` passes the channel read.
- **Painters:** the `soil` overlay is the same colour rule in both:
  - the globe (`planetarium/paint.cpp`, `soilColour`): sand pale gold, silt
    grey-brown, clay rust, organic darkening toward peat, stones greying;
  - the page (`ui/src/worldgen/fields.ts`, `soilColor`), from the
    preview's new `soil` field (4 bytes a pixel: sand, clay, organic,
    stones), offered on bodies with seas, with an icon.

  The render tool's `soil` mode draws it too.
- **What it shows:** climate (red clays where it's warm and wet, pale sand
  where it's dry, peat in cold wet basins) and geology (rings of sandstone
  and limestone soils round basins, where the level beds crop out at their
  heights).
- **Checks:** TestTerragen (the regional field keeps the climate's order
  and holds nothing at sea: clay 0.39 warm-wet, 0.22 dry), TestPlanetarium,
  and the UI's typecheck and build all pass. Not looked at in the client.

## Phase 4, part 2: water in the soil

- **Two tiers.** Settled: everywhere, stored nowhere. A soil block's pores
  hold what its texture keeps, as full as the weather says the ground is.
  Live: a woken block's mixture starts with that water as real matter, so
  heat dries it (a wet loam boils dry at 373 K) and it stays soil.
- **Where wetness comes from:** the weather's soil bucket, which was
  already stepped live (rain in, evaporation out). `fluid::Sample::soil`
  carries it and `core.terra.weather(pos).soil` reports it. `tf_matter`
  sets it per 16-block cube with `core.matter.wetness`, beside the ambient
  temperature. Ground beside standing water is soaked. Below freezing the
  pore water is ice.
- **How much:** `WorldMatter::Spec::holds` (0..1 of the pores) per kind;
  `core.matter.register_family` takes `holds`, one per point.
  `tf_world/soil.lua` gives each point its field capacity: Saxton and
  Rawls simplified, θ ≈ 0.08 + 0.40·clay + 0.20·silt + 0.5·organic, at
  most 0.45, less the stones.
- **Physics fixes it needed:**
  - `Physics::main` is now the biggest substance of whichever state takes
    more room (wet sand is sand, not water). Before, wet clay soil could
    have counted as water and poured away.
  - `settled` compares a porous block against its wet self, not its dry
    kind.
  - `WorldMatter::mixture` of a sleeping block is `asleep()`: its liquid
    fill and pore water too. Before, it left the fill out.
- **Checks:**
  - TestSandbox `testSoilHoldsWater`: dry none; wet 270 kg a cubic metre
    of loam; half as wet, half; beside water, soaked; heated, dried and
    still loam.
  - TestSandbox (22), TestMatter, `check:matter`, `check:lab` (5) and
    `check:wiki` (68) pass. A headless server's soil read 0, 190 and 380 kg
    of water at wetness 0, ½ and 1.
  - `check:moves` fails `testBonesStayOut`: the wall run sinks 0.23 against
    0.2. That's movement against a wall; not shown to be this work.
  - TestSandbox's step budgets fail when the machine's loaded and pass when
    it isn't.
- **Not yet:**
  - Mud: pile give and bearing by wetness. The client predicts piles, so
    it needs the wetness too.
  - Treading water out.
  - Soil dried by fire is held dry and doesn't take rain again.
  - Wet soil isn't drawn darker.
  - The weather's wetness hasn't been seen set in a live terra world.
