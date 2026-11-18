# TestMoves.testBonesStayOut fails on master: the figure sinks into walls

Found by the testing owner, 2026-11-17 (whole unit suite at 194ce4b1a). `deepest < 0.2f` fails at test_moves.cpp:187: `sunk wallrun: 0.227589 at 5.98 s, 890/1799 frames past 0.05` (shimmy is 0.18, just under). The figure's limbs go into the wall while wall-running. `bun check:moves` runs this module, so it was red there too.

Reproduce: `./.output/bin/luanti --test-module TestMoves`. Skipped by name in `scripts/test/skip.txt`.

## Agent notes (2026-11-17, jobs bot)

Not fixed; nothing committed for it. What I found:

- Not a recent regression. Physics code is unchanged since 6450cd82d "movement" (Nov 6) apart from
  `physics/world.cpp`; putting back world.cpp from before e4635c7df and from before 2949bb83b gives the same
  0.2276, so the perf commits didn't do it. Most likely red since the Nov 6 motion rewrite.
- The deepest part isn't during the wall run itself: it's the **right upper arm (side 1), at its elbow end**,
  while `doing=stand` beside the wall after landing (t ≈ 5.7, 5.98, 8.2, 10.1, 11.9 s, each a few frames).
  The right forearm and hand dip in at 5.98 s too. Found by printing the box and corner in `Actor::sunk()`.
- Tried, both reverted: (1) `keepArmsOut` testing the elbow as the upper arm's corner (BONE_WIDE half-diagonal)
  instead of the forearm's radius: no change (0.2319). The arm can't swing out because it's between the wall
  and its own chest, and the loop stops when turning would put it into the body. (2) The elbows counted in the
  lean's `wanted` (as the shoulders are, `ball(...)` in Posing): 0.2246, and hang got slightly worse.
- So the body stands closer to the wall than its drawn shoulders and arms allow, and the lean (KEEPS_OFF 5/s,
  capped 0.3, MOST_LEAN 0.45) doesn't take it far enough off, or not soon enough after the stance changes.

What would unblock it: a call on the pose. Either the body leans or steps off a wall faster after a wall run
(GETS_OUT-style rate for shoulders and elbows, not just the head), or the arm on the wall side is posed in
front of or behind the body there. That needs someone to watch it (`bun pose-sheet`, or `bun check:moves`
with the wiki scene) rather than tune a number blind. Or the limit for wallrun could be loosened as kong and
the others are, if that's acceptable.
