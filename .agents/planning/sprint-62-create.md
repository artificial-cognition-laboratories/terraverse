# Sprint 62 — Create

Build mode has two ways in. **X shapes the world**: add, cut and paint
change the terrain, as before. **C creates**: everything drawn is an
object, never terrain, with the same tools and the same draw-then-extrude
flow. The same key again leaves build mode; the other key switches to the
other way.

- **The first shape is the workpiece.** Creating with no workpiece, the
  first shape drawn on the ground becomes the workpiece as soon as the
  server has made it an object (`Game::adoptNext`/`adoptCheck`: a new
  object near where it was drawn, within 3 s). It goes on the bench,
  lifted half a node.
- **Creating, adding makes parts** (`Hand::acting`): drawn on the ground,
  a loose part to carry over; drawn on the workpiece, a part of it. Cut,
  paint and pull work only on objects, never on the ground. The rail shows
  the way in ("Create" / "World") and the brush as it acts.
- **F on another object makes it the workpiece** (one at a time); F on
  the workpiece puts it down, as before. Leaving creating puts it down too.
- **N is gone** (`keymap_piece_new` → `keymap_create`, C). The rail's "new
  object of the block held" stays.
- **Light under partly filled blocks.** The lighting keeps no light in a
  cut node (it counts as its material, solid), so the ground under and
  beside a thin slab was black. Meshing now takes, for a cut node, the
  light of the first uncut node above it, both smooth and flat.

Proved: the engine builds; its tests pass but TestAtlasRender's two, as
before; UI typecheck; a headless server boots clean. Lighting has no test
(meshes aren't tested headless). Not seen on screen.

Next: build the car in game (a beam chassis, axles, hinged wheels, pushed),
and what it shows about joints and snapping.

## After play

- Shapes drawn creating are each their own object; none is put on the
  bench by itself. F is the only way onto it.
- The workpiece stays floating when the tools are put away or switched to
  X (shaping the world, it's just another object). With the tools away you
  can carry other objects about it. It's put down by F, F on another
  object, Esc, or walking more than 10 nodes off.
- Built objects were lit from their origin, often inside the ground they
  rest on, so they drew black. They're lit as brightly as the air up to
  two nodes above it too.
