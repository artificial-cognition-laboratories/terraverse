// A pixel landscape from a string: sky, a few stars, a ridge and its strata. Pure.

type Column = { x: number; y: number; h: number; fill: string }
export type CoverArt = { width: number; height: number; sky: string; pixels: Column[] }

/** Strata from the surface down; one is picked per seed. */
const LANDS: readonly [readonly string[], ...(readonly string[])[]] = [
    ["#4a9b64", "#6b5236", "#4a3a2a", "#2e2620"],
    ["#c9b27a", "#a5764a", "#7a5232", "#4a321f"],
    ["#d9e4ea", "#8fa3ae", "#5d6d78", "#38434b"],
    ["#a76c55", "#7c4a39", "#5a3427", "#38211a"],
    ["#5f7f4a", "#58646b", "#3d464c", "#262c31"],
]
const SKIES: readonly [string, ...string[]] = ["#0c1a26", "#101820", "#1a1424", "#0a1f22"]

/** `width` in art pixels: 32 for a card, more for a banner, so wide art isn't a close-up. */
export function cover(seed: string, width = 32): CoverArt {
    const height = 18
    const random = rng(hash(seed))
    const land = pick(LANDS, random())
    const sky = pick(SKIES, random())
    const pixels: Column[] = []
    for (let i = 0; i < width / 3.5; i++) {
        pixels.push({ x: Math.floor(random() * width), y: Math.floor(random() * height * 0.45), h: 1, fill: "rgb(232 236 239 / 0.55)" })
    }
    const base = height * (0.45 + random() * 0.2)
    const [w0, w1, w2] = [random() * 6, random() * 6, random() * 6]
    for (let x = 0; x < width; x++) {
        const ridge = Math.round(base + Math.sin(x * 0.21 + w0) * 2.4 + Math.sin(x * 0.53 + w1) * 1.1 + Math.sin(x * 0.07 + w2) * 3)
        let y = Math.max(2, ridge)
        land.forEach((fill, layer) => {
            // the bottom stratum fills to the edge; a low ridge can run the rest off it
            const h = Math.min(height - y, layer === land.length - 1 ? height - y : layer === 0 ? 1 : 2 + Math.floor(random() * 2))
            if (h > 0) pixels.push({ x: x, y: y, h: h, fill: fill })
            y += h
        })
    }
    return { width: width, height: height, sky: sky, pixels: pixels }
}

/** One of a non-empty list, by a number in [0, 1). */
function pick<T>(list: readonly [T, ...T[]], at: number): T {
    return list[Math.floor(at * list.length)] ?? list[0]
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
