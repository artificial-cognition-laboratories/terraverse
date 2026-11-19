# TestFar and TestScale fail on master (not in CI's modules, so CI is green over them)

Found by the CI owner, 2026-11-18, running the whole unit suite (`bun run test`, 86 modules) at
5eb0a8b2 and again with tonight's test.sh: the same two fail with either script, so they predate it.

    TestFar.testNoHolesFromAbove   test_far.cpp:390
      radius 1304, 0 levels under the roots: 453520 triangles, 1 of 40000 rays through a hole
    TestScale.testEverySize        test_scale.cpp:96   steep < 40
      SCALE radius=1000000 ... steepest=145; atlas: the tiles (901 of 225 slots) don't fit the map

Likely sprint 116 (0f12048e4, "Land by the book": steeper relief), but not confirmed. Reproduce:

    ./.output/bin/luanti --test-module TestFar
    ./.output/bin/luanti --test-module TestScale

CI runs only TestOnline, TestRelease and TestTerragen today, so these don't hold master red. When the
fast tier runs the whole suite in CI they will: fix or skip them by name (scripts/test/skip.txt) first.

## Agent notes (2026-11-18, jobs bot)

Skipped both by name in `scripts/test/skip.txt` (committed), so the fast tier can take the whole
suite; the fixes stay open here.

- **TestScale.testEverySize**: real, and sprint 116's. At radius 1M, 40° N, x=589086..589114 the
  ground goes 1118, 1170 (+52), …, 1185, 1040 (−145), …, 1021, 1077, …, 1013, 1064: block-wide
  pits and spires 50–145 high, a single column apart. At 250k the worst is 38 (just under 40).
  So the amplifier/channel cuts below the grid (phase 6: `terragen/amplify.*`, `channels.cpp`)
  aren't held to a hillside gradient once a cell is ~14k blocks across. Fixing it changes
  generation, so it belongs with the sprint's TF25 bump, not a night fix. Reproduce by printing
  `world.ground(at(x), sun).terrain` round that x in the test.
- **TestFar.testNoHolesFromAbove**: one slanting ray (lat 54.84, lon −74.17, region 0/19) is
  a real miss. Brute force over all 453k triangles finds no crossing near the ground point (the
  binning is innocent). Near it, on steep land (cell tops 29, 49 and 53 within 20 blocks), the ray
  passes about 0.02 blocks outside the shared edge (−13.77, 23.86)–(10.63, 21.56) at h 29, between
  a low cell's top and the next column's skirt wall (barycentric u ≈ −0.001 on both sides). It
  grazes the north-west corner of a 53-block column, so the slit is where two tiles' edges are
  placed separately. Raising SKIRT from 2 to 4 didn't close it. Next: dump which tiles own those
  two triangles (`t.place` per key), and check whether the edge vertices of neighbouring tiles at a
  level change are placed from the same heights.
