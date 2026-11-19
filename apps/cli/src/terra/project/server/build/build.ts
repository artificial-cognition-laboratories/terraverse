import { cpSync, existsSync, mkdirSync, renameSync, rmSync } from "node:fs"
import { join } from "node:path"
import { err } from "../../../../errors"
import type { FrameT } from "../../frame"
import type { SettingValue } from "../../settings"
import type { ManifestMod, Mod } from "../../types"
import { Imports, MOD_OF, type ImportsT } from "./imports"

/**
 * Build — bundles each mod's server half into .terra/build/<prefix>/server.js, with a copy
 * of its assets/ beside it, so .terra/ alone is what runs: a server folder can be moved or
 * shipped prepared (the game's own is), with nothing pointing back at where its mods were.
 *
 * Built beside the last build and swapped in whole, so nothing reads half of one.
 */
export function Build(opts: { frame: FrameT }) {
    const build = opts.frame.path("build")
    const next = `${build}.next`

    async function run(mods: readonly Mod[], chosen: ReadonlyMap<string, Readonly<Record<string, SettingValue>>>): Promise<ManifestMod[]> {
        const imports = Imports({ mods: mods })
        rmSync(next, { recursive: true, force: true })
        mkdirSync(next, { recursive: true })
        const built: ManifestMod[] = []
        for (const mod of mods) {
            const server = await half(mod, imports)
            const assets = `${mod.root}/assets`
            mkdirSync(`${next}/${mod.prefix}`, { recursive: true })
            if (existsSync(assets)) cpSync(assets, `${next}/${mod.prefix}/assets`, { recursive: true })
            built.push({ name: mod.name, prefix: mod.prefix, version: mod.version, root: join(build, mod.prefix), server: server, settings: chosen.get(mod.name) ?? {} })
        }
        rmSync(build, { recursive: true, force: true })
        renameSync(next, build)
        return built
    }

    /** One mod's server half, or null if it has none. */
    async function half(mod: Mod, imports: ImportsT): Promise<string | null> {
        const entry = `${mod.root}/server/main.ts`
        if (!existsSync(entry)) return null
        const result = await Bun.build({
            entrypoints: [entry],
            target: "bun",
            format: "esm",
            outdir: `${next}/${mod.prefix}`,
            naming: "server.js",
            sourcemap: "inline",    // a mod's stack traces name its own .ts lines
            // `mod`, bound for this bundle alone: lexical, so a mod's code always reads its own
            banner: `const mod = globalThis.${MOD_OF}(${JSON.stringify(mod.name)});`,
            plugins: [imports.plugin(mod)],
        }).catch((e: unknown) => {
            // the bundler's messages name the file and line: they are the detail
            const said = e instanceof AggregateError ? e.errors.map(where).join("\n") : String(e)
            throw imports.refusal() ?? err("BUILD_FAILED", { detail: `${mod.name}: ${said}`, context: { mod: mod.name } })
        })
        if (!result.success)
            throw err("BUILD_FAILED", { detail: `${mod.name}: ${result.logs.map(l => String(l)).join("\n")}`, context: { mod: mod.name } })
        // join, as the manifest resolves them on reading: native separators, so both agree on Windows
        return join(build, mod.prefix, "server.js")
    }

    return { run: run }
}

export type BuildT = ReturnType<typeof Build>

/** A bundler message as `file:line:column message`, where it has a position. */
function where(message: unknown): string {
    if (typeof message !== "object" || message === null) return String(message)
    const text = String(Reflect.get(message, "message") ?? message)
    const at: unknown = Reflect.get(message, "position")
    if (typeof at !== "object" || at === null) return text
    return `${Reflect.get(at, "file")}:${Reflect.get(at, "line")}:${Reflect.get(at, "column")} ${text}`
}
