import { openSync } from "node:fs"
import { err } from "../../errors"
import { Runtime, Wire } from "../../runtime"
import type { TerraT } from "../../terra"
import { flags, type Args } from "../args"

/** The engine's fd for the sidecar's socket, on Linux and macOS. */
const FD = 3

/**
 * terra runtime <folder> [--pipe <name>] — the sidecar. Started by the engine, never by
 * hand: it loads the folder's prepared mods and serves the engine's windows until told to
 * stop. The engine hands it a socket as fd 3, or on Windows names a pipe it opens.
 */
export function RuntimeCommand(opts: { terra: TerraT }) {
    async function run(args: Args): Promise<number> {
        flags.only(args, ["pipe"])
        const folder = args.positional[0]
        if (!folder) throw err("USAGE", { detail: "terra runtime <server folder> [--pipe <name>]" })
        const pipe = flags.value(args, "pipe")
        // the engine's end is waiting for us: open it before loading, so a slow load isn't
        // mistaken for one that never came
        const fd = pipe === undefined ? FD : openSync(pipe, "r+")
        const manifest = await opts.terra.projects.server(folder).server.manifest.read()
        const runtime = Runtime({ manifest: manifest, wire: Wire({ fd: fd }) })
        await runtime.run()
        return 0
    }

    return { run: run }
}
