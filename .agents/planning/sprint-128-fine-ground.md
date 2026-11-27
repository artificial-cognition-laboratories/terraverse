# Sprint 128: fine ground

The world is sixteenth voxels, but terragen still decides its land a whole block at a time: every
height is floored to a block (`World::surface`, `World::terrain`), and `ground::Ground` puts the
slope back by averaging the integer tops of the four columns at each corner. So a one-block step is
a ramp, two is a cliff, and a gentle slope is flats joined by ramp bands. This sprint draws natural
ground as if the blocks didn't matter: terragen's own height, to a sixteenth.

## Decided (2026-11-27)

1. **terragen gives height in sixteenths.** `Ground` gains `fine` (sixteenths of the world, the
   value before flooring); `terrain` stays and is always `floorDiv(fine, 16)`, so everything
   block-level (strata, soil depth, caves, flora, the far view's columns) is unchanged in shape.
2. **Corners from the columns' fine heights.** A corner's height is the average of the fine
   heights of the four columns there within its cluster, as today, but of fine heights, not integer
   tops. Same rule, same agreement between blocks, no extra sampling. (Sampling terragen at the
   corner itself is truer but doubles the surface cost; only if the average shows artefacts.)
3. **Written once, at generation.** The generator stores the surface block's shape (the existing
   `shape::Shape` cut, a block's first cut keeps it), so the mesher, the server's collision and the
   machines' ground reads read one stored shape and never need terragen. Dug or built ground keeps
   today's rule (`ground::corner` over integer tops).
4. **Loose ground only.** Rock keeps its cubes this sprint; smooth rock is its own question
   (cliffs should read as cliffs).
5. **A world code changes meaning:** a generator version bump and new goldens.

## Gen time

Step 1 is free: the same arithmetic, rounded later. Step 2 adds no terragen calls (the corner
average reads the columns already sampled for the chunk, plus a one-column ring round it, which
`landAt` already caches). The new cost is building and storing a shape per surface block, and the
storage. Measure with `bun perf:gen` before and after, and say the number.

## Phases

1. **`fine` through terragen.** Carry the unfloored height through `surface` and `terrain`, then
   through every edit of `out.terrain` in `World::ground` (craters, mare, volcanoes, cliffs by
   hardness, deltas, lakes, rivers, walls): each works in sixteenths, or adds whole blocks to
   `fine` and leaves its fraction alone. Water levels stay whole blocks except a river's top
   (`water_part`, already sixteenths). Golden bumped; `bun check:land`. Render a slope with the
   tool, before and after, to see it.
2. **The shape at generation.** `terra::Columns` gets a column's fine top; `MapgenTerra` writes the
   surface block's cut from the corner averages of fine heights; `ground::Ground` reads a stored
   cut first. The far view's land uses the same fine height so near and far meet.
3. **Checks.** `bun check:land`, `check:far`, `check:moves` (walking, vehicles on slopes),
   `check:wiki` (worlds scenes), a headless boot, and the game: a slope, a river bank, a beach.

## Open

- What it costs to store: every surface block becomes a cut node. If it's heavy, keep the fine
  fraction as a sixteenth in the column (param2 is taken by soils) and derive the shape as today.
- The cluster rule's cliff (two blocks) for natural ground: with true heights a steep slope is a
  slope; where's the line where rock shows instead?
- A river's banks and a lake's shore: their integer floors meet the fine land; do they need a lip?
