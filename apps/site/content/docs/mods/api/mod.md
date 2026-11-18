---
title: mod
description: "This mod. Its identity, the settings the server chose for it, and the store it keeps for the whole universe."
---

# mod

This mod: who it is, what the server chose for it, and what it keeps.

```ts
interface ModApi {
    readonly name: string           // "@you/kilns"
    readonly prefix: string         // "kilns"
    readonly version: string        // "0.3.0"

    // The server's choices, typed from mod.config.ts. Fixed while the server runs.
    readonly settings: Settings

    // What this mod keeps for the whole universe.
    readonly store: Store<UniverseStore>
}
```

## settings

```ts
if (mod.settings.bricksOnly) { /* … */ }
const heat = mod.settings.kilnHeat     // number, in K, within its declared range
```

Declared in [mod.config.ts](/docs/mods/structure/config#settings), chosen in the server's
`server.config.ts`, and checked before your mod loads. A setting's value is never missing and
never out of range by the time you read it. Ready at load, so you can decide what to register
from them.

## store

```ts
interface Store<S> {
    get<K extends keyof S>(key: K): Promise<S[K] | undefined>
    set<K extends keyof S>(key: K, value: S[K]): Promise<void>
    delete(key: keyof S): Promise<void>

    // Read, change and write in one step: nothing else writes in between.
    update<K extends keyof S>(key: K, fn: (old: S[K] | undefined) => S[K]): Promise<S[K]>
}
```

Two stores, by where the thing you're keeping belongs:

| | Holds | Example |
|---|---|---|
| `mod.store` | What's true across the universe | Each player's rank, a leaderboard |
| `world.store` | What belongs to one planet | Where the kilns on it are |

Both are declared in `mod.config.ts`, so their keys and values are typed:

```ts
// mod.config.ts
store: {
    universe: {} as { fired: number; ranks: Record<AccountName, string> },
    world: {} as { kilns: Vec3[]; fires: number },
}
```

### Why it's async

Everything is saved as it's written, so a crash or a power cut loses nothing your mod kept. And
the universe-wide store doesn't assume one machine: a universe large enough to run each planet
on its own server still has one `mod.store`, and `await` is what makes that possible without
changing a line of your mod.

### Only data

Plain data, as messages are: numbers, strings, booleans, arrays, objects, `Vec3`, names. Values
are checked against the declaration when they're read, so data written by an older version of
your mod that no longer fits throws `STORE_SHAPE`, naming the key, instead of reaching your code
in a shape it doesn't expect. Migrating old data isn't designed yet.
