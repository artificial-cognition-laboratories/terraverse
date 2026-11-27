import { randomBytes } from "node:crypto"
import { chmodSync, existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs"
import { join } from "node:path"
import { refs } from "../ref"

/**
 * A server running on this machine, as the game lists it (shell: servers.running) and joins
 * it. The secret admits whoever has it, so the record is the user's alone (0600, in a 0700
 * folder); a ref never carries it.
 */
export type RunningRecord = {
    ref: string
    title: string
    mods: string[]
    port: number
    /** The engine's: a record whose process is gone is stale. */
    pid: number
    secret: string
    /** Its admin: the account terra was signed in as, or null signed out. */
    owner: string | null
    /** The server folder it runs. */
    server: string
    /** Unix seconds. */
    started: number
    /** Who runs it: terra (serve, dev), or the game, a world opened from its menu (sprint 127). */
    by: "terra" | "game"
}

/** What a server is, before it's running: the engine supplies its pid. */
export type RunningServer = Omit<RunningRecord, "ref" | "pid" | "secret" | "owner" | "started" | "by">

/**
 * Running — the servers terra runs on this machine: one record each in the home's running/,
 * named by its port (one server to a port). Records of processes gone are cleared as found.
 */
export function Running(opts: {
    /** The home's running/. */
    dir: string
    /** The account terra is signed in as now. */
    owner: () => string | null
}) {
    /** A new run: its secret and whose it is, for the engine's environment, before it starts. */
    function prepare(): { secret: string; owner: string | null } {
        return { secret: randomBytes(32).toString("hex"), owner: opts.owner() }
    }

    /** The engine started: its record written, and a way to take it away when it stops. */
    function started(server: RunningServer, run: { pid: number; secret: string; owner: string | null }): () => void {
        mkdirSync(opts.dir, { recursive: true, mode: 0o700 })
        chmodSync(opts.dir, 0o700)
        const record: RunningRecord = {
            ...server,
            ref: refs.print({ kind: "address", host: "localhost", port: server.port }),
            pid: run.pid,
            secret: run.secret,
            owner: run.owner,
            started: Math.floor(Date.now() / 1000),
            by: "terra",
        }
        const file = fileOf(server.port)
        writeFileSync(file, JSON.stringify(record, null, 2) + "\n", { mode: 0o600 })
        return () => {
            // only ours: a later run on this port may have replaced it
            if (read(file)?.pid === run.pid) rmSync(file, { force: true })
        }
    }

    /** What's running now; stale records are removed. */
    function list(): RunningRecord[] {
        if (!existsSync(opts.dir)) return []
        const out: RunningRecord[] = []
        for (const name of readdirSync(opts.dir)) {
            if (!name.endsWith(".json")) continue
            const file = join(opts.dir, name)
            const record = read(file)
            if (record && alive(record.pid)) out.push(record)
            else rmSync(file, { force: true })
        }
        return out.sort((a, b) => a.port - b.port)
    }

    function fileOf(port: number): string {
        return join(opts.dir, `${port}.json`)
    }

    /** What runs at `port` on this machine, or null. */
    function at(port: number): RunningRecord | null {
        return list().find(record => record.port === port) ?? null
    }

    return { prepare: prepare, started: started, list: list, at: at, dir: opts.dir }
}

export type RunningT = ReturnType<typeof Running>

/**
 * A record, or null for one that isn't (half-written as it was read, an older shape): the
 * caller clears it. Checked whole here, the seam: past this a record is trusted.
 */
function read(file: string): RunningRecord | null {
    let value: unknown
    try {
        value = JSON.parse(readFileSync(file, "utf8"))
    } catch (e) {
        if (e instanceof SyntaxError) return null
        throw e
    }
    if (typeof value !== "object" || value === null) return null
    const field = (key: string): unknown => Reflect.get(value, key)
    const text = (key: string) => { const v = field(key); return typeof v === "string" ? v : null }
    const num = (key: string) => { const v = field(key); return typeof v === "number" ? v : null }
    const ref = text("ref"), title = text("title"), secret = text("secret"), server = text("server")
    const port = num("port"), pid = num("pid"), started = num("started")
    const owner = field("owner"), mods = field("mods")
    if (ref === null || title === null || secret === null || server === null || port === null || pid === null || started === null) return null
    if (owner !== null && typeof owner !== "string") return null
    if (!Array.isArray(mods) || !mods.every(m => typeof m === "string")) return null
    // older records say nothing: terra wrote them
    const by = field("by") ?? "terra"
    if (by !== "terra" && by !== "game") return null
    return { ref: ref, title: title, mods: mods, port: port, pid: pid, secret: secret, owner: owner, server: server, started: started, by: by }
}

function alive(pid: number): boolean {
    try {
        process.kill(pid, 0)
        return true
    } catch (e) {
        // EPERM: someone else's process has the pid, so it lives
        if (e instanceof Error && "code" in e && e.code === "EPERM") return true
        if (e instanceof Error && "code" in e && e.code === "ESRCH") return false
        throw e
    }
}
