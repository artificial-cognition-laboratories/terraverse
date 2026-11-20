# Sprint 53 — The tool model

Taken from MagicaVoxel and Teardown's editor (a verb and a shape, any
with any), TerraTech/Besiege (a part brings its joint), Fusion/Onshape
(bodies first, joints between them as a tool of their own).

## Decisions

- **A tool is a brush and a shape.** Brushes (`Hand::BRUSHES`): add, cut,
  paint (each node touched made the material held, its shape kept: a node
  is one material), part (what's added grown as a part of its own).
  Shapes as before. One gesture for all: press on a face, drag the
  footprint, wheel for depth; the ghost shows it.
- **Shapes bring their joint.** `Kind::turns`: a cylinder drawn as a part
  is hinged about its axis (a wheel, an axle); the rest are welded. The
  makers (block, wheel, shaft) are gone: a wheel is a cylinder part.
  `Machine::grow` puts the cells on a grid of their own where they were;
  mirrored, the pair's another part.
- **Nothing commits on press.** The primary button draws, selects or
  joins, on release; let go where pressed, it's a click. The other button
  turns what's held (dragged); it never places.
- **Select and Joint** (right rail). Select: a click picks the part (the
  rest dim), its card shows what it's of and its joints, each joint's kind
  set there, detach, delete. Joint: a face of each: two parts of the
  machine held get the joint (`Machine::relink`); two machines are set
  face to face and made one. Hovered parts outline when selecting or
  joining; the selected one always.
- **Rails** low in the corners, a third of the screen tall at most,
  wrapping into columns; what opens goes above the right rail.
- TFM2 (no "made as"); TFM1 reads. TOSERVER_ATTACH/DETACH gone;
  TOSERVER_LINK, TOSERVER_PART (detach, delete), SCULPT's act and apart.

Proved: TestMachine `testGrownOffAFace` (a cylinder part's cells exactly
the stroke's, out of the face, hinged about +x; mirror; a box welded on
the node above; nothing to grow refused); TestPiece `testPainted`
(material changed, shape kept: fails keeping neither).

Not proved: on screen; the rails' wrapping; no end-to-end Workshop test.
With the tools out and not holding, the right button no longer places
blocks in the world (the brush does it all with the primary).
