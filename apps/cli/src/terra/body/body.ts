import type { ServerWebSocket } from "bun"
import type { EngineT } from "../engine"
import { contract } from "./contract"
import { wire, type Hello } from "./wire"
import { lines } from "../engine"

/** What a wearer's socket carries: which body it wears. */
export type Wearing = { name: string }

export type BodyOpts = {
    engine: EngineT
    /** The server it joins, on this machine, and the run secret that admits it. */
    port: number
    secret: string
    /** Its player's name in the world. */
    name: string
    /** Only a mind presenting this may wear it (a lease's); null for whoever connects first. */
    token: string | null
    /** Its log and what went wrong, for a person: stderr. */
    log: (line: string) => void
    /** The body is gone (the server let it go, or its process died): why. */
    gone: (name: string, why: string) => void
}

/** A body's eye: what a mind is shown. */
const VISION = { width: 320, height: 240 }
/** How long a body has to join and be in the world, ms: a first join receives the server's content. */
const JOINING_MS = 120_000

/**
 * Body — one body: a process of the engine with no window (client/app/headless), and the one
 * mind wearing it, if any. What the body senses reaches its wearer as axon.body/1 readings;
 * what the wearer commands reaches the body. Nobody wearing it, it stands where it is, its
 * controls lapsed.
 */
export function Body(opts: BodyOpts) {
    // every body sees, for now: the lease chooses its senses later (sprint 126, phase 4)
    const proc = opts.engine.body({ port: opts.port, name: opts.name, secret: opts.secret, vision: VISION })
    const seqs = new Map<string, number>()
    let wearer: ServerWebSocket<Wearing> | null = null
    let ended: string | null = null
    let hello: Hello | null = null
    let said = (h: Hello) => { void h }
    let failed = (cause: Error) => { void cause }
    const ready = new Promise<Hello>((resolve, reject) => {
        said = resolve
        failed = reject
    })
    // a rejection nobody awaits yet (the bridge's) mustn't take the process down
    ready.catch(() => {})

    function end(why: string): void {
        if (ended) return
        ended = why
        failed(new Error(`${opts.name}: ${why}`))
        wearer?.close(1011, why.slice(0, 120))
        wearer = null
        opts.gone(opts.name, why)
    }

    function send(message: Record<string, unknown>): void {
        wearer?.send(JSON.stringify(message))
    }

    function heard(line: string): void {
        const said_ = wire.read(line)
        switch (said_.t) {
            case "hello":
                hello = said_
                said(said_)
                return
            case "read": {
                const seq = (seqs.get(said_.ch) ?? 0) + 1
                seqs.set(said_.ch, seq)
                send(contract.reading(said_, seq))
                return
            }
            case "applied":
                send(contract.applied(said_))
                return
            case "refused":
                // nothing waits on one command: said, not answered (the Minecraft body's way)
                opts.log(`${opts.name} refused ${said_.ch} #${said_.seq}: ${said_.why}`)
                return
            case "gone":
                end(said_.why)
                return
        }
    }

    void lines(proc.stdout, line => {
        try {
            heard(line)
        } catch (cause) {
            opts.log(cause instanceof Error ? cause.message : String(cause))
            proc.kill()
        }
    })
    void lines(proc.stderr, line => opts.log(`${opts.name}: ${line}`))
    void proc.exited.then(code => end(`its process exited ${code}`))
    const joining = setTimeout(() => { if (!hello) { end(`it wasn't in the world within ${JOINING_MS / 1000}s`); proc.kill() } }, JOINING_MS)
    void ready.finally(() => clearTimeout(joining)).catch(() => {})

    let leaving: Promise<void> | null = null

    function tell(message: Parameters<typeof wire.write>[0]): void {
        if (ended) return
        proc.stdin.write(wire.write(message))
        proc.stdin.flush()
    }

    return {
        name: opts.name,
        token: opts.token,
        /** In the world, what it is; or why it never got there. */
        ready: ready,
        get worn(): boolean { return wearer !== null },
        get ended(): string | null { return ended },

        /** A mind puts it on: told what the body is, then sensing. */
        async wear(socket: ServerWebSocket<Wearing>): Promise<void> {
            wearer = socket
            const h = await ready
            if (wearer !== socket) return
            send({ type: "manifest", manifest: contract.manifest(h) })
            send(contract.presence("embodied"))
        },

        /** Its wearer gone: the controls lapse as they declared. */
        unwear(socket: ServerWebSocket<Wearing>): void {
            if (wearer === socket) wearer = null
        },

        /** One message from the wearer. */
        heard(text: string): void {
            const read = contract.heard(text)
            if ("refused" in read) {
                opts.log(`${opts.name}'s mind sent what it can't take: ${read.refused}`)
                return
            }
            tell(read.body)
        },

        /** Out of the world: it leaves, and its process ends. Asked again, the same leaving. */
        leave(): Promise<void> {
            leaving ??= (async () => {
                tell({ t: "leave" })
                if (!ended) proc.stdin.end()
                const gone = await Promise.race([proc.exited.then(() => true), Bun.sleep(5_000).then(() => false)])
                if (!gone) proc.kill("SIGKILL")
                end("it left")
            })()
            return leaving
        },
    }
}

export type BodyT = ReturnType<typeof Body>
