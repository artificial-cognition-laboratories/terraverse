import type { CommandArg, CommandChoice, CommandInfo } from "../../bridge/index.ts"

/**
 * A `:` line read against the commands this player may run (store.ui.commands): which one it
 * is, and which of its arguments is being typed. Value functions only.
 */
export const commandLine = {
    /**
     * The command the line names (the longest whole path it starts with), what's typed after
     * it, and the argument the last word fills (null past the last, or for an engine command,
     * whose arguments are only its usage).
     */
    read(text: string, commands: readonly CommandInfo[]): Read | null {
        const named = [...commands].sort((a, b) => b.name.length - a.name.length)
            .find((c) => text === c.name || text.startsWith(c.name + " "))
        if (!named) return null
        const rest = text.slice(named.name.length + 1)
        const words = rest.split(" ")
        const index = Math.max(0, words.length - 1)
        const args = named.args ?? []
        // the last string takes the rest of the line (the runtime's parse), so it's never past
        const last = args[args.length - 1]
        const arg = args[index] ?? (last?.type === "string" && index >= args.length ? last : null)
        return { command: named, rest: rest, typing: text === named.name ? null : words[index]!, arg: arg, index: index }
    },

    /** What could fill the argument being typed: its options, the players here, yes or no, or what it suggests. */
    choices(arg: CommandArg, players: readonly string[]): CommandChoice[] {
        const meant = (value: string) => arg.suggest?.find((s) => s.value === value)?.description
        if (arg.options?.length) return arg.options.map((o) => ({ value: o, description: meant(o) }))
        if (arg.type === "player") return players.map((p) => ({ value: p, description: "here" }))
        if (arg.type === "boolean") return [{ value: "true" }, { value: "false" }]
        return arg.suggest ?? []
    },

    /** The command as the palette lists it: its name, then what it takes (`:time [<time>]`). */
    shown(command: CommandInfo): string {
        return `:${command.name}${command.usage ? ` ${command.usage}` : ""}`
    },

    /** The line with its last word replaced by `word`, ready for the next. */
    complete(text: string, word: string): string {
        const cut = text.lastIndexOf(" ")
        return `${text.slice(0, cut + 1)}${word} `
    },

    /** As the server reads it: `/`, and a player named as @wren is wren. */
    wire(text: string): string {
        return "/" + text.trim().split(/\s+/).map((word) => word.replace(/^@(?=\S)/, "")).join(" ")
    },

    /** Whether a command can run as it's named, nothing more typed. */
    bare(command: CommandInfo): boolean {
        return command.args ? command.args.every((a) => a.optional) : command.usage === "" || command.usage.startsWith("[")
    },
}

export type Read = {
    command: CommandInfo
    /** What's typed after the command's name. */
    rest: string
    /** The word being typed (null: the name alone, not a space after it yet). */
    typing: string | null
    /** The argument that word fills, where the command says. */
    arg: CommandArg | null
    index: number
}
