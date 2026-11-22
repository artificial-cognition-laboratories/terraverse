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
