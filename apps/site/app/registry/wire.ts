// The ArcLabs registry API, as the site reads it: the seam. Every answer is
// checked once into the site's own shapes (./types); past here nothing re-checks.
// The routes: ../arclabs/apps/backend/server/api/terrafirma/registry/.

import { CATEGORY_LABELS, known } from "./labels"
import type { File, Listing, Version } from "./types"
import type { CategoryId } from "~/config/icons"

/** A refusal the API gave, or an answer not in the shape it promised: either is ours to fix, so it throws. */
export class WireError extends Error {}

/** A mod or owner the registry doesn't have: the page's 404, not an error. */
const MISSING = 404

export function Wire(opts: { api: string }) {
    const base = `${opts.api.replace(/\/$/, "")}/api/terrafirma/registry`

    /** The answer, or null for a 404 */
    async function get(path: string, query: Record<string, string> = {}): Promise<unknown> {
        const params = new URLSearchParams(Object.entries(query).filter(([, v]) => v !== ""))
        const url = `${base}/${path}${params.size ? `?${params}` : ""}`
        const res = await fetch(url)
        if (res.status === MISSING) return null
        if (!res.ok) throw new WireError(`${url}: ${res.status} ${res.statusText}`)
        return res.json()
    }

    return { get: get }
}
export type Wire = ReturnType<typeof Wire>

/** Each answer's shape, read strictly. */
export const parse = {
    listing(value: unknown): Listing {
        const v = record(value, "a listing")
        return {
            name: text(v.name, "name"),
            title: text(v.title, "title"),
            description: v.description === null ? "" : text(v.description, "description"),
            owner: text(v.owner, "owner"),
            latestVersion: v.latestVersion === null ? "" : text(v.latestVersion, "latestVersion"),
            stars: count(v.stars, "stars"),
            installs: count(v.installs, "installs"),
            updatedAt: text(v.updatedAt, "updatedAt"),
            type: "mod",
            categories: categories(v.categories),
            official: flag(v.official, "official"),
        }
    },

    version(value: unknown): Version {
        const v = record(value, "a version")
        const deps = v.dependencies === undefined ? {} : record(v.dependencies, "dependencies")
        return {
            version: text(v.version, "version"),
            publishedAt: text(v.publishedAt, "publishedAt"),
            api: String(v.api),
            dependencies: Object.fromEntries(Object.entries(deps).map(([k, range]) => [k, text(range, `dependency ${k}`)])),
            withheld: v.withheld === null ? null : text(v.withheld, "withheld"),
        }
    },

    file(value: unknown): File {
        const v = record(value, "a file")
        return { path: text(v.path, "path"), size: count(v.size, "size"), text: v.text === null ? null : text(v.text, "text") }
    },

    /** The per-category counts: every category, zero where the API has none */
    counts(value: unknown): Record<CategoryId, number> {
        const v = record(value, "category counts")
        return Object.fromEntries(Object.keys(CATEGORY_LABELS).map(id => [id, v[id] === undefined ? 0 : count(v[id], id)])) as Record<CategoryId, number>
    },

    record: record,
    text: text,
    count: count,
    list(value: unknown, what: string): unknown[] {
        if (!Array.isArray(value)) throw new WireError(`${what} isn't a list`)
        return value
    },
}

function record(value: unknown, what: string): Record<string, unknown> {
    if (typeof value !== "object" || value === null || Array.isArray(value)) throw new WireError(`${what} isn't an object`)
    return value as Record<string, unknown>
}

function text(value: unknown, what: string): string {
    if (typeof value !== "string") throw new WireError(`${what} isn't text`)
    return value
}

function count(value: unknown, what: string): number {
    if (typeof value !== "number" || !Number.isFinite(value)) throw new WireError(`${what} isn't a number`)
    return value
}

function flag(value: unknown, what: string): boolean {
    if (typeof value !== "boolean") throw new WireError(`${what} isn't true or false`)
    return value
}

// A category the site doesn't know yet is left off the card, not an error: the API's list may grow first
function categories(value: unknown): CategoryId[] {
    if (!Array.isArray(value)) throw new WireError("categories isn't a list")
    return value.filter((c): c is CategoryId => typeof c === "string" && known(CATEGORY_LABELS, c) && c !== "servers")
}
