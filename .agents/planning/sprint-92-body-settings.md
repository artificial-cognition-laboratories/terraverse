# Sprint 92 — Body settings

Every body today is built by Earth's pipeline (plates, rain, rivers,
erosion) and then painted one bare biome by `Surface` (Living, Rock, Sand,
Ice), picked from its kind. The Moon has drifted plates and river valleys
that nothing on it could have made.

## The model

`WorldCode::surface` becomes `WorldCode::processes` (`terragen/code.h`),
taken from the body card (`worlds.cpp`), never from the body's name:

| Setting | From the card | Read by |
|---|---|---|
| regime: plates or a stagnant lid | `plates` | the history: a lid is one plate that doesn't move, its crust as it formed (highlands and lowlands) |
| water: liquid water at the surface | `ocean` | the grid: no rain, no glacial scour, no erosion or sediment, no lakes or rivers; the column: no sea |
| life | `ocean`, a temperate kind | vegetation, biomes |
| bare: what ground is where nothing lives | kind (rock, sand, ice) | biomes |

The home keeps the Earth-like settings (its card is the playable body);
choosing the home's kind is the create screen's, later. Methane seas
(hydrocarbon worlds) wait for the general atmosphere: they're dry here.

## Checks

- The home is bit for bit as before: golden unchanged.
- The Moon: one plate, no boundaries; no rivers, lakes or sediment; its
  heights as the history formed them.
- `TestTerragen`, `TestBodies`, the rest; `check:terragen`.

## Outcome

- `WorldCode::processes` (`Processes`: `Regime`, `water`, `life`, `bare`)
  replaces `Surface::Living/Rock/Sand/Ice`; `Surface` is now only the bare
  ground (soil, rock, sand, ice). `worlds.cpp` takes it from the card:
  plates from `plates`, water from `ocean`, life from a temperate ocean.
- A lid (`history.cpp`) is one plate with no spin: no collisions, arcs,
  openings or events; the crust only ages. A plate covering the whole body
  keeps its centre (its cells have no middle; `geology.cpp`).
- No water (`grid.cpp`): no rain, scour, erosion, sediment, lakes, rivers or
  streams; in the history, no wearing between steps. A dry column's
  rain seasonality is 0, not a division by nothing (`world.cpp`).
- Biomes: a lifeless body is one biome by its bare ground (`BARE`).
- `render <code>@<body>` draws any body as the game builds it.

The home is bit for bit as before (golden unchanged). The Moon of the
default code: one plate, highlands and low plains as they formed, nothing
worn. Still the home's Land and Continents settings, and noise-born
continents: craters (sprint 94) and maria (95) give it its own face.

**Run:** `TestTerragen` 31/31, `TestBodies` 5/5 (the Moon's grid: one
plate, nothing worn, laid, pooled or flowing, no history events),
`TestScale`, `TestSphere`, `TestPlanetAtlas`, `TestFar` 5/5, `bun
check:terragen` 39/39. Not run: the game, the planetarium, `check:far`,
`check:lab` (home unchanged).
