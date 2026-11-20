# Sprint 64 — Fit

Sprint 63 played: the snap showed an outline, but what was in hand stayed
as it was held, so you couldn't see or steer how it would go on, and the
wheel went on wrong. The aim: build a car in under a minute.

## Done

- **What's carried goes where it'll fit.** Near a socket or a face, the
  part is drawn where it'd sit, its outline round it; look away and it's
  back in the hands. Let go while it's fitted and it's joined there.
- **The fit is chosen, not inherited from the hands.** Which of its sides
  meets the socket is picked once, when it first fits there (onto a
  turning socket: its thinnest side, so a flat wheel stands up), and kept
  while it fits the same thing. Then:
  - **Q / E** spin it a quarter about the joint (`spin`: join and plug
    take it, so the server makes exactly what's shown; protocol 74);
  - **F** puts its other side to it;
  - **R** takes the next axis round.
  With nothing to fit to, Q/E turn it in the hands about the up and R
  about the view's right. (R's turn used Irrlicht's angles before and went
  the wrong way; it uses the entity's own now.)
- **Keys:** inventory is **I**, drop is **Backspace** (Q and E are the
  turns). A Building group in Settings ▸ Controls.
- **Shift-click a socket** with the tools away: another of the last thing
  you plugged in, put on it as that was (`TOSERVER_PLUG_AGAIN`; creative
  only, since it's made from nothing). One wheel and three Shift-clicks.
- **Parts light up as you look at them** (tools away): any object's, and
  the floating workpiece's.
- **Sockets show their joint:** a square plate (fixed), a whole ring round
  an axle (rotary), half a ring (hinge).
- **The joint menu** closes with a right-click too.
- **The select trap:** the rail's Select mode could only be left with the
  cursor freed (its Done button). Esc leaves it now.

Proved: TestMachine 12/12 (new: joined spun a quarter about the joint,
faces still meet, four quarters are none). TestPhysics 16/16. Not seen on
screen.

## Next

- Undo for joins and plugs.
- A hinge's stops set per joint; a spin arrow on rotary joints.
- Copies in survival (they'd cost the materials).
- Handles to push and pull by; a motor on a rotary joint.
