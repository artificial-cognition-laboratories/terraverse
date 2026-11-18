// Building materials: what machines and buildings are made of, as against the ground's dirt
// and stone. Each says only what it is made of and how much of the block that fills (sawn
// timber is wood with air in its grain); how heavy, strong and grippy it is, and whether
// light passes through, follow from its matter (assets/matter/substances.json).
//
// Plain for now: in survival they'll be worked from raw stock (a log sawn, an ingot
// rolled), not found.

const MATERIALS = [
    { name: "flint", matter: "silica", share: 1 },        // hard and brittle: it breaks clean
    { name: "timber", matter: "wood", share: 0.4 },
    { name: "aluminium", matter: "aluminium", share: 1 },
    { name: "steel", matter: "steel", share: 1 },
    { name: "rubber", matter: "rubber", share: 1 },       // a tyre's tread: it grips
    { name: "canvas", matter: "wood", share: 0.33 },      // TEMP(2026-11-18): woven fibre, as wood until there's cellulose
    { name: "glass", matter: "glass", share: 1 },
] as const

for (const m of MATERIALS) {
    content.blocks.create(`materials:${m.name}`, {
        matter: m.matter,
        share: m.share,
        texture: `${m.name}.png`,
    })
}
