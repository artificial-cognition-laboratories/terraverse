# Planet rendering: next stability pass

The screenshots from 2026-10-25 establish that shared terrain/cloud depth
occludes clouds behind land. They also show three distinct problems at
altitude. Keep them separate while debugging; changing the sphere lens to
hide one may conceal another.

## Observations and likely boundaries

| Screenshot | Symptom | First boundary to inspect |
|---|---|---|
| 1 | Isolated tree and thin dark trails over ocean | Near mesh versus far tree/terrain mesh and shadow pass; identify which layer draws each mark |
| 2, 3, 5 | Large white fans and a dark wedge near a face crossover | Clouds are still generated as one flat 64-block-cell sheet around the camera (`client/clouds.cpp`); the sphere shader assigns a cube face per vertex. A quad spanning a net gap can have corners projected onto different faces, making a triangle across the planet. Also inspect far tile coverage there. |
| 4 | Dark squares precisely aligned to mapblocks in water | Far view mask has one texel per near mapblock column (`far/view.cpp::updateMask`), while near water is transparent and sorted separately. Inspect which water layer is selected and its depth, not just its colour. |

These are hypotheses until each layer is isolated in the client. The white
triangles' shape strongly points to cloud mesh topology, but the far view can
also make long triangles at face boundaries and must be checked independently.

## Capture the reproducible scene

For each artifact, record the world code, player node position, altitude,
heading, view range, cloud settings, and a screenshot. Hold the camera still
and capture the same scene with clouds disabled, then with the far view
disabled. For water, also compare a view just above and just below the near
mesh boundary. This separates cloud geometry, far terrain and near water
without changing several systems at once.

## Work order

1. **Cloud shell topology.** Make each cloud patch belong to one planet face;
   clip patches to face boundaries before triangulation. A cloud cell must
   never contain vertices on opposite sides of a storage-net gap. Give the
   shader an explicit face rather than guessing it from a vertex's map
   coordinates. Subdivide long patches enough that curvature between
   vertices does not make a visible chord. Test all 12 edges, 8 corners,
   ground level, cloud level and orbit.
2. **Far terrain coverage.** Add temporary face/tile/mask debug colours. Check
   that `far::mesh` cuts every tile at face edges, that `View::choose` covers
   the visible cap, and that fragment-level hidden-side culling does not
   remove a visible tile. Close any dark wedge before tuning atmospheric
   colour or haze.
3. **Water ownership and depth.** Visualise the mask's red/green values and
   near/far water layers. Make one layer own each surface pixel at the seam.
   Check transparent buffer order by physical, lensed distance across faces;
   current `ClientMap::updateTransparentMeshBuffers` still uses flat map
   distance. Preserve the far seabed where near water draws above it.
4. **Depth precision.** Shared depth is necessary for cloud/land occlusion,
   but the camera now reaches the whole visible planet. Measure depth error
   at the surface, cloud layer and orbit for every planet size. If distant
   surfaces flicker, use a deliberate depth strategy (for example a separate
   far depth representation) rather than another depth clear.

### Cloud geometry pass (2026-10-25)

`Clouds::updateMesh` now maps cloud quads from the player's face onto the
planet and recursively splits a quad when its interior crosses a cube face.
Every emitted quad has one destination face; an unresolved metre-scale corner
piece is omitted instead of stretching across the storage net. Cloud noise
is still anchored to the source face's flat grid, so its pattern may change
when the player crosses an edge. A later weather/cloud field should be
sampled in planet coordinates.

The water squares remain a separate near/far layer issue. Isolate the mask
and transparent pass in the user's scene before changing their colours.

## Gates

- Pure geometry tests: no cloud triangle spans multiple faces or a net gap;
  far tiles cover every visible face edge and corner.
- Shader compilation for cloud and far programs, including their varyings.
- Engine build and existing `TestSphere`, `TestPlanet`, `TestFar` modules.
- User-run visual matrix: at face centre, edge midpoint and corner; sea
  level, cloud level and orbit; clouds/far view individually on and off.
  The repo's agent brief prohibits launching the client here.
