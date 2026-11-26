# Three atlas checks are red on master: no sea at sea level, land on the moon

Found by the testing owner on 2026-11-26, timing every check:* on a clean build of master
(aeaecaaff, in a separate worktree: the shared tree had uncommitted sidecar work in it).

- `bun check:atlas-world`: `ATLAS equator solid 0`, `air 11776`, no liquid at all at the
  equator's sea level → `ATLAS pass 154633 fail 1` ("no solid at the equator's sea level").
- `bun check:atlas-seams`: 3 courses fail with "past the seam is air with no water in it".
- `bun check:bodies`: "BODIES FAILED: land on the moon".

Not caused by aeaecaaff (`perf`): with it reverted, all three still fail. No check:* runs in
the gate or CI, so it's unknown when they broke; bisect from 2c0061283 (2026-11-20, the
re-sort) forward. All three read sea and land from the generator, so start in
`library/world/terragen` and `src/server/world/generate`.

Reproduce: `cd apps/game && bun check:atlas-world` (4 s after the build).

Also seen: `check:interact` failed once in 16 runs, under load, on "three drops of dirt lying
together become one of 6 (found 2)": drop merging looks timing-dependent.
