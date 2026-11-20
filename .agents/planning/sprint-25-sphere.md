# Sprint 25 — The sphere spike

The flat map can only make a torus, so a world built on it gets a polar
wall. The direction from here: a real block planet. It's an equiangular
cube-sphere, with six faces, every column pointing at the centre and
gravity down the column. The 8 corners where three faces meet become
landmarks. One face fits in the engine's coordinates (|x|, |z| < 31000) at
a radius of about 38 km. That's orbit at ~600 m/s and an orbit every
6–7 minutes. The flat map, the polar wall, the far view's flat-to-globe
blend and the space bubbles will be replaced, not extended.

## What the spike answers

Does one face, as an ordinary engine map drawn as the planet, feel like
Minecraft? Within a face the grid already is the planet's grid: columns
are radial, so the engine's −y gravity is radial gravity, and collision,
building and water are untouched. Only the drawing changes.

## Built

- `terragen::Sphere` (sphere.h): faces, place ↔ direction, settle (a place
  past an edge onto its neighbour), frames, latitude/longitude. It also has
  `Lens`: the planet seen from an eye, the grid itself next to the eye and
  curving away further off, in sums that keep float precision a planet
  across.
- `client/shaders/sphere/lens.glsl`: the same lens for every vertex shader.
  The nodes and object shaders place vertices with `sphereClip`.
  `SphereLensFactory` (src/client/sphere_lens.h) sets its uniforms.
- TEMP: on only with the client setting `terra_sphere_spike` (blocks from a
  face's centre to its edge: 29845 is the real planet, 500 a tiny one that
  shows the curve plainly). The default of 0 draws the world flat, as before.
- `bun check:sphere`: the unit tests (TestSphere), plus lens.glsl compiled,
  plus an off-screen render: straight down from 500 m, a tiny planet is a
  disc with sky round it and the flat world is ground to the corners.

## Found

- Equiangular blocks are 1 m on the centre lines and 0.71 m at the corners.
  The grid's lines meet at 120° at a corner, so blocks there are skewed.
  That's part of why corners should be landmarks.
- At 38 km a kilometre of view drops ~13 m: subtle on foot, plain from
  altitude. That's honest, and the far view will matter for seeing the planet.
- Fixed along the way: the engine never set the vertex shaders'
  `cameraPosition` (it set the pixel one twice).

## Not yet (the spike draws, it doesn't simulate)

- Lighting normals, fog, shadows, selection boxes, particles, clouds and
  the far view still use flat positions. The far view should be off
  (`far_view = false`) with the spike.
- Mapgen is still the flat map's. terragen's climate needs latitude and
  longitude from `Sphere`, not from `Map`.
- One face only: its edges end. Six faces means the engine's map keyed by
  face, with the handover at edges (map blocks, collision, raycasts,
  liquids, objects, the network, Lua `pos`).
- Orbits: core.orbit in the planet's frame from `Sphere::point`.

## Next

1. Flown 2026-10-24 at 500 (tiny): confirmed. Real blocks, trees and
   water curving to a horizon, no gaps. Clouds and rain are still flat, so
   they hang over it like a ceiling.
2. The far view goes onto the same geometry: its globe() becomes
   `Sphere::Lens`, so the near world and the LOD curve as one surface and
   the planet can be seen whole from orbit.
3. Then plan the six faces: map storage and the edge handover first,
   under tests that cross every edge (blocks, players, water, entities).
