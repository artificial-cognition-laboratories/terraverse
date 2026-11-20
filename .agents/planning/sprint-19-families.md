# Sprint 19 — Material families: blocks as compositions

Goal: blocks are what they're made of, in gradients, not a catalogue. A
block is a **family** (its structure: loose grains, solid rock), plus a
**composition** (a point in that family's space, in the node's `param2`),
plus its state and temperature. It's coloured from a palette by its
composition, and the world's physics reads its matter from it.

## Landed

- **Small fixes first:**
  - no "fly mode" status text;
  - the inventory shows moves at once (a prediction, replaced by the
    server's answer), so it no longer flickers;
  - Esc and E are caught in the capture phase;
  - heat-made liquids **pour** in the world's matter, falling into space
    below and spilling from an overfull block (molten zinc grows ~9%), while
    Luanti's own liquids keep Luanti's flow;
  - soft, small, fading flame, smoke, steam and spark particles.
- **Soil** (`src/terragen/soil.cpp`, `tf_world/soil.lua`):
  - **The composition space:** sand and clay in tenths on the texture
    triangle (silt is the rest), at three organic levels (mineral, topsoil,
    peat): 198 points, defined once in C++ (`soilPoint` / `soilIndex`) and
    served to Lua (`core.terra.soil_points`).
  - **terragen chooses each column's soil** from the parent rock (by
    province), weathering (warm and wet makes clay, dry keeps sand),
    floodplains (silt and clay by rivers and lakes), slope (steep keeps the
    coarse part), and cold wet ground keeping peat.
  - **Its matter:** quartz for the sand and silt, kaolinite for the clay,
    organic matter as cellulose. It's 55% solid, the rest air between grains.
  - **Named by the USDA texture classes** ("Silty clay loam (sand 10% · silt
    60% · clay 30%)"), with "Peaty" when organic matter is high.
  - Biomes fill with soil; the grass stays on top.
- **Rocks as blends:** each of the nine textured rocks blends toward a
  geological neighbour (granite → gneiss → slate → shale → limestone →
  sandstone; basalt ↔ andesite ↔ tuff; obsidian → basalt) over 16 steps in
  `param2`. The step comes from smooth 3D noise (`World::blend`), and the
  matter is both rocks in proportion. The pure point is still the form
  (lava cools into basalt).
- **Palettes** (`scripts/assets/palettes.py`, using terragen's points
  through `soil_points.cpp`): soil coloured by composition (sand pale,
  silt grey-brown, clay reddish, organic darkening toward peat); rocks
  tinted toward their partner.
- **The world's matter:**
  - a kind can be a mix of substances and rocks;
  - `core.matter.register_family` registers one kind per point, and the
    ground reads and writes `param2`;
  - dug blocks keep their composition (Luanti's `palette_index`) and are
    named for it;
  - the probe shows the composition's name.
- **The generator** (with the far-view session's `terra::Columns`, which
  the far view also reads):
  - each column's `soil_point` is decided in `Columns::at`;
  - rock gets its blend as `param2`.

## Verified

- **Engine tests pass**, including the new soil test:
  - every point round-trips;
  - clay is 0.59 where warm and wet, against 0.09 where dry;
  - organic matter is 0.27 in cold, wet ground, against 0.04 where warm;
  - sand is 0.75 on steep ground, against 0.35 on flat ground.
- **Sandbox tests:** pouring tin runs down.
- **`bun check:matter`** also heats a peat block, which is mostly quartz,
  and burns its organic part.
- **The UI** typechecks and builds.

Not run: the client.

## Notes

- **No world-code bump.** terragen's terrain didn't change; only the blocks
  laid in it did (soil and rock compositions). Chunks generated before this
  in an existing world keep their dirt.
- **Still to come:**
  - **More families:** plant matter, metal (alloys as element ratios),
    ceramic/glass, ice/water.
  - **Separation processes:** sieving, washing, panning.
  - **Mixing to new points.**
  - **The wiki's view of the families.**
