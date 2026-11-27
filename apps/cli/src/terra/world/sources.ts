import type { EngineT, EngineUniverse } from "../engine"
import type { RunningRecord } from "../running"

/** What the engine prints once it takes players: its server is up. */
const LISTENING = "listening on"
/** How often an attached world's process is looked for. */
const WATCH_MS = 1000

/** A world taking players, on this machine: where, the secret that lets a body in, and how it goes. */
export type Open = {
    port: number
    secret: string
    /** Settles when the contract is over: the world stopped, or was let go. Its exit code. */
    ended: Promise<number>
    /** Closing the contract: a served world stops; an attached one is let go, and stays up. */
    close(): void
}

/** Where a contract's world comes from. `open` resolves once it takes players, or to an exit code when it never did. */
export type WorldSource = { title: string; open(): Promise<Open | number> }

export const sources = {
    /** A world this run starts: the engine's server, gone when the contract closes. */
    served(opts: { engine: EngineT; universe: Exclude<EngineUniverse, { code: string }>; title: string; port: number; log: (line: string) => void }): WorldSource {
        return {
            title: opts.title,
            async open(): Promise<Open | number> {
                let listening = () => {}
                const up = new Promise<void>(resolve => { listening = resolve })
                const engine = opts.engine.start({
                    mode: "server",
                    ...opts.universe,
                    port: opts.port,
                    output: line => {
                        opts.log(line)
                        if (line.includes(LISTENING)) listening()
                    },
                })
                // it died before taking players: its exit code, and why is on stderr
                const first = await Promise.race([up.then(() => null), engine.exited])
                if (first !== null) return first
                if (engine.secret === null) throw new Error("a server ran with no run secret: its bodies have nothing to be let in by")
                return { port: opts.port, secret: engine.secret, ended: engine.exited, close: engine.stop }
            },
        }
    },

    /**
     * A world already running here, by its running/ record: one the game opened, or another
     * terra's. Never stopped from here: closing lets it go, and the contract ends when the world
     * does — its record gone (the game left it, and lives on) or replaced.
     */
    attached(opts: { record: RunningRecord; still: () => boolean }): WorldSource {
        const record = opts.record
        return {
            title: record.title,
            async open(): Promise<Open> {
                let end: (code: number) => void = () => {}
                const ended = new Promise<number>(resolve => { end = resolve })
                const watching = setInterval(() => {
                    if (!opts.still()) end(0)
                }, WATCH_MS)
                void ended.finally(() => clearInterval(watching))
                return { port: record.port, secret: record.secret, ended: ended, close: () => end(0) }
            },
        }
    },
}
