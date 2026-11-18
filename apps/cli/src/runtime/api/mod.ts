import { err } from "../../errors"
import type { ManifestMod } from "../../terra"
import { checks } from "./checks"

/**
 * ModApi — each mod's `mod`: who it is and what the server chose for it. Not one global:
 * each mod's bundle binds its own (terra's Build), by name, from `of`, so `mod` in hello's
 * code is hello's whoever calls that code.
 */
export function ModApi(opts: { mods: readonly ManifestMod[] }) {
    const apis = new Map(opts.mods.map(m => [m.name, Object.freeze({
        name: m.name,
        prefix: m.prefix,
        version: m.version,
        settings: Object.freeze({ ...m.settings }),
        get store() { return checks.unwired("mod.store") },
    })]))

    /** A mod's own `mod`. */
    function of(name: string) {
        const api = apis.get(name)
        if (!api) throw err("PROTOCOL", { detail: `a bundle asked for ${name}'s mod, which this server doesn't run` })
        return api
    }

    return { of: of }
}

export type ModApiT = ReturnType<typeof ModApi>
