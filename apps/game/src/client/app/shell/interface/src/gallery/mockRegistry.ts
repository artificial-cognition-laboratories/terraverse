// The gallery's stand-in for the ArcLabs registry: a fuller catalogue than the real one
// has yet (categories, kinds, installs, files), so every part of the panel shows.

import type { RegistryFront, RegistryListing, RegistryMod, RegistryResults, RegistrySearch } from "../bridge"

const DAY = 86_400_000
const CRAFT: Record<string, string> = { "@terraverse/craft": "1.4.2" }
const ago = (days: number) => new Date(Date.now() - days * DAY).toISOString()

const CATEGORIES = [
    { id: "games", label: "Games" }, { id: "survival", label: "Survival" }, { id: "building", label: "Building" },
    { id: "machines", label: "Machines" }, { id: "matter", label: "Matter" }, { id: "world", label: "World" },
    { id: "textures", label: "Textures" }, { id: "library", label: "Libraries" },
]

type Seed = [name: string, title: string, description: string, categories: string[], stars: number, installs: number, days: number, extra?: Partial<RegistryListing>]
const SEEDS: Seed[] = [
    ["@terraverse/hunger", "Hunger Games", "Last one standing on a shrinking island. Loot, sealed chests and a border that boils the sea.", ["games", "survival"], 412, 9820, 3, { official: true }],
    ["@terraverse/craft", "Craft", "Forms × materials: knap, carve, saw and forge at a bench. The official crafting layer.", ["survival", "library"], 380, 12400, 6, { official: true, installed: true }],
    ["@terraverse/race", "Rally", "Point-to-point races over real terrain. Build the car, then beat the clock.", ["games", "machines"], 221, 4120, 12, { official: true }],
    ["@ash/kilns", "Kilns", "Fired clay, brick and glass from heat you have to build and feed.", ["matter", "building"], 164, 2210, 2],
    ["@wren/rails", "Iron Rails", "Track, points and wagons on Jolt. Hauling ore stops being a crawl.", ["machines"], 301, 5530, 9],
    ["@juno/stonework", "Stonework", "A warm, chunky texture pack for stone, brick and timber.", ["textures"], 97, 3300, 21, { type: "textures", installed: true }],
    ["@juno/clear-skies", "Clear Skies", "Brighter skies and quieter terrain colours.", ["textures"], 58, 1210, 40, { type: "textures" }],
    ["@pell/stealth", "Shadowplay", "Light and sound decide who sees you. Hide-and-seek with real optics.", ["games"], 140, 1870, 5],
    ["@marrow/waterwheel", "Waterwheels", "Mills, wheels and shafts that turn on rivers the world already has.", ["machines", "matter"], 188, 2690, 15],
    ["@ash/towns", "Towns", "Claims, plots and a town hall: building together on a busy server.", ["building", "survival"], 76, 980, 30],
    ["@tidal/biomes-plus", "Biomes Plus", "More plants and ground for the generator's wetter climates.", ["world"], 119, 2040, 8],
    ["@lumen/lib-ui", "lib-ui", "Shared panels and HUD widgets for other mods.", ["library"], 44, 5400, 60],
    ["@pell/orbit-race", "Orbit Race", "First to the Moon and back. Creative rockets, real gravity.", ["games", "machines"], 33, 210, 1],
    ["@sable/ores-of-old", "Ores of Old", "Deeper, rarer deposits worth engineering a route to.", ["world", "survival"], 61, 870, 18],
]

const LISTINGS: RegistryListing[] = SEEDS.map(([name, title, description, categories, stars, installs, days, extra]) => ({
    name: name, title: title, description: description, owner: name.slice(1, name.indexOf("/")),
    latestVersion: `${1 + (stars % 3)}.${stars % 7}.${installs % 5}`, stars: stars, updatedAt: ago(days),
    type: "mod", categories: categories, icon: null, banner: null, installs: installs, installed: false, official: false,
    ...extra,
}))

const SORTS = {
    relevance: (a: RegistryListing, b: RegistryListing) => b.stars - a.stars,
    trending: (a: RegistryListing, b: RegistryListing) => b.stars / (1 + age(b)) - a.stars / (1 + age(a)),
    stars: (a: RegistryListing, b: RegistryListing) => b.stars - a.stars,
    new: (a: RegistryListing, b: RegistryListing) => b.updatedAt.localeCompare(a.updatedAt),
    updated: (a: RegistryListing, b: RegistryListing) => b.updatedAt.localeCompare(a.updatedAt),
}
function age(l: RegistryListing) { return (Date.now() - Date.parse(l.updatedAt)) / DAY }

function search(s: RegistrySearch): RegistryResults {
    const q = s.query.toLowerCase()
    const pageSize = 12
    const hits = LISTINGS.filter((l) => (!q || (l.name + " " + (l.title ?? "") + " " + l.description + " " + l.owner).toLowerCase().includes(q))
        && (!s.type || l.type === s.type) && s.categories.every((c) => l.categories.includes(c)) && (!s.installed || l.installed))
        .sort(SORTS[s.sort])
    return { listings: hits.slice((s.page - 1) * pageSize, s.page * pageSize), total: hits.length, page: s.page, pageSize: pageSize }
}

function front(): RegistryFront {
    const shelf = (id: string, title: string, s: Partial<RegistrySearch>) =>
        ({ id: id, title: title, search: s, listings: search({ query: "", sort: "trending", categories: [], type: null, installed: false, page: 1, ...s }).listings.slice(0, 6) })
    return {
        featured: LISTINGS.filter((l) => l.official),
        categories: CATEGORIES.map((c) => ({ ...c, count: LISTINGS.filter((l) => l.categories.includes(c.id)).length })),
        shelves: [
            shelf("trending", "Trending mods", { sort: "trending", type: "mod" }),
            shelf("new", "New this week", { sort: "new" }),
            shelf("textures", "Texture packs", { type: "textures", sort: "stars" }),
        ],
    }
}

/** A screenshot stand-in: a striped SVG, so README images have something real to draw. */
function shot(label: string, hue: number): string {
    const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="640" height="300"><rect width="640" height="300" fill="hsl(${hue} 30% 14%)"/><rect y="190" width="640" height="110" fill="hsl(${hue} 35% 28%)"/><rect y="230" width="640" height="70" fill="hsl(${hue} 30% 20%)"/><text x="24" y="44" fill="#e8ecef" font-family="monospace" font-size="20">${label}</text></svg>`
    return "data:image/svg+xml," + encodeURIComponent(svg).replace(/\(/g, "%28").replace(/\)/g, "%29")
}

function mod(name: string): RegistryMod {
    const listing = LISTINGS.find((l) => l.name === name)
    if (!listing) throw new Error(`${name} is not on the registry`)
    const title = listing.title ?? name
    const readme = `# ${title}

${listing.description}

![${title} in play](${shot(title + " — in play", (listing.stars * 37) % 360)})

## Getting started

Add it to your server, then restart the world:

\`\`\`sh
terra add ${name}
\`\`\`

- Works on **any planet** the generator makes
- Settings live in \`mod.config.ts\`; see the [docs](https://terraverse.dev/docs/mods)
- Needs \`@terraverse/craft\` for its recipes

> Feedback welcome: open an issue on the source.

![The settings](${shot("Settings", (listing.stars * 53) % 360)})
`
    return {
        ...listing, readme: readme, deprecated: null, media: null,
        versions: [0, 1, 2, 3].map((i) => ({
            version: i === 0 ? listing.latestVersion : `${listing.latestVersion.split(".")[0]}.${i}.0`,
            publishedAt: ago(Math.floor(age(listing)) + i * 9), api: "3", title: title,
            dependencies: i < 2 ? CRAFT : {}, withheld: i === 2 ? "crashed on load in 0.3" : null,
        })),
        files: [
            { path: "package.json", size: 412 }, { path: "README.md", size: readme.length }, { path: "mod.config.ts", size: 640 },
            { path: "server/main.ts", size: 4210 }, { path: "server/rules.ts", size: 2380 }, { path: "server/state.ts", size: 1190 },
            { path: "assets/icon.png", size: 2048 },
        ],
    }
}

function file(name: string, path: string): string {
    const m = mod(name)
    if (path === "README.md") return m.readme
    if (path === "package.json") return JSON.stringify({ name: name, version: m.latestVersion, description: m.description, dependencies: m.versions[0]?.dependencies ?? {} }, null, 2)
    if (!m.files?.some((f) => f.path === path)) throw new Error(`${path} is not in ${name}`)
    return `// ${path}: ${m.title}\n\nexport default function main() {\n    world.on("tick", () => {\n        // the gallery's stand-in source\n    })\n}\n`
}

export const mockRegistry = { front: front, search: search, mod: mod, file: file }
