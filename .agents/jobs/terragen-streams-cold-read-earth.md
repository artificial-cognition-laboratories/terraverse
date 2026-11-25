# Streams::read at Earth's size: seconds of a core for a read somewhere new

Found by the testing owner, 2026-11-22, timing `terragen.soil.slow` (`tests/soil.cpp`,
`SoilByWater`, TF24.dGVycmE at 40 030 km round). Not a bug, a cost: it's half the full tier's CPU.

- `World::ground` at that size is 30–110 ms per read, and **Streams::read is all of it**
  (timers round ground's phases: ground 654 s, streams 653 s, column 0.0 s, the rivers' loop 0.3 s,
  over 17 000 reads on 8 threads).
- The cost isn't per read but per new place: a cluster of reads round a river (17 per transect,
  five transects a width apart) costs ~1–2.5 CPU-s for the first and milliseconds for the rest.
  Spreading the same reads over 600 places instead of 200 took 654 → 727 CPU-s; cutting the reads
  per place from 41 to 17 barely moved it. So it's the lattice nodes of every level (12 levels,
  ~90 km cells down to 200 m) being grown for each new region (`streams_grow.cpp`, kept per shard).
- The old version of the test (100 windows of 20 × 20 km at 1 km steps) was the same: 488 CPU-s.

Reproduce:

    bash apps/game/scripts/build.sh --target terragen_tests -j
    time .output/tests/terragen_tests "--source-file=*soil.cpp" "--test-suite=slow"
    # ~500 CPU-s, ~90 s wall on 8 cores

Why it matters beyond the test: anything reading scattered columns on a big planet (the far view's
coarse tiles, a teleport, the soil and ranges tests) pays this per place. `bun perf:gen` at 50k/100k
may not show it, since the lattice's coarse levels are what's dear at Earth's scale.

Not asked: changing the test's planet size (it tests Earth's scale on purpose) or its sampling,
which is already one place per river.

## Notes (2026-11-25, testing owner)

Still the full tier's critical path: idle, `bun run test` is 133 s wall, and soil.slow alone is
88 s wall (638 CPU-s); the first read of each of its 200 places is 400 CPU-s of that. It's not only
the slow test: **TestBodies.testABodysLandIsItsKind is 31 of TestBodies' 32 s**, all in 400 scattered
`ground(at, 0)` reads on the home body at 628 320 round (land-sized check, `test_bodies.cpp:307`),
~75 ms each, single-threaded; the other eight bodies take milliseconds. The likely cost inside is
`Streams::area` (`streams_grow.cpp:273`): each new node sums its whole upstream basin at every
level, recursively, before its cache is warm. Cutting the cold cost would take the full tier under
120 s (the testing owner's milestone 3) without changing any test.
