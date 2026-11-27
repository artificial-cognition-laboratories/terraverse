import { Bodies, Bridge, PlaceRefused, type BodiesT, type BodyT } from "../body"
import type { EngineT } from "../engine"
import { refs } from "../ref"
import { requests, type Request } from "./requests"
import { lines } from "../engine"
import type { WorldSource } from "./sources"

export const CONTRACT = "axon.world/0"
/** A dedicated server's step, s: the engine's `dedicated_server_step`. */
const TIMESTEP = 0.09

export type WorldContractOpts = {
    /** Bodies are its clients. */
    engine: EngineT
    /** The world: one this run serves, or one already running here that it attaches to (./sources). */
    world: WorldSource
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
 * WorldContract — a world spoken for to axond over the world contract (axon.world/0) on terra's
 * own pipes: one it serves, or one it attaches to. It says `ready` once the world takes players,
 * answers each request by its id, and closes the world (a served one stops, an attached one is
 * let go) when asked or when its runner goes (stdin ends). `place` puts a body in it (../body): a
 * player with no window, worn at the bridge by one mind.
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
                    result: { world: opts.world.title, time: seconds(), pace: { kind: "real" }, bodies: world.bodies.list().map(b => described(b, world.bridge)) },
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

    /** Speak for the world until it ends or is closed: its exit code. */
    async function run(): Promise<number> {
        let bridge: ReturnType<typeof Bridge> | null = null
        const world = await opts.world.open()
        // it never took players: its exit code, and why is on stderr
        if (typeof world === "number") return world
        try {
            const bodies = Bodies({ engine: opts.engine, port: world.port, secret: world.secret, log: opts.log })
            bridge = Bridge({ bodies: bodies })
            const where = bridge.open()
            say({
                type: "ready",
                contract: CONTRACT,
                version: opts.version,
                engine: { name: "terraverse", version: opts.version },
                world: opts.world.title,
                bridge: where,
                view: refs.print({ kind: "address", host: "localhost", port: world.port }),
                timestep: TIMESTEP,
                pace: { kind: "real" },
            })
            const ticking = setInterval(() => say({ type: "ticked", time: seconds(), factor: 1 }), 1000)
            // closing: no more ticks while the world goes, or is let go
            const stop = () => {
                clearInterval(ticking)
                world.close()
            }
            // the runner gone, stdin ends: the world is closed with it
            void lines(opts.asked, line => {
                const read = requests.read(line)
                if ("code" in read) return failed(read.id, read.code, read.message)
                answer(read, { bodies: bodies, bridge: where, stop: stop })
            }, world.ended).finally(() => bodies.clear().finally(stop))
            try {
                return await world.ended
            } finally {
                clearInterval(ticking)
                // an attached world outlives us: its bodies don't
                await bodies.clear()
            }
        } finally {
            bridge?.close()
        }
    }

    return { run: run }
}

export type WorldContractT = ReturnType<typeof WorldContract>
