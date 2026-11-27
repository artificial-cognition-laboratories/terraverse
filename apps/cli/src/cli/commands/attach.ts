import { err } from "../../errors"
import { refs, sources, WorldContract, type TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/**
 * terra attach <terra://localhost:port> --world-contract — speak the world contract (axon.world/0)
 * for a world already running on this machine: one the game opened, or another terra's. Bodies
 * are placed and removed as `terra serve` does; closing takes them out and leaves the world up,
 * and it ends when the world does. axond's Terraverse lab runs it to attach (sprint 127).
 */
export function AttachCommand(opts: { terra: TerraT; presentation: PresentationT; version: string }) {
    async function run(args: Args): Promise<number> {
        flags.only(args, ["world-contract"])
        if (!flags.on(args, "world-contract"))
            throw err("USAGE", { detail: "terra attach speaks only the world contract: terra attach <terra://localhost:port> --world-contract" })
        const [said, ...extra] = args.positional
        if (said === undefined || extra.length > 0) throw err("USAGE", { detail: "terra attach <terra://localhost:port> --world-contract" })
        const ref = refs.parse(said)
        if (ref?.kind !== "address") throw err("USAGE", { detail: `${said} isn't a world's address (terra://localhost:port)` })
        if (!["localhost", "127.0.0.1", "::1"].includes(ref.host))
            throw err("NOT_WIRED", { detail: `${said} is on another machine; only worlds on this one can be attached to` })
        const record = opts.terra.running.at(ref.port)
        if (!record) throw err("NOTHING_RUNNING_THERE", { detail: said })
        const log = (line: string) => opts.presentation.diagnostics.line(line)
        log(`attached to ${record.title} (${record.ref}, ${record.by === "game" ? "opened in the game" : "terra's"})`)
        return await WorldContract({
            engine: opts.terra.engine,
            // its record still there, still its: the world is up (a game that left it lives on)
            world: sources.attached({ record: record, still: () => opts.terra.running.at(record.port)?.pid === record.pid }),
            version: opts.version,
            asked: Bun.stdin.stream(),
            say: opts.presentation.emit,
            log: log,
        }).run()
    }

    return { run: run }
}
