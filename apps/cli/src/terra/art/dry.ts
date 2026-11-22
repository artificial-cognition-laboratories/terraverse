import { err } from "../../errors"
import { Api } from "../../runtime/api"
import type { ContentTable } from "../../runtime/api/content"
import { Channel } from "../../runtime/channel"
import { Globals } from "../../runtime/globals"
import { Loader } from "../../runtime/loader"
import { Running } from "../../runtime/running"
import { MemoryWire, Op } from "../../runtime/wire"
import type { ManifestData } from "../project/types"

/** A mod's line of log while it loaded. */
export type DryLog = { readonly mod: string; readonly text: string }

/**
 * DryRun — a prepared server's mods loaded as the sidecar loads them, with no engine: the
 * real API over a wire that goes nowhere. What they declare is what the engine would be
 * sent, and art draws into their built folders as it would there.
 */
export function DryRun(_opts: Record<string, never>) {
    // each load imports its bundles afresh: a process that loads twice (terra art view) must
    // see the second build, not the module it cached the first time
    let loads = 0

    async function load(manifest: ManifestData): Promise<{ table: ContentTable; logs: DryLog[] }> {
        loads++
        const mods = manifest.mods.map(m => (m.server ? { ...m, server: `${m.server}?load=${loads}` } : m))
        const memory = MemoryWire({ answer: () => { throw err("NOT_RUNNING", { detail: "a mod asked the universe something while it loaded, and a dry run has none" }) } })
        const channel = Channel({ wire: memory.wire })
        const running = Running({ channel: channel })
        const api = Api({ channel: channel, running: running, mods: mods })
        const loader = Loader({ running: running })
        const globals = Globals({ api: api, channel: channel, running: running })
        const restore = globals.install()
        // what Channel.log wrote: [level, mod, text]
        const logs = (): DryLog[] => memory.written().flatMap(f => {
            if (f.op !== Op.log || !Array.isArray(f.payload)) return []
            const [, mod, text]: unknown[] = f.payload
            return typeof mod === "string" && typeof text === "string" ? [{ mod: mod, text: text }] : []
        })
        try {
            await loader.load(mods)
        } catch (error) {
            // what the mod threw went to its log: that's what the person needs to read
            const said = logs().map(l => `${l.mod}: ${ownLines(l.text)}`).join("\n")
            throw said ? err("MOD_LOAD_FAILED", { detail: said }) : error
        } finally {
            restore()
        }
        return { table: api.seal(), logs: logs() }
    }

    return { load: load }
}

export type DryRunT = ReturnType<typeof DryRun>

/** terra's own source, as stack frames name it (from source, or inside the compiled binary). */
const TERRA = new URL("../../", import.meta.url).pathname

/** A failure as the mod's author needs it: the message and the mod's own frames, not terra's. */
function ownLines(text: string): string {
    return text.split("\n").filter(l => !/^\s+at /.test(l) || !(l.includes(TERRA) || l.includes("$bunfs"))).join("\n")
}
