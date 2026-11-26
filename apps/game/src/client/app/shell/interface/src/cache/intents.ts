import type { Intents } from "../bridge/index.ts"
import TYPES from "../bridge/types.ts?raw"

/**
 * The intents the page keeps answers to, and where each one is kept: `disk` survives a restart
 * (IndexedDB), `session` lives until the page does. Only reads belong here: an answer that's
 * shown again before the live one comes back must be safe to show.
 */
export const CACHED = {
    "registry.front": "disk",
    "registry.search": "disk",
    "registry.mod": "disk",
    "registry.file": "disk",
    "servers.list": "disk",
    "mods.local": "disk",
    // a mod you're editing: its blocks change with every save, and they're heavy
    "mods.contents": "session",
} as const satisfies Partial<Record<keyof Intents, "disk" | "session">>

export type CachedKind = keyof typeof CACHED
export type Ask<K extends CachedKind> = Intents[K]["request"]
export type Answer<K extends CachedKind> = Intents[K]["response"]

/**
 * What a kept answer was shaped by: the bridge's types. Any change to them drops every entry,
 * so an old answer is never read as a new shape. Keyed by what actually changes them (a build
 * that doesn't touch the types keeps the cache).
 */
export const SHAPE = hash(TYPES)

/** One entry's key: the intent and its request, with keys sorted so `{a,b}` and `{b,a}` are one. */
export function keyOf<K extends CachedKind>(kind: K, request: Ask<K>): string {
    return `${kind}|${stable(request)}`
}

function stable(value: unknown): string {
    if (value === null || typeof value !== "object") return JSON.stringify(value) ?? "null"
    if (Array.isArray(value)) return `[${value.map(stable).join(",")}]`
    const record = value as Record<string, unknown>
    return `{${Object.keys(record).filter((k) => record[k] !== undefined).sort()
        .map((k) => `${JSON.stringify(k)}:${stable(record[k])}`).join(",")}}`
}

// FNV-1a: a short, stable fingerprint, not a security boundary
function hash(text: string): string {
    let h = 0x811c9dc5
    for (let i = 0; i < text.length; i++) h = Math.imul(h ^ text.charCodeAt(i), 0x01000193)
    return (h >>> 0).toString(36)
}
