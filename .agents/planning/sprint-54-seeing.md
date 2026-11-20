# Sprint 54 — Seeing it, and carrying it

- **See-through select.** Selecting, the machine's parts are faint glass;
  the part under the cursor is drawn solid and lit, the selected one
  tinted (`GenericCAO::Look`). No boxes: it's the part's own shape.
- **Joints only while selecting**, as handles: a hinge an axle with a
  ring round it, a weld a plate on the face. The part card lists its
  joints, each's kind set there or removed (TOSERVER_LINK joint 255).
- **A new part's joint chosen as it's drawn**: the Part brush has Welded
  / Hinged (`Hand::partJoint`, SCULPT's apart = joint + 1); **Wheel** is a
  preset (cylinder, part, hinged). A shape no longer decides it.
- **Carry** (the tools away): press on an object and it's held as far
  along the look as it was grabbed, posed as you move; let go, it falls.
  Pointed at another's face, a ghost shows it joined there (its face the
  one turned most against that face: `Machine::faceToward`); let go, it's
  welded on, or hinged with Shift. The click-a-face-of-each Joint tool is
  gone: carrying replaces it.

Proved: TestMachine `testFaceToward`. The rest is on screen only:
not seen yet. Clicking a joint's handle doesn't select it (its part's card
lists it). The carried object isn't turned while carried.
