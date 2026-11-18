import { err } from "../../../errors"
import type { RunningT } from "../../running"
import { checks } from "../checks"
import type { Disposer } from "../handlers"

type Timer = { due: number; readonly mod: string; readonly fn: () => unknown; readonly every: number | null; live: boolean }

/**
 * Time — the game's clock and the timers mods set on it: `game.time`.
 *
 * Game time, not the wall's: timers fire in the window whose step reaches them. A sidecar
 * may load long after the universe began (a restart), so timers set while loading count
 * from its first window, not from zero.
 */
export function Time(opts: { running: RunningT }) {
    let now = 0
    let started = false
    let timers: Timer[] = []

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
        skip: () => checks.unwired("game.time.skip"),
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
