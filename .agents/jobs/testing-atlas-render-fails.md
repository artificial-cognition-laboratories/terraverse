# TestAtlasRender fails on master: holes in views, a view over budget

Found by the testing owner, 2026-11-17, running the whole unit suite (`luanti --run-unittests`) at 194ce4b1a. The release gate runs only TestOnline, TestRelease and TestTerragen, so this went unseen.

- `testViewsMatchThePlanet`: "4 of 40 eyes see the planet wrong" (test_atlas_render.cpp:468). E.g. `eye lat 89.9 lon 137 +3: 1368 rays, 1 holes`: holes near the pole.
- `testWithinBudget`: "1 of 4 views over budget" (test_atlas_render.cpp:506): `eye lat 84.6 lon 10 +3 ... choosing 2266 ms (1500)`. This one is a wall-clock budget, so it may also be load-sensitive; it failed on an otherwise idle sequential run.

Reproduce: `bash scripts/build.sh -j8 && ./.output/bin/luanti --test-module TestAtlasRender`

Both are skipped by name in `scripts/test/skip.txt`; remove those lines when fixed.
