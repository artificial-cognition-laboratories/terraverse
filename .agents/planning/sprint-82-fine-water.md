# Sprint 82 — Fine water

The server's water stays coarse: an amount per node, run by virtual pipes
between nodes (sprint 80–81). What players see becomes fine: near the
camera, water is run and drawn at a sixteenth, on the client alone.

## Plan

1. Fine columns near the camera: each wet node 16×16 columns of water,
   shallow water between them (waves, sloshing, ripples), held to the
   server's amount per node.
2. Drawn as voxels: each column's top on a sixteenth.
3. Pouring as drops: a cup a sixteenth-cube, falling, splashing.
4. What moves through water pushes it (wakes, splashes).
5. The sea drawn by the same layer, meeting the shore's fills.

## Outcome

**Fine water** (`client/liquids.*`). Within 24 nodes of the camera, every
node with a fill (not under more liquid) is `Fine`: per column a floor
(its solid sixteenths from the bottom), a depth and four outflows. A step
(1/120 s, at most six a frame) is virtual pipes per column edge, sped by
the surface difference × the column's depth (waves travel at √(g·h)),
damped 1.2/s, never giving more than a column holds; flow crosses node
edges into neighbouring fine nodes. A node's total is pulled toward the
server's amount at 3/s (added over its open columns, taken in proportion),
so the fine water only moves water about inside the truth. A node still
for a second sleeps; inflow, a drop, a push or a new amount wakes it.
Further off, water is drawn flat and gliding, as in sprint 81.

**Voxels.** A fine node is drawn per column, its top rounded to a
sixteenth (runs merged), with sides wherever it stands above the column
beside it; columns above the node's mean are drawn lighter, so ripples
read.

**Drops** (`TOCLIENT_STREAM`, protocol 87; `core.matter.stream(from, to,
colour, cups)`). Pouring a liquid sends the clients near (48 nodes) a
stream; each cup falls as a sixteenth-cube drop thrown to land where the
server pours. Landing in fine water adds its sixteenth of depth to the
column it hits and drives flow outward; landing fast throws two smaller
drops. Landing anywhere else it's gone (the server's fill shows it).
Powders keep their particles.

**Pushing.** The player and every object with a box drive the columns
they stand in at their own horizontal speed (a bow wave, a trough
behind), and falling in throws water out.

**The sea.** `tf_world:water_source` is in group `liquid_layer`: the map
mesher leaves it (`content_mapblock.cpp`, one hook) and the layer draws
its seen nodes (surface, edges) in its `color`, so the sea meets the
shore's fills at the sixteenth. Node and shape changes now tell the layer
(the water beside a dug block meets it anew). Shores generated before
sprint 81 are filled by an LBM (`tf_world:shore_fills`, once per block).

**Run:** engine build; headless boot (no Lua errors); Lab check 3/3;
matter check; TestSandbox.

**Not run:** the game client. None of the fine water, voxel drawing,
drops, wakes or the sea's new drawing has been seen. The fine water has
no unit test (it lives in the scene node).

**Not done:** the sea itself isn't run fine (no wakes on the open sea
yet: it's flat, and no longer textured); waves don't pass into water drawn
flat; drops don't land on fills drawn flat but are just removed.

## Second pass (after the first run)

The user ran it: splashing and sloshing worked; dark water along every
shore, corner gaps, specks on pit slopes, a hitch every second or two,
lag placing a block of water, a pit that wouldn't overflow, ethanol
hard to see.

- **Dark edges:** liquid in a sculpted node was lit by that node, which is
  solid and dark; now by it or the air over it, the brighter. Drops too.
- **Shore:** corners (diagonal neighbours) and smoothed ground under the
  sea are filled, only where there's open space (`core.matter.open`); the
  LBM is renamed (`shore_fills_2`) so it runs again. The sea's sides
  aren't drawn against smoothed ground.
- **Settling:** a column's drawn top moves only when its water is ~¾ of a
  sixteenth past it, and goes dry/wet with a margin; films under 1/32
  run off whole to the lowest column beside; the server's amount is added
  to wet columns (else the lowest floor), never as a film over slopes.
  Ripple shading halved. Splash drops come only from fast poured drops,
  and throw no more.
- **Clear liquids:** tops catch a little of the sky (`SHEEN`), so spirit
  reads as a liquid.
- **Hitches:** fine water has its own mesh per block, rebuilt alone; the
  flat mesh (sea) is rebuilt on change, and by the day's light six blocks
  a frame rather than all at once every two seconds; a fill's change no
  longer rescans its block for the sea (only node and block changes do);
  what's beside a fine node is looked at once a side, not per column.
- **Overflow:** liquid past a block's open space rises into the block over
  it (`pouring.cpp`), so a pit poured past full runs out over the floor.
  `testPitOverflows`; `testPoolSleeps` pours over the pool rather than all
  into one block (which now, rightly, spilled over its walls).

**Run:** build; TestSandbox 21/21; Lab 3/3; wiki 51/51; matter check;
headless boot. **Not run:** the client, and the shore LBM on a terra world.

## Third pass

The user ran it: smooth with thousands of blocks around, wakes and beach
ripples good; but shallow water never settled (specks flickering over
every puddle), gaps opened in water run through, some nodes blinked, a
full pool of spirit read as a skin, and a placed block of water vanished
for a moment before spreading.

- **Settling:** a node's water may be up to 1/128 node³ off the server's
  before it's pulled back (`SLACK`), so neighbours' slightly different
  amounts no longer keep water running between them for ever; the ground
  drags thin water (`FRICTION`, over its depth in sixteenths), so a
  puddle stops and sleeps. The film run-off is gone.
- **Thin water is a sheet:** any water in a column is drawn at least a
  sixteenth deep (wet past 0.2 of a sixteenth, dry under 0.08), so a
  puddle thinner than the grid reads as one sheet, not specks.
- **Depth:** tops are darker and less clear the deeper the water under
  them (a full node under it counts), so a full pool reads full.
- **Wakes** drive the water toward the mover's speed and never past it;
  piled up, they emptied columns (the see-through gaps).
- **Blinking:** a node stops being run fine under a fill of 48/255, and
  starts only under less than 8/255 (it flipped on every film).
- **A placed block of liquid** is poured all at once (it was over 2 s,
  a trickle too thin to draw at first).

**Run:** build; Lab 3/3; matter check; headless boot. **Not run:** client.

## Fourth pass

The user ran it: settled, full pools read full. But water kept to its
metre squares, ripples were gone from shallow water, pouring slowly
showed nothing, and the shore's fills were darker than the sea.

- **Pools, not nodes.** Fine nodes joined side by side are a pool, and a
  pool's total is held to the server's (plus drops not yet confirmed);
  inside it water runs freely across node edges. Dry ground beside wet
  nodes (open, on something: `liesIn`) is run too, so water spreads
  across a node's edge as across a sixteenth's.
- **Drops stay.** A drop landing on ground near starts fine water there
  (`credit`: its water, fading at 1/s as the server's amount arrives), so
  every poured voxel lies where it fell.
- **Ripples on shallow water:** friction halved (1.5), and a wake throws
  thin water as if two sixteenths deep.
- **One colour:** the flat water (the sea, far fills) is shaded by depth
  as the fine water is, so the shore's fills meet the sea.

**Run:** build; Lab 3/3; headless boot. **Not run:** client.

## Fifth pass

The user ran it: water drains down slopes well. But frames dipped about
once a second standing still and to ~40 fps running through water (the
wake lagged), a placed block still spread into a square, the sea had no
ripples, and the game crashed twice (SIGILL in a Chromium thread; the
main thread was uploading the page's texture; no frame of ours).

- **The dip every second:** a node that couldn't reach its amount (a
  shape's overhangs aren't columns) was pulled forever: woken every
  frame, asleep after a second, woken again. Targets are capped at what a
  node's columns hold, and a node is woken only by what holding actually
  changed.
- **Cheaper:** fine water within 16 nodes (was 24), at most four steps a
  frame.
- **Round, not square:** what a pool's held to is added where its water
  is, evenly (not where the server says it's short, which redrew the
  server's squares), and a node that joins a pool starts dry, filled by
  the pool's water running in.
- **The sea:** its surface within 10 nodes is run fine (target full), so
  wakes, splashes and drops ripple it; a node with open air over it lets
  a wave stand up to a quarter above its top (`CREST`).

**Run:** build; Lab 3/3; headless boot. **Not run:** client. The crash
isn't reproduced or explained.

## Sixth pass: the grid

The user ran it: wakes across shallow water and the sea's surface looked
right, but 22–30 fps (draw ~24 ms, non-rendering ~40–60 ms), the sea
rippled wherever it came into reach and never stilled, hard edges water
wouldn't cross, pours aimed at blocks' middles, and a gap in the sea
where the fine and flat drawing met.

The fine water is rewritten as one grid (`client/liquids.cpp`):

- **One grid of columns** 32×32 nodes (512² columns) round the camera,
  following it by whole nodes once it's 4 off the middle, keeping what
  overlaps. Per column the ground under it (world sixteenths, found by
  looking down from 8 over the camera to 24 under: solid, or a shape's
  highest solid sixteenth) and a depth. Neighbours are the next index:
  no node edges, no hash lookups, no walls between nodes.
- **Tiles** (a node's 16×16 columns) are looked at nearest first, 48 a
  frame, again when their block's nodes or fills change; a tile's water
  is the highest liquid over it in reach (the sea's top, a fill), its
  target what the server has standing over its ground. Only awake tiles
  are run (something moved, landed, flowed in, or its target or ground
  changed); still water, the sea included, costs nothing.
- **Pools** (wet tiles side by side) are held to the server's total as
  before; deep water's waves travel as over a node's depth at most.
- **Drawing:** a mesh per tile, rebuilt only while it's awake or changed
  (and by the day's light, 64 a frame); no vertex cap to hit. The flat
  layer leaves exactly the nodes the grid draws (`owned`), so the two
  meet with no gap and no overlap.
- **Drops** land on the grid's water or ground where they fall.
- **Pouring** aims the stream at the point looked at, not the block's
  middle (`pour.lua`).
- **Timings** on the F3 graph: `Water look`, `Water run`, `Water mesh`.

**Run:** build; Lab 3/3; headless boot. **Not run:** the client; the
grid has no unit test.

## Seventh pass

The user ran it in a new world: pours land where aimed and spread round,
drops roll down slopes, 60 fps inland. By the sea 12–30 fps (`Water mesh`
40–80 ms, `Water run` 20–36 ms), and walls of water rose over the land on
arriving, settling after a while.

- **The walls:** a tile looked at before the blocks under it arrived took
  the unloaded ground for solid; when the real (deeper) ground came, its
  depth stayed and its target didn't, and holding the pool to the server
  poured the difference in at once. Now a tile meeting an unloaded node
  waits for its block; when its ground moves its surface stays put; and
  a pool is filled or drained at most 0.25 nodes³ a tile a second.
- **Meshing:** an awake tile is drawn again only when one of its tops
  moved a sixteenth (water moving under that looks the same).

**Run:** build; Lab 3/3; headless boot. **Not run:** client.

## Eighth pass

The user ran it: walls gone, near 60 fps standing still; walking the
coast, the water "repooled" as the grid moved (worse where earlier,
messier sessions had left fills), artifacts where it met the sea, 15–40
fps (`Water mesh` 15–60 ms, `Water run` 9–30 ms, `Water look` 4–11 ms).
The page's renderer died once and reloaded (Chromium, not the water).

- **Repooling:** each time the grid followed the camera it woke every
  tile it had, so the whole coast was run (and drawn) again every four
  steps. Now only the tiles that were its edge (walled till then) wake.
- **Meshing has a budget:** at most 48 moving and 32 still tiles drawn
  again a frame, nearest first; the rest wait a frame. Sides alike along
  an edge are one quad.
- **Looking:** 24 tiles a frame (was 48).

**Run:** build; Lab 3/3; headless boot. **Not run:** client.

## Ninth pass: on the GPU

The user ran it: steady by the coast, 36–41 fps walking it, `Water mesh`
still 20–50 ms; tiles came in behind a running player. Asked for all
three: the GPU, calm deep water asleep, and less work further off, for
headroom before a destructible world.

- **Drawn on the GPU** (`liquid_shader`, GL 3): one shared mesh of a
  tile's columns (a top and four sides each, 5120 vertices), drawn once
  per wet tile. The grid's tops and ground are a 512² RG float texture
  (`m_texels`, uploaded when anything changed); the vertex shader reads
  its column and its neighbour and puts a face that isn't there (dry, a
  side with nothing lower beside it, the grid's edge) behind the eye.
  Depth shading, ripple light, sheen and fog in the shader. Meshing is
  now writing 512 floats a tile; other drivers keep the CPU meshes.
- **Calm deep water stays asleep:** a deep column (over a node) wakes the
  tile beside only by moving 20× what a shallow one must.
- **Less further off:** a tile over 10 nodes away sleeps after 0.3 s
  still (1 s near). Not a coarser grid: that's still to do if needed.

**Run:** build; GLSL checked with glslangValidator (as GLSL 150); Lab
3/3; headless boot. **Not run:** the client: the shader has never drawn.

## Tenth pass

The user ran it: the GPU water draws and is smoother, mostly near 60
fps standing, 33–41 walking the coast; `Water mesh` still 30–50 ms, and a
patch by the sea kept being run again.

- **The texture upload** locked the height texture read-write, which
  reads all of it back from the GPU first; it's locked write-only now.
- **`Water flat`** is its own line on the graph (the flat layer's
  blocks, the sea's), and what the grid takes or lets go of is drawn
  flat again four blocks a frame (walking the coast changed them every
  frame).
- **Fewer wakes:** a tile first seen, or at the old edge when the grid
  moves, is run only if its water doesn't stand level with a known
  neighbour's along their edge (`uneven`).

**Run:** build; Lab 3/3; headless boot. **Not run:** client.

## Eleventh pass: the sea as a reservoir, two grids, cheap flat sea

The user ran it: `Water mesh` ~2 ms, `Water look` ~5, but `Water flat`
17–60 ms and `Draw scene` 26–42 ms; the fine sea stood a sixteenth under
the flat sea and its edges kept waking (the sea's water ran into the
shore's unfilled corners and slopes, and a pool's amount kept it low).

- **The sea is a reservoir:** columns whose water is the sea's (`sea`,
  its level) are held at that level (4/s, awake tiles), not to an amount;
  a pool joined to the sea isn't held to one. A tile first seen fills any
  gap under the sea's level at once.
- **Two grids, one code** (`Grid`): fine, a sixteenth, 20 nodes a side;
  coarse, a quarter, 96 nodes a side. A tile is 16×16 columns in both (a
  node fine, 4×4 nodes coarse), so one shared mesh and one shader draw
  both (`waterColour.a` is columns a node). Corners sit on 4 nodes, so a
  coarse tile is wholly in the fine grid (not drawn) or out. Drops and
  wakes go to the fine grid where it is, else the coarse. The fine grid
  now reaches 10 nodes (was 16); 3D water reaches 48.
- **Level tiles are one quad** (face 5) where every top is at one height
  and no neighbour stands lower: the open sea is a few hundred quads.
- **The flat sea is cheap:** a sea node's faces are looked at once when
  its ground changes (`seaFaces`), its tops drawn in runs along x; only
  tiles left behind when a grid moves are drawn flat again.
- Without GL 3 the grids are off and water is drawn flat (said in the log).

**Run:** build; GLSL validated; Lab 3/3; headless boot. **Not run:** client.

## Twelfth pass

The user ran it: fine and coarse meet cleanly, `Water mesh` 2 ms, `run`
4, `look` 5, `flat` 6–15, `Draw scene` 16–40 ms; but water vanished and
came back as the server's squares now and then (a round puddle too), and
the coast was run again while just looking about.

- **Resets:** a grid that followed the camera threw everything away when
  its height had changed at all (every step up or down the beach), and
  started again from the server's amounts per node: the squares. Now it
  always keeps what it has; its height follows only a climb or fall past
  `follow`, and then its tiles are looked at again keeping their water.
- **Culling:** a tile is drawn only if its box (its tops and the ground
  its sides stand on) is in the camera's view.

**Run:** build; Lab 3/3; headless boot. **Not run:** client.

## Thirteenth pass

The user ran it: near 60 fps walking the coast and running through
water; pours flow away well. Left: a split-second see-through flash as
water came into the fine grid, darker squares in the shallows, and the
sea as an endless source: water poured by the coast joined it and crept
over the island for good.

- **The endless source:** the sea's tiles are in no pool now; land beside
  the sea is its own pool, held to the server's amount, counting only
  water above the sea's level (`sea_level`: what's below ran in from the
  sea and is the sea's). Poured water drains as the server's does and
  runs into the sea, which takes it at its level; the shore's low
  corners stay filled.
- **The flash:** the coarse grid stopped drawing a place the moment the
  fine grid reached it, before the fine tile was looked at; now it draws
  there till the fine tile's known and drawn (`covers`).
- **Squares in the shallows:** a level tile drawn as one quad was shaded
  by its middle column's depth; it's one quad only where all of it is at
  least a node deep (shaded alike either way).

**Run:** build; Lab 3/3; headless boot. **Not run:** client.

## Fourteenth pass

The user ran it: waves seen far off on the coarse grid, near 60 fps. But
the sea still overflowed onto land (most on the coarse grid, so the far
water showed floods that weren't there when walked to), a see-through
gap now and then as water came in, and `Water flat` 19–35 ms.

- **The overflow:** a tile that's partly sea held its sea columns at the
  sea's level but nothing held its land columns: waves washed water up
  them, the sea made it up, and it spread inland for good. Coarse tiles
  (4×4 nodes) are nearly all partly sea on a coast. Now a sea tile's
  land columns drain whatever stands above the sea's level (or the
  server's own water there: `want`, per column).
- **Gaps:** what a grid lets go of (left behind as it moves, or a node it
  no longer draws) is drawn flat again at once, not a few blocks a frame.
- `Water flat blocks` on the graph: how many flat blocks were drawn again
  in the frame, to see what drives `Water flat`.

**Run:** build; Lab 3/3; headless boot. **Not run:** client.

## Fifteenth pass

The user ran it: `Water flat` mostly ~6 ms (spiking ~15 swimming in
shallows), solid 60 fps much of the time. Water still crept over land,
near and far, and wasn't there when walked to.

- **The cause:** every block of full water counted as the sea, a
  reservoir held at its own level: a lake in the hills, or blocks of water
  placed earlier by the coast (full water is `water_source`), each an
  endless source above the sea, pouring onto the land. Only full water at
  or under the world's sea level (`Horizon::water_level`, passed in each
  frame) is the sea now; above it, it's water like a fill's, held to the
  server's amount. The sea's level for draining land beside it is the
  world's, not the last sea node seen.

**Run:** build; Lab 3/3; headless boot. **Not run:** client.

## Sixteenth pass

The user ran it, in a new world too: spilling mostly gone, but now and
then a blob of water rose over a beach and spread inland (near and far),
in the same places, and only coming at them from one side; and water
coming into the grid sometimes arrived moving.

- **The blob:** a pool beside the sea counts only what it holds over the
  sea's level, but it was held to the server's whole amount (a shore
  fill's water is all under the sea's level), so it was always short and
  kept being filled, over the beach. Each tile now has `above`, the
  server's water over the sea's level, and a coast pool is held to that.
  Which side it's come at from decided which tiles joined a pool with
  the shore's fills first.

**Run:** build; Lab 3/3; headless boot. **Not run:** client.

## Seventeenth pass

The user ran it: the blobs are gone ("absolutely stunning"), steady
frame rate. Left: shallow sloped water along the coast animating as it
came into the grid, and all the water vanishing and coming back tile by
tile when flying up or down.

- **Flying:** a climb past `follow` has every tile looked at again, and a
  tile waiting to be wasn't drawn; now a known tile's drawn as it was
  till it's looked at again.
- **The shallows:** the shore's low ground the server never filled (the
  corners and slopes under the sea's level) started dry and was seen
  filling from the sea. A tile first seen that is the sea's or beside it
  starts with that ground full to the sea's level.

**Run:** build; Lab 3/3. **Not run:** client.

## Eighteenth pass

The user ran it: flying is fixed; coast tiles still sometimes came in
animating, and some drew black or grey.

- **Black tiles:** a tile was lit by one node, its middle at its water's
  mean height, often inside the shore's sand (dark). It's lit by the
  brightest of five places in it, just over its highest top.
- **Animating shallows:** a coast tile seen before the sea tile beside it
  still started dry; a sea tile first seen now fills the low ground of
  the known tiles beside it to the sea's level too.

**Run:** build; Lab 3/3. **Not run:** client.

## Nineteenth pass

The user ran it: coast "pretty much perfect". Left: rare dark shore
tiles, a few water tiles darker for a frame or two while walking, and a
rare tiny re-run of a coast tile.

- **Darker for a frame:** a node a grid took was drawn flat as well till
  its block's flat mesh was rebuilt (four blocks a frame): two
  transparent layers. Taken nodes are drawn flat again at once now.
- **Dark shore tiles (flat):** flat water's light is the brightest of the
  node, the two over it and the four beside the one over it.
- **Re-runs:** a tile first seen starts still, as the server has it.

**Run:** build; Lab 3/3. **Not run:** client.

## Twentieth pass

- **Dark shore tiles:** flat fills counted ground under them (walled, 2)
  as a full node of liquid, so were shaded a node deep: near opaque and
  dark over the sand. Only liquid under counts now.
- **`Water flat` every second or two:** the flat blocks were drawn again
  by the day's light every 2 s whether it had changed or not; now only
  when it has.

**Run:** build; Lab 3/3. **Not run:** client.
