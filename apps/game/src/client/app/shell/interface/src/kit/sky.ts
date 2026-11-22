// The night behind the menus, as the website paints it (apps/site/app/lib/sky.ts): a flat
// dark, pixel stars at a few depths, the odd glint. Pure: a seed and a size make the same
// sky every time. TEMP(2026-11-22): the painter is a copy of the site's until both apps
// import one package; change both together until then.

export type Star = { x: number; y: number; size: number; fill: string }

/** Stars over a `width` × `height` area in art pixels; `glints` of them are bright crosses. */
export function stars(seed: string, width: number, height: number, density = 0.004): Star[] {
    const random = rng(hash(seed))
    const out: Star[] = []
    const count = Math.round(width * height * density)
    for (let i = 0; i < count; i++) {
        const hue = random()
        const tone = hue < 0.07 ? "156 196 255" : hue < 0.11 ? "255 217 160" : "232 236 239"
        const near = random() < 0.08
        out.push({
            x: Math.floor(random() * width),
            y: Math.floor(random() * height),
            size: near ? 2 : 1,
            fill: `rgb(${tone} / ${(0.14 + random() * random() * (near ? 0.7 : 0.5)).toFixed(2)})`,
        })
    }
    return out
}

function hash(text: string): number {
    let h = 2166136261
    for (let i = 0; i < text.length; i++) h = Math.imul(h ^ text.charCodeAt(i), 16777619)
    return h >>> 0
}

function rng(seed: number): () => number {
    let s = seed || 1
    return () => {
        s = (s + 0x6d2b79f5) | 0
        let t = Math.imul(s ^ (s >>> 15), 1 | s)
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296
    }
}
