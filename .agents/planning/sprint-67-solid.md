# Sprint 67 — Solid

Cars built in half a minute, pushed, tipped over (wheels the wrong size:
good). What went wrong: a wheel sank through the floor when a car was put
down, a tipped car lay in the ground, and running over a car threw it.

## Why

- A wheel's tyre is cast for the ground (sprint 66) and its body is
  touched only by players: on its side, or put down with a wheel already
  in the ground (drawn on a socket at ground level), nothing held it up.
- Standing on a held mirror, the client sent the player's weight as a
  push (~520 N·s a second, down) on top of the server's own character
  standing there; pushes landed at a corner of the body's bounds, which
  can be high and off the body. Together: a light car run over was
  stamped on at its edge and flipped.
- A player placed once a server step went all the way in the first of
  that step's 5–6 physics steps: walking was ~30 m/s to what it met.
- Thin parts (a sixteenth) could pass through the ground in one step.

## Built

- **Wheel backstops**: each wheel's cylinder, short of the tyre by its
  suspension's travel and a little, is in the root's shape. It only meets
  what the tyre lets past: a tipped car rests on its wheels, a wheel built
  into the ground is lifted out.
- **Swept bodies** (`EMotionQuality::LinearCast`) for every machine body.
- **Stood on isn't a push**: contacts pressing down aren't sent.
- **Pushes at the centre of mass's height** (at the nearest point of the
  bounds across): corners turn it, nothing tips it.
- **Placed players move through the step's physics steps**, a share each.

## Checked

TestPhysics 21/21: `testTippedWheelsHoldUp` (on its side it rests on its
wheels' faces, 0.25 up, not its plate's edge, 0), `testPutDownInTheGround`,
`testWalkingIntoACartDoesntThrowIt`, and standing on a held block sends no
push (it sent 520 N·s/s before; checked by undoing the fix). The walking
test passes without its fix too, so it guards but didn't find the throw.
Full unittests: only the old TestAtlasRender failures. Headless boot
clean. Not seen on screen.

## Next

Interactions: grab a point of an object and pull it (righting a tipped
car is that), then twisting about an axis.
