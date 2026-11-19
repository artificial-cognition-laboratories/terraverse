---
title: commands
description: "game.commands. Chat commands with typed arguments and permissions. Run by players from chat, or from the server console."
---

# game.commands

Commands players type in chat, with typed arguments and a permission.

```ts
interface Commands {
    // Register a command. Throws COMMAND_TAKEN if another mod has the path.
    create<A extends Args>(path: string | readonly string[], def: CommandDef<A>): Disposer
}

interface CommandDef<A extends Args> {
    description: string               // shown in chat's suggestions and /help
    args?: A
    permission?: PermissionName       // who may run it. default: everyone
    run(e: {
        player?: Player               // undefined from the server console
        args: Parsed<A>
        reply(text: string): void     // to whoever ran it, wherever they are
    }): void | Promise<void>
}
```

```ts
game.commands.create(["kiln", "light"], {
    description: "Light the kiln you're looking at",
    permission: "kilns:light",
    args: { heat: { type: "number", min: 600, max: 1800, optional: true } },
    run({ player, args, reply }) {
        if (!player) throw new Refused("Only a player can look at a kiln.")
        const pos = kilnInView(player)
        if (!pos) throw new Refused("You're not looking at a kiln.")
        light(player.world, pos, args.heat ?? mod.settings.kilnHeat)
        reply("Lit.")
    },
})
```

Typed as `/kiln light 1500`. Paths that share a prefix share a group: `/kiln` lists what's under
it.

## Arguments

```ts
type Arg =
    | { type: "string"; optional?: boolean; options?: readonly string[] }
    | { type: "number" | "integer"; optional?: boolean; min?: number; max?: number }
    | { type: "boolean"; optional?: boolean }
    | { type: "player" | "world" | "block" | "position"; optional?: boolean }
```

Parsed before `run` is called, and typed in `args`: a `"player"` argument arrives as a `Player`,
a `"position"` as a `Vec3` (typed `x y z`). The last argument, if it's a string, takes the rest
of the line, so `/msg @ana see you at the kiln` needs no quotes. A command typed wrong never reaches you; the player is told what was
expected, and chat suggests as they type.

## Permissions

A permission is declared in [mod.config.ts](/docs/mods/structure/config), granted to roles in
the server's [setup](/docs/server/setup#what-each-role-may-do), and checked before `run`. A
player without it is told so, and the command doesn't run.

## Not only players

The server console can run commands too, with no `player`. A command that
needs one says so with `Refused`; one that doesn't, like `/kiln count`, works from anywhere.
