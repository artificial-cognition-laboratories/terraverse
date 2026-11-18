import { err } from "../errors"
import { MODS_GLOBAL, type ManifestMod } from "../terra"
import type { RunningT } from "./running"

/**
 * Loader — loads each mod's server half, in dependency order. Importing it is loading
 * it. What it exports is what the mods after it import (see terra's build Imports).
 */
export function Loader(opts: { running: RunningT }) {
    const loaded: Record<string, unknown> = {}

    async function load(mods: readonly ManifestMod[]): Promise<void> {
        Object.defineProperty(globalThis, MODS_GLOBAL, { value: loaded, configurable: true })
        for (const mod of mods) {
            const server = mod.server
            if (!server) continue
            loaded[mod.name] = await opts.running.load(mod, () => import(server)).catch((error: unknown) => {
                // a mod that can't load leaves a universe with holes in it: nothing starts
                opts.running.report(mod.name, error)
                throw err("MOD_LOAD_FAILED", { detail: mod.name, context: { mod: mod.name } })
            })
        }
    }

    return { load: load }
}

export type LoaderT = ReturnType<typeof Loader>
