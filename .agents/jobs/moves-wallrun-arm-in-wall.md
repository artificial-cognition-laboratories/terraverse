# The figure's wall-side arm sinks into the wall after a wall run

Split from testing-moves-bones-stay-out (jobs bot, 2026-11-18). TestMoves.testBonesStayOut holds
wallrun to 0.25 sunk (every other move: 0.2) until this is fixed; put it back to 0.2 then
(`SUNK_MOST` in test_moves.cpp).

What sinks: the **right upper arm, at its elbow end**, while `doing=stand` beside the wall after
landing from the run (t ≈ 5.7, 5.98, 8.2, 10.1, 11.9 s, a few frames each), worst 0.2276 at
5.98 s; the forearm and hand dip in at 5.98 s too. The arm sits between the wall and its own chest,
and `keepArmsOut` won't turn it into the body. The body stands closer to the wall than its drawn
shoulders and arms allow, and the lean (KEEPS_OFF 5/s, capped 0.3, MOST_LEAN 0.45) doesn't take it
far enough off, or not soon enough after the stance changes.

Tried, no good: the elbow as the upper arm's corner in `keepArmsOut` (0.2319); the elbows in the
lean's `wanted` (0.2246, hang got worse).

Needs a call by eye (`bun pose-sheet`, or the wiki scene through `bun check:moves`): the body steps
or leans off a wall faster after a wall run (a GETS_OUT-like rate for shoulders and elbows, not
just the head), or the wall-side arm is posed in front of or behind the body there.
