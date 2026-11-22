// TEMP(2026-11-21): a sample catalogue while the registry's UI is built ahead of
// the backend (../arclabs: categories, type, sort, installs and file listings).
// Every page reads it through Registry (./index.ts); swapping this for the API
// changes no page.

import type { File, Mod, Server, Version } from "./types"

type Row = Omit<Mod, "readme" | "license" | "createdAt" | "deprecated" | "versions" | "files" | "dependents" | "latestVersion"> & {
    versions: string[]
    dependencies?: Record<string, string>
    readme?: string
}

const ROWS: Row[] = [
    { name: "@terraverse/hunger-games", title: "Hunger Games", owner: "terraverse", official: true, type: "mod", categories: ["games"], stars: 412, installs: 9800, updatedAt: "2026-11-18", versions: ["2.6.0", "2.5.1", "2.5.0", "2.0.0"],
        description: "Last one standing on a shrinking island. Loot, sealed chests and a border that boils the sea.",
        dependencies: { "@terraverse/survival": "^3.0.0", "@terraverse/commands": "^0.1.0" } },
    { name: "@terraverse/survival", title: "Survival", owner: "terraverse", official: true, type: "mod", categories: ["survival"], stars: 520, installs: 21400, updatedAt: "2026-11-20", versions: ["3.4.0", "3.3.2", "3.0.0"],
        description: "The game out of the box: hunger, health, night and the long walk to a better tool.",
        dependencies: { "@terraverse/materials": "^0.1.0" } },
    { name: "@terraverse/craft", title: "Craft", owner: "terraverse", official: true, type: "mod", categories: ["survival", "building"], stars: 380, installs: 12400, updatedAt: "2026-11-15", versions: ["3.2.0", "3.1.0"],
        description: "Forms × materials: knap, carve, saw and forge at a bench. No recipe book, just what a material can take.",
        dependencies: { "@terraverse/materials": "^0.1.0" } },
    { name: "@terraverse/rally", title: "Rally", owner: "terraverse", official: true, type: "mod", categories: ["games", "machines"], stars: 198, installs: 3100, updatedAt: "2026-11-09", versions: ["1.2.0", "1.0.0"],
        description: "Point-to-point races over real terrain. Build the car, then beat the clock down the mountain." },
    { name: "@terraverse/materials", title: "Materials", owner: "terraverse", official: true, type: "mod", categories: ["matter", "library"], stars: 140, installs: 24100, updatedAt: "2026-11-20", versions: ["0.1.0"],
        description: "The game's own blocks, each made of a substance: granite, clay, oak, iron, and what they become." },
    { name: "@terraverse/commands", title: "Commands", owner: "terraverse", official: true, type: "mod", categories: ["library"], stars: 96, installs: 23800, updatedAt: "2026-11-21", versions: ["0.1.0"],
        description: "The chat commands every server wants: kick, travel, time, weather and roles." },
    { name: "@ash/kilns", title: "Kilns", owner: "ash", official: false, type: "mod", categories: ["matter", "building"], stars: 164, installs: 2200, updatedAt: "2026-11-19", versions: ["3.3.0", "3.2.0", "2.0.0"],
        description: "Fired clay, brick and glass from heat you have to build and feed.",
        dependencies: { "@terraverse/materials": "^0.1.0" } },
    { name: "@wren/iron-rails", title: "Iron Rails", owner: "wren", official: false, type: "mod", categories: ["machines"], stars: 301, installs: 5500, updatedAt: "2026-11-12", versions: ["2.0.0", "1.4.0"],
        description: "Track, points and wagons on real physics. Hauling ore stops being a crawl.",
        dependencies: { "@terraverse/materials": "^0.1.0" } },
    { name: "@pell/shadowplay", title: "Shadowplay", owner: "pell", official: false, type: "mod", categories: ["games"], stars: 140, installs: 1900, updatedAt: "2026-11-16", versions: ["3.0.0"],
        description: "Light and sound decide who sees you. Hide-and-seek with real optics." },
    { name: "@oren/windmills", title: "Windmills", owner: "oren", official: false, type: "mod", categories: ["machines", "building"], stars: 88, installs: 1300, updatedAt: "2026-11-05", versions: ["0.9.0", "0.8.0"],
        description: "Sails, gears and millstones turned by the weather that's actually blowing." },
    { name: "@mira/terraced", title: "Terraced", owner: "mira", official: false, type: "mod", categories: ["building"], stars: 72, installs: 940, updatedAt: "2026-10-28", versions: ["1.1.0"],
        description: "Retaining walls, terraces and steps that hold the hillside they're cut into." },
    { name: "@tove/lumen", title: "Lumen", owner: "tove", official: false, type: "shaders", categories: ["textures"], stars: 233, installs: 6100, updatedAt: "2026-11-17", versions: ["4.0.0", "3.2.0"],
        description: "Soft shadows, long dusks and water that catches the moon. Light by the real sun." },
    { name: "@kit/pixelwork", title: "Pixelwork", owner: "kit", official: false, type: "textures", categories: ["textures"], stars: 175, installs: 4800, updatedAt: "2026-11-02", versions: ["2.3.0", "2.0.0"],
        description: "Every block redrawn at 32 pixels, in a calm, earthy palette." },
    { name: "@juno/archipelago", title: "Archipelago", owner: "juno", official: false, type: "world", categories: ["world"], stars: 121, installs: 2600, updatedAt: "2026-11-14", versions: ["1.0.0"],
        description: "A star system of island worlds, tuned for boats, trade and long crossings." },
]

/** Everything the catalogue holds, as the API will answer it. */
export const MOCK: Mod[] = ROWS.map(row => {
    const { versions, dependencies, readme, ...listing } = row
    const latest = versions[0] ?? "0.1.0"
    return {
        ...listing,
        latestVersion: latest,
        readme: readme ?? readmeFor(row),
        license: listing.official ? "LGPL-2.1-or-later" : "MIT",
        createdAt: "2026-09-02",
        deprecated: null,
        versions: versions.map((v, i): Version => ({
            version: v,
            publishedAt: shift(listing.updatedAt, i * 9),
            api: i === 0 ? "0.1" : "0.1",
            dependencies: dependencies ?? {},
            withheld: null,
        })),
        files: filesFor(row, latest),
        dependents: ROWS.filter(r => r.dependencies && listing.name in r.dependencies).map(r => r.name),
    }
})

export const MOCK_SERVERS: Server[] = [
    { name: "Ember Isles", game: "Hunger Games", players: 23, slots: 24, region: "EU", mods: 4 },
    { name: "Long Haul", game: "Survival", players: 41, slots: 64, region: "US", mods: 12 },
    { name: "Rally Cross", game: "Rally", players: 9, slots: 16, region: "EU", mods: 3 },
    { name: "Quiet Valley", game: "Building", players: 6, slots: 20, region: "AU", mods: 7 },
]

function readmeFor(row: Row): string {
    const bare = row.name.split("/")[1] ?? row.name
    const needs = Object.keys(row.dependencies ?? {})
    return `# ${row.title}

## Install

\`\`\`bash
terra install ${row.name}
\`\`\`

Then add it to a server's \`server.config.ts\`, or play it straight away:

\`\`\`bash
terra ${row.name}
\`\`\`
${needs.length ? `\n## Needs\n\n${needs.map(n => `- [\`${n}\`](/registry/${n})`).join("\n")}\n` : ""}
## Settings

\`\`\`ts
settings: {
    "${row.name}": { enabled: true },
},
\`\`\`

Every setting is typed from the mod, so a wrong value is a red line in your editor
before it's a broken server.

## How it works

${row.title} is built on the public API and nothing else: what it does, your own
mod could do too. The source is on the left; \`server/main.ts\` is where it starts.

> Found a bug? Open an issue where \`${bare}\` is developed, or tell us on the forum.
`
}

function filesFor(row: Row, version: string): File[] {
    const bare = row.name.split("/")[1] ?? row.name
    const text = (path: string, body: string): File => ({ path: path, size: body.length, text: body })
    const binary = (path: string, size: number): File => ({ path: path, size: size, text: null })
    return [
        text("README.md", row.readme ?? readmeFor(row)),
        text("package.json", `{
    "name": "${row.name}",
    "version": "${version}",
    "type": "module",
    "dependencies": ${JSON.stringify(row.dependencies ?? {}, null, 8).replace(/\n}/, "\n    }")}
}
`),
        text("mod.config.ts", `export default defineMod({
    title: "${row.title}",
    description: "${row.description.replace(/"/g, '\\"')}",
    categories: ${JSON.stringify(row.categories)},
})
`),
        text("server/main.ts", `// The server half: runs once as the universe loads, then reacts to what happens.

import { setup } from "./${bare}"

setup(game)

game.players.on("join", player => player.tell(\`${row.title} is on this server.\`))
`),
        text(`server/${bare}.ts`, `/** ${row.description} */
export function setup(game: Game) {
    game.commands.create(["${bare}", "info"], {
        description: "What ${row.title} adds",
        run({ reply }) {
            reply("${row.title} ${version}")
        },
    })
}
`),
        text("client/main.ts", `// The client half: one player's screen. ${row.title} draws nothing of its own yet.
export {}
`),
        binary(`assets/textures/${bare}_icon.png`, 1840),
        binary(`assets/textures/${bare}_block.png`, 2210),
    ]
}

/** An ISO date `days` before another. */
function shift(iso: string, days: number): string {
    const date = new Date(`${iso}T12:00:00Z`)
    date.setUTCDate(date.getUTCDate() - days)
    return date.toISOString().slice(0, 10)
}
