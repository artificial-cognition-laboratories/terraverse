---
title: mod.config.ts
description: mod.config.ts holds what the game needs to know about a mod without running it, chiefly its title and the settings a server can choose.
icon: vscode-icons:file-type-typescript
---

# mod.config.ts

What the game needs to know about your mod without running it.

```ts
export default defineMod({
    title: "Kilns",

    settings: {
        kilnHeat: {
            type: "number",
            default: 1300,
            unit: "K",
            range: [900, 1800],
            description: "The hottest a kiln burns with a full load of charcoal.",
        },
        bricksOnly: {
            type: "boolean",
            default: false,
            description: "Kilns fire clay to brick and nothing else.",
        },
    },
})
```

Read **without running any of your code**, which is what lets a server's setup screen
list your settings, and the registry show them, for a mod that has never loaded.

## title

How players see the mod: in the mod list, a server's setup and the registry. The
id is in [package.json](/docs/mods/structure/package); this is just its display name.

## settings

What a server can choose about your mod. Each is set once per server, in its
[server.config.ts](/docs/server/structure/config) or its setup screen, and holds for every world
on it. Everything shown about a setting comes from this declaration: the label, the range, the
unit and the description all come from here.

```ts
// server/main.ts
mod.settings.kilnHeat     // number, typed from the declaration
mod.settings.kilnHeet     // a compile error
```

Values are checked against their declaration before your `server/main.ts` runs. A server can't hold
a setting your mod doesn't declare, and a value out of range refuses to load with the
setting's name, not a surprise twenty minutes into play.

| `type` | Takes |
|---|---|
| `"number"` | `default`, `range`, `step`, `unit` |
| `"boolean"` | `default` |
| `"choice"` | `default`, `options`: the values, each with a label |
| `"string"` | `default`, `pattern` |

A setting carries what the universe *is*, decided once: how hot kilns burn, whether bricks are the
only fired good. It isn't somewhere to keep state. That belongs in the world, written as it
plays.

## messages

What the two halves of your mod may say to each other, named by who receives them.

```ts
export default defineMod({
    messages: {
        server: {                            // a client may send these to the server
            bellows: {} as {},
            stoke: {} as { kg: number },
        },
        client: {                            // the server may send these to a client
            crack: {} as { pos: Vec3 },
        },
    },
})
```

Each half gets a global named for the other side, typed from this declaration:

```ts
// client/main.ts
server.send("stoke", { kg: 2 })
server.on("crack", ({ pos }) => player.camera.shake({ strength: 0.4, at: pos }))

// server/main.ts
client.on("stoke", ({ player, kg }) => stoke(player, kg))
client.send(player, "crack", { pos })
```

A client can only send what's under `server`, and only receive what's under `client`. Rename a
field here and both halves stop compiling until they agree.

Names are your mod's own, so `stoke` can't collide with another mod's `stoke`: each mod talks
only to its own other half.

### Checked on arrival

Types vanish at runtime, and a client is someone else's machine: it may be modified, or simply
out of date. So every message is checked against this declaration as it arrives, from types
the build turns into validators. One that doesn't match is dropped and reported, and never
reaches your handler. Your `client.on` can trust its payload because nothing else gets through.

That's also why a message is plain data: numbers, strings, booleans, arrays, objects, and the
engine's own types like `Vec3` and block names. Nothing that can't cross a network intact.

## permissions

What your mod lets a server grant, to whichever roles it chooses.

```ts
permissions: {
    light: "Light any kiln, with or without fuel",
},
```

Named with your prefix, `kilns:light`, granted to roles in the server's
[setup](/docs/server/setup#what-each-role-may-do), and checked with `player.can("kilns:light")`
or on a [command](/docs/mods/api/game/commands#permissions). The description is what an admin sees
when choosing who gets it.

## store

The shape of what your mod keeps, for the whole universe and for each world. See
[mod.store](/docs/mods/api/mod#store).

```ts
store: {
    universe: {} as { fired: number },
    world: {} as { kilns: Vec3[] },
},
```

## schedules

What your mod can schedule to happen later, kept across restarts, with the data each carries.
See [time](/docs/mods/api/game#time).

```ts
schedules: {
    cooled: {} as { world: WorldName; pos: Vec3 },
},
```

Like messages and the store, these are plain data, checked when they come back.

## Not in here

**Your name, version and dependencies.** Those are in
[package.json](/docs/mods/structure/package), because they are what a package manager needs,
and two files saying them would eventually say different things.
