---
title: assets/
description: A mod's textures, sounds and models. Referenced by name, typed from the files that are actually there.
icon: vscode-icons:folder-type-images
---

# assets/

Textures, sounds and models. Everything here ships with your mod, and every file is a name
you can reference.

```bash
assets/
├── textures/
│   ├── granite_brick.png
│   └── kiln.png
├── sounds/
│   └── kiln_thud.ogg
└── models/
    └── bellows.glb
```

| Folder | Takes | Used by |
|---|---|---|
| `textures/` | PNG, 16 × 16 for a block face | blocks, items, panels |
| `sounds/` | Ogg Vorbis | blocks, items, `game.sound` on the client |
| `models/` | glTF binary (`.glb`) | items in the hand, machine parts |

## Named by where they sit

```ts
content.blocks.create("my_mod:kiln", {
    matter: "my_mod:fireclay",
    texture: "kiln.png",            // assets/textures/kiln.png
    sounds: { place: "kiln_thud.ogg" },
})
```

A name is a path relative to its kind's folder, so `texture` only ever completes from
`textures/`. The list comes from the files on disk: reference one that isn't there and it's
a compile error, not a magenta block in someone's world.

## What a texture decides, and what it doesn't

A texture is how a block looks up close. How it looks from a mountain away, the colour it
gives the far view, is baked from it when the mod loads, so there's nothing to set and
nothing to drift.

What it is made of is not the texture's business. A block that looks like granite and is
declared as `matter: "iron"` is iron, and it will rust.

## Your assets, your licence

Everything here is published with your mod. Make sure you have the right to redistribute
it, and say under what licence in your `package.json`.
