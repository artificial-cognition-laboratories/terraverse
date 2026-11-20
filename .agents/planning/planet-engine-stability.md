# Planet engine contract

The geometry and movement work is scheduled in
`planning/sprint-29-seamless-globe.md`.

## Accepted grid tradeoff (2026-10-26)

The playable world remains a sphere with a quadrilateral voxel grid. Eight
three-face junctions are intrinsic to that topology. We accept slightly
non-cubic cells in bounded regions around them; we do not accept visible
holes, forced landmark peaks, invalid neighbours, collision discontinuities,
camera turns, or position snaps. Distortion should be distributed smoothly
across the surrounding cells while normal building stays visually cubic.

The six face IDs and the unfolded net are storage addresses, not physical
positions. A player's or craft's motion and view must be continuous in
planet-centred metres. Face selection and map coordinates are derived for
block access. An abrupt map-coordinate change at a face transition must not
be interpolated as a physical move. Every boundary cell has one owner, and
adjacent faces use the same physical vertices for rendering and collision.

The current 72 m stone peak at each corner (`mapgen/terra_column.cpp`) is a
temporary cover for an unresolved junction, not the topology solution. It
should be removed only after a flat corner fixture passes the gates below.

### Corner acceptance gates

1. **Topology:** from every face, each cardinal edge step has one reciprocal
   neighbour; a query around a three-face vertex has a documented three-way
   result. No gameplay query reads the net's diagnostic gap as terrain.
2. **Geometry:** all three faces agree on the junction's physical vertices.
   A bare, level fixture has no missing or overlapping pixels from ground
   level or low orbit. The rendered surface and collision surface agree.
3. **Movement:** walk, jump, diagonal approach, and high-speed flight through
   all eight junctions preserve physical position, velocity and camera
   direction. Map-address changes are tagged transitions, never unexplained
   teleports. Test multiple frame steps and both travel directions.
4. **Building:** place, mine, point, rotate, light, liquid and multi-node
   structure operations on each of the three incident faces. A saved world
   reloads with the same block identity and ownership.
5. **Streaming:** all incident mapblocks arrive and activate by physical
   proximity. The far view and loaded blocks neither double-cover nor leave
   a gap at the join.

`TestPlanet::testFlyingThroughCorners` is the first fast-motion geometry
gate. It compares the mapped flight result with a direct planet-centred
integration at each of the eight corners. It cannot prove attached-player
interpolation, collision or rendered coverage; those need the later gates.

### First measurement

`TestPlanet` passes 22 tests, including fast 600 m/s crossings near all eight
corners at three altitudes. `bun check:flight` passes the live headless climb,
equatorial orbit and landing. Thus the orbital integrator and its current
map conversion preserve physical position in those cases. The observed
jetpack snap must be traced at the craft's `set_pos` / attached-player client
handoff, camera turn, or collision/streaming boundary before changing the
integrator. The local player's `Planet::carry` and `Planet::halo` still have
no defined diagonal continuation through a three-face corner. The present
`Flying::Step::turn` exposes only quarter turns; preserving a physical view
direction across a skewed corner grid needs a continuous heading transform.

The six cube-sphere faces remain ordinary Luanti voxel grids in storage. The
planet is first class at the boundaries where code asks about adjacency,
physical location, motion, or visibility. These are different questions; a
raw map coordinate must not silently answer all four.

## Coordinates and ownership

| Domain | Meaning | Owner |
|---|---|---|
| Map position | Node or mapblock address in one of the six squares | `Map`, map storage |
| Topological neighbour | Next node, including a face-edge turn | `Planet::step`, `Map::next` |
| Local view of an area | Nodes across one edge, rotated into the origin face | `Planet::halo`, `Map::see` |
| Physical position | Planet-centred metres, radial altitude and tangent frame | `Sphere`, `Flying` |
| View position | A physical point relative to the eye | `planet::Lens`, sphere shader |

The seam gap is a diagnostic boundary. Gameplay operations which hit its
solid node should be traced to the flat-coordinate path that bypassed the
planet operation; changing the seam to air would conceal the error.

`Planet::wrap` is for enumerating nearby mapblocks from a face's grid. It is
exact over one edge for up to a whole adjacent face, using `halo`. A square
continued diagonally through a corner has no unique voxel counterpart: three
faces meet there. Beyond one adjacent face it uses spherical projection and
can duplicate or skip mapblocks. It must not be used for individual node
neighbours, collision, or movement. Those use `step`, `see`, or `carry`.

## Invariants to keep green

1. Crossing any edge and stepping back returns to the same node, with inverse
   orientation. A block's type and rotation survive a read or mesh across it.
2. A moving body's physical position and tangent velocity are continuous at a
   face transition. Client prediction and server authority choose the same
   destination and turn, including when motion exceeds one node in a tick.
3. A collision, ray, liquid update or light propagation sees the same blocks
   at an edge as it does away from edges. At corners the three-face rule is
   explicit; no query silently reads the storage gap as a fourth face.
4. Blocks remain visually regular in the local play area. At distance the
   terrain, entities, clouds and effects share one spherical view transform;
   culling must use that transform or conservatively retain candidates.
5. A client receives and activates mapblocks by physical proximity, across
   faces. Render, interaction and sound range use physical distance rather
   than distance across the unfolded storage net.

## Engine audit

The first pass has hooks in `Map::next`/`see`/`distance`, collision, raycasts,
light, liquids, edge meshes, players and entities, block streaming, active
blocks, and the lens. A maintenance audit should cover **every** operation
that forms `p + offset`, a flat bounding box, or a Euclidean map distance:

| Area | Existing path | Remaining decision or check |
|---|---|---|
| Server block streaming and active blocks | `Planet::wrap`, planet-view cones | Exact one-edge mapping and view selection; corner and longer-range enumeration need physical-radius coverage tests |
| Movement | `Planet::carry` in local player, Lua entity and client entity; `Flying` for craft | Shared physical continuity test for position, tangent velocity, and orientation at speed |
| Collision and pointing | `Map::see` | Sweeps spanning an edge or corner; placed node rotation |
| Neighbours and area operations | `Map::next` and selected Lua APIs | Audit builtins, mods, VoxelManip and multi-node structures |
| Rendering | Sphere lens, conservative culling, shared terrain/cloud depth, fragment-level planet occlusion | Camera frame/skew, high-altitude depth precision, transparent ordering, shadows, particles and sound placement |
| Simulation | Sphere latitude/longitude, radial height | Verify sky, weather, geology and flight sample the same physical position |

## Verification gates

- Unit: `TestPlanet`, `TestPlanetMap`, `TestPlanetMesh`, `TestSphere`.
- Headless: `bun check:planet` for walking, water and falling nodes across
  every edge; `bun check:flight` for orbit and landing.
- Visual handoff: player views at face centre, edge midpoint and near each
  kind of corner, at ground level and altitude. Compare block angle, apparent
  size, horizon, entity position and camera continuity while turning and
  crossing. The user runs the client and supplies captures.

The near-view skew is a separate rendering issue from topological adjacency.
`Sphere::Lens` now blends from the local grid frame to a perpendicular
physical frame by 128 m. The orbital silhouette has a geometric regression
test, but the corner still needs the visual gate for near blocks, far-view
coverage and camera continuity.
