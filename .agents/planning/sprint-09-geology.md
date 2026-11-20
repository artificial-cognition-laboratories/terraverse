# Sprint 9 — Geology: plates, volcanoes, rock, ore and caves

Goal: the ground under you has a history. Plates drift and collide, and
mountains rise where they meet; volcanoes stand on arcs, rifts and hot spots
(islands where they break the sea); every column has rock in layers that
match where it is (granite shields, folded mountain cores, sedimentary
basins, basalt sea floor and lava fields); ores sit where real processes put
them; caves follow the rock (karst in limestone, lava tubes in basalt, deep
caverns with underground lakes). All deterministic, all from the world code.
Phase 5 of "Generation" in AGENTS.md, and the geology step of "Matter: one
chain".

## Decisions

1. **Plates on the coarse grid.** Seeded plate centres (more with more
   continents), each with a drift; every cell joins its nearest plate
   (wrapping east–west). Boundaries are classified by how the plates move
   across them: **convergent** (mountains; arcs of volcanoes on the
   overriding side; trenches at sea), **divergent** (rifts on land, ridges
   under the sea), **transform**. Each cell knows its plate, its boundary
   kind and its distance to the nearest boundary.
2. **Mountains follow the plates.** The ridge noise is weighted by a
   mountain-belt field (strong along convergent boundaries, low in plate
   interiors), so ranges run along plate edges with noise for detail. Land
   share stays exact (the sea is still placed by quantile).
3. **Volcanoes as landforms.** Seeded along convergent arcs, divergent rifts
   and hot spots; each is a cone with a crater, active ones (by the planet's
   heat) holding lava. Ocean hot spots that clear the sea are islands.
4. **Strata.** Each column gets a **province** (shield, orogen, volcanic,
   basin, ocean floor) and a stack of rock below the soil: crystalline
   basement (granite, gneiss), metamorphic cores (slate, gneiss), volcanic
   rock (basalt, andesite, tuff, obsidian near craters), and sedimentary
   beds (sandstone, shale, limestone) whose boundaries wobble and fold, more
   strongly near mountains.
5. **Ores by process**, decided per block by terragen: hydrothermal veins of
   copper, tin, lead and gold around granite in orogens and arcs; banded iron
   in old shield sediments (where life made oxygen); magnetite in basalt;
   coal seams in humid basins; salt beds in arid basins; gold and tin
   placers in river gravel downstream of mountains; clay along rivers and
   lakes; sulfur at active craters.
6. **Caves by rock.** Karst networks dense in limestone, lava tubes in
   basalt near volcanoes, rare deep caverns with underground lakes (lava
   lakes in volcanic provinces). Caves keep their distance from rivers, lakes
   and sea. The world code's Caves setting scales them.
7. **3D fixed-point noise** in terragen for veins, folds and caves, so caves
   and ores are deterministic like the terrain.
8. **TF3.** Mountains move, so codes move: TF2 worlds are refused with a
   clear message.
9. **Content:** rock, ore, soil, lava and volcanic blocks in `tf_world`,
   textured from the Good Vibes pack (CC BY 4.0, credited), with rocks and
   ores it lacks derived by recolouring its art (noted in the credits). The
   matter library gains the minerals, and rocks become named mixtures of
   them (`matter/rocks.json`).
10. **Seeing it:** `core.terra.geology(pos)`; preview overlays for plates
    and surface geology on the create screen's globe and map; the interior
    cross-section for every body in the viewer.

## Work

| # | Piece | Where |
|---|---|---|
| 1 | 3D fixed-point noise | `src/terragen/noise.*` |
| 2 | Plates and boundaries; mountain belts; volcano list | `src/terragen/geology.*`, `grid.*` |
| 3 | Elevation from belts; volcano cones and craters; lava | `src/terragen/world.*` |
| 4 | Provinces, strata, ores, caves per column and block | `src/terragen/geology.*` |
| 5 | TF3 | `code.cpp`, `ui/src/worldgen/config.ts` |
| 6 | Map generator: strata, ores, caves, lava | `src/mapgen/mapgen_terra.*` |
| 7 | Blocks and textures | `games/terrafirma/mods/tf_world`, `scripts/assets/` |
| 8 | Minerals, rocks as mixtures, ore reactions | `matter/` |
| 9 | `core.terra.geology` | `src/script/lua_api/l_terra.*` |
| 10 | Preview overlays; interior view | `terragen/preview.*`, `ui/src/worldgen`, `ui/src/system` |
| 11 | Tests and the end-to-end check (rock and ore below the surface) | `src/unittest`, `scripts/terragen-check*` |

---

## Outcome (2026-10-23)

Landed: plates, belts, rifts, ridges and trenches; volcanoes (arcs, rifts,
hot-spot chains, islands, level lava craters); provinces including inland
basins; strata with folds; ores by process (copper, tin, lead, gold, banded
iron, magnetite, coal, salt, sulfur, placer gravel); caves (karst, lava tubes,
rare flooded or lava-filled caverns); TF3; rock, ore and lava blocks (Good
Vibes, derived textures via `scripts/assets/goodvibes.py`); 14 minerals, rocks
as mixtures (`matter/rocks.json`), 8 more reactions including smelting every
placed ore and firing clay; `core.terra.geology`; plates and geology overlays;
the interior cut-away (body layers from the model).

Verified: 390 engine tests pass (new: plates, volcanoes, strata, ores and
caves, body layers, rocks and ores smelting; TF3 golden `1213676375`).
`bun check:terragen` 42/42 on terrain, water, lava and the block 20 below the
surface, and 42/42 on `core.terra` (geology included). `bun check:lab` passes.
UI typechecks and builds.

Not run: the client. Not done from the night's list: vegetation, a wider
matter library (alloys), weather. AGENTS.md not yet updated for this sprint.
