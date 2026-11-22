// What the registry is made of, as the site shows it. Aligned with the game's
// bridge (apps/game/.../interface/src/bridge/types.ts: RegistryListing, RegistryMod),
// so a mod means the same thing in the game and on the web.

import type { CategoryId } from "~/config/icons"

/** Every kind of listing, in the order Browse offers them. */
export const LISTING_TYPES = ["mod", "textures", "shaders", "world"] as const
export type ListingType = (typeof LISTING_TYPES)[number]
/** `relevance`: best match for a query, by stars without one. */
export type Sort = "relevance" | "trending" | "new" | "stars" | "updated"

/** A mod as a card shows it. `name` is its address: `@scope/name`, and its page's path. */
export interface Listing {
    name: string
    title: string
    description: string
    owner: string
    latestVersion: string
    stars: number
    installs: number
    /** ISO date */
    updatedAt: string
    type: ListingType
    categories: CategoryId[]
    official: boolean
}

export interface Version {
    version: string
    /** ISO date */
    publishedAt: string
    /** The game API it was built against */
    api: string
    /** What it needs: `@scope/name` → version range */
    dependencies: Record<string, string>
    /** Why it was pulled, when it was */
    withheld: string | null
}

/** One file in a version: its text when it's text, null for an image or other binary. */
export interface File {
    path: string
    size: number
    text: string | null
}

/** A mod as its page shows it. */
export interface Mod extends Listing {
    readme: string
    license: string
    /** ISO date */
    createdAt: string
    deprecated: string | null
    versions: Version[]
    files: File[]
    /** Mods that depend on this one */
    dependents: string[]
}

export interface Category {
    id: CategoryId
    label: string
    count: number
}

export interface Search {
    query: string
    sort: Sort
    categories: CategoryId[]
    type: ListingType | null
    official: boolean
    page: number
}

export interface Results {
    listings: Listing[]
    total: number
    page: number
    pageSize: number
}

/** Discover: featured mods, the categories, and shelves each answered by one search. */
export interface Shelf {
    id: string
    title: string
    search: Partial<Search>
    listings: Listing[]
}

export interface Front {
    featured: Listing[]
    categories: Category[]
    shelves: Shelf[]
    total: number
}

export interface Server {
    name: string
    game: string
    players: number
    slots: number
    region: string
    mods: number
}

/** An owner, as their page shows them. */
export interface Owner {
    scope: string
    official: boolean
    listings: Listing[]
    stars: number
    installs: number
}

/** The whole catalogue in a few numbers: Browse's stats and its type counts. */
export interface Totals {
    listings: number
    installs: number
    authors: number
    types: Record<ListingType, number>
}
