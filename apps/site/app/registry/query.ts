import type { LocationQuery, LocationQueryRaw } from "vue-router"
import { CATEGORY_LABELS, SORT_LABELS, TYPE_LABELS, known } from "./labels"
import type { Search } from "./types"
import type { CategoryId } from "~/config/icons"

/** How Browse lays its results out. */
export type View = "grid" | "list"

/**
 * Browse's state lives in its URL, so every search is a link. This is the seam:
 * the query is read once into a strict search, and anything unknown is dropped.
 */
export const query = {
    read(q: LocationQuery): { search: Search; view: View } {
        const one = (key: string) => {
            const value = q[key]
            return typeof value === "string" ? value : ""
        }
        const categories = one("category").split(",").filter((c): c is CategoryId => known(CATEGORY_LABELS, c))
        const sort = one("sort")
        const type = one("type")
        const page = Number.parseInt(one("page"), 10)
        return {
            search: {
                query: one("q"),
                sort: known(SORT_LABELS, sort) ? sort : "relevance",
                categories: categories,
                type: known(TYPE_LABELS, type) ? type : null,
                official: one("official") === "1",
                page: Number.isFinite(page) && page > 0 ? page : 1,
            },
            view: one("view") === "list" ? "list" : "grid",
        }
    },

    /** A search as a URL query, leaving out every default so links stay short. */
    write(search: Search, view: View): LocationQueryRaw {
        const out: LocationQueryRaw = {}
        if (search.query) out.q = search.query
        if (search.sort !== "relevance") out.sort = search.sort
        if (search.categories.length) out.category = search.categories.join(",")
        if (search.type) out.type = search.type
        if (search.official) out.official = "1"
        if (search.page > 1) out.page = String(search.page)
        if (view === "list") out.view = "list"
        return out
    },
}

