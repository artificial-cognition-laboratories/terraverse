# Arc: air you can see

Every body's sky comes from its card: blue over Earth, and whatever its own gases,
dust and star make elsewhere, with red sunsets and a glowing rim from space. It only
changes what's drawn, so codes don't change and floats are fine. Then shadows, then
belts (`arc-systems.md`, phase 8).

Method, from the paper alone: Hillaire, "A Scalable and Production Ready Sky and
Atmosphere Rendering Technique" (2020). Rayleigh and Mie scattering, kept in small
tables so drawing the sky is a lookup.

## What's there today
- `Sky::render` draws space, clears depth, then the far view and near world draw. The
  air goes at the end of that pass, over the stars and other bodies, so they fade by
  day without a special case.
- Render targets and 16-bit float textures already work (bloom uses them). Irrlicht has
  no 3D textures, so the paper's 3D table for air between the eye and land is out: the
  far view works that out per pixel from the 2D table.
- Near-world shaders read `dayLight` and `fogColor`. Setting those gives the near world
  its tint and fog without touching `nodes_shader`.
- `tf_sky` sends a black plain sky on planets, so `fogColor` is black today.
- The far view's haze ignores the card. It uses `terragen::sphere(radius).scale_height`
  and a fixed blue `AIR`, so airless bodies get blue air too. This goes in phase 3.

## Scale
Drawn air is thick against its planet (a tenth of the radius; Earth's is under 1/700).
Real colours depend on that ratio: a sunset's long path through the air is what makes it
red. So tables are built for the body's real radius and air height, and the eye's
altitude is mapped by share of the air's depth. The ground sees a real sky; only the
height where it fades to black is drawn to the game's scale.

## 1. Air optics (sprint 110) — built, nothing in game yet
`src/air` (`mediumOf`, `Tables`, `TestAir`); sheets from `scripts/air-check/sky.cpp`, real
or game scale. At game scale the tables can be built (`Medium::resized`), but the sun stays
lit past the horizon and there's no red sunset: which to draw is decided in phase 2. Air too
thick for the paper's sums gets a two-stream diffusion term (`Tables::diffuse`). Tables take
about 100 ms a body on the CPU.
A pure module, no engine: card and star in, scattering, absorption and air height out.
- Rayleigh per gas, from published refractivities, by share and column of air. Air
  height from temperature, gravity and molar mass.
- Absorption: ozone from oxygen (it keeps twilight blue), methane in the red.
- Particles from the card, never the name: dust (thin air over dry ground), organic haze
  (methane under starlight), cloud deck (thick hot air). These are the estimates to tune.
- The star's colour by class. Tables built on the CPU: sunlight through air, and the
  paper's multiple scattering. They're also what the land's light reads.
- `TestAir`: Earth's coefficients against published values; no air gives no scattering.
- A render tool like `render.cpp`: a sky strip per body and sun height, to a PNG.
- Done when: the PNGs show Earth blue with a red sunset, and Mars, Venus and Titan
  distinct, reviewed by eye before anything is in game.

## 2. Sky in game (sprint 111) — built, unseen
The sky image is made on a worker thread, not a GPU pass: no float targets, no per-frame
sums, and `Sky` only looks it up (`planetarium/air`, `air_sky_shader`). True-size colours,
the far view's depth to climb. Fog takes the horizon's colour, which was phase 3's.
- A small sky image (about 200×100, float), redrawn only when a sun or the eye has moved
  enough; drawn over space, thinning with altitude, with stars showing through.
- Each star adds its own light; a companion too faint to matter is skipped.
- Done when: day, sunset and night read right on Earth, the Moon stays black, and the air
  fades out on the climb to orbit. The user judges; sky pass timed against the budget.

## 2b. Planets lit (sprint 112) — built, unseen
Land drawn from tiles is lit from the star's real direction (`planetarium/lit`,
`planet_lit_shader`), by the far spheres' curve; an overlay turns it off to stay readable.
`SystemLayer::night` is gone. The sky logs its state changes to find why it vanishes.

## 4. The rim and eclipses (sprint 113) — built, unseen
A shell at 3% of the radius with the real air's colours (`planetarium/rim`, `air::rimImage`,
`air_rim_shader`). Far spheres are now lit by the same shader as land; a body's family
(moons, planet, fellow moons: four that look biggest) casts soft shadows on it, per pixel.

## 5. Tint and shadows (sprint 114) — built, unseen
Daylight's colour from the air (`air::sunTint`, `solar::tint`, into `dayLight`); dynamic shadows
on by default, cast by the brighter star's real direction (`Sky::light`) at any planet. One
shadow map: no second star's shadows. The far view casts and takes none. `system.world` answered
in the menu too. Haze, exposure and strength stay for the detailed atmosphere pass.

## 3. Air on the land (after the rim, eclipses)
- `dayLight` and `fogColor` from the tables: sunlight reddens low, fog is the horizon's colour.
- The far shader's haze from the same tables; `AIR` and the `sphere` air go, so the seam
  with the near world still matches and airless bodies have no haze.
- The light's level stays as it is (`solar`); this changes its colour first.
- Done when: distant land fades to that world's sky and no seam shows at sunset.

## (Superseded by 4 above) From space
- A shell round each body with air, in the planetarium and the game sky, from the same
  tables: the rim, and the day/night edge. Giants get a rim; their bands aren't part of this.
- Done when: the climb from ground to orbit has no jump, and the rim looks right.

## Budget
Target under 0.5 ms a frame for the air, on the user's machine, measured in phase 2. If
it's over: redraw the image less often before making it smaller. Numbers here are targets
until measured.

## Not in this arc
Weather clouds, shadows, light shafts, eclipses dimming the sky, seasonal change in dust.

## Risks
- Particles are estimates: the gas is physics, but Mars' red and Titan's orange rest on them.
- Exposure: Venus' surface and Earth's noon differ hugely, and the engine has no such
  range by default. The sky needs its own tone mapping, and post-processing may be off.
- The seam between near and far at sunset (phase 3).
- Float render targets on the legacy and ES drivers: refuse loudly rather than show a wrong sky.
