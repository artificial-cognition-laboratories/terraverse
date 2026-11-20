# Sprint 1 — terragen: the world is real

Goal: everything the create menu shows becomes the world you walk in. The
engine generates terrain, climate and biomes from the world code, and the
menu's preview comes from the same code in the engine (the JS mock retires to
the gallery). Phases 0 + 1 of AGENTS.md "Generation".

## Decisions taken for this sprint

1. **terragen is a pure, deterministic function** of the world code:
   `(code, x, z) → elevation, climate, biome`. It has no engine dependencies,
   so the generator, the preview and unit tests all call the same code.
2. **Fixed-point maths throughout** (Q16.16 in integers): hashed value noise,
   CORDIC sin/cos/atan, integer sqrt. No libm on any path that decides a block.
   Same code → same world on every machine.
3. **No coarse simulation grid yet.** Phase 1 has nothing that needs one: every
   field is a pure function evaluated per column. The grid arrives with Phase 2
   (water), which needs neighbourhoods (flow, erosion). Building it now would be
   an addition without a reason.
4. **Noise is periodic east–west by construction**: lattice indices wrap modulo
   an integer period, so no trigonometry is needed for the wrap. North–south it
   is not periodic — the world ends in polar edges.
5. **Coordinates:** the world is centred on the origin. `u = (x + size/2) / size`
   wraps; `v = (size/2 − z) / size` runs north (+z, v = 0) to south. Outside
   `|z| < size/2` is the polar edge: an ice wall to the build limit. East–west
   past the edge the terrain repeats exactly (true seamless wrap is later
   engine work).
6. **The generator is `mg_name = terra`**, a `MapgenBasic` subclass, so caves,
   ores, decorations, dust, liquids and lighting come from the engine as now.
   It reads `mgterra_code` from the world's map settings (`map_meta.txt`),
   written by `worlds.create` along with `mg_name = terra` and the engine seed.
7. **Biomes: terragen decides, Lua describes.** terragen classifies each column
   into one of a fixed list of biome names; the generator looks up the
   Lua-registered biome of that name and paints its surface, filling the
   engine's biome map so decorations (trees) and dust follow our climate.
   Every class must have a registered biome — a missing one is an error.
8. **Mapgen climate is the annual mean** (averaged over the year in fixed
   point); the preview takes a date and shows that day's climate. Seasons on
   the live world (snow coming and going) are a later sprint.
9. **The preview intent** `worldgen.preview { code, date, resolution }` returns
   quantized fields as base64 binary (int16 elevation, int8 heat and moisture,
   u8 biome) plus the biome name list, so the page needs no copy of the
   classifier. Registered by the main menu.
10. **The map seed** written to `map_meta.txt` is terragen's hash of the code's
    seed, so the engine's own randomness (ores, decorations) is tied to it.

## Work

| # | Piece | Where |
|---|---|---|
| 1 | Fixed point, CORDIC, isqrt | `src/terragen/fixed.*` |
| 2 | World code decode (TF1), fields, defaults | `src/terragen/code.*` |
| 3 | Periodic value noise + fBm | `src/terragen/noise.*` |
| 4 | Solar model (declination, insolation, annual mean) | `src/terragen/sun.*` |
| 5 | The model: elevation, heat, moisture, biome per column | `src/terragen/world.*` |
| 6 | Preview fields | `src/terragen/preview.*` |
| 7 | Unit tests: decode goldens, periodicity, determinism golden, sun sanity | `src/unittest/test_terragen.cpp` |
| 8 | `MapgenTerra` + registration | `src/mapgen/mapgen_terra.*`, `mapgen.cpp/h` |
| 9 | `worlds.create` writes map settings; `worldgen.preview` verb | `src/shell/worlds.cpp`, `menu.cpp` |
| 10 | Page: preview from the engine; mock moves to the gallery | `ui/src/worldgen`, `ui/src/gallery` |
| 11 | Lua: a biome (and nodes) for every terragen class | `games/terrafirma/mods/tf_world` |
| 12 | Headless generation check (world mod emerges and reports) | test procedure below |

## Verification

- `luanti --run-unittests` passes (terragen tests included).
- Headless: create a world with `mg_name = terra`, a worldmod emerges an area,
  logs surface heights and biome names along a line, and shuts the server down.
  Heights and biomes must match the preview fields at the same points.
- UI typechecks and builds; the gallery still renders (mock engine).
- Not run by agents: the game client (the user runs it).

## Out of scope (next sprints)

Water (coarse grid, rivers, erosion), sky and seasons on the live world, the
spawn picked on the map (`static_spawnpoint`), seamless east–west wrap in the
engine, geology and caves beyond the engine's defaults.

---

## Outcome (2026-10-22)

All twelve pieces landed. Verified:

- `TestTerragen` (8 tests) passes: code decoding against codes the page
  produced, fixed-point trig, an exact east–west seam, solar sanity (polar
  night, midnight sun), coordinate mapping, and the determinism golden
  (`990153078` for `TF1.YWJj.AQC2ZoCAgICAgICA41U` at 48²).
- The whole engine suite (`--run-unittests`) still passes.
- `bun check:terragen`: a `terra` world generated headless agrees with
  standalone terragen on **25/25** probed columns — every surface height and
  every top block. (The standalone build uses different compiler flags from
  the engine and produces identical numbers: a small determinism check.)
- The preview wire format round-trips: terragen's fields encoded with the
  engine's `base64_encode` decode correctly with the page's `decodePreview`.
- UI typechecks and builds; the gallery answers `worldgen.preview` from the
  mock.

Not verified here: the game client (the user runs it) — the CEF hop of
`worldgen.preview`, and walking the generated world.

Deviations from the plan: none of substance. The duplicated "decode a code or
refuse" logic was made one shared function (`shell::readWorldCode`).

Found along the way:

- **Land share runs below the slider** (≈20% land at "40%"), inherited from
  the mock's formula. Recalibrating changes every world, so it should happen
  once, deliberately, with a code version bump — before codes are shared.
- `core.get_biome_data()` in Lua still answers from Luanti's own biome noise,
  not terragen. Nothing uses it yet; fix when a system needs biome-at-point.
- Caves and dungeons are off in `terra` until the geology phase.
- Old worlds (`awd`, etc.) keep their original generator; only new worlds are
  terra.
