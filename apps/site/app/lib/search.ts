// Finding a docs section by the words typed: pure, so the box that asks only
// shows what it's handed. Every word must appear; where it appears ranks it.

/** One heading's part of a page, as Nuxt Content's search sections give it. */
export interface Section {
    /** Its address: the page, and `#anchor` below the page's own title */
    id: string
    title: string
    /** The headings above it, from the page's title down */
    titles: string[]
    content: string
}

export interface Hit {
    id: string
    title: string
    /** Where it sits: "Server › Structure" */
    trail: string
    /** A few words of its text around the first match */
    snippet: string
}

/** Where a word lands, by weight: in the heading it's surely about that, in the text perhaps. */
const WEIGHT = { title: 10, trail: 4, content: 1 } as const

export const search = {
    find(sections: readonly Section[], query: string, limit = 8): Hit[] {
        const words = query.toLowerCase().split(/\s+/).filter(Boolean)
        if (!words.length) return []
        return sections
            .map(section => ({ section: section, score: score(section, words) }))
            .filter(s => s.score > 0)
            .sort((a, b) => b.score - a.score)
            .slice(0, limit)
            .map(({ section }) => ({
                id: section.id,
                title: section.title,
                trail: section.titles.join(" › "),
                snippet: snippet(section.content, words[0] ?? ""),
            }))
    },
}

// 0 unless every word is somewhere in the section
function score(section: Section, words: readonly string[]): number {
    const title = section.title.toLowerCase()
    const trail = section.titles.join(" ").toLowerCase()
    const content = section.content.toLowerCase()
    let total = 0
    for (const word of words) {
        const weight = (title.includes(word) ? WEIGHT.title : 0) + (trail.includes(word) ? WEIGHT.trail : 0) + (content.includes(word) ? WEIGHT.content : 0)
        if (!weight) return 0
        total += weight
    }
    return total
}

// About 90 characters of the text, around the word when the text has it
function snippet(content: string, word: string): string {
    const at = content.toLowerCase().indexOf(word)
    const start = Math.max(0, at - 30)
    const text = content.slice(start, start + 90).trim()
    return `${start > 0 ? "…" : ""}${text}${start + 90 < content.length ? "…" : ""}`
}
