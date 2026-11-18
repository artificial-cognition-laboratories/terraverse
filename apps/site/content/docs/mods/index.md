---
title: Modding overview
description: Terraverse mods are TypeScript projects written against the same public API the official game uses. A mod says what things are, and the universe works out the rest.
---

# Modding overview

A mod is a TypeScript project. It says what things are, and the universe works out the rest.

```ts
content.blocks.create("kilns:granite_brick", {
    matter: "granite",
    texture: "granite_brick.png",
})
```

That's a whole block. It's heavy because granite is dense, it takes a pick to break, it
melts at granite's melting point, and from a mountain away it's the right colour in the
far view. You didn't write any of that, because you said what it is.

## The same API the game uses

Every official mod, the ones that make blocks, weather, crafting and the player's own HUD, is
built on the public API and nothing else. Anything the game does, your mod can do, and if the
API can't express something the game needs, the API gets fixed.

Switch every official mod off and you have a clean universe: real planets, matter and physics,
and no rules on top. That's where a different game starts.

## Mechanisms and policy

The engine supplies **mechanisms**: matter that heats and flows, bodies that move under
gravity, planets generated from plates and climate. Your mod supplies **policy**: what there
is, what it's for, and what happens when someone does something.

So a mod never runs per block, per tick. It doesn't move the water; it says what water is. It
doesn't simulate a fire; it lights one and is told when it spreads. That's what keeps a
universe with fifty mods in it fast, and why a mod written today keeps working when the
simulation underneath gets better.

## Two halves

A mod has a **server half**, which runs the universe, and a **client half**, which runs on
each player's screen.

```
server/   the universe's point of view: every player, or none. decides what happens
client/   one player's point of view: draws, plays, listens, asks
```

The server half is where nearly everything happens. Many mods never have a client half at
all: blocks, items and rules reach every player's screen without one. The client half is for
what a declaration can't say: a panel, a keybind, an effect on one player's screen.

## Typed, end to end

Every name in the universe is in your editor: blocks, items, substances, panels, settings and
assets, yours and every mod you build on. A typo is a red line before it is a hole in someone's
world. Errors at runtime name the mod, the file and the line, and say what was expected.

## Shared like a package

A mod is published to the registry under your account, versioned, and installed into a server
by name. Mods build on each other the way packages do, by importing them, so a crafting mod's
forms and processes are just its exports.

## Where next

- **[Lifecycle](/docs/mods/lifecycle)**: when each part of a mod runs.
- **[Server](/docs/mods/server)**: writing the half that runs the universe.
- **[Client](/docs/mods/client)**: writing the half on each player's screen.
- **[Publish](/docs/mods/publish)**: sharing it on the registry.
- **[Mod structure](/docs/mods/structure)**: the folder, file by file.
