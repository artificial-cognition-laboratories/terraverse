# Sprint 58 — The body

The player is a physical body (sprint 59 adds gravity as a field, a
jetpack and orbit).

- **Characters** in `physics::World`: Jolt's `CharacterVirtual`, a capsule
  that walks by collide-and-slide, up 0.6-node steps (not whole blocks),
  standing on and carried by what moves, pushing bodies only as hard as
  it's strong (a person: 600 N: a 470 kg cart rolls; a 5 t block doesn't).
  The server's players are characters placed where their clients say,
  pushing on the way; tiers count every character as someone near.
- **Walking** (`physics/walk.*`, pure): inertia. Up to a walk (4 nodes/s)
  in ~¼ s; past it to a run (× the sprint override) slowly; braking
  harder than starting; turning grips (a curve at a run); barely any
  steering in the air; relative to what it stands on; a jump is a height
  (1.25 nodes × the jump override) under whatever gravity. Real gravity
  (Luanti doubled it for players): falls take longer to hurt (~10 nodes,
  was ~5).
- **The client's body** (`client/body.*`): this client's own physics world:
  its character moved by `walk` in fixed 60 Hz steps and drawn between
  them; nearby machines mirrored kinematic where they're drawn (stand on
  a cart and ride it); what it carries predicted here. `LocalPlayer`
  hands walking to it; Luanti's movement still flies, swims, climbs and
  noclips (the body picks up from wherever that leaves the player).
  Knockback (`add_velocity`) nudges it; landing reports a collision so
  fall damage still works. Map changes rebuild its ground
  (`Client::addUpdateMeshTaskWithEdge`).
- **Carrying is a spring** (`World::carry` / `pullTo`): pulled toward the
  hands as hard as the carrier is strong against its weight
  (`workshop::STRENGTH`: 1500 N; creative: unlimited), so light things
  follow, heavy ones sag and drag; carried things collide with the world
  (not with their carrier). Server and client both pull it; the client
  draws its own, in the same frame as the camera (the turning jitter was
  the old carry drawn a frame behind the view).
  `TOSERVER_OBJECT_HOLD`'s byte is a grip (free, still, carried);
  `TOCLIENT_MATERIALS` sends what blocks weigh. Protocol 71.
- **The figure** (`client/figure.*`, visual `"body"`, set by `tf_player`):
  head, torso, arms and legs as boxes in Minecraft-ish proportions, each
  turning at its joint, posed from movement: the stride follows the
  ground covered, wider at a run; a lean into a run; arms out and legs
  apart in the air; the head pitches with the look (the local player's;
  others' don't, yet).

Proved (TestPhysics): inertia, run-up, braking, turning, air, riding
(walk); the jump's height under two gravities; up a half step, not a
whole one; carried along on a moving platform; a person pushes a cart,
not a 5 t block; carried wood follows the hands, stone drags, both fall
when let go. Sabotage: unlimited strength and no slow run-up each fail.
Headless server boots; the live machine check still passes.

Not done: the ragdoll on hard falls; others' heads looking; client-side
prediction of machines (a pushed cart moves on the server; the player's
mirror of it follows a step later, so pushing feels a little sticky);
sneaking doesn't stop at edges; the player's own collision with others.

Not seen on screen.
