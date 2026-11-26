import type { ChannelT, Step } from "../../channel"
import type { RunningT } from "../../running"
import { Call } from "../../wire"
import { checks } from "../checks"
import type { HandlersT } from "../handlers"
import { Commands } from "./commands"
import { Players } from "./players"
import { Time } from "./time"
import { Worlds } from "./worlds"

type GameOpts = {
    channel: ChannelT
    running: RunningT
    handlers: HandlersT
}

/** Game — what's being played: the `game` global, its players and its clock. */
export function Game(opts: GameOpts) {
    const worlds = Worlds({ channel: opts.channel })
    const players = Players({ channel: opts.channel, handlers: opts.handlers, worlds: worlds })
    const time = Time({ running: opts.running, channel: opts.channel })
    const commands = Commands({ channel: opts.channel, running: opts.running, players: players.api })

    const api = Object.freeze({
        players: players.api,
        time: time.api,
        announce: (message: string) => opts.channel.tell(Call["game.announce"], String(message)),
        get worlds() { return checks.unwired("game.worlds") },
        commands: commands.api,
        get system() { return checks.unwired("game.system") },
        snapshot: () => checks.unwired("game.snapshot"),
        reset: () => checks.unwired("game.reset"),
        stop: () => checks.unwired("game.stop"),
    })

    /** A window: who came and went, what they typed, then the clock. */
    function step(step: Step): void {
        players.step(step)
        commands.step(step.happened)
        time.step(step.now)
    }

    return { api: api, step: step }
}

export type GameT = ReturnType<typeof Game>
