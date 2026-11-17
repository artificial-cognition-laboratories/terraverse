# TestScale.testEverySize fails on master: land too steep on a small planet

Found by the testing owner, 2026-11-17 (whole unit suite at 194ce4b1a). `steep < 40` fails at test_scale.cpp:96: `SCALE radius=2607 round=16380 ... steepest=283`. A planet of that size has a column 283 steep against a limit of 40: cliffs where the land should be walkable. Generation change, so possibly tied to the TF23 bump.

Reproduce: `./.output/bin/luanti --test-module TestScale`. Skipped by name in `scripts/test/skip.txt`.
