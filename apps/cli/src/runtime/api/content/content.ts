import type { ChannelT } from "../../channel"
import type { RunningT } from "../../running"
import { Art } from "../art"
import { checks } from "../checks"
import { Blocks, type BlockRow } from "./blocks"
import { Items, type ItemRow } from "./items"
import { Media, type MediaFile } from "./media"

/** What the engine is sent once every mod has loaded. */
export type ContentTable = {
    readonly blocks: readonly BlockRow[]
    readonly items: readonly ItemRow[]
    readonly media: readonly MediaFile[]
}

/** Content — what things are, declared while mods load and sealed after: the `content` and `art` globals. */
export function Content(opts: { running: RunningT; channel: ChannelT }) {
    const media = Media({})
    const blocks = Blocks({ running: opts.running, media: media })
    const items = Items({ running: opts.running, media: media })
    const art = Art({ running: opts.running, media: media })

    const api = Object.freeze({
        blocks: Object.freeze({
            create: blocks.create,
            get: () => checks.unwired("content.blocks.get"),
            list: () => checks.unwired("content.blocks.list"),
        }),
        items: Object.freeze({
            create: items.create,
            get: () => checks.unwired("content.items.get"),
            list: () => checks.unwired("content.items.list"),
        }),
        get substances() { return checks.unwired("content.substances") },
        get mixtures() { return checks.unwired("content.mixtures") },
        get reactions() { return checks.unwired("content.reactions") },
        get structures() { return checks.unwired("content.structures") },
    })

    function seal(): ContentTable {
        return { blocks: blocks.seal(), items: items.seal(), media: media.list() }
    }

    return { api: api, art: art.api, seal: seal }
}

export type ContentT = ReturnType<typeof Content>
