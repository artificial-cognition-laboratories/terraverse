# Sprint 115 — Rivers

Water today is a pond everywhere. The server keeps an amount per node; the
client runs fine shallow water near the camera, held to those amounts
(sprint 82). That's right for a pond and wrong for a river: a river's
sloped surface only holds while water keeps arriving at the top and
leaving at the bottom. Wake the sim on a river with neither and it levels
out like a pond: the high end drains, the low end piles past the bank.
That's the overflow seen near rivers today. And terragen cuts channels
without knowing how much water they carry, so even right flow would spill.

The aim is one water system from the planet down to the ripple, where
what players do upstream reaches downstream: block a river and it
overflows at once; dam a valley and the valley fills; cut an outlet and a
new river carves its own way, with ponds along it. Changing the landscape
on that scale is the point.

## The one idea

**A river is a channel with a known discharge, and the local sim is a
window onto it.** Each scale reads only the boundary values of the scale
above, the way the sea is a reservoir held at its level and the weather a
coarse grid with local detail. Water that a channel can't carry stops
being river and becomes ordinary fills again, and the network rewires
around wherever it ends up going.

## Three scales

| Scale | What it is | Tier |
|---|---|---|
| Global | A graph: reaches (discharge, travel time), lakes (V(h), level, outlets), from terragen's drainage network | Settled / Scheduled, everywhere |
| Block | The server's fills, plus a flow per node; reaches feed water in at their upstream edge and take it at the downstream edge | Live near players |
| Local | The client's fine shallow water, edges driven by the block layer | Live near the camera, a picture only |

## Decisions

**The weather drives the rivers.** This is the direction, and nothing
replaces it: a river's flow is never set by hand or by a rule per place.
Rain falls where the weather (`fluid::Layer`, stepped to the game clock)
says, and runs down the drainage terragen already built:
- **Runoff:** each grid cell sheds its precipitation by how full its soil
  bucket is: saturated ground sheds most, dry ground soaks it up. A wet
  season on wet ground is a monsoon by itself.
- **Routing:** each grid cell's discharge is its runoff plus what drains
  into it, each cell a small reservoir letting water go over its run's
  travel time. A storm upstream reaches the mouth days later. 64k cells a
  weather step (3 h): everywhere, always (the Settled tier), conserving.
- **Below the grid:** a channel's discharge is its catchment times its
  region's runoff per area now; small streams answer within a step.
- **Water from discharge:** terragen sized each channel for its mean flow;
  the ratio now/mean sets depth (~Q^0.4) and speed (~Q^0.3): drought drops
  a river by sixteenths in its bed and dries small streams and washes, a
  wet season runs it full and fast, and past its brim it floods (with
  basins: a floodplain is a shallow basin a river overtops for a season).
- Rivers in different climates behave differently because their weather
  does. The day's length is the player's: the rivers follow the clock the
  weather runs on. If rivers look wrong, look at the rain first: they're
  only as good as the weather feeding them.

**Channels are cut for their water.** Width and depth by hydraulic
geometry from discharge (sprint 108 P5), the bed smooth at sixteenths so a
sloped river reads as a slope, not terraces.

**Blocking a river overflows at once.** Live, the reach keeps feeding its
discharge into the upstream end; with the way blocked the level rises
until it finds the lowest way out and pours over. Below the block the
reach loses that flow and drains over its travel time.

**A dam fills its valley.** When an obstruction is built, a priority
flood of the terrain behind it gives the basin's volume by level, V(h),
and its spill point (the crest, or a lower saddle elsewhere). The level at
any time comes from V(h) = (inflow − evaporation − seepage) × t: a lake is
one number and a curve. Blocks loading are written as `level(t)` against
their ground; nothing steps. "Spills at T" is a Scheduled event. Where
losses balance inflow the lake stops rising and never spills: a closed
lake, decided by climate.

**Outlets share by the weir law.** Each gap out of a lake passes
Q = C · w · h^1.5 (w its width, h the lake above its floor), so a notch's
flow is set by its shape, not a setting. A lake settles where inflow =
losses + Σ outlets. A notch below the crest that can pass the inflow keeps
water off the dam: a spillway. Several outlets split the flow; changes
upstream re-split it.

**New streams route over raw land.** Water leaving an outlet with no reach
runs by steepest descent. Hollows on the way become lakes by the same
basin code, fill, spill and continue, until it joins a channel, the sea,
or a basin that balances it. Each stretch found becomes a reach, each
hollow a lake node: the network is data, nothing original is special.
Away from players the ground comes from terragen directly (a function of
place); changed places from the map.

**Erosion moves the rivers.** Flow over loose ground (soil, sand, gravel)
removes it at a rate by stream power above a threshold per material, and
it settles where the water slows. The same law that shaped the land in
terragen, at play speed:
- a soil dam overtopped cuts itself through (a breach);
- a held obstruction's lowest spill deepens into a channel and captures
  the flow (avulsion), the old bed drying;
- an outlet in soil widens and can run away, capturing the whole river and
  dropping the lake to its floor; one cut in rock or lined in stone holds
  its size;
- ponds on a new stream silt up over time.
Live, sixteenths removed per step; away, the time until the next capture,
breach or fill is a threshold event and the graph is rewired then.

**Rough away, concrete when visited.** Away from players everything is
the sparse graph and its events. A player arriving makes it concrete
(blocks written from the graph's state); leaving hands it back. Handovers
conserve water exactly; paths and shapes may be approximate. It doesn't
need to match what watching would have given, only to be plausible from
where the player stands. The same history still gives the same result.

**Upstream changes travel.** A change of discharge moves down the graph
reach by reach at each one's travel time, never instantly.

**Waterfalls** are where a reach drops more than shallow water can carry:
falling sheets (like poured drops) landing in a plunge pool that returns
the water to the reach below. Where they sit comes from hard beds
(sprint 108 P3).

**Dam strength** waits for breaking and structural strength. Until then a
simple check of head against material; a failure releases the reservoir as
a flood pulse down the graph.

**The server holds the water that matters.** Where water goes affects
other players, mills and crops, so the block layer's flow is the server's.
The client keeps ripples and wakes.

## Phases

1. **Confirm the overflow.** Dump server fills and client depths at a
   failing spot over a few seconds; check that it's the pond levelling.
2. **Reaches feed the block sim.** Inflow and outflow at a reach's edges,
   flow per node on the server, the client's edges driven by it. Rivers
   flow locally; the overflow bug goes.
3. **Rivers from the weather.**
   - 3a: runoff and routing on the grid's drainage tree, fed by the
     weather's precipitation and soil; discharge per cell, saved beside the
     weather. Tested: a storm upstream arrives downstream later, nothing lost.
   - 3b: any river block's discharge: its reach, mean flow, now/mean, depth
     and speed.
   - 3c: the server sends each map block's river level and speed when they
     change; the client holds and carries its water by them (and the way it
     runs from the network, so long level runs flow too).
   - 3d: seen by eye: a river's Q, ratio and travel time on the debug line;
     a command to bring a storm or a drought.
4. **Channels for their water.** Hydraulic geometry and smooth beds in
   terragen (with sprint 108 P5/P6); a generator change, so a version bump.
5. **Basins.**
   - 5a: the basin finder: from a pool, a priority flood over column tops
     gives V(h), A(h) and the spill (crest or saddle). Pure, tested.
   - 5b: a dam on a live river: an edit beside held water looks again at
     its pool; dammed, it becomes a lake its river's discharge raises,
     written into the map, standing at its spill; dug out, it drains.
   - 5c: away from players: ground not loaded from terragen, level(t)
     written as blocks load, spills as Scheduled events.
   - 5d: floods over floodplains (a season overtopping a shallow basin).
   - 5e: closed lakes: evaporation from the weather against inflow.
6. **Outlets and new streams.**
   - 6a: the weir law: a lake rises over its crest till its ways out pass
     what comes in, each crest column a weir; several share by their heads.
   - 6b: new streams: its outlets run down over raw land to water, or into
     a hollow they fill (a lake of their own, spilling on in turn).
   - 6c: the graph growing: a stream joining a river adds to it; a hollow
     lake found again when its ground changes; streams away from players.
7. **Erosion.** Stream power on loose ground, Live; breach, avulsion and
   runaway outlets as Scheduled events.
8. **Waterfalls.**
9. **Dam failure**, once structural strength exists.

## Limits

- Routing away from players is only as fine as its sampling, corrected
  when blocks load, conserving the amount.
- Erosion away from players is an estimate measured from the Live model
  (simulation scales rule 1).
- Routing grows with every stream players start: a budget per step.

## Not doing

Per-block water across the whole planet, groundwater as a field (seepage is
a rate), sediment as a tracked substance beyond loose ground.

## Done

- **Phase 1, the overflow, found** (from the code): the client's fine water
  holds every body of water above the sea to the server's amount as one
  pool (`Layer::pools`, `hold`), free to move within it. A river is
  generated as a staircase of full blocks, so run as a pool its upper steps
  drained into the lower and over their banks; only the sea was held at a
  level.
- **Phase 2, first cut: held water.** Generated river and lake water carries
  `fill::HELD` in its param2 (`terra_column.cpp`); the client holds it as it
  holds the sea, each column at its own block's level, and the land beside
  it drains what's above the server's water. Only new chunks have it.
- **Phase 2, currents.** The client finds each held column's way from the
  staircase (`Layer::currents`, with the pools): toward its nearest step
  down, across its own level's water. A lake with no step lies still, a run
  longer than the grid sees too. `carry` moves only what stands over or under
  the level (wakes, splashes) downstream at `CURRENT` (0.8 nodes/s, till
  discharge sets it), so still water costs nothing and a wake drifts away.
- **Phase 2, the server keeps it full.** `Ground::held` (param2's
  `fill::HELD`): a woken river block is made up to full after each flow
  step, so a ditch dug from it fills to its level and the river doesn't
  fall (`testRiverKeepsFull`). A block in it no longer overflows: that's
  basins' (phase 5).

- **3a, rivers from the weather** (`rivers/`). `Network`: each grid cell a
  linear reservoir (outflow = store / its travel time, solved exactly over a
  step so a 3 h step is stable), upstream first down terragen's `receiver`
  tree; a running mean per cell (about a year) is what its channel was cut
  for. `runoff()`: each cell's rain from `Weather::cellRain` (the layer's,
  scaled to the grid's climate for the season, as a column's is) times
  `shed(soil)` (5% off dry ground, 90% off saturated). The weather calls
  `Opts::each_step` after every layer step, its settling year included, so
  the rivers run on its own steps. Tests (`TestRivers`): settled stays
  settled; a storm over a chain's head peaks at its mouth 69 h later at a
  quarter of the height, every m³ held or at sea; saves; a real world
  (TF22.dGVycmE): 3155 river cells average 2831 m³/s against 111 elsewhere,
  flows move >5% in a week, water conserved. Not wired into the server yet.
- **3a, ground water.** Rivers ran dry 60 days into a drought: a cell held a
  day of water. Now `shed(rain, soil)` splits what ground does with rain:
  run straight off, and soaked deep (30% of what soaks in) into each cell's
  ground water, let go over `GROUND_TIME` (60 days) into its channel. A dry
  month leaves a river a third of its flow, a dry year next to nothing.
- **3b, a column's flow** (`rivers::flowAt`): a grid river's from its run's
  cell (terragen's `Ground::river_cell`, exposed: the generator's unchanged);
  a stream's from its own cell's ground (`shedding` against its mean); a
  lake stands. Depth goes as the ratio's 0.4th power, speed as its 0.3th
  over 0.3 + 20√slope nodes/s; the surface stays between its bed and the top
  of the block over it. The server runs the network on the weather's steps
  (`stepRivers`, `<world>/rivers.bin` each day) and Lua reads it:
  `core.terra.river(pos)` → discharge, ratio, rise, speed, lake. Headless on
  TF22.dGVycmE by spawn: rivers of 500–2600 m³/s at 3–4× their mean (a wet
  season), lakes standing.
- **3c, rivers to the client** (protocol 102, `TOCLIENT_RIVERS`). Every 2 s
  the server looks at the map blocks round each player (7×7 across, 2 down,
  1 up; `terraSendRivers`): a block's held water is found once, its flow read
  again only after a rivers step, and each player is sent the blocks whose
  rise (sixteenths) or speed (nodes/s × 40) changed since it was last told.
  The client holds a block's held columns at the generated level plus its
  rise (a changed level wakes the tile) and its currents run at its speed
  (`CURRENT` till it's told). A weather opened from its file with no step to
  take starts the rivers all the same, and a network with no file is settled
  on the year's mean rain (`rivers::climate`), not the day's: settled on a
  dry day, every river started at nothing.
  Seen in the game: holds and runs as drawn.
- **3d, seen by eye, and 3c's gaps closed** (protocol 103). The probe on a
  river's water reads its flow, its ratio to the usual, its rise, speed and
  days to the sea (`Network::toSeaTime`: each cell's travel down to the
  mouth); `/river` reads the one you stand in. `/rivers storm|drought
  [hours [km]]` (server priv) puts a storm (8 mm/h on saturated ground) or
  a drought (nothing shed) over the grid cells round you in place of the
  weather's runoff (`rivers::Forcing`, `core.terra.force_rivers`); `clear`
  hands them back. Headless: a river at 559 m³/s went to ×20 (+0.94 blocks)
  a day into a storm, and to ×0.13 (−0.56) after a month's drought.
  The way a river runs comes from its run: terragen's `Ground::river_along`
  (blocks along the run, rising downstream; no generation change), its
  gradient over ±4 blocks found once per map block on the server and sent
  as a byte; the client's current takes it where it has one, so long level
  reaches flow. Swimming follows the held level: a held column is swum in
  under its top plus its rise, and over it in a flood (`localplayer`), so
  you no longer swim in water drawn lower. Speed's floor is 0.6 nodes/s.
  Left: water past the fine-water grids is drawn as generated (no rise is
  sent that far; a far-view concern, not the network's); the server's
  matter still keeps held blocks full, which only matters once something
  reads a river's amount (pumps, mills: phase 5 on).
- **5a, the basin finder** (`rivers::Basin`). A priority flood from a pool
  over column tops (a function: the map's, terragen's later), lowest first;
  each column's `enter` is the least level that reaches it, and the first
  column under the pool's own level is the way out: the flood's level then
  is the spill. Volume and area by level are exact from that (a pocket
  past a pass joins only over the pass), and `levelOf` inverts it. A pool
  with its river's next step beside it spills at once, so only a closed
  river rises. Columns it can't tell stop the flood that way (`capped`).
  Tested: a dam fills its valley to the crest, a lower saddle spills first,
  an open river doesn't rise.
- **5b, a dam on a live river** (`server/lakes.cpp`). Edits are noted
  (`terraEdited`, from `onMapEditEvent`) and looked at every 0.5 s: river
  water beside one has its pool's basin found over the map (plants are
  open, water stands round them; unloaded space over the ground is open
  air); dammed, it's a lake. Its river's discharge (`flowAt` at its seed)
  raises it by Q / A(h) in the physics' seconds, as the block sim runs, so
  a big river blocked overflows at once; each block it rises is written as
  held water marked `fill::LAKE` (the finder sees the ground under it), the
  part block over as the rivers' rise for its top block, still. An edit on
  or round its basin (its dam) looks again: dug out, it falls at 0.25
  blocks/s to its river and is gone. Saved to `lakes.txt`.
  `core.terra.lake(pos)`. Headless: a stone plug across a 158 m³/s river
  dammed it; the pool above became a 99-column lake that rose to its spill
  a block over its banks (onto the floodplain) at once, and dug out, it
  drained back to the river in 4 s.
  Not yet: what pours over the spill goes nowhere (phase 6's outlets
  route it); the river below the dam still runs as the network says
  (it should lose the lake's inflow while it fills); a basin's ground not
  loaded (5c).
- **5c, lakes away from players.** A basin's ground not loaded is the
  generator's own column (`terraGeneratedTop`: the far view's surface,
  cached): headless, 1089 loaded columns round a river matched it exactly,
  and a dam with only ±12 blocks loaded found the same 99-column lake as
  with all of it. A lake's water is kept per column (`wet`: the level each
  is written to): a column whose blocks aren't loaded waits and is written
  when they load or are made (headless: loaded later, the columns past the
  first area had its water), and draining takes water out of columns
  written earlier even if they were away. Written columns are saved with
  the lake, so a restart neither loses nor strands them. A lake reaching
  its spill says so (the log), and `core.terra.lake` gives `spills_in`, the
  seconds to full at its inflow now: the Scheduled event as data, for Lua.
  Lakes run on the server's own steps whether or not anyone's near, once
  the rivers have started (the weather's first opening).
  Not yet: a Lua event when one spills; an edit made while a basin's blocks
  were away (only ever by another lake) isn't seen until it's next looked at.
- **5d, floods over floodplains.** `flowAt` gives the river's `surge`, its
  surface over the generated level unbounded (`rise` stays within the
  block over it). A pool whose surge passes 15/16 is over its banks: a
  flood, a lake that stands where its river's flow puts it (`target`, the
  highest of its pool's blocks each rivers step) instead of filling by
  volume, spread (`Basin::spread`) over the dry ground beside it at or over
  its level and under the surge, never onto lower ground (another pool's,
  or where it runs off). It comes up over the land at 0.05 blocks/s, goes
  down as a lake drains when the flow falls, and is gone once it's back
  in its channel. Rivers are read, and flood, in the server's Live blocks
  (round players and kept loaded), no longer only round each player as
  they're sent, so the server decides floods whoever's watching. Floods
  need a ratio of about 2.6 on a river two blocks deep (5 on one block): a
  wet season's, not a day's. Headless, a day of forced storm (×20, 4.6
  blocks over) flooded 566 of the area's columns in 30 s and 1607 in a
  minute; ten dry days on it had drained back to its channel.
  Not yet: floods away from players (rough away: none written there);
  two neighbouring pools' floods over the same ground each keep their own
  level, the lower's water left under the higher's; a flood stands still
  (no current over the plain).
- **5e, closed lakes.** Open water's evaporation from the weather
  (`weather::openWaterEvaporation`: Dalton's law, the air's want of vapour
  times Penman's wind function halved, since nothing bounds it by the sun's
  heat: 12 mm a day on a hot dry windy day, 0.35 on a cool damp still one,
  none into fog). Two scales:
  - The network: a terragen lake cell's rain is all its water's, and the air
    over it takes `Shed::lose` off it; solved exactly, a lake losing more
    than comes in falls to nothing and lets nothing go on (a closed basin),
    and in a wetter season fills and runs on. Water's conserved with what's
    evaporated counted (`Network::evaporated`; saved state TFRIVER3, an
    older file is settled afresh). The network settles with its lakes'
    losses now. On TF22: 44 of 210 lake cells are closed, decided by their
    climate.
  - A dammed lake: its river in, plus rain less evaporation over its surface;
    it stands where they balance if that's under its spill.
    `core.terra.lake` gives rain, evaporation, `closed` and the `balance`
    level. At a player's scale this never closes on a river (a few mm a day
    off a hundred columns against 100+ m³/s): it matters once small streams
    carry their own small share (see below).
  Cost: sampling the air over 210 lake cells each weather step, ~2 ms.
- **The streams fix, and the scale it found.** A stream below the grid
  now carries its share of its cell's runoff: terragen's channels' drained
  area (`Ground::stream_cells`, in cells; no generation change), where it
  had carried all of it. But on a world the size of TF22 (grid cells ~36
  blocks across) there are no streams below the grid: every river a player
  meets is a grid river. The real mismatch was scale: the network's
  discharge is its planet's in real metres (130 to 2900 m³/s near spawn),
  while the blocks squeeze that planet ~2400 times across, so a river a few
  blocks wide carried a continent's water into a dam, which filled at once
  and could never close by evaporation. `Flow::local` is what the channel
  the player sees carries: its width × its depth now × its speed (all by
  the weather's ratio): 3 m³/s for that river. Lakes fill by it; the
  network's number stays the planet's, shown beside it (`/river`, probe).
  With `Basin::area` counting the columns just at a level (a pool's own:
  it divided by 1 m² instead of 99), a dam's lake now rises at a pace you
  watch: a block in 33 s over 99 columns.
- **6a, the weir law** (`Basin::outflow`). A dammed lake's level: its river
  in, rain less evaporation over it, less what pours out, unbounded at its
  spill (8 substeps a step, for a small lake's weir). Ground at its crest's
  height that the flood reaches over the crest is its (the crest itself, a
  flat beyond, the river's next pool up): under it once it rises over. Its
  ways out (`rim`) are its crest columns with the ground beside them
  falling away, each a weir a block wide, Q = 1.7 (h − crest)^1.5: a
  narrow notch holds a lake higher than a wide crest, a spillway cut lower
  passes it before the crest does, and outlets share by their heads.
  Tested: nothing over a crest under its spill, the 1.5 power over it, a
  saddle passes a lake whose dam stands 9 blocks higher. Headless, the
  dammed river's lake (320 columns with its floodplain) poured out by one
  crest column and kept rising till that notch passed its 3 m³/s.
  A lake with its dam dug out drains (0.25 blocks/s) and its water's taken
  out of every column it had, loaded below or not.
- **6b, new streams.** A lake pouring out runs its largest outlets (4 at
  most; the lowest, nearest where its flood ran off) down over the land:
  the steepest way down, and across a flat by the shortest way to the
  nearest way down (never back into it, never uphill), till its water
  joins a river, a lake or the sea, or a hollow holds it: that hollow's
  basin becomes a lake its stream feeds (`fed`), which spills on in its
  turn (6 generations at most). A stream's water is a block of held lake
  water on its ground all the way down, kept as a lake's columns are; each
  takes its outlet's weir share; when the lake stops pouring they dry.
  Headless: the lake's one outlet ran 5 columns down to the river below
  its dam.
  Not yet: a hollow filled live (the code path's run only to its basin
  check); a lake risen more than a block over its spill covers only its
  basin (it should spread over what its level reaches, re-found); what a
  stream brings a river isn't added to it (the network's); streams and
  hollows away from players.
- **6c, the graph growing.**
  - A lake over its spill by another block covers what that level reaches
    (`Basin::rise`: across ground at its crest's height or over it, never
    down the far side), and its ways out are found again from there.
  - A stream runs into what it meets: a river's channel carries it on from
    where it joins, downstream only (`river_joins`, by how far along the
    river's run: a lake whose stream rejoins its own river below its dam
    doesn't feed itself); another lake's water feeds that lake (`fed`).
  - An edit across a stream's way re-routes it (its columns are its lake's
    `touch`).
  - A hollow's basin finds its way out where its water meets other water at
    its level (`Basin::find` with `water`), not only ground lower than its
    floor: a pit by a river spills into the river at the river's level (it
    had flooded 20 000 columns looking for ground lower than its floor).
    Hollow lakes keep their generation through a save.
  Headless: a pit dug across a dammed river's stream; the stream ran into
  it and it became a 22-column lake, which filled in ~100 s at the stream's
  ~1 m³/s, found its way out where it met the river, and poured on by 4
  streams into it. Risen a block over its spill, the dammed lake found the
  pit lake lower than its crest and poured into it, settling where that
  passed its 3 m³/s.
  Not yet: streams and hollows away from players are routed over
  terragen's ground but only written where loaded (as lakes are); a lake
  isn't read by `core.terra.lake` where another's stream lies on it.
