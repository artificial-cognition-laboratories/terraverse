---
title: game
description: "What's being played. The game's worlds, its players, the commands they can type, its clock, and its star system."
---

# game

What's being played: its places, its people, what they can type, and its clock.

```ts
interface GameApi {
    readonly worlds: Worlds        // the planets, one World each
    readonly players: Players      // who's here, one Player each
    readonly commands: Commands    // what they can type
    readonly time: Time            // the game's clock, and timers
    readonly system: StarSystem    // the star the worlds share. not designed yet

    // A line to everyone connected.
    announce(message: string): void

    // Record the worlds as the starting state, or go back to it, without stopping.
    snapshot(): Promise<void>
    reset(e?: { worlds?: readonly WorldName[] }): Promise<void>

    // Save everything, tell everyone, and stop the server.
    stop(reason: string): Promise<void>
}
```

A server runs one game: a universe of worlds, under one set of rules. `game` is that universe
while it runs. What things *are* was settled before it started, in [content](/docs/mods/api/content);
`game` is what's happening.

```ts
game.players.on("join", player => player.tell(`Welcome to ${player.world.name}.`))

game.commands.create("kilns", {
    description: "How many kilns have fired, everywhere",
    async run({ reply }) {
        reply(`${(await mod.store.get("fired")) ?? 0} firings.`)
    },
})
```

**[worlds](/docs/mods/api/game/worlds)** · **[players](/docs/mods/api/game/players)** ·
**[commands](/docs/mods/api/game/commands)** have pages of their own. The clock is here.

## time

```ts
interface Time {
    readonly now: number            // s of game time since it began

    // In memory, for this run. Gone if the server restarts.
    after(d: Duration, fn: () => void): Disposer
    every(d: Duration, fn: () => void): Disposer
    wait(d: Duration): Promise<void>

    // Move the whole universe forward, settled: crops grow, kilns finish, carts arrive.
    skip(d: Duration): Promise<void>

    // For labs and admins: stop the clock, run it one tick at a time, or faster.
    pause(): void
    resume(): void
    step(ticks?: number): Promise<void>
    rate: number                    // 1 is real time. ignored while paused

    // Durable: saved, and kept across restarts.
    schedule<K extends keyof Schedules>(name: K, when: Duration, data: Schedules[K]): void
    on<K extends keyof Schedules>(name: K, fn: (data: Schedules[K]) => void): Disposer
}
```

Game time runs whether anyone's there or not, and stops only when the server does.

### Skipping, never setting

There's no way to set the time of day. With a real sky, the time of day *is* where the planet is
in its turn, and setting it would tear it out of its orbit and away from every other world. `skip`
is the honest version: the whole universe moves forward together, settled, so a night skipped is a
night that passed. The crops grew through it.

A skip moves at most 24 hours of game time at once, and resolves in the first window past it. To
skip to a moment, ask the [calendar](/docs/mods/api/game/worlds#calendar) how long until it, at a
place. Today the sky, the weather and the rivers follow a skip, and timers come due; blocks near
players don't yet live through the time skipped.

### Pause, step and rate

For an experiment, or an admin. A paused game holds everything still: no physics, no matter, no
timers. `step` runs exactly as many ticks as you ask, and `rate` runs faster or slower than real
time, as far as the machine keeps up.

`after` and `every` hold a function, and a function can't be saved, so they last for this run.
Anything that must happen even across a restart is **scheduled** by name, with data, and handled
by a handler your mod registers on every load:

```ts
// at load, every time
game.time.on("cooled", ({ world, pos }) => {
    game.worlds.get(world).set(pos, "kilns:kiln")
})

// when a firing ends
game.time.schedule("cooled", { hours: 6 }, { world: world.name, pos })
```

Schedules are declared in [mod.config.ts](/docs/mods/structure/config#schedules), with their
data, so they're typed and checked as messages are. Like messages, their names are your mod's own:
`cooled` can't collide with another mod's.
