# Gas, fire and flow

Gas has momentum, fire travels as a front, liquids flow by their viscosity, and
explosions push. None of it simulates the whole atmosphere: we simulate only
what differs from the air, at the cheapest scale that behaves correctly
(`simulation-scales.md`).

## Where we are

- Gas moves by `mix` (a share traded with neighbours) and `balance` (gas a block
  can't hold leaves in one step). No pressure field, no velocity, no inertia, so
  a reaction making hot gas just vents, and nothing can blast.
- Fire is only Arrhenius. A premixed gas burns as soon as it's hot: methane goes
  in one frame (open since sprint 104).
- Liquids flow by virtual pipes, but every liquid flows alike: no viscosity.

## The scales

| Scale | Model | Cost | Tier |
|---|---|---|---|
| The sky | a formula: the body's air, pressure by height, the weather layer's temperature and wind | zero | never stepped |
| A room or cave | a zone: one volume, pressure, gas, a hot layer under the ceiling and cool air below | one node per room | Settled |
| Near a flame or leak | cells: per-face flow with momentum | only where something's happening | Live |
| Plumes, blasts, leaks to vacuum | calculated directly | per event | — |

Handovers between scales conserve matter, energy and momentum, as every tier
handover does.

### 1. The reservoir

The air is known everywhere without stepping it. A cell wakes when its gas
differs from ambient by more than a tolerance (composition, temperature,
pressure, speed) and sleeps when it settles back. Today's `Sky` and the room's
air become this reservoir.

### 2. Cells: one flow model for gas and liquid

Each face of an awake cell carries a flow. It speeds up with the pressure
difference across the face and with buoyancy (from density), and it slows by
viscosity. That extends liquids' virtual pipes to gas rather than adding a
second system.

- **Viscosity** belongs to each substance and depends on temperature: lava and
  glass stiffen as they cool, tar runs when warm. The same rule covers honey,
  water and air.
- **Open faces** border sleeping air and lead to the reservoir. Outflow is
  absorbed; pressure pushes back with a realistic resistance, so nothing
  reflects off an invisible box. The weather's wind comes in there.
- **Step size** goes with how fast things move: sub-steps only where gas is
  fast, near a burst.

### 3. Zones: enclosed spaces, lumped

Fire engineers' zone models: a room is one volume with a pressure, its gas, and
a hot layer over cool air, joined to other rooms by openings (doors, vents,
cracks).

- Volumes come from a flood fill when blocks change, and are cached.
- A quiet room is one node, whatever its size.
- Cells run only near activity (a flame, a leak, a door just opened), and the
  cells and their zone exchange matter and energy at their boundary.
- Behaviour you get: firedamp gathering at a mine's roof, smoke filling a room
  from the top down, a stove drawing air under a door, a sealed room choking its
  fire. This is the sealed-rooms design pass AGENTS.md says is still owed.

### 4. Fire as a front

A block's fire spreads to its neighbour only at that mixture's flame speed
(methane in air about 0.4 m/s, hydrogen about 3 m/s), and burns no faster than
fresh oxygen can mix in. Flame speed comes from the mixture and its temperature,
never from a name. This gives fireballs, flashback along a leaking gas trail,
starved flames in closed rooms, and smouldering against open burning, and it
fixes the one-frame burn.

### 5. Shapes for big effects

- **Plumes**: a few blocks above a fire, rising gas goes to a standard plume
  model (how wide and how fast it rises, how much air it draws in). Smoke higher
  up becomes something the weather layer carries.
- **Deflagration** (fast burning that builds pressure) runs in the cells, with
  sub-steps: a gas pocket pops, blows out a door, pushes the player.
- **Detonation** is handled in one go: released energy gives peak overpressure
  and impulse by distance (standard blast scaling), blocked by walls along the
  line of sight, and applied to Jolt bodies and the player. Blocks break once
  structural strength exists.
- **Leaks to vacuum**: flow through the opening at the speed of sound, with no
  grid outside it.
- Nothing is marked "explosive". Black powder, hydrogen and grain dust follow
  from their reaction's energy and how fast it makes gas.

### 6. A budget

There is a cap on awake gas cells. At the cap, the region with the least going
on drops a scale (cells to a zone, a zone to the reservoir), conserving as it
goes. Cost tracks how much is happening, never how much air there is. Results
stay deterministic.

## States in between

Solid, liquid and gas stay the states. Behaviour in between comes from
properties:

- **Viscosity** (above).
- **Yield stress**: mud, wet clay, fresh concrete hold a shape until pushed hard
  enough, then slump.
- **Particles carried in gas**: smoke, soot, ash, flour and coal dust. They move
  with the air, show where it goes, and can burn: dust explosions.
- **Granular**: grains and piles already cover this.

Not doing: plasma, gels, turbulence beyond flow on faces, per-block chemistry of
particle size.

## Visuals

Sprint 104's puffs are drawn at sixteenth-block scale. They drift with the
faces' velocities, so gas stops looking like boxes without any change to how
it's drawn.

## Order

1. **Cells, viscosity and the open face**, proven in the wiki scenes (`Stage`
   runs the same code): a candle plume, honey beside water, a door between hot
   and cold air. *Done: sprint 117.*
2. **Flame fronts**: a gas leak lit from across the room; methane burning at its
   flame speed. *Done: sprint 118.*
3. **Zones**: flood-filled rooms, the hot layer, cells inside a zone; a mine
   gathering firedamp.
4. **Plumes and blasts**: plume handover, blast impulse on bodies and the player.
   Breaking blocks waits for structural strength.
5. **Particles in gas**: smoke, then dust explosions.

Each step adds Lab scenarios with an `expect` and passes `check:wiki`,
`check:moves` (Flow) and `check:matter`, plus a benchmark of awake cells per tick
next to the matter one.

## Open questions

- Do blasts break blocks in the first version, or only push until structural
  strength lands?
- Where do cells end and a zone begin inside a large open cave: is size alone
  enough, or does it need its own measure?
- Network sync: the client draws puffs from the server's matter today. Do face
  velocities get sent, or does the client rebuild them from pressure?
