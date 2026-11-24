/**
 * What this browser last saw of each live read, so a page draws it at once and
 * swaps in the fresh answer when it lands. One localStorage key per read, under
 * a version: bump VERSION when a cached shape changes, and every old entry is
 * dropped rather than drawn wrong. The newest LIMIT reads are kept.
 */

const VERSION = 1
const PREFIX = `terraverse:live:v${VERSION}:`
const INDEX = `terraverse:live:v${VERSION}`
const LIMIT = 60
/** A read bigger than this (a mod's whole source) isn't worth a browser's storage */
const MAX_BYTES = 200_000

export const cache = {
    read<T>(key: string): T | undefined {
        const raw = localStorage.getItem(PREFIX + key)
        if (raw === null) return undefined
        // Written by cache.write alone, under this version: it's our own shape
        return JSON.parse(raw) as T
    },

    write(key: string, value: unknown): void {
        const raw = JSON.stringify(value)
        if (raw.length > MAX_BYTES) return
        const keys = [key, ...index().filter(k => k !== key)]
        for (const dropped of keys.splice(LIMIT)) localStorage.removeItem(PREFIX + dropped)
        try {
            localStorage.setItem(PREFIX + key, raw)
            localStorage.setItem(INDEX, JSON.stringify(keys))
        } catch (e) {
            // Storage full: a cache that can't keep something just doesn't, and the page still has it live
            if (e instanceof DOMException && e.name === "QuotaExceededError") return
            throw e
        }
    },

    /** Drops every earlier version's entries: once, as the page wakes. */
    sweep(): void {
        for (const key of Object.keys(localStorage)) {
            if (key.startsWith("terraverse:live:") && key !== INDEX && !key.startsWith(PREFIX)) localStorage.removeItem(key)
        }
    },
}

function index(): string[] {
    const raw = localStorage.getItem(INDEX)
    return raw === null ? [] : (JSON.parse(raw) as string[])
}
