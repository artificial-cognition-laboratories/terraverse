# TestFarPalette fails on master: the far view's level of detail

Found by the testing owner, 2026-11-17, running the whole unit suite at 194ce4b1a (likely since a8ab49e2a "Perf fixes ... TF23", which touched far/).

- `testTilesForAnEye`: "the nearest tile (level 3) isn't finer than the farthest (3)" (test_far_palette.cpp:197): every drawn tile is at one level.
- `testNothingInsideTheCut`: "a tile reaching 395 blocks off is drawn inside the cut (700)" (test_far_palette.cpp:314): the far view draws over the near ground.

Players would see the far view without detail near them, or overlapping the real ground.

Reproduce: `./.output/bin/luanti --test-module TestFarPalette`. Skipped by name in `scripts/test/skip.txt`.
