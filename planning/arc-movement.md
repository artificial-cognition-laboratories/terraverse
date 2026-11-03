# Arc: a body that moves

Terra Firma is a universe games happen in, so the body has to serve parkour, stealth,
survival and racing alike. Three layers, each ignorant of the one after it:
1. **Reach:** what's possible from here (ledges, gaps, cover), a pure query of the blocks,
   cuts and machines round the body.
2. **Movement:** what the body does (`physics/walk`, grown into states). All physics, no clips;
   moves are data a world's rules can switch off or put a cost on.
3. **The figure:** shows it (`physics/pose`, `client/figure`). Never affects physics, and
   rebuilt from a little state on other players' machines.

Learned from: David Rosen, "An Indie Approach to Procedural Animation" (GDC 2014): few key
poses on springs. Limbs by two-bone inverse kinematics. No animation library.

## 1. The body (sprint 115) — built, unseen
13 blocks: hips, chest, head, upper arms, forearms, block hands, thighs, shins. `physics::Poser`
gives the joints: feet by where they must be (planted feet don't skate, and meet cut or smoothed
ground), the upper body on springs between key poses. F5 adds over the shoulder, drawn in by
walls; first person sees its own body, without the head. No new moves.
The camera doesn't follow the head yet: it has nothing to follow until stances move it.

## 2. Stances (sprint 116) — built, unseen
Hips face the way it goes, capped short of square; backing up runs the stride in reverse; standing
still the feet hold till the hips reach their limit. `physics::Stance`, rows of `STANCES` and
`ROWS`: crouch and prone, each with a smaller capsule (`World::resize`, refused under a low roof),
sent to others (`AO_CMD_SET_STANCE`, protocol 91); the view eases after it. Crouched, it doesn't
walk off an edge (`Body::keepOnEdge`). Keys: Shift runs and thrusts, Ctrl crouches (tap or hold)
and flies down, Z lies down, C the camera, N frees the cursor, F gets up, P zooms. Shift-click
modifiers read Shift (aux1), Ctrl+C/V/S read Ctrl (sneak).

Then: the head does the turning, the chest stays with the hips; the shoulder view holds above the
feet, rises over what's behind, and eases.

## 2b. Slides and speed (sprint 117) — built, unseen
Crouch at a run slides (`physics::slide`, friction and slope); crouch asked landing hard rolls and
takes the fall (`physics::roll`, `rollsOut`), otherwise the landing costs speed. In the air it keeps
what it left with (`Gait::air_reach`). A clean line builds past a run (`Gait::flow`), a sharp turn
costs speed; a jump throws it up to 30° toward where it's asked, keeping most of its speed. A slide ends
crouched; pressing run gets up. `nextStance` decides; protocol 92. The shoulder view eases only how far and how high.

## 3. Reach (sprint 118) — built, unseen
`physics/reach`: `findLedge` from which nodes are solid, `climbFor` by how high above the takeoff.
A new press of Space: up to about a node moving toward it vaults from the ground; in the air, about
two mantles, about three hangs. A and D shimmy, Space pulls up, Ctrl lets go (`Body::climbStep`). Whole
nodes only: a cut edge counts as its node. Lab: `moves.walls`. Protocol 93.

## 4. On walls (sprint 119) — built, unseen
A vault goes at the speed it came in, and lands just past the wall. S lets go too. In the air, Space
with no ledge in reach: along a wall at speed runs on it (`runsAlong`, up to 1.2 s), otherwise kicks off
(`wallKick`); not the same wall twice before the ground. Coming in at a slant, a side vault. Protocol 94.
Still: a Kong pose of its own, corners, moving ledges, seams mid-climb, an F3 view of the query, hands by IK.

## 5. Impact
Punch, kick, knockback (server confirms), ragdoll falls on Jolt's ragdoll.

Each sprint adds to a parkour course in the Lab.

## Risks
Feel is tuned by playing. Seams change the grid's axes under a reach query, and moving ledges
are the same problem. How moves look to other players has to be designed with each move.
