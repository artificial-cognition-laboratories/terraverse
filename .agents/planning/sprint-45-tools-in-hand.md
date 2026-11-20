# Sprint 45 — Tools in hand

## Decision

Sprint 44 cut a block a sixteenth at a time. Now the hand holds a tool, a
shape and a size, and works the world at any size from a sixteenth to a
few blocks: the rich editing every later tool is built on. The tools are
Cody's list (Cut, Bore, Extrude, Sculpt, Pattern, Measure, Join, Save),
which later become survival's own tools; this sprint is the three that act
on the world where it stands:

- **Sculpt**: fine local editing. A cube or a ball; dig takes it away,
  place adds it (of the held block, else the material pointed at).
- **Cut**: planes. Dig shaves everything standing out past the face
  pointed at, place fills everything short of it: flat ground, flat walls.
- **Bore**: holes. A round passage into the face pointed at.

1. **Strokes.** `sculpt::Stroke`: a tool, a size (a sixteenth to four
   blocks), where it's pointed (a sixteenth and its face), dig or place:
   the sixteenths it takes or fills, in the world's sixteenths. A size of a
   whole block or more snaps to the grid (a 1-block cube is Minecraft's dig).
2. **Applied.** The server applies a stroke to the map: smoothed ground
   touched is settled first, each node's sixteenths taken or filled, one
   material to a node (filling never mixes), a node emptied is air, one
   filled whole is whole. `TOSERVER_SCULPT` replaces `TOSERVER_CUT`.
3. **In hand.** X takes the tools out or puts them away; R moves through
   them; the wheel sizes it while they're out. The status line names it.
4. **Ghost.** What a click would change is outlined where it is, its edges
   only, before the click.
5. **Proved.** Each tool's volume (snap, the ball inside its cube, the cut
   at the plane); a stroke applied to a map: settled first, materials kept
   apart, emptied nodes air. Cody: digs, flattens, bores.

Not this sprint: the focus view, Extrude/Pattern/Measure/Join/Save, pieces
off the grid, undo, light through cut nodes (a cut node shades as whole).

## Steps 1–4 (2026-10-29)

- `sculpt::Stroke` (`src/sculpt/`): `cells()` in world sixteenths; what
  each tool is lives in `KINDS` (places, works outward, depth, round).
  Sizes of whole blocks snap to the node grid.
- `sculpt::Sculptor::apply`: settles touched ground, then per node: dig
  takes from blocks (`NDT_NORMAL`), emptied → air; place fills open nodes
  (`buildable_to`) and cut nodes of its material, never another. Placed
  material: the held block, else the one pointed at.
- `TOSERVER_SCULPT` replaces `TOSERVER_CUT` (still protocol 65: the chisel
  never shipped). Creative, interact, reach 12 + the stroke's size.
- In hand: X tools out/away, R next tool, the wheel sizes (1/16 … 4
  blocks) while out; the status line names it. The ghost: the dig
  stroke's outline (`sculpt::outline`, its bends only), placed on the
  planet through `AtlasView::place` (which `outline` now uses too).

Proved: TestSculpt (6): sculpt volumes (a sixteenth, a block snapped, a
ball in its cube and round); the cut at the plane both ways; the bore
round, twice its size deep, never placing; outlines (a box is 12 edges at
any size, an L 18); applied (a block dug to air, a ball cut, placing into
the open and never into stone, a cut filled whole again); a slope dug
from its shape. Fails without the settle, and with flat faces outlined.

Not yet: the ghost shows only what a dig takes; a large ghost (4 blocks)
is recomputed as the crosshair moves (≈260k sixteenths), likely a hitch;
cut nodes shade as whole (no light through them); no undo.
