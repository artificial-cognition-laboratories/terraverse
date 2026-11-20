# Sprint 59 — To orbit

Gravity is a field, the air drags, and a creative player can thrust their
way to orbit.

- **`orbit::Field`** (`src/orbit/field.*`): the pull at a place in the
  map, in the map's terms. Each body's planet card (`terragen::sphere` at
  its radius) gives gravity toward the centre (weaker with height), the
  turning frame's pulls and the air, worked out by `orbit::Flight` (now
  public `accel`) in the planet's frame and given in the axes of the grid
  the place is in (`planet::Atlas::frame`). `carry` re-expresses a
  velocity at a new place as the same motion: inside a grid (its lines
  curve round the planet) and across a seam (the true change of axes, not
  only a turn about up). No atlas: the home card straight down −y.
- **Physics** (`physics::World`): `Opts::field` replaces one gravity
  (`physics::uniform` for tests). Jolt's gravity is off; each moving body
  is weighed by the field where it is and dragged by its air (Cd 1, the
  mean face of its boxes' bounds); Jolt's own linear damping is gone.
  Characters can fly through unloaded ground (`unknownIsAir`).
- **The server** owns the field (`Server::m_field`) and gives it to the
  workshop's machines (`workshop::fieldOf`). Players with `fly` skip the
  movement anticheat (any speed).
- **The body** (`client/body`): walking and falling use the field (walk's
  gravity is a vector); in the air the velocity is carried as it moves,
  taken at the step's midpoint, and the air drags. The body crosses seams
  itself now (sprint 58's body didn't: it moved the player without
  `LocalPlayer::carry`), with the velocity carried through the planet and
  the view turned (`LocalPlayer::turnWithGround`).
- **Thrusting**: flying (creative), Ctrl starts it. W/S along the look,
  A/D sideways, Space/Shift the grid's up and down; from 10 m/s², doubling
  every 4 s held, to 160. B brakes (against the motion, then holds still).
  Gravity and drag act throughout. Landing ends it and turns flying off.
  Key `keymap_brake` (Settings ▸ Controls).
- **The sky**: new worlds' atlases reach a radius up (`height = radius`,
  at most 28,000 layers): the air there is ten scale heights thin, clear
  for an orbit. On the default 16k world that's ~2,600 up; orbit there is
  ~110 m/s, a lap ~5 minutes. Old worlds keep 256.
- **F3** shows speed, altitude, apoapsis, periapsis and period (a stand-in
  for the flight HUD, which is its own design sprint).

Proved: TestOrbit: the field points down the grid and weakens with height
as μ/r² at four places on the planet, two heights; flat is the home card;
thrown at orbital speed over the pole, height holds within 2 m for half a
lap across 15 seams. TestPhysics: a block in air settles at √(2mg/ρCdA);
in none it doesn't. Sabotage: the field not carrying velocity fails the
orbit test (and turning only about up at seams kicked the orbit 40 m,
which is how the seam fix was found). Every other engine test passes but
TestAtlasRender's two, as before. Headless: a tall-sky atlas world
generates (154,634 nodes checked, the atlas world probe); the live machine
check falls, rests and is pushed through the field.

Fixed after the first try: thrusting past an old world's 256-layer sky
put the player in seam, and `core.terra.weather` (tf_matter) threw and
ended the game. Above the top is now a place: `planet::Atlas::charted`
charts it from the top cell of the column under it, for `core.terra`'s
columns and for the field (TestOrbit checks the pull 300 up past the top;
live, `core.terra.weather` answers 300 and 1,000 above it).

Not done: past the atlas's top as its own space (bubbles) (none there yet: the sky's tall enough to
orbit inside); other bodies' pull; the flight HUD; thrust for anything but
a creative player (the body takes a vector, so a jetpack or engines plug
in); machines' velocities aren't carried within a grid (they're slow).
Chunks arriving at ~100 m/s are unproven: the body flies through what
isn't loaded, so diving into unloaded ground at speed can end inside it.

Not seen on screen.
