# Sprint 78: Drive (first pass)

The buggy crawled: its engine was 40 N·m (a lawnmower's) on a block
weighed as 0.4 m³ of iron (~3 t), and the seat as half a cubic metre of wood.

## Done

- **Engines are a power, not a torque.** `_machine = { power = kW, rpm = most }`;
  the vehicle's engine gets the torque that gives that power on Jolt's
  curve, shifts by its revs (0.8 / 0.4 of the most, not Jolt's fixed
  4000/2000) and a clutch that holds it. Several engines add their power,
  turning as fast as the slowest. The motor is 370 kW (500 hp) to 7000 rpm.
- **Fittings weigh what they say** (`_machine.mass`, kg): engine 220, seat 25.
  They're made things (crafted later), not chiselled blocks.
- **Snapping along a face is to the sixteenth**, flush or centred within
  ¼ node; the box aim stays.
- Protocol 84 (TOCLIENT_MATERIALS: power and rpm).
- TestPhysics `testQuick`: a 470 kg cart on 370 kW, rear-driven, is at
  15 m/s after 3 s and 27 m/s after 6 s (grip-limited).

## Not yet

- Built structure is still solid matter: a full-block wooden plank is
  ~600 kg/m³, a steel frame's beams 7.9 t/m³. Hollow sections (tube,
  box) as a shape property would be the honest fix.
- Anti-roll bars and struts: the player's to build (a linking mechanism).
- Speedo and revs wait for the widget work.
- Air control, self-righting, camera pull-back: not done.
