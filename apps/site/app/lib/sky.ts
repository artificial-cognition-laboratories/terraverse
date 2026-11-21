// The landing's world, as pixel art: the sky, its stars, the sun and moon, and the
// ground the page ends on. Pure: each painter turns a seed and a size into pixels,
// the same on the server and in every browser.
//
// The art style, for everything drawn here: one art pixel is `PX` CSS pixels, colours
// come from the generator's own palette, and depth is stepped colour bands with ordered
// dithering where they meet, never a smooth gradient.

export const PX = 5

export type Pixel = { x: number; y: number; w: number; h: number; fill: string }
export type Art = { width: number; height: number; pixels: Pixel[] }

// 4×4 ordered dither: a pixel takes the next colour when its threshold is under the mix
const BAYER = [
    [0, 8, 2, 10],
    [12, 4, 14, 6],
    [3, 11, 1, 9],
    [15, 7, 13, 5],
] as const

/**
 * The night from the top of the page to the horizon: equal steps, each one solid,
 * dithered into the next only across a narrow seam. Words sit on the solid parts.
 */
const NIGHT = ["#04070b", "#060a0f", "#080d13", "#0a1018", "#0c131c", "#0f1722"] as const
const SEAM = 5 // art pixels of dither each side of a step

export const sky = {
    /** The page-tall sky: 4 pixels wide, since a dither repeats every 4 and the sky is the same across. */
    gradient(height: number): Art {
        const pixels: Pixel[] = []
        const step = height / NIGHT.length
        for (let y = 0; y < height; y++) {
            const band = Math.min(NIGHT.length - 1, Math.floor(y / step))
            // the nearest step below or above, and how far into its seam this row is
            const edge = Math.round(y / step) * step
            const into = y - edge
            const below = NIGHT[Math.min(NIGHT.length - 1, Math.round(y / step))] ?? NIGHT[0]
            const above = NIGHT[Math.max(0, Math.round(y / step) - 1)] ?? NIGHT[0]
            const seam = Math.abs(into) < SEAM && edge > 0 && edge < height
            for (let x = 0; x < 4; x++) {
                const fill = seam ? (dither(x, y, (into + SEAM) / (2 * SEAM)) ? below : above) : (NIGHT[band] ?? NIGHT[0])
                pixels.push({ x: x, y: y, w: 1, h: 1, fill: fill })
            }
        }
        return { width: 4, height: height, pixels: pixels }
    },

    /** A tile of stars that repeats: `glints` of them are bright, with a cross of light. */
    stars(seed: string, width: number, height: number, count: number, glints: number): Art {
        const random = rng(hash(seed))
        const pixels: Pixel[] = []
        for (let i = 0; i < count; i++) {
            const x = Math.floor(random() * width)
            const y = Math.floor(random() * height)
            const hue = random()
            const tone = hue < 0.07 ? "156 196 255" : hue < 0.11 ? "255 217 160" : "232 236 239"
            pixels.push({ x: x, y: y, w: 1, h: 1, fill: `rgb(${tone} / ${(0.12 + random() * random() * 0.6).toFixed(2)})` })
        }
        for (let i = 0; i < glints; i++) {
            const x = 1 + Math.floor(random() * (width - 2))
            const y = 1 + Math.floor(random() * (height - 2))
            pixels.push({ x: x, y: y, w: 1, h: 1, fill: "#f4f7fa" })
            for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]] as const) {
                pixels.push({ x: x + dx, y: y + dy, w: 1, h: 1, fill: "rgb(232 236 239 / 0.35)" })
            }
        }
        return { width: width, height: height, pixels: pixels }
    },

    /** A small sun: a hot core, then a halo that thins out in dither. */
    sun(radius: number): Art {
        const size = radius * 2 + 1
        const pixels: Pixel[] = []
        for (let y = 0; y < size; y++) {
            for (let x = 0; x < size; x++) {
                const d = Math.hypot(x - radius, y - radius)
                if (d <= 1.6) pixels.push({ x: x, y: y, w: 1, h: 1, fill: "#fffaf0" })
                else if (d <= 2.6) pixels.push({ x: x, y: y, w: 1, h: 1, fill: "#ffe6b0" })
                else if (d <= radius && dither(x, y, ((radius - d) / (radius - 2.6)) ** 2 * 0.55)) {
                    pixels.push({ x: x, y: y, w: 1, h: 1, fill: "rgb(255 214 150 / 0.22)" })
                }
            }
        }
        return { width: size, height: size, pixels: pixels }
    },

    /** A moon lit from `light` (x right, y down, z toward us), its craters drawn from the seed. */
    moon(seed: string, radius: number, light: readonly [number, number, number]): Art {
        const random = rng(hash(seed))
        const craters = Array.from({ length: 9 }, () => {
            const a = random() * Math.PI * 2
            const r = Math.sqrt(random()) * radius * 0.8
            return { x: radius + Math.cos(a) * r, y: radius + Math.sin(a) * r, r: 1 + random() * radius * 0.22 }
        })
        const tones = ["#10161c", "#2a3036", "#4a5259", "#737c84", "#a2aab1", "#ccd3d8"]
        const [lx, ly, lz] = unit(light)
        const size = radius * 2 + 1
        const pixels: Pixel[] = []
        for (let y = 0; y < size; y++) {
            for (let x = 0; x < size; x++) {
                const nx = (x - radius) / radius
                const ny = (y - radius) / radius
                const rim = nx * nx + ny * ny
                if (rim > 1) continue
                const nz = Math.sqrt(1 - rim)
                // light on the sphere; the night side is one flat tone, a trace of earthshine
                const lit = nx * lx + ny * ly + nz * lz
                if (lit <= 0.04) {
                    pixels.push({ x: x, y: y, w: 1, h: 1, fill: tones[1] ?? "#2a3036" })
                    continue
                }
                let level = 1 + lit * (tones.length - 1.6)
                for (const c of craters) {
                    const d = Math.hypot(x - c.x, y - c.y)
                    if (d < c.r) level -= 0.9
                    else if (d < c.r + 1 && (x - c.x) * lx + (y - c.y) * ly > 0) level += 0.5
                }
                level = Math.min(tones.length - 1, Math.max(0, level))
                const base = Math.floor(level)
                const step = dither(x, y, level - base) ? base + 1 : base
                pixels.push({ x: x, y: y, w: 1, h: 1, fill: tones[Math.min(step, tones.length - 1)] ?? "#10161c" })
            }
        }
        return { width: size, height: size, pixels: pixels }
    },

    /**
     * Where the page comes to land: a far range, the near ridge and its soil, then
     * stone darkening to bedrock, with ore here and there in the deep.
     */
    ground(seed: string, width: number, height: number): { far: Art; near: Art } {
        const random = rng(hash(seed))
        const [w0, w1, w2, w3] = [random() * 6, random() * 6, random() * 6, random() * 6]
        const far: Pixel[] = []
        const near: Pixel[] = []
        const strata = ["#5f7f4a", "#58646b", "#3d464c", "#262c31"]
        const deep = [
            { at: 0.42, fill: "#1d2226" },
            { at: 0.62, fill: "#16191c" },
            { at: 0.8, fill: "#0f1113" },
            { at: 1, fill: "#0c0e10" },
        ]
        const ores = ["#7a5232", "#3f8a6a", "#c9a24a", "#8fa3ae"]
        for (let x = 0; x < width; x++) {
            const back = Math.round(height * 0.12 + Math.sin(x * 0.045 + w3) * 5 + Math.sin(x * 0.13 + w1) * 2)
            far.push({ x: x, y: back, w: 1, h: height - back, fill: "#09111a" })

            const ridge = Math.round(height * 0.26 + Math.sin(x * 0.03 + w0) * 4 + Math.sin(x * 0.11 + w1) * 1.8 + Math.sin(x * 0.29 + w2) * 0.8)
            let y = ridge
            strata.forEach((fill, layer) => {
                const h = layer === 0 ? 1 : 2 + Math.floor(random() * 3)
                near.push({ x: x, y: y, w: 1, h: h, fill: fill })
                y += h
            })
            // below the soil: stone, each band dithered into the next
            for (; y < height; y++) {
                const t = y / height
                const i = Math.max(0, deep.findIndex(d => d.at > t) - 1)
                const from = deep[i] ?? deep[0]!
                const to = deep[i + 1] ?? from
                const mix = to === from ? 0 : (t - from.at) / (to.at - from.at)
                near.push({ x: x, y: y, w: 1, h: 1, fill: dither(x, y, mix) ? to.fill : from.fill })
                if (t > 0.45 && t < 0.85 && random() < 0.0022) near.push({ x: x, y: y, w: 1, h: 1, fill: ores[Math.floor(random() * ores.length)] ?? "#7a5232" })
            }
        }
        return { far: { width: width, height: height, pixels: far }, near: { width: width, height: height, pixels: near } }
    },
}

function dither(x: number, y: number, mix: number): boolean {
    return (BAYER[y % 4]?.[x % 4] ?? 0) / 16 < mix
}

function unit([x, y, z]: readonly [number, number, number]): [number, number, number] {
    const l = Math.hypot(x, y, z) || 1
    return [x / l, y / l, z / l]
}

function hash(text: string): number {
    let h = 2166136261
    for (let i = 0; i < text.length; i++) h = Math.imul(h ^ text.charCodeAt(i), 16777619)
    return h >>> 0
}

function rng(seed: number): () => number {
    let state = seed || 1
    return () => {
        state ^= state << 13; state ^= state >>> 17; state ^= state << 5
        return (state >>> 0) / 4294967296
    }
}
