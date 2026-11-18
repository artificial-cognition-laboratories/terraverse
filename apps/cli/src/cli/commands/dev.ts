import { errorReport, header, status } from "@arcforge/arcline"
import type { DevEvent, DevOptions, TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/** What's worth showing of the engine's log while developing: the mods, and trouble. */
const SHOWN = /mods:|ERROR|WARNING|listening on/
/** "2026-11-18 17:41:02: ACTION[Main]: " */
const STAMP = /^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d: (\w+)\[\w+\]: /

/**
 * terra dev [folder] [--headless] [--fresh] [--in <server>] [--port n] — the nearest
 * project, run in the game as it's edited: a server as itself, a mod in a scratch universe
 * (or inside a copy of a real server). Every save prepares again and the mods reload.
 */
export function DevCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    const p = opts.presentation
    const r = p.renderer

    async function run(args: Args): Promise<number> {
        flags.only(args, ["headless", "port", "fresh", "in"])
        const headless = flags.on(args, "headless")
        const options: DevOptions = {
            mode: headless ? "server" : "game",
            port: flags.port(args, 30000),
            fresh: flags.on(args, "fresh"),
            in: flags.value(args, "in") ?? null,
        }
        if (headless) p.line(status(r, "info", "Headless", `join localhost:${options.port} from the game`))
        return opts.terra.dev.run(args.positional[0] ?? ".", { options: options, report: show })
    }

    function show(event: DevEvent): void {
        switch (event.kind) {
            case "prepared":
                if (event.first) {
                    p.line()
                    p.line(header(r, { title: "Developing", subtitle: event.prepared.title }))
                    p.line()
                }
                p.line(status(r, "ok", event.first ? "Prepared" : "Rebuilt", `${plural(event.prepared.mods.length, "mod")} · ${event.ms}ms`))
                return
            case "failed":
                for (const line of errorReport(r, event.error)) p.line(line)
                return
            case "waiting":
                p.line(status(r, "pending", "Waiting for a save"))
                return
            case "restarting":
                p.line(status(r, "info", "Restarting the universe", {
                    content: "the mods' content changed",
                    config: "a config changed",
                }[event.why]))
                return
            case "closed":
                p.line(status(r, "ok", "Closed"))
                return
            case "stopped":
                p.line(status(r, "fail", "The server stopped", `exit ${event.code}: fix it and save to start it again`))
                return
            case "engine":
                engine(event.line)
                return
        }
    }

    function engine(line: string): void {
        if (!SHOWN.test(line)) return
        const stamp = STAMP.exec(line)
        const level = stamp?.[1] ?? ""
        const text = stamp ? line.slice(stamp[0].length) : line
        if (level === "ERROR") p.line(r.c.error(text))
        else if (level === "WARNING") p.line(r.c.warn(text))
        else p.line(r.c.dim(text))
    }

    return { run: run }
}

const plural = (n: number, word: string) => `${n} ${word}${n === 1 ? "" : "s"}`
