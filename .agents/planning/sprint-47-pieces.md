# Sprint 47 — Pieces

## Decision

A piece is blocks on a grid of its own, cut to sixteenths as the world's
are, not fixed to the world: the start of every detached thing (a hull, a
barrel, a cart). It's an entity whose `piece` property holds its blocks;
both sides edit it through a small Map of its own, so the shapes, the
mesher, the tools, mirroring and undo all work on it unchanged. The focus
view orbits one as it orbits a block of the world.

1. **The piece.** `piece::Piece` (`src/piece/`): up to 16 a side, local
   node (0, 0, 0) at the entity; written by node name (a palette), so the
   same text is sent, saved (staticdata) and read back whatever the ids.
   `piece::extent` (no map, no names) sizes the entity's selection and
   collision (`ObjectProperties::validate`).
2. **Drawn.** Visual "piece": the block mesher over the piece's grid,
   lit as an object (`generatePieceMesh`).
3. **Made and worked.** P (tools out): a new piece, a block of the block
   held, where you point (`TOSERVER_PIECE_NEW`). `TOSERVER_SCULPT` names
   a piece (its object id) or the world (0); the server applies the stroke
   in the piece's grid, clipped to it, and sends its entity's properties.
   Undo knows which it was made on.
4. **Pointed at.** A piece's ray again in its own frame, against its
   blocks as cut (`Piece::raycast`); the ghost drawn through its frame.
5. **Focused.** F on a piece: the orbit round its middle, following it;
   mirroring across its middle in its own grid. Right-drag orbits in the
   focus view (a right click is its release, undragged).

## Done (2026-10-29)

- The grid has a block of air round it every way: light spread and the
  tools read past its sides (without, placing at its edge failed).
- Fixed on the way: `MapEditEvent::getArea` didn't know `MEET_SHAPE`
  (sprint 44): a shape change's area was empty.

Proved: TestPiece (5): written and read back (blocks, params, cuts; the
same text again); by name (other ids, the same blocks; an unknown name and
text that isn't a piece throw; it caught coordinates read out of order);
sculpted in its grid (dug, placed, undone, nothing past its side); pointed
at through a cut to the block behind; its entity's box and its piece over
the wire. Headless (a server, a world mod): a piece entity made from its
text keeps it, is boxed as its blocks, saves it as its staticdata.
TestSculpt, TestShape, TestGround, TestMapblockMeshGenerator,
TestCollision, TestMapBlock, TestAtlasView pass.

Not yet: drawn (the client mesh isn't run headless: Cody); a piece stands
still in the air (no gravity, no moving it, no turning it); it's stood on
as its box, not its blocks; on the planet, a piece's ghost is placed as
the node at its origin is drawn (near enough a few blocks out).

## The focus view's crash (2026-10-29)

Cody: the game hung opening the focus view; the mesh thread died with "a
cut block's clipped buffer outgrew 16-bit indices", and a stroke before
it was refused out of reach.

- Clipping a seam block's mesh to its territory (`clipToTerritory`) adds
  the clipped triangles' vertices to their buffer; sixteenth-fine ground
  and cut blocks fill buffers near their 16-bit limit, and it threw. Now
  what won't fit goes to buffers of its own (of the same material, one
  buffer's spill never mixed with another's).
- The focus view's camera can be further out than the player stands; the
  server measures reach from the player. `sculpt::REACH` is both sides'
  now: past it the client shows no ghost and sends nothing.

Proved: TestAtlasRender `testClippingSpillsOver` (new): a seam block near
its band's edge, its buffer near full, a cube over each cut cell: clipped,
it spills, every buffer under the limit, every index in its buffer; fails
with the old append. The view and budget faults as before.
