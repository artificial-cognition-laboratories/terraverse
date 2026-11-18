import { existsSync, mkdirSync, readdirSync, writeFileSync } from "node:fs"
import { dirname, resolve } from "node:path"
import { err } from "../../errors"
import { KINDS, detectKind, type ProjectKind } from "./kinds"
import { freshSystem } from "./create/system"
import { names } from "./names"
import { Project, type ProjectT } from "./project"
import type { ServerT } from "./server"
import type { PrepareInput, PreparedServer } from "./types"

/** A server project: its extension there for the reading, and prepare's result a server's. */
export type ServerProjectT = Omit<ProjectT, "kind" | "server" | "prepare"> & {
    readonly kind: "server"
    readonly server: ServerT
    prepare(e?: PrepareInput): Promise<PreparedServer>
}

/** Projects — finds and opens projects of every kind: which folder, and what kind it is. */
export function Projects(opts: { bun: string }) {
    const opened = new Map<string, ProjectT>()

    /** The project at `root`. */
    function open(root: string): ProjectT {
        const absolute = resolve(root)
        const kind = detectKind(absolute)
        if (!kind) throw err("PROJECT_NOT_FOUND", { detail: `${absolute} has no package.json and project config` })
        const existing = opened.get(absolute)
        if (existing) return existing
        const project = Project({ root: absolute, kind: kind, bun: opts.bun })
        opened.set(absolute, project)
        return project
    }

    /** The nearest project at or above `from`. */
    function find(from: string): ProjectT {
        const start = resolve(from)
        for (let dir = start; ; dir = dirname(dir)) {
            if (detectKind(dir)) return open(dir)
            if (dirname(dir) === dir) throw err("PROJECT_NOT_FOUND", { detail: `nothing at or above ${start}` })
        }
    }

    /** The nearest server at or above `from`, refusing any other kind. */
    function server(from: string): ServerProjectT {
        const project = find(from)
        if (project.kind !== "server" || !project.server)
            throw err("PROJECT_WRONG_KIND", { detail: `${project.root} is a ${project.kind}, not a server (no ${KINDS.server.config})` })
        return {
            ...project,
            kind: "server",
            server: project.server,
            async prepare(e?: PrepareInput): Promise<PreparedServer> {
                const prepared = await project.prepare(e)
                if (prepared.kind !== "server") throw err("PROJECT_WRONG_KIND", { detail: `${project.root} prepared as a ${prepared.kind}` })
                return prepared
            },
        }
    }

    /**
     * A new project of `kind` in `dir`, named `name` (its package name), opened. Refuses a
     * folder that exists and isn't empty: init never writes over anyone's files.
     */
    function create(kind: ProjectKind, e: { dir: string; name: string }): ProjectT {
        const root = resolve(e.dir)
        if (existsSync(root) && readdirSync(root).length > 0)
            throw err("PROJECT_EXISTS", { detail: `${root} isn't empty` })
        const scaffold = KINDS[kind].scaffold({ name: e.name, title: names.title(e.name), system: freshSystem() })
        for (const dir of scaffold.dirs) mkdirSync(`${root}/${dir}`, { recursive: true })
        for (const [path, text] of Object.entries(scaffold.files)) {
            mkdirSync(dirname(`${root}/${path}`), { recursive: true })
            writeFileSync(`${root}/${path}`, text)
        }
        return open(root)
    }

    return { open: open, find: find, server: server, create: create }
}

export type ProjectsT = ReturnType<typeof Projects>

export type { ProjectKind }
