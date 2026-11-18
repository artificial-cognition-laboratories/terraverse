---
title: content
description: "What things are. Blocks, items, substances, mixtures, reactions and biomes, declared while the mod loads and sealed once every mod has. Everything physical is derived from matter."
---

# content

What things are. Declared while your mod loads, sealed once every mod has, and the names every
world is saved in.

```ts
interface ContentApi {
    readonly blocks: Blocks
    readonly substances: Substances
    readonly mixtures: Mixtures
    readonly reactions: Reactions
    readonly structures: Structures   // builds, from assets/structures
    readonly bodies: BodyKinds        // what can be spawned as a body. not designed yet
    readonly items: Items             // not designed yet
    readonly biomes: Biomes           // not designed yet
}
```

Every `create` throws `CONTENT_SEALED` once loading is over, and `NAME_NOT_YOURS` for a name
without your prefix. Every `get` throws on a name that doesn't exist, rather than returning
nothing.

## blocks

```ts
interface Blocks {
    // Declare a block. Load only.
    // Throws NAME_TAKEN, MATTER_UNKNOWN, ASSET_MISSING.
    create(name: OwnName, def: BlockDef): BlockType

    // One by name. Throws BLOCK_UNKNOWN.
    get(name: BlockName): BlockType

    // Every block: the engine's, then each mod's, in load order.
    list(): readonly BlockType[]
}

interface BlockDef {
    matter: SubstanceName | MixtureName   // what it's made of. everything physical follows
    share?: number                        // 0–1: how much of the block is matter. default 1
    texture: TextureName | { top?: TextureName; bottom?: TextureName; sides: TextureName }
    sounds?: { place?: SoundName; break?: SoundName; step?: SoundName }
    on?: BlockHandlers
}

interface BlockHandlers {
    // Each runs before the action. Return, and it happens; throw Refused, and it doesn't.
    use?(e: BlockEvent & { player: Player }): void | Promise<void>
    place?(e: BlockEvent & { player: Player }): void
    break?(e: BlockEvent & { player: Player }): void
}

interface BlockEvent {
    world: World
    pos: Vec3
    block: BlockType
}
```

### Say what it is

```ts
content.blocks.create("kilns:granite_brick", {
    matter: "granite",
    texture: "granite_brick.png",
})
```

A block's physics come from its matter, never from numbers you set. The brick weighs what a
cubic metre of granite weighs, breaks as granite breaks, conducts heat as granite does and
melts where granite melts. There's no `hardness` or `weight` to set, so there's none to get
wrong, and a better granite in a later engine makes your brick better too.

`share` is for blocks that are mostly air: a block of leaves is about `0.05` wood, so it weighs
and burns like 5% of a cubic metre of wood.

### What it can work out

```ts
interface BlockType {
    readonly name: BlockName
    readonly matter: Substance | Mixture
    readonly share: number
    readonly mass: number           // kg in one block: density × share
    readonly melts: number | null   // K, where its matter starts to melt; null if it doesn't
}
```

### Behaviour belongs to the block

```ts
content.blocks.create("kilns:kiln", {
    matter: "kilns:fireclay",
    texture: "kiln.png",
    on: {
        use({ player, world, pos }) {
            if (world.matter.temperature(pos) > 600) throw new Refused("Too hot to open.")
            player.ui.open("kilns:kiln", kiln(world, pos))
        },
    },
})
```

What a block does when it's used, placed or broken is part of what it is, so it's declared with
it. To watch blocks you didn't make, say every `core:` block broken in a world, use
[universe](/docs/mods/api/game/worlds#events).

## substances

```ts
interface Substances {
    // Declare a pure substance. Load only. Its formula is balanced against its composition.
    create(name: SubstanceName, def: SubstanceDef): Substance
    get(name: SubstanceName): Substance
    list(): readonly Substance[]
}

interface Substance {
    readonly name: SubstanceName
    readonly title: string                         // "Carbon dioxide"
    readonly formula: string                       // "CO₂"
    readonly composition: Readonly<Record<Element, number>>
    readonly density: { solid?: number; liquid?: number }   // kg/m³. gas follows from pressure
    readonly heatCapacity: number                  // J/(kg·K)
    readonly melts: number | null                  // K at one atmosphere
    readonly boils: number | null                  // K at one atmosphere; null if it sublimes
    readonly conductivity: { solid?: number; liquid?: number; gas?: number }   // W/(m·K)

    // Its phase at a temperature and pressure (Pa, default one atmosphere).
    phase(temperature: number, pressure?: number): "solid" | "liquid" | "gas"
}
```

`SubstanceDef` takes the measured data the engine already uses for its own: composition, triple
and critical points, heats of fusion and vaporisation, densities, conductivities, viscosity,
heat capacity. Everything else is derived from those, so a substance you add melts, boils,
flows and burns by the same rules as water.

## mixtures

```ts
interface Mixtures {
    // Declare a mixture of substances or other mixtures, by mass fraction. Fractions sum to 1.
    create(name: OwnName, def: { parts: Record<SubstanceName | MixtureName, number>; title: string }): Mixture
    get(name: MixtureName): Mixture
}
```

Rock, soil, clay, bronze. A mixture's properties follow from its parts: granite is 30% silica,
55% feldspar and 15% mica, and it's as dense and as hard to melt as that makes it.

```ts
content.mixtures.create("kilns:fireclay", {
    title: "Fireclay",
    parts: { kaolinite: 0.7, silica: 0.25, water: 0.05 },
})
```

## reactions

```ts
interface Reactions {
    // Declare a reaction. Load only.
    // Throws REACTION_UNBALANCED if the elements don't balance.
    create(name: OwnName, def: ReactionDef): Reaction
    get(name: ReactionName): Reaction
}

interface ReactionDef {
    title: string
    reactants: Record<SubstanceName, number>   // moles
    products: Record<SubstanceName, number>
    activation: number                         // kJ/mol
    factor: number                             // Arrhenius pre-factor, 1/s
    on?: { reacted?(e: { world: World; pos: Vec3 }): void }   // it happened, in any world
}
```

The heat a reaction gives off or takes in comes from its substances' thermodynamic data, not
from you, and how fast it runs comes from the temperature where it happens. As with blocks, what
should follow from your own reaction is declared with it, and fires in whichever world it
happens. A reaction that
doesn't balance by element is refused at load, naming the element.

## structures

```ts
interface Structures {
    // A build saved as a file in assets/structures/, by its name. Throws STRUCTURE_UNKNOWN.
    get(name: StructureName): Structure
    list(): readonly StructureName[]
}
```

A build as data: blocks and the matter in them, made in a world with
[world.copy](/docs/mods/api/game/worlds#structures) and shipped with your mod. Arenas, ruins,
starting bases. Pasting one is `world.paste("arenas:colosseum", at)`.

## bodies

Kinds of body that can be spawned: `core:humanoid`, a crate, a machine built in the workshop.
What a kind declares, its shape, its matter, its joints, what it can sense and whether it can act,
is not designed yet: it comes with machines.

## items

Hand-sized things: a form (axe, rope, nail) in a material, with real mass. Not designed yet: it
comes with crafting.

## biomes

What the ground, plants and surface are made of where the world generator puts each biome. Not
designed yet: it comes with the generator's controls.
