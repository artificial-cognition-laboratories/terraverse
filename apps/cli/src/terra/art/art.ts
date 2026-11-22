import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs"
import { join } from "node:path"
import { err } from "../../errors"
import { png, type Raster } from "../../image"
import type { ScratchT } from "../dev/scratch"
import type { ProjectsT } from "../project"
import { names, Packages } from "../project"
import { DryRun, type DryLog } from "./dry"
import { iso } from "./iso"
import { GameBlocks } from "./game"
import { sheet, type SheetEntry } from "./sheet"

/** One thing a mod declared, as art shows it: what it is, and its pictures. */
export type ArtEntry = SheetEntry & {
    readonly description: string
    /** A block's: what it's made of. */
    readonly matter: string | null
    /** The files its pictures are, as the engine is sent them: a block's six faces, an item's icon. */
    readonly files: readonly string[]
}

export type Catalogue = { readonly root: string; readonly entries: readonly ArtEntry[]; readonly logs: readonly DryLog[] }

/**
 * Art — what a project's mods draw, seen without the game: loaded dry (as the sidecar
 * would load them), each block's faces and each item's icon read back from the files art
 * wrote, and drawn as a sheet. A mod alone is shown alone; a server, with all its mods.
 */
export function Art(opts: { projects: ProjectsT; scratch: ScratchT; share: string }) {
    const dry = DryRun({})
    const game = GameBlocks({ share: opts.share })
    const packages = Packages({})

    /**
     * Everything the nearest project at or above `from` declares, `match` in its name or
     * description; after the game's own blocks named in `beside`, to compare with.
     */
    async function catalogue(from: string, e: { match: string | null; beside: readonly string[]; references?: readonly string[] }): Promise<Catalogue> {
        const match = e.match
        const project = opts.projects.find(from)
        const own = project.kind === "mod" ? `${names.prefix((await packages.read(`${project.root}/package.json`)).name)}:` : ""
        const server = await opts.scratch.serverOf(project, { in: null })
        const prepared = await server.prepare()
        const { table, logs } = await dry.load(prepared.manifest)
        const paths = new Map(table.media.map(m => [m.name, m.path]))
        const picture = (name: string): { raster: Raster; path: string } => {
            const path = paths.get(name)
            if (!path) throw err("ASSET_MISSING", { detail: `${name} was named but never sent` })
            return { raster: png.decode(new Uint8Array(readFileSync(path)), path), path: path }
        }
        const entries: ArtEntry[] = [
            ...table.blocks.map(b => {
                const [top, bottom, side] = [b.tiles[0]!, b.tiles[1]!, b.tiles[5]!].map(picture)
                return { type: "block" as const, source: "mod" as const, name: b.name, description: b.description, matter: b.matter, files: b.tiles.map(t => picture(t).path), top: top!.raster, bottom: bottom!.raster, side: side!.raster }
            }),
            ...table.items.map(i => {
                const icon = picture(i.icon)
                return { type: "item" as const, source: "mod" as const, name: i.name, description: i.description, matter: null, files: [icon.path], icon: icon.raster }
            }),
        ].filter(e => e.name.startsWith(own) && (match === null || `${e.name} ${e.description}`.toLowerCase().includes(match.toLowerCase())))
        const reference: ArtEntry[] = game.entries(e.beside).map(g => ({ ...g, description: g.name, matter: null, files: [] }))
        // another mod's things, drawn the same way, to hold these against
        // drawing only items (a tool), the reference's blocks are just in the way
        const onlyItems = entries.length > 0 && entries.every(x => x.type === "item")
        const others: ArtEntry[] = []
        for (const folder of e.references ?? [])
            for (const r of (await catalogue(folder, { match: null, beside: [] })).entries)
                if (!onlyItems || r.type === "item") others.push({ ...r, source: "reference" })
        return { root: project.root, entries: [...reference, ...others, ...entries], logs: logs }
    }

    /** The sheet and each entry alone, written into `out`: their paths. */
    function render(c: Catalogue, out: string): { sheet: string; each: Map<string, string> } {
        if (!c.entries.some(e => e.source === "mod")) throw err("ART_INVALID", { detail: "nothing to draw: no blocks or items declared (or none matched)" })
        mkdirSync(out, { recursive: true })
        const path = join(out, "sheet.png")
        writeFileSync(path, png.encode(sheet.compose(c.entries)))
        const each = new Map<string, string>()
        for (const e of c.entries.filter(x => x.source === "mod")) {
            const file = join(out, `${e.name.replace(":", ".")}.png`)
            writeFileSync(file, png.encode(sheet.single(e)))
            each.set(e.name, file)
        }
        return { sheet: path, each: each }
    }

    /**
     * Each entry's pictures as files in `dir` (made empty first): a block's top, bottom and
     * side and an inventory cube, an item's icon. By entry name, the files' names in `dir`.
     */
    function pictures(c: Catalogue, dir: string): Map<string, { icon: string; faces: { top: string; bottom: string; side: string } | null }> {
        rmSync(dir, { recursive: true, force: true })
        mkdirSync(dir, { recursive: true })
        let n = 0
        const write = (r: Raster) => {
            const file = `${n++}.png`
            writeFileSync(join(dir, file), png.encode(r))
            return file
        }
        const out = new Map<string, { icon: string; faces: { top: string; bottom: string; side: string } | null }>()
        for (const e of c.entries) {
            if (e.type === "item") out.set(e.name, { icon: write(e.icon), faces: null })
            else out.set(e.name, {
                icon: write(iso.cube({ top: e.top, left: e.side, right: e.side }, 2)),
                faces: { top: write(e.top), bottom: write(e.bottom), side: write(e.side) },
            })
        }
        return out
    }

    /** The folder of the nearest project at or above `from`: what to watch. */
    function rootOf(from: string): string {
        return opts.projects.find(from).root
    }

    return { catalogue: catalogue, render: render, pictures: pictures, rootOf: rootOf }
}

export type ArtT = ReturnType<typeof Art>
