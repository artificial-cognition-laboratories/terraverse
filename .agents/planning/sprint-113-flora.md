# Sprint 113: Flora

Procedural plants and trees, grown from where they stand. The thing to avoid is
random trait soup: endless variety that all reads the same. So nothing about a
plant is rolled freely. A plant is **lineage × site**:

- A **lineage** is seeded per world (later: per continent). It fixes what climate
  never changes: leaf type, how it branches, its crown's profile, bark and leaf
  colour, and what cold and drought it can bear.
- A **site** is what terragen already knows of a column: heat, rain and its
  season, the soil's composition and depth, fertility, slope, wind, banks.
- The site collapses to a **strategy** (ecology's competitor / stress-tolerator /
  pioneer triangle): how well warmth, water and food are met gives *vigour*, and
  wind, slope and fresh ground give *disturbance*. Traits move together from
  those two numbers, which is why a player can learn them: the same conditions
  give the same look everywhere.

`apps/game/src/flora/` is a pure, fixed-point library like terragen (a plant's
blocks decide the world, so no floats). The wiki's Flora page runs it live, and
is the tuning loop: sliders for the site, a lineage list, and the grown plant.

## Generators

Five, the only hand-authored shapes. A shrub is a small many-stemmed tree.

| Generator | Covers | State |
|---|---|---|
| Woody | trees, shrubs, scrub, mangroves | step 1 |
| Rosette | palms, ferns, agaves, tussocks, reeds | step 2 |
| Stem succulent | cacti | step 2 |
| Mat | moss, lichen, heath, ground cover | step 2 |
| Climber | vines, on a host | step 2 |

## Steps

1. `flora/`: site → strategy → traits, the woody generator, the wiki page.
2. The other four generators.
3. Lineages and species per world; a site's leaf budget spent in layers
   (emergent, canopy, understorey, shrub, ground), each in the light the ones
   above leave; planting in the generator and the far view. Replaces
   `tf_world/vegetation.lua`'s hand-written species and `terra_species`.
4. Small plants as voxel models baked to a mesh per species.

## Open

- Blocks and textures for per-world species against the baked `palette.json`:
  likely a fixed pool of bark and leaf textures, tinted per species.
- Names for generated species.
- How small plants fit the sixteenth-voxel ground code.

## Step 1, as built

- `flora/site.cpp`: `strategy()` (warmth, water, food, rooting → vigour by the
  shorter climate supply; wind, slope and fresh ground → disturbance),
  `lineages()` (seven habits, each a row of ranges a lineage is drawn inside,
  never across), `traits()`.
- `flora/woody.cpp`: `grow()`. A crown envelope from the traits, filled with
  points; limbs grow toward them (space colonisation); thickness by the pipe
  model; wood, thin branches and leaf tufts as voxels. About 0.2 ms a plant.
- The wiki's Flora section (`shell/flora.*`, `wiki/FloraSection.vue`): lineages
  and places in the list, the site's sliders and what it comes to in the card.
  `Scene::Boxes` draws the plant.
- `TestFlora` pins one plant's blocks (re-pin on any change to the generator).

Not yet: leaves shed by season, buttresses beyond a flared foot, bark and leaf
textures of a lineage's own (the page tints the pack's birch bark and oak and
spruce foliage), a figure beside the plant for scale.

## Step 2, as built

- Plants now grow in their own grain: whole blocks (trees), quarter blocks
  (bushes, palms, tall cacti) or sixteenths (everything under 2.5 blocks, and
  covers). `Plant::grain`; `flora/grow.h` holds what the generators share.
- `rosette.cpp`: leaves from one point, each leaning out by how far from the
  heart it grew and bending along its length. Fern, tussock, reed, agave, palm.
- `succulent.cpp`: a round-topped column and its arms. Cactus.
- `mat.cpp`: a cover on a patch of ground: moss cushions, turf blades.
- A site has `light` (what the canopy leaves), and a lineage a least light, a
  most water (`sodden`: succulents rot) and `stores` (drought doesn't stunt it).
  A lineage's vigour is its own: the site's, with those.
- Bushes are the woody generator in quarter blocks.
- The wiki lists one lineage of each of the fifteen habits, by form.

Not yet: climbers (they need a tree planted to climb), covers as a property of
the ground itself rather than a patch, how any sub-block plant is drawn and
stored in the world.

## Step 3, as built: diversity

- A lineage carries kinds, not just measurements: foliage (tuft, plate, spray,
  hang), crown layout (even, tiers, lobes), trunk (straight, sinuous, forked,
  bottle), bloom (plain, scattered, massed); rosette heads (spike, plume, ball,
  disc), fan or feather leaves, tree ferns; cactus stems (column, barrel, pads,
  cluster). Each habit draws only from the kinds that suit it.
- Colours from palettes, two blended: bark (pale to near black, red, olive),
  flowers, autumn colour, and an odd leaf colour in one lineage in five.
- Names from the world's own syllables.
- Each stress has its look: shade lifts and narrows the crown and flattens its
  leaves; heat with a dry season (fire country) and drought flatten and lift
  it; thin soil, wind, age and the edge of its range make it crooked; wet banks
  give thirsty lineages prop roots.
- Each plant its own: height and crown ±15%, its top off-centre, one side lost.
- `Life`: age (seedling to ancient, with dead limbs) and season (bare, autumn
  colour, flowering; grasses die back to straw).
- New habits: flowering herb, lichen; meadow turf.

## Step 4, as built: one generator

The forms are gone. Every plant is `flora/plant.cpp` reading one genome of 43
genes (`Gene`, `GENES` with ranges and labels): stems (woody, fleshy, height,
stems from the foot, splay, girth, taper, upright or arching, crooked),
branching (levels, angle, per node, the turn between nodes, spacing, length,
cone to umbrella, bare trunk, droop or rise), leaves (at a node, crowning
each tip, length, stalk, width, where widest, tip, heart base, teeth, lobes
and their depth, splits, holes, simple or feathered or fan and how many
leaflets, angle, droop, cupped or rolled, thickness, pattern) and flowers.

A leaf is a stalk and a blade drawn from its outline and edge, or rows of
leaflets, or a fan. Presets (`ROWS` in site.cpp) are corners of the space:
conifer, broadleaf, flat-topped, columnar, weeping, rainforest giant, shrub,
cane grass, feather and fan palm, banana, split-leaf, elephant ear,
patterned leaf, fern, tree fern, tussock, reed, flowering herb, spiked and
fleshy rosette, columnar and barrel cactus, moss, turf, lichen. A lineage
draws its genes inside its preset's ranges; one in three has one gene
strayed past them. The site bends genes (shade: bigger, wider, level leaves
and a bare trunk; drought: smaller, narrower, thicker; fire country: flat
crowns; cold: cones; the edge of its range, wind, thin soil, age: crooked).

The wiki's Flora page has a Genes tab: every gene a slider, its own value
and what the site grew of it.

## Step 5, as built: in the world

- `mapgen_terra` plants flora in place of the old trees and Lua planting
  (`tf_world/vegetation.lua` keeps only the far view's species). Trees at
  terragen's tree spots; between them, ground plants as thick as the cover,
  in the light a closed canopy leaves. `flora::Forest` picks a lineage per
  site and keeps grown individuals; `terra_flora` turns voxels into
  `plant_*` blocks with shapes at sixteenths and a palette colour.
- No seams: each chunk places every plant reaching into it, wherever its foot
  is, and writes only its own nodes. `flora::MOST_REACH` (32 blocks, trees)
  and `MOST_GROUND_REACH` (6) bound how far it looks; TestFlora checks no
  plant reaches further. Past the ground reach a column is read only at a
  tree's spot (`World::treeSpot`), and each column's choice is cached
  (`MapgenTerra::footAt`). About 8% on a world's making at the usual chunk.
- Thin branches reach halfway to their woody neighbours, so a diagonal one
  is a branch, not a row of floating dots.

## Step 6, as built: small plants as sprigs

- A plant has an id (`Forest::idOf`, 31 bits): its lineage, its site
  rounded (heat, wet, season, fertility, wind, light, soil depth, bank) and
  which of six individuals. Plants grow from the rounded site, so a plant
  no longer depends on which site asked first (it did: a determinism bug).
- `sprig::Sprigs`, a map block's small plants by their foot: forest seed,
  id, pose (turn, mirror). Saved and sent after the fills (serialization 32,
  protocol 101); dropped when the foot node's content changes, as shapes.
- The generator plants a small plant as `tf_world:growth` (airlike, walk
  through, build into) and its sprig; trees stay blocks.
- The client grows each sprig (`client/sprigs`: its open faces merged into
  rectangles, kept per plant) and draws it in the mesh its foot is in, in
  the plant blocks' grey textures and its own colours, lit as its foot.

## Step 7, as built: sway and distance

- Sway: a sprig's vertices carry their height over the foot as their
  normal's length past one; the node shader (waving plants) bends them by
  it, gusts rolling across the land and a shiver per plant, always on (not
  the waving setting). `tf_world:sprig` is the look node whose tiles
  (one per part) put sprigs in that shader. Bending onto the planet keeps
  normals' lengths.
- Distance, by the mesh rings cut ground uses: whole within
  `ground_detail_near` (12), quarter blocks to `ground_detail_far` (32),
  none further. A small plant averages ~1200 faces whole, ~110 in quarters;
  ~54 plants to a block's worth of land.

Not yet: instancing. Measured, it's memory and mesh time it saves, not
vertices drawn: the rings cut those. It needs an instanced draw in the
Irrlicht driver, a scene node gathering visible sprigs, per-instance data
(place, turn, colour, light) in the node and shadow shaders, and each
instance set on the curved planet. Also: wind from the weather (the shader's
is fixed), digging or cutting a sprig, smooth lighting, the far view.

## Next: a living vegetation system

Plants split in two:

1. **Structure** (trunks, limbs): blocks, as now. Chopped, climbed, built from.
2. **Soft growth** (leaves, grass, herbs, ferns): a sparse record per node
   (lineage, individual, state: bent, cut, grazed) instead of shaped blocks.
   The client grows the mesh from the same generator and draws each
   individual instanced; the server sends a few bytes.

Then, in order: wind sway in the vertex shader (each vertex's distance from
its stem); a trample field round the camera (bodies stamp it, it fades; grass
bends away) and drag from it on the server; trampled records flatten, so
paths form; cutting by tool × plant (straw, fibre), regrowth Scheduled.
Tiers: full near (~32 blocks), sway only further, ground plants gone into the
ground's colour past ~60, trees to the far view's impostors.
