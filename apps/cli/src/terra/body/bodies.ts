import type { EngineT } from "../engine"
import { Body, type BodyT } from "./body"

export type BodiesOpts = {
    engine: EngineT
    /** The server's port, on this machine, and its run secret. */
    port: number
    secret: string
    log: (line: string) => void
}

/** A player's name, as the engine takes one. */
const NAME = /^[A-Za-z0-9_-]{1,20}$/

/** A body that can't be placed, and why: refused by the world contract's code. */
export class PlaceRefused extends Error {
    constructor(readonly code: "BODY_NOT_FOUND" | "REFUSED", message: string) {
        super(message)
    }
}

/**
 * Bodies — every body in one server, by name. Placing one starts it and answers once it's in
 * the world; removing one takes it out. Terraverse has one kind of body, a player.
 */
export function Bodies(opts: BodiesOpts) {
    const placed = new Map<string, BodyT>()

    return {
        get(name: string): BodyT | null {
            return placed.get(name) ?? null
        },

        list(): BodyT[] {
            return [...placed.values()]
        },

        /** A body in the world as `name`, worn only with `token` when one's given. */
        async place(input: { body: string; name: string; token: string | null }): Promise<BodyT> {
            if (input.body !== "player") throw new PlaceRefused("BODY_NOT_FOUND", `Terraverse has one body, a player, not ${input.body}`)
            if (!NAME.test(input.name)) throw new PlaceRefused("REFUSED", `${input.name} isn't a player's name: 1–20 letters, digits, - and _`)
            if (placed.has(input.name)) throw new PlaceRefused("REFUSED", `a body is already placed as ${input.name}`)
            const body = Body({
                engine: opts.engine,
                port: opts.port,
                secret: opts.secret,
                name: input.name,
                token: input.token,
                log: opts.log,
                gone: (name, why) => {
                    if (placed.get(name) === body) placed.delete(name)
                    opts.log(`${name} is gone: ${why}`)
                },
            })
            placed.set(input.name, body)
            await body.ready
            return body
        },

        async remove(name: string): Promise<void> {
            const body = placed.get(name)
            if (!body) throw new PlaceRefused("BODY_NOT_FOUND", `no body is placed as ${name}`)
            await body.leave()
        },

        /** Every body out: the world's closing. */
        async clear(): Promise<void> {
            await Promise.all([...placed.values()].map(body => body.leave()))
        },
    }
}

export type BodiesT = ReturnType<typeof Bodies>
