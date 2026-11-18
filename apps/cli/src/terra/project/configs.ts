import { err } from "../../errors"
import { KINDS, type ProjectKind } from "./kinds"
import { settings } from "./settings"
import type { ConfigData } from "./types"

/**
 * Configs — evaluates a project's config file (server.config.ts, mod.config.ts). It's
 * read without running the project: its define function is only the identity while it
 * loads.
 */
export function Configs(_opts: Record<string, never>) {
    /** The object a `kind` project's config at `root` exports, as written. */
    async function evaluate(kind: ProjectKind, root: string): Promise<object> {
        const spec = KINDS[kind]
        const path = `${root}/${spec.config}`
        Reflect.set(globalThis, spec.define, (config: unknown) => config)
        // a query, so an edited config is read again rather than served from the module cache
        const loaded: unknown = await import(`${path}?at=${Bun.file(path).lastModified}`).catch((e: unknown) => {
            throw err("CONFIG_INVALID", { detail: `${path} threw while loading`, cause: e })
        })
        const config: unknown = typeof loaded === "object" && loaded !== null ? Reflect.get(loaded, "default") : undefined
        if (typeof config !== "object" || config === null)
            throw err("CONFIG_INVALID", { detail: `${path} must export default ${spec.define}({ … })` })
        return config
    }

    /** What every kind's config says: its title, or the package's name. */
    async function read(kind: ProjectKind, root: string, name: string): Promise<ConfigData> {
        const config = await evaluate(kind, root)
        const title: unknown = Reflect.get(config, "title") ?? name
        if (typeof title !== "string") throw err("CONFIG_INVALID", { detail: `${KINDS[kind].config}: title must be a string` })
        const declared = KINDS[kind].declares ? settings.declarations(Reflect.get(config, "settings"), `${name}'s ${KINDS[kind].config}`) : {}
        return { title: title, settings: declared }
    }

    return { evaluate: evaluate, read: read }
}

export type ConfigsT = ReturnType<typeof Configs>
