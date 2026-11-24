// The registry, as the site reads it: one door for every page, answered by the
// ArcLabs API (./wire.ts is the seam). Pages never see the wire's shapes.

import { CATEGORY_LABELS, keys } from "./labels"
import { Wire, parse } from "./wire"
import type { Category, Front, Listing, Mod, Owner, Results, Search, Server, Totals } from "./types"

export type * from "./types"
export { LISTING_TYPES } from "./types"
export { CATEGORY_LABELS, SORT_LABELS, TYPE_ICONS, TYPE_LABELS, keys, known } from "./labels"

/** A search with every field given: what a page asks when it only says some. */
export function search(partial: Partial<Search> = {}): Search {
    return { query: "", sort: "relevance", categories: [], type: null, official: false, page: 1, ...partial }
}

export function Registry(opts: { wire: Wire }) {
    const { get } = opts.wire

    async function find(s: Search): Promise<Results> {
        // Only mods publish yet: a search for another type is honestly empty
        if (s.type !== null && s.type !== "mod") return { listings: [], total: 0, page: s.page, pageSize: 0 }
        const v = parse.record(await get("search", {
            q: s.query,
            sort: s.sort,
            category: s.categories.join(","),
            official: s.official ? "1" : "",
            page: String(s.page),
        }), "a search")
        return {
            listings: parse.list(v.mods, "mods").map(parse.listing),
            total: parse.count(v.total, "total"),
            page: parse.count(v.page, "page"),
            pageSize: parse.count(v.pageSize, "pageSize"),
        }
    }

    async function facets(): Promise<{ categories: Category[]; totals: Totals }> {
        const v = parse.record(await get("facets"), "facets")
        return facetsOf(v)
    }

    async function listings(names: readonly string[]): Promise<Listing[]> {
        const found = await Promise.all(names.map(name => get(`packages/${name}`)))
        return found.filter(f => f !== null).map(parse.listing)
    }

    return {
        search: find,

        front: async (): Promise<Front> => {
            const v = parse.record(await get("front"), "the front page")
            const shelves = parse.list(v.shelves, "shelves").map(s => parse.record(s, "a shelf"))
            const { categories, totals } = facetsOf(parse.record(v.facets, "facets"))
            return {
                featured: parse.list(v.featured, "featured").map(parse.listing),
                categories: categories,
                shelves: SHELVES.map(shelf => ({
                    ...shelf,
                    listings: parse.list(shelves.find(s => s.id === shelf.id)?.listings ?? [], shelf.id).map(parse.listing),
                })),
                total: totals.listings,
            }
        },

        /** null when there's no such mod: the page says so */
        mod: async (name: string): Promise<Mod | null> => {
            const found = await get(`packages/${name}`)
            if (found === null) return null
            const v = parse.record(found, name)
            const listing = parse.listing(v)
            const source = listing.latestVersion ? parse.record(await get(`source/${name}/${listing.latestVersion}`), "its source") : null
            return {
                ...listing,
                readme: v.readme === null ? "" : parse.text(v.readme, "readme"),
                license: v.license === null ? "" : parse.text(v.license, "license"),
                createdAt: parse.text(v.createdAt, "createdAt"),
                deprecated: v.deprecated === null ? null : parse.text(v.deprecated, "deprecated"),
                versions: parse.list(v.versions, "versions").map(parse.version),
                files: source ? parse.list(source.files, "files").map(parse.file) : [],
                dependents: parse.list(v.dependents, "dependents").map(d => parse.text(d, "a dependent")),
            }
        },

        /** These mods' cards, in the order asked; a name the registry doesn't hold is left out */
        listings: listings,

        /** null when nobody holds the scope */
        owner: async (scope: string): Promise<Owner | null> => {
            const found = await get(`owners/${scope}`)
            if (found === null) return null
            const v = parse.record(found, scope)
            return {
                scope: scope,
                official: v.official === true,
                listings: parse.list(v.mods, "mods").map(parse.listing),
                stars: parse.count(v.stars, "stars"),
                installs: parse.count(v.installs, "installs"),
            }
        },

        categories: async (): Promise<Category[]> => (await facets()).categories,
        totals: async (): Promise<Totals> => (await facets()).totals,
        // TEMP(2026-11-24): no public server list yet (phase 3: hosted worlds marked public); none is the honest answer
        servers: async (): Promise<Server[]> => [],
    }
}
export type Registry = ReturnType<typeof Registry>

/** The registry every page reads: ArcLabs' API, production's or the local stack's (nuxt.config's `api`). */
export function useRegistry(): Registry {
    return Registry({ wire: Wire({ api: useRuntimeConfig().public.api }) })
}

// Discover's shelves: the API answers each by id, the site names and links them
const SHELVES: { id: string; title: string; search: Partial<Search> }[] = [
    { id: "trending", title: "Trending", search: { sort: "trending" } },
    { id: "updated", title: "Recently updated", search: { sort: "updated" } },
    { id: "official", title: "Official", search: { official: true, sort: "stars" } },
]

function facetsOf(v: Record<string, unknown>): { categories: Category[]; totals: Totals } {
    const counts = parse.counts(v.categories)
    const types = parse.record(v.types, "types")
    return {
        categories: keys(CATEGORY_LABELS).filter(id => id !== "servers").map(id => ({ id: id, label: CATEGORY_LABELS[id], count: counts[id] })),
        totals: {
            listings: parse.count(v.listings, "listings"),
            installs: parse.count(v.installs, "installs"),
            authors: parse.count(v.authors, "authors"),
            types: { mod: parse.count(types.mod, "mods"), textures: 0, shaders: 0, world: 0 },
        },
    }
}
