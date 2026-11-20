import type { BodyCard, BodyKind } from "../bridge"
import type { Body } from "../system/model"

export type BodySurface = { image: ImageData; elevation: Float32Array }

function hash(x: number, y: number, z: number, seed: number) {
    let n = Math.imul(x, 374761393) + Math.imul(y, 668265263) + Math.imul(z, 2147483647) + seed
    n = Math.imul(n ^ (n >>> 13), 1274126177)
    return ((n ^ (n >>> 16)) >>> 0) / 4294967295
}

const smooth = (v: number) => v * v * (3 - 2 * v)
function noise(x: number, y: number, z: number, seed: number) {
    const ix = Math.floor(x), iy = Math.floor(y), iz = Math.floor(z)
    const fx = smooth(x - ix), fy = smooth(y - iy), fz = smooth(z - iz)
    let value = 0
    for (let dz = 0; dz <= 1; dz++)
        for (let dy = 0; dy <= 1; dy++)
            for (let dx = 0; dx <= 1; dx++) {
                const weight = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy) * (dz ? fz : 1 - fz)
                value += hash(ix + dx, iy + dy, iz + dz, seed) * weight
            }
    return value * 2 - 1
}

function seedOf(text: string) {
    let seed = 2166136261
    for (const char of text) seed = Math.imul(seed ^ char.charCodeAt(0), 16777619)
    return seed
}


// How each kind of world looks from space: its ground, how cratered it is,
// and what covers it. Data, so a new kind is one row.
const LOOKS: Record<BodyKind, { ground: readonly [number, number, number] | null; craters: number; cover: "none" | "sea" | "ice" | "cloud" | "haze" | "bands" | "lava" }> = {
    temperate: { ground: null, craters: 0, cover: "sea" },
    "cold desert": { ground: [176, 98, 64], craters: 0.35, cover: "none" },
    hothouse: { ground: [214, 190, 140], craters: 0, cover: "cloud" },
    volcanic: { ground: [196, 170, 72], craters: 0, cover: "lava" },
    "ice shell": { ground: [205, 220, 226], craters: 0.15, cover: "ice" },
    hydrocarbon: { ground: [200, 140, 70], craters: 0.05, cover: "haze" },
    airless: { ground: null, craters: 0.9, cover: "none" },
    giant: { ground: null, craters: 0, cover: "bands" },
}

/**
 * A body's look from space, painted from what it became (its card) and its
 * own colour and seed. A sketch of the kind, not terrain: only the playable
 * world has generated land.
 */
export function bodySurface(body: Body, card: BodyCard): BodySurface {
    const width = 256, height = 128
    const image = new ImageData(width, height)
    const elevation = new Float32Array(width * height)
    const look = LOOKS[card.kind]
    const own = [1, 3, 5].map((i) => Number.parseInt(body.color.slice(i, i + 2), 16))
    const ground = look.ground ?? own
    const seed = seedOf(body.id)
    const craters = Array.from({ length: Math.round(look.craters * 30) }, (_, i) => {
        const u = hash(i, 5, 9, seed), v = hash(i, 11, 4, seed)
        const lon = u * Math.PI * 2, lat = Math.asin(v * 2 - 1)
        return { x: Math.cos(lat) * Math.cos(lon), y: Math.sin(lat), z: Math.cos(lat) * Math.sin(lon),
            radius: 0.035 + hash(i, 7, 3, seed) * 0.11 }
    })
    // polar ice where it's cold enough for some; the sea's share from its water
    const capFrom = card.temperature < 240 ? 0.45 : card.temperature < 290 ? 0.8 : 1.1
    const seaLevel = card.ocean ? 0.05 : -2

    for (let py = 0; py < height; py++)
        for (let px = 0; px < width; px++) {
            const lon = (px / width) * Math.PI * 2
            const lat = (py / (height - 1) - 0.5) * Math.PI
            const x = Math.cos(lat) * Math.cos(lon), y = Math.sin(lat), z = Math.cos(lat) * Math.sin(lon)
            const broad = noise(x * 3.2 + 8, y * 3.2 + 8, z * 3.2 + 8, seed)
            const detail = noise(x * 13 + 19, y * 13 + 19, z * 13 + 19, seed + 17)
            const fine = noise(x * 35 + 41, y * 35 + 41, z * 35 + 41, seed + 31)
            let relief = broad * 0.65 + detail * 0.25 + fine * 0.1
            let shade = 0.74 + relief * 0.35
            for (const crater of craters) {
                const distance = Math.sqrt(Math.max(0, 2 - 2 * (x * crater.x + y * crater.y + z * crater.z))) / crater.radius
                if (distance < 1.25) {
                    relief += distance < 0.74 ? -0.11 : distance < 1 ? 0.17 : 0.04
                    shade += distance < 0.74 ? -0.09 : distance < 1 ? 0.12 : 0
                }
            }

            let rgb: number[] = ground.map((g) => g * shade)
            let height01 = Math.max(0, relief * 0.7 + 0.25)
            switch (look.cover) {
                case "sea":
                    if (relief < seaLevel) { rgb = [22, 75, 125]; height01 = 0 }
                    else rgb = relief < seaLevel + 0.08 ? [196, 182, 140] : [72 * shade, 128 * shade, 62 * shade]
                    break
                case "ice": {
                    const crack = Math.abs(detail) < 0.03 ? 0.75 : 1
                    rgb = [205 * crack * shade, 220 * crack * shade, 226 * crack]
                    height01 *= 0.3
                    break
                }
                case "cloud":
                    rgb = ground.map((g) => g * (0.9 + broad * 0.12)); height01 = 0
                    break
                case "haze":
                    rgb = ground.map((g) => g * (0.82 + broad * 0.1)); height01 *= 0.2
                    break
                case "bands": {
                    const band = Math.sin(lat * 34 + broad * 3) * 0.17 + Math.sin(lat * 83 + detail * 2) * 0.07
                    rgb = own.map((g) => g * (0.8 + band + broad * 0.1)); height01 = 0
                    break
                }
                case "lava":
                    if (fine > 0.55) rgb = [255, 120, 40]
                    break
                case "none":
                    break
            }
            if (look.cover !== "bands" && look.cover !== "cloud" && Math.abs(y) > capFrom) rgb = [228, 236, 240]

            const at = (py * width + px) * 4
            for (let c = 0; c < 3; c++) image.data[at + c] = Math.max(0, Math.min(255, rgb[c]!))
            image.data[at + 3] = 255
            elevation[py * width + px] = height01
        }
    return { image, elevation }
}

/** Keep other bodies' looks while one body's controls are being dragged. */
export function BodySurfaceCache() {
    const cache = new Map<string, { signature: string; surface: BodySurface }>()
    return {
        get(body: Body, card: BodyCard) {
            const signature = [card.kind, Math.round(card.temperature), card.ocean, body.color].join(":")
            const hit = cache.get(body.id)
            if (hit?.signature === signature) return hit.surface
            const surface = bodySurface(body, card)
            cache.set(body.id, { signature, surface })
            return surface
        },
    }
}
