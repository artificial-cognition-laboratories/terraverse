import type { CategoryId, IconId } from "~/config/icons"
import type { ListingType, Sort } from "./types"

/** How each category, type and sort reads: one table each, every page reads them. */
export const CATEGORY_LABELS: Record<CategoryId, string> = {
    games: "Games",
    survival: "Survival",
    building: "Building",
    machines: "Machines",
    matter: "Matter",
    world: "Worlds",
    textures: "Shaders & textures",
    library: "Libraries",
    servers: "Servers",
}

export const TYPE_LABELS: Record<ListingType, string> = {
    mod: "Mods",
    textures: "Textures",
    shaders: "Shaders",
    world: "Worlds",
}

/** Each type's glyph in Browse's filters; `all` is no type chosen. */
export const TYPE_ICONS: Record<ListingType | "all", IconId> = {
    all: "all",
    mod: "mod",
    textures: "textures",
    shaders: "shaders",
    world: "world",
}

export const SORT_LABELS: Record<Sort, string> = {
    relevance: "Relevance",
    trending: "Trending",
    stars: "Most starred",
    updated: "Recently updated",
    new: "Newest",
}

/** Whether a word (from the URL, a select) is one of a table's keys: the table is the list of what's valid. */
export function known<K extends string>(table: Record<K, unknown>, key: string): key is K {
    return Object.hasOwn(table, key)
}

/** A table's keys, typed: every one is known by construction. */
export function keys<K extends string>(table: Record<K, unknown>): K[] {
    return Object.keys(table).filter((k): k is K => known(table, k))
}
