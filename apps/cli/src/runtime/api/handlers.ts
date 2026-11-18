import type { RunningT } from "../running"

export type Disposer = () => void

/**
 * Handlers — functions mods register for something, each run as the mod that
 * registered it. A throwing handler is reported and stays registered.
 */
export function Handlers(opts: { running: RunningT }) {
    /** A new set of handlers for one thing: join, leave… */
    function set<A extends unknown[]>() {
        const entries = new Set<{ mod: string; fn: (...args: A) => unknown }>()

        function add(fn: (...args: A) => unknown): Disposer {
            const entry = { mod: opts.running.current(), fn: fn }
            entries.add(entry)
            return () => { entries.delete(entry) }
        }

        function run(...args: A): void {
            // a handler that registers or disposes another changes the next run, not this one
            for (const entry of [...entries]) opts.running.run(entry.mod, entry.fn, ...args)
        }

        return { add: add, run: run }
    }

    return { set: set }
}

export type HandlersT = ReturnType<typeof Handlers>
