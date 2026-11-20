# Sprint 66 — Rolling

The first car was built (four wheels, rotary joints, a soil body) and
didn't drive: pushed, it jolted up and stayed put; moving, it chattered up
and down; a wheel sometimes wasn't drawn.

## Why

- **Weight.** Soil is ~1.5 t a block: the car was tens of tonnes. Its
  axles' rolling resistance (2% of its weight) was thousands of newtons; a
  person pushes 600.
- **Pushes** landed at the root's centre of mass with their upward part
  kept: a push too weak to roll it lifted it.
- **Wheels** were free cylinder bodies on hinges. Even on flat ground the
  test cart chattered (~40 m/s² spikes): Jolt's answer for wheels is its
  vehicle constraint, not free bodies.
- **Ground**: shaped nodes come as sixteenth slices, a staircase.
- **Drawing**: the body's position glided between server updates but the
  wheels' turn was put at once, so they shook against it; group nodes were
  culled by a box that didn't follow their turn.

## Built

- **Wheels are vehicle wheels** (`physics::Wheel`, `Assembly::wheels`).
  `workshop::compile` makes a round part turning freely on a level axle off
  the root a wheel (all on the first wheel's axis; one something hangs off
  stays a hinge). The physics puts a Jolt `VehicleConstraint` on the root:
  cylinder-cast tyres, a short stiff suspension (0.1 node of travel, 5 cm
  sag), tyre grip; axle friction is taken from each wheel's spin each step
  (Jolt's brakes lock a stopped wheel for good). The root weighs with its
  wheels and its centre of mass is theirs together (`wheeledMass`). Each
  wheel's body follows its wheel, kinematic, on a layer only characters
  touch: you can walk into a wheel and push the car by it.
- **Ground slopes**: a node of slices whose hull overfills it by at most
  0.06 of a node is its hull (`slopes`); boxes stay boxes (merged, no
  seams); a node cut hollow keeps its slices.
- **Pushes** land at the nearest point of the nearest (non-wheel) body to
  the pusher, so a corner pushed turns it; standing, a push never lifts.
  In creative a push is as hard as gets the machine going at 2 m/s².
- **Weight in the label**: the part label starts with the machine's mass
  (client-side, from the same materials).
- **Wheels on screen** glide between updates as the body does, and group
  nodes aren't culled.

## Checked

- TestPhysics 18/18: new `testRollsDownASlicedSlope` (a cart down a 14°
  hill of sliced nodes never thrown up more than 3 m/s²; it was ~40),
  `testCornerPushTurns`; the cart's wheels roll as far as they turn and stay
  on their axles within the suspension's travel. Hinge tests use a hinged
  cart.
- TestMachine 13/13 (a round part on a rotary is a wheel; a square plate a
  hinge). Full unittests: only the old TestAtlasRender failures.
- Headless boot clean. Not seen on screen.

## Next

- A seat and a motor: driving, steering, brakes (the vehicle's controller
  already has the engine and inputs).
- Wheels off the root (bogies), steering knuckles as hinges.
- The disappearing wheel: if culling wasn't it, find it.
