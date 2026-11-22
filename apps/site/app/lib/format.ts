/** How the registry's numbers, names and dates read. Pure. */
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
    /** "2026-11-18" → "18 Nov 2026": absolute, so the prerendered page and the live one agree */
    date(iso: string): string {
        return new Date(`${iso.slice(0, 10)}T12:00:00Z`).toLocaleDateString("en-GB", { day: "numeric", month: "short", year: "numeric", timeZone: "UTC" })
    },
    /** 2210 → "2.2 KB" */
    size(bytes: number): string {
        if (bytes < 1024) return `${bytes} B`
        return `${(bytes / 1024).toFixed(1).replace(/\.0$/, "")} KB`
    },
    /** "@terraverse/hunger-games" → "hunger-games" */
    bare(name: string): string {
        return name.slice(name.indexOf("/") + 1)
    },
}
