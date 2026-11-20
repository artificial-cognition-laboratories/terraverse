# Sprint 77: Assemble

Building a shape was easy. Putting shapes together wasn't: a wheel had to
be turned about by hand till it fitted, and once it was on, nothing took
it off again but a menu. The goal is to bench a beam, drag four corners on,
pull one off, change it, put it back, and drive.

## Built

- **A copy goes back on as it hung.** A plug (sprint 76) now keeps the
  face it hung by (`Machine::Plug::By`: its middle and its normal toward
  what it hung on; `Machine::plugFace`). Carried onto a face or a socket,
  it meets it by that face, flush on the sixteenth pointed at, turned
  square, with no Q/E/R. A starter corner meets the chassis by its hub's
  inner face and its wheel stands outboard. F or R still picks another
  side. A machine's text may end in it (protocol 83).
- **Hold left on a workpiece part to pull it off** (tools away). The part
  and what hangs from it come off as an object of their own
  (`Machine::pull`, `Workshop::pull`, `TOSERVER_CLIPBOARD` Pull), carried
  from the point pressed while the button's down
  (`TOCLIENT_HELD` says how it's handed: `workshop::Handed`). Let go on
  a face of the workpiece to move it there, in the open to put it down.
  It keeps its joint (a corner goes back on its spring). The first part
  isn't pulled: the rest hangs from it. Shift-click still selects a part.
- **Anything carried joins the workpiece.** A beam drawn on the ground,
  Alt-carried to the workpiece, snaps on and welds (this was there; it
  was the turning that made it unusable).
- **The ground round a benched workpiece isn't dug** by a press that
  misses it (within its reach plus two nodes).
- **Parts are in the creative catalogue**, as a tab along the top with
  the others, in the same grid, with the hotbar beneath; search finds
  them. The separate Parts panel is gone.

- **It goes on the way it's held.** What's carried keeps its turn
  relative to the player (it turns as they turn). Snapped to a face, it
  takes the square turn nearest how it's held (`piece::nearestFit`, of
  the 24), so a beam held across the view goes on across it from any
  side; a copy still meets by its plug's face, only its turn about the
  joint chosen so. **E** turns it a quarter about the up, **Q** tips it
  toward the player, Shift the other way; four of either is back where
  it began. F and R no longer turn what's carried.
- Snapped, it's pulled to where it'd go, not into what it's fitted to
  (the workpiece shoved it about).

## Fixed

- **Drawn parts met at their nodes, not their shapes.** A beam a few
  sixteenths thick carried onto another was met at its node's face and
  outlined as whole nodes, so it floated off what it was joined to.
  `faceToward` now takes the outermost *filled* sixteenth, and the
  carry's outline is the part's filled extent (`Machine::filled`).

- **The crash** after carrying something just let go of from a grab:
  its coast on the client settled and held it, which ended the carry,
  and the next pull threw ("only what's carried is pulled to a pose").
  A carry ends a coast. The error also skipped `Game::shutdown`, so the
  planetarium's input capture called into a Game already gone
  (SIGSEGV); `~Game` clears it too.

## Tests

`TestMachine` 18 (`testPulledOff`: an arm and its wheel pulled off a
body, the plug's face written and read, put back where it was, hinged;
`testStarters`: corners go on by the hub's inner face; `testOnAsHeld`:
a beam held across goes on across from four sides and at 30°, tipped up
it stands, a plug's face is kept). TestPhysics 32,
TestSculpt 13, TestPiece 8, TestLibrary 3, TestSandbox 11; Lab check;
headless boot; UI typecheck.

Not run: the client. No test drives `Workshop::pull` through the
server (it needs a live environment).

## Not yet

- Part icons rendered from their mesh: a glyph for now (a wheel for
  wheels and corners).
- Dragging a part to a new place on the workpiece is a pull and a put
  back: its joint kind is kept, a socket it was on is not remade.
- Survival costs for pasting.

## Snapping to parts (follow-up)

What's carried is aimed at a part's whole filled box (a quarter node
round it), not the voxel under the crosshair: a frame's top is met over
its hole, a side anywhere along it. Along the face it's lined up with
that part: kept on it (or over it, if bigger), flush with an edge or
centred when aimed within ¾ node of that, else stepped by whole nodes
from an edge. Same-size parts land exactly on each other. Client only
(`Game::handleCarry`, `Machine::faceAt`); the server joins the face it's
sent, as before. Sockets still win over faces.
