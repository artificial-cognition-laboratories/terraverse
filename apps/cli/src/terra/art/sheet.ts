import { raster, type Raster, type Rgba } from "../../image"
import { font } from "./font"
import { iso } from "./iso"

/** Whose it is: the mod being drawn, the game's own, or another mod's held up as a reference. */
export type Source = "mod" | "game" | "reference"

/** One thing on the sheet: a block's faces or an item's icon; a mod's, or the game's own to compare with. */
export type SheetEntry =
    | { readonly type: "block"; readonly source: Source; readonly name: string; readonly top: Raster; readonly bottom: Raster; readonly side: Raster }
    | { readonly type: "item"; readonly source: Source; readonly name: string; readonly icon: Raster }

const CELL = { width: 380, height: 260, pad: 12 }
const INK: Rgba = [214, 220, 228, 255]
const DIM: Rgba = [120, 128, 140, 255]
const GAME: Rgba = [0, 148, 210, 255]
const CHECK: readonly [Rgba, Rgba] = [[38, 41, 48, 255], [46, 50, 58, 255]]

/**
 * sheet — everything a mod draws, side by side on one picture: each block as a cube, its
 * faces flat, and its top and side tiled 3 × 3 as the world repeats them (where seams and
 * banding show); each item large. The game's own blocks and another mod's things, asked
 * for, come first, marked, to hold the rest against.
 */
export const sheet = {
    compose(entries: readonly SheetEntry[]): Raster {
        const columns = Math.max(1, Math.min(4, entries.length))
        const rows = Math.max(1, Math.ceil(entries.length / columns))
        const out = raster.blank(columns * CELL.width, rows * CELL.height)
        raster.rect(out, 0, 0, out.width, out.height, [24, 26, 31, 255])
        entries.forEach((e, i) => cell(out, e, (i % columns) * CELL.width, Math.floor(i / columns) * CELL.height))
        return out
    },

    /** One entry alone, large: a cube at 8 or an icon at 16 to a texel. */
    single(e: SheetEntry): Raster {
        if (e.type === "block") return iso.cube({ top: e.top, left: e.side, right: e.side }, Math.max(1, Math.round(128 / e.top.width)))
        const out = raster.blank(e.icon.width * 16, e.icon.height * 16)
        raster.blit(out, e.icon, 0, 0, 16)
        return out
    },
}

function cell(out: Raster, e: SheetEntry, x: number, y: number): void {
    const p = CELL.pad
    font.draw(out, e.name, x + p, y + p, 2, e.source === "mod" ? INK : GAME)
    if (e.source !== "mod") font.draw(out, e.source === "game" ? "the game's" : "reference", x + p + font.width(e.name, 2) + 8, y + p + 4, 1, DIM)
    const top = y + p + 18
    if (e.type === "block") {
        const n = e.top.width
        const k = Math.max(1, Math.round(64 / n))
        checker(out, x + p, top, 2 * n * k, 2 * n * k)
        raster.blit(out, iso.cube({ top: e.top, left: e.side, right: e.side }, k), x + p, top)
        const right = x + p + 2 * n * k + 16
        const s = Math.max(1, Math.round(48 / n))
        const flat: [string, Raster][] = [["top", e.top], ["side", e.side], ["bottom", e.bottom]]
        flat.forEach(([label, face], j) => {
            const fx = right + j * (n * s + 8)
            checker(out, fx, top, n * s, n * s)
            raster.blit(out, face, fx, top, s)
            font.draw(out, label, fx, top + n * s + 5, 1, DIM)
        })
        // as the world repeats them: a seam, a band or a blob that lines up shows here
        const t = Math.max(1, Math.round(32 / n))
        const tiled: [string, Raster][] = [["top tiled", e.top], ["side tiled", e.side]]
        tiled.forEach(([label, face], j) => {
            const tx = right + j * (3 * n * t + 12), ty = top + n * s + 20
            for (let r = 0; r < 3; r++) for (let c = 0; c < 3; c++) raster.blit(out, face, tx + c * n * t, ty + r * n * t, t)
            font.draw(out, label, tx, ty + 3 * n * t + 5, 1, DIM)
        })
    } else {
        const s = Math.max(1, Math.round(128 / e.icon.width))
        checker(out, x + p, top, e.icon.width * s, e.icon.height * s)
        raster.blit(out, e.icon, x + p, top, s)
        // as small as the hotbar draws it: if it doesn't read here, it doesn't read
        const fx = x + p + e.icon.width * s + 16
        for (const [i, scale] of [1, 2].entries()) {
            const sx = fx + i * (e.icon.width + 12)
            checker(out, sx, top, e.icon.width * scale, e.icon.height * scale)
            raster.blit(out, e.icon, sx, top, scale)
        }
        font.draw(out, "1x 2x", fx, top + e.icon.height * 2 + 6, 1, DIM)
    }
}

/** The clear behind a picture, as image editors show it. */
function checker(out: Raster, x: number, y: number, w: number, h: number): void {
    for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) raster.put(out, x + i, y + j, CHECK[(Math.floor(i / 8) + Math.floor(j / 8)) % 2]!)
}
