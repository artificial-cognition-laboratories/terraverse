import type { ChannelT } from "../channel"
import type { RunningT } from "../running"
import { Content } from "./content"
import { Game } from "./game"
import type { ManifestMod } from "../../terra"
import { Handlers } from "./handlers"
import { ModApi } from "./mod"
import { Refused } from "./refused"

/** Api — what a mod's server half sees: content, game and Refused as globals, and its own `mod`. */
export function Api(opts: { channel: ChannelT; running: RunningT; mods: readonly ManifestMod[] }) {
    const handlers = Handlers({ running: opts.running })
    const content = Content({ running: opts.running, channel: opts.channel })
    const game = Game({ channel: opts.channel, running: opts.running, handlers: handlers })
    const mod = ModApi({ mods: opts.mods })

    const globals = { content: content.api, game: game.api, Refused: Refused }

    return {
        globals: globals,
        /** Each bundle's `mod`, by name (installed as MOD_OF). */
        modOf: mod.of,
        seal: content.seal,
        step: game.step,
    }
}

export type ApiT = ReturnType<typeof Api>
