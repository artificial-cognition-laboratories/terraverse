import { MOD_OF } from "../terra"
import type { ApiT } from "./api"
import type { ChannelT, LevelCode } from "./channel"
import type { RunningT } from "./running"
import { Level } from "./wire"

/**
 * Globals — makes the process the mods' world: the API as globals, and everything a mod
 * prints or leaves unhandled into the server's log, under its name.
 */
export function Globals(opts: { api: ApiT; channel: ChannelT; running: RunningT }) {
    /** Install them. What it returns puts the process back as it was. */
    function install(): () => void {
        const saved = { ...console }
        const names = Object.keys(opts.api.globals)
        const print = (level: LevelCode) => (...args: unknown[]) =>
            opts.channel.log(level, opts.running.current(), args.map(a => typeof a === "string" ? a : Bun.inspect(a)).join(" "))
        const unhandled = (error: unknown) => opts.running.report(opts.running.current(), error)

        Object.assign(globalThis, opts.api.globals)
        // what each bundle binds its own `mod` from: not enumerable, not a mod's to call
        Object.defineProperty(globalThis, MOD_OF, { value: opts.api.modOf, configurable: true })
        console.log = print(Level.info)
        console.info = print(Level.info)
        console.debug = print(Level.info)
        console.warn = print(Level.warn)
        console.error = print(Level.error)
        process.on("unhandledRejection", unhandled)

        return () => {
            Object.assign(console, saved)
            process.off("unhandledRejection", unhandled)
            for (const name of names) Reflect.deleteProperty(globalThis, name)
            Reflect.deleteProperty(globalThis, MOD_OF)
        }
    }

    return { install: install }
}

export type GlobalsT = ReturnType<typeof Globals>
