import { err } from "../../errors"

/** Names: what a package is called, and what that makes everything it creates. */
export const names = {
    /**
     * `@you/kilns` → `kilns`: the prefix everything the mod creates is named under. Node
     * names take [a-z0-9_], so a package's hyphens become underscores.
     */
    prefix(name: string): string {
        const prefix = name.slice(name.lastIndexOf("/") + 1).replaceAll("-", "_")
        if (!/^[a-z0-9_]+$/.test(prefix)) throw err("MOD_NAME_INVALID", { detail: `${name} can't be a prefix`, context: { mod: name } })
        return prefix
    },

    /** `kilns`, `@you/kilns`, or `kilns` under `--scope @you`: the package's name. */
    package(given: string, scope: string | undefined): string {
        if (given.startsWith("@")) {
            if (!/^@[a-z0-9-]+\/[a-z0-9-]+$/.test(given)) throw err("MOD_NAME_INVALID", { detail: `${given} isn't @scope/name` })
            return given
        }
        if (!/^[a-z0-9-]+$/.test(given)) throw err("MOD_NAME_INVALID", { detail: `${given}: a name is lower-case letters, digits and hyphens` })
        if (scope === undefined) return given
        const bare = scope.startsWith("@") ? scope.slice(1) : scope
        if (!/^[a-z0-9-]+$/.test(bare)) throw err("MOD_NAME_INVALID", { detail: `${scope} isn't a scope` })
        return `@${bare}/${given}`
    },

    /** `@you/hunger-games` → "Hunger games": a title until its author writes one. */
    title(name: string): string {
        const words = name.slice(name.lastIndexOf("/") + 1).replaceAll("-", " ")
        return words.charAt(0).toUpperCase() + words.slice(1)
    },
}
