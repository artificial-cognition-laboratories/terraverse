# Sprint 52 — Machines

## Fixes first

- Held by its middle as it grows (recomputed each frame: `Hold::reshape`).
- The wheel moves the camera, not the object (`Hold::zoom`).
- Level (most-up axis made up, heading kept) and Reset (square to you)
  on the rail. The Q/E/W/A/S/D turn keys are gone: E's the inventory.
- The hotbar's the palette: clickable with the mouse free (`hotbar.select`).

## Machines

- `piece::Machine` (TFM1): parts and links. A part is an object (design
  and built) at an offset (sixteenths, corner-based) turned one of the 24
  square turns (`piece::turn`): every part's sixteenths are the machine's.
  Joints by table (`JOINTS`): fixed, hinge. TFO1/TFP texts read as one part.
- Makers by table (`MAKERS`): block (welded), wheel and shaft (generated
  discs and rods along +z, hinged about the face's normal), tagged as made
  with their radius for physics.
- Geometry on the machine: `point`/`normal` of a face, `attach` (+z along
  the normal, hub on the point; a block on the node beyond), `mirrored`,
  `join` (their face turned against ours, middles met).
- Workshop: strokes and hollow on a part; `attach` (mirrored pairs),
  `join`, `detach` (a part made its own machine where it was); blueprints
  keep the machine. Entities split out (`workshop/entities`).
- Client: every part drawn at its pose; aim finds the part; mirror is the
  machine's middle in the part's grid. Holding: N attaches a block to the
  face pointed at; the Parts panel sets what clicks do (attach block /
  wheel / shaft at a size, join fixed / hinge, detach, pick a part to work
  on alone: the rest dim; Esc backs out to the machine). Joints drawn:
  a hinge's axis, a weld's cross.

Proved: TestMachine (24 turns and their sums, a pose's frame; the text,
boxes, legacy objects, renew, a link to no part refused; a wheel on a face
facing out, hub on the point, hinged; its mirror; a block attached;
two machines joined face to face, turned part and all). Sabotaged: the
join not turned against, the wheel not turned out: each fails.

Not proved: any of it on screen; no end-to-end Workshop test. Hinges
don't turn yet (sprint 53). Join between two loose things wants the mode
set with the mouse free (pause, or while holding one). A machine's
collision is its parts' box, unturned.
