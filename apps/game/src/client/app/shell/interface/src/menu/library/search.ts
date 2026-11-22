import type { RegistrySearch, RegistrySort } from "../../bridge/index.ts"

// A registry search as the Browse page holds it. Pure.

export const SORTS: readonly { value: RegistrySort; label: string }[] = [
    { value: "relevance", label: "Best match" },
    { value: "trending", label: "Trending" },
    { value: "stars", label: "Stars" },
    { value: "new", label: "New" },
    { value: "updated", label: "Updated" },
]

export const search = {
    blank(): RegistrySearch {
        return { query: "", sort: "relevance", categories: [], type: null, installed: false, page: 1 }
    },
    from(partial: Partial<RegistrySearch>): RegistrySearch {
        return { ...search.blank(), ...partial }
    },
}
