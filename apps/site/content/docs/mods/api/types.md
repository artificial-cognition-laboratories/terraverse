---
title: types
description: "The vocabulary every Terraverse mod file shares: positions, areas, durations, refusals, and the typed names of everything in the universe. SI units throughout."
---

# types

The vocabulary every file in a mod shares, on either half or neither: positions, areas,
durations, refusals, and the names of everything in the universe.

```ts
type Vec3 = readonly [x: number, y: number, z: number]   // a block, in its world; y is up
type Area = { min: Vec3; max: Vec3 }                      // both corners included

type Duration = { seconds?: number; minutes?: number; hours?: number }

type Mass = { kg: number }

// Returned by everything that registers: call it, and the registration is gone.
type Disposer = () => void

// Throw from any handler to stop the action. The player sees the reason.
class Refused extends Error {
    constructor(reason: string)
}
```

## Units are SI, always

Kelvin, kilograms, metres, seconds, joules, pascals. A block is a metre. There are no game units
to learn and no conversions to get wrong: granite is 2700 kg/m³ because granite is.

```ts
content.substances.get("water").boils   // 373.15 (K)
world.matter.temperature(pos)           // 291.4  (K)
player.ui.open("kilns:kiln", { heat: 1180 })   // and kelvin on the panel too
```

Convert for people at the edge, in a panel, never in the numbers you pass around.

## Refused

```ts
on: {
    use({ player, world, pos }) {
        if (world.matter.temperature(pos) > 600) throw new Refused("Too hot to open.")
        player.ui.open("kilns:kiln", kiln(world, pos))
    },
}
```

An expected *no*. Thrown from a handler for something a player did, it stops that thing: the
block isn't placed, the panel doesn't open, the command does nothing. The player is shown the
reason. Anything else thrown is a bug: it's reported with your mod's name, file and line.

## Names

Every name in the universe is a type, generated into `.terra/types/registry.d.ts` from your source
and every mod you build on:

| Type | Holds | Example |
|---|---|---|
| `BlockName` | Every block | `"kilns:kiln"` · `"core:basalt"` |
| `ItemName` | Every item | `"core:flint_axe"` |
| `SubstanceName` | Every substance | `"water"` · `"granite"` |
| `MixtureName` | Every mixture | `"granite"` · `"kilns:fireclay"` |
| `ReactionName` | Every reaction | `"methane_burns"` |
| `WorldName` | This server's worlds | `"verdant"` |
| `PanelName` · `HudName` | Your panels and HUD pieces | `"kilns:kiln"` |
| `TextureName` · `SoundName` | Files in your `assets/` | `"kiln.png"` |
| `PermissionName` | Every declared permission | `"kilns:light"` |
| `AccountName` | An ArcLabs account | `"@cody"` |

`OwnName` is any name starting with your prefix. It's what `create` takes, so you can only make
things that are yours.

Substances and the engine's built-in mixtures have no prefix: `water` is water in every universe.
Anything a mod adds has one.
