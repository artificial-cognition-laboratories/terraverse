# Sprint 26 — The planet, for real

Sprint 25's spike proved it: one face of a cube-sphere, drawn from the
engine's own blocks, is a planet you can stand on, build on and see curve
away. This sprint makes the whole planet: six faces you can walk, build and
fly across, poles and all, with no polar wall. It's in five phases, and
each is finished under its own tests before the next starts.

## The shape (decided 2026-10-24)

**The six faces are laid out flat in the engine's coordinates, as an
unfolded net.** Positions, the map database, the network and the Lua API
stay as they are: a position is still a `v3s16`. Faces are regions of the
map, and stepping off one face onto another goes through one remap.

```
 z ↑   +--------+   +--------+   +--------+
       |   3    | g |   5    | g |   4    |
       +--------+   +--------+   +--------+
           g            g            g
       +--------+   +--------+   +--------+
       |   0    | g |   1    | g |   2    |
       +--------+   +--------+   +--------+   → x
```

- **Size.** A face is 20,592 blocks across (half = 10,296, a whole number
  of map blocks). Three faces and two gaps are 61,808 blocks: all the map
  generator makes with its default chunks (it makes whole chunks only,
  from -30,912 to 30,927). That gives a radius of ~13.1 km and a 1 m block
  along the centre lines (0.71 m at the corners). Orbit is ~360 m/s with an
  orbit every ~4 minutes, and the horizon on foot is ~230 m: you can see
  that it's round. It can grow later by widening coordinates to 32-bit,
  with nothing else changing.
- **Gaps (g).** Every pair of faces has one map block (16 nodes) between
  them, filled with `seam`: a solid node that's never generated over,
  never walkable into and never drawn. Only 4 of the net's 7 inner joins
  are real edges of the cube anyway. With gaps everywhere there's one
  mechanism, not two. Code that steps naively across a join hits `seam`,
  never the wrong face's blocks, so a missed crossing is a visible wall,
  not silent corruption.
- **The net is data.** A table holds each face's origin and quarter-turn
  in the net. The 24 face sides and their partners are worked out from
  `terragen::Sphere` (block for block, a whole-number turn) and checked by
  tests, not written by hand.
- **The spare z** (~21,900 blocks north of the net) is kept for later: other
  bodies and ships.
- `MAX_MAP_GENERATION_LIMIT` stays at 31007 (changed from the plan:
  raising it to the s16 edge puts loops like `for (y = min; y <= max; y++)`
  one step from overflowing). The server refuses a world whose chunks
  don't hold the net.

## The noun: `Planet` (src/planet/, no engine dependencies)

`Planet` owns the net. It answers every question that crosses a face:

- `face(p)`: which face a node is on (or a gap, or outside).
- `step(p, dir)`: the node next to `p` in direction `dir`, across a face
  edge if need be, and the quarter-turn taken. Every neighbour lookup that
  can reach an edge goes through this.
- `turn(param2, ptype, turn)`: a node's rotation (facedir, 4dir,
  wallmounted, degrotate) carried across an edge. Stairs stay stairs.
- `place(p)` ↔ `Sphere::Place`, `latitude/longitude(p)`, `point(p)`: the
  planet's frame (`terragen::Sphere`).
- `carry(pos, vel, yaw)`: a position past an edge (fractional, an object
  or a player) onto the neighbour face, with its velocity and facing turned.

Sprint 25's `Sphere` stays the geometry under it.

## Phase 0 — The far view on the sphere

The far view draws its tiles with `Sphere::Lens`, the same sums as the near
world. Its own globe (`Map::share/across/curve`) goes. Near blocks and far
tiles are one curved surface, and from altitude you see the whole face.
Still one face. It also fixes the spike's warp past an edge: anything off
the eye's face is hidden until phase 1.

Tests:
- The far shader's lens matches the C++ one (the same float tests).
- `check:sphere` gains a render: the near world and the far view meet with
  no step at the near world's edge.

### Phase 0 outcome (2026-10-24)

- The far shader places its tiles with `sphereLens` when the sphere is on,
  so the near world and the LOD are one surface. From 2.5 km over a
  2,000-block face the whole face is a ball with the LOD terrain on it.
- Past the eye's face edge, geometry is pinned to the edge (it flattens to
  nothing). Discarding vertices left coarse far-view triangles straddling
  the edge stretched into streaks; pinning doesn't.
- The far view's range on the sphere reaches the planet's horizon (at most
  the face), and its tiles aren't frustum-culled by their flat boxes.
- Changed from the plan: the old globe (`Map::share/across/curve`) stays
  until phase 4. Worlds are still flat until then, and it draws them.
- Tests: the lens's float test now reaches the face's far side (a part in
  10⁵). `check:sphere` has the far-view render (whole face vs near speck).
- Left: the far view's haze still measures distance flat, which shows as a
  pale band at the horizon from low down. It's fixed with the sky in phase 4.

## Phase 1 — Six faces, drawn

- The map is the net: mapgen fills each face (flat placeholder terrain is
  fine) and `seam` in the gaps. `Planet` is built from the world's size,
  and the server sends it to the client (replacing the TEMP setting).
- Each map block belongs to a face. The client draws a block with its own
  face's lens, placed against the eye in doubles on the CPU: per mesh, the
  block's origin in the eye's frame, so the shader only ever sums small
  numbers. Standing near an edge you see over it onto the next face.
- Block meshes at a face's edge take their neighbours across the edge, so
  there are no stray side faces drawn along a join.
- The far view draws all six faces.

Tests:
- Unit: the net table, `face()`, and `step()` round trips across all 24
  sides; each turn applied four times is no turn; the 8 corners.
- Render (off-screen): from orbit-ish height over a corner, all three
  faces are drawn and meet without cracks (pixel checks along the joins).

### Phase 1 outcome (2026-10-24)

- `planet::Planet` (src/planet/): the net, `face`, `nearest`, `place`/`at`,
  `step` (with the turn), `wrap` (a grid carried past an edge, onto the
  face the planet has there), `distance` (through the planet) and
  `glslAxes`. `planet::Lens<T>`: the planet seen from an eye anywhere,
  which the shader copies line for line. TestPlanet (15): every side of
  every face, all 8 corners closing with three blocks, turns following the
  ground, the lens against the planet itself (on and off the eye's face,
  points on edges), no cracks either side of an edge, floats against
  doubles, wrap and distance.
- The world is a planet by its map setting `planet_half` (any mapgen),
  known before mods load. The server sends it (`TOCLIENT_PLANET`, protocol
  59, before the horizon), and the TEMP spike setting is gone.
- Drawn: `sphereLens` places each vertex from its own face. On the eye's
  face it uses the precise sums; off it, the planet seen from the eye. Fog
  goes by the curved distance (`sphereEyeVec`). The wielded item and HUD
  models aren't lensed (their view isn't the world's).
- Near on the planet, not in the net: the server's send shells are the
  player face's grid wrapped over edges (`Planet::wrap`), and its sight
  cone judges where the player sees a block (`planet::Lens`). The client
  draws every loaded block within range over the planet (the loops
  culler), with flat-map frustum and occlusion culling off. Without this
  the face next to you on the planet, 1,000+ blocks off in the net, never
  loaded.
- The far view cuts terrain cells at face edges, and judges a tile's
  distance over the planet.
- `tf_planet`: the seam node (solid, invisible, never drawn) fills
  everything off the faces; players spawn on face 0; `core.planet` has
  `enabled`, `half`, `face`, `square`.
- `check:sphere`: TestSphere and TestPlanet, the real shader prelude and
  lens compiled, and renders: a flat world (unchanged), a small planet as
  a disc, and a corner where three painted faces meet.
- Found by the renders: the lens curved the wielded item; fog went by flat
  distance; `core.planet` was off while mods loaded; an 80-block strip of
  the net could never be generated, and the send loop waited on it forever.
- Not yet: the far view on a planet shows nothing until terra generates on
  the sphere (phase 4; it reads the net as past its polar wall). Blocks a
  long way round a corner come through `wrap`, which is only approximate
  there, and it'll be replaced by stepping in phase 2.

## Phase 2 — Crossing edges (the engine fork)

Every place the engine asks "the node next to this one" learns `step()`.
This is the list, and the sprint isn't done until each has a test:

| Where | What changes |
|---|---|
| `Map::getNode` neighbours, `MapBlock` neighbour access | through `step()` at a face edge |
| `VoxelManipulator` / `MMVManip` | filling an area that crosses an edge pulls the neighbour face's blocks turned into this face's grid, and writes them back turned the other way |
| Mesh generation (`MeshMakeData`) | its neighbour copy crosses the edge |
| Lighting (day and artificial) | spreads across the edge |
| Liquids (transform, flowing) | flow across the edge |
| Falling nodes, attached nodes | fall and check support across it |
| Collision (`collisionMoveSimple`), on server and client prediction | node boxes near an edge come from across it |
| Raycasts and pointing | a ray past an edge carries on, on the neighbour face |
| Objects and players | `carry()` when they cross: position, velocity, yaw; the client's camera turns with it so nothing jumps |
| ABMs/LBMs neighbours, `find_node_near`, area queries | across edges (or documented as one-face) |
| Node rotation | `turn()` wherever a node crosses an edge (schematics, pistons later) |
| Active blocks (objects, ABMs, node timers near players) | near on the planet, not in the net, as block sending is |
| Server culling | the flat map's occlusion rays are off on a planet (phase 1); anything else that culls by flat position likewise |

Corners: three faces meet, so a step from a corner block can only go
three ways round the point. `step()` makes that exact; the corner
landmarks (phase 4) keep players from finding it awkward.

Tests:
- Unit, engine-level (a real `Map` in the unit tests, as test_map does):
  place, dig, read across every edge and corner; a VoxelManip spanning an
  edge round-trips; light crosses; water poured at an edge flows over it
  and settles the same as on flat ground; sand falls across; a collision
  box sliding over an edge never snags or falls through.
- Headless server check (`check:planet`, like far-check): a bot walks,
  builds and digs across every one of the 12 edges and all 8 corners,
  pours water and drops sand at each, and checks the map afterwards.
- Client: a player crossing an edge in an off-screen run has no jump in
  camera position or direction (frame-to-frame check).

### Phase 2 outcome (2026-10-24)

Two primitives carry it, both on `Map` (which holds the planet, server and
client):

- `Map::next(p, dir)`: the node next to p (over an edge onto the next face,
  with the turn). For what walks node to node.
- `Map::see(from, p)` / `getNodeSeen`: what's at p as seen from `from`'s
  face. Past an edge it's the halo (`Planet::halo`: a step across, then on,
  exact; nothing round a corner), the node turned into this face's grid
  (`rotateAlongYAxis`, so a stair faces on). For what looks at an area round
  something.

Wired:

| Where | How |
|---|---|
| Collision (server and client) | `add_area_node_boxes` reads off-face nodes through `getNodeSeen` from the mover |
| Players | `LocalPlayer::carry`: off the face, onto the next (`Planet::carry`, exact at the crossing), speed turned; the camera's yaw turns with it (`takePlanetTurn`) |
| Entities | `LuaEntitySAO::carry` on the server; `GenericCAO::carry` for the client's own prediction, and a carried object is put there, not glided across the net |
| Anticheat, interact distance | over the planet (`Map::distance`) |
| Meshes | `QueuedMeshUpdate::halo`: the border past an edge, read on the main thread and written over the seam before meshing; edits and new blocks at an edge remesh across it |
| Raycasts, pointing | read through `getNodeSeen` from the ray's start; the pointed nodes are the real ones (dig, place); the selection box is lensed |
| Liquids | `transformLiquidsLocal` steps with `next` |
| Light | `step_on_planet` in voxelalgorithms: spreading and unspreading cross edges, the source direction turned |
| ABM neighbours | through `getNodeSeen` |
| Active blocks, objects sent | wrapped round players (as block sending); objects' range over the planet; on the client their culling is off |

Tests:

- TestPlanet (18): adds `halo` (20 deep off every edge, step-consistent) and
  `carry` (exact at the crossing, round trip).
- TestPlanetMap (7): `next`, `see` with a stair turned right, walking over
  every edge (and the control: read flat, the gap's wall stops it), pointing
  across, light across and back. TestPlanetMesh (1): the mesh halo.
- `check:planet` (new): a headless small planet; off every side of every face
  (the middle and beside both corners) a walker crosses onto the right face
  at ground height still going, water poured on one side flows onto dry
  ground across, sand lands past the edge. 344 checks.
- `check:sphere`: adds a client walking itself (continuous_forward) north
  off face 1, where the grid turns: it ends up on face 4 looking west in
  face 4's grid (straight on), the server's anticheat doesn't reset it.

Not crossing yet (phase 3, Lua, or later):

- builtin's own neighbour arithmetic: attached nodes (torches on walls
  across an edge), placement's facedir from the look direction (a stair
  placed across an edge is turned from what the placer saw),
  `find_node_near` and area queries.
- Connected node boxes (fences) don't connect across an edge; sounds and
  particles across one are placed at the net position (heard and seen in
  the wrong place); mapgen decorations (trees) at an edge are cut by the
  seam, which phase 4's mapgen on the sphere replaces.
- Round a corner there's only seam: walking diagonally into the corner
  point meets a wall the width of a block (the landmark will cover it).

## Phase 3 — Lua

- `pos` stays a position. New `core.planet`: `face(pos)`,
  `step(pos, dir)`, `latitude/longitude(pos)`, `up(pos)` (always +y for
  blocks: columns are radial), `carry(...)`.
- builtin's own neighbour arithmetic (falling, liquids if any, item drops,
  `core.find_node_near`) goes through it. The game's mods are checked for
  `pos + offset` near edges and moved to `core.planet.step` where it matters.
- Mods that never think about edges still work: they just meet `seam` at
  a join. That's a wall, not a crash or wrong data.

Tests: Lua unit tests for `core.planet`, and `check:planet`'s bot driven
through the Lua API.

## Phase 4 — The planet

- **Mapgen.** terragen generates a column from its latitude and longitude
  on the sphere (`Planet::place` → `Sphere`), not the equirectangular map.
  It has poles, and climate, seasons and the sun line up with the sphere.
  The polar wall and the space band past it go.
- **The corners.** Each of the 8 gets a landmark (crater, peak, monolith;
  generated, data-driven) so no one builds across one by accident.
- **Height.** A block at height h is (R + h)/R metres across: columns
  widen. Movement and flight convert true metres to blocks by it, as
  `core.terra.across` does now. A craft high up crosses blocks faster than
  its true speed suggests, and orbit is just flight.
- **Orbit and air.** `core.orbit` works in the planet's frame from
  `Planet::point`. The map's top (~32 km, 2.3 R) is above low orbit, so
  orbiting is flying through the net. The air's scale height is set for a
  13.1 km planet. Space bubbles go, or move to the spare z for other bodies.
- **Sky.** Clouds and rain are drawn with the lens: a shell around the
  planet, not a ceiling.

Tests:
- terragen: `check:terragen` on the sphere (land share, rivers to the sea,
  climate bands by latitude); columns agree across every edge (no cliffs
  along a join).
- Orbit: the flight check flies a craft up, around the planet once (over
  every face), and back down.

### Phase 4 progress (2026-10-24): the planet you can see

- terra generates on the sphere: a map column is the world model's at its
  latitude and longitude (`Planet::equirect`; `terra::Columns` remaps on the
  way in, so the generator, the far view and `core.terra.surface` agree).
  Rock, ore and caves keep the column's own coordinates (no stretching at the
  poles). The poles are the world model's polar caps; the polar wall and the
  space band never come up. `planet_half = 1024` is the default world's size
  round (8 × 1024 = 8192): one block a block.
- `core.terra` on a planet: sun, climate, geology, weather and cover at a
  position go by its latitude and longitude; `globe`/`block` are the
  sphere's; `across` is a metre a block on the ground, widening with height.
- The far view draws the whole planet (cells cut at face edges); clouds are a
  shell round it (the cloud shader is lensed).
- Spawning searches outward from face 0's middle for land.
- Tests: TestFar's `testOnThePlanet` (the poles are the world's poles, the
  faces' middles its meridians, and blocks either side of every edge are
  neighbours in the world model: no cliff along a join).
- Seen off-screen: the planet whole from 2.5 km (seas, coasts, continents),
  and from the ground (grass, trees, terraces, rain at 33 °C on the equator,
  clouds curving down to the horizon).

Left in phase 4: the corner landmarks; orbit and air for a planet this size
(and the space bubbles); the haze measured over the curve.

## Out of scope

- Coordinates wider than s16 (the planet's size grows with them later).
- Other planets and moons: the spare z is kept for them.

## Done when

You can walk from the equator over a pole and back, across edges and past
a corner, building as you go. Water and sand behave at every edge. You can
fly up and see the whole planet with the LOD on it, orbit it in a few
minutes, and land. `check:sphere`, `check:planet`, `check:terragen`,
`check:flight` and the unit tests all pass.

## Order

0 → 1 → 2 → 3 → 4. Phase 2 is the biggest and gets the most tests.
Phases 0 and 1 are the ones you can fly.
