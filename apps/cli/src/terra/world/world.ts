import { Bodies, Bridge, PlaceRefused, type BodiesT, type BodyT } from "../body"
import type { EngineT, EngineUniverse } from "../engine"
import { refs } from "../ref"
import { requests, type Request } from "./requests"
import { lines } from "../engine"

export const CONTRACT = "axon.world/0"
/** A dedicated server's step, s: the engine's `dedicated_server_step`. */
const TIMESTEP = 0.09
/** What the engine prints once it takes players: its server is up. */
const LISTENING = "listening on"

export type WorldContractOpts = {
    engine: EngineT
    /** What it serves: a server folder's universe, or a world the player has. */
    universe: Exclude<EngineUniverse, { code: string }>
    /** What the world is called, as the contract says it. */
    title: string
    port: number
    /** terra's, and so the engine's: they ship together. */
    version: string
    /** The requests, a JSON line each: stdin. */
    asked: ReadableStream<Uint8Array>
    /** One whole line of the contract: stdout, which carries nothing else. */
    say: (line: string) => void
    /** What a person reads: the engine's output, on stderr. */
    log: (line: string) => void
}

/**
 * WorldContract — a server run for axond, spoken to over the world contract (axon.world/0) on
 * terra's own pipes. It says `ready` once the engine takes players, answers each request by its
 * id, and stops the engine when asked to close or when its runner goes (stdin ends). `place`
 * puts a body in it (../body): a player with no window, worn at the bridge by one mind.
 */
export function WorldContract(opts: WorldContractOpts) {
    const started = performance.now()
    const seconds = () => (performance.now() - started) / 1000

    function say(message: Record<string, unknown>): void {
        opts.say(JSON.stringify(message))
    }

    function failed(id: number | null, code: string, message: string): void {
        say({ type: "failed", id: id, code: code, message: message })
    }

    /** A body as the contract's `place` and `state` say it (axon.world/0 BodyState). */
    function described(body: BodyT, bridge: string): Record<string, unknown> {
        // its joints: the neck's turn and the head's nod (client/app/headless, frames)
        return { name: body.name, body: "player", bridge: `${bridge}/body/${body.name}`, worn: body.worn, joints: 2 }
    }

    /** A request that takes a while, answered when it's done, or refused by its code. */
    function later(id: number, work: Promise<unknown>): void {
        work.then(
            result => say({ type: "reply", id: id, result: result }),
            cause => cause instanceof PlaceRefused
                ? failed(id, cause.code, cause.message)
                : failed(id, "REFUSED", cause instanceof Error ? cause.message : String(cause)),
        )
    }

    /** One request answered, in a world with these bodies at this bridge; `stop` is how close ends it. */
    function answer(request: Request, world: { bodies: BodiesT; bridge: string; stop: () => void }): void {
        switch (request.type) {
            case "state":
                return say({
                    type: "reply",
                    id: request.id,
                    result: { world: opts.title, time: seconds(), pace: { kind: "real" }, bodies: world.bodies.list().map(b => described(b, world.bridge)) },
                })
            case "close":
                return later(request.id, world.bodies.clear().then(() => {
                    world.stop()
                    return {}
                }))
            case "place":
                return later(request.id, world.bodies.place({ body: request.body, name: request.name, token: request.token })
                    .then(body => described(body, world.bridge)))
            case "remove":
                return later(request.id, world.bodies.remove(request.name).then(() => ({})))
            case "pace":
            case "step":
                return failed(request.id, "NOT_WIRED", "a Terraverse world keeps to the wall's time; scaled and stepped pace aren't wired yet")
            case "reset":
            case "snapshot":
            case "restore":
                return failed(request.id, "NOT_WIRED", `${request.type} isn't wired yet: a Terraverse world is its map on disk`)
        }
    }

    /** Serve until the engine stops: its exit code. */
    async function run(): Promise<number> {
        let bridge: ReturnType<typeof Bridge> | null = null
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
        try {
            // it died before taking players: its exit code, and why is on stderr
            const first = await Promise.race([up.then(() => null), engine.exited])
            if (first !== null) return first
            if (engine.secret === null) throw new Error("a server ran with no run secret: its bodies have nothing to be let in by")
            const bodies = Bodies({ engine: opts.engine, port: opts.port, secret: engine.secret, log: opts.log })
            bridge = Bridge({ bodies: bodies })
            const where = bridge.open()
            say({
                type: "ready",
                contract: CONTRACT,
                version: opts.version,
                engine: { name: "terraverse", version: opts.version },
                world: opts.title,
                bridge: where,
                view: refs.print({ kind: "address", host: "localhost", port: opts.port }),
                timestep: TIMESTEP,
                pace: { kind: "real" },
            })
            const ticking = setInterval(() => say({ type: "ticked", time: seconds(), factor: 1 }), 1000)
            // closing: no more ticks while the engine saves and goes
            const stop = () => {
                clearInterval(ticking)
                engine.stop()
            }
            // the runner gone, stdin ends: the world goes with it
            void lines(opts.asked, line => {
                const read = requests.read(line)
                if ("code" in read) return failed(read.id, read.code, read.message)
                answer(read, { bodies: bodies, bridge: where, stop: stop })
            }, engine.exited).finally(() => bodies.clear().finally(stop))
            try {
                return await engine.exited
            } finally {
                clearInterval(ticking)
            }
        } finally {
            bridge?.close()
        }
    }

    return { run: run }
}

export type WorldContractT = ReturnType<typeof WorldContract>
