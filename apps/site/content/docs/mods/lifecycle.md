---
title: Lifecycle
description: When each part of a Terraverse mod runs. The server half loads once and runs for the life of the universe; the client half runs for each player, from joining to leaving.
---

# Lifecycle

A mod lives two lives at once. Its server half loads once and runs for as long as the
universe does. Its client half starts each time a player joins, and ends when they leave.

```
SERVER                                          CLIENT (per player)

load      server/main.ts runs once
          declares content, registers handlers
sealed    no new content from here on
running   handlers fire as the world changes    join    client/main.ts runs
          timers tick in world time                     keybinds, HUD, listeners
          panels open for players  ───────────────────► props in
                                   ◄─────────────────── events out
                                                leave   torn down
stopped
```

## Load

The server runs each mod's `server/main.ts` once, at module scope, in dependency order. There's
no `setup()` to export: **importing the file is loading it.**

```ts
// server/main.ts
content.blocks.create("kilns:kiln", {
    matter: "kilns:fireclay",
    texture: "kiln.png",
    on: { use: ({ player, world, pos }) => openKiln(player, world, pos) },
})

game.players.on("join", player => player.tell("Mind the kiln."))
```

Load is for saying what things are and what you react to. The world isn't open yet, and nobody
has joined, so there's no world to read and no player to greet. `settings` are ready: decide
what to register from them here.

## Sealed

When every mod has loaded, content is **sealed**. Blocks, items and substances are the names
worlds are saved in, so a name that appeared and vanished between boots would leave holes in
the map. Registering one now throws `CONTENT_SEALED`.

Behaviour is never sealed. Handlers, timers and panels can be added and removed for the whole
life of the universe.

## Running

The worlds open and the universe runs. Your mod is now a set of handlers, called when
something it registered for happens:

```ts
game.players.on("join", player => player.tell("Mind the kiln."))
client.on("bellows", ({ player }) => pump(player))
```

Every registration returns a `Disposer`. Call it, and that handler is gone:

```ts
const stop = game.time.every({ seconds: 1 }, () => watch(world, pos))
stop()
```

An `await` resumes on a later tick, and the world moves on in between: a player you were
talking to may have left. Read again what you need after it.

A throw inside a handler refuses that one action: the block isn't placed, the panel doesn't
open. It's reported, the handler stays registered, and the universe carries on.

## A player joins

When someone joins, they're sent the universe's rules: every block's definition, every mod's
assets, and every mod's client half. Then each mod's `client/main.ts` runs on their machine, in
dependency order, before they spawn, so a keybind or HUD piece is ready on the first frame.

From then on the two halves talk: the server opens panels with props and sends messages; the
client answers with events and requests. See [Client](/docs/mods/client).

## A player leaves

Their client half is torn down with everything it registered. On the server nothing stops:
the handlers you registered for them are yours to remove, usually in `game.players.on("leave")`.

## Stopped

A server can stop at any moment, power cut included, so your mod isn't asked to save on the way
out. It doesn't need to be: what it keeps in [store](/docs/mods/api/mod#store) is saved as it's
written. A mod that only saved on exit would lose data the first time the power went.

## Reloading, in development

Under `terra dev`, saving a file reloads the mod. Every handler, timer and panel from the last
run is disposed first, so nothing piles up. A changed block definition applies live; adding or
removing one needs a restart, and the reload says so rather than half-applying it. Client
halves reload on every connected player's screen.
