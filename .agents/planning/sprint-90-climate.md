# Sprint 90 — An energy-balance climate

The climate (sprint 13) already moves moisture along the winds and rains it
out where air rises, but:

- **it runs on a 128 × 128 latitude–longitude grid**, read back onto the
  sphere's cells: coarse, stretched at the poles, and smooth enough that
  biome edges come out as long straight lines (the deserts in the
  planetarium);
- **temperature is a formula** (`heatAt`: sunlight, a star and setting
  offset, noise, minus height), not a balance of energy: no ice that
  cools a world further, no continental interiors with fierce seasons and
  coasts with mild ones, no heat carried poleward.

## The model

**Energy balance** (Budyko 1969, Sellers 1969, North 1975; papers only):

- A zonal energy-balance model on equal-area latitude bands: sunlight
  absorbed (the solar model's insolation, less albedo) = heat radiated
  (A + B·T) − heat carried poleward (diffusion in sin latitude). Solved in
  fixed point for the year and each solstice season.
- Albedo from what's there: ocean dark, land brighter, deserts brighter
  still, cloudier toward the poles; ice and snow where it's below −10 °C,
  which cools further (the ice-albedo feedback), so a cold world is icier
  than its sunlight alone says.
- The star and the Temperature setting are forcings (W/m²), not degrees:
  the ice decides how far they go.
- Per cell, about the band: its own albedo, the ocean currents' heat, and
  **continentality** — the sea's heat capacity damps the seasons, so the
  swing grows from coasts into continental interiors.
- Height cools by the lapse rate, per column (as now).

**On the sphere.** Everything on the grid's own cells (64 002): the
moisture march goes semi-Lagrangian along the winds (where each cell's air
came from, read between cells), blurs and gradients over neighbours,
currents scanned along the parallels. No latitude–longitude grid.

**One answer for any date.** The grid keeps each cell's yearly mean heat at
sea level and how much it gains per unit of sunlight above the yearly mean;
a column on a date is those plus the lapse rate. `World::column` keeps its
signature; `heatAt` goes.

## Checks

- Earth's defaults: a global mean near 14 °C, the equator near 27 °C, the
  poles well below freezing.
- A colder star or setting: more ice than the forcing alone (feedback).
- Seasons: a continental interior swings more than a coast at its latitude.
- Rain tests (windward/leeward, monsoon, currents and coastal deserts) on
  sphere-built land.
- No climate field follows a latitude–longitude grid.
- Golden re-pinned; `bun check:terragen` (the weather's anchor), the
  rest. **TF9.**

## Not in this sprint

The live weather's own physics (it stays anchored to this). Clouds as their
own albedo field. Sea ice moving.

## Outcome

Before the climate, sprint 89's cleanup: the plates' history now goes on
the body's History timeline ("N million years ago": collisions, oceans
opening, arcs; `Grid::history`, the preview's `history`, `History.vue`'s
`land`), and the ocean floor is smoothed so floor of different ages
leaves no lines.

- `terragen/climate.cpp` rebuilt on the grid's cells. The 128² grid,
  `climateAt` and `heatAt` are gone.
- **Energy balance:** 90 equal-area bands, solved exactly (Thomas, 64-bit
  fixed) for the year and each solstice season, ice and temperature
  settling together (40 rounds, halfway each). A = 188, B = 2.09, D = 0.40
  W/m²/°C; albedo: sea 0.07, land 0.16, ice 0.42 (over −5 to −15 °C
  yearly), clouds 0.24 + 0.05·P₂. Each cell about its band: its own albedo
  (mostly mixed away: land and sea at a latitude differ by a degree or
  two), the currents' heat, and continentality (the seasons ×½ at sea to
  ×1¼ inland). The star and the Temperature setting are forcings at 0.4 of
  their old offsets; ice takes them about as far as the offsets went, further
  on the cold side.
- **On the sphere:** slopes over neighbours; blurs on a coarser sphere
  (frequency 20) read back between its cells; the moisture march
  semi-Lagrangian, each cell's upstream spot found once a season; the
  currents scanned along parallels on a 512 × 256 index of the cells.
  The ground is smoothed to the old grid's scale before the air climbs it
  (a cell's own slopes are about eight times steeper).
- `Grid::heat` (yearly, sea level) and `Grid::heat_gain` (per unit of
  sunlight above the year's): a column's heat on any date, with the lapse
  rate (`LAPSE`). `Grid::ice`. `celsiusOf` / `heatOfCelsius` in climate.h.
- A world builds in about 1.9 s.

**Numbers:** defaults: 12.6–14.6 °C mean over three codes, equator
26–30 °C, poles −14 to −27 °C, about 14% iced; hottest summer at sea level
31–38 °C, coldest winter −34 to −40 °C. Temperature setting −1 … +1:
−7 … 26 °C; red … blue star: −5 … 25 °C. No snowball at any setting.

**Concavity** after the new rain: −0.26 / −0.40 / −0.25 overall; rising
ranges −0.10 (119 cells, noisy: the probe fits slope against rain-weighted
area, and windward rain now differs from the lee's twentyfold) / −0.42 /
−0.45.

TF9; TF8 refused. Golden re-pinned (2729987046).

**Run:** `TestTerragen` 31/31 (testClimateFromLand and testOceanCurrents
on sphere-drawn land; new testEnergyBalance: Earth-like temperature, ice
growing faster than the cooling, interiors' seasons fiercer than coasts'),
`TestScale`, `TestSphere`, `TestPlanetAtlas`, `TestFluid`, `bun
check:terragen` 38/38 with the weather's checks, `check:far` 100%,
`check:lab`, `check:palette`, UI typecheck. **Fails:** `TestFar`
`testNoHolesFromAbove`, the same ray through the same far-region seam as
sprint 89 (unchanged).
