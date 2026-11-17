# TestMoves.testBonesStayOut fails on master: the figure sinks into walls

Found by the testing owner, 2026-11-17 (whole unit suite at 194ce4b1a). `deepest < 0.2f` fails at test_moves.cpp:187: `sunk wallrun: 0.227589 at 5.98 s, 890/1799 frames past 0.05` (shimmy is 0.18, just under). The figure's limbs go into the wall while wall-running. `bun check:moves` runs this module, so it was red there too.

Reproduce: `./.output/bin/luanti --test-module TestMoves`. Skipped by name in `scripts/test/skip.txt`.
