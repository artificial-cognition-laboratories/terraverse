---
title: Server
description: Writing the server half of a Terraverse mod. The universe's point of view, where you say what things are, react to what happens, and every decision is made.
---

# Server

The server half is the universe's point of view. It's where you say what things are, react to
what happens, and make every decision that matters.

```ts
// server/main.ts
content.blocks.create("kilns:kiln", {
    matter: "kilns:fireclay",
    texture: "kiln.png",
    on: {
        use({ player, world, pos }) {
            if (world.matter.temperature(pos) > 600) throw new Refused("Too hot to open.")
            openKiln(player, world, pos)
        },
    },
})
```

## No "you"

There's no `player` here, because there isn't one. A server might have nobody on it, or a
hundred people. Players are handed to you by whatever they did:

```ts
game.players.on("join", player => …)                     // someone arrived
on: { use({ player, world, pos }) { … } }            // someone used your block
game.commands.create("kiln", { run({ player }) { … } })   // someone typed /kiln
client.on("bellows", ({ player }) => …)              // someone's client asked
```

`game.players` is everyone connected, for when you need them all. But a mod that thinks in "the
player" works alone and breaks with a friend, so the API never offers one.

The same goes for worlds. A universe has several planets, so events hand you the `world` along
with the `pos`, and a position on its own means nothing.

## Declare first

Most of a good mod is declarations: blocks, items and substances that say what they are.
Behaviour follows from that without your code. A granite brick doesn't need a handler to be
heavy, to melt, or to break into rubble under a pick.

Reach for a handler when something should happen that the universe wouldn't do on its own:
a kiln that only opens cool, a player greeted, a door that answers to a key.

## React, never poll

You don't loop over blocks to find what changed. You say what you care about and you're told:

```ts
content.reactions.create("kilns:clay_fires", {
    /* … */
    on: { reacted: ({ world, pos }) => world.set(pos, "kilns:brick") },
})
```

The engine already knows when clay reaches temperature, because it's simulating the heat. A mod
that checked every kiln every tick would be doing that work twice, slower, and everywhere at
once. A mod that subscribes costs nothing until it matters.

## The authority

Whatever the server half decides is what happened. A player's screen can ask; only this side
answers. So every rule that should hold, *you can't open a hot kiln*, *only builders place this*,
lives here, where nobody's client can skip it.

```ts
panel.on("stoke", () => {
    if (!world.matter.contents(pos).has("charcoal")) throw new Refused("No charcoal.")
    stoke(world, pos)
})
```

A [`Refused`](/docs/mods/api/types#refused) is an answer, not an error. It stops the action and
shows the player why. Anything else thrown is a bug, reported with your mod's name and line.

## Talking to clients

Beyond panels, your server half talks to its own client half with messages declared in
[mod.config.ts](/docs/mods/structure/config#messages):

```ts
client.on("bellows", ({ player }) => pump(player))       // a client asked
client.send(player, "crack", { pos })                     // tell one player's screen
```

What arrives has already been checked against the declaration, so the payload is the shape it
claims to be. Whether the player is *allowed* to do what they're asking is still yours to decide.

## Near and far

Only the land near players runs **live**. Further out, the world runs settled: a kiln left
alone finishes firing when it would have, and its handlers fire then, not tick by tick. Your mod
sees the same events either way, so a kiln on the far side of the planet behaves like one at
your feet, without costing like one.

## Time

The universe keeps its own time, which runs whether or not anyone's watching:

```ts
game.time.after({ minutes: 10 }, () => …)     // this run only
world.calendar.on("dawn", () => …)                 // each dawn, on this planet

game.time.schedule("cooled", { hours: 6 }, { world: world.name, pos })   // kept across restarts
```

`after` and `every` hold a function, so they last for this run. Anything that must happen even
if the server restarts is **scheduled**: saved by name with its data, and handled by whatever
your mod registers for that name on every load. See [time](/docs/mods/api/game#time).

## Keeping things

Anything your mod needs to remember goes in a store: [`mod.store`](/docs/mods/api/mod#store)
for the whole universe, `world.store` for one planet, saved as it's written. Variables in your module are gone on the next
boot; the store isn't.

## Settings

```ts
const heat = mod.settings.kilnHeat   // typed from mod.config.ts, chosen by the server
```

Settings are the server's choices about your mod, fixed while it runs. Read them anywhere,
including at load, to decide what to register.
