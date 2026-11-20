# Sprint 75: Suspension and structure

Cars that ride: a wheel's suspension is built, not guessed. And the
structure under it is drawn as structure, not block by block.

## How it plays (creative)

1. **A chassis in one drag:** pick **Frame** (7) on the shape bar, size
   the beams with the wheel (4/16 is a stout beam), drag its footprint,
   click, and pull it up or click again for a flat one. **Truss** (8) is
   the same, braced corner to corner across each side big enough to
   take it. **Beam** (9) is a member from where you press to where you
   drag, at any angle across the face: bracing, a roll cage, a ramp.
   Deck it with a thin Box in timber.
2. **A sprung corner:** a small block on the chassis for a hub (Add a
   part, or carry one on), its joint to the chassis set to **sprung**
   (right-click the joint: it's on the top row, a coil). Grow the wheel
   on the hub (rotary, steers or driven, as before). Right-click the
   sprung joint again for its **travel** (short, medium, long) and
   **firmness** (soft, medium, firm). Where you built the wheel is its
   ride height: it sits there on its spring.
3. Copy the corner (Ctrl+C on the hub), paste it three times.
4. **Round** (the rail, beside Hollow) rounds the part's edges and
   corners to the tool's size.

## Built

- **Frame, truss, beam** (`sculpt::Tool`, `Kind::Round::Frame/Truss/
  Line`): cells of the drawn box within the size of two of its sides; and
  of each side big enough (three sizes), within half the size of its
  diagonals; a beam's within half the size of the line across the face.
  They're strokes like any other: mirrored, undone, grown as parts.
- **The sprung joint** (`piece::Joint::Sprung`, `JointKind::sprung`):
  held as welded (nothing on it can fall off), and a wheel turning on its
  hub rides its spring: `physics::Wheel::travel` and `sag` from its
  `piece::Spring` (`TRAVELS` 0.25/0.5/0.8 nodes, `FIRMNESSES` 1.2/1.7/
  2.4 Hz, sag = g/ω²). Of a spring's two parts the hub's the one with
  fewer joints. A wheel on no spring rides as it did (0.1 travel, stiff).
  The machine's text is TFM5 (each joint's spring; TFM4 reads with the
  middle steps). `TOSERVER_LINK` ends in the spring. Protocol 81.
- **Round** (`Sculptor::round`, an opening: what a ball of the radius
  rolled round inside can't reach is taken); Hollow shares its set-up.
  `TOSERVER_HOLLOW` ends in `rounded`.
- **Right-click to stop a stroke** no longer opens the workpiece's menu
  as it's let go (`m_place_spent`).

## Not yet

- The hub doesn't move with its wheel: the wheel rides up and down past
  it. Swinging arms (a wheel's arc) need free bodies; later.
- The spring's settings are on the joint's own menu (hover the joint),
  not the face's.
- A frame's footprint is four blocks at most, as a box's is: a longer
  chassis is two frames joined.

## Tests

`TestSculpt` 13 (`testStructure`: the frame's a ring, the truss braces
it, the beam's a diagonal; `testRounded`), `TestMachine` 16
(`testSprung`: a wheel on a sprung hub travels and sags as its spring
says, kept in the text; TFM4 and TFM3 still read), `TestPhysics` 31
(`testSprings`: dropped, soft long springs give more than stiff ones,
and both settle where they were built). Lab check, headless boot of a
copy of a played world, UI typecheck.

Not run: the client.
