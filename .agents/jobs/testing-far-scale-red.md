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
