# Sprint 69: Drive

Grabbing's bugs, then a car you drive: a seat, a motor, steering and
brakes, with the driver's client predicting the car.

## Fixed

- **Falling through the floor.** A machine in someone's hands (held,
  carried, grabbed, now driven) skipped the check that keeps the ground
  round it built, so two seconds on, the ground under it was dropped and
  it fell through. The ground is kept for everything Live
  (`testGrabbedKeepsItsGround`, which fails without it).
- **The hand in the ground.** The grab's hand stops at the first solid
  block along the look, and a grip presses down no harder than the
  point's weight.
- **Pointing at machines.** A machine is pointed at by its parts' voxels,
  not its box: through the box's empty corners the ray reaches what's
  behind, and a part nearer than the block hit is what's pointed at.
- **Lifting what you stand on** lets go ("You can't lift what you're
  standing on"). The pull isn't felt back yet.
- **The grip drawn:** a mark where it's held and a line to the hand,
  green to red as it stretches.

## Built

- **Seat and motor blocks** (`tf_world:seat`, `tf_world:motor`): blocks
  with a role (`_machine = { role, torque }`, given to
  `core.physics.material`; `workshop::Role`). The root's first seat is
  where its driver sits; every motor adds its torque (40 N·m a block).
  `TOCLIENT_MATERIALS` carries role and torque (protocol 77).
- **Driving** (`physics::Drive`, `World::drive`): Jolt's wheeled vehicle
  controller with the motors' torque through a differential per axle, the
  front axle (the wheels furthest ahead of the driver) steering, and
  brakes. Which way is ahead is the way along the wheels' line the
  driver faced when they sat.
- **Sitting** (right-click a machine with a seat, tools away): W/S go
  and back, A/D steer, Space brakes, Shift gets up. `TOSERVER_DRIVE`
  ~20 a second. The client simulates the car (`Body::sit`/`ride`) and
  puts the player in the seat; it's brought toward the server's word as
  what's grabbed is. A driver isn't a body in the server's physics and
  skips the movement anticheat (`PlayerSAO::setDriving`).

## Tests

`TestPhysics` 26: driven (no motor goes nowhere, a motor drives it,
steering turns it, the brake stops it, facing back drives back), grabbed
keeps its ground.

## Not yet

- The seat is a block you stand on: no sitting pose, the camera's a
  standing one.
- Steering is the front axle's: a steering joint of its own is later.
- Nothing's checked of seats and motors compiled from real blocks
  (`TestMachine` has no seat yet); seen in the game only.
- The pull isn't felt back by the grabber.
