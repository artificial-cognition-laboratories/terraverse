# Sprint 73: Vehicles

A car built rather than guessed: the seat says which way is ahead, and the
wheels' joints say which steer and which the engines turn.

## How you build one (creative)

1. Draw a chassis (Tab, Add).
2. Wheels on rotary sockets as before. Right-click a wheel's joint (hover
   it with the tools away, or the face on the bench) and pick from the
   second row: **steers**, **driven**, or **steers, driven**.
3. Hold a **Seat** (tools away, as a block's held) and look at the
   object: the block it'd take is outlined. Right-click fits it there,
   whole, facing the way you look. Its dark side is the backrest. Same
   for an **Engine** (four are a small car's torque). In build mode, Add
   with one held fits it too.
4. Put the tools away, right-click the machine, **sit**. W/S, A/D,
   Space brakes, Shift gets up.

## Built

- **Joint kinds** `steers`, `driven`, `steers_driven` (`piece::Joint`,
  new numbers in TFM4; `JointKind` gives each a label, whether it steers
  and whether it's driven). Only wheels on steering joints steer, behind
  the driver as well (a forklift's); the engines' torque is shared among
  the driven wheels only, by axle. An engine joined to no driven wheel
  goes nowhere.
- **Fittings:** a block whose material has a role (seat, motor) is
  fitted, not drawn: Add puts one whole block on the face, its facedir
  the way its fitter looks in the part's grid (`Workshop::fit`; undo
  takes it off). Paint with one is refused, as is fitting one to the
  world or to a part on its side. Mirroring doesn't copy fittings.
- **Ahead is the seat's front** (`Assembly::facing`, from its facedir
  through its part's turn). `Drive::ahead` and the driver's facing byte
  in `TOSERVER_DRIVE` are gone. Protocol 79.
- **Fitted at any height:** a fitting is a part of its own, one block,
  set flat on the face at whatever sixteenth it's at (on a plate a
  sixteenth thick, not a block up), on the block grid across it, welded
  (`workshop::fittingCorner`, shared by the server and the client's
  outline). Taken off with the part's delete, not undo.
- **The floor grid in build mode:** the floor's block edges in a bubble
  five blocks round the player, fading out (`Hud::getGrid`, blended),
  always there while building.
- **Mirrored wheels on the body:** the mirror's plane is the machine's,
  and a stroke applied it in the part's own grid; a part that had moved in
  the machine (as one does when room's made on it) mirrored off the body.
  It's turned into the part's grid first (`partMirror`).
- **Steering:** A steered right: Jolt steers right-handed in a
  left-handed world. Tested (`testDriven`: right, facing +x, is -z).
- **Sitting:** the driver's hips on the seat (feet half a block below
  its top), the view turned to the seat's front as they sit and turned
  with the machine as it turns; the mouse still looks about.

## Changed for what was built before

- Cars from sprint 69 don't steer or drive until their wheels' joints are
  set: the guesses (the front axle steered, every wheel was driven) are
  gone. Seats placed before face +z of their part (facedir 0).

## Tests

`TestMachine` 14: a seat's facedir is ahead through its part's turn,
each wheel joint's kind compiles to steering and driving. `TestPhysics`
29 (`testDriven`): driven only through driven wheels, steering only by
steering wheels, the seat facing back drives back.

Not run: the client. Hand-over: build the car above and drive it.

## Not yet

- The chest, and real blocks on a machine (the deck): sprint 74.
- Struts (suspension as a joint you build).
- Other players see a driver standing in the seat: no sitting pose
  (the server doesn't tell clients who's seated).
- In build mode, Add's preview while holding a fitting is the brush's,
  not the block's (with the tools away it's outlined).
