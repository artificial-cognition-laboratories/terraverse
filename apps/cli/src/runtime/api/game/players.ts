import { err } from "../../../errors"
import type { ChannelT, Step } from "../../channel"
import { Call } from "../../wire"
import { checks } from "../checks"
import type { Disposer, HandlersT } from "../handlers"

export type Player = {
    readonly name: string
    readonly connected: boolean
    tell(message: string): void
    can(permission: string): boolean
    kick(reason: string): void
}

/** Players — who's in the game: the `game.players` global, kept by each window's joins and leaves. */
export function Players(opts: { channel: ChannelT; handlers: HandlersT }) {
    const present = new Map<string, PlayerHandle>()
    const joins = opts.handlers.set<[Player]>()
    const leaves = opts.handlers.set<[Player]>()
    let seeded = false

    const api = Object.freeze({
        on(event: unknown, fn: (player: Player) => unknown): Disposer {
            checks.function(fn, "players.on's handler")
            if (event === "join") return joins.add(fn)
            if (event === "leave") return leaves.add(fn)
            throw err("ARGUMENT_INVALID", { detail: `players.on has no event ${JSON.stringify(event)}` })
        },
        list: (): readonly Player[] => [...present.values()].map(p => p.player),
        get: (name: string): Player | undefined => present.get(name)?.player,
    })

    function step(step: Step): void {
        // A sidecar started, or restarted, under a running universe: whoever's here is
        // here, without a join. Handlers greet arrivals, not a reload.
        if (!seeded) {
            seeded = true
            const names = opts.channel.ask(Call["players.list"])
            if (!Array.isArray(names)) throw err("PROTOCOL", { detail: "players.list didn't answer a list" })
            for (const name of names) present.set(String(name), PlayerOf({ name: String(name), channel: opts.channel }))
        }
        for (const happening of step.happened) {
            if (happening.kind === "join") {
                const handle = present.get(happening.player) ?? PlayerOf({ name: happening.player, channel: opts.channel })
                present.set(happening.player, handle)
                joins.run(handle.player)
            } else if (happening.kind === "leave") {
                const handle = present.get(happening.player)
                if (!handle) continue
                present.delete(happening.player)
                handle.left()
                leaves.run(handle.player)
            }
        }
    }

    return { api: api, step: step }
}

export type PlayersT = ReturnType<typeof Players>
type PlayerHandle = ReturnType<typeof PlayerOf>

/** One player, as mods hold them: still there to read after they've left, and saying so. */
function PlayerOf(opts: { name: string; channel: ChannelT }) {
    let connected = true

    function here(): void {
        if (!connected) throw err("PLAYER_GONE", { detail: `${opts.name} has left` })
    }

    const player: Player = Object.freeze({
        name: opts.name,
        get connected() { return connected },
        tell(message: string) {
            here()
            opts.channel.tell(Call["player.tell"], opts.name, String(message))
        },
        can(permission: string) {
            here()
            checks.permission(permission)
            const may = opts.channel.ask(Call["player.can"], opts.name, permission)
            if (typeof may !== "boolean") throw err("PROTOCOL", { detail: "player.can didn't answer yes or no" })
            return may
        },
        kick(reason: string) {
            here()
            if (typeof reason !== "string" || reason.trim() === "") throw err("ARGUMENT_INVALID", { detail: "a kick says why" })
            opts.channel.tell(Call["player.kick"], opts.name, reason)
        },
    })

    return { player: player, left: () => { connected = false } }
}
