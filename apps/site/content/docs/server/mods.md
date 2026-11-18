---
title: Mods
description: Choosing a Terraverse server's rules. Installing mods from the registry, pinned to exact versions, writing your own in mods/, and what happens to worlds when the rules change.
---

# Mods

A server's mods are its rules. Some come from the registry, written by someone else; some you
write yourself, for this server only.

```bash
terra install @terraverse/survival
terra install @you/kilns@0.3.0
```

That's `bun add` with the checks a universe needs, and it adds each mod to the server's
[package.json](/docs/server/structure/package) as a dependency:

```json
"dependencies": {
    "@terraverse/survival": "1.2.0",
    "@you/kilns": "0.3.0",
    "ranks": "file:./mods/ranks"
}
```

Every dependency is a rule of this universe, and nothing else is. Installing from the game's mod
browser does exactly the same thing: it runs the same install and writes the same line.

## Pinned, always

Every mod is listed at an exact version. The version you see is the version that runs, here
and on any machine the server is cloned to. A mod's author publishing a new version changes
nothing until you choose it:

```bash
terra install @you/kilns@0.4.0
```

`bun.lock` pins what they depend on in turn, and bun keeps one copy of each version on the
machine, shared by every server that runs it.

## Your own

```bash
mods/
└── ranks/        # a mod, exactly like a published one
```

A folder in `mods/` is a mod like any other, written with the same API, and a `file:`
dependency: `"ranks": "file:./mods/ranks"`. It's for the rules that belong to this server
alone: ranks, a welcome message, a tweak to how the game you installed plays here. It's never published on its own,
but it travels with the server when you publish that.

## Load order is dependency order

You don't order the list. A mod loads after everything it builds on, so `@you/kilns`, which
builds on `@terraverse/craft`, always finds craft ready. Two mods that don't depend on each
other don't care which goes first.

## When two mods disagree

Each mod's things are named by its prefix: everything `@you/kilns` makes is `kilns:something`.
Two mods with the same prefix can't run on one server, and it refuses to start, naming both,
rather than letting one quietly replace the other's blocks.

A mod that depends on one you don't have, or on a different version than you've installed,
is fetched with it or refused with the reason. You never find out from a crash.

## Changing the rules

Adding a mod is safe: existing worlds just gain things.

Removing one is not, if a world has been saved with its blocks. Before removing anything,
`terra uninstall` names the worlds that would no longer open, and the server checks again
every time it starts.

Updating a mod within its major version is meant to be safe. A new major version is the author
saying it might not be, so the server asks before loading one into worlds saved with the old.

## Settings

Each mod's settings are chosen in the config, once for the whole server: see
[Setup](/docs/server/setup#settings).
