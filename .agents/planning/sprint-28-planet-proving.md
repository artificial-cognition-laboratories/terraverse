# Sprint 28 — Prove the planet

## Current evidence (2026-10-26)

The first independent oracle is in `TestPlanet::testOrbitalSilhouetteIsRound`.
Before the lens change, the sampled horizon's projected radius at orbit height
varied by a factor of 1.71 near a face corner. The lens now uses the local
grid frame within 16 m of the eye and eases into a perpendicular physical
frame by 128 m; the CPU and vertex shader use the same transform. The new
test samples 72 horizon azimuths at three positions on every face and three
altitudes, requiring a radius ratio below 1.02 and agreement with the
analytic angular radius within 2%. `TestPlanet` (21 cases), `TestSphere`
(8 cases), and the live headless `check:planet` (3440 assertions) pass. The
live probe now checks each walker step for a valid face and a bounded change
in planet-centred position, in addition to its endpoint checks. This proves
the specific face-dependent orbital skew at sampled points and rules out
large entity jumps in the 72 walked crossings.

The wider sprint gate below is still open. The current live probe measures
entity motion, so it cannot establish player prediction, correction reasons,
or render coverage. No client render capture has
been run by the agent. Those measurements must be implemented and run before
claiming the reported gaps and teleports are resolved.

## Why this sprint

The planet works in ordinary play, but particular positions still reveal
missing terrain, unexpected position jumps, and an oval or egg-shaped globe
in orbit. More local fixes without reproducible failures will move the bugs
around. This sprint builds a deterministic way to *find* the failures, keep
their evidence, and make every fix pass the same cases.

No finite test suite can guarantee the absence of every visual or engine bug.
The achievable guarantee is that every face, edge, corner, altitude band and
movement transition in the declared matrix is exercised and fails with a
specific invariant or render artifact. Seeded exploration looks between the
matrix points; every failure becomes a permanent case.

## What the existing checks miss

- `check:planet` uses a flat, small world. Its walkers are Lua entities at
  3 m/s, starting near three positions on each directed side. It checks final
  face, height and speed, not the path, player prediction, camera, server
  correction, high speed, altitude or diagonal corner approaches.
- `check:flight` follows one equatorial orbit and checks face order and
  altitude range. It does not cross polar faces or corners, compare each
  physics step's physical point, or check what the client draws.
- `check:sphere` has useful geometric unit tests, but its client scenes are
  one disc and one corner, with far view and clouds off. Its image assertions
  inspect a handful of pixels, so a large hole elsewhere can pass.
- `TestPlanet::testLensIsThePlanet` compares the lens with its own skewed
  local frame. That establishes consistency, but cannot establish that a
  planet projects as a circle to a physical camera.

## Route a failing case to an owner

| First failed measurement | Inspect first |
|---|---|
| Physical point jumps at a face change | `planet/planet.cpp::carry`, `client/localplayer.cpp::carry`, `server/luaentity_sao.cpp::carry`, `planet/flying.cpp` |
| Physical point is continuous, then the client snaps | `server/player_sao.cpp::checkMovementCheat`, client reconciliation and camera turn in `client/game.cpp` |
| Outline is oval with complete terrain IDs | `terragen/sphere.cpp::Lens`, `planet/planet.cpp::Lens`, sphere shader and `client/camera.cpp` projection/frame |
| Disc has missing IDs | `far/view.cpp::choose` and `render`, `far/mesh.cpp` face cuts, near draw list and mask |
| White triangles have cloud ID | `client/clouds.cpp` patch projection and cloud shader |
| Squares have water IDs | `far/view.cpp::updateMask`, far shader layer rules and `client/clientmap.cpp` transparent ordering |

This table is a debugging route, not a diagnosis based on appearance. The
trace and ID/depth output choose the row before code is changed.

## The probe contract

Every scenario has a stable ID and a single manifest entry: world kind/code,
`planet_half`, face/place or physical position, altitude, look direction,
velocity, frame step, view range, cloud/far/water settings, and duration.
The harness writes under `.output/planet-probes/<run>/<case>/`:

- `trace.jsonl`: timestamp/tick, client and server map positions, face,
  planet-centred position and velocity, look basis, predicted/corrected
  state, correction reason, nearby collision result, and render camera.
- `result.json`: each invariant with measured value, threshold and first
  failing tick; the engine version and complete manifest.
- For user-run render cases: colour PNG, linear depth and an ID image that
  distinguishes near blocks, far ground, far water, near water, clouds and
  sky. A contact sheet and a one-line replay command make the failure easy
  to inspect. The capture is of the game render target, never the desktop.

Record a short ring buffer before a failure and stop at its first frame.
Replay the exact manifest without relying on the user's current world,
settings, frame rate or random seed. Allow `--case <id>` to run one failure.

## 1. Prove topology and movement headlessly

Add pure geometry/property tests and a live headless player/server probe.
Exercise all 24 directed face sides and all 8 corners, including approaches
0.25, 1 and 16 blocks from a join; face centres are controls. Use the
smallest supported, default and largest planet sizes. Run walking, fast
movement and orbital speed with normal, delayed and long frame steps. Use
cardinal and diagonal motion, both crossing directions, and travel around
equator, polar caps and corners. Include a second player on another face.

For each integration step assert:

1. One valid face owns the position; no playable actor enters the storage
   seam, produces NaN/Inf, or changes face without a recorded transition.
2. Before/after physical positions differ by at most the integrated travel
   plus a numerical tolerance; speed and world-facing change continuously.
   A face-local 90-degree grid turn is not a physical turn.
3. A step's inverse returns to the same physical state to tolerance.
   Vary sub-node fractions, speed and `dt`, not only integer node centres.
4. Client prediction and server authority agree on face and physical point.
   Every server reset is tagged as anticheat, collision, scripted teleport or
   reconciliation; an unrequested reset fails the case.
5. Collision, pointing and loaded/active blocks agree across the join.

Instrument `LocalPlayer::carry`, entity carry, `PlayerSAO` movement checks and
`planet::Flying` at their existing boundaries. Keep the trace behind a probe
flag so normal play does not log every frame. Do not disable anticheat to
make a case pass.

## 2. Prove rendering against an independent oracle

Build a user-run deterministic render harness. The agent can implement and
inspect its source, but per `AGENTS.md` does not launch the client or capture
the user's desktop. Use a simple uniform-colour spherical fixture first;
then the seeded terrain case that reproduces the user's gaps.

The fixture provides an **independent** analytical ray/sphere silhouette,
with screen rays from the camera's perpendicular frame. At face centres,
edges and corners, rotate through headings and pitch at sea level, cloud
level, low orbit and high orbit. Assert:

- A uniform planet's projected outline is circular: horizontal/vertical and
  azimuthal radii agree within a specified pixel tolerance. The expected
  angular radius is `asin(R / (R + h))`; perspective and aspect ratio are
  included in the reference. A face-dependent oval fails.
- Every pixel safely inside the expected disc has a terrain/water ID and
  finite depth. No contiguous missing region exceeds the small allowed
  raster seam tolerance. Test a one-pixel strip along each face join too.
- Depth order is correct where cloud, water and ground overlap. Far/near
  ownership is unambiguous at their boundary; no mapblock-sized water holes.
- Coverage and outline remain stable as the eye moves in small increments,
  crosses a face, and changes altitude. Save the first divergent frame.

Use layer-ID and depth output, not only final colour: fog, weather and sky
can conceal missing geometry. Add a small set of deterministic cloud/water
cases after the bare sphere passes. The visual harness must report a numeric
failure, not require the user to spot one by eye.

## 3. Search between the fixtures

Run seeded property-based trajectories over face location, height, heading,
speed and `dt`. Bias generation toward the 12 edges, 8 corners, altitude
where near/far meshes exchange ownership, and orbit height. Log the seed and
minimise a failure to the shortest path and fewest toggles that retain it.
Promote each minimised case into the fixed suite. Do not accept a correction
that improves a screenshot while breaking an earlier case.

## Work order and gates

1. Manifest, trace and one-case replay; prove a deliberately injected jump
   is reported before testing real faults.
2. Headless topology/motion matrix; first failing case for each teleport
   mechanism, then fix the cause and keep the regression.
3. Analytic circle and coverage oracle; prove an injected missing tile and
   asymmetric scale are detected before touching the renderer.
4. Run the user-provided gap and oval scenes. Use ID/depth images to locate
   cloud, far tile, near mesh, water-mask or camera-frame ownership; fix each
   at its boundary.
5. Seeded exploration and replay audit. The sprint ends when all fixed cases
   and exploration seeds pass, no untagged correction occurs, and the user
   reviews the contact sheet at the reported trouble spots.

Keep pure tests and headless probes agent-runnable. The client render command
is handed to the user, who returns its `.output/planet-probes` result or
screenshots for review. Existing `check:planet`, `check:flight`,
`TestSphere`, `TestPlanet` and `TestFar` remain gates throughout.
