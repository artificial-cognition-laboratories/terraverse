// How the registry's numbers and dates read on a card. Pure.

export const format = {
    count(n: number): string {
        return n >= 1e6 ? `${(n / 1e6).toFixed(1)}M` : n >= 1e3 ? `${(n / 1e3).toFixed(1)}k` : String(n)
    },
    ago(iso: string, now = Date.now()): string {
        const days = Math.floor((now - Date.parse(iso)) / 86_400_000)
        return days < 1 ? "today" : days < 2 ? "yesterday" : days < 31 ? `${days}d ago` : days < 365 ? `${Math.floor(days / 30)}mo ago` : `${Math.floor(days / 365)}y ago`
    },
    /** A day as the website writes it: "20 Nov 2026". */
    date(iso: string): string {
        return new Date(iso).toLocaleDateString("en-GB", { day: "numeric", month: "short", year: "numeric" })
    },
    size(bytes: number): string {
        return bytes < 1024 ? `${bytes} B` : bytes < 1024 * 1024 ? `${(bytes / 1024).toFixed(1)} kB` : `${(bytes / 1024 / 1024).toFixed(1)} MB`
    },
    bare(name: string): string {
        return name.replace(/^@[^/]+\//, "")
    },
}
