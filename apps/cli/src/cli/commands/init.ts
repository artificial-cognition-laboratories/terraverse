import { next } from "@arcforge/arcline"
import { basename } from "node:path"
import { err } from "../../errors"
import { names, type ProjectKind, type TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"
import type { PrepareCommandT } from "./prepare"

/**
 * terra server init <name> · terra mod init <name> [--scope @you] — a new project, made
 * and prepared: installed and typed, ready to run. The only command with a noun: there's
 * no folder yet for it to read.
 */
export function InitCommand(opts: { terra: TerraT; presentation: PresentationT; prepare: PrepareCommandT }) {
    /** The command for one kind: `terra <kind> init …`. */
    function of(kind: ProjectKind) {
        return async (args: Args): Promise<number> => {
            flags.only(args, ["scope"])
            const [verb, given] = args.positional
            if (verb !== "init") throw err("USAGE", { detail: `terra ${kind} init <name>` })
            if (!given) throw err("USAGE", { detail: `terra ${kind} init <name>: name the ${kind}` })
            const name = names.package(given, flags.value(args, "scope"))
            const dir = basename(name)
            const project = opts.terra.projects.create(kind, { dir: dir, name: name })
            await opts.prepare.prepare(project)
            opts.presentation.line()
            opts.presentation.line(next(opts.presentation.renderer, `cd ${dir} && ${kind === "server" ? "terra host" : "terra dev"}`))
            return 0
        }
    }

    return { of: of }
}
