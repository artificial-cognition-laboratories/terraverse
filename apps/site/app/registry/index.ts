// The registry, as the site reads it: one door for every page. Today it answers
// from the sample catalogue (./mock.ts); pointed at the ArcLabs API, no page changes.

import { CATEGORY_LABELS, keys } from "./labels"
import { MOCK, MOCK_SERVERS } from "./mock"
import type { Category, Front, Listing, ListingType, Mod, Owner, Results, Search, Server, Totals } from "./types"

export type * from "./types"
export { LISTING_TYPES } from "./types"
export { CATEGORY_LABELS, SORT_LABELS, TYPE_ICONS, TYPE_LABELS, keys, known } from "./labels"

const PAGE_SIZE = 12

/** A search with every field given: what a page asks when it only says some. */
export function search(partial: Partial<Search> = {}): Search {
    return { query: "", sort: "relevance", categories: [], type: null, official: false, page: 1, ...partial }
}

export function Registry(opts: { catalogue: readonly Mod[]; servers: readonly Server[] }) {
    const listings: Listing[] = opts.catalogue.map(listingOf)

    function find(s: Search): Results {
        const words = s.query.toLowerCase().split(/\s+/).filter(Boolean)
        const hits = listings
            .filter(l => !s.type || l.type === s.type)
            .filter(l => !s.official || l.official)
            .filter(l => s.categories.every(c => l.categories.includes(c)))
            .filter(l => words.every(w => `${l.name} ${l.title} ${l.description} ${l.owner}`.toLowerCase().includes(w)))
            .sort(ORDER[s.sort])
        const start = (s.page - 1) * PAGE_SIZE
        return { listings: hits.slice(start, start + PAGE_SIZE), total: hits.length, page: s.page, pageSize: PAGE_SIZE }
    }

    return {
        search: async (s: Search): Promise<Results> => find(s),

        front: async (): Promise<Front> => ({
            featured: listings.filter(l => l.official).sort(ORDER.stars).slice(0, 4),
            categories: categories(listings),
            shelves: SHELVES.map(shelf => ({ ...shelf, listings: find(search(shelf.search)).listings.slice(0, 4) })),
            total: listings.length,
        }),

        /** null when there's no such mod: the page says so */
        mod: async (name: string): Promise<Mod | null> => opts.catalogue.find(m => m.name === name) ?? null,

        owner: async (scope: string): Promise<Owner | null> => {
            const theirs = listings.filter(l => l.owner === scope.replace(/^@/, "")).sort(ORDER.stars)
            if (!theirs.length) return null
            return {
                scope: scope,
                official: theirs.some(l => l.official),
                listings: theirs,
                stars: theirs.reduce((n, l) => n + l.stars, 0),
                installs: theirs.reduce((n, l) => n + l.installs, 0),
            }
        },

        categories: async (): Promise<Category[]> => categories(listings),
        totals: async (): Promise<Totals> => ({
            listings: listings.length,
            installs: listings.reduce((n, l) => n + l.installs, 0),
            authors: new Set(listings.map(l => l.owner)).size,
            types: { mod: count(listings, "mod"), textures: count(listings, "textures"), shaders: count(listings, "shaders"), world: count(listings, "world") },
        }),
        servers: async (): Promise<Server[]> => [...opts.servers],
    }
}
export type Registry = ReturnType<typeof Registry>

/** The registry every page reads. TEMP(2026-11-21): the sample catalogue, until the API serves all of it. */
export function useRegistry(): Registry {
    return Registry({ catalogue: MOCK, servers: MOCK_SERVERS })
}

// Discover's shelves: each one search, answered when the front page is asked for
const SHELVES: { id: string; title: string; search: Partial<Search> }[] = [
    { id: "trending", title: "Trending", search: { sort: "trending" } },
    { id: "new", title: "Recently updated", search: { sort: "updated" } },
    { id: "official", title: "Official", search: { official: true, sort: "stars" } },
]

// Each sort, as a comparison: one table every search reads
const ORDER: Record<Search["sort"], (a: Listing, b: Listing) => number> = {
    relevance: (a, b) => b.stars - a.stars,
    stars: (a, b) => b.stars - a.stars,
    trending: (a, b) => b.installs / age(b) - a.installs / age(a),
    new: (a, b) => b.updatedAt.localeCompare(a.updatedAt),
    updated: (a, b) => b.updatedAt.localeCompare(a.updatedAt),
}

function age(l: Listing): number {
    return Math.max(1, (Date.parse("2026-11-21") - Date.parse(l.updatedAt)) / 86_400_000)
}

function listingOf(m: Mod): Listing {
    return {
        name: m.name, title: m.title, description: m.description, owner: m.owner, latestVersion: m.latestVersion,
        stars: m.stars, installs: m.installs, updatedAt: m.updatedAt, type: m.type, categories: m.categories, official: m.official,
    }
}

function count(listings: readonly Listing[], type: ListingType): number {
    return listings.filter(l => l.type === type).length
}

function categories(listings: readonly Listing[]): Category[] {
    return keys(CATEGORY_LABELS).filter(id => id !== "servers").map(id => ({
        id: id,
        label: CATEGORY_LABELS[id],
        count: listings.filter(l => l.categories.includes(id)).length,
    }))
}
