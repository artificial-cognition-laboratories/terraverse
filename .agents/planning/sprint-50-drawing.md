# Sprint 50 — Holding and drawing

## Decision

- **Aim is what's drawn.** `piece::frame` is the one frame for an object:
  the entity draws with `setPitchYawRoll(-rotation)`, and the tools aimed
  with `setRotationDegrees(rotation)`: turned, strokes landed mirrored.
- **Hold, don't orbit.** F picks the object up: it hangs at arm's length in
  front of the eye, the camera over the right shoulder. Dragging off the
  object (place button) or W/A/S/D turns the object in 3D; Q/E 15° (Shift
  90°); the wheel nearer/further. The client poses it (TOSERVER_OBJECT_POSE,
  20 a second at most), the server checks reach. Let go, it falls as turned.
- **Shapes are drawn by footprint.** Press on a face, drag, release: the
  footprint corner to corner on that face's plane, the size (wheel) how far
  it goes. Click: the size square. Box, Cylinder (wheels, axles: along the
  face's normal), Cone, Sphere; Cut and Bore as before. Dig drags take.
  The status line reads the size as it's dragged.
- **Hollow**: the held object's design made a shell the size thick
  (distance to open space, 26 round), undone as a stroke.
- **Pause** is buttons only: no box, no dimming. The HUD stays up and
  clickable: map, rails, the profile and world controls (always top right
  now). The weather strip is gone.

Proved: TestSculpt `testHoldPointsWhereItLooks` (the frame's yaw, the
hold's ray, pose puts the middle where it's held turned any way, mirror
axis in the object's grid; fails with the old frame),
`testDrawnByFootprint` (each shape's cells, placed and dug, off-plane and
too wide refused, measure; fails ignoring the drag); TestPiece
`testHollowed` (exact wall, undo; fails a sixteenth thick). Server boots.

Not proved: any of it on screen. The drag's feel (turn direction, speed).
Held objects still collide as their unturned box. Clicks on the rails while
paused wait for the game to run again (singleplayer pauses it).
