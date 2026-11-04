# Sprint 103: Gliders

The first of aircraft: lift comes from the shape you draw, not from a wing block.

## Built

- **`physics/aero`:** each body's boxes are sliced along each way across its front.
  A thin run along the front is a wing's section: its slope is incidence, its bow
  camber, its depth thickness. Side by side, sections make a wing; span against
  area is its aspect. A fin is the same on its side. A block isn't a wing.
- **Forces:** each surface lifts by strip theory, stalling past 10 to 16° by
  thickness. Everything drags with its silhouette along each of its own axes,
  replacing one blunt area. Thin sections are taken out of the silhouette ahead
  and pay friction instead. Forces act where they arise, so balance is real.
- **Ahead with no seat** is the machine's front.
- **The balance on the bench:** with a workpiece focused, its centre of mass and
  centre of lift are marked, with how far apart they are and whether that's steady.

## Tests

`TestPhysics` 36: `testAeroReadsShapes`, `testGlides` (a built glider settles to a
glide better than 5:1; a lump of its weight only falls).

Not run: the client. Hand-over: build a wing, tail and nose weight, read the marks,
launch it off something high.

## Found

- Solid 1/16 wood is 37.5 kg/m² of wing, so gliders are fast (about 40 m/s). A light
  material will matter.
- With no dihedral a glider wanders into a turn. That's real, not a bug.

## Not yet

- Control surfaces and flight controls, then props and thrust (and the centre of
  thrust mark), fuel, balloons, turbines.
- A wing joined to a fuselage counts it in one aspect, so a joined tail's is overstated.
- Wind in the physics field.
- Whether terrain loads fast enough for an aircraft's speed.
