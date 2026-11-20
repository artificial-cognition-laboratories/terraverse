# Sprint 70 — Build mode

Building had four modes (hands, World X, Create C, Select) that each changed
what the rail, the numbers, the menus and the mirror did. This sprint folds
them into one build mode, separates tools from shapes, and gives every
workpiece a front.

## Done

- **Tab is build mode**, in and out. X and C as ways in are gone; C now frees
  the cursor (was Tab). Add on the ground or a socket makes an object or part;
  **Alt held adds into the world**. Cut and Paint work on whatever they hit:
  terrain or an object.
- **Tools and shapes are separate.** The left rail is Add, Cut, Paint and
  **Joint**; R steps through them. The shapes sit on a bar on the hotbar
  (`ShapeBar.vue`) and **1–6 pick them** while building, with the size at the
  end (the wheel). Alt and a number picks the hotbar item. The cut-plane shape is
  now called "Plane", so it isn't confused with the Cut tool.
- **Select is gone.** Parts are clicked with the tools away, as before.
- **The joint tool:** clicked within half a block of a joint between two
  parts, it steps that joint through fixed, rotary and hinge. Clicked
  elsewhere on a face, it steps the socket there through none, fixed, rotary,
  hinge and back to none (the socket's mirror goes with it). With the tool
  out, the workpiece is see-through and its joints are drawn.
- **The part menu is the page's** (`PartMenu.vue`, state `partMenu`). It
  offers Edit alone, Detach, Clear sockets and Delete, beside the crosshair,
  and frees the cursor while open. Right-click opens it in build mode too. It
  is no longer drawn in 3D, so it can't sink into the floor. Its
  socket and joint rows went to the joint tool.
- **Every workpiece has a front** (`Machine::front`, machine text TFM4,
  protocol 78). It is set when the object is begun, to the way its maker
  faced, and older machines face their grid's +z. A blue arrow on the bench
  disc shows it. **The mirror follows the front**, not the view: Alt+=
  cycles left/right, front/back, then off.

## Checked

TestMachine (front round-trip, TFM3 read as +z, a bad front refused),
TestPhysics, TestPiece and TestSculpt; the engine build; the UI typecheck
and build; a headless boot; and `bun check:lab`. Not run in the game client.

## Left

- F floats the workpiece up on a spring instead of snapping it (agreed, not done).
- Wedge and slope shapes for hulls.
- A link's joint isn't mirrored to its twin; a detached part's front
  defaults to +z.
- Driving still takes forward from the wheels' axis, not the front.
