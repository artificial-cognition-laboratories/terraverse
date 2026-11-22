import { existsSync } from "node:fs"
import { dirname } from "node:path"
import { err } from "../../errors"
import type { PreparedServer } from "../project"

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

/** One running engine. */
export type EngineProcess = {
    /** Its exit code, once it's stopped. */
    readonly exited: Promise<number>
    /** Ask it to stop: it saves, stops its sidecar and exits. */
    stop(): void
}

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
export function Engine(opts: { bin: string }) {
    /**
     * The engine in its folder. An installed version starts it through its `terraverse`
     * wrapper, one folder up, which finds its bundled libraries first; the build tree has
     * none, and runs it as it is.
     */
    function binary(): string {
        const bin = opts.bin
        const wrapper = `${dirname(bin)}/terraverse`
        if (process.platform !== "win32" && existsSync(wrapper)) return wrapper
        const path = `${bin}/${process.platform === "win32" ? "luanti.exe" : "luanti"}`
        if (!existsSync(path)) throw err("ENGINE_MISSING", { detail: `nothing at ${path}` })
        return path
    }

    function start(e: EngineRun): EngineProcess {
        const args = [
            ...MODES[e.mode](e.port),
            ...(e.profile ? ["--profile", e.profile] : []),
            "--gameid", "terrafirma",
            ...("server" in e
                ? ["--world", e.world, "--world-code", e.server.config.system, "--server-folder", e.server.root]
                : ["--world-code", e.code]),
        ]
        // SIGTERM, not SIGINT: the game's Chromium takes Ctrl-C for itself and never passes it on
        const output = e.output
        if (!output) {
            const proc = Bun.spawn([binary(), ...args], { stdio: ["inherit", "inherit", "inherit"] })
            return { exited: proc.exited, stop: () => proc.kill("SIGTERM") }
        }
        const proc = Bun.spawn([binary(), ...args], { stdin: "ignore", stdout: "pipe", stderr: "pipe" })
        const read = Promise.all([lines(proc.stdout, output), lines(proc.stderr, output)])
        return { exited: read.then(() => proc.exited), stop: () => proc.kill("SIGTERM") }
    }

    /** Run it until it stops, in this terminal. Its exit code. */
    async function run(e: EngineRun): Promise<number> {
        const engine = start(e)
        // Ctrl-C reaches the engine too, in our process group: it saves and exits, and we
        // wait for that rather than leaving it behind
        const ignore = () => {}
        process.on("SIGINT", ignore)
        try {
            return await engine.exited
        } finally {
            process.off("SIGINT", ignore)
        }
    }

    return {
        start: start,
        run: run,
        /** The folder the engine's data is in, one up from bin: world/, textures/, matter/. */
        share: dirname(opts.bin),
    }
}

export type EngineT = ReturnType<typeof Engine>

/** A stream, a line at a time. */
async function lines(stream: ReadableStream<Uint8Array>, each: (line: string) => void): Promise<void> {
    const decoder = new TextDecoder()
    let rest = ""
    for await (const chunk of stream) {
        rest += decoder.decode(chunk, { stream: true })
        const parts = rest.split("\n")
        rest = parts.pop() ?? ""
        for (const line of parts) each(line)
    }
    if (rest) each(rest)
}
