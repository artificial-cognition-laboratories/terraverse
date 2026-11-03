// The world preview as the page holds it: fields decoded from the engine's
// worldgen.preview answer (src/terragen/preview.h), and how to paint them.

/** Raw fields, `resolution` per side, row-major; row 0 is the north edge. */
export type Fields = {
    resolution: number
    elevation: Float32Array // −1..~1.2, 0 = sea level
    heat: Float32Array      // ~−1 frozen .. 1 scorching
    wet: Float32Array       // ~−1 arid .. 1 soaked: log₂ of the year's rain (relative to the land's median), halved
    season: Float32Array    // when the rain falls: 1 all in summer .. −1 all in winter
    fertility: Float32Array // 0..1: what the ground supplies, less what rain washes out (0 at sea)
    current: Float32Array   // what the ocean currents add to the heat (warm +, cold −), coasts included
    biome: Uint8Array       // index into `biomes`
    biomes: string[]        // the engine's biome names, in index order
    lake: Uint8Array        // 1 where a lake stands
    rivers: River[]         // between the coarse grid's cells
    province: Uint8Array    // index into PROVINCES
    plate: Uint8Array       // which plate
    boundary: Uint8Array    // 0, or the kind of plate edge running through: 1 converging, 2 spreading, 3 sliding
    volcanoes: Volcano[]
    history: LandMoment[]
}

/** Something the plates did, in million years before today (terragen/history.cpp). */
export type LandMoment = { ago: number; text: string }

/** A volcano's vent, as a share of the world across (u, east) and down (v, south). */
export type Volcano = { u: number; v: number; active: boolean }

/** The engine's geological provinces, in index order (src/terragen/geology.cpp: PROVINCE_NAMES). */
export const PROVINCES = ["shield", "orogen", "arc", "rift", "basin", "volcanic", "ocean floor", "highland", "mare"] as const

/** A river segment: from a grid cell's centre to the one it drains into, as shares of the map across (u) and down (v); strength 1..255 (√flow over threshold ×16). */
export type River = { u0: number; v0: number; u1: number; v1: number; strength: number }

/** What worldgen.preview sends: the same fields, quantized, as base64. */
export type PreviewWire = {
    resolution: number
    elevation: string // int16 × 10000
    heat: string      // int8 × 100
    wet: string       // int8 × 100
    season: string    // int8 × 100
    fertility: string // uint8 × 255
    current: string   // int8 × 100
    biome: string     // uint8
    biomes: string[]
    lake: string      // uint8
    rivers: string    // 9 bytes per segment: u0, v0, u1, v1 (uint16 LE), strength
    province: string  // uint8
    plate: string     // uint8
    boundary: string  // uint8
    volcanoes: string // 5 bytes each: u (uint16 LE), v (uint16 LE), active
    history: LandMoment[] // what happened to the plates, oldest first
}

function bytesOf(b64: string) {
    return Uint8Array.from(atob(b64), (c) => c.charCodeAt(0))
}

export function decodePreview(wire: PreviewWire): Fields {
    const n = wire.resolution * wire.resolution
    const elevation = new Int16Array(bytesOf(wire.elevation).buffer)
    const heat = new Int8Array(bytesOf(wire.heat).buffer)
    const wet = new Int8Array(bytesOf(wire.wet).buffer)
    const season = new Int8Array(bytesOf(wire.season).buffer), fertility = bytesOf(wire.fertility)
    const current = new Int8Array(bytesOf(wire.current).buffer)
    const biome = bytesOf(wire.biome)
    const lake = bytesOf(wire.lake)
    const riverBytes = bytesOf(wire.rivers)
    const province = bytesOf(wire.province), plate = bytesOf(wire.plate), boundary = bytesOf(wire.boundary)
    const volcanoBytes = bytesOf(wire.volcanoes)
    if (elevation.length !== n || heat.length !== n || wet.length !== n || season.length !== n || fertility.length !== n || current.length !== n ||
        biome.length !== n || lake.length !== n || riverBytes.length % 9 ||
        province.length !== n || plate.length !== n || boundary.length !== n || volcanoBytes.length % 5)
        throw new Error(`preview fields don't match resolution ${wire.resolution}`)
    const share = (b: Uint8Array, i: number) => (b[i]! | (b[i + 1]! << 8)) / 65535
    const rivers: River[] = []
    for (let i = 0; i < riverBytes.length; i += 9)
        rivers.push({ u0: share(riverBytes, i), v0: share(riverBytes, i + 2), u1: share(riverBytes, i + 4), v1: share(riverBytes, i + 6), strength: riverBytes[i + 8]! })
    const volcanoes: Volcano[] = []
    for (let i = 0; i < volcanoBytes.length; i += 5)
        volcanoes.push({ u: (volcanoBytes[i]! | (volcanoBytes[i + 1]! << 8)) / 65535, v: (volcanoBytes[i + 2]! | (volcanoBytes[i + 3]! << 8)) / 65535,
            active: volcanoBytes[i + 4] === 1 })
    return {
        resolution: wire.resolution,
        elevation: Float32Array.from(elevation, (v) => v / 10000),
        heat: Float32Array.from(heat, (v) => v / 100),
        wet: Float32Array.from(wet, (v) => v / 100),
        season: Float32Array.from(season, (v) => v / 100),
        fertility: Float32Array.from(fertility, (v) => v / 255),
        current: Float32Array.from(current, (v) => v / 100),
        biome: biome,
        biomes: wire.biomes,
        lake: lake,
        rivers: rivers,
        province: province,
        plate: plate,
        boundary: boundary,
        volcanoes: volcanoes,
        history: wire.history,
    }
}

/** Colours for the engine's biome names. A name missing here is a contract mismatch. */
export const BIOME_COLORS: Record<string, readonly [number, number, number]> = {
    deep: [18, 52, 96],
    ocean: [30, 84, 140],
    seaice: [200, 220, 235],
    beach: [214, 200, 140],
    desert: [222, 190, 118],
    savanna: [176, 170, 84],
    grassland: [104, 160, 72],
    forest: [52, 116, 56],
    jungle: [28, 120, 60],
    taiga: [60, 100, 80],
    tundra: [150, 160, 140],
    rock: [120, 116, 110],
    snow: [236, 240, 244],
    monsoon: [96, 140, 40],
    mediterranean: [160, 150, 96],
    regolith: [168, 166, 160],
    dust: [164, 83, 44],
    sulfur: [216, 194, 60],
    mare: [84, 84, 88],
}

/** What the map and globe colour by. */
export const OVERLAYS = [
    { id: "terrain", label: "Terrain" },
    { id: "biomes", label: "Biomes" },
    { id: "elevation", label: "Elevation" },
    { id: "temperature", label: "Temperature" },
    { id: "moisture", label: "Rainfall" },
    { id: "seasons", label: "Rain seasons" },
    { id: "fertility", label: "Fertility" },
    { id: "currents", label: "Ocean currents" },
    { id: "plates", label: "Plates" },
    { id: "geology", label: "Geology" },
    { id: "weather", label: "Weather" },
] as const

export type OverlayId = (typeof OVERLAYS)[number]["id"]

type Rgb = readonly [number, number, number]

function biomeColor(name: string): Rgb {
    const c = BIOME_COLORS[name]
    if (!c) throw new Error(`no colour for biome "${name}" — the engine and page disagree on biomes`)
    return c
}
const mix = (a: Rgb, b: Rgb, t: number): Rgb => [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t]
const clamp01 = (t: number) => Math.max(0, Math.min(1, t))

/** A colour ramp through evenly spaced stops. */
function ramp(stops: readonly Rgb[], t: number): Rgb {
    const s = clamp01(t) * (stops.length - 1)
    const i = Math.min(stops.length - 2, Math.floor(s))
    return mix(stops[i]!, stops[i + 1]!, s - i)
}

const HEAT_RAMP: Rgb[] = [[40, 60, 160], [90, 170, 230], [235, 235, 210], [240, 170, 70], [200, 50, 40]]
const FERTILE_RAMP: Rgb[] = [[150, 120, 90], [190, 170, 100], [110, 160, 70], [40, 120, 50]]
const WET_RAMP: Rgb[] = [[170, 120, 60], [220, 210, 150], [110, 180, 120], [40, 120, 170], [20, 60, 140]]
const LAND_RAMP: Rgb[] = [[70, 120, 60], [160, 150, 90], [130, 110, 95], [245, 245, 245]]
const SEA_RAMP: Rgb[] = [[10, 30, 70], [50, 110, 170]]
// one colour per province (PROVINCES order), and per kind of plate edge
const PROVINCE_COLORS: Rgb[] = [[150, 140, 128], [176, 84, 70], [214, 142, 62], [92, 172, 160], [126, 172, 92], [74, 52, 52], [44, 74, 132], [196, 192, 184], [70, 72, 80]]
const BOUNDARY_COLORS: Rgb[] = [[0, 0, 0], [236, 52, 44], [52, 214, 236], [236, 214, 52]]

function currentColor(current: number, sea: boolean): Rgb {
    const a = Math.max(-1, Math.min(1, current / 0.25)), base = sea ? 60 : 140
    return [base + 180 * Math.max(0, a) - 30 * Math.max(0, -a), base - 30 * Math.abs(a), base + 180 * Math.max(0, -a) - 30 * Math.max(0, a)]
}

function seasonColor(season: number, wet: number): Rgb {
    const amount = Math.max(0.35, Math.min(1, 0.6 + wet * 0.5))
    // only a strong season shows its colour: weak ones stay near grey
    const s = Math.sign(season) * Math.max(0, Math.abs(season) - 0.25) / 0.75
    return [(150 + 100 * Math.max(0, s) - 90 * Math.max(0, -s)) * amount, (150 - 70 * Math.abs(s)) * amount,
        (150 + 100 * Math.max(0, -s) - 90 * Math.max(0, s)) * amount]
}

function plateColor(plate: number, sea: boolean): Rgb {
    const k = Math.imul(plate + 1, 2654435761) >>> 0
    const t = sea ? 0.45 : 0.85
    return [(60 + ((k >>> 8) % 160)) * t, (60 + ((k >>> 16) % 160)) * t, (60 + ((k >>> 24) % 160)) * t]
}

/** Paint the fields for an overlay, with hillshading on land (light from the north-west). */
export function paint(fields: Fields, overlay: OverlayId): ImageData {
    const r = fields.resolution
    const image = new ImageData(r, r)
    const elev = (x: number, y: number) => fields.elevation[Math.min(r - 1, Math.max(0, y)) * r + ((x + r) % r)]!
    for (let y = 0; y < r; y++) {
        for (let x = 0; x < r; x++) {
            const i = y * r + x
            const e = fields.elevation[i]!
            const biome = fields.biomes[fields.biome[i]!]!
            let color: Rgb
            switch (overlay) {
                case "terrain":
                case "biomes": color = biomeColor(biome); break
                case "elevation": color = e < 0 ? ramp(SEA_RAMP, 1 + e * 1.4) : ramp(LAND_RAMP, e * 1.3); break
                case "temperature": color = ramp(HEAT_RAMP, (fields.heat[i]! + 1) / 2); break
                case "moisture": color = e < 0 ? [24, 50, 90] : ramp(WET_RAMP, (fields.wet[i]! + 1) / 2); break
                // red: rain in summer (monsoon), blue: in winter (Mediterranean), grey: all year
                case "seasons": color = e < 0 ? [24, 40, 70] : seasonColor(fields.season[i]!, fields.wet[i]!); break
                // red where the currents warm the sea and coast, blue where they chill it
                case "currents": color = currentColor(fields.current[i]!, e < 0); break
                case "fertility": color = e < 0 ? [24, 40, 70] : ramp(FERTILE_RAMP, fields.fertility[i]!); break
                case "plates": color = fields.boundary[i] ? BOUNDARY_COLORS[fields.boundary[i]!]! : plateColor(fields.plate[i]!, e < 0); break
                case "geology": color = PROVINCE_COLORS[fields.province[i]!]!; break
                case "weather": throw new Error("the weather overlay is painted by paintWeather (weather.ts)")
            }
            const shaded = overlay !== "biomes" && e > 0
            const light = shaded ? Math.max(0.55, Math.min(1.35, 1 + (elev(x - 1, y - 1) - elev(x + 1, y + 1)) * 6)) : 1
            const depth = overlay === "terrain" && e < 0 && biome !== "seaice" ? 1 + e * 1.2 : 1
            const o = i * 4
            image.data[o] = color[0] * light * depth
            image.data[o + 1] = color[1] * light * depth
            image.data[o + 2] = color[2] * light * depth
            image.data[o + 3] = 255
        }
    }
    if (WATER_OVERLAYS.has(overlay)) paintWater(fields, image)
    if (overlay === "plates" || overlay === "geology") paintVolcanoes(fields, image)
    return image
}

/** Volcanoes as small marks: white where active, grey where dormant. */
function paintVolcanoes(fields: Fields, image: ImageData) {
    const r = fields.resolution
    for (const v of fields.volcanoes) {
        const cx = Math.floor(v.u * r), cy = Math.floor(v.v * r)
        const c = v.active ? 255 : 150
        for (let dy = -1; dy <= 1; dy++)
            for (let dx = -1; dx <= 1; dx++) {
                const y = cy + dy
                if (y < 0 || y >= r) continue
                const o = (y * r + (((cx + dx) % r) + r) % r) * 4
                image.data[o] = c
                image.data[o + 1] = c * (v.active ? 0.6 : 1)
                image.data[o + 2] = c * (v.active ? 0.3 : 1)
            }
    }
}

/** Overlays that show lakes and rivers; the climate ones stay readable without them. */
const WATER_OVERLAYS = new Set<OverlayId>(["terrain", "biomes", "elevation"])
const LAKE: Rgb = [58, 140, 205]

function paintWater(fields: Fields, image: ImageData) {
    const r = fields.resolution
    const set = (x: number, y: number, c: Rgb) => {
        if (y < 0 || y >= r) return
        const o = (y * r + (((x % r) + r) % r)) * 4
        image.data[o] = c[0]
        image.data[o + 1] = c[1]
        image.data[o + 2] = c[2]
    }
    for (let i = 0; i < r * r; i++) if (fields.lake[i]) set(i % r, Math.floor(i / r), LAKE)

    // rivers: lines between cell centres, deeper blue and wider as flow grows
    for (const river of fields.rivers) {
        const x0 = river.u0 * r, y0 = river.v0 * r
        // across the east–west seam, the short way round (set() wraps x)
        let x1 = river.u1 * r
        if (x1 - x0 > r / 2) x1 -= r
        if (x0 - x1 > r / 2) x1 += r
        const y1 = river.v1 * r
        const s = Math.min(1, river.strength / 96)
        const color: Rgb = [40 - 20 * s, 120 + 20 * s, 215]
        const wide = river.strength >= 48
        const steps = Math.max(2, Math.ceil(Math.hypot(x1 - x0, y1 - y0) * 2))
        for (let k = 0; k <= steps; k++) {
            const x = Math.floor(x0 + ((x1 - x0) * k) / steps), y = Math.floor(y0 + ((y1 - y0) * k) / steps)
            set(x, y, color)
            if (wide) set(x + 1, y, color)
        }
    }
}
