import { err } from "../errors"
import { Call, Event, Level, Op, PROTOCOL, type WireT } from "./wire"

type CallCode = (typeof Call)[keyof typeof Call]
export type LevelCode = (typeof Level)[keyof typeof Level]

/** What happened in the universe since the last window. */
export type Happening =
    | { readonly kind: "join" | "leave"; readonly player: string }
    /** Someone typed a command: its first word, and the rest of the line. player is "" from the console. */
    | { readonly kind: "command"; readonly player: string; readonly command: string; readonly text: string }
/** A window the engine opened: game time, and what happened. */
export type Step = { readonly now: number; readonly dt: number; readonly happened: readonly Happening[] }

/**
 * Channel — the sidecar's side of the protocol: windows, calls, logs.
 *
 * Mod code only runs inside a window (or while loading, before the first). Between
 * windows the sidecar is blocked reading the wire, so a call is always answered at once,
 * in order with every write before it.
 */
export function Channel(opts: { wire: WireT }) {
    const wire = opts.wire
    let open = false
    let nextId = 1

    /** The next window to run, or "stop". Only between windows. */
    function next(): Step | "stop" {
        const frame = wire.read()
        if (frame.op === Op.stop) return "stop"
        if (frame.op !== Op.step) throw err("PROTOCOL", { detail: `op ${frame.op} between windows` })
        return parse.step(frame.payload)
    }

    /** Run a window: fn, then what it started, until it waits on the universe again. */
    async function window(fn: () => void): Promise<void> {
        open = true
        try {
            fn()
            // two event loop turns: awaits on timers that came due, and on answers, run on
            await new Promise<void>(resolve => setImmediate(resolve))
            await new Promise<void>(resolve => setImmediate(resolve))
        } finally {
            open = false
        }
        wire.write(Op.idle, 0, null)
    }

    /** Ask the engine something, and wait for the answer. */
    function ask(call: CallCode, ...args: unknown[]): unknown {
        if (!open) throw err("NOT_RUNNING")
        const id = nextId++
        wire.write(Op.call, id, [call, ...args])
        const answer = wire.read()
        if (answer.op !== Op.reply || answer.id !== id)
            throw err("PROTOCOL", { detail: `call ${id} answered by op ${answer.op}, id ${answer.id}` })
        return parse.reply(id, answer.payload)
    }

    /** Tell the engine to do something. No answer, but in order with everything else. */
    function tell(call: CallCode, ...args: unknown[]): void {
        if (!open) throw err("NOT_RUNNING")
        wire.write(Op.call, 0, [call, ...args])
    }

    /** A line for the server's log, under a mod's name. Any time a mod runs, loading too. */
    function log(level: LevelCode, mod: string, text: string): void {
        wire.write(Op.log, 0, [level, mod, text])
    }

    /** Once, after every mod has loaded: what they declared. */
    function content(table: object): void {
        wire.write(Op.content, 0, { protocol: PROTOCOL, ...table })
    }

    return { next: next, window: window, ask: ask, tell: tell, log: log, content: content }
}

export type ChannelT = ReturnType<typeof Channel>

const parse = {
    step(payload: unknown): Step {
        if (typeof payload !== "object" || payload === null) throw err("PROTOCOL", { detail: "a step without a payload" })
        const now: unknown = Reflect.get(payload, "now")
        const dt: unknown = Reflect.get(payload, "dt")
        const events: unknown = Reflect.get(payload, "events")
        if (typeof now !== "number" || typeof dt !== "number" || !Array.isArray(events))
            throw err("PROTOCOL", { detail: "a step is { now, dt, events }" })
        return { now: now, dt: dt, happened: events.map(parse.event) }
    },

    event(event: unknown): Happening {
        if (!Array.isArray(event) || typeof event[1] !== "string") throw err("PROTOCOL", { detail: "an event is [kind, player, …]" })
        if (event[0] === Event.join) return { kind: "join", player: event[1] }
        if (event[0] === Event.leave) return { kind: "leave", player: event[1] }
        if (event[0] === Event.command) {
            if (typeof event[2] !== "string" || typeof event[3] !== "string")
                throw err("PROTOCOL", { detail: "a command event is [kind, player, command, text]" })
            return { kind: "command", player: event[1], command: event[2], text: event[3] }
        }
        throw err("PROTOCOL", { detail: `an event of kind ${event[0]}` })
    },

    reply(id: number, payload: unknown): unknown {
        if (!Array.isArray(payload) || payload.length !== 2 || typeof payload[0] !== "boolean")
            throw err("PROTOCOL", { detail: `the reply to call ${id} isn't [ok, value]` })
        if (!payload[0]) throw err("PROTOCOL", { detail: `the engine refused call ${id}: ${String(payload[1])}` })
        return payload[1]
    },
}
