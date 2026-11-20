# Sprint 22 — The space plane, and the planet all the way up

Sprint 21 made the physics real: the planet's air and gravity, the boundary
at 30 km, bubbles on orbits. Getting there by pack proved it works and
felt wrong: you stare up, the planet vanishes by 2 km, and you arrive in a
black void. This sprint makes the journey: a creative space plane flown in
third person, and the planet in view from the runway to orbit, so the
boundary can't be seen.

The air is staying as it is. At 30 km there's still enough of it to eat
your speed; orbits hold above ~100 km. That's the mechanic, not a bug.

## 1. The planet in view, all the way up (`src/far`)

Today the far view is the map drawn as blocks with the curve applied, and
it fades out by ~2 km. Two problems going up:

- **Scale.** Across, a block is ~490 m of planet; up, a block is a metre.
  Close to the ground the far view draws the map as you'd walk it. From
  orbit you have to see the planet at its true size, 637 km in radius.
- **The polar wall.** The space region past the wall is drawn as ice.

The plan: two views, blended by altitude.

- **Low (the map):** the far view as now, reaching further as you climb.
  Its range grows with height, and the tiles coarsen.
- **High (the globe):** a sphere at the planet's true radius, coloured from
  terragen's surface (the same columns, sampled coarsely), with day/night
  from the sun. It's drawn from the camera's real orbital position, so in a
  bubble the planet sits where the orbit says it is.
- **Between:** a crossfade band, set so the map's horizon and the globe's
  limb line up. Air haze thins with height and hides the seam. The sky
  turns from blue to black by density, not at the boundary.
- In a bubble the near world is the cell; everything else is the globe.
  Above and below the boundary the same globe is drawn from the same
  position, so crossing it shows nothing.

The server sends the client its orbital state (or the client reads the
bubble's) so the globe can be placed. Native: the globe mesh and shader.
Lua: none, beyond reporting where the player is.

### Built (first pass)

- `terragen::Map` (map.h): the map's geometry on the planet, fractional
  both ways, and `share(altitude)` / `across(altitude, latitude)`: how far
  the map has become the planet. It starts at 400 m (`SPREAD`; the land tops
  out below) and is complete at the boundary (`BOUNDARY`, now native and
  read by Lua as `core.terra.world().boundary`).
- The far view's vertex shader moves each vertex from the flat map toward
  its place on the true-size globe by `share`. The haze is the air along
  the sight line, thinning with height, and turns to lit air seen from
  above. Space cells draw nothing.
- `TOCLIENT_ALOFT` / `player:set_aloft`: in a bubble, the server says where
  the player is over the planet (protocol 57), and the far view draws from
  there. The client carries it forward between packets.
- Flight over the blocks moves by `core.terra.across`: speeds are true
  m/s, so map crossing slows with height as the far view grows. The
  340 cap is now on blocks/s (what the map can load), not true speed.
- The sky (tf_sky) goes from blue to black with the air's density over
  where the player is (`tf_flight.aloft`), with stars by day and clouds
  gone above 12 km.

Second pass, after flying it: blending each point in a straight line from
the flat map to the globe made a mangled small ball partway up. The centre
swelled and the edges folded, and the land grew 490× while the height grew
75×, so climbing looked like getting closer. Now there's one sphere at every
height: the map is laid on it from the eye's latitude and longitude, a
block spans `across` metres of it, and it curves with `Map::curve`. Both
grow geometrically from 400 m up to `FULL` (400 km), with a block's span
growing a little slower than the height, so the land shrinks (if slowly) as
you climb. At FULL it's exactly the planet. Over 30 km→400 km a block grows
from ~48 m to ~490 m across. The far view's mask no longer hides land under
columns whose surface it doesn't know (the pale square seen from high up).

## 2. The space plane (`tf_flight`, creative only)

A vehicle with proper flight controls, flown from behind.

- **Camera:** third person behind and above the plane, with the mouse
  looking around it freely. A key switches to the cockpit.
- **Controls:** W/S throttle up and down (it holds), A/D roll,
  Space/Shift pitch up and down, Q/E yaw. Ctrl boosts. Shown on the HUD.
- **Air:** wings give lift ∝ density·v², drag the same way, and a stall
  below flying speed. Low down it flies like a plane: no 340 m/s cap while
  it's above the blocks at cruising height. The cap moves to what the
  map can stand (see below).
- **Thin air:** lift fades and thrust stays, so the natural climb curves
  up and east. Past 100 km, burn east until the periapsis clears.
- **Space:** in a bubble the plane is a lone craft. Its thrust pushes the
  bubble's orbit, attitude by the same keys (reaction wheels, no lift).
- **Coming back:** drag brakes it on the way down, it glides, and it lands
  on wheels and brakes on the ground.
- **Model:** a simple block-built mesh for now. It'll be replaced by
  vehicles built from blocks.

Sideways speed over the blocks: the 340 m/s cap exists because a block
across is ~490 m. The plane needs to cruise faster than that. Decide in the
sprint: lift the cap with height (the map moving faster under you up
high is less visible), or scale how the map moves under a fast craft.

## 3. Along the way

- The pack drops out of the hand while flying: find out why.
- The orbit panel becomes the plane's instruments: speed, altitude, climb,
  attitude, apoapsis/periapsis, and a mark for orbit.

## Done when

From the runway: take off, fly around the planet low, climb to orbit
with the planet in view the whole way, and come back to land. The flight
check covers the plane's climb to orbit and its return.

## Order

1. The globe and the blend (the biggest piece, and the feel).
2. The plane: model, camera, controls, air.
3. The plane in space, and coming back.
4. Instruments, the check, the pack bug.
