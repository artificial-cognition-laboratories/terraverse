# Sprint 41 — The planetarium: a system you can move round

## Decision

Bodies can be visited (sprint 40); now they should be seen properly, from
the map and from the ground. Seamless ascent moves to sprint 42.

1. **Camera and frame.** One scene for every body, orbit and the star; the
   camera kept in its body's radii; hover cards, double-click to focus;
   seams off by default; a plain star background.
2. **Layers in the map.** The world builder's layer buttons, one component,
   used by the fullscreen map too (the map's own set goes).
3. **Bodies by kind.** A body's kind (airless, cold desert, …) drives its
   generator: sea level, plant cover, palette. Replaces sprint 40's
   `// TEMP:` Earth-like bodies.
4. **Bodies in the game sky.** A small baked map image per body (the map's
   preview), drawn on a lit sphere where its orbit puts it at the game date.

## Step 1 (2026-10-29)

- Fixed: a body nearer the camera than the tiled body was drawn in the tiled
  body's spun view (`SystemLayer::draw` left it set after the focused body's
  draw): focused on another body, it sat off its orbit, off centre, and
  wheeled with the spin and the camera. The scene's view is set again after.
- Fixed: a planetarium rebuilt (a size change in the world builder) kept the
  old camera in blocks: a smaller body could swallow it. It keeps the camera
  in the body's radii (`keep`), and a flight's end if one was on.
- Seams are off by default (the engine, the shell's host and the builder).
- The camera round the tiled body is in its ground's frame (`offsetOf`):
  the land stays still under it as the body spins; the star and the rest
  move instead (as Google Earth). Play/pause stays for the date.
- The camera is a direction and a screen-up, not a latitude and longitude:
  a drag rolls it round the body, over the poles too (no gimbal lock; north
  is up only after a focus). `resize` keeps it as high in radii.

Proved: TestPlanetarium (6, `testDragOverThePole` new).

## Step 1, the system's look and hover (2026-10-29)

- Orbits: each body's own colour, bright just behind it and fading round to
  where it's going (KSP's trail); moons' dimmer; the hovered body's bright.
  512 pieces an orbit (was 160). The lines are still 1-pixel and aliased:
  left until the game's look is worked on.
- A star field behind everything (2,400, a fixed seed; squares a pixel or
  two across at the screen's height).
- Hover: the Planetarium finds the body under the mouse each frame (the
  nearest whose disc, at least 10 px, holds it; not the body the camera's
  round while it fills the screen) and the host tells the page
  (`hovered`: id, name, kind, its disc in page pixels; the star "star").
  `HoverCard.vue`: four corners closing round the disc, a readout beside
  it. Hidden while the pointer is on page UI (the page takes the mouse
  there, so the engine never hears it leave).
- Double-click a body: flies to it (2.5 radii); the star: out to the whole
  system. The host says `chosen`: the builder selects it; the map shows it
  when it can be stood on (a giant is only looked at).

Proved: TestPlanetarium (7, `testCentreIsAhead` new: the projection the
hover uses); the page typechecks. Not proved: any of it on screen (Cody).

## Step 2, shared buttons (2026-10-29)

- Hover no longer flickers on an edge: what's hovered keeps it 14 px past
  its disc (4 to start), and the body the camera's round stays hoverable
  to half the screen once hovered (0.4 to start); the card lingers 180 ms
  after the engine drops it.
- `system/BodyStrip.vue` (was the builder's `BodyNavigator`): the star,
  each planet's globe icon, its moons under it; `tag` for a word under one.
  The builder (`play`) and the map (`you`) both use it.
- `worldgen/LayerRail.vue`: the builder's layer tiles, two columns, a slot
  for a screen's own (seams, the cutaway). The builder and the map use it.
- The map: the strip bottom right, every body of the system (the world's
  system is the starter round its code's star, so the page makes it as the
  builder does; `system.preview` is the game session's too, for the
  sketches). A body to stand on is shown (its land); the star flies out to
  the system; a giant is looked at. Layers on the globe as well as the flat
  map (every one but the weather); on another body in survival none (the
  player's explored record is of where they are).

(The page's copy of the starter system this left open is gone: step 3.)

## Step 3, one system and bodies by kind (2026-10-29)

- One system, the engine's. The page's starter copy (`STARTER`) is gone:
  `system.home` (the menu's) gives `SystemSpec::home` in the page's shape
  (`shell::systemJson`: each body's causes in page units and its swatch,
  now kept in the spec), and a new world's draft starts from it;
  `system.world { code }` (the game's) gives the world's system and every
  body's card, and the map's strip is that. The copies had already parted:
  the engine's Moon orbit is tilted 5 degrees, the page's wasn't. The
  gallery has a canned system of its own (no engine there).
- Bodies by kind: `terragen::Surface` (Living, Rock, Sand, Ice), not in a
  code's text, set by `bodyWorlds` from the body's card (airless and
  volcanic: rock; cold desert, hothouse, hydrocarbon: sand; ice shell: ice).
  A barren body has one bare biome all over (Rock, Desert, Snow), no sea,
  lakes or rivers (its basins stand dry), nothing growing. Its ground,
  biomes and cover are the world model's, so the generator, far view,
  planetarium, map and minimap all follow. The home is as it was (the
  terragen golden world unchanged). Sprint 40's Earth-like TEMP is gone.

Proved: TestSystem `testThePagesSystemIsTheEngines` (the page's draft, sent
back as causes, gives the engine's cards); TestBodies
`testABodysLandIsItsKind` (the home has water, plants and many biomes; the
moon is Rock only, dry, bare; Nacre is ice); TestTerragen, TestFar,
TestPlanetarium pass; check:bodies passes (the moon's ground basalt, dry
below the old sea level); the page typechecks.
