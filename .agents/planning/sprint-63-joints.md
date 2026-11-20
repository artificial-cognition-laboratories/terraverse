# Sprint 63 — Joints, snapping, and the menu in the world

Sprint 62 played: wheels drawn on the ground wouldn't go on the car. A
wheel's drawn lying flat, and carried to a socket it met it with its rim:
the carried part offered whichever face pointed most at the socket. And
"hinge" was one word for two joints.

## Done

- **Three joints** (`piece::Joint`, `JOINTS` as data): **fixed** (welded),
  **rotary** (spins freely: an axle), **hinge** (turns 90° either way,
  then stops: a door, a steering knuckle). Saved by number, so what was a
  hinge (free) loads as rotary. Physics gives a hinge its stops
  (`physics::Hinge::limit`, Jolt's hinge limits). Protocol 73.
- **Snapping onto a turning socket:** what's carried meets a rotary or
  hinge socket with its thinnest side, so a wheel drawn flat stands up on
  its axle however it's carried. The ghost shows where it'll sit; let go
  to join.
- **R turns what's carried** a quarter about the up, Shift+R about the
  view's right. For welding a part on by any face.
- **Plain faces only weld.** Shift no longer hinges a face: joints that
  move are made by sockets, so they're chosen, not held down.
- **The joint menu is in the world.** Right-click a face of the
  workpiece: three buttons (Fixed, Rotary, Hinge) stand off that face,
  each with its sign and name. Look at one and click: a socket of it
  there (and its mirror's, mirrored). Click off them or Esc to close. The
  cursor stays captured. The page's menu is gone (`tools.menu` out of the
  contract); the part card keeps joints, sockets and detach/delete.
- The tool rail's part joints are Welded, Rotary, Hinged; Wheel draws a
  cylinder part on a rotary joint.

Proved: TestPhysics 16/16 (new: a hinge stops, its wheels turn to their
stop down a slope). TestMachine 11/11. Not seen on screen.

## Next

- The car built in game: four rotary sockets, four wheels, pushed.
- A hinge's stops set per joint (now 90° each way for all).
- Handles to push and pull by; a motor on a rotary joint.
- Slider and ball joints when something needs them.
