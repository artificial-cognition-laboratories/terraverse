# Sprint 68: Grab

Touching the world: a point of anything pressed on is pulled toward the
hand as hard as a person, so weight decides what happens. The build keys
get a layer (Alt) so shapes and mirroring don't need the mouse.

## Built

- **Grab** (left-click held on an object, tools away): the point pressed
  on is pulled toward the hand (`physics::World::grab/grabTo/release`).
  The pull is what closes the gap at that point, with the point's own
  mass (a lever's end is light), plus that share of its weight, capped at
  `workshop::STRENGTH`. Light things come up; a beam lifted by its end
  tips about the other; four tonnes of stone barely move. Anyone can grab
  the same thing (grips keyed by player). The wheel brings the hand
  nearer or further. `TOSERVER_OBJECT_GRAB`, protocol 76.
- **Prediction:** the grabbed object is simulated in the client's own
  physics (`Body::grab`) and drawn there, with no wait for the server.
  It's brought toward where the server has it (extrapolated by its
  velocity for the delay) by 5% a frame, and put there if it's 1.5 nodes
  off (`World::correct`).
- **Carry** is Alt-click (the old click-and-hold): whole, turned, fitted.
- **Alt layer:** Alt+1–6 picks the shape (box, cylinder, cone, sphere,
  plane cut, bore) with the material kept in hand; Alt+= steps mirroring
  (on, across the screen's right; each other axis; off). Bound in settings
  as `keymap_alt`.
- **Mirror plane kept:** chosen at the first stroke and kept until
  mirroring's turned off. It was recomputed from the screen's right at
  every stroke, so it flipped as the player walked around the bench.

## Tests

`TestPhysics`: grabbed lifts, tips or holds; corrected toward another;
a cart on a cart pushed (passes: the server's physics doesn't jolt them,
so the glitch the player saw is on the client, still to find).

## Not yet

- The grabber feels no pull back (a heavy thing doesn't drag them).
- Twisting what's grabbed about the line to it (Q/E).
- Other moving objects near the player still draw the server's word,
  a step behind; only what's grabbed is predicted.
- Stacked carts' glitch: not reproduced in the server's physics.
