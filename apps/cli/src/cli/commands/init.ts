import { timer } from "@arcforge/arcline"
import { basename, join, relative } from "node:path"
import { err } from "../../errors"
import { names, type ProjectKind, type TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/**
 * terra server init <name> · terra mod init <name> [--scope @you] — a new project, made
 * and prepared: installed and typed, ready to run. A bare name makes it in your profile
 * (a mod in its mods/, which the game finds; a server in its servers/); a path (./kilns)
 * makes it there. Ends with the folder it made, and what to run in it. The only command
 * with a noun: there's no folder yet for it to read.
 */
export function InitCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    /** The command for one kind: `terra <kind> init …`. */
    function of(kind: ProjectKind) {
        return async (args: Args): Promise<number> => {
            flags.only(args, ["scope"])
            const [verb, given] = args.positional
            if (verb !== "init") throw err("USAGE", { detail: `terra ${kind} init <name>` })
            if (!given) throw err("USAGE", { detail: `terra ${kind} init <name>: name the ${kind}` })
            const name = names.package(basename(given), flags.value(args, "scope"))
            // a path is where it's asked to be; a bare name is the profile's (home.md)
            const isPath = given.includes("/") || given.includes("\\") || given.startsWith(".")
            const dir = isPath ? given : join(kind === "mod" ? opts.terra.home.mods() : opts.terra.home.servers(), basename(name))
            const started = timer.start()
            const surface = opts.presentation.surfaces.init(kind, name)
            try {
                surface.scaffolding()
                const project = opts.terra.projects.create(kind, { dir: dir, name: name })
                await project.prepare({ progress: surface.preparing })
                surface.finish({
                    files: opts.presentation.tree.read(project.root),
                    root: shown(project.root),
                    next: `cd ${shown(project.root)} && ${kind === "server" ? "terra host" : "terra dev"}`,
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
