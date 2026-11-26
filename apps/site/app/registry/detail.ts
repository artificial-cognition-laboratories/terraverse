import type { LocationQuery, LocationQueryRaw } from "vue-router"
import { format } from "~/lib/format"
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

/** A mod page's tabs, with what each holds: the side card lists them, and a phone's tab strip. */
export function contents(mod: Mod, version: string) {
    const needs = Object.keys(mod.versions.find(v => v.version === version)?.dependencies ?? {}).length
    return [
        { id: "readme" as const, label: "Readme", icon: "readme" as const },
        { id: "versions" as const, label: "Versions", icon: "versions" as const, count: mod.versions.length },
        { id: "dependencies" as const, label: "Dependencies", icon: "dependencies" as const, count: needs + mod.dependents.length },
        { id: "source" as const, label: "Source", icon: "source" as const, count: mod.files.length },
    ]
}

/** A mod's headline numbers; `starred`: the visitor's own star, not yet counted by the registry. */
export function numbers(mod: Mod, starred: boolean) {
    return [
        { icon: "download" as const, value: format.count(mod.installs), label: "installs" },
        { icon: "star" as const, value: format.count(mod.stars + (starred ? 1 : 0)), label: "stars" },
        { icon: "dependencies" as const, value: format.count(mod.dependents.length), label: "dependents" },
    ]
}
