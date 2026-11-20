// Tileable fractal noise for the preview mock. A point on the wrapping world
// is mapped onto a 4D torus, so the field matches across both edges exactly.

function hash4(x: number, y: number, z: number, w: number, seed: number) {
    let h = seed ^ Math.imul(x, 0x27d4eb2d) ^ Math.imul(y, 0x165667b1) ^ Math.imul(z, 0x9e3779b1) ^ Math.imul(w, 0x85ebca77)
    h = Math.imul(h ^ (h >>> 15), 0x2c1b3c6d)
    h = Math.imul(h ^ (h >>> 12), 0x297a2d39)
    return ((h ^ (h >>> 15)) >>> 0) / 4294967295
}

const smooth = (t: number) => t * t * (3 - 2 * t)
const lerp = (a: number, b: number, t: number) => a + (b - a) * t

/** 4D value noise in -1..1. */
function value4(x: number, y: number, z: number, w: number, seed: number) {
    const xi = Math.floor(x), yi = Math.floor(y), zi = Math.floor(z), wi = Math.floor(w)
    const tx = smooth(x - xi), ty = smooth(y - yi), tz = smooth(z - zi), tw = smooth(w - wi)
    let acc = 0
    for (let c = 0; c < 16; c++) {
        const dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1, dw = (c >> 3) & 1
        const weight = (dx ? tx : 1 - tx) * (dy ? ty : 1 - ty) * (dz ? tz : 1 - tz) * (dw ? tw : 1 - tw)
        acc += weight * hash4(xi + dx, yi + dy, zi + dz, wi + dw, seed)
    }
    return acc * 2 - 1
}

export function seedNumber(seed: string) {
    let h = 2166136261
    for (let i = 0; i < seed.length; i++) h = Math.imul(h ^ seed.charCodeAt(i), 16777619)
    return h >>> 0
}

/**
 * Fractal noise at (u, v) ∈ [0,1)² on the wrapping world, -1..1.
 * `frequency` is how many features fit across one side.
 */
export function torusFbm(u: number, v: number, seed: number, frequency: number, octaves: number, persistence: number) {
    let sum = 0, amp = 1, norm = 0, f = frequency
    for (let o = 0; o < octaves; o++) {
        const r = f / (2 * Math.PI)
        const a = u * 2 * Math.PI, b = v * 2 * Math.PI
        sum += amp * value4(r * Math.cos(a), r * Math.sin(a), r * Math.cos(b), r * Math.sin(b), seed + o * 1013)
        norm += amp
        amp *= persistence
        f *= 2
    }
    return sum / norm
}
