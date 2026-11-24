/**
 * What this browser remembers of the account, behind one key. The seam: what
 * comes back from localStorage is checked once into a strict shape, and
 * anything else is dropped, never half-trusted.
 */
import type { User } from "./types"

export interface Saved {
    /** Starred mods' names */
    stars: string[]
    /** Who was signed in here last: drawn until the session is read, so the topnav is right on its first frame */
    user: User | null
}

const KEY = "terraverse:account"
const EMPTY: Saved = { stars: [], user: null }

export const store = {
    read(): Saved {
        const raw = localStorage.getItem(KEY)
        if (!raw) return EMPTY
        return parse(json(raw)) ?? EMPTY
    },
    /** Changes what's given and keeps the rest */
    write(change: Partial<Saved>): void {
        localStorage.setItem(KEY, JSON.stringify({ ...store.read(), ...change }))
    },
}

// Not JSON is the same as not ours: this browser's own leftovers, dropped rather than trusted
function json(raw: string): unknown {
    try {
        return JSON.parse(raw)
    } catch (e) {
        if (e instanceof SyntaxError) return null
        throw e
    }
}

function parse(value: unknown): Saved | null {
    if (typeof value !== "object" || value === null) return null
    const stars = Reflect.get(value, "stars")
    if (!Array.isArray(stars) || !stars.every(s => typeof s === "string")) return null
    return { stars: stars, user: user(Reflect.get(value, "user")) }
}

// A remembered user that isn't whole is forgotten: the session will say who it is
function user(value: unknown): User | null {
    if (typeof value !== "object" || value === null) return null
    const field = (key: string) => Reflect.get(value, key)
    const text = (v: unknown) => typeof v === "string"
    const orNull = (v: unknown) => v === null || typeof v === "string"
    if (!text(field("id")) || !text(field("email")) || !orNull(field("name")) || !orNull(field("username"))) return null
    return { id: field("id") as string, email: field("email") as string, name: field("name") as string | null, username: field("username") as string | null }
}
