# Sprint 20 — The horizon

Goal: from anywhere, you see to the horizon. Standing on a beach, the sea
meets the sky a few kilometres out and distant peaks show only their tops.
On a summit you see further around the curve. The world stays a flat,
wrapping voxel map; the planet's roundness is drawn, not stored.

## Decisions

1. **The far world is a function.** terragen is a pure function of the
   world code, so the client builds distant terrain itself
   (`World::ground`, `World::cover`, the cached coarse grid). The server
   sends no far data this sprint; player edits beyond the voxel range
   arrive in a later sprint as summaries.
2. **Two layers.**
   - **Near:** real map blocks, as now, out to `viewing_range`.
   - **Far:** a heightfield **clipmap** around the camera. Rings of a fixed
     vertex grid, each with twice the cell size of the one inside it
     (1, 2, 4 … 64+ blocks). Rings slide as the camera moves and are
     rebuilt on a worker thread, a ring at a time.
   - The innermost far ring is **blocky** (cell 1, stepped heights), so the
     join with real blocks doesn't read as a smooth mesh. Outer rings are
     smooth.
   - Far terrain is drawn first. Where real map blocks are loaded, it is
     discarded (a per-column mask of loaded blocks), so the near layer
     always wins.
   - **Water** is a separate sheet at sea and lake level, so the ocean
     horizon is one clean line.
3. **Curvature at render time.** Every vertex, on both layers, on objects
   and on clouds, drops by `d² / 2R`, where `d` is its horizontal distance
   from the camera. It's one uniform (`horizonRadius`) set beside
   `cameraPosition` in `GameGlobalShaderUniformSetter` and applied in
   `nodes_shader`, `object_shader`, `cloud_shader` and the new
   `far_shader`. The near field barely moves (0.1 block at 100 blocks for
   R = 50 km).
4. **R is a property of the body, not of the wrap.** The literal radius of
   a 16k world (≈2.5k blocks) puts the horizon 93 blocks away. A body card
   gets `horizon_radius`, which the server sends with the sky. Earth is
   tuned so a beach shows ~3–5 km and a peak 30–60 km; the Moon is smaller,
   so its horizon feels close.
5. **Range comes from the physics.** The far layer reaches
   `√(2Rh) + √(2R·h_peak)`: the horizon for the eye's height, plus the
   tallest terrain that can peek over it. Capped by `far_view_max`.
6. **Wrap and poles.** Sampling wraps x, so looking east across the seam
   is seamless (the noise is periodic). The polar edge is drawn as the ice
   wall it is.
7. **Settings** (Settings ▸ Horizon, whitelisted in `src/shell/options.cpp`):
   - `far_view` (on): distant terrain;
   - `far_view_detail` (1–4, default 2): ring vertex density, and so the
     ring count;
   - `far_view_max` (2–64 km, default 16): the furthest it ever draws.
   Changing one rebuilds the far view live.

## Where it lives

- `src/far/` (ours): `clipmap.*` (rings, sliding, rebuild queue),
  `sample.*` (terragen → height and colour per cell, thread-safe),
  `mask.*` (which columns real blocks cover), `far.*` (owns it, draws it).
- `client/shaders/far_shader/`: the heightfield, colour by biome, rock,
  snow and water, with the same fog and curvature as nodes.
- Upstream hooks under `#if USE_SHELL`: the uniform setter and draw call
  in `game.cpp`, and the curvature line in the three shaders.
- The client needs the world code: it's sent with the sky (`set_sky`'s
  solar field grows `code` and `horizon_radius`). That's a protocol bump.

## Steps

1. **Curvature alone** on today's view: the uniform, the shaders and R from
   the body card. This is where we judge the feel.
2. **Far clipmap:** height and colour from terragen, the water sheet, the
   loaded-block mask, the settings wired.
3. **Fog to the horizon:** fog distance follows the far range instead of
   `viewing_range`. Proper aerial perspective from the air model is the
   next sprint.

## Checking

- Unit: sampling a column on the client matches `World::ground` exactly,
  and the clipmap's ring cells line up across levels.
- `bun check:terragen` is unchanged: this sprint doesn't touch generation.
- By eye (the user runs it): a beach at sea level, a summit, the east–west
  seam, a pole.

## Outcome

Built: steps 1–3, as one piece.

- `TOCLIENT_HORIZON` (protocol 56), sent on join to a terra world: the
  code, the water level, and each biome's top, water-top, water and river
  blocks plus stone, ice and lava, as content ids. The client colours them
  from each block's top texture, so the horizon is painted with the game's
  own blocks: no colour table.
- `src/far/terrain.*` (pure): a column as the far view sees it (top face,
  water, cliffs show rock, forests stand ~5 blocks high and darker, deep
  water darker, the polar wall), and a clipmap level built from it. Levels
  morph into the next coarser one over their outer band, using the same
  quad split, and keep a two-sample border so normals match too: levels
  meet exactly, without cracks or shading seams (unit-tested). A moved
  level reuses its overlapping samples.
- `src/far/view.*`: the scene node and its worker thread (innermost level
  first; a request replaces the pending one for that level). Drawn in the
  sky pass with its own near/far planes, then depth is cleared. A finer
  level's square is cut out of the next coarser level by a uniform, and
  everything inside the near world's range is discarded.
- `far_shader`: the curve, one-light shading, water that takes the sky's
  colour at a grazing view, and fog to the horizon.
- The fog moves out to the far view's range while it shows.
- Settings ▸ Horizon wired, and rebuilt live when changed.

Changed from the plan:

- **R comes from the code, not the packet:** `far::radius` =
  24 × world size. It's derived where it's used, from the one input the
  client already has. Bodies other than Earth will scale it by their radius.
- **The near world isn't curved.** The far view's drop is counted from the
  cut (`(d² − cut²) / 2R`), so it meets the flat near world without a step.
  At the near world's range the true drop is well under a block. That's
  invisible, and it keeps the stock shaders untouched. The horizon distance
  moves by under 2%.
- **No blocky innermost ring.** The innermost level starts at 2–8 blocks
  per cell, and the overlap band with real blocks reads fine smooth.
- **Range is a cap, not computed from eye height:** the smaller of
  `far_view_max` and half the world (beyond that east–west is the same land
  again). The curve itself hides what's past the horizon.
- **The polar wall** is drawn 512 blocks high, not to the build limit.

Not yet checked: everything that needs a GPU and a client. The shader
compiling on each driver, the look, and frame cost are for the user to run.
Known: volumetric light treats far terrain as sky (depth is cleared after
it). Snow from the weather isn't in the far colour yet.

## Second pass: the far world is blocks

The first pass painted a heightfield, and it looked painted: average
colours on smooth hills, a ring of sky at the seam, forests the wrong
colour. The second pass replaces it with the world itself, at a lower
resolution.

- **One rule for every block.** `terra::Columns` (src/mapgen/terra_column.*)
  decides a column once (ground, soil, water, ice), and
  `Columns::node(column, x, y, z)` is the block there. The generator fills
  every chunk from it, and the far view reads it. It was proven
  block- and param2-identical to the old fill-then-paint passes by hashing
  generated areas, A/B on one binary.
- **Trees are the world's trees.** Species are data in the engine
  (`core.terra.register_species`); `core.terra.species(pos, ground)`
  chooses, and `tf_world/vegetation.lua` plants with it (proven identical
  by the same hash, with a negative control). The far view chooses with
  the same function and grows each tree again with treegen, seeded by its
  position as the world's are, clipped to its chunk. Coarser levels stand
  each species' measured crown wherever the forest is dense enough.
- **TOCLIENT_HORIZON** carries the generator's whole block table and the
  resolved species (thresholds as exact bits): the client builds with the
  server's data.
- **Tiles of cells** (src/far/terrain.*): each cell is the ground block and
  height, the soil bands and rock a cliff shows, any liquid, and the tree
  runs. Level k cells are 2ᵏ blocks wide.
- **Meshed as blocks** (src/far/mesh.*): box faces in each block's own
  colours (the average of each face's texture, as the GPU's smallest
  mipmap shows it; tinted by palette and param2 as the near mesher tints
  it) and shaded per face as the near mesh is. Water is seen through as
  the near world's is.
- **A quadtree of tiles** (src/far/view.*): finer near the near world's
  edge, coarser with distance, built nearest first on worker threads. A
  tile draws until all of what replaces it is ready, so there are no holes.
- **The seam is the near world's own edge.** A mask of the map-block
  columns the near world draws this frame (`ClientMap::forEachDrawn`) is
  read by the far shader: it draws exactly where they aren't.
- **Lit and fogged as the near world is** (the node shader's daylight and
  fog), plus haze that builds from the seam outward.
- **R = 8 × world size**, so the horizon falls inside the drawn range.
- `core.terra.surface(pos)`: a column as the generator makes it, without
  generating it. `bun check:far` compares it with generated chunks.

## Later

Edit summaries from the server (built-up places show on the horizon);
snow cover from `tf_weather` in the far colour; far terrain written to the
depth the post-processing reads;
aerial perspective from the air model; `World::cover` canopy and snow in
the far colour; and the same far layer seen from orbit.
