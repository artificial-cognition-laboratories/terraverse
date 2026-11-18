import { err } from "../errors"

export type Args = {
    readonly command: string | undefined
    readonly positional: readonly string[]
    /** `--port 30000` is "30000"; `--fresh`, on its own, is true. */
    readonly flags: Readonly<Record<string, string | true>>
}

/**
 * argv → a command, its positional arguments and its flags. A flag takes the value after
 * it, unless that's another flag or there's none: then it's a switch. `-h`, `--help`, `-v`
 * and `--version` are commands of their own.
 */
export function parseArgs(argv: readonly string[]): Args {
    const [command, ...rest] = argv
    const positional: string[] = []
    const flags: Record<string, string | true> = {}
    for (let i = 0; i < rest.length; i++) {
        const arg = rest[i]!
        if (!arg.startsWith("--")) {
            positional.push(arg)
            continue
        }
        const value = rest[i + 1]
        if (value === undefined || value.startsWith("--")) {
            flags[arg.slice(2)] = true
            continue
        }
        flags[arg.slice(2)] = value
        i++
    }
    return { command: command, positional: positional, flags: flags }
}

/** Reading flags as a command means them: a value, a switch, or a port. */
export const flags = {
    /** Refuse any flag the command doesn't take: a typo is never silently ignored. */
    only(args: Args, known: readonly string[]): void {
        const unknown = Object.keys(args.flags).filter(f => !known.includes(f))
        if (unknown.length > 0)
            throw err("USAGE", { detail: `terra ${args.command} doesn't take ${unknown.map(f => `--${f}`).join(", ")}${known.length ? `; it takes ${known.map(f => `--${f}`).join(", ")}` : ""}` })
    },

    /** `--name value`, or undefined. A switch where a value belongs is a usage error. */
    value(args: Args, name: string): string | undefined {
        const v = args.flags[name]
        if (v === true) throw err("USAGE", { detail: `--${name} needs a value` })
        return v
    },

    /** `--name` on its own. A value after it is a usage error: it was meant for something. */
    on(args: Args, name: string): boolean {
        const v = args.flags[name]
        if (typeof v === "string") throw err("USAGE", { detail: `--${name} takes no value (it was given ${v})` })
        return v === true
    },

    /** `--port n`, 1–65535, or the default. */
    port(args: Args, fallback: number): number {
        const port = Number(flags.value(args, "port") ?? fallback)
        if (!Number.isInteger(port) || port < 1 || port > 65535) throw err("USAGE", { detail: "--port is 1–65535" })
        return port
    },
}
