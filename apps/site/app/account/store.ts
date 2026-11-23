/**
 * What this browser remembers of the account, behind one key. The seam: what
 * comes back from localStorage is checked once into a strict shape, and
 * anything else is dropped, never half-trusted.
 */
export interface Saved {
    /** Starred mods' names */
    stars: string[]
}

const KEY = "terraverse:account"
const EMPTY: Saved = { stars: [] }

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
    const stars = Reflect.get(value, "stars")
    if (!Array.isArray(stars) || !stars.every(s => typeof s === "string")) return null
    return { stars: stars }
}
