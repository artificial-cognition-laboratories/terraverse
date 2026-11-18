import { cpSync, existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs"
import { basename, relative, resolve } from "node:path"
import { err } from "../../errors"
import type { PlacesT } from "../places"
import { freshSystem, names, Packages, templates, type ProjectT, type ProjectsT, type ServerProjectT } from "../project"

/**
 * Scratch — the server a project runs in under `terra dev`. A server is its own. A mod gets
 * a scratch universe in terra's cache: a server running it and the mods it builds on, kept
 * between runs so its world is still there. Or, with `in`, a copy of a real server: its
 * mods, its config and a fresh copy of its save, with this mod running in it too.
 *
 * Never inside the mod's folder: bun copies a file: dependency whole, and a scratch server's
 * node_modules inside the mod it copies would be copied into itself.
 */
export function Scratch(opts: { projects: ProjectsT; places: PlacesT }) {
    const packages = Packages({})

    async function serverOf(project: ProjectT, e: { in: string | null }): Promise<ServerProjectT> {
        if (project.kind === "server") {
            if (e.in) throw err("USAGE", { detail: "--in runs a mod inside a server: this is a server already" })
            return opts.projects.server(project.root)
        }
        return e.in ? inside(project, opts.projects.server(e.in)) : alone(project)
    }

    /** The mod, and the mods it builds on, in a universe of its own. */
    async function alone(mod: ProjectT): Promise<ServerProjectT> {
        const pkg = await packages.read(`${mod.root}/package.json`)
        const root = place(mod, names.prefix(pkg.name))
        mkdirSync(root, { recursive: true })
        // its dependencies follow the mod's every run; its system is kept, and so its world
        writeFileSync(`${root}/package.json`, manifest(`@dev/${names.prefix(pkg.name)}`, {
            ...rebase(pkg.dependencies, mod.root, root),
            [pkg.name]: `file:${relative(root, mod.root)}`,
        }))
        if (!existsSync(`${root}/server.config.ts`)) {
            const scaffold = templates.server({ name: pkg.name, title: `${names.title(pkg.name)} (dev)`, system: freshSystem() })
            writeFileSync(`${root}/server.config.ts`, scaffold.files["server.config.ts"] ?? "")
        }
        return opts.projects.server(root)
    }

    /** The mod inside a copy of `server`: its rules, its settings, its save as it is now. */
    async function inside(mod: ProjectT, server: ServerProjectT): Promise<ServerProjectT> {
        const target = await packages.read(`${server.root}/package.json`)
        const pkg = await packages.read(`${mod.root}/package.json`)
        const root = place(mod, `${names.prefix(pkg.name)}-in-${basename(server.root)}`)
        mkdirSync(root, { recursive: true })
        writeFileSync(`${root}/package.json`, manifest(`@dev/${names.prefix(pkg.name)}-in-${names.prefix(target.name)}`, {
            ...rebase(target.dependencies, server.root, root),
            [pkg.name]: `file:${relative(root, mod.root)}`,
        }))
        cpSync(`${server.root}/server.config.ts`, `${root}/server.config.ts`)
        // a copy of the save as it is now, every run: the real one is never touched
        rmSync(`${root}/save`, { recursive: true, force: true })
        if (existsSync(`${server.root}/save`)) cpSync(`${server.root}/save`, `${root}/save`, { recursive: true })
        return opts.projects.server(root)
    }

    /** A scratch server's folder: named for people, made unique by the mod's own folder. */
    function place(mod: ProjectT, name: string): string {
        return `${opts.places.cache()}/dev/${name}-${Bun.hash(mod.root).toString(16).slice(0, 8)}`
    }

    return { serverOf: serverOf }
}

export type ScratchT = ReturnType<typeof Scratch>

function manifest(name: string, dependencies: Record<string, string>): string {
    return JSON.stringify({ name: name, private: true, type: "module", dependencies: dependencies }, null, 4) + "\n"
}

/** Dependencies written from `from`, as they'd be written from `to`: file: paths moved. */
function rebase(dependencies: Readonly<Record<string, string>>, from: string, to: string): Record<string, string> {
    return Object.fromEntries(Object.entries(dependencies).map(([dep, spec]) =>
        [dep, spec.startsWith("file:") ? `file:${relative(to, resolve(from, spec.slice(5)))}` : spec]))
}
