# Sprint 12 — Weather: a fluid layer on a spinning sphere

Goal: weather that moves, from physics general enough for any body. The
atmosphere is a **fluid layer** whose behaviour comes from the body's numbers
(size, gravity, spin, tilt, sunlight, what the air is made of, what can
condense in it); Earth's atmosphere is its first instance. The rest of the
world reads it: the player sees clouds, rain, snow and ash; the HUD shows the
air; the create screen shows a year of it on the globe.

## Decisions

1. **One general primitive, `src/fluid`.** SI units, doubles, deterministic;
   depends on `matter` only (what the air is made of, what condenses: phase
   curves and latent heats from the library). No Earth constants inside: a
   unit test spins the planet faster and slower and the bands change.
2. **The circulation from classic results, not a simulated fluid.** Held &
   Hou give the Hadley cells' reach from the spin, size, gravity and air
   depth (~30° on Earth, pole to pole on a slow world); Rhines gives how many
   jets fit beyond them (one on Earth, many on a fast giant). This is the
   modelling boundary: a regime from rules, calibrated on known worlds.
3. **What moves on the grid:** temperature (as potential temperature, so
   air blown up a plateau arrives cooler), the condensable's vapour, ash.
   Heat in by latitude and season; the surface follows with sea and land
   inertia (land a month behind the sun, sea two); eddies carry heat poleward.
4. **Rain emerges.** Rising air is fed by converging low air and cools;
   what it can't hold rains out and warms the air aloft. Rising comes from
   the circulation (the ITCZ, the polar front), storms, wind up slopes, and
   ground warmer than the air. Sinking air dries (the deserts at 30°).
5. **The condensable is chosen, not assumed:** the substance whose vapour
   pressure at the body's mean temperature is the largest share of the air's
   pressure that is still below 1: water on Earth, methane on Titan.
6. **Storms as a list:** lows and highs along the fronts, drifting east with
   the jet; tropical cyclones over sea above 299 K, drifting west and
   poleward, strengthening over warm water, dying over land. Winds circle
   and spiral in (geostrophic balance, against the spin's direction by
   hemisphere).
7. **Events are data:** thresholds on the result (thunderstorm, blizzard,
   fog, hurricane, drought, ashfall), temperatures measured from where the
   condensable melts.
8. **Earth's instance, `src/weather`:** the ground from terragen (sea, land
   heights, how wet the soil starts), active volcanoes as ash vents, and
   **anchored to terragen's climate**: the surface's annual mean and seasonal
   swing per cell are terragen's (what the biomes were made from), and a
   sample's temperature is terragen's for that exact column, date and hour
   plus the layer's departure (the seasons' lag and the weather). Rain, wind,
   cloud, pressure and storms are the layer's own.
9. **Live state, saved with the world** (`<world>/weather.bin`, at each new
   day, atomically). The saved state and the clock decide the weather, so
   steps after the last save are replayed exactly. A fresh world starts
   settled (a year run before now). A clock set back, or more than a year
   ahead, starts the weather over, settled.
10. **Scale:** 64 × 64 cells over the world; one game day is one model day;
    3-hour steps (~3 ms each); a block of height is 25 m of air (the world's
    heights are exaggerated against its width, the air sees real mountains).

## Pieces

| Piece | Where |
|---|---|
| Fluid layer: circulation, heat, moisture, advection, storms, events, save/load | `src/fluid/` |
| Earth's weather for a terra world: ground, anchors, clock, persistence, the preview year | `src/weather/` |
| `core.terra.weather(pos)` | `src/script/lua_api/l_terra.cpp` |
| `weather.preview` (a settled year in 24 maps) | `src/shell/menu.cpp` |
| Clouds, rain, snow and ash around each player; `set_ui("weather")` | `games/terrafirma/mods/tf_weather` |
| Weather overlay on the globe and map, following the date slider | `ui/src/worldgen/weather.ts`, `menu/create/Viewer.vue` |
| HUD: temperature, what's falling, wind, barometer, events | `ui/src/game/WeatherStrip.vue` |
| Tests; the headless check samples the weather at every probed column | `src/unittest/test_fluid.cpp`, `scripts/terragen-check*` |

---

## Outcome (2026-10-23)

All pieces landed. Verified:

- **`TestFluid`, 7 tests.** Spin shapes the bands (Earth: Hadley edge
  25–36°, one jet; ×4 spin: narrower cells, more jets; Venus's spin: cells
  to the poles, no jets; a Jupiter: ≥ 5 jets). Water condenses on Earth,
  methane on Titan. On an ocean Earth, a year settled: equator ~10 mm/day,
  subtropics ~1.3, storm tracks ~3.7; trades easterly, westerlies westerly.
  A north–south range in the westerlies: the windward side gets ~30× the
  leeward rain. Fronts and tropical cyclones form; thunderstorms happen;
  air 3 km up is colder and thinner. Stepping is deterministic, and a saved
  and reloaded layer carries on identically. On a real terra world:
  temperature agrees with terragen, the file round-trips (reopened, it
  continues exactly as if never closed), another world's file is refused,
  the preview year has summer warmer than winter.
- The whole engine suite: 399 tests pass.
- `bun check:terragen`: 42/42 terrain, 42/42 `core.terra`, and **42/42
  weather** (finite, near terragen's climate, sane pressure and cloud, and
  `weather.bin` saved by a live headless server).
- `bun check:lab` passes; UI typechecks and builds.

Not run: the client. That covers the clouds, rain, snow and ash by eye, the
HUD strip, and the weather overlay on the create screen (the gallery has a
mock year).

Found while calibrating (fixed):
- Air carried its temperature as-is onto plateaus, 16 K too warm: it now
  carries potential temperature.
- A storm sitting still kept pulling moisture in without draining its
  neighbours, until the rain heated the air near the ground into a runaway.
  The inflow is now capped at what the surface could saturate, and most of
  the latent heat goes aloft.
- The layer's own radiative seasons swing ±45 K at 60°, far more than
  terragen's. Earth now takes its seasons from terragen; other bodies keep
  the layer's own, damped by the sea's and the land's inertia.

Open, for Cody:
- **Feed the weather back into the terrain.** Biomes still come from
  terragen's moisture noise, not from where the atmosphere rains. A settled
  rainfall climatology could replace that noise: wet tropics and dry belts
  in the biomes themselves. Fertility, from mineral supply minus leaching
  plus recycling, follows from the same step. That changes generation, so it
  needs a TF4 code.
- **The ocean as a second layer** (currents carrying heat, coupled to the
  air), then another body (the moon: almost no air; Titan: methane weather).
- **A Lab scenario** for the rain shadow needs a way for Lua to run a small
  standalone layer (`core.fluid`?). Not built.
- Weather doesn't yet change anything in the world itself: snow cover,
  frozen lakes, fires going out, crops. It's readable everywhere through
  `core.terra.weather`, so these are game-side work.
