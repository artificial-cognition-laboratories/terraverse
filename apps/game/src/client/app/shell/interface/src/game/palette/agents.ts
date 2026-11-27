import { onBeforeUnmount, ref, type Ref } from "vue"
import type { AgentRow, AgentsView, BridgeHandle } from "../../bridge/index.ts"
import { fuzzy } from "../../kit/index.ts"
import type { Entry, View } from "./lists.ts"

/** How often the list is read again while an agent is on its way. */
const POLL_MS = 1000

/**
 * AgentsFeed — the player's agents as the palette's `*` shows them, read from the engine
 * (agents.list: Axon's daemon) as the palette opens, and again each second while one is on its
 * way, until it's here or failed. A component's: polling stops as it goes.
 */
export function AgentsFeed(opts: { bridge: BridgeHandle }) {
    const view: Ref<AgentsView | null> = ref(null)
    const problem = ref<string | null>(null)
    let timer: ReturnType<typeof setTimeout> | null = null
    let gone = false

    async function read(): Promise<void> {
        if (timer) clearTimeout(timer)
        timer = null
        try {
            view.value = await opts.bridge.send("agents.list", {})
            problem.value = null
        } catch (error) {
            problem.value = error instanceof Error ? error.message : String(error)
        }
        const waiting = view.value?.agents.some((a) => a.state === "starting") ?? false
        if (waiting && !gone) timer = setTimeout(() => void read(), POLL_MS)
    }

    /** Asked of the engine, then the list read again: what it said no to is the list's to show. */
    async function ask(work: Promise<unknown>): Promise<void> {
        try {
            await work
        } catch (error) {
            problem.value = error instanceof Error ? error.message : String(error)
            return
        }
        await read()
    }

    onBeforeUnmount(() => {
        gone = true
        if (timer) clearTimeout(timer)
    })
    void read()

    return {
        view: view,
        /** The engine's last no, said in the list. */
        problem: problem,
        embark: (agent: string) => ask(opts.bridge.send("agents.embark", { agent: agent })),
        stop: (session: string) => ask(opts.bridge.send("agents.stop", { session: session })),
    }
}

export type AgentsFeedT = ReturnType<typeof AgentsFeed>

/** What each state says, and whether Enter does anything with it. */
const STATES: Record<AgentRow["state"], { detail: (row: AgentRow) => string; group: string; dim: boolean }> = {
    here: { detail: () => "Here · Enter sends it away", group: "here", dim: false },
    starting: { detail: () => "On its way…", group: "here", dim: true },
    failed: { detail: (row) => `Couldn't come: ${row.reason ?? "no reason given"} · Enter tries again`, group: "yours", dim: false },
    idle: { detail: () => "Bring it here", group: "yours", dim: false },
    elsewhere: { detail: () => "Running elsewhere · Enter brings another here", group: "yours", dim: false },
}

/** The `*` list: the player's agents, each brought here or sent away; a folder or ref typed whole, brought too. */
export function agentsView(feed: AgentsFeedT, text: string): View {
    const placeholder = "Bring an agent here: one of yours, a folder, or @scope/name"
    const view = feed.view.value
    if (view === null) return { entries: [], placeholder: placeholder, note: feed.problem.value ?? "Asking Axon…" }
    if (view.daemon === "down")
        return { entries: [], placeholder: placeholder, note: `Axon isn't running here (axon daemon up starts it): ${view.reason}` }
    if (!view.world) return { entries: [], placeholder: placeholder, note: "This world isn't open to agents: sign in, then open it again." }
    const want = text.trim()
    const rows = view.agents.map((row) => ({ row: row, m: fuzzy(row.name, want) }))
        .filter((r): r is { row: AgentRow; m: NonNullable<typeof r.m> } => r.m !== null)
        .sort((a, b) => b.m.score - a.m.score)
    const entries = rows.map(({ row, m }): Entry => {
        const state = STATES[row.state]
        return {
            item: { id: `agent:${row.agent}`, label: `*${row.name}`, marks: m.marks.map((i) => i + 1), detail: state.detail(row), group: state.group, dim: state.dim },
            take: (how) => {
                if (how === "tab" || row.state === "starting") return
                if (row.state === "here" && row.session) void feed.stop(row.session)
                else void feed.embark(row.agent)
            },
        }
    })
    // what the engine said no to, said where it's seen
    if (feed.problem.value)
        entries.unshift({ item: { id: "agent:problem", label: "Axon said no", detail: feed.problem.value, group: "problem", dim: true }, take: () => undefined })
    // a folder or a ref, typed whole: brought as it's named
    const named = want.startsWith("/") || want.startsWith("~/") || /^@[\w.-]+\/[\w.-]+$/.test(want)
    if (named && !view.agents.some((row) => row.agent === want))
        entries.unshift({
            item: { id: "agent:typed", label: `*${want}`, detail: "Bring it here", group: "named" },
            take: (how) => { if (how !== "tab") void feed.embark(want) },
        })
    return { entries: entries, placeholder: placeholder,
        note: (view.agents.length ? `None of your agents is called ${want}.` : "You have no agents yet: make one with axon init, or type its folder.") }
}
