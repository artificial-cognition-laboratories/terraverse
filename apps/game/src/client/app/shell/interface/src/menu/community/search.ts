import type { RegistryCategory, RegistrySearch, RegistrySort } from "../../bridge/index.ts"

// A registry search as the Mods page holds it, and the chips that show it. Pure.

export const SORTS: readonly { value: RegistrySort; label: string }[] = [
    { value: "relevance", label: "Best match" },
    { value: "trending", label: "Trending" },
    { value: "stars", label: "Stars" },
    { value: "new", label: "New" },
    { value: "updated", label: "Updated" },
]

export const KINDS = [
    { value: "all", label: "All" },
    { value: "mod", label: "Mods" },
    { value: "textures", label: "Textures" },
] as const

export const search = {
    blank(): RegistrySearch {
        return { query: "", sort: "relevance", categories: [], type: null, installed: false, page: 1 }
    },
    from(partial: Partial<RegistrySearch>): RegistrySearch {
        return { ...search.blank(), ...partial }
    },
    /** One chip per filter in force; its id is what `drop` takes off. */
    chips(s: RegistrySearch, categories: readonly RegistryCategory[]): { id: string; label: string }[] {
        return [
            ...(s.query ? [{ id: "query", label: `“${s.query}”` }] : []),
            ...(s.type ? [{ id: "type", label: s.type === "mod" ? "Mods" : "Texture packs" }] : []),
            ...s.categories.map((c) => ({ id: `category:${c}`, label: categories.find((k) => k.id === c)?.label ?? c })),
            ...(s.installed ? [{ id: "installed", label: "Installed" }] : []),
        ]
    },
    drop(s: RegistrySearch, id: string): RegistrySearch {
        if (id === "query") return { ...s, query: "", page: 1 }
        if (id === "type") return { ...s, type: null, page: 1 }
        if (id === "installed") return { ...s, installed: false, page: 1 }
        return { ...s, categories: s.categories.filter((c) => `category:${c}` !== id), page: 1 }
    },
}
