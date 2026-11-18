---
title: worlds
description: "game.worlds. The planets of the game, one World each, with their blocks, matter, calendar and events."
---

# game.worlds

The planets of the game, one `World` each: their blocks, what those blocks are doing, and their
days and seasons.

```ts
interface Worlds {
    // A world by the name server.config.ts gave it. Throws WORLD_UNKNOWN.
    get(name: WorldName): World
    list(): readonly World[]

    // A temporary copy of a world, as it is now or as it starts: for a match, or a trial.
    instance(from: WorldName, e?: { from?: "now" | "start" }): Promise<Instance>

    // Something happened in any world. See Events.
    on<E extends keyof WorldEvents>(event: E, fn: (e: WorldEvents[E]) => void): Disposer
}
```

```ts
interface Instance extends World {
    // Throw it away: its land, its bodies, and what mods kept for it.
    close(): Promise<void>
}
```

Most code never asks for a world by name: the event that called it hands it one, as
`{ world, pos }`. A game has several, and code that assumed one would be wrong on all the
others.

## Instances

```ts
const match = await game.worlds.instance("arena", { from: "start" })
for (const p of team) p.place(match.spawn, match)
// …
await match.close()
```

Twenty matches on one arena, or a hundred trials of one scenario, are twenty or a hundred
instances of one world, each its own copy, running side by side and thrown away when done. An
instance is a full world while it lasts, and saved like one, so a server restart doesn't lose a
match in progress.

## World

```ts
interface World {
    readonly name: WorldName
    readonly planet: Planet        // radius, gravity, length of day
    readonly players: readonly Player[]
    spawn: Vec3                    // where players new to this world land

    // What's at a position. Generates the land there if nobody has been yet.
    get(pos: Vec3): BlockType | "air"

    // Every position in an area holding a block. Throws AREA_TOO_LARGE past 64³.
    find(area: Area, block: BlockName): readonly Vec3[]

    // Building. Each creates and destroys matter: see Conserving, and not.
    set(pos: Vec3, block: BlockName | "air"): void
    fill(area: Area, block: BlockName | "air"): void
    replace(area: Area, from: BlockName, to: BlockName | "air"): void
    regenerate(area: Area): void   // back to what the world code generates there
    copy(area: Area): Structure    // a build, as data
    paste(structure: Structure | StructureName, at: Vec3, e?: { rotate?: 0 | 90 | 180 | 270 }): void

    // Keep an area running live with nobody near: a farm, a machine, an experiment.
    keep(area: Area): Disposer

    // Decide, before anyone places, breaks or uses anything in an area.
    guard(area: Area, fn: (e: GuardEvent) => void): Disposer

    readonly matter: WorldMatter
    readonly bodies: Bodies
    readonly calendar: Calendar
    readonly store: Store          // what this mod keeps for this world. see mod

    // Something loose on the ground: an item, or a pile of matter.
    drop(pos: Vec3, what: ItemName | SubstanceName | MixtureName, amount: Mass): void

    on<E extends keyof WorldEvents>(event: E, fn: (e: WorldEvents[E]) => void): Disposer

    climate(pos: Vec3): Climate    // not designed yet
    readonly weather: Weather      // not designed yet
    readonly creatures: Creatures  // not designed yet
}

interface Planet {
    readonly name: string
    readonly radius: number        // m
    readonly gravity: number       // m/s² at the surface
    readonly day: number           // s, one turn
}
```

### Conserving, and not

Almost everything in Terraverse conserves: matter moves, burns and reacts, but it's never made
from nothing, and energy and time are kept. The game's rules are built from calls that keep it
that way: `matter.heat`, a player digging, a reaction you declared.

Some calls don't, and say so: `set`, `fill`, `paste`, `drop`, `matter.put`, `bodies.spawn`. They
make and unmake matter at will, which is what building, setting up an arena, running an admin
tool or starting an experiment needs. Use them for those, and the universe stays honest
everywhere else.

### guard

```ts
interface GuardEvent {
    action: "place" | "break" | "use"
    world: World
    pos: Vec3
    block: BlockType
    player?: Player
}
```

```ts
world.guard(spawnArea, ({ player }) => {
    if (!player?.can("core:build_spawn")) throw new Refused("Spawn is protected.")
})
```

A guard runs before any place, break or use in its area, on any block, by anyone, and refuses by
throwing `Refused`. It's how spawn protection, land claims and arena walls are made. Guards from
every mod are asked, and any one refusing stops the action.

A block's own [handlers](/docs/mods/api/content#behaviour-belongs-to-the-block) decide what
happens to that block anywhere; a guard decides what happens anywhere in its area, to any block.

### Structures

```ts
const arena = world.copy({ min: [0, 60, 0], max: [80, 90, 80] })
world.paste(arena, [400, 60, 0])
world.paste("arenas:colosseum", [0, 60, 0], { rotate: 90 })
```

A structure is a build as data: blocks and the matter in them. Copy one from a world, or ship one
as content in `assets/structures/`.

## matter

What the blocks are doing: how hot, what's in them, what's reacting.

```ts
interface WorldMatter {
    temperature(pos: Vec3): number          // K
    pressure(pos: Vec3): number             // Pa

    // What's in a block's open space: water in a basin, ore in a crucible.
    contents(pos: Vec3): Contents

    // Add matter to a block's open space, or take it out. Neither conserves.
    // Throws NO_ROOM if it won't fit, NOT_THERE if there isn't that much.
    put(pos: Vec3, matter: SubstanceName | MixtureName, amount: Mass): void
    take(pos: Vec3, matter: SubstanceName | MixtureName, amount: Mass): void

    // Add heat (positive) or take it away (negative), in joules.
    heat(pos: Vec3, joules: number): void

    // Set a temperature outright, whatever it takes. Doesn't conserve.
    setTemperature(pos: Vec3, kelvin: number): void

    // Light it, if there's anything to burn and air to burn it in.
    ignite(pos: Vec3): void

    // An explosion: this much energy at a point. Physics does the rest.
    release(pos: Vec3, joules: number): void

    // A reaction happened somewhere in this world: one you don't own, or for this world only.
    on(reaction: ReactionName, fn: (e: { world: World; pos: Vec3 }) => void): Disposer
}

interface Contents {
    has(matter: SubstanceName | MixtureName): boolean
    mass(matter: SubstanceName | MixtureName): number    // kg
    list(): readonly { matter: Substance | Mixture; kg: number; phase: Phase }[]
}
```

You never move water or spread fire: the engine does, because it's simulating them. You add
heat or matter and react to what the universe does with it.

```ts
// count fires on this planet: methane_burns is the engine's, not yours
world.matter.on("methane_burns", ({ pos }) => world.store.update("fires", n => (n ?? 0) + 1))
```

For a reaction you declared, put what follows from it in its
[declaration](/docs/mods/api/content#reactions) instead: it fires in every world.

## bodies

Physical things that move: a player's body, a boulder, a crate, a cart, a built machine. Every one
is a rigid body under real physics.

```ts
interface Bodies {
    // A body of a declared kind. Creates the matter it's made of.
    spawn(kind: BodyKind, pos: Vec3, e?: { sensors?: readonly SensorName[] }): Body
    near(pos: Vec3, radius: number): readonly Body[]
}

interface Body {
    readonly kind: BodyKind          // "core:humanoid", a machine, a crate
    readonly world: World
    readonly position: Vec3
    readonly velocity: Vec3          // m/s
    readonly mass: number            // kg, from what it's made of

    // Who's in it, if anyone. A body can stand empty.
    readonly player: Player | null

    // Drive it from the server: an NPC, a test, a cutscene. Only while no player is in it.
    readonly controls?: Controls     // for kinds that can act

    push(impulse: Vec3): void        // N·s
    freeze(): void                   // held still, out of the physics, until thawed
    thaw(): void
    remove(): void                   // gone, with the matter it's made of
}
```

### Bodies and players

A **player** is someone connected: an account, and a client. A **body** is the thing in the world.
A player is always in one body, `player.body`, but a body doesn't need a player.

```ts
// a lab: twenty bodies, waiting for twenty agents
const seats = range(20).map(i => world.bodies.spawn("core:humanoid", [i * 4, 64, 0], {
    sensors: ["sight", "imu"],
}))

game.players.on("join", player => {
    const seat = seats.find(b => !b.player)
    if (seat) player.control(seat)
})
```

So one shape covers a lot. A server spawns bodies and drives them itself, with the same
[controls](/docs/mods/api/client/player#controls) an agent gets: that's an NPC, or a test walking
through your mod. An agent joins and takes one over. A player climbs into a rover, or a spectator
into a camera drone, by changing body. Creatures, when they come, will be bodies driven by a brain.

## calendar

```ts
interface Calendar {
    readonly day: number            // whole days since the world began
    readonly year: number
    readonly season: "spring" | "summer" | "autumn" | "winter"

    hour(pos: Vec3): number         // 0–24, local: it's noon on one side and midnight on the other
    on(moment: "dawn" | "dusk" | "noon" | "midnight", fn: (e: { world: World }) => void): Disposer
}
```

Days and years are this planet's own: its day is one turn, its year one orbit. Two worlds in one
universe can have days of different lengths, which is why days live here and not on
`game.time`.

## Events

```ts
interface WorldEvents {
    placed: { world: World; pos: Vec3; block: BlockType; player?: Player }
    broken: { world: World; pos: Vec3; block: BlockType; player?: Player }
    used:   { world: World; pos: Vec3; block: BlockType; player: Player }
    arrived: { world: World; player: Player }    // landed on this world
    left:    { world: World; player: Player }
}
```

Observers, after the fact: they can't refuse. To decide whether something happens to your own
block, use its [handlers](/docs/mods/api/content#behaviour-belongs-to-the-block).

There's no tick event, on purpose. A mod that did something every tick would be doing the
engine's work again, slower. Subscribe to what you care about, or use `game.time.every` with a real
interval.
