# Sprint 81 — The liquid layer

Liquids leave the block grid. A block holds one material, so water couldn't
enter a sculpted block's empty sixteenths (a pit in the sand stayed dry, the
sea left a gap along its sculpted shore), and a level change swapped a
block and remeshed its map chunk, so water stepped instead of running.

## Plan

1. **Fills, beside the blocks.** A map block keeps its fills as it keeps its
   cut nodes' shapes (`fill::Fills`, serialization version 31): per node, a
   liquid (substance index), how full its open space is (0..255) and a
   colour (the mixture's). Any node with open space can hold one: air, or a
   sculpted block's empty sixteenths. The sand stays sand.
2. **The world's matter runs against the open space.** A block's room for
   liquid is its empty volume; its surface is found by filling the empty
   sixteenths from the bottom (per-layer counts); liquid passes between
   neighbours through the open part of the face they share. Asleep, a
   block's liquid is its fill (`Ground::fill` / `setFill`). The pipes stay
   per block: the same cost as now.
3. **Its own renderer.** The server sends fill changes (`TOCLIENT_FILLS`,
   protocol 86) without touching the block; the client keeps the fills and
   draws liquid as its own mesh per map block (`client/liquids.*`): the
   surface over the open columns of each node, sides where it meets air,
   heights gliding toward each update, coloured by what's in it. Map chunks
   are never remeshed for water.
4. **The world generator** fills sculpted shore and riverbank blocks below
   the water's level, so the sea meets the shore.
5. **Swimming and wading** read the fill: deep enough at the head swims.
6. **The levelled water blocks go:** `water_puddle` and every `_puddle`
   become fills on load (an LBM), the full blocks stay as the sea's (cheap,
   meshed as now) until the layer draws them too.
7. **Lab:** a pit dug in sixteenths fills; the shore has no gap; water
   runs visibly down a sculpted channel.

Powders on the same layer follow (they share the code), then waves.

Not doing: per-sixteenth fluid simulation. Water is an amount per block;
only its drawing and its room know the sixteenths.

## Outcome

**Fills** (`src/fill`): a map block's `m_fills` (map format 31, after its
shapes; by liquid name, so the matter table can grow): per node a liquid,
how full its open space is (/255) and a colour. `Map::getFill`/`setFill`
(a `MEET_FILL` event; the server sends `TOCLIENT_FILL`, protocol 86).
`Map::solidShape(p)` gives a node's solid sixteenths where it's partly
there: cut, smoothed loose ground (its `ground::Top`), or the air a
smoothed block below rises into.

**The world's matter** runs liquid in open space: a block's room is its
empty sixteenths (`Ground::room`, cached per awake block for 5 s), its
surface is found filling them from the bottom (`Room::surface`), liquid
passes only through layers open on both sides of a face (`Ground::face`)
and falls only through an open floor into an open top. A liquid is never a
block now but full (where it has one: the sea's `water_source`); anything
less lies in the air (or what melted, ice and tin, becomes the air its
liquid lies in) as a fill, written by sixty-fourths four times a second
while it runs and exactly when it sleeps. Asleep, a block wakes with its
fill's liquid. A puddle runs onto dry ground where it can fall or carry on
to somewhere lower (water finds the pit); only liquid resting on something
runs sideways; mist (under a tenth of a litre in the air) hangs.

**The client's liquid layer** (`client/liquids.*`, a scene node in the
transparent pass): per map block a mesh of its fills, the surface over the
node's open columns at the height its liquid stands, sides where it stands
above what's beside it, lit by the node's light, in the mixture's colour,
each level gliding to its fill (a quarter-second ease). Map blocks are
never remeshed for liquid. Swimming reads fills (`LocalPlayer`).

**In the game:** `core.matter.set_fill`, `core.matter.get_fill`; the
generator fills smoothed loose blocks beside water at its level
(`tf_world/shore.lua`, on generated); old puddle blocks become fills on
load (an LBM); the water-spill scenario reads fills.

**Tests:** `testPitFills` (a pit cut a quarter deep beside a pour takes
all of it; the granite stays granite), and the flow tests on fills.

**Run:** engine build; TestSandbox, TestMatter, TestShape, TestMapBlock;
headless boot; Lab check 3/3; wiki 51/51; live fire check.

**Not run:** the game client — the renderer, swimming in fills and the
shore fill on a generated terra world have only been built, not seen.

**Not done:** powders are still levelled blocks (the next step on this
layer); the sea's full blocks are still meshed as blocks; fills don't flow
through shapes' interior walls in detail (a face is open where any column
along it is open at that layer); waves.
