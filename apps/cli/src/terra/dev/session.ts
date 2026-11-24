import { createWriteStream, mkdirSync, rmSync } from "node:fs"
import { errOf } from "../../errors"
import { Exit } from "../../runtime"
import { STOPS, type EngineProcess, type EngineT } from "../engine"
import type { PreparedServer, ServerProjectT } from "../project"
import type { DevEvent, DevOptions } from "./types"
import type { WatchT } from "./watch"

type SessionOpts = {
    server: ServerProjectT
    engine: EngineT
    watch: WatchT
    options: DevOptions
    report: (event: DevEvent) => void
}

/**
 * Session — one `terra dev`: prepare, run the engine, and on every save prepare again.
 *
 * The engine notices a new build itself and reloads the mods (sidecar/), so a save is only
 * a prepare here. A config change, or mods declaring different content (the engine stops
 * with Exit.restart), starts the engine again. A build that fails keeps the last one
 * running; an engine that stops on its own waits for the next save. Ctrl-C ends it.
 */
export function Session(opts: SessionOpts) {
    let first = true
    let stopping = false
    let running: EngineProcess | null = null

    async function prepare(): Promise<PreparedServer | null> {
        const started = performance.now()
        try {
            const prepared = await opts.server.prepare()
            opts.report({ kind: "prepared", prepared: prepared, ms: Math.round(performance.now() - started), first: first })
            first = false
            return prepared
        } catch (thrown) {
            opts.report({ kind: "failed", error: errOf(thrown) })
            return null
        }
    }

    function start(prepared: PreparedServer): EngineProcess {
        // the whole log is kept; what's shown is the reporter's choice
        const logs = opts.server.frame.path("logs")
        mkdirSync(logs, { recursive: true })
        const log = createWriteStream(`${logs}/engine.log`, { flags: "a" })
        // --fresh: a save made new for every run, beside the real one and never it
        const world = opts.options.fresh ? opts.server.frame.path("fresh") : opts.server.server.worlds.folder
        if (opts.options.fresh) rmSync(world, { recursive: true, force: true })
        const engine = opts.engine.start({
            mode: opts.options.mode,
            // a profile of its own: Chromium locks its cache to one game, and the player's own may be open
            profile: "terra-dev",
            server: prepared,
            world: world,
            port: opts.options.port,
            output: line => {
                log.write(line + "\n")
                opts.report({ kind: "engine", line: line })
            },
        })
        running = engine
        engine.exited.finally(() => {
            log.end()
            if (running === engine) running = null
        })
        return engine
    }

    /** Until Ctrl-C. */
    async function run(): Promise<number> {
        // the engine may share our Ctrl-C (one process group) or not: stop it either way
        const interrupt = () => {
            stopping = true
            running?.stop()
        }
        process.on("SIGINT", interrupt)
        for (const signal of STOPS) process.on(signal, interrupt)
        opts.watch.start()
        try {
            let prepared = await prepare()
            while (!stopping) {
                if (!prepared) {
                    opts.report({ kind: "waiting" })
                    await opts.watch.next()
                    prepared = await prepare()
                    continue
                }
                prepared = await serve(start(prepared), prepared)
            }
            return 0
        } finally {
            opts.watch.stop()
            process.off("SIGINT", interrupt)
            for (const signal of STOPS) process.off(signal, interrupt)
        }
    }

    /** One engine, until it has to start again (what with) or stops (null, after a save). */
    async function serve(engine: EngineProcess, prepared: PreparedServer): Promise<PreparedServer | null> {
        for (;;) {
            const next = await Promise.race([
                engine.exited.then(code => ({ kind: "exited" as const, code: code })),
                opts.watch.next().then(change => ({ kind: "changed" as const, change: change })),
            ])
            if (next.kind === "exited") {
                if (stopping) return null
                if (next.code === Exit.restart) {
                    opts.report({ kind: "restarting", why: "content" })
                    return prepared
                }
                // stopped cleanly, by whoever was playing (the game closed, /shutdown): done
                if (next.code === 0) {
                    opts.report({ kind: "closed" })
                    stopping = true
                    return null
                }
                opts.report({ kind: "stopped", code: next.code })
                await opts.watch.next()
                return prepare()
            }
            const rebuilt = await prepare()
            if (!rebuilt) continue      // the last build keeps running
            prepared = rebuilt
            if (next.change === "config") {
                opts.report({ kind: "restarting", why: "config" })
                engine.stop()
                await engine.exited
                return prepared
            }
            // code: the engine sees the new build and reloads its mods
        }
    }

    return { run: run }
}

export type SessionT = ReturnType<typeof Session>
