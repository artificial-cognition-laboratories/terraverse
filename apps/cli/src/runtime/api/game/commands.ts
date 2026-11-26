import { err } from "../../../errors"
import type { ChannelT, Happening } from "../../channel"
import type { RunningT } from "../../running"
import { Call, Level } from "../../wire"
import { checks } from "../checks"
import type { Disposer } from "../handlers"
import { Refused } from "../refused"
import type { Player } from "./players"

/** A value offered while it's typed, and what it means. Offered, not required. */
type Suggestion = string | { readonly value: string; readonly description?: string }
/** What every argument may say of itself, for the palette. */
type ArgShown = { readonly optional?: boolean; readonly description?: string; readonly suggest?: readonly Suggestion[] }

type ArgDef =
    | ({ readonly type: "string"; readonly options?: readonly string[] } & ArgShown)
    | ({ readonly type: "number" | "integer"; readonly min?: number; readonly max?: number } & ArgShown)
    | ({ readonly type: "boolean" } & ArgShown)
    | ({ readonly type: "player" | "position" } & ArgShown)

type Run = (e: { player?: Player; args: Record<string, unknown>; reply(text: string): void }) => unknown

/** What create is given, as typed. Mods are someone else's code: parse checks it all again. */
type CommandIn = {
    readonly description: string
    readonly args?: Readonly<Record<string, ArgDef>>
    readonly permission?: string
    readonly run: Run
}

type Entry = {
    readonly path: readonly string[]
    readonly description: string
    readonly args: readonly (readonly [string, ArgDef])[]
    readonly permission: string | undefined
    readonly run: Run
    readonly mod: string
}

const WORD = /^[a-z0-9_-]+$/
const TYPES = ["string", "number", "integer", "boolean", "player", "position"]
const BOOLEANS: Record<string, boolean> = { true: true, yes: true, on: true, false: false, no: false, off: false }

/**
 * Commands — what players type: `game.commands`.
 *
 * The engine knows only each command's first word, and hands every line typed under one to
 * the next window. Paths, arguments and permissions are decided here, so a command typed
 * wrong never reaches the mod: whoever typed it is told what was expected.
 */
export function Commands(opts: { channel: ChannelT; running: RunningT; players: { get(name: string): Player | undefined } }) {
    const entries = new Map<string, Entry>()
    // the engine's set is replaced whole: sent in the first window (a restarted sidecar's
    // commands replace its predecessor's), then whenever it changes
    let dirty = true

    const api = Object.freeze({
        // typed for the runtime's own callers; mods' calls are checked whatever they pass
        create(path: string | readonly string[], def: CommandIn): Disposer {
            const entry = parse.entry(path, def, opts.running.current())
            const key = entry.path.join(" ")
            if (entries.has(key)) throw err("COMMAND_TAKEN", { detail: `/${key} is ${entries.get(key)!.mod}'s` })
            entries.set(key, entry)
            dirty = true
            return () => {
                if (entries.get(key) !== entry) return
                entries.delete(key)
                dirty = true
            }
        },
    })

    /** A line someone typed: found, checked, parsed and run as the mod whose command it is. */
    function typed(happening: Extract<Happening, { kind: "command" }>): void {
        const player = happening.player === "" ? undefined : opts.players.get(happening.player)
        if (happening.player !== "" && !player) return // gone before the window opened
        const words = happening.text.trim().split(/\s+/).filter(w => w !== "")
        const under = [...entries.values()].filter(e => e.path[0] === happening.command)
        // the longest path the line starts with
        let entry: Entry | undefined
        for (const e of under) {
            const rest = e.path.slice(1)
            if (rest.every((w, i) => words[i] === w) && (!entry || e.path.length > entry.path.length)) entry = e
        }
        const mod = entry?.mod ?? under[0]?.mod ?? ""
        const reply = (text: string) => {
            if (player?.connected) player.tell(text)
            else if (!player) opts.channel.log(Level.info, mod, text)
        }
        if (!entry) {
            reply(under.length === 0 ? `There's no /${happening.command}.` :
                [`/${happening.command}:`, ...under.map(e => `  /${e.path.join(" ")} ${usage(e)} — ${e.description}`)].join("\n"))
            return
        }
        const chosen = entry
        // the console runs anything: it's whoever runs the server
        if (chosen.permission && player && !player.can(chosen.permission)) {
            reply(`You may not: /${chosen.path.join(" ")} needs ${chosen.permission}.`)
            return
        }
        let args: Record<string, unknown>
        try {
            args = parse.args(chosen, words.slice(chosen.path.length - 1), opts.players)
        } catch (e) {
            if (!(e instanceof Refused)) throw e
            reply(`${e.message} Usage: /${chosen.path.join(" ")} ${usage(chosen)}`.trimEnd())
            return
        }
        opts.running.run(chosen.mod, async () => {
            try {
                await chosen.run({ player: player, args: args, reply: reply })
            } catch (e) {
                if (e instanceof Refused) {
                    reply(e.message)
                    return
                }
                reply(`/${chosen.path.join(" ")} failed: what went wrong is in the server's log.`)
                throw e
            }
        })
    }

    /** A window: the lines typed since the last, then the engine told of any new commands. */
    function step(happened: readonly Happening[]): void {
        for (const h of happened) if (h.kind === "command") typed(h)
        if (!dirty) return
        dirty = false
        // each by its whole path, with what it takes and who may: the engine routes by the
        // first word, lists them for /help, and gives each player's palette the ones they may run
        opts.channel.tell(Call["commands.set"], [...entries.values()].map(e => [
            e.path.join(" "), e.description, usage(e), e.permission ?? "",
            e.args.map(([name, a]) => [name, a.type, a.optional ?? false, a.type === "string" ? [...(a.options ?? [])] : [],
                a.description ?? "", (a.suggest ?? []).map(s => typeof s === "string" ? [s, ""] : [s.value, s.description ?? ""])]),
        ]))
    }

    return { api: api, step: step }
}

export type CommandsT = ReturnType<typeof Commands>

/** `<heat> [<x>]`: what a command takes, as chat shows it. */
function usage(e: Entry): string {
    return e.args.map(([name, a]) => {
        const shown = a.type === "string" && a.options ? a.options.join("|") : `<${name}>`
        return a.optional ? `[${shown}]` : shown
    }).join(" ")
}

const parse = {
    entry(path: unknown, def: unknown, mod: string): Entry {
        const words = typeof path === "string" ? path.trim().split(/\s+/) : path
        if (!Array.isArray(words) || words.length === 0 || !words.every(w => typeof w === "string" && WORD.test(w)))
            throw err("ARGUMENT_INVALID", { detail: `a command's path is words of a-z, 0-9, - and _, not ${JSON.stringify(path)}` })
        if (typeof def !== "object" || def === null) throw err("ARGUMENT_INVALID", { detail: "a command is { description, args?, permission?, run }" })
        const description: unknown = Reflect.get(def, "description")
        if (typeof description !== "string" || description.trim() === "")
            throw err("ARGUMENT_INVALID", { detail: `/${words.join(" ")} needs a description: chat shows it` })
        const fn = checks.function(Reflect.get(def, "run"), `/${words.join(" ")}'s run`)
        const run: Run = e => Reflect.apply(fn, undefined, [e])
        const given: unknown = Reflect.get(def, "permission")
        const permission = given === undefined ? undefined : checks.permission(given)
        const args = parse.argDefs(Reflect.get(def, "args"), words.join(" "))
        return { path: words, description: description, args: args, permission: permission, run: run, mod: mod }
    },

    argDefs(args: unknown, path: string): readonly (readonly [string, ArgDef])[] {
        if (args === undefined) return []
        if (typeof args !== "object" || args === null) throw err("ARGUMENT_INVALID", { detail: `/${path}'s args is { name: { type, … } }` })
        const out: [string, ArgDef][] = []
        let optional = false
        for (const [name, a] of Object.entries(args)) {
            const type: unknown = typeof a === "object" && a !== null ? Reflect.get(a, "type") : undefined
            if (type === "world" || type === "block") checks.unwired(`a command argument of type ${type}`)
            if (typeof type !== "string" || !TYPES.includes(type))
                throw err("ARGUMENT_INVALID", { detail: `/${path}'s ${name} is { type: ${TYPES.join(" | ")}, … }` })
            const def: ArgDef = a
            parse.shown(def, `/${path}'s ${name}`)
            if (optional && !def.optional) throw err("ARGUMENT_INVALID", { detail: `/${path}: ${name} comes after an optional argument, so it must be optional too` })
            optional ||= def.optional === true
            out.push([name, def])
        }
        return out
    },

    /** An argument's description and suggestions: words for the palette, checked as everything a mod passes is. */
    shown(a: ArgShown, what: string): void {
        if (a.description !== undefined && (typeof a.description !== "string" || a.description.trim() === ""))
            throw err("ARGUMENT_INVALID", { detail: `${what}'s description is text` })
        if (a.suggest === undefined) return
        if (!Array.isArray(a.suggest)) throw err("ARGUMENT_INVALID", { detail: `${what}'s suggest is a list` })
        for (const s of a.suggest) {
            const value: unknown = typeof s === "string" ? s : typeof s === "object" && s !== null ? Reflect.get(s, "value") : undefined
            const description: unknown = typeof s === "object" && s !== null ? Reflect.get(s, "description") : undefined
            if (typeof value !== "string" || !/^\S+$/.test(value) || (description !== undefined && typeof description !== "string"))
                throw err("ARGUMENT_INVALID", { detail: `${what}'s suggestions are words, or { value, description }, not ${JSON.stringify(s)}` })
        }
    },

    /** The words after a command's path, as its args. Throws Refused for what's typed wrong. */
    args(e: Entry, words: readonly string[], players: { get(name: string): Player | undefined }): Record<string, unknown> {
        const out: Record<string, unknown> = {}
        let at = 0
        e.args.forEach(([name, a], i) => {
            if (at >= words.length) {
                if (!a.optional) throw new Refused(`Missing ${name}.`)
                return
            }
            if (a.type === "position") {
                const xyz = words.slice(at, at + 3).map(Number)
                if (xyz.length < 3 || !xyz.every(Number.isFinite)) throw new Refused(`${name} is a position: x y z.`)
                out[name] = [xyz[0], xyz[1], xyz[2]]
                at += 3
                return
            }
            // the last argument, a string, takes the rest of the line: a message, a reason
            const word = a.type === "string" && i === e.args.length - 1 ? words.slice(at).join(" ") : words[at]!
            at = a.type === "string" && i === e.args.length - 1 ? words.length : at + 1
            out[name] = parse.arg(name, a, word, players)
        })
        if (at < words.length) throw new Refused(`Too much: ${words.slice(at).join(" ")}.`)
        return out
    },

    arg(name: string, a: ArgDef, word: string, players: { get(name: string): Player | undefined }): unknown {
        switch (a.type) {
            case "string":
                if (a.options && !a.options.includes(word)) throw new Refused(`${name} is one of ${a.options.join(", ")}.`)
                return word
            case "number":
            case "integer": {
                const n = Number(word)
                if (!Number.isFinite(n) || (a.type === "integer" && !Number.isInteger(n)))
                    throw new Refused(`${name} is ${a.type === "integer" ? "a whole number" : "a number"}, not ${word}.`)
                if ((a.min !== undefined && n < a.min) || (a.max !== undefined && n > a.max))
                    throw new Refused(`${name} is from ${a.min ?? "-∞"} to ${a.max ?? "∞"}.`)
                return n
            }
            case "boolean": {
                const b = BOOLEANS[word.toLowerCase()]
                if (b === undefined) throw new Refused(`${name} is yes or no.`)
                return b
            }
            case "player": {
                const p = players.get(word) ?? players.get(`@${word}`)
                if (!p) throw new Refused(`${word} isn't here.`)
                return p
            }
            case "position":
                throw err("PROTOCOL", { detail: "a position is read whole" })
        }
    },
}
