# Sprint 2 — water: rivers, lakes and carved land

Goal: rain falls on the land, runs downhill, gathers into rivers that reach
the sea, pools into lakes where the land holds it, and carves the valleys it
runs through. The preview shows the rivers and lakes; the generated world has
them, contained in their banks. Phase 2 of AGENTS.md "Generation".

## Decisions

1. **A coarse grid of the whole world**, 256 × 256 cells whatever the size
   (8k world → 32-block cells). Water needs neighbourhoods — where does this
   cell drain, how much land drains through it — so this is where the grid
   finally earns its place. One constant decides the resolution.
2. **Computed once per code, shared.** A thread-safe cache keyed by the code
   string holds the simulated grid; the map generator's threads and the
   preview reuse it. Building it is pure and deterministic (fixed point,
   ties broken by cell index).
3. **The simulation, in order:**
   1. base elevation per cell from the existing terrain model;
   2. **sea level by quantile**: shift elevation so the share of land cells is
      exactly the Land setting (fixes sprint 1's ≈20%-at-"40%");
   3. **priority-flood** depression filling (Barnes et al.) from the sea —
      every land cell then drains to the ocean; filled basins are lakes;
   4. **stream-power erosion**, the implicit Braun–Willett scheme (stable, one
      pass per iteration), a few iterations, sea fixed;
   5. fill again, then **flow** (steepest descent, wrapping east–west; polar
      rows are walls) and **accumulation** weighted by rainfall (the moisture
      field);
   6. **rivers**: cells whose drained area passes a threshold set by the
      Rivers slider; **lakes**: cells the fill raised by at least a block.
4. **Blocks from the grid:** a column's height is the existing fine terrain
   plus the grid's erosion change at that point (bilinear), so carving shows
   without losing detail. **Rivers** are drawn as segments from each river
   cell to the cell it drains into, width growing with drained area: a channel
   below the water, and banks that rise from it, so water stays put. **Lakes**
   use the lake mask (bilinear): inside, the floor drops below the lake level
   and water fills to it; on the rim, the ground is raised to the level.
5. **Code version TF2.** Land calibration and water change every world. TF1
   codes and worlds are refused with a clear message — pre-release, nothing is
   shared yet; no TF1 generator is kept.
6. **Preview** gains a `water` field: 0 dry, 1 lake, 2..255 river strength.
   The map and globe draw it. The Rivers slider now shows in the preview.

## Work

| # | Piece | Where |
|---|---|---|
| 1 | Grid: base, quantile sea level, fill, erosion, flow, accumulation, rivers, lakes | `src/terragen/grid.*` |
| 2 | Shared cache by code | `src/terragen/grid.*` |
| 3 | World: `ground(x, z)` — final height, water surface, kind; sea offset in `column` | `src/terragen/world.*` |
| 4 | TF2 decode; old versions refused | `src/terragen/code.cpp`, `ui/src/worldgen/config.ts` |
| 5 | Preview `water` field | `src/terragen/preview.*`, `src/shell/menu.cpp`, `ui/src/worldgen/fields.ts` |
| 6 | Map generator: carve, fill rivers and lakes, riverbeds | `src/mapgen/mapgen_terra.cpp` |
| 7 | Map and globe draw water; Rivers no longer "not in preview" | `ui/src` |
| 8 | Tests: TF2 goldens, land share exact, every land cell drains to the sea, rivers flow downhill, new determinism golden | `src/unittest/test_terragen.cpp` |
| 9 | Visual tuning: render maps from standalone terragen and look at them | `scripts/terragen-check/` |
| 10 | End-to-end check extended: river and lake columns hold water | `scripts/terragen-check.sh` |

## Verification

Unit tests; the whole engine suite; `bun check:terragen` (heights, water,
top blocks match standalone terragen); maps rendered and inspected; UI
typecheck/build. Not run: the client.

## Out of scope

Small streams below grid resolution, waterfalls as features, deltas, aquifers,
seasonal water (frozen rivers), sediment deposition, an Erosion slider.

---

## Outcome (2026-10-22)

All ten pieces landed, plus async verbs (below). Verified:

- `TestTerragen`: 12 tests pass — decoding (TF2; TF1 refused), trig, seam,
  sun, coordinates, **land share exact**, **every land cell drains to the sea**,
  **water always flows strictly downhill**, **rivers only on land**, and the
  new determinism golden (`3256529572`). The whole engine suite passes.
- `bun check:terragen`: **40/40** probed columns agree with standalone
  terragen — 8 dry, 6 lake, 9 river, 17 sea — on ground height, top block
  (riverbeds included) and water surface height.
- The page's own decoder and painter render terragen's encoded preview with
  rivers and lakes (checked as an image).
- Maps rendered for the default world and every preset and inspected while
  tuning (erosion strength, lake rules, plains, flat-crossing rivers).
- Land share: default 40.0% for 40%; Pangaea 65.1%; Archipelago 25.1%;
  Highlands 60.0%; Waterworld 12.2%.
- Grid build ≈ 140 ms; a full preview ≈ 150 ms when the land changes, ~15 ms
  when only the date does.

Not verified here: the client — the preview through Chromium (now async), and
walking rivers and lakes in game (do they look right, does water stay in its
banks after the engine's liquid updates).

What tuning found and fixed:

- Fixed river thresholds don't fit every world → the Rivers setting now picks
  a *share* of the land.
- Erosion was wearing coastal land below the sea → floored at sea level.
- One-unit slopes vanished when divided by √2 → slopes compared by
  multiplication; land sinks went from 696 to 0.
- Ridge noise made a pit in every mountain → basins smaller than 10 cells or
  shallower than 6 blocks fill with sediment; basins over 3% of the land too.
- Supercontinent interiors climbed into vast plateaus → plains level off
  inland; height comes from mountain ranges.
- Rivers ran ruler-straight across filled flats → the fill step is jittered.

Added beyond the plan: **async bridge verbs**. The preview takes ~150 ms when
the land changes; on the main thread that would stall the menu while a slider
moves. Slow, pure verbs now run on the bridge's worker thread.

Known items: very large enclosed basins become high sediment plains; no
streams below grid resolution; the JS gallery mock shows no water.
