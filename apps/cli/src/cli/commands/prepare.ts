import { timer } from "@arcforge/arcline"
import type { Prepared, PrepareInput, ProjectKind, TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/**
 * terra prepare [folder] — the nearest project made ready, whatever its kind. Its steps
 * while it works, then one line, and for a server the mods it built.
 */
export function PrepareCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    async function run(args: Args): Promise<number> {
        flags.only(args, [])
        await prepare(opts.terra.projects.find(args.positional[0] ?? "."))
        return 0
    }

    /** Prepare a project, showing it. For any command that needs one prepared. */
    async function prepare<P extends Prepared>(project: { root: string; kind: ProjectKind; prepare(e?: PrepareInput): Promise<P> }): Promise<P> {
        const started = timer.start()
        const surface = opts.presentation.surfaces.prepare(project.root, project.kind)
        try {
            const prepared = await project.prepare({ progress: surface.preparing })
            const mods = prepared.kind === "server" ? prepared.mods : []
            surface.finish({
                title: prepared.title,
                modules: mods.map(m => ({ name: m.name, outcome: "ok" as const, detail: m.version, note: m.prefix })),
                ms: timer.end(started),
            })
            return prepared
        } catch (error) {
            throw surface.fail(error)
        }
    }

    return { run: run, prepare: prepare }
}

export type PrepareCommandT = ReturnType<typeof PrepareCommand>
