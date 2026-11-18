import type { RendererHandle } from "@arcforge/arcline"
import { Terra } from "../terra"
import { parseArgs } from "./args"
import { Boundary } from "./boundary"
import { Commands } from "./commands"
import { Presentation } from "./presentation"
import pkg from "../../package.json"

type CliOpts = {
    /** Where terra is: process.execPath. The engine is beside it, and it is bun. */
    exec: string
    /** Where output goes. A test passes arcline's Renderer.capture(). */
    renderer?: RendererHandle
}

/**
 * Cli — the terra command. Presentation only: arguments in, output and an exit code out.
 * The work is Terra's, and the sidecar is Runtime's.
 */
export function Cli(opts: CliOpts) {
    const terra = Terra({ exec: opts.exec })
    const presentation = Presentation({ renderer: opts.renderer })
    const boundary = Boundary({ presentation: presentation })
    const commands = Commands({ terra: terra, presentation: presentation, version: pkg.version })

    /** Run argv: the exit code. */
    function dispatch(argv: readonly string[]): Promise<number> {
        return boundary.run(() => {
            const args = parseArgs(argv)
            return commands.find(args)(args)
        })
    }

    return {
        dispatch: dispatch,
        terra: terra,
    }
}

export type CliT = ReturnType<typeof Cli>
