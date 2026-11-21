import { timer } from "@arcforge/arcline"
import { basename } from "node:path"
import { err } from "../../errors"
import { names, type ProjectKind, type TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/**
 * terra server init <name> · terra mod init <name> [--scope @you] — a new project, made
 * and prepared: installed and typed, ready to run. Ends with the folder it made, and what
 * to run in it. The only command with a noun: there's no folder yet for it to read.
 */
export function InitCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    /** The command for one kind: `terra <kind> init …`. */
    function of(kind: ProjectKind) {
        return async (args: Args): Promise<number> => {
            flags.only(args, ["scope"])
            const [verb, given] = args.positional
            if (verb !== "init") throw err("USAGE", { detail: `terra ${kind} init <name>` })
            if (!given) throw err("USAGE", { detail: `terra ${kind} init <name>: name the ${kind}` })
            const name = names.package(given, flags.value(args, "scope"))
            const dir = basename(name)
            const started = timer.start()
            const surface = opts.presentation.surfaces.init(kind, name)
            try {
                surface.scaffolding()
                const project = opts.terra.projects.create(kind, { dir: dir, name: name })
                await project.prepare({ progress: surface.preparing })
                surface.finish({
                    files: opts.presentation.tree.read(project.root),
                    root: `./${dir}`,
                    next: `cd ${dir} && ${kind === "server" ? "terra host" : "terra dev"}`,
                    ms: timer.end(started),
                })
                return 0
            } catch (error) {
                throw surface.fail(error)
            }
        }
    }

    return { of: of }
}
