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

## 4b. Contact and flow (sprint 120) — built, unseen
Hands on what they hold (`physics::armTo`, `Grip`): a hang leans in to the wall with its hands over the edge,
vaults and mantles press on it and let go as the body passes, a side vault uses one hand. A shimmy is hand
over hand (`HandOverHand`) with legs on a pendulum. In the air the hands go for an edge in reach, which is the cue
that it can be taken (no block highlight). Waist high to a two-node wall goes over (`STEPS_OVER`); a mantle keeps
three quarters of the run; Space during a move is the next move; Space remembered lands as a jump. Along a wall,
Space or Space still held runs on it, ahead of any ledge. Local player only: others see key poses.

## 4c. Moves by velocity (sprint 121) — built, unseen
`MOVES` in `physics/reach`: first row that fits by height, pace and angle off square; a Kong takes within 15° at
4.5 nodes/s, a side vault 25–60°, slow lands on top. Vaults keep their direction. Letting go of Space while rising
is a hop (`HOP`). `physics/skeleton` is the figure's bones, pure; `bun pose-sheet` renders every move to a PNG from it.
Next: a mass model (chest and hips with momentum), then timelines with contact pivots, then moves as Lua data, then melee.

## 4d. The mass (sprint 122) — built, unseen
A body's held by what it touches (`Poser`): hung, all of it is a pendulum about its hands, with a caught run's speed carried
into the swing; on its feet the upper body leans into speeding up and turning, and landings go into the legs. In the air the
legs go by how it's rising, leading legs alternate, and a wall kick's leg straightens toward the wall (`PoseIn::push`).
Tuned after play: a stiffer hang, a gentler lean, hands wider, head and shoulders kept out of walls (`Figure::Clear`), hands on the wall in a fast kick.
A wide gait (toes off at the ends of a stride), legs reaching for the landing (`PoseIn::above`) and taking it slower, a one-two shimmy.
Next: falls and collapse (a "down" state combat shares), then timelines and hand pivots, then moves as Lua data.

## 4e. Climbs by force (sprint 123) — built, unseen
`physics::Haul`: a catch's arrival is taken up by the arms (sagging if falling), then a pull that builds, hands off as it rises,
one leg on the top (`PoseIn::step_on`), a drive off it. Falling with the edge at the chest can't vault; falling hard only hangs.
Feet in the air have a top speed. Vaults still follow a path: Kong and side-vault pivots are next.

## 4f. Contact, refined (sprint 124) — built, unseen
Hands stay on what's there (`Body::keepOnEdge`); A and D go by the body while hung; grabs reach 0.75 (a vault's last stride 1.2);
hands stay out of the head and each one's let-go swings the legs; hands have a top speed. A wall kick is a real push
(`physics::WallPush`): in, foot and at speed hands planted, bent, then straightening over 0.09 s. After play: both feet and hands plant and the body turns to the wall; the look steers a kick; turned away while hung, Space jumps off; grabs reach 0.5. Next: Kong and side vault as pivots, then falls.

## 4g. Pivots and corners (sprint 125) — built, unseen
A wall kick uses what's touching: square on all of it, side on the near foot and hand (`WallPush::nearSide`, `facing`). Climbs and
landings go over a block's corner the way they came (`overCorner`, `Ledge::in`). A side vault throws the legs round the wall-side hand,
a Kong dives head first over both (`Contacts::throw_side`, `throw_back`). Space near an edge coming up waits for it rather than kicking; Space held from a jump takes hold; hands go for edges from a stride out.
Next: falls and the "down" state.

## 4h. Balance (sprint 126) — built, unseen
A landing costs by how hard and how far from the way it wants to go (`landingStagger`). A step taken in one physics step is drawn catching up
(`Body::m_lift`), and at a run with a shin's height it's a trip: hands down on it, pitched over, a scramble (`trips`). Next: falls and getting up.

## 4i. Down and up (sprint 127) — built, unseen
Past what balance takes, or landing at 11 nodes/s unrolled, it goes down (`physics::Fallen`, `Body::knockDown`, `Stance::DOWN`): over about its feet the way it's
going, front or back; slides to a stop; hands plant, knee and foot drawn under it, stands. A direction or Space gets up sooner, and off that way. Protocol 95.

## 5. Impact
Punch, kick, knockback (server confirms), ragdoll falls on Jolt's ragdoll.

Each sprint adds to a parkour course in the Lab.

## Risks
Feel is tuned by playing. Seams change the grid's axes under a reach query, and moving ledges
are the same problem. How moves look to other players has to be designed with each move.
