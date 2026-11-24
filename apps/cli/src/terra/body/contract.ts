import type { Applied, Hello, Read, ToBody } from "./wire"

/**
 * The body contract (axon.body/1, arclabs' @arcforge/types `body`), as terra speaks it for a
 * body: what the body process says, turned into the contract's messages, and what a mind sends,
 * read into the body's wire. The body declares its own frames, senses and controls (one table,
 * in C++); this only wraps them in what every Terraverse body shares.
 */
export const CONTRACT = "axon.body/1"

/** What a mind sent, read: something for the body, or a refusal to tell the mind. */
export type Heard = { body: ToBody } | { refused: string }

export const contract = {
    /** The manifest a mind is sent on connecting. */
    manifest(hello: Hello): Record<string, unknown> {
        return {
            contract: CONTRACT,
            kind: "terraverse.player",
            clock: { unit: "ms", step: hello.step, pace: { kind: "real" } },
            // the world place: local, x east, y north, z up, a node a metre (headless/senses.h)
            world: { space: { dimensions: 3 }, places: [{ name: "world", kind: "local" }] },
            frames: hello.frames,
            senses: hello.senses,
            controls: hello.controls,
            skills: [],
        }
    },

    /** One reading, the `seq`th on its channel: values, a picture's ref, or a text sense's object as its JSON. */
    reading(read: Read, seq: number): Record<string, unknown> {
        const base = { type: "reading", channel: read.ch, seq: seq, sensed: read.at }
        if (read.v !== null) return { ...base, values: read.v }
        if (read.ref !== null) return { ...base, ref: read.ref }
        return { ...base, text: JSON.stringify(read.text) }
    },

    applied(applied: Applied): Record<string, unknown> {
        return { type: "applied", channel: applied.ch, seq: applied.seq, sensed: applied.at, values: applied.v }
    },

    presence(state: "embodied"): Record<string, unknown> {
        return { type: "presence", state: state }
    },

    /** One message from a mind. */
    heard(text: string): Heard {
        let value: unknown
        try {
            value = JSON.parse(text)
        } catch {
            return { refused: "a message that isn't JSON" }
        }
        if (typeof value !== "object" || value === null || Array.isArray(value)) return { refused: "a message that isn't an object" }
        const f = Object.fromEntries(Object.entries(value))
        switch (f.type) {
            case "command": {
                if (typeof f.channel !== "string" || typeof f.seq !== "number") return { refused: "a command needs a channel and a seq" }
                if (f.channel === "/say") {
                    if (typeof f.text !== "string") return { refused: "/say takes text" }
                    return { body: { t: "say", text: f.text } }
                }
                if (f.trajectory !== undefined) return { refused: `${f.channel} takes no trajectories` }
                if (!Array.isArray(f.values) || !f.values.every(v => typeof v === "number")) return { refused: `${f.channel} takes values, numbers` }
                return { body: { t: "cmd", ch: f.channel, seq: f.seq, v: f.values.filter(v => typeof v === "number") } }
            }
            case "skill":
            case "cancel":
                return { refused: "a Terraverse body has no skills yet" }
            case "step":
                return { refused: "a Terraverse body keeps to the wall's time: stepped pace isn't wired yet" }
            default:
                return { refused: `${String(f.type)} isn't a message of ${CONTRACT}` }
        }
    },
}
