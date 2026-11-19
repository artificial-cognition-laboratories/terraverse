import { mkdirSync, rmSync } from "node:fs"
import { KINDS, type ProjectKind, type TypeFrame } from "../kinds"
import type { FrameT } from "../frame"
import { GLOBALS, type GlobalsName } from "./globals.gen"

/**
 * The mod API this terra writes types for. A mod records it when it's published, and the
 * engine keeps loading mods built against older versions; a mod built against a newer one
 * needs a newer terra. Bump it when the globals change in a way an old mod can't follow.
 */
export const MOD_API = 1

/** What every type frame compiles with: a Bun-style project, checked strictly, never emitted. */
const COMPILER_OPTIONS = {
    target: "ESNext",
    module: "Preserve",
    moduleResolution: "bundler",
    lib: ["ESNext"],
    types: [],
    strict: true,
    noEmit: true,
    skipLibCheck: true,
    allowImportingTsExtensions: true,
    resolveJsonModule: true,
}

/**
 * Typegen — a project's .terra/types/: the engine's globals, and a tsconfig for each part
 * of it (KINDS' `types`) seeing only the globals that part runs with. Written whole every
 * time: nothing in it is anyone's to edit.
 */
export function Typegen(opts: { frame: FrameT }) {
    const dir = opts.frame.path("types")

    /** Everything, anew: the globals, each frame's tsconfig, and the settings' types. */
    async function write(kind: ProjectKind, settingsTypes: string): Promise<void> {
        const frames = KINDS[kind].types
        rmSync(dir, { recursive: true, force: true })
        mkdirSync(dir, { recursive: true })
        const used = new Set<GlobalsName>(frames.flatMap(f => f.globals))
        for (const name of used) await Bun.write(`${dir}/${name}.d.ts`, GLOBALS[name])
        for (const frame of frames) await Bun.write(`${dir}/${frame.name}.json`, JSON.stringify(tsconfig(frame), null, 2) + "\n")
        // which API these types are, so publish records what the project was built against
        await Bun.write(`${dir}/api.json`, JSON.stringify({ api: MOD_API }) + "\n")
        await settings(settingsTypes)
    }

    /** The settings' types alone: a server knows its mods' only once it's read them. */
    async function settings(text: string): Promise<void> {
        await Bun.write(`${dir}/settings.d.ts`, text)
    }

    return { dir: dir, write: write, settings: settings }
}

export type TypegenT = ReturnType<typeof Typegen>

/** A frame's generated tsconfig. Its paths are from .terra/types/, two folders down from the root. */
function tsconfig(frame: TypeFrame): object {
    const fromRoot = (glob: string) => `../../${glob}`
    return {
        compilerOptions: COMPILER_OPTIONS,
        // files, not include: exclude (which keeps .terra/ out of a root frame) never drops them
        files: [...frame.globals.map(g => `./${g}.d.ts`), ...(frame.settings ? ["./settings.d.ts"] : [])],
        include: frame.include.map(fromRoot),
        exclude: frame.exclude.map(fromRoot),
    }
}
