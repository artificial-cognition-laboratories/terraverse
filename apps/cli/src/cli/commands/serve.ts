import { status } from "@arcforge/arcline"
import { Exit } from "../../runtime"
import { err } from "../../errors"
import { refs, sources, WorldContract, type EngineUniverse, type TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"
import type { PrepareCommandT } from "./prepare"

/**
 * terra serve [folder] [--world <name>] [--port n] [--world-contract] — run a server until it
 * stops: the nearest server folder, prepared, or with --world one of the player's worlds (the
 * game's Play list, `terra worlds`) as the game runs it. With --world-contract it's run for
 * axond: stdout carries the world contract (axon.world/0) and nothing else, and what a person
 * reads goes to stderr.
 */
export function ServeCommand(opts: { terra: TerraT; presentation: PresentationT; prepare: PrepareCommandT; version: string }) {
    /** What it serves, prepared (quietly for a program), and what it's called. */
    async function served(args: Args, quiet: boolean): Promise<{ universe: Exclude<EngineUniverse, { code: string }>; title: string }> {
        const named = flags.value(args, "world")
        if (named !== undefined) {
            if (args.positional.length > 0) throw err("USAGE", { detail: "terra serve takes a server folder or --world, not both" })
            const saved = opts.terra.worlds.find(named)
            const own = saved.server ? opts.terra.projects.server(saved.server) : null
            const server = own ? (quiet ? await own.prepare() : await opts.prepare.prepare(own)) : null
            return { universe: { saved: saved, server: server }, title: saved.title }
        }
        const project = opts.terra.projects.server(args.positional[0] ?? ".")
        const prepared = quiet ? await project.prepare() : await opts.prepare.prepare(project)
        return { universe: { server: prepared, world: project.server.worlds.folder }, title: prepared.title }
    }

    async function run(args: Args): Promise<number> {
        flags.only(args, ["port", "world", "world-contract"])
        const port = flags.port(args, 30000)
        if (flags.on(args, "world-contract")) {
            // prepared once, quietly: a restart for new content is the runner's to ask for
            const { universe, title } = await served(args, true)
            const log = (line: string) => opts.presentation.diagnostics.line(line)
            return await WorldContract({
                engine: opts.terra.engine,
                world: sources.served({ engine: opts.terra.engine, universe: universe, title: title, port: port, log: log }),
                version: opts.version,
                asked: Bun.stdin.stream(),
                say: opts.presentation.emit,
                log: log,
            }).run()
        }
        for (;;) {
            const { universe, title } = await served(args, false)
            const ref = refs.print({ kind: "address", host: "localhost", port: port })
            opts.presentation.line(status(opts.presentation.renderer, "ok", title, `Join ${ref} · in the game's Servers list`))
            opts.presentation.line()
            const code = await opts.terra.engine.run({ mode: "server", ...universe, port: port })
            // its mods were prepared again with new content: it stopped to be started again
            if (code !== Exit.restart) return code
            opts.presentation.line(status(opts.presentation.renderer, "info", "Restarting the universe", "the mods' content changed"))
        }
    }

    return { run: run }
}
