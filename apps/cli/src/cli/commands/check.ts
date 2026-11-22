import { status, timer } from "@arcforge/arcline"
import type { TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/**
 * terra check [folder] [--json] — the nearest project made sure of without the game: its
 * types, then its mods loaded dry. Fails with what's wrong and where; passes with what it declares.
 */
export function CheckCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    const p = opts.presentation

    async function run(args: Args): Promise<number> {
        flags.only(args, ["json"])
        const started = timer.start()
        const checked = await opts.terra.check.run(args.positional[0] ?? ".")
        if (p.mode.machine) {
            p.emit(JSON.stringify({ ok: true, ...checked }))
            return 0
        }
        for (const l of checked.logs) p.line(`  ${p.renderer.c.dim(l)}`)
        p.line(status(p.renderer, "ok", "Checked", `${checked.blocks} blocks, ${checked.items} items · types and load, ${timer.end(started)} ms`))
        return 0
    }

    return { run: run }
}
