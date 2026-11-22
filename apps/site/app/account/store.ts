import type { User } from "./types"

/**
 * What this browser remembers of the account, behind one key. The seam: what
 * comes back from localStorage is checked once into a strict shape, and
 * anything else is dropped, never half-trusted.
 */
export interface Saved {
    /** Who is signed in; null when signed out */
    user: User | null
    /** The username claimed, kept across sign-outs so signing back in finds it */
    claimed: string | null
    /** Starred mods' names */
    stars: string[]
}

const KEY = "terraverse:account"
const EMPTY: Saved = { user: null, claimed: null, stars: [] }

export const store = {
    read(): Saved {
        const raw = localStorage.getItem(KEY)
        if (!raw) return EMPTY
        return parse(json(raw)) ?? EMPTY
    },
    write(saved: Saved): void {
        localStorage.setItem(KEY, JSON.stringify(saved))
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
    const user = Reflect.get(value, "user")
    const claimed = Reflect.get(value, "claimed")
    const stars = Reflect.get(value, "stars")
    if (!(user === null || isUser(user))) return null
    if (!(claimed === null || typeof claimed === "string")) return null
    if (!Array.isArray(stars) || !stars.every(s => typeof s === "string")) return null
    return { user: user, claimed: claimed, stars: stars }
}

function isUser(value: unknown): value is User {
    if (typeof value !== "object" || value === null) return false
    const field = (key: string) => Reflect.get(value, key)
    const nullable = (v: unknown) => v === null || typeof v === "string"
    return typeof field("id") === "string" && typeof field("email") === "string" && nullable(field("name")) && nullable(field("username"))
}
