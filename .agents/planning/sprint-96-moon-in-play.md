# Sprint 96 — The Moon in play

The last of the Moon arc (`planning/arc-moon.md`): standing on the Moon is
standing on an airless body with a sixth of a g, not on Earth painted grey.
And the arc's loose ends tied up.

## Causes (never the body's name)

Every body's surroundings come from its card (`terragen::Surroundings` on
`BodyWorld`): its gravity (m/s²), its surface pressure and temperature, and
its air by the matter table's gases. What falls, flies, breathes and burns
reads them:

- **Gravity and air drag**: `terragen::planetOf(body)` is the body as flight
  sees it (its pull; its air's density at the ground, ρ = PM/RT).
  `orbit::Field` takes one for each atlas body, on the server (from its
  body worlds) and on the client (from the horizon's code). It was
  `sphere(radius)` for every body: Earth's pull and air everywhere.
- **The world's matter**: `WorldMatter` runs one air per body
  (`Opts::airs`, `Ground::air`: which body a block is in). Each awake block
  keeps its air (`Cell::sky`), and the physics runs at that pressure with
  that gas. No air is a trace of a pascal: a block's gas leaves and nothing
  comes back, so a fire starves and water boils off.
- **Weather**: off the home, still and dry as before, now at the body's own
  pressure.

## The game sky

Orbits are a viewer's aid: the planetarium draws them, the sky seen from
the ground doesn't (`SystemLayer::Opts::orbits`).

## Leftovers tied up

- **Mare shores** follow the land, not the grid's triangles (after sprint
  95's first look): see sprint 95's outcome.
- **The lava's own relief** (`World::mareRelief`): wrinkle ridges (300 m,
  sinuous, tens of km long), rilles (narrow winding channels 250 m deep),
  low domes. Fading in from the shore.
- **Rays**: a third of the young complex craters are still bright: their
  ejecta and streaks running straight out to ten radii, thinning, light
  regolith over the dark mare (`World::rayed`).
- **KREEP** under thin crust from 0.6 (was 0.7), at full share.
- **Ilmenite** follows the column's own lava depth, not the cell's.

## Outcome

Built as planned. From the default system's cards:

| body | g (m/s²) | air | ρ at the ground |
|---|---|---|---|
| earth | 9.88 | 1.30 bar N₂ O₂ | 1.55 |
| vesper | 3.76 | 0.09 bar CO₂ | 0.20 |
| moon | 1.31 | none | 0 |
| nacre | 0.64 | 0.19 bar N₂ CH₄ | 0.45 |

(The home's air is its card's, 1.3 bar: water boils a few degrees hotter
than before. Helium, which the matter table lacks, is left out of an air.)

- Every body flew and fell by Earth's pull and air (`sphere(radius)`):
  now its own. The client builds its field once the horizon (the world's
  code) has come, holding the body a frame till then.
- A vacuum ran away: gas a block can't hold wakes the air beside it so it
  can burn there, and in a vacuum no block holds any, so every puff woke
  the next block, to the budget. Into a vacuum it now spreads to nothing.
- The game sky draws no orbits.
- Mare relief and rays as planned; ridges and rilles show at a few
  thousand blocks across, not from orbit.

Tests: `TestSandbox` (a log lit with no air only chars), `TestBodies`
(`testSurroundings`: the home's oxygen and pull, the Moon's lack of air
and drag, a thin air thinner), `TestOrbit` (the field takes its planets).

Not done: liquids still run at Earth's pull in the world's matter
(`pouring.cpp` GRAVITY); breath and suits (a rule, later); the far view's
and planetarium's mare edges are still per cell; the biome stripes seen on
the create screen don't show in terragen's own render (`render biomes`), so
they're in the planetarium's painting, not found yet.
