# Sprint 31 — The Planetarium

## Decision

Terra Firma gets one planet viewer: the **Planetarium**. It is native,
always drawn underneath, and the web view lays its panels on top when it
wants to. It replaces the three globes we have now:

| Today | What it is | After |
|---|---|---|
| `shell/orbit.cpp` | the menu background: a fixed 128×64 grid planet, moon and clouds | the Planetarium, menu host |
| `ui/src/worldgen/globe.ts` | WebGL2 in CEF, a heightmap on a sphere (create screen, in-game map) | the Planetarium, create and map hosts |
| `far/` (`far::View`) | in game, past the loaded blocks: a quadtree of tiles from terragen | the Planetarium's tiles; the in-game far view is one of its eyes |

There aren't three viewers. The far view is the Planetarium seen from the
player's eye. It is already what we need: tiles from the ground out to
orbit, built from the generator's own rule, and independent of how blocks
are stored. So the Planetarium is a different eye on the same tiles, not
a new renderer.

The community map stays as it is: it shows other people's worlds, not
this scene.

## The model

```
system (terragen/body: causes → bodies)      the truth: shared, pure
  └─ Planetarium (client)                    a scene: bodies, a camera, layers
       ├─ far::Tiles   per body              choose / build / keep tiles for an Eye
       ├─ Camera       orbit · focus · zoom  makes the Eye
       ├─ Layers       atlas seams first     drawn over the tiles; a function on the sphere
       └─ bridge       planetarium.*         what CEF may ask and is told
hosts: debug screen · create screen · in-game map · menu · far view (the player's eye)
```

- **The system is the source of truth.** `terragen/body` already models
  bodies as causes, stepped through time. The Planetarium reads a system
  and never derives anything itself. This sprint the system has one body;
  several bodies, orbits on rails and time are sprint 32.
- **An Eye** is what tiles are chosen for: a point in the body's frame (in
  doubles, from the body's centre), how far it reaches, and how fine it
  wants tiles. The player's camera makes one; the Planetarium's camera
  makes another.
- **Precision:** doubles in the system and the eye, with tiles drawn
  relative to the eye in floats, as the far view does now. Reversed depth
  goes in once there are several bodies (sprint 32), but the eye-relative
  rule is set now.
- **Bodies are drawn from the generator, never from map blocks.** Changes
  players make to the world show only near them.
- **Layers** are functions on the sphere (or through it, for the cutaway)
  that colour or overlay tiles. Adding one never changes the viewer. This
  sprint has one: the atlas's segments, caps and seams.

## What has to change in far/

Reading `far/` found two couplings. Both are fixed by moving the code
where it belongs, not by adding a second path.

1. **Tiles are tied to the player.** `View::update` takes the `Camera`
   and the `ClientMap`, and `Layout::look(eye, map)` reads the near world.
   Choosing, building, keeping and evicting tiles move into **`far::Tiles`**
   (`choose`, `install`, `evict`, workers), which takes an Eye. The near
   world's mask, the fog and the haze stay in `far::View`, the in-game host.
   `Layout` splits the same way: what the planet is (roots, distance,
   transform, mesh) separates from what the near world draws (mask, surfaces,
   covered).
2. **Tiles need the game's content.** A tile is made of blocks (`content_t`
   from a server-sent `far::Horizon`), and `far::Colours` reads their
   textures through the `NodeDefManager`. The menu has neither: no server,
   and no game Lua run. Proposed fix (to confirm): **a palette, baked from
   the game.** The game's colours per block (what `Colours` computes) and its
   `Horizon` blocks and species are exported once, by a headless server at
   build time (`bun build` step, checked), to `games/terrafirma/palette.json`.
   In game, the same palette comes from the live content. One colour path
   in both places: the menu's planet is the game's planet, block for block.
   - The other option, a biome colouring for the menu, is what `orbit.cpp`
     does now. It means two colourings that drift apart, so it's rejected.

## Phases

Each phase ends playable and tested.

### 1. Tiles take an Eye (no visible change)

Done 2026-10-28, but for Cody's play test:
- `far::Tiles` (`far/tiles.h`) owns a planet's globe tiles for a
  `far::Eye` (a point over the sea, altitude, cut, range, split): choosing,
  building on workers, installing, evicting. It knows nothing of the camera
  or the map; a tile's mesh is in true positions round its middle, and
  whoever draws it places the middle.
- `far::View` is the in-game host: the player's eye, range, haze and fog
  from the camera; the mask; tiles placed through `AtlasView`.
- `far::Layout` is gone. With one planet kind left it had one
  implementation, and its surface/covered machinery was the net's (stubs
  on the globe); the mask is now the drawn blocks' columns alone.
- `core.terra.surface` on an atlas is the column the generator builds at
  a storage column's place (no trees: they're planted per chunk).
- `check:far` and `check:terragen` run on atlas worlds and pass: far
  31,065/31,065 columns (39 with a tree rooted in their top block left
  out); terragen 37/37, core.terra 37/37, weather 37/37.
- Gate: TestFar, TestAtlasView, TestPlanetMap pass. Still to do: Cody
  plays the far view (it should look as before).
- `Tiles` has no unit test of its own yet: `Colours` needs the node
  definitions and textures. Phase 2's palette makes it constructible
  headless; the test goes in then (finer tiles near the eye, none past
  its range, the fallback while refining).

### 2. The palette

Done 2026-10-28:
- `far::Palette` (`far/palette.h`): how each block looks from afar, as
  plain data (each face's texture average, whether it takes the node's
  colour, the node's colour: one, or 256 by param2 for a palette, and a
  liquid's colour over black and clarity). Replaces `far::Colours`; its
  live builder (`Palette::live`, client: textures) is the old code.
- `far::Horizon` split: the world's part (code, sea) and the game's
  (`far::Content`: the generator's blocks by role, the tree species).
- `far::Baked` (`far/baked.h`): content and palette by block name, as
  JSON. Read back, its blocks get ids of their own, consistent across
  content and palette.
- The export, `luanti --export-palette <file> --world … --gameid …`
  (`far/export.h`): the world's mods loaded by `Server::init()` (no
  thread), the server's images given to the client's texture source on
  the NULL video driver (no window; CEF never starts), and
  `Palette::live`, the far view's own code. `bun palette` writes
  `apps/game/game/palette.json` (48 blocks, 10 species); `check:palette`
  fails if it's out of date. Regenerating is explicit: a build never
  rewrites a committed file.
- `Server::horizon()` is the one place a terra world's horizon is made
  (`SendHorizon`, `core.terra.surface`, the export).
- Tests (`TestFarPalette`): baked round trip, bit for bit (doubles at 17
  digits: the niches); the shipped palette names a look for every block
  its content places; `Tiles` for an eye on a small planet from the
  shipped palette, no game: tiles come, none past the eye's reach, the
  nearest finer than the farthest.
- The gate (a tile from the baked palette is the tile in game, colour for
  colour) holds by construction: both are `Palette::live` over the same
  content and image bytes, `check:palette` holds the file to a fresh
  export, and the round trip is exact.
- Found on the way: `blocksIn` left out the ground blocks species grow
  on; it names every block the content does now.

### 3. The Planetarium scene (debug host)

Built 2026-10-28, for Cody to look at:
- `planetarium/` (client): `Planetarium` (the palette, a body's
  `far::Tiles`, the camera, the layers; raw input in, a frame out),
  `OrbitCamera` (latitude, longitude, altitude over the body, looking at
  its centre; drag orbits, slower the lower it is; the wheel zooms from 6
  radii out to 100 blocks over the sea; doubles, camera-relative floats to
  the GPU) and `SeamLayer` (the atlas layer: each segment's west meridian,
  cap circle to cap circle, and the cap circles, 2 blocks over the sea).
- Tiles drawn with the built-in solid material: their vertex colours carry
  each face's shade, so no shader of its own yet (it runs the same in both
  hosts). Sun lighting, day and night, wait for the layers that need a
  shader.
- Hosts: in game, F7 (`keymap_planetarium`) opens it over the whole
  window and F7 or Escape closes it; while it's open it has the mouse and
  the keyboard (`InputHandler::capture`) and the player stands still.
  Alone, `luanti --planetarium <code>`: a window on the planet a world code
  makes, from the baked palette; no world, no server. The menu entry
  needs the bridge (phase 4); this is the native side it will open.
- Tests: `TestPlanetarium` (zoom stays over the body, drag slows near the
  ground, the centre is drawn mid-screen); the standalone host runs headless
  on the NULL driver (8 s of building and drawing, no errors). The frame
  time gate (under 8 ms at 1080p) needs Cody's machine: measured with him.

### 4. The bridge

- `planetarium.show / focus / camera / layers` from CEF; `picked {lat, lon}`
  and `camera {…}` back. A JSON contract, validated at the bridge, like
  `show_screen`.
- A minimal Vue panel: layer toggles, camera reset, the lat/lon under the cursor.
- Gate: a bridge contract test (a bad message throws, by name).

### 5. Replace the globes

- Create screen: the scene focused on the body being made, regenerated
  when the seed or a cause changes. Then delete `globe.ts`,
  `GlobeView.vue` and the WebGL code that only it used.
- In-game map: the scene full screen with the player's marker. Then delete
  `MapGlobe.vue`'s globe.
- Menu background: the scene in slow orbit. Then delete `orbit.cpp`.
- Gate: no globe left outside the Planetarium (`rg` check); Cody runs
  all three.

## Not this sprint

Several bodies, orbits on rails and time (32); the cutaway (it needs the
interior model); weather and climate layers; trajectories; the menu's
fly-in to "new world"; rotating the atlas by seed.

## Phase 0: the cube net retired (done 2026-10-28)

Every planet is an atlas, so the far view has one layout before phase 1
splits it. Removed: `planet::Planet`, `planet::Flying`, `terragen::Sphere`,
the sphere lens (`client/sphere_lens.*`, `registry/shaders/sphere`, every
vertex shader's call), the net far shader and `far::netLayout`, the net's
paths through Map, collision, light, block sending, active blocks, clouds,
meshes and entities, `TOCLIENT_PLANET` (protocol 62), `planet_half` /
`planet_mapping`, the `planet_atlas_worlds` setting (new worlds are always
atlases), `core.planet.enabled/half/face/square/step/fly/state`,
`core.terra.globe/block/across`, the flight pack (`tf_flight` keeps the
walking body), and the checks `sphere`, `planet`, `planet-grid` and
`flight`. A world made on the net is refused by name.

Atlas gaps found and fixed on the way (each had only a net path):
- `Map::distance` goes through the planet (the server's dig reach and
  object ranges measured storage distance across seams);
- the camera's far plane reaches the planet from orbit;
- a node edited at a seam remeshes the blocks that show it across;
- the client carries entities over seams and puts, rather than glides,
  one the server moved into another grid;
- `core.terra.world_column` and `cover` read the place (the map's player
  position, weather, vegetation);
- the in-game map's player marker (`mapPose`).

`TestPlanetMap` now holds Map's crossing to the atlas (`next`, `see`,
`distance`).

### Still open, found on the way

- **Active blocks aren't carried over seams** (`ActiveBlockList`): ABMs
  and objects across a seam near a player are inactive.
- **Clouds are a flat sheet in storage space** on an atlas.
- **The flight HUD and trajectory UI are fed by nothing** until flight
  returns (`FlightNear`/`FlightSpace`, `map_trajectory`).
- `check:atlas` builds the deleted lab: `scripts/atlas-check.sh` is still
  there for Cody to remove.

## Outcome

(filled in at the end)
