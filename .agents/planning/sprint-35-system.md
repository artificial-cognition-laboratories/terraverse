# Sprint 35 — One system, drawn natively

## Decision

A new world is a new system: its star, its planets, their moons. The
system is one model, in terragen (`terragen::System`), and the
Planetarium draws all of it in one view: it focuses a body, and every
other body is drawn at the detail its distance calls for. The page edits
causes and picks what to look at; it never computes a body or draws one.
This retires `globe.ts`'s system view, its concept bodies and, with native
overlay layers, the page's globe on the create screen.

Bodies other than the playable one aren't walkable yet: that is the scale
sprint's (regions as tiled maps, a crust shell: see the end).

## Phases

### 1. `terragen::System`

- The system as the player made it: the star's causes, every body's causes
  (`BodyCauses`), its name, where it starts on its orbit, its inclination
  and tilt, and which body is played. `SystemSpec::home()` is the starter
  system (moved here from `ui/src/system/model.ts`).
- What each body is today: its card (`bodies()`, the body model).
- Where each body is at a moment: Kepler orbits round the star (planets)
  and round their parent (moons), periods from the masses. Real units
  (km, days). Doubles: positions place bodies in a view, they never decide
  a block.
- `system.preview` and the page's `causes()` go through it.

### 2. The Planetarium draws a system

- A scene of bodies, the camera focused on one, going to another without a
  load. Bodies sorted far to near, each drawn with its own depth range, so
  a planet 1 AU off and the ground under the camera share one frame.
- Detail by distance: a body under a pixel or two is a lit point (kept
  visible, labelled by the page); farther than its tiles are worth, a lit
  sphere coloured by its kind; the playable body near the camera is its
  far tiles (as now).
- The star: a light source for every body (the side facing it lit) and a
  glow. Orbits: thin lines, for the system view.

### 3. Overlays as native layers

- Biomes, temperature, rain, … are the playable body's tiles coloured by a
  field instead of by their blocks. The page picks the layer.

### 4. The create screen uses it

- The viewer's body strip focuses bodies in the native scene; the system
  view is the native system. `globe.ts` goes, but for the inside cutaway
  (the octree in depth, sprint 32's phase 3).

## Later: scale

Regions as their own maps, tiled where wider than a map (16-bit local
positions, a 32-bit tile id: no size limit; a tile's edge a seam with no
turn), and voxels only in a crust shell, the deep interior a model. Then
other bodies are walkable and planets grow to Kerbin's size.

## Built (2026-10-28), for Cody to look at

- `terragen::System` (`terragen/system.h`): the system's causes and
  placement (`SystemSpec`, `home()` the starter system), the cards from the
  body model, Kepler orbits with periods from the masses (km, days).
  `TestSystem`: Earth's year 365.25 days, the Moon's month ~27 days, orbits
  close after a period, moons round their planet, one playable body.
- `planetarium::SystemLayer` (`planetarium/system.h`): the star (a bright
  sphere, a glow, the light), every other body as a sphere lit from the
  star and coloured by its kind, never under a 2.5-pixel dot, the orbits as
  lines. Far to near, a depth range each; the focused body's tiles in their
  turn. The camera zooms out to the whole system.
- Every host gives the Planetarium the home system (`TEMP`: a world
  doesn't record its system yet).

Then (same day):

- Fixed: the star vanished off to the side (each body's depth range was
  from its distance, not its depth along the view). Space is black.
- Tilt: the scene is the playable body's frame, the system turned so its
  spin axis is up (its tilt from the world code).
- Focus: `planetarium.focus { body }` puts the camera in orbit round any
  body, at its scale; the playable body's tiles chosen from wherever the
  camera is. The create screen's body strip focuses natively.
- The world's own system: `SystemSpec::forWorld` (the code's star and
  tilt); in game the date follows the world's (`System::dayFor`: as far
  through the orbit as the world through its year).

Not yet: the playable body's tiles lit by the star (day all round); bodies
the player adds on the create screen (the native system is the home one
round the code's star: focusing an added body is refused); a body's spin.
