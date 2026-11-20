# Sprint 84 — Loose ground

The first step of destruction (matter in motion). The world's matter can be
**bonded** (as now: sixteenths held in place, asleep), **loose** (grains
that heap at their angle of repose and slide) or **debris** (chunks as Jolt
bodies). Destruction is matter moving between them. This sprint builds the
loose layer, the one everything else drops into: a dune that flows when a
car climbs it, ruts worn by passes, mud that holds until it's pushed.

Later sprints, on this one: breaking (explosions from the chemistry's energy,
crater scaling, Voronoi chunks as Jolt bodies, thrown mass landing as loose
depth; crashes and felling by impact energy against bond strength), then the
support graph (buildings, trees and mine roofs that stand or fall).

## Rules

- **What persists, the server decides; what's gone in a second, the client
  draws.** Ground is gameplay (it's driven on, dug, holds water), so its
  shape is the server's, to the sixteenth. The client predicts with the same
  rule and is corrected; only dust and spray are its own.
- **Approximate, plausibly.** No grains, no velocities: the excess over a
  slope moves downhill and stops. Not true to life, never implausible.
- **Performance first.** Asleep it costs nothing. Target on a 2080 Ti: under
  1 ms server step and 1 ms client a frame while a car churns a dune; a
  few ms spike at most, spread over frames.

## Plan

1. **Piles: loose ground stored to the sixteenth.** Today smoothed ground is
   computed from its neighbours every time it's asked (`ground::Top`),
   nothing stored. A node whose loose ground has moved keeps a **pile**
   (`pile::Pile`, beside `shape::Shapes` and `fill::Fills` in the map block,
   map format 32): 16×16 column heights in sixteenths (0..32, rising into
   the cell above as `Top` does) and its material. Undisturbed ground stays
   unstored and computed; the first disturbance writes its `Top` as the
   pile. `Map::solidShape` reads piles first, so collision, digging, the
   water's floor and meshing see them with no other change.
2. **The flow rule, in integers.** Per column pair, the excess over the
   material's angle of repose moves half-way downhill (rounded to
   sixteenths, conserving the total exactly). A yield stress (mud, wet
   clay) means nothing moves until the slope or a push passes it. All in
   integer sixteenths and a fixed order, so the client and server reach the
   same answer from the same start. Angle, yield and density are data:
   from the material's matter (`_matter`), wet sand steeper than dry by the
   water in its mixture.
3. **Run where it's touched, then sleep.** Tiles of columns (as the water's
   grid), awake only where something stirred them, asleep after a step with
   no move. No velocity state, so a tile settles in a handful of steps. The
   server steps awake tiles at a fixed rate (20 Hz) within a budget, busiest
   first (as the world's matter does).
4. **Contact.** What presses on loose ground moves it: wheels (the vehicle
   constraint's contacts), the body's feet, falling debris later. Under a
   contact patch, by its pressure against the material's bearing strength,
   ground is **compacted** (lowered, denser: a compaction per pile column,
   so passes wear a rut that stops deepening) and **displaced** (pushed to
   the sides and ahead, as the water's wakes are). The wheel then sinks and
   drags by how soft it is: Jolt's vehicle reads the pile under each wheel.
5. **Sending.** The server sends pile changes per map block
   (`TOCLIENT_PILES`, protocol 88): a few times a second while a tile moves,
   exactly when it sleeps; only the columns that changed, compressed.
6. **Client prediction.** The client runs the same rule on the same piles,
   from its own contacts (its own car, its own feet), so its ruts appear at
   once. The server's piles replace its own when they arrive; they agree
   unless someone else touched it. Map blocks aren't remeshed while a pile
   moves: moving tiles are drawn as height-textured voxels on the GPU (the
   water's grid drawing, shared), and baked into the block's mesh when they
   sleep, a few blocks a frame.
7. **Lab.** A dune driven up (it slides under the wheels, the car slows);
   a track driven ten times (a rut that deepens then stops); a mud flat
   (holds a boot, gives under a wheel); a sand heap poured on a slope
   (slides till it rests at its angle). `expect`s: mass conserved to the
   sixteenth, every tile asleep after settling, steps under budget.
8. **Tests.** `TestPile`: the rule conserves, settles at the angle of
   repose, is deterministic (same start, same end, any tile order between
   frames), and a yielding material holds under its yield.

## Not doing

Grains or particle methods (MPM is a possible later close-up tier, not the
base); breaking and debris (next sprint); erosion by water (later, on the
same rule); powders poured as matter still heap as before until moved onto
this layer.

## Open questions

- Whether undisturbed ground should ever be written back to computed (a
  pile that matches its `Top` dropped) to keep saves small.
- How much the far view must know: a pile changes a node's surface by
  under a block, so likely nothing.

## Progress

**Changed from the plan: piles are cut shapes.** Moved ground is written
into the map as the cut shapes it already keeps (`Map::setShape`, sent
as `TOCLIENT_SHAPE`), not a new per-node pile (no map format 32). Meshing,
collision, digging, the water's floor and the physics ground all read it
unchanged. So the field is in sixteenths, not finer: a slope resting
between sixteenths would slide again each time it's read back.

**Built so far:**
- `pile::Field` (`src/pile/pile.*`): the rule. Red-black phases by world
  column parity (no two slides share a column, so no ringing, and fields
  with other edges agree); half the excess a pair; only stirred columns
  move (a generated cliff stands till touched); presses sink by
  pressure over bearing, less as a column packs, pushing a ridge.
- `pile::Piles` (`src/pile/piles.*`), the server's: a press wakes a region
  (9×9 nodes, 5 high) of the pressed material, read from the nodes'
  solid sixteenths, run at 20 Hz, written back as shapes four times a
  second while it moves, let go after a second still.
- Pressers: wheels (`physics::World::contacts`, `Motion::wheelsOn`: the
  suspension's load over the tyre's patch) and players' feet
  (`Server::pressGround`); off the ground is ignored; each presses once
  per sixteenth it travels.
- Materials as node groups (`pile_repose`, `pile_yield`, `pile_bearing`,
  `pile_give`) on sand, gravel, soils, clay, snow.
- `TestPile`: the rule's four, plus a wheel rolled across a sand floor in
  a test map (a rut cut, no sand lost, everything asleep after).

**Not yet:** client prediction and GPU drawing of moving ground (clients
see the server's shapes, remeshed, a few times a second); the Lab
scenarios; profiler budget checked on a real world.

### Second pass (after the first play)

Seen: tracks in sand worked, but black squares round them, and in a
churned patch a checker of black nodes. Performance fine.

- **The black:** sand pushed up into the air cell over the ground (a
  ridge) is a thin layer low in a cut node. Smooth lighting blends a
  voxel face's light by its height between the node's corners, so those
  tops took the light of the ground under the cell. Upward voxel faces
  are now lit at the top of their cell (`voxelQuad`), and the flat light
  of a cut face or a smoothed top looks past cut nodes above to the air
  (`drawShape`, `drawGround`), as solid nodes already did.
- **Client prediction:** the client runs the same `pile::Piles` on its own
  map for its own presses (its feet, or the wheels of what it drives:
  `Body::drivenWheels`), writing cut shapes there at once and remeshing
  them (`Client::pressGround`). The server's shapes replace them as they
  arrive.
- **Only what changed is written:** a region writes only the node columns
  whose ground moved since its last write (and those beside them), so a
  client's prediction never writes over the server's word where it
  didn't move, and the server sends less.
- Profiler: `Client: piles [us]`.

Not done: drawing moving ground on the GPU. The prediction's writes are
remeshed like any cut node (the mesh thread, off the frame); a height
texture drawing is only worth building if that remeshing shows in the
frame time.

### Third pass: the grey was water

The dark grey on fresh tracks wasn't lighting: it was the fine water
grid (sprint 82) inventing water in them. When the ground under a grid
column dropped, the grid kept its water surface where it was, so a dry
rut got water as deep as it was dug: only near the player (the fine
grid's reach), which is why older tracks further off looked right. Now
only a column already under water keeps its surface.

Beside the sea, three rules held water under the sea's level as the
sea's (a fresh tile filled to it, land in a sea tile wasn't drained
under it, coast pools didn't count it): a beach rut dug under sea level
filled from the client's sea and never drained, though the server has no
water there. All three now apply only where the server has water.

Also from this pass: lighting looks past smoothed ground (a loose node
with open air over it) as it does cut nodes, for corner light and a cut
face's flat light; voxel faces other than undersides are lit at their
cell's top.

## Outcome

Loose ground moves. `pile::Field` (`src/pile/pile.*`) is the rule:
columns a sixteenth wide, the excess over a material's rest sliding
half-way downhill in red-black phases by world parity, only where stirred,
in integers (the server and a predicting client agree). Presses sink by
pressure over bearing and push a ridge; packing caps a rut. `pile::Piles`
(`piles.*`) wakes a region round a press, reads it from the nodes' solid
sixteenths, runs it at 20 Hz and writes it back as cut shapes (the map's,
sent as `TOCLIENT_SHAPE`: meshing, collision, digging and the water read
it unchanged), only the node columns that moved; asleep after a second
still. Pressers: wheels (`physics::World::contacts`, the suspension's
load over the tyre) and feet (`Server::pressGround`). Materials are node
groups (`pile_repose`, `pile_yield`, `pile_bearing`, `pile_give`). The
client predicts its own feet and wheels (`Client::pressGround`), only
reshaping the ground's own nodes; when a region settles the server sends
the shapes it wrote, and the client skips those it has.

Along the way: voxel faces are lit at their cell's top and light looks
past cut and smoothed ground (no black or grey ridges); the fine water no
longer invents water in a dry rut, or holds a beach rut's water under
sea level as the sea's where the server has none.

**Run:** engine build; `TestPile` (the rule: heaps rest at their angle,
deterministic, mud holds, ruts stop; a wheel cuts a test map's sand, none
lost; a dug hole slumps); TestShape, TestGround, TestPhysics, TestMapBlock;
Lab check 3/3; headless boot. **Seen** in the game by the user over many
passes (ruts, footprints, dunes, dug holes).

**Not done:** moving ground drawn on the GPU (the prediction remeshes);
erosion by water; Lab scenarios for the dune, the rut and the mud flat;
the sixteenth jolt walking on sand.
