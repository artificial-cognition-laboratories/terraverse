/**
 * The wire between terra and a body process (client/app/headless/link.h): a JSON line each
 * way. What the body says is read once, here, into a strict shape; it's our own engine, but
 * it's another process, so a line that isn't the wire is refused by name, never half-believed.
 */

/** What the body is, said once it's in the world: its frames, senses and controls, as axon.body/1 has them. */
export type Hello = { t: "hello"; name: string; step: number; frames: unknown[]; senses: unknown[]; controls: unknown[] }
/** A picture's bytes, and where they are: a file the reading names (a ring in /dev/shm). */
export type Ref = { uri: string; mime: string; bytes: number }
/** One reading of one sense, at `at` ms on the body's clock: numbers, a JSON object for a text sense, or a picture's ref. */
export type Read = { t: "read"; ch: string; at: number; v: number[] | null; text: unknown; ref: Ref | null }
/** A command that took effect, after the body's clamping. */
export type Applied = { t: "applied"; ch: string; seq: number; at: number; v: number[] }
/** A command the body wouldn't take, and why. */
export type Refused = { t: "refused"; ch: string; seq: number; why: string }
/** The server let it go. */
export type Gone = { t: "gone"; why: string }

export type FromBody = Hello | Read | Applied | Refused | Gone

/** What terra tells a body: a command on a vector control, a line to say, or to leave. */
export type ToBody =
    | { t: "cmd"; ch: string; seq: number; v: number[] }
    | { t: "say"; text: string }
    | { t: "leave" }

export const wire = {
    /** One line of a body's stdout. */
    read(line: string): FromBody {
        let value: unknown
        try {
            value = JSON.parse(line)
        } catch {
            return refuse(`not JSON: ${line.slice(0, 200)}`)
        }
        if (typeof value !== "object" || value === null || Array.isArray(value)) return refuse(`not an object: ${line.slice(0, 200)}`)
        const f = Object.fromEntries(Object.entries(value))
        switch (f.t) {
            case "hello":
                return { t: "hello", name: text(f, "name"), step: num(f, "step"), frames: list(f, "frames"), senses: list(f, "senses"), controls: list(f, "controls") }
            case "read": {
                const v = f.v === undefined ? null : numbers(f, "v")
                const ref = f.ref === undefined ? null : refOf(f.ref)
                if (v === null && ref === null && f.text === undefined) return refuse(`a reading with no values, text or ref: ${line.slice(0, 200)}`)
                return { t: "read", ch: text(f, "ch"), at: num(f, "at"), v: v, text: f.text, ref: ref }
            }
            case "applied":
                return { t: "applied", ch: text(f, "ch"), seq: num(f, "seq"), at: num(f, "at"), v: numbers(f, "v") }
            case "refused":
                return { t: "refused", ch: text(f, "ch"), seq: num(f, "seq"), why: text(f, "why") }
            case "gone":
                return { t: "gone", why: text(f, "why") }
            default:
                return refuse(`a line that isn't the body's wire: ${line.slice(0, 200)}`)
        }
    },

    /** One message to a body, as its line. */
    write(message: ToBody): string {
        return `${JSON.stringify(message)}\n`
    },
}

function refOf(value: unknown): Ref {
    if (typeof value !== "object" || value === null || Array.isArray(value)) return refuse(`a ref that isn't an object: ${JSON.stringify(value)}`)
    const f = Object.fromEntries(Object.entries(value))
    return { uri: text(f, "uri"), mime: text(f, "mime"), bytes: num(f, "bytes") }
}

function refuse(detail: string): never {
    throw new Error(`body: ${detail}`)
}

function text(f: Record<string, unknown>, key: string): string {
    const v = f[key]
    if (typeof v !== "string") return refuse(`${key} isn't text: ${JSON.stringify(v)}`)
    return v
}

function num(f: Record<string, unknown>, key: string): number {
    const v = f[key]
    if (typeof v !== "number" || !Number.isFinite(v)) return refuse(`${key} isn't a number: ${JSON.stringify(v)}`)
    return v
}

function list(f: Record<string, unknown>, key: string): unknown[] {
    const v = f[key]
    if (!Array.isArray(v)) return refuse(`${key} isn't a list`)
    return v
}

function numbers(f: Record<string, unknown>, key: string): number[] {
    return list(f, key).map(v => {
        if (typeof v !== "number" || !Number.isFinite(v)) return refuse(`${key} holds ${JSON.stringify(v)}, not a number`)
        return v
    })
}
