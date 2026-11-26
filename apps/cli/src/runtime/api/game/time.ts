import { err } from "../../../errors"
import type { ChannelT } from "../../channel"
import type { RunningT } from "../../running"
import { Call } from "../../wire"
import { checks } from "../checks"
import type { Disposer } from "../handlers"

// The most one skip moves: replaying the weather and rivers through it is the engine's work
const SKIP_MOST = 24 * 3600

type Timer = { due: number; readonly mod: string; readonly fn: () => unknown; readonly every: number | null; live: boolean }

/**
 * Time — the game's clock and the timers mods set on it: `game.time`.
 *
 * Game time, not the wall's: timers fire in the window whose step reaches them. A sidecar
 * may load long after the universe began (a restart), so timers set while loading count
 * from its first window, not from zero.
 */
export function Time(opts: { running: RunningT; channel: ChannelT }) {
    let now = 0
    let started = false
    let timers: Timer[] = []
    // skips told to the engine, each settled when the next window's clock shows it
    let skipped: { to: number; resolve: () => void }[] = []

    /** The universe forward: the engine moves its clock now, and the next window opens past it. */
    function skip(d: unknown): Promise<void> {
        const s = Math.round(checks.seconds(d))
        if (s < 1) throw err("ARGUMENT_INVALID", { detail: "a skip moves at least a second" })
        if (s > SKIP_MOST) throw err("ARGUMENT_INVALID", { detail: `a skip moves at most ${SKIP_MOST / 3600} hours of game time at once` })
        opts.channel.tell(Call["time.skip"], s)
        const to = now + s
        return new Promise<void>(resolve => { skipped.push({ to: to, resolve: resolve }) })
    }

    function schedule(d: unknown, fn: () => unknown, every: boolean): Disposer {
        checks.function(fn, "a timer's function")
        const s = checks.seconds(d)
        if (every && s <= 0) throw err("ARGUMENT_INVALID", { detail: "every needs an interval of more than nothing" })
        const timer: Timer = { due: (started ? now : 0) + s, mod: opts.running.current(), fn: fn, every: every ? s : null, live: true }
        timers.push(timer)
        return () => { timer.live = false }
    }

    const api = Object.freeze({
        get now() { return now },
        after: (d: unknown, fn: () => unknown) => schedule(d, fn, false),
        every: (d: unknown, fn: () => unknown) => schedule(d, fn, true),
        wait: (d: unknown) => new Promise<void>(resolve => { schedule(d, resolve, false) }),
        skip: skip,
        pause: () => checks.unwired("game.time.pause"),
        resume: () => checks.unwired("game.time.resume"),
        step: () => checks.unwired("game.time.step"),
        schedule: () => checks.unwired("game.time.schedule"),
        on: () => checks.unwired("game.time.on"),
    })

    /** Move the clock to `to`, firing what came due, soonest first. */
    function step(to: number): void {
        if (!started) {
            started = true
            for (const t of timers) t.due += to
        }
        now = to
        const settled = skipped.filter(k => k.to <= now)
        skipped = skipped.filter(k => k.to > now)
        for (const k of settled) k.resolve()
        // what comes due while these run waits for the next window
        const due = timers.filter(t => t.live && t.due <= now).sort((a, b) => a.due - b.due)
        for (const t of due) {
            if (t.every === null) t.live = false
            else t.due += Math.max(t.every, now - t.due)
            opts.running.run(t.mod, t.fn)
        }
        timers = timers.filter(t => t.live)
    }

    return { api: api, step: step }
}

export type TimeT = ReturnType<typeof Time>
