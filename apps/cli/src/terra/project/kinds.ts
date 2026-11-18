import { existsSync } from "node:fs"
import { templates, type Scaffold, type ScaffoldInput } from "./create/templates"
import type { GlobalsName } from "./typegen"

/**
 * How one kind of project differs from the others, and nothing else.
 *
 * Every per-kind decision reads a field here, so a kind is never an `if` repeated across
 * prepare, publish and typegen. Adding a kind (a texture pack, shaders, skins) is adding
 * a row; something about a kind that can't be a field is the signal to widen the table.
 */
export type KindSpec = {
    /** The file whose presence identifies the kind. */
    readonly config: string
    /** The global its config is written with: identity while the config loads. */
    readonly define: "defineServer" | "defineMod"
    /** Whether prepare installs package.json's dependencies first. */
    readonly installs: boolean
    /** Whether it runs mods: prepare finds them, builds them, and writes a manifest for the sidecar. */
    readonly runs: boolean
    /** Whether its config declares settings for a server to choose (a mod's does). */
    readonly declares: boolean
    /**
     * Its type frames: one per part of the project that sees its own globals. Each is a
     * tsconfig.json the project owns at `at`, extending .terra/types/<name>.json, which
     * prepare writes. Globs are from the project's root.
     */
    readonly types: readonly TypeFrame[]
    /** What `terra <kind> init` writes: only what the author owns, never .terra/. */
    readonly scaffold: (input: ScaffoldInput) => Scaffold
}

export type TypeFrame = {
    readonly name: string
    /** Where its tsconfig.json sits, from the root: the nearest one is the one an editor uses. */
    readonly at: string
    readonly include: readonly string[]
    readonly exclude: readonly string[]
    readonly globals: readonly GlobalsName[]
    /** Whether it sees the settings types prepare writes (settings.d.ts): `mod.settings`, or a server's `settings`. */
    readonly settings: boolean
}

export const KINDS = {
    server: {
        config: "server.config.ts",
        define: "defineServer",
        installs: true,
        runs: true,
        declares: false,
        types: [
            // local mods in mods/ are projects of their own, with their own frames
            { name: "root", at: ".", include: ["**/*.ts"], exclude: ["mods", "save", "node_modules", ".terra"], globals: ["shared", "config"], settings: true },
        ],
        scaffold: templates.server,
    },
    mod: {
        config: "mod.config.ts",
        define: "defineMod",
        installs: true,
        runs: false,
        declares: true,
        types: [
            // the config, and any other folder: code both halves share
            { name: "root", at: ".", include: ["**/*.ts"], exclude: ["server", "client", "node_modules", ".terra"], globals: ["shared", "config"], settings: false },
            { name: "server", at: "server", include: ["server/**/*.ts"], exclude: [], globals: ["shared", "server"], settings: true },
            { name: "client", at: "client", include: ["client/**/*.ts", "client/**/*.vue"], exclude: [], globals: ["shared", "client"], settings: false },
        ],
        scaffold: templates.mod,
    },
} as const satisfies Record<string, KindSpec>

export type ProjectKind = keyof typeof KINDS

/** Checked in this order: a folder is the first kind it has the config of. */
const ORDER: readonly ProjectKind[] = ["server", "mod"]

/** The kind of project `root` is, or null. A project has a package.json and its kind's config. */
export function detectKind(root: string): ProjectKind | null {
    if (!existsSync(`${root}/package.json`)) return null
    return ORDER.find(kind => existsSync(`${root}/${KINDS[kind].config}`)) ?? null
}
