---
title: Setup
description: Making a Terraverse server. Its star system and worlds, its mods' settings, who may join and what each role may do, and where it starts.
---

# Setup

Making a server is writing down what universe it is. Its rules are the mods in
[package.json](/docs/server/structure/package); everything else lives in
[server.config.ts](/docs/server/structure/config), and you can build it up a piece at a time.

```bash
terra server init my_server
cd my_server
terra host
```

That's already a server: one star system, one world, no mods, and you're its admin. The rest
of this page grows it into a survival server for a few friends.

## The star system

```ts
export default defineServer({
    system: "TF9.dGVycmE",
    worlds: {
        verdant: "@verdant",
        ember: "@ember",
    },
})
```

`system` is a world code: the star, its planets and their land, generated the
same on any machine. Each entry in `worlds` puts one of its planets on the server, under a name
you choose. Leave a planet out and it's still in the sky, but nobody can land on it.

Change a code and you have a different universe. Add a world later and it generates from the
same system, sharing its sun and orbits with the rest.

## Rules

```bash
terra install @terraverse/survival @you/kilns
```

The mods are the universe's rules, and they're the server's dependencies in `package.json`,
pinned to exact versions. **[Mods](/docs/server/mods)** covers choosing them.

## Settings

```ts
    settings: {
        "@terraverse/survival": { hunger: "gentle", keepInventory: true },
        "@you/kilns": { kilnHeat: 1500 },
    },
```

Each mod declares what can be chosen about it, and this is where you choose. Everything is
typed from the mods you've installed: a setting that doesn't exist, or a value out of its range, is
an error in your editor, and the server refuses to start with it rather than discovering it an
hour in.

Leave a setting out and the mod's default holds.

## Who may join

```ts
    access: {
        join: "friends",
        roles: {
            admin: ["@cody"],
            builder: ["@sam", "@ren"],
        },
    },
```

`join` is who may connect at all: `"anyone"`, `"friends"` (your ArcLabs friends), or a list
of accounts. Everyone is identified by their ArcLabs account, so there are no passwords to
manage and a name can't be borrowed.

## What each role may do

Roles say who may change what, while the server runs:

| Role | May |
|---|---|
| `admin` | Everything: settings, roles, worlds, stopping and resetting the server |
| `builder` | Build in creative, and edit the starting state |
| `player` | Play by the rules. Everyone who joins is one |

Mods can add their own. A ranks mod adds `moderator`; a game adds `spectator`, who can watch
but not touch. A role is a set of permissions, and what each permission allows comes from
whoever declared it.

## The starting state

A server can start from more than bare planets. Build the arena, stock the chests, place the
spawn, then:

```bash
terra snapshot
```

With the server stopped, that records the worlds as they are now into [start/](/docs/server/structure/start), as the state every [reset](/docs/server/lifecycle#reset)
returns to and the state a published server opens in. It's source, like the config: commit it,
and it travels with the server when you publish.

A survival server usually has no starting state, and starts from its world code alone.

## Changing it later

Edit the file and restart. On the way back up, the server prepares again, checks every world
against the new rules, and refuses anything that would damage one. Admins can also change
settings from the game, which writes the same file.
