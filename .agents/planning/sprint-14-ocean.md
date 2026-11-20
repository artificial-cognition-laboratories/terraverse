# Sprint 14 — Ocean currents

Goal: the sea carries heat. Wind-driven gyres warm some coasts and chill
others, and cold water dries the air above it, so coasts differ by which
side of an ocean they face, not only by latitude. TF5.

## Decisions

1. **Gyres by rule, in the climate solver** (`terragen/climate.cpp`,
   `currents`). In the subtropics, a narrow warm current runs poleward along
   an ocean's western edge (off east coasts) and a broad cold one runs
   toward the equator along its eastern edge (off west coasts). In the
   subpolar band the gyre turns the other way: warm water off west coasts,
   cold off east coasts. Cold water also wells up off eastern shores at the
   equator. A coast's current reaches 40 cells across an ocean; the result is
   smoothed and carried 3 cells inland.
2. **What it changes:** the heat of the sea and coasts (which feeds biomes,
   temperatures and the live weather's anchor), evaporation (warm water
   holds more), and stability (air over cold water sinks: the coastal
   deserts).
3. **Seen as** the create screen's **Ocean currents** overlay (red warmer,
   blue colder than the latitude).

## Outcome (2026-10-23)

Landed as above. Also in this pass:
- **Rain seasons overlay:** only strong seasonality now shows its colour.
- **Live weather:** lows are deeper (−1600 Pa) and lift their air harder,
  so storms stand out from the bands.

Verified:
- **402 engine tests pass.** The new currents test is an ocean between two
  continents: the warm current is on its western side, the cold on its
  eastern, and the warm-current coast gets ~26× the rain of the
  cold-current coast.
- **Golden** `2757000011`.
- **`bun check:terragen`:** 37/37 on terrain, `core.terra` and weather.
- **`bun check:lab`** passes; the UI typechecks and builds.
- **Land biomes** across four codes barely move (the currents shape coasts,
  not continents).

Not run: the client.

Still open: Mediterranean climates stay rare, because cold currents make
west coasts dry all year. A west coast needs the winter storm track to
reach down to ~35°, which the two-season solver only partly gives.
