# Sprint 57 — It rolls

Machines are rigid bodies on **Jolt Physics** (MIT, vendored at v5.4.0 in
`library/jolt`, built double precision and cross-platform deterministic,
SSE4.2 and no newer).

- **`src/physics`** (pure, no engine deps): `physics::World`, assemblies of
  bodies (boxes with densities) and hinges, a fixed 60 Hz step on one
  thread; ground asked a map block at a time and only round Live
  assemblies; players as kinematic capsules that push; held assemblies
  kinematic; tiers Live and Frozen (`planning/simulation-scales.md`).
  Ground boxes merge along x then z, and bodies use Jolt's enhanced
  internal edge removal, so nothing catches on the seams between blocks.
- **`workshop/motion`**: each machine entity compiled
  (`workshop::compile`): `Machine::rigid` groups parts by welds, each group
  a body of its built blocks' boxes weighed by `workshop::Materials`; each
  hinge a hinge whose friction is rolling resistance (0.02 of the weight
  at the wheel's radius). A hinged group round about its axle (it reaches
  ≥ 85% as far every way) rolls as a smooth cylinder. Entities are driven
  (`LuaEntitySAO::setDriven`): position, turn and velocity from physics;
  groups' poses to clients as `AO_CMD_SET_PARTS` (protocol 70), drawn as
  one scene node per group. Edited machines are rebuilt in place.
- **A grown hinged part's axle runs through what's drawn's middle**, not
  where the drag began (wheels drawn by dragging spun off-centre before).
- **Materials**: `core.physics.material(node, { density, friction })`;
  `tf_matter` declares every solid block from `core.matter.density` (new),
  slippery ones at 0.1. 14 lab and debug blocks, the bellows and cactus
  have no matter, so a machine of them can't move (logged at load, and
  each such machine is logged when it's met).
- `core.physics`: `material`, `velocity`, `mass`, `impulse`.

Proved: TestPhysics: rests and sleeps; freezes away from players and wakes
when one comes; a cart rolls down a 10° slope turning its wheels as far
as it goes (rolling, not sliding); rolls further than a sledge slides;
hinges keep their axles; stone-bottomed rights itself, top-heavy tips;
held goes where it's told and falls when let go; a player pushes it;
pushed when frozen, over sliced (shaped) ground; the same run twice is
bit for bit the same. TestMachine: rigid groups; the hinge through the
middle; compiled masses, boxes, the wheel's cylinder and hinge; entity
turns to quaternions and back. Sabotage: no hinges, square wheels,
unmerged ground and the turn's sign each fail them. Live headless server:
a two-block machine dropped on a flat world falls at g, rests on the
ground and slides when pushed.

Not done, on purpose: the spring carry (the carrier draws what they hold
where they put it, so a lagging spring only others would see; it wants
client-side physics, the movement sprint); `on_impact`; Settled and
Scheduled tiers; the player as a Jolt body. Collision for players still
uses the machine's unturned box. Clients point at a wheel as it was built,
not as it's turned (the same shape for a round one).

Not seen on screen.
