import type { WorldMods } from "../bridge/index.ts"

// The worlds' mods as the gallery's engine keeps them: the game's two, and what's added.

const OFFICIAL = [
    { name: "@terraverse/materials", version: "0.1.0", official: true, blocks: ["materials:granite", "materials:basalt"] },
    { name: "@terraverse/commands", version: "0.1.0", official: true, blocks: [] },
]

export function MockWorldMods() {
    const worlds = new Map<string, WorldMods>([
        ["/worlds/pine_ridge", { own: true, mods: [...OFFICIAL,
            { name: "@ana/kilns", version: "0.2.0", official: false, blocks: ["kilns:kiln"] },
            { name: "@river/signs", version: "1.0.0", official: false, blocks: [] }] }],
    ])
    const of = (path: string): WorldMods => worlds.get(path) ?? { own: false, mods: [...OFFICIAL] }

    return {
        mods: (path: string): WorldMods => structuredClone(of(path)),
        add(path: string, specs: string[]): WorldMods {
            const now = of(path)
            for (const spec of specs) {
                const at = spec.lastIndexOf("@")
                const [name, version] = at > 0 ? [spec.slice(0, at), spec.slice(at + 1)] : [spec, "1.0.0"]
                now.mods = [...now.mods.filter((m) => m.name !== name), { name: name, version: version, official: false, blocks: [] }]
            }
            worlds.set(path, { own: true, mods: now.mods })
            return structuredClone(of(path))
        },
        remove(path: string, name: string): WorldMods {
            const mod = of(path).mods.find((m) => m.name === name)
            if (!mod) throw new Error(`${name} isn't in this world.`)
            if (mod.blocks.length) throw new Error(`${name} has put ${mod.blocks.length} kinds of block in this world: taking it out would leave them as unknown blocks.`)
            worlds.set(path, { own: true, mods: of(path).mods.filter((m) => m.name !== name) })
            return structuredClone(of(path))
        },
        added: (path: string): number => of(path).mods.filter((m) => !m.official).length,
    }
}
