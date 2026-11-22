import { status } from "@arcforge/arcline"
import { join, relative } from "node:path"
import { err } from "../../errors"
import type { ArtEntry, TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/**
 * terra art render [folder] [--out dir] [--match text] [--with stone,dirt] [--json] — every
 * block and item the nearest project's mods declare, drawn without the game: a sheet of them
 * all (after the game's own blocks named by --with, and every block and item of the mods
 * in the folders --beside names, to compare), and each alone. terra art list [folder]
 * [--match text] [--json] [--pictures dir] — the same as a list; --pictures writes each
 * one's faces and icon as files there, named in the JSON. terra art list [folder] [--match text] [--json] — the same, as a list.
 */
export function ArtCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    const p = opts.presentation

    async function run(args: Args): Promise<number> {
        const [verb, folder] = args.positional
        if (verb === "render") return render(args, folder ?? ".")
        if (verb === "list") return list(args, folder ?? ".")
        throw err("USAGE", { detail: "terra art render [folder] [--out dir] [--match text] · terra art list [folder] [--match text]" })
    }

    async function render(args: Args, folder: string): Promise<number> {
        flags.only(args, ["out", "match", "with", "beside", "json"])
        const list = (name: string) => (flags.value(args, name) ?? "").split(",").map(n => n.trim()).filter(n => n !== "")
        const c = await opts.terra.art.catalogue(folder, { match: flags.value(args, "match") ?? null, beside: list("with"), references: list("beside") })
        const out = flags.value(args, "out") ?? join(c.root, ".terra", "art")
        const drawn = opts.terra.art.render(c, out)
        if (p.mode.machine) {
            p.emit(JSON.stringify({ sheet: drawn.sheet, entries: c.entries.filter(e => e.source === "mod").map(e => ({ ...row(e), image: drawn.each.get(e.name) })), logs: c.logs }))
            return 0
        }
        for (const l of c.logs) p.line(`  ${p.renderer.c.dim(`${l.mod}:`)} ${l.text}`)
        p.line(status(p.renderer, "ok", `${c.entries.filter(e => e.source === "mod").length} drawn`, relative(process.cwd(), drawn.sheet) || drawn.sheet))
        for (const e of c.entries.filter(e => e.source === "mod")) p.line(`  ${e.name.padEnd(28)} ${p.renderer.c.dim(relative(process.cwd(), drawn.each.get(e.name) ?? ""))}`)
        return 0
    }

    async function list(args: Args, folder: string): Promise<number> {
        flags.only(args, ["match", "json", "pictures"])
        const c = await opts.terra.art.catalogue(folder, { match: flags.value(args, "match") ?? null, beside: [] })
        // --pictures <dir>: every face and icon written there, for something else to show (the game's Contents)
        const into = flags.value(args, "pictures")
        const pictures = into ? opts.terra.art.pictures(c, into) : null
        if (p.mode.machine) {
            p.emit(JSON.stringify({ entries: c.entries.map(e => ({ ...row(e), ...(pictures ? { pictures: pictures.get(e.name) } : {}) })) }))
            return 0
        }
        if (c.entries.length === 0) p.line(p.renderer.c.dim("  nothing declared (or nothing matched)"))
        for (const e of c.entries)
            p.line(`  ${e.name.padEnd(28)} ${e.type.padEnd(6)} ${e.description}${e.matter ? p.renderer.c.dim(` · ${e.matter}`) : ""}`)
        return 0
    }

    return { run: run }
}

/** An entry as JSON: what it is and where its pictures are, without the pixels. */
function row(e: ArtEntry) {
    return { name: e.name, type: e.type, description: e.description, matter: e.matter, files: e.files }
}
