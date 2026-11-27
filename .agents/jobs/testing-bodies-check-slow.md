# check:bodies went 5 s → 65 s, mostly idle

Found 2026-11-27 by the testing owner timing the probe checks on clean master (23818a1b2):
`BUILT=1 bash apps/game/tests/checks/bodies.sh` passes but takes 65 s wall at 8% CPU
(2026-11-26 at c052913a1: 5 s). Something waits rather than works: likely the probe finding
the moon's ground (8be45a747, "find the equator's coast and the moon's ground rather than
assume them") polling for emerged blocks on a timer, or a fixed sleep.

Reproduce: build, then `time BUILT=1 bash apps/game/tests/checks/bodies.sh`. It is now the
critical path of `bun run test:checks` (with window, 66 s).
