import type { Ref } from "vue"
import type { BridgeHandle, CommandInfo } from "../../bridge/index.ts"
import { fuzzy, type PaletteItem } from "../../kit/index.ts"
import type { GameStoreHandle } from "../store.ts"
import { commandLine } from "./commands.ts"

/**
 * The palette's lists, by the prefix that opens each (VS Code's `>` and `@`): what a line typed
 * there could mean, and what taking each does. Plain text is chat.
 */
export type Context = {
    store: GameStoreHandle
    bridge: BridgeHandle
    /** Who's here, asked as the palette opened. */
    players: Ref<string[]>
    /** Command lines run lately, the latest first. */
    recent: Ref<string[]>
    /** The line becomes this (a list chosen, a word completed). */
    go(query: string): void
    /** A command line run: sent, remembered, the palette shut. */
    run(text: string): void
    close(): void
    /** The Developer log, in the palette. */
    showLog(): void
}
/** A row, and what taking it does ("tab" completes, never runs). */
export type Entry = { item: PaletteItem; take(how: "enter" | "tab" | "click"): void }
export type View = { entries: Entry[]; placeholder: string; note?: string }
type List = { prefix: string; name: string; about: string; view(ctx: Context, text: string): View }

export const LISTS: readonly List[] = [
    { prefix: ":", name: "Commands", about: "Run a command", view: commands },
    { prefix: "@", name: "People", about: "Someone here", view: people },
    { prefix: "#", name: "Places", about: "Another body, another world", view: places },
    { prefix: ">", name: "Actions", about: "Open a screen, leave, the log", view: actions },
]

/** The view for the whole line: a list's, by its prefix (`/` is `:`, as chat has always had it), else chat. */
export function viewOf(ctx: Context, query: string): View {
    const line = query.startsWith("/") ? ":" + query.slice(1) : query
    const list = LISTS.find((l) => line.startsWith(l.prefix))
    return list ? list.view(ctx, line.slice(list.prefix.length)) : chat(ctx, line)
}

// ranked by how well `text` matches each label, the unmatched gone
function ranked<T>(things: readonly T[], label: (t: T) => string, text: string) {
    return things.map((t) => ({ t: t, m: fuzzy(label(t), text) }))
        .filter((r): r is { t: T; m: NonNullable<typeof r.m> } => r.m !== null)
        .sort((a, b) => b.m.score - a.m.score)
}

function chat(ctx: Context, text: string): View {
    const view = { placeholder: "Say something, or : commands, @ people, # places, > actions" }
    if (text.trim() === "")
        return { ...view, entries: LISTS.map((l) => ({
            item: { id: `list:${l.prefix}`, label: `${l.prefix}  ${l.name}`, detail: l.about, group: "lists" },
            take: () => ctx.go(l.prefix),
        })) }
    // what's typed is said; it might also have been a command or an action, looked for
    const said: Entry = { item: { id: "say", label: text.trim(), detail: "Say it to everyone here", group: "chat" }, take: (how) => {
        if (how !== "tab") { void ctx.bridge.send("chat.send", { text: text.trim() }); ctx.close() }
    } }
    const also = [...commands(ctx, text).entries, ...actions(ctx, text).entries].filter((e) => e.item.marks?.length).slice(0, 6)
    return { ...view, entries: [said, ...also] }
}

function commands(ctx: Context, text: string): View {
    const all = ctx.store.ui.commands ?? []
    const view = { placeholder: "Run a command" }
    const read = commandLine.read(text, all)
    // a name still being typed lists every command it could be, the exact one first (:river, :rivers)
    if (!read || read.typing === null) {
        const of = (c: CommandInfo, group: string, marks: readonly number[] = []): Entry => ({
            item: { id: `cmd:${group}:${c.name}`, label: `:${c.name}`, marks: marks.map((i) => i + 1), detail: c.description, group: group },
            take: (how) => (how === "tab" || !commandLine.bare(c) ? ctx.go(`:${c.name} `) : ctx.run(c.name)),
        })
        const recent: Entry[] = text.trim() === "" ? ctx.recent.value.map((line) => ({
            item: { id: `recent:${line}`, label: `:${line}`, group: "recently used" },
            take: (how) => (how === "tab" ? ctx.go(`:${line}`) : ctx.run(line)),
        })) : []
        const found = ranked(all, (c) => c.name, text).map((r) => of(r.t, "commands", r.m.marks))
        return { ...view, entries: [...recent, ...found],
            note: all.length === 0 ? "This world gives you no commands." : `There's no :${text.split(" ")[0]}.` }
    }
    const { command, arg, typing } = read
    const run: Entry = {
        item: { id: "run", label: `:${text.trim()}`, detail: command.description, group: "run" },
        // named alone, one that needs more is given a space to type it in
        take: (how) => (typing === null && !commandLine.bare(command) ? ctx.go(`:${command.name} `) : how === "tab" ? undefined : ctx.run(text.trim())),
    }
    if (!arg || typing === null) return { ...view, entries: [run] }
    const word = typing.replace(/^@/, "")
    const fills = ranked(commandLine.choices(arg, ctx.players.value), (c) => c, word)
        .filter((r) => r.t !== word)
        .map((r): Entry => ({
            item: { id: `fill:${r.t}`, label: r.t, marks: r.m.marks, detail: arg.type === "player" ? "here" : undefined, group: `<${arg.name}>` },
            take: () => ctx.go(`:${commandLine.complete(text, r.t)}`),
        }))
    return { ...view, entries: [run, ...fills] }
}

function people(ctx: Context, text: string): View {
    const msg = (ctx.store.ui.commands ?? []).some((c) => c.name === "msg")
    const entries = ranked(ctx.players.value, (p) => p, text).map((r): Entry => ({
        item: { id: `player:${r.t}`, label: `@${r.t}`, marks: r.m.marks.map((i) => i + 1), detail: msg ? "Say something to them alone" : undefined, group: "here", dim: !msg },
        take: () => ctx.go(`:msg ${r.t} `),
    }))
    return { entries: entries, placeholder: "Someone here", note: ctx.players.value.length ? `Nobody here is called ${text}.` : "Nobody's here." }
}

function places(ctx: Context, text: string): View {
    const { store } = ctx
    const travel = (store.ui.commands ?? []).some((c) => c.name === "travel")
    const here = store.ui.body?.id
    const bodies = (store.ui.map?.bodies ?? []).filter((b) => b.id !== here)
    const onBodies = ranked(bodies, (b) => b.name, text).map((r): Entry => ({
        item: { id: `body:${r.t.id}`, label: `#${r.t.name}`, marks: r.m.marks.map((i) => i + 1),
            detail: travel ? "Travel there" : "Travel needs :travel (an admin's)", group: "this world", dim: !travel },
        take: (how) => (how === "tab" ? ctx.go(`:travel ${r.t.id} `) : ctx.run(`travel ${r.t.id}`)),
    }))
    const code = store.ui.map?.code
    const worlds = (store.worlds ?? []).filter((w) => w.code !== code)
    const toWorlds = ranked(worlds, (w) => w.name, text).map((r): Entry => ({
        item: { id: `world:${r.t.path}`, label: `#${r.t.name}`, marks: r.m.marks.map((i) => i + 1), detail: "Leave this world for it", group: "your worlds" },
        take: (how) => {
            if (how === "tab") return
            store.hop = r.t.path // the menu enters it as it opens (menu/MainMenu.vue)
            void ctx.bridge.send("game.leave", {})
        },
    }))
    return { entries: [...onBodies, ...toWorlds], placeholder: "Go somewhere",
        note: store.worlds === null ? "Your worlds are listed once the menu's shown them." : "Nowhere by that name." }
}

// what the game can be asked from anywhere: `shows` says where it's there
const ACTIONS: readonly { label: string; shows?: (store: GameStoreHandle) => boolean; take(ctx: Context): void }[] = [
    { label: "Inventory: Open", take: (ctx) => void ctx.bridge.send("screen.open", { name: "inventory" }) },
    { label: "Map: Open", shows: (store) => store.ui.map !== null, take: (ctx) => void ctx.bridge.send("screen.open", { name: "map" }) },
    { label: "Wiki: Open", shows: (store) => store.ui.wiki === true, take: (ctx) => void ctx.bridge.send("screen.open", { name: "wiki" }) },
    { label: "Library: Open", take: (ctx) => void ctx.bridge.send("screen.open", { name: "library" }) },
    { label: "Game: Pause", take: (ctx) => void ctx.bridge.send("screen.open", { name: "pause" }) },
    { label: "Developer: Show Log", take: (ctx) => ctx.showLog() },
    { label: "Game: Leave World", take: (ctx) => void ctx.bridge.send("game.leave", {}) },
    { label: "Game: Quit", take: (ctx) => void ctx.bridge.send("game.quit", {}) },
]

function actions(ctx: Context, text: string): View {
    const shown = ACTIONS.filter((a) => !a.shows || a.shows(ctx.store))
    const entries = ranked(shown, (a) => a.label, text).map((r): Entry => ({
        item: { id: `action:${r.t.label}`, label: r.t.label, marks: r.m.marks, group: "actions" },
        take: (how) => (how === "tab" ? ctx.go(`>${r.t.label}`) : r.t.take(ctx)),
    }))
    return { entries: entries, placeholder: "Do something", note: "No action by that name." }
}
