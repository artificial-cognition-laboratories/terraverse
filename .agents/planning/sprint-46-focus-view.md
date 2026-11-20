# Sprint 46 — The focus view

## Decision

The focus view on the world: one block (and what's round it) edited from
outside, as the piece model (sprint 47 on) will edit every piece. F on a
block: the camera orbits it, the player stands, the mouse is free; strokes
come from the cursor; the rail's tools are clicked; strokes can be
mirrored across the block; any stroke can be undone. Esc or F closes it.

1. **Focus.** `Focus` (`client/focus.h`): an orbit round the node, from
   where the eye was; keys orbit (A/D round, W/S over), the wheel zooms
   (sneak + wheel sizes). Through `Camera::setShot`; refused while the
   world's camera (the Lab's Director) holds it.
2. **Pointing.** The ray through the cursor (`Focus::ray`), as far as the
   focused block and the hand's reach past it.
3. **The hand.** `Hand` (`client/hand.h`): the tools' state, out of Game.
   The page reads it (`tools`: tool, size, mirrored, focused) and picks a
   tool (`tools.pick`) on the rail, clickable in the focus view.
4. **Mirror.** Y: a stroke carries its plane (`Stroke::Mirror`), cells and
   their reflection once each; in focus across the block focused, left to
   right on screen; outside, across the block pointed at.
5. **Undo.** U: `Sculptor::apply` returns an `Edit` (every node it and the
   settle touched, as they were); `sculpt::History` keeps each player's
   last 64 on the server (`TOSERVER_UNDO`).

## Done (2026-10-29)

Proved: TestSculpt (9): mirrored strokes (both halves, once where they
meet); undo of a ball dug and a cube placed on a slope that rose into the
cell above: blocks, shapes, and the ground's smoothing as it was (fails
recording only the nodes the stroke names); the focus ray through the
screen's middle meets its block and the left edge is left (fails with the
basis flipped); a page's unknown tool throws. TestShape, TestGround,
TestMapblockMeshGenerator, TestCollision, TestMapBlock pass. Page
typecheck passes.

Not yet: right-drag to orbit (keys only); the mirror plane isn't drawn;
undo puts back what a stroke found even if someone's built there since;
two writers to `Camera::setShot` (the Director, the focus) are kept apart
by a check, not by one owner.
