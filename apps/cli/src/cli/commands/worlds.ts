import { status } from "@arcforge/arcline"
import type { TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/**
 * terra worlds [--json] — the worlds the active profile has: the game's Play list. Each runs as a
 * server with `terra serve --world <name>`; --json is how axond's Terraverse lab lists them.
 */
export function WorldsCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    async function run(args: Args): Promise<number> {
        flags.only(args, ["json"])
        const worlds = opts.terra.worlds.list()
        if (opts.presentation.mode.machine) {
            opts.presentation.emit(JSON.stringify({
                profile: opts.terra.home.active(),
                worlds: worlds.map(w => ({ name: w.name, title: w.title, path: w.path, server: w.server })),
            }))
            return 0
        }
        if (worlds.length === 0) {
            opts.presentation.line(status(opts.presentation.renderer, "info", "No worlds yet", `${opts.terra.home.active()} has none: make one in the game`))
            return 0
        }
        for (const w of worlds)
            opts.presentation.line(status(opts.presentation.renderer, "ok", w.title, w.server ? "its own mods" : "the game's mods"))
        opts.presentation.line()
        opts.presentation.line(`  serve one:  terra serve --world "<name>"`)
        return 0
    }

    return { run: run }
}
