# Sprint 65 — Parts

Sprint 64 played: fitting worked, but a wheel was never centred on its
axle (circles were drawn from a corner, and joins met at a node's middle,
not the shape's), the rail mixed modes with ways of making parts, and the
parts that now lit up couldn't be clicked.

## Done

- **Round shapes are drawn from their middle** (`sculpt::Stroke::cells`):
  press where the middle goes, drag the radius. Cylinders, cones and
  spheres come out round. Boxes are still corner to corner. Protocol 75.
- **Draw on a socket:** creating, look at a socket of the workpiece and
  draw. The shape starts at the socket's middle, out of its face, and is
  a part of its own joined as the socket says; the socket's used up (and
  its mirror's, drawn mirrored). A wheel on its axle in one stroke.
- **Carried parts meet at their shape's middle** (`Machine::faceToward`):
  the middle of what's filled, to the sixteenth, not the middle of a node.
  TestMachine: a wheel reaching unevenly into the nodes round it is met
  at its middle.
- **The rail is four modes:** Add, Cut, Paint, Select, and the shape.
  Shift with Add or Cut pulls a flat face out or in. New part, Wheel and
  the welded/rotary/hinged choice are gone: parts come from the ground or
  sockets, joints from sockets.
- **Parts are worked on in the world.** With the tools away (or Select):
  - left-click a part of the workpiece: selected (again: unselected);
  - Shift-click another: the joint between the two (Fixed, Rotary, Hinge,
    None);
  - right-click: its menu. Top row, a socket here (Fixed, Rotary,
    Hinge); bottom row, Edit (work on it alone), Detach, Delete, and No
    sockets if it has any.
  The part selected (or looked at) says over it what it is and how it's
  joined. The page's part card is gone (`tools.part` out of the
  contract).
- Selecting no longer hides the other parts from the aim, so a second
  part can be Shift-clicked.

Proved: TestMachine 13/13, TestSculpt 11/11 (round strokes from their
middle), TestPhysics 16/16. Not seen on screen.

## Next

- Undo for joins and plugs.
- A hinge's stops set per joint; a spin arrow on rotary joints.
- Copies in survival.
- Handles and motors.
