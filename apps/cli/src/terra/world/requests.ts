/**
 * The world contract's requests (axon.world/0), as terra serves them: each line on stdin read
 * once into a strict shape, or a refusal saying why. The contract's own types are arclabs'
 * (`@arcforge/types` world.ts); terra doesn't depend on them, so these are the lines it reads.
 */
export type Request =
    | { type: "place"; id: number; body: string; name: string; token: string | null }
    | { type: "remove"; id: number; name: string }
    /** The rest carry nothing terra reads yet: pace, step, reset, snapshot and restore aren't wired. */
    | { type: (typeof ONLY_ID)[number]; id: number }

/** A line that wasn't a request: its id when it had one, and why. */
export type Unread = { id: number | null; code: "WIRE" | "CONTRACT_UNSAID"; message: string }

const ONLY_ID = ["pace", "step", "reset", "snapshot", "restore", "state", "close"] as const

export const requests = {
    /** One line of stdin. */
    read(line: string): Request | Unread {
        let value: unknown
        try {
            value = JSON.parse(line)
        } catch {
            return { id: null, code: "WIRE", message: `not JSON: ${line.slice(0, 200)}` }
        }
        if (typeof value !== "object" || value === null || Array.isArray(value)) return { id: null, code: "WIRE", message: `not an object: ${line.slice(0, 200)}` }
        const fields = Object.fromEntries(Object.entries(value))
        const id = fields.id
        if (typeof id !== "number" || !Number.isInteger(id) || id < 0) return { id: null, code: "WIRE", message: `a request with no id: ${line.slice(0, 200)}` }
        const type = fields.type
        if (type === "place") {
            if (typeof fields.body !== "string" || typeof fields.name !== "string") return { id: id, code: "WIRE", message: "place needs a body and a name" }
            if (fields.token !== undefined && typeof fields.token !== "string") return { id: id, code: "WIRE", message: "place's token is text" }
            return { type: "place", id: id, body: fields.body, name: fields.name, token: typeof fields.token === "string" ? fields.token : null }
        }
        if (type === "remove") {
            if (typeof fields.name !== "string") return { id: id, code: "WIRE", message: "remove needs a name" }
            return { type: "remove", id: id, name: fields.name }
        }
        const known = ONLY_ID.find(t => t === type)
        if (known === undefined) return { id: id, code: "CONTRACT_UNSAID", message: `${String(type)} is not a request of axon.world/0` }
        return { type: known, id: id }
    },
}
