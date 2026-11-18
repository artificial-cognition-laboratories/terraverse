---
title: .terra/
description: The generated .terra directory holds your mod's types. Created when the mod is installed, regenerated whenever its source or the engine version changes.
icon: vscode-icons:folder-type-typings
---

# .terra/

Generated types. Regenerated whenever you save, and whenever the engine version changes.
Never edit anything inside it.

```bash
.terra/
└── types/
    ├── server.d.ts      # the server half's globals
    ├── client.d.ts      # the client half's globals
    ├── shared.d.ts      # what every file sees: Vec3, names, your messages
    ├── registry.d.ts    # every name your mod and its dependencies declare
    └── tsconfig.json    # the config your tsconfig.json extends
```

## Why your mod has autocomplete

```ts
// server/main.ts: no imports, full types
content.blocks.create("my_mod:granite_brick", { matter: "granite" })
```

The declarations are the engine's mod API, built into `terra` and written here by
[prepare](/docs/cli/prepare). Which ones a file sees depends on where it sits: `server/` gets
`server.d.ts`, `client/` gets `client.d.ts`, and every other folder gets only the shared types,
each through its folder's `tsconfig.json`. That's what makes calling `world.set` from a panel a compile error.

They are globals rather than imports because there is exactly one of each, and a mod that
forgot to import `world` has never once meant something else by it.

## registry.d.ts

Every literal name in your source and your dependencies' source: blocks, items, substances,
panels and the files in `assets/`. Read from the code, so the types exist before anything
boots.

Only literal names can be seen. `content.blocks.create(name, …)` where `name` is a variable is
invisible to the generator, which is honest: the name it resolves to isn't known until the
mod runs either.

## tsconfig.json

The mod's root has a one-line `tsconfig.json`:

```json
{ "extends": "./.terra/types/tsconfig.json" }
```

Two files rather than one because the generated half is regenerated and the root half is
yours: add options to it and they survive the next upgrade.

## Regenerable

```bash
rm -rf .terra          # the next build rebuilds it
```

Nothing in here is authored, so nothing is lost. If your editor stops resolving the globals,
this is the folder to delete.
