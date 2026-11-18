import type { BunPlugin } from "bun"
import { err, type TerraError } from "../../../../errors"
import type { Mod } from "../../types"

/**
 * Where the sidecar keeps each loaded mod's exports, by name. The contract between
 * Imports (which reads it) and the runtime's loader (which fills it).
 */
export const MODS_GLOBAL = "__terraMods"

/**
 * What a mod's bundle asks for its own `mod`: a function from its name to its identity and
 * settings. Each bundle binds `const mod` from it (Build's banner), so `mod` is lexical: in
 * hello's code it's hello, whichever mod called that code.
 */
export const MOD_OF = "__terraModOf"

/**
 * Imports — a mod's import of another mod is that mod's one loaded module, never a copy
 * bundled in. Every mod loads after the mods it builds on, so by the time an import runs
 * the sidecar has its exports. (The compiled terra doesn't resolve packages from disk, so
 * this can't be left to the filesystem either.)
 */
export function Imports(opts: { mods: readonly Mod[] }) {
    const byName = new Map(opts.mods.map(m => [m.name, m]))
    // The bundler turns what a plugin throws into a plain message, code and all lost, so
    // the refusal is kept here for Build to rethrow as it is.
    let refused: TerraError | null = null

    function refuse(error: TerraError): never {
        refused = error
        throw error
    }

    /** Why the last build was refused here, if it was. */
    function refusal(): TerraError | null {
        return refused
    }

    /** The bundler plugin for one mod's server half. */
    function plugin(self: Mod): BunPlugin {
        return {
            name: "terra-mods",
            setup(build) {
                build.onResolve({ filter: /^[@a-z]/ }, args => {
                    const name = packageOf(args.path)
                    const mod = byName.get(name)
                    if (!mod || mod === self) return undefined
                    if (!self.needs.includes(name))
                        refuse(err("MOD_UNDECLARED", { detail: `${self.name} imports ${name}`, context: { mod: self.name, imports: name } }))
                    if (args.path !== name)
                        refuse(err("MOD_SUBPATH", { detail: `${self.name} imports ${args.path}; import ${name}`, context: { mod: self.name } }))
                    return { path: name, namespace: "terra-mod" }
                })
                build.onLoad({ filter: /.*/, namespace: "terra-mod" }, args => ({
                    contents: `module.exports = globalThis.${MODS_GLOBAL}[${JSON.stringify(args.path)}]`,
                    loader: "js",
                }))
            },
        }
    }

    return { plugin: plugin, refusal: refusal }
}

export type ImportsT = ReturnType<typeof Imports>

/** "@you/kilns/x" → "@you/kilns", "lodash/fp" → "lodash". */
function packageOf(specifier: string): string {
    const parts = specifier.split("/")
    return specifier.startsWith("@") ? parts.slice(0, 2).join("/") : parts[0] ?? specifier
}
