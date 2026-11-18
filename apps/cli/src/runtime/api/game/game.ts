import type { ChannelT, Step } from "../../channel"
import type { RunningT } from "../../running"
import { Call } from "../../wire"
import { checks } from "../checks"
import type { HandlersT } from "../handlers"
import { Players } from "./players"
import { Time } from "./time"

type GameOpts = {
    channel: ChannelT
    running: RunningT
    handlers: HandlersT
}

/** Game — what's being played: the `game` global, its players and its clock. */
export function Game(opts: GameOpts) {
    const players = Players({ channel: opts.channel, handlers: opts.handlers })
    const time = Time({ running: opts.running })

    const api = Object.freeze({
        players: players.api,
        time: time.api,
        announce: (message: string) => opts.channel.tell(Call["game.announce"], String(message)),
        get worlds() { return checks.unwired("game.worlds") },
        get commands() { return checks.unwired("game.commands") },
        get system() { return checks.unwired("game.system") },
        snapshot: () => checks.unwired("game.snapshot"),
        reset: () => checks.unwired("game.reset"),
        stop: () => checks.unwired("game.stop"),
    })

    /** A window: who came and went, then the clock. */
    function step(step: Step): void {
        players.step(step)
        time.step(step.now)
    }

    return { api: api, step: step }
}

export type GameT = ReturnType<typeof Game>
