/** How the registry's numbers and names read on a card. Pure. */
export const format = {
    /** 9800 → "9.8k" */
    count(n: number): string {
        if (n < 1000) return String(n)
        if (n < 1_000_000) return `${(n / 1000).toFixed(n < 10_000 ? 1 : 0).replace(/\.0$/, "")}k`
        return `${(n / 1_000_000).toFixed(1).replace(/\.0$/, "")}M`
    },
    /** "Iron Rails" → "IR", the monogram a listing without an icon wears */
    monogram(title: string): string {
        const words = title.split(/\s+/).filter(Boolean)
        const letters = words.length > 1 ? (words[0]?.[0] ?? "") + (words[1]?.[0] ?? "") : title.slice(0, 2)
        return letters.toUpperCase()
    },
}
