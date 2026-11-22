import { err } from "../../../errors"
import type { RunningT } from "../../running"
import type { MediaT } from "../content/media"
import { paint } from "./paint"
import { spec } from "./spec"

/**
 * Art — textures drawn from code: the `art` global. A mod says what a picture is (a grid,
 * a palette, a few procedural layers) and it's painted here, into a PNG among the mod's
 * media, so no picture is ever hand-made or generated outside the mod's own source.
 */
export function Art(opts: { running: RunningT; media: MediaT }) {
    /** A texture, painted now: what a block's `texture` or an item's `icon` takes. */
    function texture(def: unknown): object {
        const mod = opts.running.loadingMod()
        if (!mod) throw err("CONTENT_SEALED", { detail: "art is drawn while mods load" })
        const s = spec.read(def)
        return opts.media.drawn(mod, paint.draw(s))
    }

    const api = Object.freeze({ texture: texture })

    return { api: api }
}

export type ArtT = ReturnType<typeof Art>
