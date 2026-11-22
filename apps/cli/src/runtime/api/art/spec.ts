import { err } from "../../../errors"
import type { Rgba } from "../../../image"

/** One pass of paint, checked: colours parsed, grids measured. */
export type Layer =
    | { readonly fill: Rgba }
    | { readonly noise: readonly Rgba[]; readonly scale: number; readonly seed: number }
    | { readonly grain: readonly Rgba[]; readonly along: "x" | "y"; readonly seed: number }
    | { readonly speck: Rgba; readonly density: number; readonly seed: number }
    | { readonly rows: readonly (Rgba | null)[][] }
    | { readonly border: Rgba; readonly width: number }
    | { readonly bevel: number }
    | { readonly outline: Rgba }

/** A texture as art declares it, checked: what paint draws. */
export type Spec = { readonly size: number; readonly layers: readonly Layer[] }

const SIZES = [16, 32]
const CLEAR = new Set([".", " "])
const KEYS = ["fill", "noise", "grain", "speck", "rows", "border", "bevel", "outline"] as const

/** spec — art.texture's argument, read at the seam into a Spec, or ART_INVALID saying where. */
export const spec = {
    read(def: unknown): Spec {
        if (!isObject(def)) throw invalid("art.texture takes { size?, palette?, layers?, rows? }")
        const size = def.size ?? 16
        if (typeof size !== "number" || !SIZES.includes(size)) throw invalid(`size is 16 or 32, not ${JSON.stringify(size)}`)
        const palette = readPalette(def.palette)
        const layers: unknown = def.layers ?? []
        if (!Array.isArray(layers)) throw invalid("layers is a list")
        const read = layers.map((l, i) => readLayer(l, `layers[${i}]`, size, palette))
        if (def.rows !== undefined) read.push({ rows: readRows(def.rows, "rows", size, palette) })
        if (read.length === 0) throw invalid("a texture needs layers or rows: nothing would be drawn")
        return { size: size, layers: read }
    },
}

function readLayer(l: unknown, at: string, size: number, palette: Palette): Layer {
    if (!isObject(l)) throw invalid(`${at} is an object, one of { ${KEYS.join(" | ")} }`)
    const key = KEYS.find(k => k in l)
    if (!key) throw invalid(`${at} has none of ${KEYS.join(", ")}`)
    const seed = readNumber(l.seed ?? 1, `${at}.seed`, 0, 1e9)
    switch (key) {
        case "fill": return { fill: color(l.fill, `${at}.fill`, palette) }
        case "noise": return { noise: colors(l.noise, `${at}.noise`, palette), scale: readNumber(l.scale ?? 4, `${at}.scale`, 1, size), seed: seed }
        case "grain": {
            const along = l.along ?? "y"
            if (along !== "x" && along !== "y") throw invalid(`${at}.along is "x" or "y"`)
            return { grain: colors(l.grain, `${at}.grain`, palette), along: along, seed: seed }
        }
        case "speck": return { speck: color(l.speck, `${at}.speck`, palette), density: readNumber(l.density ?? 0.1, `${at}.density`, 0, 1), seed: seed }
        case "rows": return { rows: readRows(l.rows, `${at}.rows`, size, palette) }
        case "border": return { border: color(l.border, `${at}.border`, palette), width: readNumber(l.width ?? 1, `${at}.width`, 1, size / 2) }
        case "bevel": return { bevel: readNumber(l.bevel, `${at}.bevel`, 0, 1) }
        case "outline": return { outline: color(l.outline, `${at}.outline`, palette) }
    }
}

type Palette = ReadonlyMap<string, Rgba>

function readPalette(p: unknown): Palette {
    const out = new Map<string, Rgba>()
    if (p === undefined) return out
    if (!isObject(p)) throw invalid("palette maps one letter to a colour: { o: \"#8a5a32\" }")
    for (const [k, v] of Object.entries(p)) {
        if ([...k].length !== 1 || CLEAR.has(k)) throw invalid(`palette key ${JSON.stringify(k)}: one character, not "." or " " (those are clear)`)
        out.set(k, hex(v, `palette.${k}`))
    }
    return out
}

function readRows(rows: unknown, at: string, size: number, palette: Palette): (Rgba | null)[][] {
    if (!Array.isArray(rows) || rows.length !== size) throw invalid(`${at} is ${size} strings, one per row (got ${Array.isArray(rows) ? rows.length : typeof rows})`)
    return rows.map((row: unknown, y) => {
        if (typeof row !== "string") throw invalid(`${at}[${y}] is a string`)
        const cells = [...row]
        if (cells.length !== size) throw invalid(`${at}[${y}] is ${cells.length} wide; every row is ${size}`)
        return cells.map((c, x) => {
            if (CLEAR.has(c)) return null
            const found = palette.get(c)
            if (!found) throw invalid(`${at}[${y}][${x}] is "${c}", which isn't in the palette`)
            return found
        })
    })
}

/** A colour: #hex, or a palette letter. */
function color(v: unknown, at: string, palette: Palette): Rgba {
    if (typeof v === "string" && [...v].length === 1) {
        const found = palette.get(v)
        if (!found) throw invalid(`${at} is "${v}", which isn't in the palette`)
        return found
    }
    return hex(v, at)
}

function colors(v: unknown, at: string, palette: Palette): Rgba[] {
    if (!Array.isArray(v) || v.length === 0) throw invalid(`${at} is a list of colours, darkest first`)
    return v.map((c: unknown, i) => color(c, `${at}[${i}]`, palette))
}

function hex(v: unknown, at: string): Rgba {
    const m = typeof v === "string" ? /^#([0-9a-f]{3}|[0-9a-f]{6}|[0-9a-f]{8})$/i.exec(v) : null
    if (!m) throw invalid(`${at} is #rgb, #rrggbb or #rrggbbaa, not ${JSON.stringify(v)}`)
    const h = m[1]!.length === 3 ? [...m[1]!].map(c => c + c).join("") : m[1]!
    const n = (i: number) => parseInt(h.slice(i, i + 2), 16)
    return [n(0), n(2), n(4), h.length === 8 ? n(6) : 255]
}

function readNumber(v: unknown, at: string, min: number, max: number): number {
    if (typeof v !== "number" || !Number.isFinite(v) || v < min || v > max) throw invalid(`${at} is a number from ${min} to ${max}, not ${JSON.stringify(v)}`)
    return v
}

function isObject(v: unknown): v is Record<string, unknown> {
    return typeof v === "object" && v !== null && !Array.isArray(v)
}

function invalid(detail: string) {
    return err("ART_INVALID", { detail: detail })
}
