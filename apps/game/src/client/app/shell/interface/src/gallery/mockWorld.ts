// The JavaScript stand-in for terragen, for the gallery's mock engine. Same
// inputs and world shape as the engine's generator, not the same numbers.

import { star, type WorldConfig } from "../worldgen/config"
import type { PreviewWire } from "../worldgen/fields"
import type { WeatherWire } from "../worldgen/weather"
import { declination, insolation } from "../worldgen/sun"
import { seedNumber, torusFbm } from "./noise"

const BIOMES = ["deep", "ocean", "seaice", "beach", "desert", "savanna", "grassland", "forest", "jungle", "taiga", "tundra", "rock", "snow", "monsoon", "mediterranean"] as const
type Biome = (typeof BIOMES)[number]

const POLAR_EDGE = 0.035

// in chunks: spreading a whole field into one call can exceed the argument limit
function b64(bytes: Uint8Array) {
    let text = ""
    for (let i = 0; i < bytes.length; i += 8192) text += String.fromCharCode(...bytes.subarray(i, i + 8192))
    return btoa(text)
}

export function mockPreview(config: WorldConfig, date: number | null, resolution: number): PreviewWire {
    const seed = seedNumber(config.seed)
    const octaves = 3 + Math.round(config.roughness * 3)
    const persistence = 0.35 + config.roughness * 0.25
    // no date: the engine shows the yearly mean; the mock approximates with an equinox
    const decl = declination(config.tilt, date ?? 0)
    const n = resolution * resolution
    const elevation = new Int16Array(n), heats = new Int8Array(n), wets = new Int8Array(n), biomes = new Uint8Array(n)
    const seasons = new Int8Array(n), fertility = new Uint8Array(n), soil = new Uint8Array(4 * n)
    const q = (v: number, s: number, lo: number, hi: number) => Math.max(lo, Math.min(hi, Math.trunc(v * s)))

    for (let y = 0; y < resolution; y++) {
        const v = (y + 0.5) / resolution
        const latitude = (0.5 - v) * Math.PI // north positive
        // sunlight on this date, plus a share of the yearly mean (land and sea store heat)
        const sun = insolation(latitude, decl) * 0.6 + insolation(latitude, 0) * 0.4
        const polar = v < POLAR_EDGE || v > 1 - POLAR_EDGE
        for (let x = 0; x < resolution; x++) {
            const u = x / resolution
            const continent = torusFbm(u, v, seed, config.continents * 1.3, octaves, persistence)
                + torusFbm(u, v, seed + 3, config.continents * 9, 3, 0.5) * config.coast * 0.22
            const ridge = 1 - Math.abs(torusFbm(u, v, seed + 7, config.continents * 4, octaves, 0.5))
            const base = continent + (config.land - 0.5) * 0.9
            const inland = Math.max(0, Math.min(1, base / 0.25))
            const e = base < 0 ? base : base * 0.6 + ridge * ridge * config.mountains * inland

            const heat = (sun - 0.55) * 2.2 + config.temperature * 0.6
                + torusFbm(u, v, seed + 13, 3, 3, 0.5) * 0.2 - Math.max(0, e) * 0.9
            const wet = torusFbm(u, v, seed + 29, 3, 3, 0.5) + config.humidity * 0.6

            const i = y * resolution + x
            elevation[i] = q(e, 10000, -32767, 32767)
            heats[i] = q(heat, 100, -127, 127)
            wets[i] = q(wet, 100, -127, 127)
            // the stand-in's seasons by latitude alone: summer rain at 10–25°, winter rain at 30–45°
            const lat = Math.abs(latitude) * 180 / Math.PI
            seasons[i] = q(lat > 10 && lat < 25 ? 0.7 : lat > 30 && lat < 45 ? -0.5 : 0, 100, -127, 127)
            fertility[i] = e < 0 ? 0 : Math.round(Math.max(0, Math.min(1, 0.5 + torusFbm(u, v, seed + 51, 4, 3, 0.5))) * 255)
            // the stand-in's soil: sandy to clayey across the land, a little stonier high up
            if (e >= 0) {
                const sand = Math.max(0, Math.min(0.8, 0.4 + torusFbm(u, v, seed + 57, 4, 3, 0.5)))
                soil[i * 4] = Math.round(sand * 255)
                soil[i * 4 + 1] = Math.round((0.8 - sand) * 0.5 * 255)
                soil[i * 4 + 2] = 8
                soil[i * 4 + 3] = Math.round(Math.min(1, Math.max(0, e)) * 200)
            }
            biomes[i] = BIOMES.indexOf(polar ? (e < 0 ? "seaice" : "snow") : classify(e, heat, wet))
        }
    }
    return {
        resolution: resolution,
        elevation: b64(new Uint8Array(elevation.buffer)),
        heat: b64(new Uint8Array(heats.buffer)),
        wet: b64(new Uint8Array(wets.buffer)),
        season: b64(new Uint8Array(seasons.buffer)),
        fertility: b64(fertility),
        soil: b64(soil),
        current: b64(new Uint8Array(n)), // the stand-in has no ocean currents
        biome: b64(biomes),
        biomes: [...BIOMES],
        // the JS stand-in has no water simulation: no lakes, no rivers
        lake: b64(new Uint8Array(n)),
        rivers: "",
        // nor any geology: one plate, all shield, no volcanoes
        province: b64(new Uint8Array(n)),
        plate: b64(new Uint8Array(n)),
        boundary: b64(new Uint8Array(n)),
        volcanoes: "",
        history: [],
    }
}


function classify(e: number, heat: number, wet: number): Biome {
    if (e < 0) return heat < -0.75 ? "seaice" : e < -0.18 ? "deep" : "ocean"
    if (e < 0.03) return heat < -0.6 ? "snow" : "beach"
    if (heat < -0.6) return "snow"
    if (e > 0.5) return heat < -0.1 ? "snow" : "rock"
    if (heat < -0.3) return wet > 0 ? "taiga" : "tundra"
    if (heat > 0.35) return wet > 0.15 ? "jungle" : wet < -0.15 ? "desert" : "savanna"
    return wet > 0.05 ? "forest" : "grassland"
}


/**
 * A stand-in year of weather for the gallery: the ITCZ's rain band following
 * the sun, storm-track cloud at 50°, a low drifting east through the year.
 */
export function mockWeather(snapshots: number): WeatherWire {
    const r = 64, n = r * r
    const rain = new Uint8Array(n * snapshots), cloud = new Uint8Array(n * snapshots)
    const temperature = new Int8Array(n * snapshots), east = new Int8Array(n * snapshots)
    const north = new Int8Array(n * snapshots), pressure = new Int8Array(n * snapshots)
    for (let s = 0; s < snapshots; s++) {
        const itcz = 8 * Math.sin((2 * Math.PI * s) / snapshots)
        const lowX = (s / snapshots) * r * 3
        for (let y = 0; y < r; y++) {
            const lat = 90 - ((y + 0.5) * 180) / r
            for (let x = 0; x < r; x++) {
                const i = s * n + y * r + x
                const tropical = Math.exp(-(((lat - itcz) / 7) ** 2))
                const dx = ((x - lowX) % r + r) % r - r / 2, dy = y - 14
                const low = Math.exp(-(dx * dx + dy * dy) / 30)
                const track = Math.exp(-(((Math.abs(lat) - 50) / 8) ** 2))
                rain[i] = Math.min(255, Math.round(Math.sqrt(tropical * 0.5 + low * 0.8 + track * 0.1) * 100))
                cloud[i] = Math.min(255, Math.round((tropical * 0.8 + low + track * 0.5) * 255))
                temperature[i] = Math.round(28 - Math.abs(lat) * 0.6)
                east[i] = Math.round(Math.abs(lat) < 30 ? -6 : 8)
                north[i] = 0
                pressure[i] = Math.round(-low * 20 + (Math.abs(lat) > 20 && Math.abs(lat) < 35 ? 6 : 0))
            }
        }
    }
    const bytes = (a: Uint8Array | Int8Array) => b64(new Uint8Array(a.buffer))
    return { resolution: r, snapshots: snapshots, rain: bytes(rain), cloud: bytes(cloud), temperature: bytes(temperature),
        east: bytes(east), north: bytes(north), pressure: bytes(pressure) }
}
