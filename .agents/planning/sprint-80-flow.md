# Sprint 80 — Flow

Liquids and powders that run, pour and evaporate, on the world's matter
(`src/sandbox`), and the world's water taken off Luanti's liquids.

## Plan

1. Liquid and powder levels per block, flow between blocks, levels drawn.
2. Open surfaces: evaporation into the air; vapour that burns at a flame.
3. Still pools sleep; a per-step budget, busiest first.
4. Pouring at a rate (a pour key), a placed liquid block poured out.
5. The sea and lakes as sleeping matter, replacing Luanti's water.

## Outcome

**Flow** (`sandbox/pouring.cpp`). A block holds an amount; the world draws
its level in sixty-fourths (levelled nodes, `paramtype2 = "leveled"`).

- Liquids fall into space below, then run sideways by virtual pipes (Mei,
  Decaudin & Hu 2007): a flow per side, sped by the difference in surface
  height, kept between steps, damped (2/s). Substeps of 0.1 s. A liquid
  spreads no thinner than a sixteenth (a puddle). A full block's surface is
  the top of the column on it, so a U-bend levels.
- Powders (anything poured, or a `loose` kind) fall and slide where steeper
  than their angle of repose (0.7 rise over run, one for every powder for
  now); denser than the liquid under them, they sink through it.
- Evaporation from an open surface by vapour pressure (Raoult against the
  air's share), taking its latent heat from the liquid, into awake air over
  it (a flame, a cloud) or the block's own air. Enough vapour (0.02 mol/s)
  wakes the air it rises into. A cloud of vapour that isn't burning is
  carried off 25× faster on its open faces (wind). A pond at the weather's
  temperature under asleep air doesn't evaporate: drying by the day belongs
  to the Settled tier.
- Awake air over an asleep liquid wakes it, so a torch over spirit lights it.

**Sleep and keeping** (`sandbox/world.*`). A block whose matter isn't just
its kind's (a puddle over its drawn level, brine, a heap, a crucible's
contents) keeps its mixture while asleep (`Held`); a still pool costs
nothing. Asleep liquid isn't woken by a difference under 1/128 of a block,
so a cup into the sea stays where it's poured. Saves are version 4: each
block's amounts by state, whether it's loose, its pipes' flow and owed
time, and the held blocks.

**Budget.** Past `budget()` (8192) awake blocks, a step runs the busiest
(reaction heat, flow, distance from the weather) and the rest owe their
chemistry's time, made up later (to a second). `MOST_AWAKE` (65 536) now
leaves a block asleep instead of throwing.

**Drawing.** A running level is redrawn by sixteenths and at most twice a
second (each redraw remeshes the client's map block); asleep, exactly.

**In the game.**
- Every substance has a powder (`_powder`) and a puddle (`_puddle`) beside
  its block and full liquid; descriptions give a block's weight.
- `tf_world:water_source` is the world's matter's full water (drawtype
  liquid, no Luanti liquid, swum through); `tf_world:water_puddle` its
  levelled water (waded); `water_flowing` is an alias of it.
- `core.matter.pour(pos, id, moles[, T])` (heaps and runs) and
  `core.matter.disturb(pos)` (dug or built: what can run there wakes); Lua
  calls disturb on every dig and place.
- The pour key (X, `keymap_pour`; `control.pour`, bit 10, protocol 85):
  held with a substance block, the scoop or water, it pours a cup on a tap,
  then a stream that doubles twice a second to a sixteenth of a block a
  second; a block item empties a cup at a time (creative: never).
- A placed block of liquid pours out over 2 s (molten ones just over their
  melting point).
- A seat facing up or down is no longer the driver's seat (it stopped the
  whole machine moving).

**Tests** (`test_sandbox.cpp`): a puddle spreads and nothing's lost; a
ditch fills level; salt heaps; boiled spirit's vapour leaves (11% returns
as dew in a walled hole, from 41%); spirit under a flame catches; a pool
sleeps, keeps its matter exactly and saves; a budget of 4096 over 27 000
awake blocks costs ~24 ms. The ice and tin tests got walls: what melts runs.

**Run:** engine build, all unit tests (TestAtlasRender's two planet-view
tests fail as before this sprint), UI typecheck, headless boot, Lab check
3/3, wiki check 51/51, the live fire check. The user ran the game: flow
smooth, pouring works, ethanol under the torch evaporates and catches.

**Not done, and why:**
- Liquids can't enter sculpted blocks (a block is one material): the pit in
  the sand stays dry and the sea leaves a gap along its sculpted shore.
  Sprint 81's liquid layer.
- Levels step rather than run on screen, and a mixture is drawn as its main
  liquid: also the liquid layer (its own renderer, colour per block).
- A crash in Chromium (SIGILL in libcef, 10:15 on 31 Oct) while digging
  under a puddle: not reproduced; the matter log shows nothing.
- Powders' angle of repose is one number for all; puddles don't dry.
