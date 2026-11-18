import { AsyncLocalStorage } from "node:async_hooks"
import type { ManifestMod } from "../terra"
import type { ChannelT } from "./channel"
import { Level } from "./wire"

/**
 * Running — which mod's code is running: the one loading, or the one whose handler this
 * is, kept across awaits. So a log line, a failure or a registration names its mod, even
 * from the far side of a promise.
 */
export function Running(opts: { channel: ChannelT }) {
    const store = new AsyncLocalStorage<string>()
    let loading: ManifestMod | null = null

    /** Load a mod: what it does until fn settles is its. */
    async function load<T>(mod: ManifestMod, fn: () => Promise<T>): Promise<T> {
        loading = mod
        try {
            return await fn()
        } finally {
            loading = null
        }
    }

    /** The mod loading now, or null once they all have. */
    function loadingMod(): ManifestMod | null {
        return loading
    }

    /** The mod whose code is running, or "" for none. */
    function current(): string {
        return loading?.name ?? store.getStore() ?? ""
    }

    /** Run fn as `mod`. What it throws, now or after an await, is reported, never rethrown. */
    function run<A extends unknown[]>(mod: string, fn: (...args: A) => unknown, ...args: A): void {
        store.run(mod, () => {
            try {
                const result = fn(...args)
                if (result instanceof Promise) result.catch(error => report(mod, error))
            } catch (error) {
                report(mod, error)
            }
        })
    }

    /** A failure into the server's log, under the mod's name. */
    function report(mod: string, error: unknown): void {
        opts.channel.log(Level.error, mod, trace(error))
    }

    return { load: load, loadingMod: loadingMod, current: current, run: run, report: report }
}

export type RunningT = ReturnType<typeof Running>

/** The mod's frames, not the runtime's beneath them. */
function trace(error: unknown): string {
    if (!(error instanceof Error)) return String(error)
    const stack = error.stack ?? `${error.name}: ${error.message}`
    return stack.split("\n").filter(line => !/\$bunfs|node:async_hooks/.test(line)).join("\n")
}
