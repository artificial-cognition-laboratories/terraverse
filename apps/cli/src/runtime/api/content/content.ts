import type { ChannelT } from "../../channel"
import type { RunningT } from "../../running"
import { checks } from "../checks"
import { Blocks, type BlockRow } from "./blocks"
import { Media, type MediaFile } from "./media"

/** What the engine is sent once every mod has loaded. */
export type ContentTable = { readonly blocks: readonly BlockRow[]; readonly media: readonly MediaFile[] }

/** Content — what things are, declared while mods load and sealed after: the `content` global. */
export function Content(opts: { running: RunningT; channel: ChannelT }) {
    const media = Media({})
    const blocks = Blocks({ running: opts.running, media: media, channel: opts.channel })

    const api = Object.freeze({
        blocks: Object.freeze({
            create: blocks.create,
            get: () => checks.unwired("content.blocks.get"),
            list: () => checks.unwired("content.blocks.list"),
        }),
        get substances() { return checks.unwired("content.substances") },
        get mixtures() { return checks.unwired("content.mixtures") },
        get reactions() { return checks.unwired("content.reactions") },
        get structures() { return checks.unwired("content.structures") },
        get items() { return checks.unwired("content.items") },
    })

    function seal(): ContentTable {
        return { blocks: blocks.seal(), media: media.list() }
    }

    return { api: api, seal: seal }
}

export type ContentT = ReturnType<typeof Content>
