// A world's generation settings, and the world code that carries them.
// The world wraps east–west and ends in polar edges north and south.
// The page owns this shape until terragen exists; then the engine does.
// The code carries the land only — how a world is *played* (rules) is separate.

export type WorldConfig = {
    seed: string
    size: number        // blocks per side; wraps east–west, with polar north/south edges
    continents: number  // 1..8, roughly how many landmasses
    land: number        // 0..1, share of the surface above sea level
    coast: number       // 0..1, smooth shores ↔ fjords and islets
    mountains: number   // 0..1, how high and how much
    roughness: number   // 0..1, small-scale detail
    temperature: number // -1..1, colder ↔ hotter
    humidity: number    // -1..1, drier ↔ wetter
    rivers: number      // 0..1, river density (not in the preview yet)
    caves: number       // 0..1, how hollow the ground is (not in the preview yet)
    ores: number        // 0..1, how rich the rock is (not in the preview yet)
    star: StarId        // the sun's type: its colour and how much warmth it gives
    tilt: number        // 0..45 degrees of axial tilt: how strong the seasons are
    year: number        // in-game days per year: how long each season lasts
}

/** Star types, coolest to hottest. `warmth` shifts the whole planet's climate. */
export const STARS = [
    { id: "red", label: "Red dwarf", note: "dim and ruddy — a cold, long-shadowed world", color: [255, 128, 82], warmth: -0.55 },
    { id: "orange", label: "Orange", note: "warm amber light, slightly cooler", color: [255, 190, 120], warmth: -0.2 },
    { id: "yellow", label: "Yellow", note: "like our sun", color: [255, 244, 220], warmth: 0 },
    { id: "white", label: "White", note: "bright and harsh, a hotter world", color: [235, 242, 255], warmth: 0.25 },
    { id: "blue", label: "Blue", note: "fierce blue-white — deserts spread", color: [180, 205, 255], warmth: 0.5 },
] as const satisfies readonly { id: string; label: string; note: string; color: readonly [number, number, number]; warmth: number }[]

export type StarId = (typeof STARS)[number]["id"]

export function star(id: StarId) {
    const found = STARS.find((s) => s.id === id)
    if (!found) throw new Error(`unknown star type ${id}`)
    return found
}

/** Blocks round the equator: a planet of radius size / 2π, from 2.6k to the
 *  Earth's 6.371M. Must match src/terragen/code.cpp SIZES (the code stores the index). */
export const SIZES = [16384, 62832, 157088, 314160, 628320, 1570800, 3141600, 6283184, 12566368, 20106192, 40030176] as const
/** The Earth's radius in blocks (a block a metre). */
export const EARTH_RADIUS = 6_371_000

export const DEFAULTS: Omit<WorldConfig, "seed"> = {
    size: 16384, // blocks round the planet's equator: an atlas of radius size / 2π
    continents: 4,
    land: 0.4,
    coast: 0.5,
    mountains: 0.5,
    roughness: 0.5,
    temperature: 0,
    humidity: 0,
    rivers: 0.5,
    caves: 0.5,
    ores: 0.5,
    star: "yellow",
    tilt: 23,
    year: 48,
}

/** One-click starting points for the land. Anything not listed stays as it is. */
export const WORLD_PRESETS = [
    { id: "continents", label: "Continents", note: "a few big landmasses, open seas", values: { continents: 4, land: 0.4, coast: 0.5, mountains: 0.5 } },
    { id: "archipelago", label: "Archipelago", note: "scattered islands, warm water", values: { continents: 8, land: 0.25, coast: 0.85, mountains: 0.3, temperature: 0.3 } },
    { id: "pangaea", label: "Pangaea", note: "one supercontinent", values: { continents: 1, land: 0.65, coast: 0.35, mountains: 0.6 } },
    { id: "highlands", label: "Highlands", note: "rugged peaks, cold valleys", values: { continents: 3, land: 0.6, coast: 0.6, mountains: 0.95, roughness: 0.75, temperature: -0.4 } },
    { id: "waterworld", label: "Waterworld", note: "almost all ocean", values: { continents: 6, land: 0.12, coast: 0.7, mountains: 0.2 } },
] as const satisfies readonly { id: string; label: string; note: string; values: Partial<WorldConfig> }[]

const between = (lo: number, hi: number) => lo + Math.random() * (hi - lo)

/** Random land settings within sensible bounds — for discovering world types (size is kept). */
export function surprise(): Omit<WorldConfig, "seed" | "size"> {
    return {
        continents: 1 + Math.floor(Math.random() * 8),
        land: between(0.15, 0.7),
        coast: between(0, 1),
        mountains: between(0.1, 1),
        roughness: between(0.2, 0.9),
        temperature: between(-0.6, 0.6),
        humidity: between(-0.6, 0.6),
        rivers: between(0, 1),
        caves: between(0, 1),
        ores: between(0.2, 0.8),
        star: STARS[Math.floor(Math.random() * STARS.length)]!.id,
        tilt: Math.round(between(0, 40)),
        year: Math.round(between(16, 96)),
    }
}

export function randomSeed() {
    const alphabet = "abcdefghjkmnpqrstuvwxyz23456789"
    return Array.from(crypto.getRandomValues(new Uint8Array(8)), (b) => alphabet[b % alphabet.length]).join("")
}

/*
 * World code: TF9.<seed>.<params>  (TF1: before water and exact land share; TF2: before plates and geology; TF3: before the climate; TF4: before the ocean currents; TF5: on the flat map, before the sphere; TF6: places too coarse for large planets; TF7: before the plates had a history; TF8: before the energy-balance climate)
 *   seed   — base64url of the UTF-8 seed
 *   params — base64url: one byte for the size, one for the star, then one
 *            per field (fixed order, quantized); omitted when all default
 * The version prefix means a code never silently changes meaning.
 */
const VERSION = "TF9"

type Field = { key: Exclude<keyof WorldConfig, "seed" | "size" | "star">; min: number; max: number }
const FIELDS: Field[] = [
    { key: "continents", min: 1, max: 8 },
    { key: "land", min: 0, max: 1 },
    { key: "coast", min: 0, max: 1 },
    { key: "mountains", min: 0, max: 1 },
    { key: "roughness", min: 0, max: 1 },
    { key: "temperature", min: -1, max: 1 },
    { key: "humidity", min: -1, max: 1 },
    { key: "rivers", min: 0, max: 1 },
    { key: "caves", min: 0, max: 1 },
    { key: "ores", min: 0, max: 1 },
    { key: "tilt", min: 0, max: 45 },
    { key: "year", min: 8, max: 128 },
]

const b64 = {
    encode: (bytes: Uint8Array) => btoa(String.fromCharCode(...bytes)).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, ""),
    decode: (text: string) => Uint8Array.from(atob(text.replace(/-/g, "+").replace(/_/g, "/")), (c) => c.charCodeAt(0)),
}

const quantize = (f: Field, v: number) => Math.round(((v - f.min) / (f.max - f.min)) * 255)
const unquantize = (f: Field, b: number) => f.min + (b / 255) * (f.max - f.min)

export function encodeWorldCode(config: WorldConfig): string {
    const seed = b64.encode(new TextEncoder().encode(config.seed))
    const atDefaults = config.size === DEFAULTS.size && config.star === DEFAULTS.star &&
        FIELDS.every((f) => quantize(f, config[f.key]) === quantize(f, DEFAULTS[f.key]))
    if (atDefaults) return `${VERSION}.${seed}`
    const size = SIZES.findIndex((s) => s === config.size)
    const starIndex = STARS.findIndex((s) => s.id === config.star)
    if (size < 0 || starIndex < 0) throw new Error("world config has a size or star this build doesn't know")
    const bytes = new Uint8Array([size, starIndex, ...FIELDS.map((f) => quantize(f, config[f.key]))])
    return `${VERSION}.${seed}.${b64.encode(bytes)}`
}

export class WorldCodeError extends Error {}

export function decodeWorldCode(code: string): WorldConfig {
    const [version, seedPart, params] = code.trim().split(".")
    if (version === "TF1" || version === "TF2" || version === "TF3" || version === "TF4" || version === "TF5" || version === "TF6" || version === "TF7" || version === "TF8") throw new WorldCodeError(`This world code is from an older build (${version}) — its worlds can't be generated any more.`)
    if (version !== VERSION) throw new WorldCodeError(`Unknown world code version "${version ?? ""}" — this game reads ${VERSION}.`)
    if (!seedPart) throw new WorldCodeError("This world code has no seed.")
    const config: WorldConfig = { ...DEFAULTS, seed: new TextDecoder().decode(b64.decode(seedPart)) }
    if (params === undefined) return config

    const bytes = b64.decode(params)
    if (bytes.length !== FIELDS.length + 2) throw new WorldCodeError("This world code is damaged, or from an older build.")
    const size = SIZES[bytes[0]!]
    const starType = STARS[bytes[1]!]
    if (size === undefined || starType === undefined) throw new WorldCodeError("This world code names a world size or star this game doesn't have.")
    config.size = size
    config.star = starType.id
    FIELDS.forEach((f, i) => (config[f.key] = unquantize(f, bytes[i + 2]!)))
    config.continents = Math.round(config.continents)
    config.tilt = Math.round(config.tilt)
    config.year = Math.round(config.year)
    return config
}
