import { err } from "../../../errors"
import type { ManifestMod } from "../../../terra"
import type { RunningT } from "../../running"
import { checks } from "../checks"
import type { MediaT } from "./media"

/** One block as the engine is sent it. */
export type BlockRow = {
    readonly name: string
    readonly description: string
    /** top, bottom, then the four sides: media names. */
    readonly tiles: readonly string[]
    readonly matter: string
    readonly share: number
}

type BlocksOpts = {
    running: RunningT
    media: MediaT
}

const NAME = /^[a-z0-9_]+:[a-z0-9_]+$/

/** Blocks — the blocks mods declare while they load. Sealed once they all have. */
export function Blocks(opts: BlocksOpts) {
    const rows = new Map<string, BlockRow>()
    let sealed = false

    function create(name: unknown, def: unknown): { readonly name: string } {
        const mod = opts.running.loadingMod()
        if (sealed || !mod) throw err("CONTENT_SEALED", { detail: `${String(name)}: blocks are declared while mods load` })
        if (typeof name !== "string" || !NAME.test(name)) throw err("NAME_INVALID", { detail: `${JSON.stringify(name)} isn't prefix:lower_case` })
        if (!name.startsWith(`${mod.prefix}:`)) throw err("NAME_NOT_YOURS", { detail: `${mod.name} creates ${mod.prefix}:…, not ${name}` })
        if (rows.has(name)) throw err("NAME_TAKEN", { detail: `${name} was already declared` })
        const row = shape(mod, name, def)
        rows.set(name, row)
        return Object.freeze({ name: name })
    }

    /** Every block declared, and no more from now on. */
    function seal(): BlockRow[] {
        sealed = true
        return [...rows.values()]
    }

    function shape(mod: ManifestMod, name: string, def: unknown): BlockRow {
        if (typeof def !== "object" || def === null) throw err("ARGUMENT_INVALID", { detail: `${name} needs a definition` })
        const matter: unknown = Reflect.get(def, "matter")
        const share: unknown = Reflect.get(def, "share") ?? 1
        const texture: unknown = Reflect.get(def, "texture")
        if (typeof matter !== "string") throw err("MATTER_UNKNOWN", { detail: `${name}: say what it's made of (matter)` })
        if (typeof share !== "number" || !(share > 0 && share <= 1)) throw err("ARGUMENT_INVALID", { detail: `${name}: share is more than 0, up to 1` })
        if (Reflect.get(def, "sounds") !== undefined) checks.unwired("a block's sounds")
        if (Reflect.get(def, "on") !== undefined) checks.unwired("a block's handlers (on)")
        return { name: name, description: describe(name), tiles: tiles(mod, texture), matter: matter, share: share }
    }

    function tiles(mod: ManifestMod, texture: unknown): string[] {
        if (typeof texture === "string") return Array<string>(6).fill(opts.media.texture(mod, texture))
        if (typeof texture !== "object" || texture === null)
            throw err("ASSET_MISSING", { detail: "texture is a file, or { top?, bottom?, sides }" })
        const sides = opts.media.texture(mod, Reflect.get(texture, "sides"))
        const top: unknown = Reflect.get(texture, "top")
        const bottom: unknown = Reflect.get(texture, "bottom")
        return [
            top === undefined ? sides : opts.media.texture(mod, top),
            bottom === undefined ? sides : opts.media.texture(mod, bottom),
            sides, sides, sides, sides,
        ]
    }

    return { create: create, seal: seal }
}

export type BlocksT = ReturnType<typeof Blocks>

/** "kilns:granite_brick" → "Granite brick", until mods can say their own. */
function describe(name: string): string {
    const words = name.slice(name.indexOf(":") + 1).replaceAll("_", " ")
    return words.charAt(0).toUpperCase() + words.slice(1)
}
