# Sprint 51 — Control

## Decision

Turning a held object is explicit: one axis at a time, in steps, about
its middle.

- Holding, only the held object is aimed at: a press that misses it
  turns it, and nothing else is cut.
- **The handle**: a disc on the ground under it (a grid across it) and a
  ring round it facing the camera. Press on the disc and go round: it
  yaws as the cursor goes. Press on the ring's rim and go round: it rolls.
  Anywhere else off it: the first move locks the axis, across yaws, up or
  down pitches (about the camera's right). The grabbed one lights.
- **Steps**: 15°, Shift 90°, for drags and keys alike.
- **Keys**: Q/E yaw, W/S pitch, A/D roll, a step a press.
- **Tab**: the face pointed at turned square to you (the nearest of the
  24 square turns that does it).

Proved: TestSculpt `testTurnedByHandle` (steps; the disc turns with the
cursor, not against it: fails with the turn reversed; rim rolls, middle
is nothing; a face turned to you looks back, square).

Not proved: on screen. The handle's size and colours; whether the rim
band (0.35 nodes) is easy to grab.
