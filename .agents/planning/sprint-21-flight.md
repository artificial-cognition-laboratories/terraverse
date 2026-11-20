# Sprint 21 — Flight: from the blocks to orbit, and space as a place

The long thread: gain a few km/s sideways above the air and you're in
orbit; from there, stations, the moon, other bodies. This sprint makes the
mechanics real in creative, end to end.

## The planet card (`src/terragen/planet.h`)

One definition of the body everything moves on: radius, surface gravity,
day, sunlight, air density and scale height. `home()` is Earth's pull and
air at **a tenth of Earth's size, turning ten times as fast**: its equator
moves as Earth's (~463 m/s) and low orbit is ~2.3 km/s.

Climate and weather used to hardcode Earth (`climate.cpp`, `weather.cpp`).
Both now read the card through `resized(home(), EARTH_RADIUS)`: the same
planet at Earth's size with the same surface gravity and equator speed,
whose air flows the same (same Rossby number). That keeps the weather
layer's Earth-tuned constants valid and the terrain bit-identical (the
terragen golden code is unchanged).

## The world is a map of the planet

The blocks wrap the whole planet: `WorldCode::size` blocks around (8192 by
default), so a block across is ~490 m of a 637 km-radius planet. That's why
no "charts" were needed: every place on the planet is already in the one
map (`World::latitudeAt/longitudeAt/xAt/zAt`, `core.terra.globe/block`).

Horizontally a block stands for ~490 m of planet, but where you stand it is
a metre: fine at walking pace, wrong at orbital speed (2.3 km/s would cross
a quarter of the map a second). So fast flight doesn't happen over the
blocks: sideways speed there is held to 340 m/s, and the sideways burn to
orbit happens above the boundary.

## Space is a place (revised mid-sprint)

The first cut had a 1500 m "flight ceiling" where the world was swapped for
a map screen of the globe. Wrong: space isn't a view, it's somewhere you
are, with the same character controls. Rebuilt as KSP's physics bubble:

- **The boundary** is 30 km above the sea (`tf_flight`), the top of the
  planet's blocks: you can build a tower to the edge of space.
- **Space** is the map north of the polar wall: `World::spaceFrom()` (the
  edge plus `SPACE_WALL` = 256 blocks of ice), where `terra::Columns` makes
  nothing at all (`Column::space`), sealed from the planet by the wall,
  which rises to the build limit. `core.terra.world().space` is that z.
- **Bubbles** (`tf_flight/space.lua`): a 1024-block cell of space whose
  centre falls around the planet on an orbit. Everything in the cell rides
  it, so inside it's ordinary map: you float (`body.lua`: gravity off, walk
  keys move you, Space/Shift puff up and down), build, walk a station's
  floor. A pack alone in its bubble pushes the bubble's orbit; sharing one,
  it moves about the cell.
- **Crossing up**, pack or not, puts you in a bubble on the orbit you were
  on; with no sideways speed it falls and puts you back over the block
  under it (200 m below the boundary, so it doesn't flicker).
- **Meeting**: a lone bubble within 300 m of another joins it (contents
  placed where they are relative to it). **Parting**: something 400 m from
  a shared or built bubble's centre gets its own; a lone bubble follows.
- **Stations**: placing a block marks a bubble built. Built bubbles hold
  their orbit (no drag), and blocks can only go in a bubble whose lowest
  point is above the boundary, since blocks can't re-enter until vehicles
  carry them.
- `orbit::Flight::frame` (east/up/north over a place) is what turns a cell's
  metres into the orbit and back (`core.orbit.frame`).
- The sky and weather mods treat the space region themselves: a black sky
  with stars, no weather.

## Flight (`src/orbit`, `core.orbit`)

`orbit::Flight` integrates (RK4) gravity, exponential air with drag, and
thrust, planet-centred in the frame turning with the planet (Coriolis and
centrifugal included, so launching east is helped by the spin). `near` is
the same physics over the flat patch of blocks. `elements` (apoapsis,
periapsis, period, inclination), `directions` (prograde/radial/normal),
`path` (the orbit now), `coast` (a thrown path), `where`/`at`.

Unit-tested: the card's Earth-size match, a circular orbit closing after a
period, the spin's ~930 m/s east/west difference, terminal speed in air,
and the cannonball's three outcomes.

## In the game (`tf_flight`)

The **flight pack** (creative, "Tools for finding out"): right-click to
strap in or out. W/S forward and back where you look, A/D sideways, Space
up, Ctrl for more; over the ground S brakes and Shift on the ground
unstraps; in space Shift goes down and Ctrl while coasting alone runs time
×20. A player who leaves in space, or mid-flight, is still there on return.

The page (`ui/src/game/flight`): over the blocks a strip (altitude, speed,
climb, how far to space); in space a readout (the orbit's shape in its
plane, apoapsis, periapsis, period, inclination, air, and whether you can
build here).

## In the wiki

A **Flight** section: Newton's cannon on this planet (`orbit.coast`, the
same flight as the craft): speed, angle, height, east/west, air on/off.

## Checks

- `./bin/luanti --run-unittests` (TestOrbit).
- `bun check:flight`: on a live terra world, an autopilot climbs over the
  blocks (sideways speed held), enters a bubble, burns it to a clear orbit,
  coasts it once, merges a second bubble into it, splits one off, and comes
  back down to the blocks. Stand-ins are entities; what only a player
  exercises (floating, the readout, rejoining in space) is a playtest.

## Known simplifications, next

- The air is one exponential (scale height 7.2 km); no re-entry heating
  yet (the matter physics is ready for it).
- The pack is creative: infinite propellant, no rocket equation. Survival
  craft get mass ratios and exhaust speeds from the matter tables.
- Moving block structures (vehicles built of blocks) need native support:
  the next foundation, for planes, boats, stations.
- The moon: another body's card, and where its blocks live.
- Seeing the planet from space: the far view still draws the space region
  as polar ice, and nothing draws the planet below a bubble. That's the far
  view's (the other session's): a sphere under the bubble from its place.
- An orbit map overlay (key M) over the world.
- Vacuum: a station's blocks still react as in the pole's air at 1 atm.
- Bubbles that meet carry objects across, not blocks: two stations don't
  merge (vehicles).
- terragen's `circulate` duplicates `src/fluid/circulation.cpp`'s: one
  should go.
