// The site's sun and moon (apps/site/app/lib/sky.ts), drawn as image URLs for the landing.
// TEMP(2026-11-23): a copy of the site's painters, as the kit's sky.ts copies its stars, until
// both apps import one package; change both together until then.

type Pixel = { x: number; y: number; fill: string }
type Art = { size: number; pixels: Pixel[] }

// 4×4 ordered dither: a pixel takes the next colour when its threshold is under the mix
const BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]] as const

export const bodies = {
    /** A small sun: a hot core, then a halo that thins out in dither. */
    sun(radius: number): string {
        const size = radius * 2 + 1
        const pixels: Pixel[] = []
        for (let y = 0; y < size; y++) for (let x = 0; x < size; x++) {
            const d = Math.hypot(x - radius, y - radius)
            if (d <= 1.6) pixels.push({ x: x, y: y, fill: "#fffaf0" })
            else if (d <= 2.6) pixels.push({ x: x, y: y, fill: "#ffe6b0" })
            else if (d <= radius && dither(x, y, ((radius - d) / (radius - 2.6)) ** 2 * 0.55)) pixels.push({ x: x, y: y, fill: "rgb(255 214 150 / 0.22)" })
        }
        return paint({ size: size, pixels: pixels })
    },

    /** A moon lit from `light` (x right, y down, z toward us), its craters drawn from the seed. */
    moon(seed: string, radius: number, light: readonly [number, number, number]): string {
        const random = rng(hash(seed))
        const craters = Array.from({ length: 9 }, () => {
            const a = random() * Math.PI * 2
            const r = Math.sqrt(random()) * radius * 0.8
            return { x: radius + Math.cos(a) * r, y: radius + Math.sin(a) * r, r: 1 + random() * radius * 0.22 }
        })
        const tones = ["#10161c", "#2a3036", "#4a5259", "#737c84", "#a2aab1", "#ccd3d8"] as const
        const l = Math.hypot(...light)
        const [lx, ly, lz] = [light[0] / l, light[1] / l, light[2] / l]
        const size = radius * 2 + 1
        const pixels: Pixel[] = []
        for (let y = 0; y < size; y++) for (let x = 0; x < size; x++) {
            const nx = (x - radius) / radius, ny = (y - radius) / radius
            const rim = nx * nx + ny * ny
            if (rim > 1) continue
            const lit = nx * lx + ny * ly + Math.sqrt(1 - rim) * lz
            // the night side is one flat tone, a trace of earthshine
            if (lit <= 0.04) { pixels.push({ x: x, y: y, fill: tones[1] }); continue }
            let level = 1 + lit * (tones.length - 1.6)
            for (const c of craters) {
                const d = Math.hypot(x - c.x, y - c.y)
                if (d < c.r) level -= 0.9
                else if (d < c.r + 1 && (x - c.x) * lx + (y - c.y) * ly > 0) level += 0.5
            }
            level = Math.min(tones.length - 1, Math.max(0, level))
            const base = Math.floor(level)
            pixels.push({ x: x, y: y, fill: tones[Math.min(dither(x, y, level - base) ? base + 1 : base, tones.length - 1)]! })
        }
        return paint({ size: size, pixels: pixels })
    },
}

function paint(art: Art): string {
    const canvas = document.createElement("canvas")
    canvas.width = canvas.height = art.size
    const context = canvas.getContext("2d")
    if (!context) throw new Error("bodies: no 2D canvas")
    for (const p of art.pixels) { context.fillStyle = p.fill; context.fillRect(p.x, p.y, 1, 1) }
    return canvas.toDataURL("image/png")
}

function dither(x: number, y: number, mix: number): boolean {
    return (BAYER[y % 4]?.[x % 4] ?? 0) / 16 < mix
}

function hash(text: string): number {
    let h = 2166136261
    for (let i = 0; i < text.length; i++) h = Math.imul(h ^ text.charCodeAt(i), 16777619)
    return h >>> 0
}

function rng(seed: number): () => number {
    let state = seed || 1
    return () => { state ^= state << 13; state ^= state >>> 17; state ^= state << 5; return (state >>> 0) / 4294967296 }
}
