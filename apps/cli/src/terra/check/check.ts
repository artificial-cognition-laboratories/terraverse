import { existsSync, readdirSync } from "node:fs"
import { err } from "../../errors"
import type { ArtT } from "../art"
import type { ProjectsT } from "../project"

/** What the TypeScript compiler is pinned to: the one terra's own types are checked by. */
const TYPESCRIPT = "typescript@7.0.2"

/** A mod checked: what it declares, and what it logged while it loaded. */
export type Checked = { readonly root: string; readonly blocks: number; readonly items: number; readonly logs: readonly string[] }

/**
 * Check — a project made sure of without the game: prepared (so its types are current),
 * typechecked against the mod API, then loaded dry, as the sidecar would, so everything the
 * API refuses at load (a name, a texture, a grid) is refused here. Seconds, not a world.
 */
export function Check(opts: { projects: ProjectsT; art: ArtT; bun: string }) {
    async function run(from: string): Promise<Checked> {
        const project = opts.projects.find(from)
        await project.prepare()
        await types(project.root)
        const c = await opts.art.catalogue(from, { match: null, beside: [] })
        return {
            root: project.root,
            blocks: c.entries.filter(e => e.type === "block").length,
            items: c.entries.filter(e => e.type === "item").length,
            logs: c.logs.map(l => `${l.mod}: ${l.text}`),
        }
    }

    /** Each half with code (server/, client/), typechecked. TYPES_FAILED lists every mistake. */
    async function types(root: string): Promise<void> {
        const halves = ["server", "client"].filter(h => existsSync(`${root}/${h}/tsconfig.json`) && hasCode(`${root}/${h}`))
        const said: string[] = []
        for (const half of halves) {
            const proc = Bun.spawn([opts.bun, "x", "-p", TYPESCRIPT, "tsc", "--noEmit", "--pretty", "false", "-p", half], {
                cwd: root,
                env: { ...process.env, BUN_BE_BUN: "1" },
                stdout: "pipe",
                stderr: "pipe",
            })
            const [code, out, error] = await Promise.all([proc.exited, new Response(proc.stdout).text(), new Response(proc.stderr).text()])
            if (code !== 0) said.push(...(out.trim() || error.trim()).split("\n").filter(l => l.trim() !== ""))
        }
        if (said.length > 0) throw err("TYPES_FAILED", { detail: said.join("\n") })
    }

    return { run: run }
}

export type CheckT = ReturnType<typeof Check>

/** Whether a folder has a .ts file in it, at any depth. */
function hasCode(dir: string): boolean {
    return readdirSync(dir, { recursive: true, encoding: "utf8" }).some(f => f.endsWith(".ts") && !f.includes("node_modules"))
}
