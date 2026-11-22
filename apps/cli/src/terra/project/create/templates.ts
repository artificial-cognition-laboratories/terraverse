import { names } from "../names"
/** What a scaffold needs: the package's name and its title. */
export type ScaffoldInput = {
    readonly name: string
    readonly title: string
    /** A fresh star system, as a world code: what a new server is made from. */
    readonly system: string
}

/** A new project's files, by path from its root. Only what the author owns: prepare writes the rest. */
export type Scaffold = { readonly files: Readonly<Record<string, string>>; readonly dirs: readonly string[] }

const json = (value: unknown) => JSON.stringify(value, null, 4) + "\n"
const GITIGNORE = "node_modules/\n.terra/\n"

/** Templates — what `terra <kind> init` writes, per kind. Pure: input in, files out. */
export const templates = {
    server(input: ScaffoldInput): Scaffold {
        return {
            files: {
                "package.json": json({ name: input.name, version: "0.1.0", private: true, type: "module", dependencies: {} }),
                "server.config.ts": `export default defineServer({
    title: ${JSON.stringify(input.title)},

    // The star system, as a world code: every planet in it is a world here. Fixed once
    // the universe is made: a different system is a different universe.
    system: ${JSON.stringify(input.system)},

    access: {
        join: "friends",
    },
})
`,
                "tsconfig.json": json({ extends: "./.terra/types/root.json" }),
                ".gitignore": GITIGNORE,
            },
            dirs: ["mods"],
        }
    },

    mod(input: ScaffoldInput): Scaffold {
        return {
            files: {
                "package.json": json({ name: input.name, version: "0.1.0", type: "module", exports: { ".": "./server/main.ts" } }),
                "mod.config.ts": `export default defineMod({
    title: ${JSON.stringify(input.title)},
})
`,
                "server/main.ts": `// The server half: runs once as the universe loads, then reacts to what happens.
// What's available here: https://terraverse.arclabs.it/docs/mods/server
import * as pictures from "./art"

// A block or an item, drawn by art.ts: \`terra art render\` shows them without the game.
// content.blocks.create("${names.prefix(input.name)}:example", { matter: "granite", texture: pictures.example })

game.players.on("join", player => player.tell(\`Welcome, \${player.name}.\`))
`,
                "server/art.ts": `// This mod's pictures, drawn from code with the \`art\` global: a palette, letter grids and
// procedural layers. \`terra art render\` draws them all onto a sheet; \`terra check\` checks them.

export const example = art.texture({ layers: [{ noise: ["#999880", "#9c9b85"], scale: 8 }] })
`,
                "server/tsconfig.json": json({ extends: "../.terra/types/server.json" }),
                "client/tsconfig.json": json({ extends: "../.terra/types/client.json" }),
                "tsconfig.json": json({ extends: "./.terra/types/root.json" }),
                ".gitignore": GITIGNORE,
            },
            dirs: ["assets/textures"],
        }
    },
}
