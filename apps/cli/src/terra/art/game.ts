import { existsSync, readFileSync } from "node:fs"
import { err } from "../../errors"
import { png } from "../../image"
import type { SheetEntry } from "./sheet"

/**
 * GameBlocks — the game's own blocks, as the engine ships them (world/blocks.json and the
 * Good Vibes textures beside it), for a sheet to set a mod's art against: what "matches
 * the game" means, there to look at.
 */
export function GameBlocks(opts: { share: string }) {
    /** These blocks of the game's, by name ("stone" or "core:stone"), drawn as a sheet draws a mod's. */
    function entries(names: readonly string[]): SheetEntry[] {
        const defs = read()
        return names.map(given => {
            const name = given.includes(":") ? given : `core:${given}`
            const def = defs[name]
            if (!def) throw err("BLOCK_UNKNOWN", { detail: `${given}: the game has ${Object.keys(defs).filter(drawable(defs)).map(n => n.replace("core:", "")).join(", ")}` })
            const [top, bottom, side] = faces(def.tiles).map(t => picture(name, t))
            return { type: "block", source: "game", name: name, top: top!, bottom: bottom!, side: side! }
        })
    }

    function read(): Record<string, { tiles: string[] }> {
        const path = `${opts.share}/world/blocks.json`
        if (!existsSync(path)) throw err("ENGINE_MISSING", { detail: `the game's blocks aren't at ${path}` })
        const blocks: unknown = JSON.parse(readFileSync(path, "utf8")).blocks
        if (typeof blocks !== "object" || blocks === null) throw err("ENGINE_MISSING", { detail: `${path} has no blocks` })
        const out: Record<string, { tiles: string[] }> = {}
        for (const [name, def] of Object.entries(blocks)) {
            const tiles: unknown = typeof def === "object" && def !== null ? Reflect.get(def, "tiles") : undefined
            if (Array.isArray(tiles) && tiles.every(t => typeof t === "string")) out[name] = { tiles: tiles }
        }
        return out
    }

    function picture(block: string, tile: string) {
        // the base texture of a modifier chain ("a.png^[colorize…" → a.png): near enough to compare by
        const file = tile.split("^")[0]!
        const path = `${opts.share}/textures/base/pack/${file}`
        if (!existsSync(path)) throw err("ASSET_MISSING", { detail: `${block}'s ${file} isn't in the game's textures` })
        return png.decode(new Uint8Array(readFileSync(path)), path)
    }

    /** Which of the game's blocks have pictures to compare with. */
    function drawable(defs: Record<string, { tiles: string[] }>) {
        return (name: string) => defs[name]!.tiles.length > 0
    }

    return { entries: entries }
}

export type GameBlocksT = ReturnType<typeof GameBlocks>

/** A definition's tiles, as Luanti reads them, into top, bottom and the side players face first (-Z). */
function faces(tiles: readonly string[]): [string, string, string] {
    const at = (i: number) => tiles[Math.min(i, tiles.length - 1)]!
    return [at(0), at(1), tiles.length >= 6 ? at(5) : at(2)]
}
