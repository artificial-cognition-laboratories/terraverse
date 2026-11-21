import type { LocationQuery, LocationQueryRaw } from "vue-router"
import type { Mod } from "./types"

/** What a mod's page is showing: its tab, the version it's scoped to, and the open file. */
export type Tab = "readme" | "versions" | "dependencies" | "source"
export interface View {
    tab: Tab
    version: string
    file: string | null
}

const TABS: readonly Tab[] = ["readme", "versions", "dependencies", "source"]

/**
 * A mod page's view lives in its URL, so any view of a mod is a link. The seam:
 * read once against the mod, anything it doesn't have falls back to the default.
 */
export const view = {
    read(q: LocationQuery, mod: Mod): View {
        const one = (key: string) => {
            const value = q[key]
            return typeof value === "string" ? value : ""
        }
        const tab = TABS.find(t => t === one("tab")) ?? "readme"
        const version = mod.versions.some(v => v.version === one("v")) ? one("v") : mod.latestVersion
        const file = mod.files.some(f => f.path === one("file")) ? one("file") : null
        return { tab: tab, version: version, file: file }
    },

    write(v: View, mod: Mod): LocationQueryRaw {
        const out: LocationQueryRaw = {}
        if (v.tab !== "readme") out.tab = v.tab
        if (v.version !== mod.latestVersion) out.v = v.version
        if (v.tab === "source" && v.file) out.file = v.file
        return out
    },
}
