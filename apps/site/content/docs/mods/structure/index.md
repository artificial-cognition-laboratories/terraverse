---
title: Mod structure
description: A Terraverse mod is a folder, and the folder is a TypeScript project with a server half and a client half.
---

# my_mod

A mod is a folder, and the folder is a TypeScript project with two halves: one that runs the
world, and one that runs on each player's screen.

```bash
my_mod/
├── .terra/      # generated types. regenerable
├── assets/           # textures, sounds, models
├── server/
│   └── main.ts       # runs the mod in the world. once, when the world loads
├── client/
│   ├── main.ts       # runs on each player's game, when they join
│   └── kiln.vue      # panels and HUD pieces
├── mod.config.ts     # its title and its settings
├── package.json      # its name, version and the mods it builds on
└── tsconfig.json
```

`terra mod init my_mod` creates it, and `terra dev` runs it in a world that
reloads when you save.

## A mod is a project

It has a `package.json`, a `tsconfig.json`, a `bun install` and a generated type frame,
the same as any TypeScript project you have worked in. Which is why this works with no
imports and full autocomplete:

```ts
// server/main.ts
game.players.on("join", player => player.tell("Welcome to the universe."))
```

The engine's globals are declared in `.terra/types/`, generated from the version of
Terraverse you are building against.

## Two halves, two points of view

`server/` runs on the **server**, the one authority on the world. It sees every player, or
none: a world with nobody in it is still running. It never has a "you", only whoever an
event hands it.

`client/` runs in each **player's** own game. The server sends it to them when they join,
with your assets. It has exactly one player, the one it's running for, and it sees only
what the server has sent that player.

```
server/main.ts   →  the world: every block, every player, decides what happens
client/main.ts   →  one player's view: draws, plays, listens, asks
```

The client half **presents and asks; it never decides**. It can draw a panel, play a
sound, shake the camera and send the server a request. It can't change the world, which is
what lets every player, the server and every mod agree on what happened.

Each half gets its own globals. `world.set` exists in `server/` and doesn't in `client/`,
so reaching for it from the wrong side is a compile error.

## The rest is yours

Any other folder is ordinary TypeScript: `lib/`, `src/`, `shared/`, whatever suits the mod.
Both halves can import from it, so it gets neither half's globals. That's the rule that keeps
a helper honest: code that might run on either side can't reach for `world` or `effects`,
because one side wouldn't have it.

```bash
my_mod/
├── server/
├── client/
└── lib/
    └── units.ts      # imported by both: plain TypeScript, no globals
```

## Your own names are typed too

`.terra/types/registry.d.ts` holds every block, item, panel and asset your mod
declares, read from your source rather than from a running world. A name you declare is
completable on the line below it as soon as you save.

```ts
content.blocks.create("my_mod:granite_brick", {
    matter: "granite",
    texture: "granite_brick.png",   // completes from assets/textures/
})

world.set(pos, "my_mod:granite_brik")           // a compile error, not a hole in the map
```

Built-ins, your names and the names of every mod you depend on land in the same union.
Nothing about `core:basalt` is more discoverable than a block you wrote four lines up.

## What survives a wipe

| | |
|---|---|
| `server/` · `client/` · `mod.config.ts` · anything else you add | **Yours.** Source |
| `assets/` | **Yours.** Shipped with the mod |
| `.terra/` · `node_modules/` | Generated. Delete freely |

## Not in here

**What the mod has stored.** That lives on the server: what the mod keeps for the whole
universe in its `store/`, and what it keeps for one planet with that world, deleted with it.

**Installed mods.** A server lists the mods it runs as dependencies in its own
[package.json](/docs/server/structure/package). Bun keeps one copy of each version on the
machine, shared by every server that runs it.
