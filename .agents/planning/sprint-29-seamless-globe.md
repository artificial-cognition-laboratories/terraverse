# Sprint 29 — Seamless block globe

## Decision

Keep six square storage faces and the eight three-face junctions. Make the
physical surface continuous, with the necessary cell distortion spread
through transition bands. The broad face interiors should remain as close
to ordinary square blocks as the sphere permits. Edge bands run along all
12 face edges; wider corner bands join three edge bands at each of the eight
junctions. No player-facing peak, gap, snap, or special movement is allowed
at a junction. The corner peaks are temporary covers, removed only after a
bare flat fixture passes.

An exactly square, equal-size cell grid everywhere on a sphere is not a
possible gate. Instead, measure edge length, angle, area and height of the
physical cell against an ordinary block and optimize the maximum visible
departure. Keep the size and angle error below a chosen measured budget in
the interior, then distribute the remaining error over the bands. Set that
budget after a baseline scan of all supported planet sizes; do not invent a
percentage before seeing the current grid and candidate mappings.

This changes the *geometry* of a world. A saved world's block address stays
the same, but the physical location of that block may change. Version the
mapping in world metadata, keep old saves on their original mapping until
there is an explicit migration, and do not silently move existing builds.

## Geometry contract

1. One deterministic `physicalVertex(face, gridX, gridZ, radialHeight)` owns
   the geometry. Both incident faces must obtain bit-identical coordinates
   for a shared edge vertex; all three must agree at a corner. Terrain,
   placed blocks, collision, pointing, far view and the camera use this
   geometry. Face IDs remain storage addresses only.
2. Establish a measured baseline for today's equiangular mapping. Compare
   an equi-edge/equal-area candidate and a smoothed candidate. Evaluate
   distributions of cell side length, angle, area and edge-direction change
   at face centres, edge bands and corner bands. Select band widths in
   blocks and as a fraction of the face half-width, including the smallest
   supported planet. Constrain the field to be symmetric, continuous,
   monotonic and free of folded cells.
3. Fix interior vertex positions outside the bands during optimization.
   Penalize departures from square metre-scale cells, rapid changes between
   adjacent cells, and concentration of distortion at one vertex. Solve the
   three incident faces together at a corner, not as independent patches.
   A smooth image is insufficient: compute a minimum Jacobian and reject a
   mapping that reverses or collapses a cell.
4. Give every block and interface one owner. Adjacent faces share physical
   vertices and have no duplicate internal wall. A corner is a three-way
   cell neighbourhood, not a missing fourth cell. The topology API must
   expose that rule to meshing, collision, raycast, light and liquids.

## Work order

### 1. Measure and choose the mapping

- Add a deterministic C++ grid audit over six faces and all supported sizes.
  Write a compact report under `.output/planet-grid/`: heat maps of side
  length, angle, area, and edge tangent discontinuity; min/max and location.
- Implement candidate mappings behind a mapping version. Compare them on
  one flat-colour globe and one bare corner; select the one with the best
  local block quality and no fold. The first prototype may be CPU-only.
- Pin a golden mapping sample at face centre, edge midpoint, and every
  corner. Check shared vertices exactly and neighbouring cells within the
  measured distortion budget.

Baseline recorded on 2026-10-26 by `bun check:planet-grid`: across supported
planet sizes, sides span about 0.707–1.000 m and cell area 0.707–1.000 m².
Angles range from 60° to 120° at the corner singularities. More revealingly,
the provisional central 75% of each face already reaches about 72°–108°:
the current equiangular mapping does not preserve a broad square interior.
The generated heat maps show four continuous lobes of angular and size error
from the face centre to its corners. This is the baseline the candidate must
improve without reducing the positive minimum Jacobian (~0.707).

The radius fixes the total distortion budget: `R = 4h/π` gives an average
surface area of `8/(3π) ≈ 0.849 m²` per block column. If a square interior is
held near 1 m² and the transition is not allowed below roughly the current
0.707 m², that interior can occupy at most about 49.5% of a face's area, or
70% of its width. Start candidate comparisons with a central 50–65% width
held close to square and the outer 17.5–25% per side as the transition band.
The optimizer and visual fixture decide the final band; these numbers are a
conservation bound, not an art-direction preference.

### Candidate result

The audit now compares equiangular, spherified, three smooth blends, two
locally locked experiments, and the six-parameter COBE quadrilateralized
spherical cube. The locked experiments are rejected: blending a square-biased
centre back to the old edge concentrates derivative error in the band and
drops the minimum Jacobian as low as 0.575–0.649. A simple square-centre warp
is also rejected (minimum Jacobian ~0.426).

COBE is the leading candidate. At the largest supported size it gives area
0.840–0.859 m² and a minimum Jacobian ~0.840 across the whole face, compared
with 0.707–1.000 m² and ~0.707 today. Its provisional central 75% has angles
about 77°–103° instead of 72°–108°. The 60°/120° singularity remains at the
actual three-face points, as topology requires. Some cells reach 1.187 m on
one side, so the next gate must assess anisotropy and visible block shape in
the bare fixture before selecting it for engine integration.

The candidate is the symmetric six-parameter polynomial described in
[Cube-to-sphere Projections for Procedural Texturing and Beyond](https://jcgt.org/published/0007/02/01/paper.pdf),
which reports much lower area error for COBE than its univariate mappings and
provides optimized forward and inverse coefficients. Its bivariate symmetry
and fixed cube edges fit our shared-vertex contract. The paper's exact
equal-area Arvo mapping is excluded because it produces edge discontinuities.

### CPU integration result

`Sphere::Mapping` now makes the mapping an explicit part of the geometry
object. `Cobe` has the paper's optimized forward polynomial plus a
deterministic inverse refined by four Newton iterations. Unit tests pin
forward/inverse round trips on every face, continuity across every edge, and
the measured cell-size envelope at every supported size. The existing
equiangular mapping remains the constructor default and all existing planet
tests continue to exercise it.

Worlds made before the field existed mean `equiangular-v1`. The server refuses
an unknown value rather than silently moving a save.

The shared lens now supports COBE on the CPU and in the common vertex-shader
path. Its eye coordinate, per-block step, differential grid frame, near-face
precision sums, far-face placement and physical camera frame all select the
same mapping. `TestSphere` compares the COBE lens against exact spherical
points and its float form against doubles; `TestPlanet` exercises the COBE
lens across all six faces. The shader receives the mapping as an explicit
uniform and applies the same polynomial to near terrain, other faces, far
terrain, water and clouds through their existing common lens.

### Gameplay integration result

Gameplay topology now admits the real three-face corner. `halo` resolves a
diagonal corner query through the incident faces instead of returning a hole,
and map lookup, visibility, collision, raycast, light and mesh-halo tests cover
all 24 face corners. Movement carry consumes an entire displacement, including
multiple face crossings in one tick, with a deterministic corner tie. The
same suites run the mapping and camera ground-frame checks against COBE.

COBE is now a loadable, saved world version. New worlds explicitly record
`planet_mapping = cobe-v1`; legacy worlds with a missing field and worlds that
declare `equiangular-v1` keep the old geometry. Mapgen and the server verify
they loaded the same version, protocol 61 sends it to the client, and unknown
disk or wire values fail closed. The headless planet fixture now uses COBE so
its walking, falling-node and liquid crossings exercise the selected mapping
end to end.

This proves address topology and frame consistency, but not yet the visual or
high-speed experiential gate. The client must still be inspected at every
bare junction for camera snap, collision correction, near/far disagreement
and visible cell shape before removing the generated corner covers.

### 2. Make the corner a real part of the world

- Replace `halo`'s ambiguous two-edge answer in gameplay queries with an
  explicit three-face neighbourhood. Keep `step` for cardinal block moves;
  add a physical-point query for diagonal motion, wide collision sweeps and
  raycasts. Never treat the unfolded net gap as a fourth cell.
- Mesh a bare, level corner from the shared vertex field. Verify no T-junction,
  backface, double surface or missing pixel, including the near/far handoff.
- Make block selection, placement, mining, orientation, fluid, light and
  save/reload agree on the same owner on all three faces.

### 3. Make motion and camera independent of the face address

- Trace physical position/velocity, face address, client prediction,
  attachment transform, camera basis, and server corrections around every
  face change. A change in map coordinates is a rebasing event, not a
  physical displacement to interpolate.
- Preserve the camera's physical forward/up frame through an edge or corner.
  A quarter-turn of grid yaw alone is insufficient where axes skew. Apply a
  continuous heading transform to the player, craft and attached camera.
- Substep collision based on physical distance travelled so jetpack and
  orbital-speed steps cannot pass through a corner or unloaded terrain.
  Stream mapblocks by physical proximity, including all three corner faces.

### 4. Remove the cover and prove the fixture

- Disable the eight generated stone peaks in the test fixture. Only remove
  them from normal new worlds after the bare junction passes. Preserve old
  saves according to their mapping version.
- Agent-run: `TestSphere`, `TestPlanet`, `TestPlanetMap`, `TestPlanetMesh`,
  `bun check:planet`, `bun check:flight`, plus fixed and seeded corner cases.
  Inject a missing corner cell and a position correction to prove the probes
  fail for the intended reasons.
- User-run visual gate: flat surface and representative terrain at face
  centre, edge, and all eight corners; ground, low altitude and orbit;
  walking, building and fast flight. Capture colour, depth and layer ID from
  the game render target. The agent does not launch the client under
  `AGENTS.md`.

## Done when

All eight bare junctions are traversable and buildable. No gap or overlap is
visible from the ground or orbit; the near and far meshes agree. Physical
position, velocity and camera direction are continuous at walking and flight
speed, and server/client corrections are accounted for. The face interiors
meet the measured block-quality budget, the transition bands remain bounded,
and old worlds retain their declared mapping. Only then remove the corner
peaks from new worlds.
