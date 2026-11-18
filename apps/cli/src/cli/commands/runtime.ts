import { err } from "../../errors"
import { Runtime, Wire } from "../../runtime"
import type { TerraT } from "../../terra"
import type { Args } from "../args"

/** The engine's fd for the sidecar's socket. */
const FD = 3

/**
 * terra runtime <folder> — the sidecar. Started by the engine, never by hand: it loads
 * the folder's prepared mods and serves the engine's windows until told to stop.
 */
export function RuntimeCommand(opts: { terra: TerraT }) {
    async function run(args: Args): Promise<number> {
        const folder = args.positional[0]
        if (!folder) throw err("USAGE", { detail: "terra runtime <server folder>" })
        const manifest = await opts.terra.projects.server(folder).server.manifest.read()
        const runtime = Runtime({ manifest: manifest, wire: Wire({ fd: FD }) })
        await runtime.run()
        return 0
    }

    return { run: run }
}
