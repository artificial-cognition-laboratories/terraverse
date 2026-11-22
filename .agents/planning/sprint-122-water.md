# Sprint 122 — Water, sane

Water is one of the game's main costs and it doesn't yet behave. Rivers are staircases of whole
blocks, each step held flat, with currents guessed from where the steps are. Three renderers (fine,
coarse, flat) each decide the level for themselves, so seams show and resolutions change as you walk.
Caves carved beside a channel stand dry until the client's picture pours into them as you approach.
And a frame spends 4–17 ms on water (the flat drawing's spikes alone reach 5–6 ms).

This sprint makes water a sound engine mechanic before anything is built on it: rivers that run
downhill on a sloped surface, water that answers what passes through it, and a frame cost that's
small. We're not constrained by what exists: whatever doesn't fit the design below is removed, not
adapted.

## The one idea

**Water is a base plus a disturbance.** We never simulate a body of water. Every water column has a
*base*, what it is when nothing's touching it: its level and its velocity. The base comes from
whatever owns that water:

| Water | Its base |
|---|---|
| A river | The river surface: a pure function of terragen's bed and the network's flow there now |
| The sea | Its level (tides later, as a function of place and time) |
| A lake (generated, dammed, a filled hollow) | Its level, kept by the lake code (`rivers::Basin`, the weir law) |
| Water placed or poured | The server's fill amount in the node, as now |

The client simulates only the *disturbance*: how far a column is off its base right now (wakes,
splashes, drops, what a body pushes aside). The current carries it downstream and it dies back to
zero. A calm river costs nothing, and every resolution draws the same base, so they can't disagree.

There are two kinds of change, and they never mix:
- **Passing** (a wake, a splash, a swimmer, a drop): the client's disturbance. A picture only.
- **Lasting** (a pier, a weir, a dam, a ditch, a filled hollow): a change to the *base*, decided by
  the server. Never a disturbance.

## Decisions

**The river surface is generated, and the map carries it.** terragen's river level runs down its
run's profile in fixed point; the generator writes it to a sixteenth: full held water up to the
node the surface lies in, and that node's `param2` holds how many sixteenths of it are under the
surface (`fill::PART`, the low bits beside `HELD`; `fill::partOf`). Everything that reads the map
(the client's grid and flat drawing, swimming, the server) sees the same sloped surface, and
nothing new goes on the wire. (First drafted as a function both sides computed with ratios on the
wire; the map was simpler and already shared.)

**A river's rise is its run's.** The weather's flow raises or lowers a river by its ratio
(`rivers::flowAt`, depth ~Q^0.4). The depth it scales is the channel's own mid-stream depth for
its width, the same all down the run, so neighbouring map blocks rise alike (it was one column's
depth per block: steps at block edges). Sent per map block as before (`TOCLIENT_RIVERS`).

**Steep reaches ramp; hard lips fall.** terragen's profiles stay as they are: a steep reach on soft
ground is a smooth ramp at sixteenths (rapids), and a fall over a hard lip stays a step
(`World::ground`'s falls), its plunge pool below.

**Caves below the water table are water.** A cave carved under a river's water level, within
`TABLE_REACH` (32 blocks) of its edge, is generated as held water, under the river's top node. It's
local and deterministic, and every cave by a river is already full when you get there. (By the sea
and lakes: not yet; the column doesn't know how far its coast is.)

**The disturbance** is the existing virtual pipes (`shallow.h`) driven, between two held columns,
by how far each is off its level (`Grid::higher`), so a river's slope at rest is still; carried
downstream across node boundaries, and damped toward its level (a river's slower than the sea's,
so a wake's seen drifting). Tiles sleep when their offset is under a sixteenth's fraction; a calm river has
no awake tiles. Waves can't travel up a reach faster than its water runs down it (fast water carries
everything away), which falls out of the advection.

**One disturbance grid.** A grid at a sixteenth round the camera; the coarse grid goes. Everything
past it is the base drawn directly: a mesh of the surface at sixteenths per map block, built on the
mesh thread when its block arrives or its base changes (a season step, an edit), never per frame.

**Lasting changes re-solve the base locally.** An edit in or beside a river marks a window round it
dirty. The server solves steady shallow water there with friction, at block resolution: flow in
upstream from the network, the base level downstream, iterated to rest on a worker thread. That
covers a pier (water banked on its face, faster round its sides, slack behind) and a weir (deep slow
water behind it, the weir law over its crest, and the standing wave below where the water slows) in
one mechanism. The result is kept as that window's base and sent to players near it. A window that
fills over time (a dam's valley) is a lake: the basin code already does that.

**Performance budget.** A frame's water, standing in a river with the player moving: ≤ 1.5 ms
typical, ≤ 3 ms worst, no spikes from flat drawing. Measured on the Performance panel's Water lanes.

## Phases

1. **Clear the ground.** `client/world/sim/water/liquids.cpp` (1807 lines) splits into a module, its
   door `liquids::Layer` an orchestrator over leaves: following the camera, scanning the map,
   holding to the server, the shallow step, drops, flat drawing and the GPU draw. The coarse grid
   goes (past the fine grid is drawn flat). Otherwise no behaviour change. What holds rivers today
   (`pools`, `hold`, `currents`, `carry`) sits in one leaf of its own, removed whole in phase 5.
2. **The river surface.** `library/world/water`: Manning's depth, the velocity across the channel,
   steps past `STEEP`, from a column's terragen ground and a ratio. Tests in its own `tests/`
   (`bun test:hot water`): a reach's surface falls monotonically downstream, a higher ratio raises
   and speeds it, a step only where the bed is steeper than `STEEP`, the same answer twice in fixed
   point.
3. **Generation on it.** Rivers written to the surface with part-filled tops; the *river* mark in
   place of `HELD`; caves below the water table filled. Version bump, goldens, `bun check:land`.
4. **The base on the wire and drawn.** `TOCLIENT_RIVERS` sends ratios per river cell (protocol
   bump); the client computes the surface and draws it everywhere past the disturbance grid by the
   base mesher. Swimming (`localplayer`) and the server's matter read the base. The far edge's seam
   (the dark line where the sea meets the far view) is closed here too.
5. **The disturbance.** Offset from base, carried by its velocity, damped; the player, objects and
   drops push it. Tiles sleep at rest. The Performance budget is met here.
6. **Lakes on the base.** The lake code (`lakes.cpp`, split as it's touched) gives its lakes' levels
   as their base in place of writing `HELD | LAKE` water; dams, floods and new streams keep working
   (their headless checks pass).
7. **Lasting edits re-solve.** The steady local solver (`library/world/water`, pure, tested: a pier
   banks water upstream and speeds it beside; a weir raises its pool and passes Q = C·w·h^1.5); the
   server runs it for dirty windows and sends the result.

Phases 1–5 are what "sane" means. 6 keeps what we have working on the new base; 7 makes building in
a river right.

## Done means

- Walking along a river from its source to the sea: its surface slopes downhill everywhere, steps
  only at rapids and falls, and runs the way it slopes.
- No seams or resolution changes along the way; caves beside a river are full on arrival; nothing
  settles in front of you.
- A swimmer or a thrown block leaves a wake that drifts downstream and dies away.
- Water's frame cost within budget, seen on the Performance panel.
- `bun check`, `bun check:land`, `bun check:moves`, `bun check:wiki` pass; the game client run and
  looked at.

## Not this sprint

Tides and waves on the sea, boats, dam bursts (a live server sim for a time), erosion, dam strength.
They're built on this base once it's sound.

## Done

- **Phase 1** (2026-11-22). `liquids.cpp` is a module of leaves: `grid` (the columns, following the
  camera), `scan` (the map into tiles), `hold` (pools, the sea, rivers' currents: phase 5's to
  thin), `flow` (the shallow step, what movers push), `drops`, `draw` (GPU), `flat` (per map block),
  `parts` (lighting, quads), `types`; `liquids.cpp` the door. The coarse grid and its setting
  (`water_coarse_reach`) are gone. Built, typecheck, `bun check --changed`.
- **Phases 2–3.** `terragen::waterTop` (`rivers/water_top.h`): a river's and a stream's level to a
  sixteenth, `Ground::water_part`; written into the top node's `param2`. Caves by a river under
  its level are held water. Tests: terragen's `RiversSlopeSmoothly` (201 side-by-side pairs of a
  river's water, every one within its run's grade, none a whole block apart; on a staircase they'd
  all be), `TestWater` (81 river columns' tops hold their part; 407 cave nodes under the table by
  a river are water, none air). `bun check:land` passes. Not bumped to TF25: TF24 has been
  changing since sprint 116 with its golden skipped (`.agents/jobs/terragen-sprint-116-red-tests.md`);
  the bump and rebake are one decision, the developer's.
- **Phase 4, mostly by the map.** The grid's scan, the flat drawing (held water's top at its part
  plus its block's rise, redrawn when the rise changes) and swimming (`localplayer`) read the part.
  The rise is the run's (`flowAt`); `TestRivers` updated for a channel 8 wide being 3 deep.
- **Phase 5, first cut.** Held water runs by its offset from its level; carried across node
  boundaries; a river's relaxes at 0.8/s.
- **Not run:** the game client. None of the drawing, the disturbance or the flat river has been
  seen. Worlds made before this keep their staircases where they're already generated.
- **Seen in the game, then fixed** (the user's look): shallow slabs and sand through steady water.
  The surface was right, the bed wasn't: cut in whole blocks under the surface's node, the water
  sawed between a block and a sixteenth deep, and the smoothed sand bed rose through it. Now:
  - **The bed's cut from the surface** (`World::ground`, streams too): a node only part full puts the
    bed a block lower, so water is its channel's depth to a block more, never a film.
    `RiversSlopeSmoothly` checks it (shallowest water on its river: 16 sixteenths).
  - **Held water is a field, as the ground is** (`held::corners`, `parts`): a node's corner is its
    cluster's mean level (a fall past a block stays a step), a column stands between its node's
    four. The grid's columns (`Grid::relevel`) ramp a sixteenth at a time; past the grid a river's
    top is a quad through the same corners (`Faces::riverTop`), no side against its own river.
    `TestWater`: neighbours' corners meet, a fall stays a step, the grid ramps by one sixteenth.
  - `flat` split: the faces it builds are `faces` (the sea's, a river's, fills').
- **Seen again, then fixed** (the user's second look: slabs in still water till stirred, spills
  rolling down banks on the way down a river, a river's fall bunched in one place):
  - **Water moves with its level** (`Grid::relevel`): a level found again (neighbours scanned
    later, the grid moved, a river risen) carries the column's water with it, keeping its offset.
    It was set only the first time; after, the water stood off its new level and slept there.
  - **A river at rest keeps to its banks** (`Grid::higher`): held water beside unheld ground
    drives a flow only by what's over its level, not by its height over a lower bank.
  - **A river's rise blends between map blocks** (`liquids::riseAt`): each block's rise is read
    from one column's cell, so neighbours differed; it stepped at block edges. Now blended between
    block middles, everywhere it's read (the grid, the flat drawing, swimming).
  - `TestWater`: water moves with its level, a river at rest doesn't spill, the rise blends.
- **Floods held off** (TEMP, `terraFlood`): a flood was a flat lake over its pool, and on a sloped
  river each map block's flooded flat at its own level, boxes of water down the slope (seen in the
  game). Rivers stay at their brim till floods are the river's own surface over its floodplain
  (phase 6).
- **Left:** the far view's edge seam; the flat drawing's cost (spikes to ~5 ms); currents from
  each map block's way interpolated, not stepped per block; the water table by the sea and lakes;
  phases 6–7.
