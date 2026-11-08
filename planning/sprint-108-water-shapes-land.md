# Sprint 108 — Water shapes the land

Below the grid the land is noise with valleys carved into it: the channels'
relief is each level's distance to its nearest channel, a lattice halving
level by level, so every level's valleys are evenly spaced and half formed
(the ribbing across whole continents, sprint 107), and mountains are noise
with V's in them: no ridges, peaks or cliffs. Rivers are drawn by one system
and valleys by another, so their joins disagree.

This sprint turns it round: one drainage network at every scale, elevation
from erosion laws along it (stream power for channels, a critical angle for
hillslopes), geology and climate as the laws' inputs. Rivers, ridges, peaks,
cliffs, floodplains, ponds and coves come out of it; none is drawn. Nothing
changes in the game until P2 is signed off from the region tool's views.

Terragen stays a function of a place: no simulation at block scale. Each
level is a real network shaped by the level above (coarse to fine), its
nodes cached as the channels' are; a column asks a few nodes per level.

## Phases

- **P0, measuring.** `scripts/terragen-check/landscape.cpp`: a region at
  block scale scored as a landscape: hypsometry; local relief and slopes
  (cliffs: over 45°); its own drainage (flow on the sampled ground) for
  Horton's bifurcation ratio (real: 3–5), Hack's exponent (0.5–0.6), the
  slope–area exponent (concavity, 0.4–0.6) and drainage density; how
  lineated it is (the ribbing: structure-tensor coherence); closed hollows
  (ponds); water standing over dry ground. Today's land scored first.
- **P1, the network.** Flow accumulated at every level (each node's area,
  its children found by a bounded search, cached); points placed less
  regularly and steered by the level above's surface; topology coarse to
  fine.
- **P2, elevation from the network.** A node's height is its outlet's plus
  its length times stream power's slope for its flow and the uplift; anchored
  to the grid. Hillslopes rise from the nearest channel at the rock's
  critical angle: divides where two meet, peaks where divides do. Replaces
  the channels' relief, the detail noise, gullies and valley cutting.
  *Gate: views beside today's, signed off before the engine changes.*
- **P3, geology.** Erodibility by rock (provinces, the strata's beds) and
  uplift by tectonics: cliffs, escarpments, mesas, gorges, falls at hard beds.
- **P4, deposition.** Where the gradient can't carry the load: flat floors,
  floodplains, fans, deltas.
- **P5, water from the land.** Rivers by discharge; width and depth by
  hydraulic geometry; the surface on the bed; ponds and lakes from what makes
  hollows (ice scour, sills, oxbows, landslide dams, sinks), filled to their
  outlets.
- **P6, climate.** Discharge from rain; washes in dry land; glacial U's in
  the elevation; valleys carried under the sea: coves, rias, estuaries, fjords.
- **P7, the far view, cost, golden** (one that pins the land and water);
  then sloped water with sixteenth-block terrain.

Replaces sprint 107's W4–W7 and phase 8 (fjords).

## Done
- **P0, measuring** (`landscape.cpp`). Score at ~12 blocks a sample (50 km,
  512²): at 2 blocks a sample a plain's whole-block terraces read as slope 0
  and skew the fits. Today's land (628 320 round):

  | | h5r5hbmp range | h5r5hbmp river plain | 496rukkb range | real land |
  |---|---|---|---|---|
  | bifurcation | 4.04 | 5.11 | 4.61 | 3–5 |
  | Hack | 0.50 | 0.41 | 0.49 | 0.5–0.6 |
  | concavity θ | **0.05** | **−0.03** | **0.07** | 0.4–0.6 |
  | lineation | 0.26 | **0.59** | 0.28 | (lower is less ribbed) |
  | pond-sized hollows /km² | 1.29 | 1.00 | 1.19 | |
  | lake water | 0.5% | 0% | 0% | |
  | cliffs (>45°) | 1.3% | 0% | 2.2% | |

  The network's shape is plausible (branching, Hack). What isn't is the
  land along it: channel slope doesn't fall with the area drained (θ ≈ 0;
  stream power's land has θ ≈ 0.5): the valleys aren't shaped by their flow.
  Plains are lineated (the ribbing), and the ground holds ~1 pond-sized hollow
  a km² that never fills: noise's pits, not ponds.
- **P1/P2, first cut** (prototype: `TERRAGEN_LAND=drainage`, region tools
  only; the game unchanged, its checks pass):
  - each channel node's **shape** (`Channels::shape`, cached): the area it
    drains (its own lattice cell's and its children's, counted round its
    head: counting along it cost 3×), its slope by stream power,
    S = ks/√flow with ks the grid's own run's (a range's channels steep, a
    plain's gentle), its foot's height where it joins (level 1: the grid's
    water along the run, `World::linkHeight`) and its head's;
  - the **land** is the lowest of each channel near's height plus its
    hillside, divides rounded (a smooth minimum over half the finest
    lattice). Hillsides are the steepest ground there is: the critical slope
    (0.55) where a range rises fastest, gentler as the uplift's 1.5th power
    where it rises slowly (a shield's ~1%), so old plains stay low;
  - the grid's trunk valleys follow every run's bowed line (all land runs
    shaped, in the prototype), the first level's feet on it; two rings of
    runs in a list (one, and the land jumped at the lattice's planes).

  h5r5hbmp range, 50 km: relief 93 → 191 blocks, slope p50 12° → 27°, cliffs
  1.3 → 4.4%, Hack 0.53, bifurcation 4.5, concavity 0.05 → 0.15 (a steep
  range's channels below a cell's size sit at the critical slope: honest
  there). Plains as the planetarium sees them: smooth, the ribbing gone; a
  zigzag in their contours where each small valley crosses. Cost ~1.8× a
  column (P7).
- **Step 1, water from the network** (prototype): the stream is the water
  of the channel whose valley a place is in (the one giving the land its
  lowest), at that channel's own level, once `STREAM_FLOW` (40 000 blocks² of
  rain) drains to it, wider as √flow to 6 blocks; the grid's undrawn runs carry
  their drained flow. Only one channel owns a place, so streams can't cross,
  and they lie on valley floors. Open: the grid's lakes vanish on the new
  land (their level is the old ground's); `perched` reads steep streams'
  fall along their own line as perching (36% vs 24%): the measure needs to
  look across the stream, not along it.
- **Step 2, junctions** (prototype): a node's area is everything upstream
  (`Channels::drains`, cached, children found along it, all the levels down):
  it was its own cell and its direct children's, so a stream's lower reach
  could drain less than its upper and its water stop. A join facing upstream
  costs 4× (`JOIN_AGAINST`). A channel's never steeper than 60% of its
  hillsides (`CHANNEL_SHARE`): as steep, it cut no valley. Open: bare ramps
  in fast-rising ranges (cxdnspef, u 1.0 v 0.176; there before this step:
  not found yet); streams still sparse; the grid's trunk valleys' wide flat
  floors.
- **Steps 3–4** (prototype): floors: a gentle channel (under 2%) lays down
  a flat floor 4 stream widths wide, ≤ 40 blocks (sized by its catchment it
  flattened whole plains: relief halved, Hack 0.25); trunk valleys in 12
  pieces along the bowed run. Clumping: levels 2+ keep a share of their
  points by noise a few lattices wide (30–100%, `CLUMPS` 439), so valleys
  aren't evenly spaced. h5r5hbmp river plain lineation 0.65 → 0.47; the
  planetarium view of a plain dendritic, not zigzag contours. Range
  unchanged (Hack 0.52, bifurcation 4.6). Open: the wide straight-sided band
  of a grid trunk valley (jwm78d6h); the river plain's concavity −0.2.
- **Step 5, ponds** (prototype): a node may hold one (`Shape::pond_*`,
  chosen in `wear`): a tarn near its head where the ice worked
  (`World::glacial`, up to `TARN_SHARE` 30% of the most worked heads, 12–70
  blocks), a pool on a gentle floor (`POOL_SHARE` 12% of channels under
  `FLOOR_GRADE`, 8–60 blocks: oxbows, sinks, dams). A bowl under its level
  (the channel's at its downstream rim), a sill round it half its radius more
  holding the ground at the level, the stream cutting back through where it
  leaves; a lower valley crossing is dammed (a pond only where the bowl's
  replaced the land: carved only where it was higher, half were dry rings).
  The grid's lakes: their cells' channels at the bed, not the surface
  (`linkHeight`), so valleys drown into them; the outline's still the cells'
  (today's bigger lakes were noise dipping under the level). The rivers'
  banks cut back 1 in 2 (`VALLEY_WALL`) aren't the prototype's: its land
  holds them, and in a range those walls were all there was.
- **Step 6, snow and rock by the ground** (prototype, `ground()`): the
  biome's read at the column's own height (heat by the lapse from its cell's
  mean), and by its slope (`Read::slope`: its hillside's, on a floor its
  channel's): snow no steeper than 35° (`SNOW_STEEPEST`), rock above the
  trees steeper than `ALPINE_ROCK` (0.5), high gentle ground turf. Valley
  floors in a range read warmer than their cells: the dry, warm floors.
- **The ramps, found** (open): the smooth straight bands and cones in a range
  (cxdnspef, h5r5hbmp's peak) are a grid run's own hillside. Every land cell's
  run is a trunk (2.4 km apart at 628 320), its walls at the critical slope;
  a finer channel reaches a place only through a chain of parents, each
  climbing, and joins the nearest line, not the lowest. So where a run sits
  well under the cells round it, its wall wins for ~800 m, nothing cutting
  it. Tried and dropped: hillsides from a cell and its ring (no change); a
  least steepness index from the uplift (channels stood higher, the trunk won
  more); a coarse level's hillside steeper past the finest lattice (seams);
  joins biased to the trunk, or any level joining it (walls combed with
  parallel gullies: range lineation 0.41 → 0.51). Needed: nodes choosing
  where they drain by height (steepest descent), so a run's wall is cut by
  what drains into it.
- **The ramps, fixed** (prototype): a node joins what gives it the lowest
  land, not the nearest line: each candidate's rough height where it would
  join (`Node::rough_low/high`: the network's heights, a node's flow guessed
  as `ROUGH_CELLS` lattice cells, so no cycle with `drains`) plus the climb
  from there at the hillside's slope. And the grid's "not uphill" test is
  gone on the prototype's land: a deep run sits far under its cells' mean,
  so by that field its own walls were uphill of it and nothing could join it
  (the real cause; heights alone barely moved it). h5r5hbmp range: relief
  p90 257 → 223, cliffs 4.2 → 5.0%, Hack 0.50, lineation 0.41 → 0.47 (walls
  of one steady slope gully in parallel, as real ones do); river plain
  lineation 0.39 → 0.38, cliffs 1.6 → 0.6%, orders to 6. cxdnspef's and
  h5r5hbmp's peak's bands are dissected.
- **P3, rock strength** (prototype): each province's rock has a strength
  (`STRENGTH`: shield 1, orogen 0.9 … basin 0.25); hillsides stand at 0.7–1.3×
  the critical slope by it and channels' steepness index with them, so hard
  ground stands steep and soft wears low. Flat-lying beds (`BEDDED`: basins,
  rifts, platforms on shields and highlands, none where folded) step the land
  where a hillside's at least 6% (on a plain a step would cross every field):
  every height through one bed's profile, 14–48 blocks thick by noise, the
  soft 80% rising at half its share, the hard 20% cap three times as steep, the
  same map for the land and its water (`Channels::beds`): benches and cliffs,
  escarpments and mesas. Region pick `bedded` (a rising basin or rift).
  h5r5hbmp: range cliffs 5.0 → 7.5%; river plain unchanged but slope p50
  3.3 → 4.7°; bedded country cliffs 13.8%, lineation 0.35. Open: beds are
  level everywhere (no dip, so no cuestas or hogbacks); hardness by province
  is a 2.4 km cell's, so its edges are where the steepness changes.
- **P4, fans** (prototype): a channel at least `FAN_STEEPER` (2)× as steep
  as what it joins, where that's gentler than `FAN_FLOOR` (4%; on the first
  level, the grid run's grade), drops its load: a cone round a point half its
  radius up it, falling 0.6 as steep as the channel, so it buries the
  channel's last reach and spreads past its foot, its toe at the foot's
  height; radius 0.3 √ its catchment, 16–320 blocks, ×1.6 where dry down to
  ×0.6 where wet. Its own stream runs on it. Region pick `dry` (a range with
  little rain). Small so far: 3% of a dry range's 4 km view changed, scores
  flat; inside ranges few floors are gentle. Open: fans' real home is a range
  front onto a plain (needs the grid run's floor, not its mean grade);
  floodplains past 40 blocks; deltas (land past the shore, outside what the
  channels decide).
- **P6, climate in the valleys** (prototype):
  - **Troughs**: where ice worked a valley (`Shape::ice`, `World::glacial`
    at its head) and its hillsides are a range's (from `TROUGH_HILL` 15%,
    fully at 30%: on a lowland the ice was a sheet, and every channel's floor
    widened into one flat, a river plain's median 31 → 3 blocks), its floor
    widens to `TROUGH_SHARE` √ its catchment each side (≤ 160 blocks) and its
    walls steepen toward 0.9, as high as it's half wide, then the hillside
    as it was (steepened all the way up, a cold coast's land rose 100 blocks).
  - **Washes**: under `WASH_RAIN` a channel's floor is flat to `WASH_SHARE`
    √ its catchment each side (≤ 60 blocks), its banks the hillside's;
    `Read::wash`, and ground() makes its floor sand (Desert). Dry range:
    concavity 0.08 → 0.15, cliffs 9.9 → 11.9%, slope p50 25 → 23°.
  - **Drowned valleys**: within a cell or two of the sea (cells touching it,
    between centres; by height alone a low plain's valleys drowned 50 km in)
    and under 600 m, a valley's floor sinks 60 m, to 300 m where ice cut it,
    less up its walls (`Read::bed`, the owner channel's height): the sea runs
    up it. h5r5hbmp's cold coast: a ragged bay of drowned low valleys.
  Open: no fjord yet (a fjord needs a trough reaching the sea; the coasts
  tried are lowland); hanging valleys (tributaries join a trough's floor);
  troughs' floors flat, not a bowl; streams over drowned reaches untested
  in the game.
- **P7, on** (world code **TF19**): the land from the drainage is the game's
  (`World::m_land`, always; `TERRAGEN_LAND=noise` the old land, for the
  region tools to compare). TF18 codes are refused as an older build's.
  - **Cost** (`region` prints µs a column, one thread, block spacing, 2 km):
    range 39.5 → 49.5 µs (1.25×), river plain 46.7 → 74.9 µs (1.6×). Most is
    a neighbourhood's network built once (`drains` and the nodes' parent
    searches); an 8× node cache changed nothing, so it isn't thrashing.
  - **Golden**: the old one never saw this land (a dry world, and a planet
    too small for channels below its cells; TERRAGEN_LAND=noise hashed the
    same). It now hashes 6 patches of columns (terrain, water, kind, biome,
    a block and 9 apart) round land cells of TF19.dGVycmE at 628 320 round:
    2338250884.
  - **The first column in a place**: `drains` counted every level down, so a
    coarse node built the whole network under it; the far view (reading
    only coarse levels) paid it per tile, `testCoarse` 28 → 429 s. Now a
    node's children are found and each counted as a child of its level
    drains on average (its cell and each finer level's again): 41 s, the
    far view within 23 blocks of the near on average, no sea flipped. Ranges
    stand 10–20 blocks higher (coarse channels drain a little less), the
    scores otherwise the same.
