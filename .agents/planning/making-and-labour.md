# Making and labour

How a player goes from a raw planet to carts, mills, planes and rockets without the game
becoming CAD in a voxel world, and without hundred-step crafting chains. Settled in
conversation, 2026-11-23; it refines "Making and moving" in AGENTS.md, which stays the summary.

## The question

We want creative engineering freedom: what a machine is made of, and how it's shaped, changes
how it behaves. But a player who must design every bolt is doing CAD, and a player who crafts
item from item a hundred times is doing chores. Where does the engineering live?

**Answer: the engineering isn't at the crafting table, and never should be.** Kerbal Space
Program is the proof: nobody designs a turbine blade, yet everyone feels like an engineer,
because the parts are given, the assembly is free, and physics is the judge.

## Three tiers

| Tier | What it is | How it's made | Where the engineering is |
|---|---|---|---|
| **Part** | Hand-sized form × material: axe head, haft, piston, axle, wheel, plank, beam | A process at a station or in the hand: carve, knap, saw, forge, cast | Which material, which process |
| **Unit** | A body with named sockets: a tool, an engine, a transmission, a pump | Parts fitted into its sockets. No wiring, no placement | Which parts, in which sockets |
| **Machine** | Anything bigger than you: cart, mill, crane, boat, plane, rocket | Placed and joined freely in the world, judged by physics (Jolt) | Shape, layout, balance, where the thrust and load go |

- **A station makes parts and nothing more.** A crafting table, a forge, carving by hand: material +
  process → part. That's the one piece of magic we keep ("hand-sized is magical"). Everything
  above a part is visibly assembled by the player.
- **A tool is a unit.** Head, haft, binding. Flint on oak bound with sinew is cheap, light and
  breaks; steel on ash is a good axe. Nothing is a "diamond pickaxe" recipe: any material fit for
  a head makes a head.
- **An engine is a unit.** The block's material (aluminium: light, wears; steel: heavy, lasts) and
  its sockets: cylinders, displacement, a turbo or compressor, a transmission (six-speed,
  titanium), a starter. Its power, mass and wear come from what's fitted. You never design its
  insides; you decide what it's capable of. Pumps, nozzles, batteries are units the same way.
- **Sockets stay few and named.** Five sockets is a decision; forty is the CAD we're avoiding.
- **A machine is free.** Wing shape, the material of the frame, where the engine sits, the centre
  of thrust: the player places them, and the world says whether it flies.

## Rules

1. **A step exists only if it's a decision or a place.** Smelting needs a furnace and heat: a place,
   kept. Bar → rod → axle with nothing to choose between: collapsed to iron → axle, at an anvil.
2. **Eras are new processes, not longer chains.** Casting makes complex shapes; rolling, sheet; the
   lathe, precision. Each unlocks forms or raises their quality.
3. **Precision is one number:** the process that made a part. Hand-forged is loose, a lathe tight.
   Tighter parts mean less friction, higher revs, lighter builds. It's the only "quality" there is.
4. **Properties are derived, never authored.** A part's mass, strength, hardness and wear come from
   its material (`assets/matter`) and its form. A bronze and a steel axe are one form.
5. **What can't be made is shown greyed, with why** ("needs a lathe", "needs 1500 °C"). The station
   teaches the chain, so a rocket is reached by intuition, not a wiki.
6. **Failures explain themselves.** An axle that shears, an engine that overheats, a wing that
   stalls: the world shows what broke and the reason is readable.

## Making small things: the grid

Hand-sized parts and tools are made in the inventory's 2×2 (a bench's 3×3 for more inputs), and
the rule is one line: **what's in the grid is what's worked, what's on the cursor is how, and what
it could become is offered as icons.** Nothing carried is bare hands; a hard stone strikes, a blade
cuts, cord binds. Joining a head and a haft is the same: both in the grid, cord on the cursor, an
axe offered. Slots' order never matters (no shapes to memorise). One wide, scrolling, no words: the
tooltip has the name, mass and time; an offer the cursor can't make is dim, badged with what to
hold. Engineering a tool is shelved: a swung tool aims for the same thing whatever it's made of,
so choosing its parts is enough. (A feel test runs in the page: `game/making/`.)

## Members and joining

The engineering lives in the assembly, not in making parts. A first sprint had players draw every
piece in 3D, a bed panel here, a grid of aluminium there: freeing, but nothing complex was
practical, and nothing made was a real part. Now:

- **Members** are the big parts: pole, beam, plank, post, axle, wheel, later truss, panel, rail.
  Form × material as any part, in **standard sizes** (a beam 1, 2 or 4 m; a wheel 0.5 or 1 m
  across), so they stack in an inventory, fit together and read at a glance.
- **Made at a station, or where they lie.** A sawbench turns a log into planks, beams and posts; a
  wheelwright's bench makes a wheel; the same grid-and-cursor rule, a bigger grid. Carryable ones go
  to the inventory by mass (a 2 m oak beam ~40 kg: slow, fine) and are placed from the hotbar as
  physical bodies; ones too big to carry appear beside the station, to be hauled.
- **Grade, not size, is how well it was made**, and it shows on the model:

  | Grade | Made by | Mass | Joints |
  |---|---|---|---|
  | Pole | felling: a trunk or branch, bark on | heavy | lashed only, weak |
  | Hewn | an axe, on the ground: the log is its own bench | heavy, oversized | pegged, fair |
  | Sawn | sawbench, sawpit | right | good |
  | Milled | a powered mill | lightest for its strength | best |

  So the early game needs no workbench: fell, hew, peg, build. Better stations make better
  members faster.
- **Joining: hold a fastener, click where two members touch.** The fastener sets the strength
  (cord, pegs, nails, bolts, welds: the eras), the parts set the motion (a hub on an axle spins, a
  door on a hinge fitting swings, anything else is rigid). Joints fail honestly: fracture already
  reads tension, compression and toughness.
- **Assembly happens in workpiece mode** (built: F on a piece lifts it over a blueprint grid, it
  floats while you orbit it and fetch more). Members placed there stay put; released, whatever is
  joined becomes one machine and physics takes it. A missed joint falls off.
- **Cutting is the language for making members.** Hold F on a log (a boulder, an ore face): a
  radial menu, its inner ring the category (beam, plank, post, round, firewood), its outer the
  standard size. Each left click then peels one member off into the inventory and the log
  visibly shrinks (it's a piece of sixteenths: what's taken is exactly the member's volume; what's
  left over falls as chips). The mode stays with the tool until changed: twelve planks are twelve
  clicks. What's in hand sets the grade (an axe hews, a saw saws); a sawmill is the same verb on a
  conveyor. Metals the same at a station: a bar or plate in, I-beams, trusses, pipes, sheet out, a
  fixed vocabulary. Grade reads as a colour on the item's corner (greys, greens, golds: never the
  ocean, the brand's), saying how it was made, not a rarity.
- **Houses can be members too:** a pegged timber frame, blocks as its walls. Span, load and bracing
  matter; it's the builder villagers' first job.
- **The 3D editor stays** for creative and blocks, and maybe for carving a one-off member later.

A cart, start to finish: fell an oak → hew two 2 m beams, split pegs → (sawbench) planks →
(wheelwright) two wheels and an axle → in workpiece mode, beams, planks across, pegs at each
crossing, axle under, wheels on its ends → release, and it settles onto its wheels.

## Moving things

Distance is what costs; building at home stays easy. Capacity is earned by building, out of
basic materials, and every step is the world working as it does:

| Means | Rough capacity | Built from |
|---|---|---|
| By hand | ~50 kg, slow | nothing |
| Horse and cart | ~1 t a day | wood, an animal |
| Cart on roads, with bridges | ~4 t a day | cleared forest, packed earth, cut stone |
| Rail, boats, later engines | far more | the next eras' processes |

- The inventory holds mass; weight slows you, never refuses. 50 kg from a mine is easy, a
  kilotonne is a project.
- **Roads matter mechanically:** ground sets rolling resistance and speed (mud, packed earth, cut
  stone), and a bridge saves the detour. It's a property of the ground, not a new system.
- **The first cart comes early**, within the first hour, so hauling is the first engineering
  problem rather than a punishment.
- **Animals are the first engines.** Taming and harnessing come before machines.

A world's author can turn any of this down or off: an FPS played in the universe doesn't need it.

## Labour before machines

The progression is people, then machines. Early on you multiply yourself with people; machines
come when people can't give the rate, precision or power.

- **Villagers live in your settlement because you built for them** (housing, food), and take jobs:
  farmer, builder, hauler, merchant, defender. Not clever, far beyond a Minecraft villager: they
  follow you, work their trade, bring back ore they spot, take a few simple orders.
- **They're the runners of the two automation primitives** already in the design. A builder follows
  a placed blueprint, fetching from nearby stock; a hauler runs a route; a farmer runs a pattern
  on a field. The engine supplies the primitives; who runs them is a mod's policy.
- **They bring stakes.** Food, housing, defence, trade. Raiders kill two farmers, famine threatens,
  the merchant who trades with the next village leaves. That's a survival game's middle act,
  with no quest system.
- This makes the game able to be harder, because the player has more capability to bring.

It's the biggest system here (creatures, pathfinding on generated land, needs, jobs, talking to
them), so it comes after the cart, starting with one job.

## Order

1. **Trees and vegetation done properly.** Species, density from rain and soil (desert, scrub,
   woodland, forest, jungle), a vertex budget that holds. The raw world, and the first material.
2. **Parts and materials.** The catalogue of materials and their mechanical properties, the
   catalogue of forms, and the processes that make one from the other (below).
3. **Tools as units; breaking by tool × material.** Granite by a pick is rubble, by a chisel a cut
   block; a tree by an axe is logs.
4. **The cart slice.** Logs → planks, axle, wheels at a bench → a cart joined in the world → a pine
   cart is lighter and carries less, an oak axle survives the load → it hauls ore faster than
   carrying it. If this is fun, a plane is the same with more parts.
5. **Animals pull it.**
6. **Villagers as labour**, one job first, on routes and patterns.

## The first step: parts and materials

What we have: `assets/matter/substances.json` holds 78 substances, each with full thermal data, but
mechanical properties are sparse (`hardness` on 16, `strength` on 6, `friction` on 3), and `wood` is
one substance. `rocks.json` makes rocks from minerals. Machines already have parts and joints
(`src/piece`, `src/workshop`, sprints 47–77) as a fixed library.

To establish:

- **Materials:** which substances are workable materials, and the few mechanical properties every
  one needs (density, hardness, strength in tension and compression, toughness, stiffness, how it
  works: brittle, ductile, fibrous, plastic). Woods as species (oak, pine, ash, …), not one `wood`.
- **Forms:** the first catalogue of part forms, each saying which property makes it good (a head
  wants hardness and toughness, a haft toughness and low mass, an axle strength).
- **Processes:** which work which kind of material (knap: brittle; carve: fibrous; forge: ductile
  and hot; cast: molten), and where (hand, bench, forge, kiln).
- **Units:** the socket layout of the first units (axe, pick, chisel, cart wheel) and how a unit's
  numbers come from its parts.
- How today's piece library becomes forms × materials rather than a fixed list.

## Still open

- Where joints may land: anywhere two members touch, or at fixed points along them.
- Members grow out of the workshop (src/server/world/run/machines, read 2026-11-23): a member is
  a machine's part whose piece is generated from form × size × material (as starters.cpp builds
  wheels), workpiece mode is the assembly space, sockets and plug are a wheel onto an axle, the
  library keeps frames. New: generating members from a table, a fastener on each link with joints
  that fail (fracture judges one-part machines only), and cutting. motion.cpp (1069 lines),
  machine.cpp (917) and workshop.cpp (721) are past the limit: each new concern is its own leaf,
  and Workshop's ~25 verbs want splitting before this builds on it.
- Which members and sizes the first set has.
- How electricity enters: battery blocks, wires, what a circuit can express.
- What villagers understand, and how you talk to them.
- Difficulty, and how a world's author tunes it.
