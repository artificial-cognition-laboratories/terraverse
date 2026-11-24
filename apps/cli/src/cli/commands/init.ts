import { timer } from "@arcforge/arcline"
import { basename, relative, resolve } from "node:path"
import { err } from "../../errors"
import { names, type ProjectKind, type TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/**
 * terra server init <name> · terra mod init <name> [--scope @you] — a new project, made
 * and prepared: installed and typed, ready to run, in a folder of that name where you
 * are (or at the path given). Ends with the folder it made, and what to run in it. The only command
 * with a noun: there's no folder yet for it to read.
 */
export function InitCommand(opts: { terra: TerraT; presentation: PresentationT; as: string }) {
    /** The command for one kind: `terra <kind> init …`. */
    function of(kind: ProjectKind) {
        return async (args: Args): Promise<number> => {
            flags.only(args, ["scope"])
            const [verb, given] = args.positional
            if (verb !== "init") throw err("USAGE", { detail: `terra ${kind} init <name>` })
            if (!given) throw err("USAGE", { detail: `terra ${kind} init <name>: name the ${kind}` })
            const name = names.package(basename(given), flags.value(args, "scope"))
            // a bare name is a folder where you are, as every other CLI makes one
            const isPath = given.includes("/") || given.includes("\\") || given.startsWith(".")
            const dir = resolve(isPath ? given : basename(name))
            const started = timer.start()
            const surface = opts.presentation.surfaces.init(kind, name)
            try {
                surface.scaffolding()
                const project = opts.terra.projects.create(kind, { dir: dir, name: name })
                await project.prepare({ progress: surface.preparing })
                surface.finish({
                    files: opts.presentation.tree.read(project.root),
                    root: shown(project.root),
                    next: `cd ${shown(project.root)} && ${opts.as} ${kind === "server" ? "serve" : "dev"}`,
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

/** A folder as the person would type it: relative when it's under where they are, else whole. */
function shown(path: string): string {
    const rel = relative(process.cwd(), path)
    return rel && !rel.startsWith("..") ? `./${rel}` : path
}
