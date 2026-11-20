# Sprint 71 — Feel

Polish from playing sprint 70: grabbing wheels, wheels that looked to sink,
and the part menu back in the world with joints on it.

## Done

- **A grip sits on what was clicked.** It starts on the sixteenth pressed
  on (the voxel hit), not the face of the object's box, which put it
  between the player and the object. It's held in the frame of the body it's
  on: a wheel's grip turns with the wheel (`World::gripAt`, and the client
  draws the mark there).
- **Grabbing a wheel turns it.** A wheel isn't a body to Jolt, so a grip on
  it was a grip on the car. Now the pull along the rim spins the wheel, and
  the rest pushes the car at the hub (`gripWheel`). When the wheel touches
  the ground, the car's mass counts in turning it, so a pulled rim rolls the
  car only as a turning wheel would.
- **Wheels sinking while pulled was the drawing.** The client glided a
  wheel's offset in a straight line while turning it, but that offset is the
  machine's origin, not the wheel's middle. Between updates a spinning
  wheel's middle cut a chord through the ground. It now glides each group by
  its middle (`content_cao`). The server's physics was checked: a cart
  dragged by a corner sideways, along and diagonally keeps its wheels on the
  floor.
- **The menu is in the world again, drawn as the page's buttons** (`Hud::Tile`):
  a stone face, lit from the top left, the accent when lit, with its sign on
  it. It stands up at the face, facing the player as it was opened, and is
  drawn over the world, never in the ground. Look at a button and click;
  right-click or Esc closes it.
- **Right-click any object for its menu:** Sit (if it has a seat), Work on
  / Put down, Carry (hold). On the workpiece, the first row is joints:
  Fixed, Rotary, Hinge. Near a seam, these change that joint; elsewhere,
  the socket on that face. The current kind is lit, and **hovering one shows
  it where it'd be** (an axle and a ring, a half ring, a plate). The last
  row is Edit, Detach, No sockets and Delete. The workpiece goes
  see-through with its joints drawn while its menu is open.
- **The joint tool is gone**; the rail is Add, Cut and Paint. The page's 2D
  part menu is gone.

## Checked

TestPhysics (new: `testWheelTurnedByItsRim`, and `testCartDraggedKeepsItsWheelsUp`
for the server's side), TestMachine, TestPiece and TestSculpt; the engine
build; the UI typecheck and build; a headless boot; and `bun check:lab`. Not
run in the game client: the menu's look, the hover preview and the wheel
gliding are unseen.

## Left

- Mirrored wheels not landing on the chassis: not reproduced yet.
- The F float.
- A link's joint changed isn't changed on its mirror.
