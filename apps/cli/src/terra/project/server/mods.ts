import { existsSync, realpathSync } from "node:fs"
import { resolve } from "node:path"
import { err } from "../../../errors"
import type { ConfigsT } from "../configs"
import type { PackagesT } from "../packages"
import { names } from "../names"
import type { Mod } from "../types"

type ModsOpts = {
    root: string
    packages: PackagesT
    configs: ConfigsT
}

/**
 * Mods — which mods a server runs, and in what order.
 *
 * The mods are the server's dependencies that have a mod.config.ts; nothing else runs.
 * Each loads after the mods it builds on, and every mod that runs is in the server's own
 * package.json, so the list a server publishes is the whole of its rules.
 */
export function Mods(opts: ModsOpts) {
    async function read(): Promise<Mod[]> {
        const server = await opts.packages.read(`${opts.root}/package.json`)
        const found = new Map<string, Mod>()
        for (const [name, spec] of Object.entries(server.dependencies)) {
            const mod = await one(name, spec)
            if (mod) found.set(name, mod)
        }
        rules.prefixes(found)
        rules.needs(found, opts.root)
        return rules.order(found)
    }

    /** One dependency, as a mod, or null if it's a library. */
    async function one(name: string, spec: string): Promise<Mod | null> {
        const linked = `${opts.root}/node_modules/${name}`
        if (!existsSync(linked))
            throw err("NOT_INSTALLED", { detail: `${name} is in package.json but not installed`, context: { mod: name } })
        // a local mod is used where it is: bun copies file: dependencies, and edits to the
        // original would go unseen in the copy
        const root = spec.startsWith("file:") ? realpathSync(resolve(opts.root, spec.slice(5))) : realpathSync(linked)
        if (!existsSync(`${root}/mod.config.ts`)) return null
        const pkg = await opts.packages.read(`${root}/package.json`)
        if (pkg.name !== name) throw err("PACKAGE_INVALID", { detail: `${spec} is called ${pkg.name}, not ${name}` })
        const config = await opts.configs.read("mod", root, name)
        return {
            name: name,
            prefix: names.prefix(name),
            version: pkg.version,
            root: root,
            title: config.title,
            local: spec.startsWith("file:"),
            settings: config.settings,
            needs: Object.keys(pkg.dependencies),
        }
    }

    return { read: read }
}

export type ModsT = ReturnType<typeof Mods>

const rules = {
    /** Two mods with one prefix would name each other's blocks. */
    prefixes(found: Map<string, Mod>): void {
        const owners = new Map<string, string>()
        for (const mod of found.values()) {
            const other = owners.get(mod.prefix)
            if (other) throw err("PREFIX_TAKEN", { detail: `${other} and ${mod.name} are both ${mod.prefix}` })
            owners.set(mod.prefix, mod.name)
        }
    },

    /** Narrows each mod's needs to the mods among them, and refuses mods the server doesn't run. */
    needs(found: Map<string, Mod>, root: string): void {
        for (const [name, mod] of found) {
            const needs = mod.needs.filter(dep => {
                if (found.has(dep)) return true
                const isMod = existsSync(`${root}/node_modules/${dep}/mod.config.ts`) || existsSync(`${mod.root}/node_modules/${dep}/mod.config.ts`)
                if (isMod) throw err("MOD_MISSING", { detail: `${mod.name} builds on ${dep}: terra install ${dep}`, context: { mod: mod.name, needs: dep } })
                return false
            })
            found.set(name, { ...mod, needs: needs })
        }
    },

    /** Dependency order, alphabetical among equals, so the order is the same everywhere. */
    order(found: Map<string, Mod>): Mod[] {
        const ordered: Mod[] = []
        const state = new Map<string, "visiting" | "done">()
        const visit = (mod: Mod, path: readonly string[]): void => {
            if (state.get(mod.name) === "done") return
            if (state.get(mod.name) === "visiting")
                throw err("MOD_CYCLE", { detail: [...path, mod.name].join(" → ") })
            state.set(mod.name, "visiting")
            for (const dep of mod.needs) {
                const next = found.get(dep)
                if (!next) throw err("MOD_MISSING", { detail: `${mod.name} builds on ${dep}` })
                visit(next, [...path, mod.name])
            }
            state.set(mod.name, "done")
            ordered.push(mod)
        }
        for (const mod of [...found.values()].sort((a, b) => a.name.localeCompare(b.name))) visit(mod, [])
        return ordered
    },
}
