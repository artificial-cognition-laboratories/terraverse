import { raster, type Raster, type Rgba } from "../../../image"
import type { Layer, Spec } from "./spec"

/**
 * paint — a Spec drawn: each layer over the last. Deterministic (seeded, no Math.random),
 * and noise wraps at the edge so a block's faces tile.
 */
export const paint = {
    draw(s: Spec): Raster {
        const r = raster.blank(s.size, s.size)
        for (const layer of s.layers) apply(r, layer)
        return r
    },
}

function apply(r: Raster, l: Layer): void {
    const n = r.width
    const each = (fn: (x: number, y: number) => Rgba | null) => {
        for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
            const c = fn(x, y)
            if (c) raster.put(r, x, y, c)
        }
    }
    if ("fill" in l) each(() => l.fill)
    else if ("noise" in l) each((x, y) => pick(l.noise, smooth(x, y, l.scale, n, l.seed)))
    else if ("grain" in l) each((x, y) => {
        const [across, along] = l.along === "y" ? [x, y] : [y, x]
        // streaks: mostly a function of where across the grain, a little of where along it
        const v = 0.75 * smooth(across, 0, 2, n, l.seed) + 0.25 * smooth(across, along, 4, n, l.seed + 7)
        return pick(l.grain, v)
    })
    else if ("speck" in l) each((x, y) => hash(x, y, l.seed) < l.density ? l.speck : null)
    else if ("rows" in l) each((x, y) => l.rows[y]![x]!)
    else if ("border" in l) each((x, y) => Math.min(x, y, n - 1 - x, n - 1 - y) < l.width ? l.border : null)
    else if ("bevel" in l) bevel(r, l.bevel)
    else outline(r, l.outline)
}

/** One light, top-left: an edge facing it lighter, an edge facing away darker. Shapes too. */
function bevel(r: Raster, amount: number): void {
    const solid = (x: number, y: number) => x >= 0 && y >= 0 && x < r.width && y < r.height && raster.get(r, x, y)[3] > 0
    const changes: [number, number, Rgba][] = []
    for (let y = 0; y < r.height; y++) for (let x = 0; x < r.width; x++) {
        if (!solid(x, y)) continue
        const c = raster.get(r, x, y)
        if (!solid(x - 1, y) || !solid(x, y - 1)) changes.push([x, y, mix(c, [255, 255, 255, c[3]], amount)])
        else if (!solid(x + 1, y) || !solid(x, y + 1)) changes.push([x, y, mix(c, [0, 0, 0, c[3]], amount)])
    }
    for (const [x, y, c] of changes) raster.set(r, x, y, c)
}

/** A line round a shape: the clear pixels beside a solid one. */
function outline(r: Raster, c: Rgba): void {
    const solid = (x: number, y: number) => x >= 0 && y >= 0 && x < r.width && y < r.height && raster.get(r, x, y)[3] > 0
    const at: [number, number][] = []
    for (let y = 0; y < r.height; y++) for (let x = 0; x < r.width; x++)
        if (!solid(x, y) && (solid(x - 1, y) || solid(x + 1, y) || solid(x, y - 1) || solid(x, y + 1))) at.push([x, y])
    for (const [x, y] of at) raster.set(r, x, y, c)
}

function pick(colors: readonly Rgba[], v: number): Rgba {
    return colors[Math.min(colors.length - 1, Math.floor(v * colors.length))]!
}

function mix(a: Rgba, b: Rgba, t: number): Rgba {
    const at = (i: 0 | 1 | 2 | 3) => Math.round(a[i] + (b[i] - a[i]) * t)
    return [at(0), at(1), at(2), at(3)]
}

/** Value noise in [0, 1): a lattice every `scale` pixels, smoothed, wrapping at `size`. */
function smooth(x: number, y: number, scale: number, size: number, seed: number): number {
    const cells = Math.max(1, Math.round(size / scale))
    const fx = (x / size) * cells, fy = (y / size) * cells
    const x0 = Math.floor(fx), y0 = Math.floor(fy)
    const tx = ease(fx - x0), ty = ease(fy - y0)
    const at = (i: number, j: number) => hash(((i % cells) + cells) % cells, ((j % cells) + cells) % cells, seed)
    const top = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * tx
    const bottom = at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * tx
    // stretched from the middle toward the ends, so every colour in the list gets used
    return Math.min(0.999, Math.max(0, (top + (bottom - top) * ty - 0.5) * 1.6 + 0.5))
}

function ease(t: number): number {
    return t * t * (3 - 2 * t)
}

/** A number in [0, 1) from a cell and a seed: the same everywhere. */
function hash(x: number, y: number, seed: number): number {
    let h = (Math.imul(x, 374761393) + Math.imul(y, 668265263) + Math.imul(seed, 1442695041)) | 0
    h = Math.imul(h ^ (h >>> 13), 1274126177)
    h ^= h >>> 16
    return (h >>> 0) / 4294967296
}
