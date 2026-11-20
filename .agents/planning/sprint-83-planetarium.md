# Sprint 83 — The planetarium, professional grade

## Decision (2026-10-31, Cody)

The planetarium is a system viewer in the spirit of a space-flight
sim's map: every body looks like itself at every distance, the star is a
place you can go, and every overlay is as sharp as the land.

- **One live-tiled body.** Only the focused body is `far::Tiles`; every
  other body is drawn from a bake of the same LOD's coarsest level, so its
  surface terrain shows from afar. Focusing hands a body from bake to tiles.
- **The star is focusable** close up, a body like any other; zooming out
  from it takes in the whole system.
- **Overlays are painted per tile, natively**, from terragen's fields at
  each tile's own resolution, so they gain detail as you zoom in exactly as
  the land does. Today the page paints one fixed equirectangular image
  that tints the tiles: zoomed in, the blocks are sharp but the overlay
  stays a few coarse cells.

## Steps

1. **UI cleanup.** The readout (explored %, coordinates, "V or Esc to
   close") is gone. "Go there" (creative) moves to the picked place's card.
   The body strip sits top-centre, the title left and Done right of it;
   nothing overlaps.
2. **Star picking and focus.** Hover ranks by screen distance against drawn
   size, preferring a disc the mouse is inside (it took the camera-nearest,
   so a planet dot over the star won). The star is a flight target
   (`Flight::body` takes the star); a camera framed round it. Picking it in
   the strip or double-clicking it goes there, not out round the home
   planet. Tested headless: a click on the star's disc picks the star.
3. **Orbits a few pixels wide.** Orbits and the player's path as
   camera-facing ribbons (~2–3 px, soft edges), not `EPT_LINES`.
4. **Every body's surface.** Each standable body's coarsest LOD level baked
   once into an equirectangular texture (the world builder's, the strip's
   icons and later the game sky's too); `SystemLayer` draws it lit. Giants
   and the star get a procedural look from their card. Focusing crossfades
   bake → tiles.
5. **Native overlays.** The painter moves into the engine: a layer is a
   field and a colour ramp, applied per tile cell as the tile is built (and
   to the bakes, so overlays show on every body). The page selects a layer
   by id; the page's painter stays only for the flat map until it moves too.
6. **The game sky turns with the game's camera.** (Added mid-sprint.) It
   moved a little faster than the world as the mouse turned the view.

Steps 4 and 5 share the painter: build 5's per-cell colouring first, and
the bake is its coarsest level. (As built, step 4 isn't a texture: each
body keeps its own tiles, which is the same LOD's look at every distance
and needs no handover.)

## Built (2026-10-31), for Cody to try

- **UI** (`MapScreen.vue`): the readout is gone; the body strip sits in
  the header between the title and Done; a clicked place shows its own
  card bottom left (lat/lon, cell, "Go there" in creative).
- **The star** is index `star()` (the bodies' count) in the Planetarium:
  a flight target and a body to go round (`radiusOf`, `centreOf`), its
  camera's reach the whole system. `planetarium.fly { body: "star" }`; the
  strip and a double click fly there, the map and the creator alike.
  Hover: a disc the mouse is inside beats a dot's reach round it, nearest
  in front among discs (it was the nearest to the camera, so a planet's dot
  drawn over the star took its click).
- **Orbits** (and the player's path) are `Ribbons` (`planetarium/ribbon.*`):
  2.5 px strips facing the camera, solid down the middle, soft edges.
- **The game sky** drew the system with the Planetarium's fixed FOV, not
  the game camera's: `OrbitCamera::perspective` takes the FOV, and
  `SystemLayer` passes its camera's.
- **Every body's land.** The Planetarium keeps a `far::Tiles` (one thread)
  for each other body of the atlas, made when it's first 6 px across,
  updated for an eye as far off as the camera, drawn in place of its sphere
  (`SystemLayer::Surface`) once its roots are built: the coarsest land
  from afar, finer on the way in, at the system's scale and turned by its
  own day. The menu's atlas now holds every body of the code's system (as
  the server lays them out), so the creator shows them too.
- **Overlays painted natively** (`planetarium/paint.*`, `Painter`): each
  tile vertex coloured from `World::column` (and the grid's fertility,
  currents, plates, provinces) where it stands, the ramps `fields.ts`'s;
  samples cached on a 2¹⁵ lattice. On every body drawn from tiles; in
  survival the player's own is masked to what's explored and the others
  aren't painted. A body's `World` is built on its own thread; its tiles are
  tinted when it's ready. `planetarium.layer { overlay, date?, explored? }`;
  the image form stays for the weather (a year the page simulates), now
  sampled smoothly.

Proved: engine builds; UI typechecks and builds; TestPlanetarium (9, two
new: an overlay changes 6 times across one pixel of the old 256² image; a
fresh sample costs ~4 µs; water kept under the water overlays), TestFar,
TestFarPalette, TestBodies pass.

Not proved (needs Cody's eyes): any of it on screen. Hover picking of the
star has no headless test (it needs a drawn frame). A new tile is painted
unbudgeted when it arrives (~2k vertices, a few ms with shared corners):
watch for hitches with an overlay on. Volcano marks aren't painted
natively yet. Giants and the star are still spheres in their colour
(no procedural look yet).

## Second pass (2026-10-31), from Cody's screenshots

- The header's title and Done keep to the top (the strip's moons made it
  taller and pushed them down).
- The player's marker is never taller than 12% of the body's radius (from
  far out it dwarfed the planet).
- Flights: a hop across the system rises over its plane and comes down
  (no swing through the bodies' far sides), and the camera's up turns from
  the start's to the end's on the way, so it lands in the orbit's own view
  (it was the world's up in flight, then snapped: the twist).
- Opening is instant: a hidden view is parked and shown again when the same
  body's asked for; a game prepares its body's view as it joins
  (`Planetarium::prepare`) and builds its coarse land while hidden (`idle`).

Not proved: any of it on screen. The first open still builds the live
palette (`Session::world`) each time it's asked, before the parked view is
found.

## Third pass (2026-10-31)

- The header lets clicks through between its pieces (it's as tall as the
  strip and covered the tools' top rows).
- Flights run in log space: out to where both bodies are seen and in
  again, each half eased, so the far body grows into view; what's looked
  at goes across in the middle of the way. Hops take 3 s.
- A parked view keeps its camera over the player while hidden (moved a
  hundredth of a radian or more) and builds until its tiles are settled,
  not just the coarse whole, so V opens at the detail it's seen at.
- The sky: 5,000 soft round dots, most faint (alpha 0.18 up, brightness a
  fifth power), a pixel or two across, coloured by temperature; over half
  crowd a galactic band tilted across the system's plane, with a faint
  mottled glow and a dark rift. One draw, built once a screen height; the
  game's sky shares it.

Not proved: any of it on screen.
