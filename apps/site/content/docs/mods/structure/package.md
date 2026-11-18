---
title: package.json
description: A mod's package.json is its identity, its version and the mods it builds on. Depending on a mod is depending on a package.
icon: vscode-icons:file-type-json
---

# package.json

Your mod's name, its version, and the mods it builds on.

```json
{
    "name": "@you/kilns",
    "version": "0.3.0",
    "description": "Kilns that fire clay by real heat.",
    "type": "module",
    "dependencies": {
        "@terraverse/craft": "1.4.2"
    }
}
```

## name

`@scope/name`, where the scope is your account on the registry. The part after the slash is
your mod's **prefix**: everything `@you/kilns` registers is called `kilns:something`.

```ts
content.blocks.create("kilns:kiln", { /* ... */ })      // yours
content.blocks.create("craft:bench", { /* ... */ })     // NAME_NOT_YOURS
```

Two mods with the same prefix can't load into one world. The world refuses to open and names
both, rather than letting one quietly shadow the other.

## Depending on a mod is depending on a package

```ts
// server/main.ts
import { forms } from "@terraverse/craft"

forms.create("kilns:brick_mould", { /* ... */ })
```

A mod's exports are its API to other mods, typed like any other package. That is also what
decides load order: `@terraverse/craft` has loaded before your first line runs, so what you
import from it is ready.

Every dependency is **pinned**. The version you see is the version that loads, because a
mod that changed under a world could break a save that worked yesterday.

Dependencies that aren't mods work too. Need a real library rather than a hand-rolled one?
Add it here; it is bundled into your mod when you publish.

## No types to install

The engine's globals come with `terra`: [prepare](/docs/cli/prepare) writes them into
[.terra/types/](/docs/mods/structure/dot-terra), so the declarations in your editor are always
the ones the terra running your mod implements. Upgrading terra upgrades them.

## version

Semver, and it means something: a world records the version of every mod it was last run with, and asks
before loading one with a newer major version.
