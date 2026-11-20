# Sprint 10 — Vegetation: what grows where, and why

Goal: plants follow the world's climate, ground and water: forests where it's
warm and wet enough, treelines that emerge from altitude and latitude,
gallery forests along rivers through dry country, sparse cactus and scrub in
deserts, reeds at the water's edge, flowers in meadows, mushrooms under dense
canopy. Phase 4 of "Generation" in AGENTS.md.

## Decisions

1. **terragen decides how much can grow; Lua decides what.** terragen gives
   each column a cover density (0..1) from warmth (heat already falls with
   altitude, so treelines emerge), moisture, slope, river and lake banks,
   soil (thin on bare peaks, rich on volcanic ash) and woods-and-clearings
   patchiness, plus whether a tree stands there (one jittered spot per small
   cell, taken if a roll is under the density). Deterministic, from the code.
2. **Species are content** (`tf_world/vegetation.lua`): each declares its
   niche (warmth, moisture, banks, sand) and its shape; the mod picks the
   species that fits a spot and places it. A mod can add species without
   touching the engine.
3. **`core.terra.cover(minp, maxp)`** returns a chunk's columns at once
   (density, heat, moisture, slope, bank, tree spot, roll), for the mod's
   map-generation callback.
4. **More species from the Good Vibes pack:** oak, birch, spruce, jungle,
   acacia, palm, willow trees; cactus; grasses, ferns, flowers, mushrooms,
   reeds, dead bushes.

## Outcome (2026-10-23)

Landed: terragen's cover model (`terragen/vegetation.cpp`: density from
warmth and moisture, slope, banks, soil and patchiness; tree spots on a
jittered 5-block lattice); `core.terra.cover(minp, maxp)` for whole chunks;
`tf_world/vegetation.lua` with 7 tree species (spruce, birch, oak, willow,
jungle, acacia, palm), cactus and 15 small plants, each by niche, planted as
chunks generate (skipping chunks the surface doesn't cross). The old
`trees.lua` decorations are gone. Foliage the pack draws grey is tinted.

Verified: 391 engine tests (new `testCover`: warm wet ground grows more than
cold or dry; nothing beyond the treeline; trees only where it can grow);
`bun check:terragen` 42/42 and 42/42 (the probe looks past plants);
headless patches at many latitudes. In the hot dry south: acacia, then
cactus, scrub and desert grass with palms only at an oasis. Temperate
lowlands: oak woods, willows, reeds and cattails along rivers, meadows
with flowers and berry bushes. In the north: birch and spruce, thinning to
lone spruce at the treeline.

Not run: the client. Known: species are placed by the column's climate,
not yet by the soil type geology gives (a later pass). Trees use the
engine's L-system generator.
