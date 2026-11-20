# Sprint 60 — The sky is space

The stock sky is off on a planet, and the Planetarium's system is the sky:
a clean start to draw the air back onto, and a way to see an orbit.

- **Stock sky off** (`tf_sky`): on an atlas world, a plain black sky, no
  sun, moon, stars or clouds. The real sun is still sent (`solar`): it
  lights the world. Worlds without an atlas keep the blue sky.
- **Weather not drawn** (`tf_weather`): no clouds, rain, snow or ash. The
  weather is still read for the page (`set_ui "weather"`), and snow and
  ice still act on the world (`world.lua`). Its fall textures are unused.
- **Space** (`planetarium/sky.*`, `planetarium::Sky`): a scene node drawn
  first in the sky pass, before the far view (which draws the planet): the
  Planetarium's `SystemLayer` seen from the player. The eye's grid frame
  (`AtlasView::eye`) turns the camera's look into the planet's frame, and
  the time of day turns that into space (`SystemLayer::spin`, now shared
  with the Planetarium). Stars, the star, the other bodies and their
  orbits on the world's date. Then depth is cleared and the camera's own
  projection put back.
- **The orbit in the Planetarium** (V): the player's coasting path drawn
  round the body they're on, in the accent blue, bright where they are
  and dimming ahead. `orbit::Flight::path` samples the conic against the
  stars (bound: once round; escaping: out to five radii), in the turning
  frame as it's turned now, so it's drawn fresh each frame
  (`Field::path` → `Body::path` → `shell::Session` → `Planetarium::path`).
  The camera's far plane reaches round it.
- **Planetarium detail**: a tile's cell may span 6 pixels before it splits
  (was 2): about a ninth of the cells per frame.

Proved: TestOrbit `testPathIsTheOrbit` (the path starts at the craft; its
lowest and highest points are the periapsis and apoapsis within 1 m; an
escape ends at the reach). Builds clean; a copy of an atlas world boots
headless without Lua errors (no player joined, so `tf_sky`'s join path
didn't run).

Not seen on screen: none of it. Unproven by eye: that space lines up with
the far view's planet (same frame, so it should), the sun's place against
the lighting (one from the system, one from `solar`; both from the time of
day), and the path's colour and thickness (line width may be ignored).

Fixed after the first try:
- **Orbiting above an old world's sky crashed** (black, then the menu):
  `planet::Atlas::carry` gave up above the top (no cell there), so the
  body never crossed a seam up there; it drifted into the gap between
  tiles, the view went black and `core.terra.weather` (tf_matter) threw.
  Above the top it's now charted from the column under it, like the field.
  Columns of neighbouring grids lean apart up there, and a point in the
  wedge between goes to the nearest column's edge: up to 1.5% of the
  height above the top. TestPlanetAtlas `testCarryAboveTheTop` (300 up:
  every carry lands over a column and stays; worst 3.5 blocks).
- **The path drew as a few straight lines** close up: 2,048 points,
  spaced closest where the craft is.

Not done: the air (a sky colour, haze, sunsets: the atmosphere sprint);
the flight HUD; worlds without an atlas get no space.
