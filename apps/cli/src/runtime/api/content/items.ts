import { err } from "../../../errors"
import type { RunningT } from "../../running"
import { describe } from "./blocks"
import type { MediaT } from "./media"

/** One item as the engine is sent it: a thing held in the hand, drawn by its icon. */
export type ItemRow = {
    readonly name: string
    readonly description: string
    /** A media name: shown in the inventory, and extruded into a thick sprite when held. */
    readonly icon: string
}

const NAME = /^[a-z0-9_]+:[a-z0-9_]+$/

/** Items — the hand-sized things mods declare while they load. Sealed once they all have. */
export function Items(opts: { running: RunningT; media: MediaT }) {
    const rows = new Map<string, ItemRow>()
    let sealed = false

    function create(name: unknown, def: unknown): { readonly name: string } {
        const mod = opts.running.loadingMod()
        if (sealed || !mod) throw err("CONTENT_SEALED", { detail: `${String(name)}: items are declared while mods load` })
        if (typeof name !== "string" || !NAME.test(name)) throw err("NAME_INVALID", { detail: `${JSON.stringify(name)} isn't prefix:lower_case` })
        if (!name.startsWith(`${mod.prefix}:`)) throw err("NAME_NOT_YOURS", { detail: `${mod.name} creates ${mod.prefix}:…, not ${name}` })
        if (rows.has(name)) throw err("NAME_TAKEN", { detail: `${name} was already declared` })
        if (typeof def !== "object" || def === null) throw err("ARGUMENT_INVALID", { detail: `${name} needs a definition` })
        const description: unknown = Reflect.get(def, "description") ?? describe(name)
        if (typeof description !== "string" || description === "") throw err("ARGUMENT_INVALID", { detail: `${name}: description is what players see it called` })
        const icon = Reflect.get(def, "icon")
        if (icon === undefined) throw err("ASSET_MISSING", { detail: `${name}: an item needs an icon (a file, or art.texture)` })
        rows.set(name, { name: name, description: description, icon: opts.media.texture(mod, icon) })
        return Object.freeze({ name: name })
    }

    /** Every item declared, and no more from now on. */
    function seal(): ItemRow[] {
        sealed = true
        return [...rows.values()]
    }

    return { create: create, seal: seal }
}

export type ItemsT = ReturnType<typeof Items>
