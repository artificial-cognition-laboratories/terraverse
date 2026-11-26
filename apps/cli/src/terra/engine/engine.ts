import { existsSync } from "node:fs"
import { dirname } from "node:path"
import { err } from "../../errors"
import type { PreparedServer } from "../project"
import type { SavedWorld } from "../worlds"
import type { RunningT } from "../running"
import { lines } from "./lines"

/** How it runs: a dedicated server with no window, or the game, straight into the universe. */
export type EngineMode = "server" | "game"

export type EngineRun = {
    mode: EngineMode
    /** Where a dedicated server listens. The game plays alone. */
    port: number
    /** The game as another player of this machine: its own sign-in and Chromium cache. */
    profile?: string
    /** Each line it prints, instead of the terminal. */
    output?: (line: string) => void
} & EngineUniverse

/** What it opens: a prepared server's universe, or a clean one from a world code. */
export type EngineUniverse =
    | {
        /** Only a prepared server: the engine reads its .terra/, and makes its universe from its system. */
        server: PreparedServer
        /** Where the universe is saved. */
        world: string
    }
    | {
        /** A world code alone: its universe, with no mods, kept with the game's others. */
        code: string
    }
    | {
        /** A world the player has (the game's Play list): it keeps its own code. */
        saved: SavedWorld
        /** Its own server folder, prepared; null runs the game's, as the game does. */
        server: PreparedServer | null
    }

/** One running engine. */
export type EngineProcess = {
    /** Its exit code, once it's stopped. */
    readonly exited: Promise<number>
    /** Ask it to stop: it saves, stops its sidecar and exits. */
    stop(): void
    /** A server's run secret, which admits whoever holds it from this machine (its bodies); null for the game. */
    readonly secret: string | null
}

/** A body joining a server on this machine (sprint 126): the game with no window. */
export type EngineBody = {
    port: number
    /** Its player's name. */
    name: string
    /** The server's run secret, by the environment: what admits it. */
    secret: string
    /** Its eye's picture, px; none for a blind body. */
    vision: { width: number; height: number } | null
}

/** A body's process: its wire is its stdin and stdout, a JSON line each way; stderr is its log. */
export type BodyProcess = Bun.Subprocess<"pipe", "pipe", "pipe">

/** The signals that ask terra to stop, other than Ctrl-C: passed on to the engine. */
export const STOPS = ["SIGTERM", "SIGHUP"] as const

/** What each mode adds to the engine's command line. */
const MODES: Record<EngineMode, (port: number) => string[]> = {
    server: port => ["--server", "--port", String(port)],
    // played alone, as its admin: the game's singleplayer has every permission
    game: () => ["--go"],
}

/**
 * Engine — the game's engine: a dedicated server, or the game itself. `bin` is the folder it's
 * in: beside terra when installed, or a build tree's for terra run from source (terral).
 */
export function Engine(opts: {
    bin: string
    /** Where a server it starts is recorded, for the game to list and join. */
    running: RunningT
}) {
    /**
     * The engine in its folder. An installed version starts it through its `terraverse`
     * wrapper, one folder up, which finds its bundled libraries first; the build tree has
     * none, and runs it as it is.
     */
    function binary(): string {
        const bin = opts.bin
        const wrapper = `${dirname(bin)}/terraverse`
        if (process.platform !== "win32" && existsSync(wrapper)) return wrapper
        const path = `${bin}/${process.platform === "win32" ? "terraverse.exe" : "terraverse"}`
        if (!existsSync(path)) throw err("ENGINE_MISSING", { detail: `nothing at ${path}` })
        return path
    }

    function start(e: EngineRun): EngineProcess {
        const args = [
            ...MODES[e.mode](e.port),
            ...(e.profile ? ["--profile", e.profile] : []),
            "--gameid", "terraverse",
            ...universe(e),
        ]
        // a server's run: its secret admits players from this machine (online/local.h); through
        // the environment, never the command line, which anyone here can read
        const run = e.mode === "server" && !("code" in e) ? opts.running.prepare() : null
        const env = run ? { ...process.env, TERRA_LOCAL_SECRET: run.secret, ...(run.owner ? { TERRA_OWNER: run.owner } : {}) } : process.env
        // SIGTERM, not SIGINT: the game's Chromium takes Ctrl-C for itself and never passes it on
        const output = e.output
        const proc = output
            ? Bun.spawn([binary(), ...args], { env: env, stdin: "ignore", stdout: "pipe", stderr: "pipe" })
            : Bun.spawn([binary(), ...args], { env: env, stdio: ["inherit", "inherit", "inherit"] })
        const read = output && proc.stdout instanceof ReadableStream && proc.stderr instanceof ReadableStream
            ? Promise.all([lines(proc.stdout, output), lines(proc.stderr, output)])
            : Promise.resolve()
        const exited = read.then(() => proc.exited)
        if (run && !("code" in e)) {
            const done = opts.running.started({
                ...recorded(e),
                port: e.port,
            }, { pid: proc.pid, secret: run.secret, owner: run.owner })
            void exited.finally(done)
        }
        return { exited: exited, stop: () => proc.kill("SIGTERM"), secret: run ? run.secret : null }
    }

    /** A body, joining the server at `port` on this machine (client/app/headless). */
    function body(b: EngineBody): BodyProcess {
        const eye = b.vision ? ["--vision", `${b.vision.width}x${b.vision.height}`] : []
        return Bun.spawn([binary(), "--body", "--address", "127.0.0.1", "--port", String(b.port), "--name", b.name, ...eye], {
            env: { ...process.env, TERRA_LOCAL_SECRET: b.secret },
            stdin: "pipe",
            stdout: "pipe",
            stderr: "pipe",
        })
    }

    /** Run it until it stops, in this terminal. Its exit code. */
    async function run(e: EngineRun): Promise<number> {
        const engine = start(e)
        // Ctrl-C reaches the engine too, in our process group: it saves and exits, and we
        // wait for that rather than leaving it behind. A stop sent to terra alone (Fleet, a
        // service manager) is passed on, so the engine is never orphaned.
        const ignore = () => {}
        const pass = () => engine.stop()
        process.on("SIGINT", ignore)
        for (const signal of STOPS) process.on(signal, pass)
        try {
            return await engine.exited
        } finally {
            process.off("SIGINT", ignore)
            for (const signal of STOPS) process.off(signal, pass)
        }
    }

    return {
        start: start,
        run: run,
        body: body,
        /** The folder the engine's data is in, one up from bin: world/, textures/, matter/. */
        share: dirname(opts.bin),
    }
}

export type EngineT = ReturnType<typeof Engine>

/** What each universe adds to the engine's command line. */
function universe(e: EngineUniverse): string[] {
    if ("code" in e) return ["--world-code", e.code]
    if ("saved" in e) return ["--world", e.saved.path, ...(e.server ? ["--server-folder", e.server.root] : [])]
    return ["--world", e.world, "--world-code", e.server.config.system, "--server-folder", e.server.root]
}

/** A server's running/ record: what the game lists it as. */
function recorded(e: Exclude<EngineUniverse, { code: string }>): { title: string; mods: string[]; server: string } {
    if ("saved" in e) return { title: e.saved.title, mods: e.server ? e.server.mods.map(m => m.name) : [], server: e.saved.path }
    return { title: e.server.title, mods: e.server.mods.map(m => m.name), server: e.server.root }
}
