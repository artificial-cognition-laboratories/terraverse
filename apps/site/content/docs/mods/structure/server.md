---
title: server/
description: The half of a mod that runs the world. server/main.ts runs on the server, at module scope, once when the world loads.
icon: vscode-icons:folder-type-server
---

# server/

The half of your mod that runs the world. `server/main.ts` runs on the server, at module
scope, once when the world loads.

```ts
// server/main.ts
content.blocks.create("my_mod:granite_brick", {
    matter: "granite",
    texture: "granite_brick.png",
})

game.players.on("join", player => {
    const melts = content.substances.get("granite").melts
    player.tell(`Granite bricks melt at ${melts} K. Mind the lava.`)
})
```

There is no `setup()` and no default export. **Importing the file is loading it:**
`content.blocks.create(...)` runs as the module body evaluates.

Notice what isn't there. The brick has no hardness, no weight and no melting point: it is
granite, so it has granite's. Heat, breaking and the far view of a wall built from it all
follow from `matter`, the same as for every block the game ships.

## Content first, then behaviour

```
each dependency's server/main.ts  →  yours  →  the world opens  →  players can join
```

Mods load in dependency order, so a mod you build on has registered everything before your
first line runs.

Once every mod has loaded, **content is sealed**. Blocks, items and substances are the
names a world is saved in, and a name that came and went between boots would leave holes
in the map. Registering one later throws `CONTENT_SEALED`.

Behaviour isn't sealed. Handlers, timers and panels can come and go for the whole life of
the world, and every one returns a `Disposer`:

```ts
const off = game.players.on("join", greet)
off()   // and it is gone
```

## Split it up

```ts
// server/main.ts
import "./blocks"
import "./kiln"
```

An ordinary import. Anything reachable from here runs, which is how a large mod stays
navigable. Code the client half needs too belongs in a folder of its own, like `lib/`, which
both halves can import.

## A throw at load stops the world

```ts
content.blocks.create("my_mod:granite_brick", { matter: "granit" })
// MATTER_UNKNOWN: "granit" (my_mod/server/main.ts:1). Did you mean "granite"?
```

The world doesn't open, and the error names the mod, the file and the line. A world that
opened with half a mod loaded would generate and save terrain without the missing half,
and that damage outlives the bug.

A throw inside a handler, once the world is running, refuses just that action: the block
isn't placed, the panel doesn't open. It's reported in-game to anyone allowed to see
errors and in the server log, and the handler stays registered.

## Await is fine, but the world moves on

```ts
game.players.on("join", async player => {
    await game.time.wait({ seconds: 5 })
    player.tell("Still here?")   // the player may have left: this throws PLAYER_GONE
})
```

An `await` resumes on a later tick. Anything you read before it may have changed, so read
it again after.

## Reloading

Under `terra dev`, saving re-runs everything: every handler, timer and panel from the
last run is torn down first, so nothing accumulates. A changed block definition applies
live. A block added or removed needs the world restarted, and the reload says so rather
than half-applying.
