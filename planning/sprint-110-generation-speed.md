# Sprint 110: where generation's time goes, and getting it back

Making a 100k world took ~30 s from New World to a planet on screen, and the
create screen's bar sat at 0 for most of it. First: see where the time goes,
by step, for the UI and for perf work alike; then take back what doesn't
depend on the river work still to come.

## Built

- **Steps** (`terragen/progress.h`): each slow piece of work is a named,
  timed, counted `Step` recorded against the world code; whoever's waiting
  reads `making(code)`. The engine logs a world's breakdown when it's made
  (`terragen: <code> made in …`) and when its coarse planet is there (`far:`).
- **`bun perf:gen`**: 50k and 100k, cold and warm, each run a line in
  `.output/perf/gen.jsonl`, shown beside the last like it.
- **What it found** (TF20.Y3gyMzlqNHY at 100k, cold): 12.5 s, of it the river
  levels (`World::profileRuns`) 11.3; plates 0.55, crust and climate 0.36,
  erosion 0.09. Opening a world: its system's layout 0.72 s, its coarse
  planet's 242 tiles 6.8 s, every time.
- **Far tiles kept** (`far/tilecache.h`): by the sources a tile's built from
  (`FAR_SOURCES`), the horizon and the body's place; a kept planet 6.8 s →
  5 ms, and the world model isn't made till a tile's missing. 821 KiB a
  world's coarse planet; every world's under 512 MiB, least recently used
  swept first.
- **Atlas layouts kept**: each body's regions' extents (≈250 000 of the
  projection's probes a body) by the layout's code, in the process and on
  disk: 0.72 s → 0.15 ms.
- **River levels kept by terragen's sources** (`TERRAGEN_SOURCES`), not the
  engine's build: a rebuild that doesn't touch terragen keeps them.
- **River levels read in place order** (a Morton curve through the run
  starts): each thread's channel nodes reused, 9.0 → 5.8 s. **Channel caches
  two-way** (`Ways`), the same memory: → 4.7 s. Twice the slots gave 5.1 s,
  four times 3.9 s at ~60 MB a thread: not kept. Nothing generated changed
  (the golden holds).
- The create screen already starts the world as its code settles (200 ms):
  nothing to add.

## Next

- A `loading` message and a stage list, so the create screen's bar moves
  from the first second; the same for entering a world.
- Instrument entering a world: spawn generation, meshing, media.
- Once rivers settle: coarser reads for the river levels, levels only for
  the basins needed.
