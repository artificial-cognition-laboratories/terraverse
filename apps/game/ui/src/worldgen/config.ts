// A world's generation settings, and the world code that carries them.
// The world wraps east–west and ends in polar edges north and south.
// The page owns this shape until terragen exists; then the engine does.
// The code carries the land only — how a world is *played* (rules) is separate.

import type { BodyLandWire, SystemBodyWire } from "../bridge"
import { SPIN_WIRES } from "../bridge/types"
import { Companion, COMPANION_KINDS, lifetime, separationByte, separationOf, spanned, spannedByte, STAR_CLASSES, starClass, type CompanionWire, type StarClass } from "../system/stars"

/** Index = the byte in the code (src/terragen/code.h SystemStyle). `home` is the one fixed system. */
export const SYSTEM_STYLES = ["home", "sunlike", "random", "binary", "double", "harsh"] as const
export type SystemStyle = (typeof SYSTEM_STYLES)[number]

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
    star: StarId        // the sun's spectral class
    starMass: number    // solar masses, in its class's range
    metallicity: number // [Fe/H]: dex against the Sun's, -1..0.5
    age: number         // the system's, billions of years; no more than the star's life
    wind: number        // 0..1: how hard the star's wind blows
    companion: CompanionWire // the second star; none under style binary: the seed's
    style: SystemStyle  // how the seed lays out the system
    bodies: readonly SystemBodyWire[] | null // the system as edited, parents first; null: as the style lays it out
    tilt: number        // 0..45 degrees of axial tilt: how strong the seasons are
    year: number        // in-game days per year: how long each season lasts
}

export type StarId = StarClass
export const star = starClass

/** Blocks round the equator: a planet of radius size / 2π, from 2.6k to the
 *  Earth's 6.371M. Must match src/terragen/code.cpp SIZES (the code stores the index). */
export const SIZES = [16384, 62832, 157088, 314160, 628320, 1570800, 3141600, 6283184, 12566368, 20106192, 40030176] as const
/** The Earth's radius in blocks (a block a metre). */
export const EARTH_RADIUS = 6_371_000

/** What a new world starts at: radius 100k, 44 hours' walk round. Not DEFAULTS.size:
 *  that's what a code with no settings means, which never changes. */
export const NEW_WORLD_SIZE = 628320

/** What a new world is laid out as: its seed's own system. */
export const NEW_WORLD_STYLE: SystemStyle = "sunlike"

export const DEFAULTS: Omit<WorldConfig, "seed"> = {
    size: 16384, // blocks round the planet's equator: an atlas of radius size / 2π
    continents: 7,
    land: 0.25,
    coast: 0.5,
    mountains: 0.5,
    roughness: 0.5,
    temperature: 0,
    humidity: 0,
    rivers: 0.5,
    caves: 0.5,
    ores: 0.5,
    star: "G",
    starMass: 1,
    metallicity: 0,
    age: 4.5,
    wind: 0.5,
    companion: { kind: "none", class: "K", mass: 0.75, separation: 0.2 },
    style: "home",
    bodies: null,
    tilt: 23,
    year: 48,
}

/** One-click starting points for the land. Anything not listed stays as it is. */
export const WORLD_PRESETS = [
    { id: "continents", label: "Continents", note: "several landmasses, open seas", values: { continents: 7, land: 0.25, coast: 0.5, mountains: 0.5 } },
    { id: "archipelago", label: "Archipelago", note: "scattered islands, warm water", values: { continents: 8, land: 0.25, coast: 0.85, mountains: 0.3, temperature: 0.3 } },
    { id: "pangaea", label: "Pangaea", note: "one supercontinent", values: { continents: 1, land: 0.65, coast: 0.35, mountains: 0.6 } },
    { id: "highlands", label: "Highlands", note: "rugged peaks, cold valleys", values: { continents: 3, land: 0.6, coast: 0.6, mountains: 0.95, roughness: 0.75, temperature: -0.4 } },
    { id: "waterworld", label: "Waterworld", note: "almost all ocean", values: { continents: 6, land: 0.12, coast: 0.7, mountains: 0.2 } },
] as const satisfies readonly { id: string; label: string; note: string; values: Partial<WorldConfig> }[]

const between = (lo: number, hi: number) => lo + Math.random() * (hi - lo)

/** Random land settings within sensible bounds — for discovering world types (size is kept). */
export function surprise(): Omit<WorldConfig, "seed" | "size" | "style" | "bodies"> {
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
        companion: Companion(),
        ...randomStar(),
        tilt: Math.round(between(0, 40)),
        year: Math.round(between(16, 96)),
    }
}

/** Any star, at its class's usual mass, some way through its life. */
function randomStar() {
    const chosen = STAR_CLASSES[Math.floor(Math.random() * STAR_CLASSES.length)]!
    return { star: chosen.id, starMass: chosen.mass.usual, metallicity: between(-0.5, 0.3), age: Math.min(13, lifetime(chosen.mass.usual)) * between(0.2, 0.9), wind: between(0.2, 0.8) }
}

export function randomSeed() {
    const alphabet = "abcdefghjkmnpqrstuvwxyz23456789"
    return Array.from(crypto.getRandomValues(new Uint8Array(8)), (b) => alphabet[b % alphabet.length]).join("")
}

/*
 * World code: TF12.<seed>.<params>[.<bodies>]  (TF1: before water and exact land share; TF2: before plates and geology; TF3: before the climate; TF4: before the ocean currents; TF5: on the flat map, before the sphere; TF6: places too coarse for large planets; TF7: before the plates had a history; TF8: before the energy-balance climate; TF9: before stars had classes; TF10: before the code carried the system)
 *   seed   — base64url of the UTF-8 seed
 *   params — base64url: one byte each for the size and the star's class, mass,
 *            metallicity, age and wind, the system's style (src/terragen/code.cpp), then one
 *            per field (fixed order, quantized); omitted when all default
 * The version prefix means a code never silently changes meaning.
 */
const VERSION = "TF19"

type Field = { key: Exclude<keyof WorldConfig, "seed" | "size" | "star" | "starMass" | "metallicity" | "age" | "wind" | "companion" | "style" | "bodies">; min: number; max: number }
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

const AGE = { least: 0.002, usual: 4.5, most: 13 }
const SOLAR_METAL = 170 // the byte for the Sun's metallicity: exactly 0

/** What a body's own land stores: the fields before tilt, as SystemSpec::read and LAND_FIELDS take them. */
const LAND = FIELDS.slice(0, 10).map((f) => {
    if (f.key === "tilt" || f.key === "year") throw new Error("a body's land has no tilt or year")
    return { ...f, key: f.key }
})

const quantize = (f: Field, v: number) => Math.round(((v - f.min) / (f.max - f.min)) * 255)
const unquantize = (f: Field, b: number) => f.min + (b / 255) * (f.max - f.min)

/** The system's bodies as src/terragen/system.cpp SystemSpec::read takes them. */
function bodyBytes(bodies: readonly SystemBodyWire[]) {
    const out: number[] = [bodies.length]
    const text = (t: string) => { const b = new TextEncoder().encode(t); out.push(b.length, ...b) }
    const word = (v: number) => { const w = Math.round(v * 8000) & 0xffff; out.push(w & 255, w >> 8) }
    const unit = (v: number) => Math.round(Math.min(1, Math.max(0, v)) * 255)
    for (const b of bodies) {
        text(b.id)
        text(b.name)
        for (let i = 1; i < 7; i += 2) out.push(parseInt(b.color.slice(i, i + 2), 16))
        out.push(b.parent === null ? 255 : bodies.findIndex((p) => p.id === b.parent), (b.life ? 1 : 0) | (b.playable ? 2 : 0) | (b.land ? 4 : 0) | SPIN_WIRES.indexOf(b.spin) << 3)
        word(Math.log10(b.orbit))
        word(b.mass)
        word(Math.log10(b.day))
        out.push(unit(b.eccentricity), unit(b.formed), unit(b.volatiles), unit(b.phase), Math.round(b.inclination) & 255, Math.round(b.tilt * 2))
        if (b.land) for (const f of LAND) out.push(quantize(f, b.land[f.key]))
    }
    return new Uint8Array(out)
}

export function encodeWorldCode(config: WorldConfig): string {
    const seed = b64.encode(new TextEncoder().encode(config.seed))
    const stars = [spannedByte(starClass(config.star).mass, config.starMass), Math.round(config.metallicity * SOLAR_METAL) + SOLAR_METAL,
        spannedByte(AGE, config.age), Math.min(255, Math.round(config.wind * 256))]
    const c = config.companion
    const second = c.kind === "none" ? [] : [COMPANION_KINDS.indexOf(c.kind), STAR_CLASSES.findIndex((s) => s.id === c.class), spannedByte(starClass(c.class).mass, c.mass), separationByte(c.kind, c.separation)]
    const atDefaults = !second.length && !config.bodies && config.style === DEFAULTS.style && config.size === DEFAULTS.size && config.star === DEFAULTS.star && stars.join() === "128,170,128,128" &&
        FIELDS.every((f) => quantize(f, config[f.key]) === quantize(f, DEFAULTS[f.key]))
    if (atDefaults) return `${VERSION}.${seed}`
    const size = SIZES.findIndex((s) => s === config.size)
    const starIndex = STAR_CLASSES.findIndex((s) => s.id === config.star)
    if (size < 0 || starIndex < 0) throw new Error("world config has a size or star this build doesn't know")
    const bytes = new Uint8Array([size, starIndex, ...stars, SYSTEM_STYLES.indexOf(config.style), ...FIELDS.map((f) => quantize(f, config[f.key])), ...second])
    return `${VERSION}.${seed}.${b64.encode(bytes)}${config.bodies ? `.${b64.encode(bodyBytes(config.bodies))}` : ""}`
}

export class WorldCodeError extends Error {}

function readBodies(b: Uint8Array): SystemBodyWire[] {
    let at = 0
    const byte = () => {
        const v = b[at++]
        if (v === undefined) throw new WorldCodeError("This world code's system is damaged.")
        return v
    }
    const text = () => { const n = byte(); const t = new TextDecoder().decode(b.subarray(at, at + n)); at += n; return t }
    const word = () => { const lo = byte(); const w = lo | byte() << 8; return (w >= 0x8000 ? w - 0x10000 : w) / 8000 }
    const hex = () => byte().toString(16).padStart(2, "0")
    const out: SystemBodyWire[] = []
    for (let n = byte(); n > 0; n--) {
        const id = text(), name = text(), color = `#${hex()}${hex()}${hex()}`
        const parent = byte(), flags = byte()
        const spin = SPIN_WIRES[flags >> 3]
        if (!spin) throw new WorldCodeError("This world code names a spin this game doesn't have.")
        const orbit = 10 ** word(), mass = word(), day = 10 ** word()
        const eccentricity = byte() / 255, formed = byte() / 255, volatiles = byte() / 255, phase = byte() / 255
        const inclination = (byte() << 24) >> 24, tilt = byte() / 2
        let land: BodyLandWire | null = null
        if (flags & 4) {
            land = { continents: 0, land: 0, coast: 0, mountains: 0, roughness: 0, temperature: 0, humidity: 0, rivers: 0, caves: 0, ores: 0 }
            for (const f of LAND) land[f.key] = unquantize(f, byte())
            land.continents = Math.round(land.continents)
        }
        out.push({ id, name, color, phase, orbit, mass, day, eccentricity, formed, volatiles, inclination, tilt, land, spin,
            parent: parent === 255 ? null : out[parent]!.id, life: !!(flags & 1), playable: !!(flags & 2) })
    }
    return out
}

export function decodeWorldCode(code: string): WorldConfig {
    const [version, seedPart, params, bodies] = code.trim().split(".")
    if (version === "TF1" || version === "TF2" || version === "TF3" || version === "TF4" || version === "TF5" || version === "TF6" || version === "TF7" || version === "TF8" || version === "TF9" || version === "TF10" || version === "TF11" || version === "TF12" || version === "TF13" || version === "TF14" || version === "TF15") throw new WorldCodeError(`This world code is from an older build (${version}) — its worlds can't be generated any more.`)
    if (version !== VERSION) throw new WorldCodeError(`Unknown world code version "${version ?? ""}" — this game reads ${VERSION}.`)
    if (!seedPart) throw new WorldCodeError("This world code has no seed.")
    const config: WorldConfig = { ...DEFAULTS, companion: Companion(), seed: new TextDecoder().decode(b64.decode(seedPart)) }
    if (params === undefined) return config

    const bytes = b64.decode(params)
    if (bytes.length !== FIELDS.length + 7 && bytes.length !== FIELDS.length + 11) throw new WorldCodeError("This world code is damaged, or from an older build.")
    const size = SIZES[bytes[0]!]
    const starType = STAR_CLASSES[bytes[1]!]
    if (size === undefined || starType === undefined) throw new WorldCodeError("This world code names a world size or star this game doesn't have.")
    config.size = size
    config.star = starType.id
    config.starMass = spanned(starType.mass, bytes[2]!)
    config.metallicity = (bytes[3]! - SOLAR_METAL) / SOLAR_METAL
    config.age = spanned(AGE, bytes[4]!)
    config.wind = bytes[5]! / 256
    const style = SYSTEM_STYLES[bytes[6]!]
    if (style === undefined) throw new WorldCodeError("This world code names a kind of system this game doesn't have.")
    config.style = style
    if (bodies !== undefined) config.bodies = readBodies(b64.decode(bodies))
    FIELDS.forEach((f, i) => (config[f.key] = unquantize(f, bytes[i + 7]!)))
    if (bytes.length === FIELDS.length + 11) {
        const kind = COMPANION_KINDS[bytes[FIELDS.length + 7]!]
        const other = STAR_CLASSES[bytes[FIELDS.length + 8]!]
        if (!kind || kind === "none" || !other) throw new WorldCodeError("This world code names a companion star this game doesn't have.")
        config.companion = { kind, class: other.id, mass: spanned(other.mass, bytes[FIELDS.length + 9]!), separation: separationOf(kind, bytes[FIELDS.length + 10]!) }
    }
    config.continents = Math.round(config.continents)
    config.tilt = Math.round(config.tilt)
    config.year = Math.round(config.year)
    return config
}
