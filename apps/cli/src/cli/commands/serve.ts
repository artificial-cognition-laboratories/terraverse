import { status } from "@arcforge/arcline"
import { Exit } from "../../runtime"
import { refs, type TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"
import type { PrepareCommandT } from "./prepare"

/** terra serve [folder] [--port n] — prepare the nearest server, then run it until it stops. */
export function ServeCommand(opts: { terra: TerraT; presentation: PresentationT; prepare: PrepareCommandT }) {
    async function run(args: Args): Promise<number> {
        flags.only(args, ["port"])
        const port = flags.port(args, 30000)
        const project = opts.terra.projects.server(args.positional[0] ?? ".")
        for (;;) {
            const prepared = await opts.prepare.prepare(project)
            const ref = refs.print({ kind: "address", host: "localhost", port: port })
            opts.presentation.line(status(opts.presentation.renderer, "ok", prepared.title, `Join ${ref} · in the game's Servers list`))
            opts.presentation.line()
            const code = await opts.terra.engine.run({ mode: "server", server: prepared, world: project.server.worlds.folder, port: port })
            // its mods were prepared again with new content: it stopped to be started again
            if (code !== Exit.restart) return code
            opts.presentation.line(status(opts.presentation.renderer, "info", "Restarting the universe", "the mods' content changed"))
        }
    }

    return { run: run }
}
