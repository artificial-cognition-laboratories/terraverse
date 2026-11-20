# Sprint 13 — Climate from the atmosphere (plan)

Goal: the land is shaped by the air above it. Where it rains, how much, and
in which season stops being a noise field and becomes the result of the
circulation, the sea and the mountains: wet tropics under the ITCZ, deserts
on the dry belts at ~30° and in rain shadows, wet windward coasts, monsoon
coasts, dry continental interiors. Biomes, rivers, lakes and vegetation all
follow, because they already read rainfall. TF4.

## The cycle to break

Today: terragen climate → anchors the weather → the weather rains. If rain
then fed terragen's biomes, the loop would close. It doesn't need to:
terragen's **temperature** never reads moisture (heat comes from sunlight,
the star, altitude), so the order can be:

```
plates → elevation → sea level → CLIMATE (temperature, then rain from the air) → rivers, lakes → biomes → vegetation
```

`moisture()` (noise) is retired; `rain` and `wet` come from the climate step.

## Decisions (proposed)

1. **A climate solver in terragen, fixed point.** World codes are shared
   and must give the same world on every machine; the live layer is doubles
   (fine for state a server owns, not for generation). So terragen gets the
   annual-mean version of the same physics, deterministic: the circulation
   by rule (Held–Hou, Rhines: shared formulas, one source), evaporation by
   surface temperature, moisture carried along the mean winds, lifting by
   the circulation's rising branches, by slopes, and by convergence, rain
   wherever lifted air saturates. One implementation, served to the preview
   like everything else.
2. **A steady march, not a simulated year.** Annual (and per-season) means
   only: moisture is relaxed to steady state along the mean wind field in a
   few dozen sweeps on the 256² grid, not stepped through a year of
   weather. Target: under 50 ms, so the create screen's sliders stay live.
3. **Seasons matter for biomes:** solve for two seasons (the ITCZ north,
   then south). Then a place knows both its total rain and when it falls.
   That gives monsoons and wet/dry savannas, and Mediterranean climates
   (winter rain) as a biome later.
4. **Biomes from temperature × rainfall × seasonality,** as data (a
   Whittaker-style table), replacing the heat/wet thresholds in `classify`.
5. **The live weather anchors to this climate** instead of re-deriving it.
   Its rain then agrees with the biomes it falls on, and the fluid layer's
   storms and day-to-day weather are departures from a climate both share.
6. **Fertility:** mineral supply (young volcanic and alluvial ground from
   geology and rivers) minus leaching (rain × warmth) plus recycling (plant
   cover). A per-column value vegetation and later farming read. Tropical
   rainforest soils come out poor and volcanic slopes rich, with no special
   cases.
7. **TF4.** Rain moves, so rivers and biomes move: TF3 is refused with the
   usual message.

## Work

| # | Piece | Where |
|---|---|---|
| 1 | Circulation formulas shared (fixed point in terragen; `fluid` calls the same) | `terragen/climate.*`, `fluid/circulation.cpp` |
| 2 | Climate solver: evaporation, moisture march, lifting, rain; two seasons | `terragen/climate.*` |
| 3 | Grid: rain from the climate; rivers and lakes from that rain | `terragen/grid.cpp` |
| 4 | Biomes from temperature × rain × seasonality (table) | `terragen/world.cpp` |
| 5 | Fertility per column; vegetation reads it | `terragen/vegetation.cpp`, `tf_world` |
| 6 | Weather anchored to the climate (rain and temperature) | `src/weather` |
| 7 | TF4; preview fields: annual rain, seasonality, fertility; overlays | `code.*`, `preview.*`, `ui/src/worldgen` |
| 8 | Tests: wet equator, dry 30°, rain shadows, windward coasts, monsoon shift; the end-to-end check | `src/unittest`, `scripts/terragen-check*` |

## Fix first: the Weather overlay reads as stripes

In the create screen's overlay the weather is almost all zonal bands, and
too dark: it shows the background circulation more than the weather. Before
this sprint builds on it:
- paint the clouds and rain over the true terrain colours, not over terrain
  dimmed to 70%;
- draw isobars only where the pressure has structure (storms), not along
  every band;
- check that fronts and lows show up as swirls, and give them more weight
  if they don't.

## Questions for Cody

- Are you happy that worlds get re-rolled (TF4)? Every existing world code
  changes.
- Is a seasonal rain pattern (monsoon, Mediterranean) worth its own biomes
  now, or later?

---

## Outcome (2026-10-23)

Landed:
- **Climate solver** (`src/terragen/climate.*`): fixed point, deterministic,
  128² cells, two seasons, ~64 ms (over the 50 ms target).
  - The circulation formulas live here (`circulate`, `flow`) and the fluid
    layer calls them: one source.
  - What moves the moisture: evaporation driven by the air's shortfall from
    saturation, convergence feeding rising air, thermal lows judged on
    potential temperature (a plateau in summer is a heat low), lifting on
    slopes, and recycling by wet land.
- **The grid** takes its rain from the climate. Erosion, rivers and lakes
  follow from it. `moisture()` noise is gone.
- **Biomes are a niche table** of heat × wetness × seasonality, with two new
  biomes: **monsoon** (summer rain) and **mediterranean** (winter rain).
  Savanna is now the drier seasonal tropics.
- **Fertility** per cell: what the province supplies, plus river silt on
  low land, minus leaching (wetness × warmth). It is exposed through
  `core.terra.cover` and the preview.
- **Game:** monsoon and mediterranean biomes in `tf_world`; teak, olive and
  cypress trees; species can require a rain season.
- **The weather** anchors its rain to the climate: its own long-run mean is
  scaled to the climate's for that season and place.
- **TF4.** TF1–3 codes are refused.
- **UI:**
  - overlays: Rainfall, Rain seasons, Fertility;
  - biome colours for the two new biomes;
  - the Weather overlay now paints the true terrain colours, shows only
    thick cloud, and draws isobars only round the storms (pressure is sent
    as its departure from its latitude's mean).

Verified:
- **401 engine tests pass.** New tests:
  - the climate on a real world: the 0–10° band rains over 3× the 25–38°
    band, and the 50–65° band over 2×;
  - the same code gives the same climate;
  - desert, savanna, forest, jungle, monsoon and mediterranean all appear;
  - volcanic ground is more fertile than shield;
  - the climate over drawn land: a range in the westerlies gets about 11×
    more rain on its windward side than in its shadow, and a tropical
    continent gets 100× more rain in summer than in winter.
- **Golden** `4255377347`.
- **`bun check:terragen`:** 38/38 terrain, 38/38 `core.terra`, 38/38 weather.
- **`bun check:lab`** passes.
- **UI** typechecks and builds.

Land biomes across four codes: desert 8–24%, forest 10–23%, monsoon 5–20%,
savanna 2–5%, jungle 3–5%, mediterranean 0.3–4%.

Not run: the client. Not seen by eye: the new biomes and trees in game,
the overlays.

Watch:
- Mediterranean lands are rare, and they don't favour west coasts, as they
  would on Earth: the solver has no ocean currents or subtropical highs
  centred over the seas yet.
- Monsoon land is generous on some worlds.
- High-latitude land gets its rain in summer (as Siberia and Canada do).
