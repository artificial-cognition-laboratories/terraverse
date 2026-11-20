// The world's weather as the create screen shows it: a settled year from the
// engine's atmosphere (weather.preview, src/shell/menu.cpp), one map per
// snapshot date, painted over the terrain as cloud, rain, snow and isobars.

import { shallowRef } from "vue"
import type { BridgeHandle } from "../bridge"
import { encodeWorldCode, type WorldConfig } from "./config"
import { paint, type Fields } from "./fields"

/** One date's weather, `resolution` per side, row-major from the north. */
export type WeatherMap = {
    resolution: number
    rain: Float32Array        // mm/h
    cloud: Float32Array       // 0..1
    temperature: Float32Array // °C
    east: Float32Array        // m/s
    north: Float32Array       // m/s
    pressure: Float32Array    // hPa from its latitude's mean: storms, highs and lows
}

/** What weather.preview sends: every snapshot in turn, per field, as base64. */
export type WeatherWire = {
    resolution: number
    snapshots: number
    rain: string        // uint8: √(mm/h) × 100
    cloud: string       // uint8 × 255
    temperature: string // int8 °C
    east: string        // int8 m/s
    north: string       // int8 m/s
    pressure: string    // int8 hPa from its latitude's mean
}

const SNAPSHOTS = 24

function bytesOf(b64: string) {
    return Uint8Array.from(atob(b64), (c) => c.charCodeAt(0))
}

export function decodeWeather(wire: WeatherWire): WeatherMap[] {
    const n = wire.resolution * wire.resolution
    const rain = bytesOf(wire.rain), cloud = bytesOf(wire.cloud)
    const temperature = new Int8Array(bytesOf(wire.temperature).buffer), east = new Int8Array(bytesOf(wire.east).buffer)
    const north = new Int8Array(bytesOf(wire.north).buffer), pressure = new Int8Array(bytesOf(wire.pressure).buffer)
    for (const f of [rain, cloud, temperature, east, north, pressure])
        if (f.length !== n * wire.snapshots) throw new Error(`weather fields don't match ${wire.snapshots} × ${wire.resolution}²`)
    const out: WeatherMap[] = []
    for (let s = 0; s < wire.snapshots; s++) {
        const part = (from: Uint8Array | Int8Array, scale: (v: number) => number) => Float32Array.from(from.subarray(s * n, (s + 1) * n), scale)
        out.push({
            resolution: wire.resolution,
            rain: part(rain, (v) => (v / 100) ** 2),
            cloud: part(cloud, (v) => v / 255),
            temperature: part(temperature, (v) => v),
            east: part(east, (v) => v),
            north: part(north, (v) => v),
            pressure: part(pressure, (v) => v),
        })
    }
    return out
}

/**
 * A world's settled year of weather, asked for once per world code (about a
 * second in the engine) and kept; `at(date)` picks the snapshot for a date.
 */
export function WeatherPreview(opts: { bridge: BridgeHandle }) {
    const year = shallowRef<WeatherMap[] | null>(null)
    const problem = shallowRef<string | null>(null)
    let current = "", busy = false, queued: string | null = null

    async function run(code: string) {
        busy = true
        try {
            const wire = await opts.bridge.send("weather.preview", { code: code, snapshots: SNAPSHOTS })
            if (code === current) {
                year.value = decodeWeather(wire)
                problem.value = null
            }
        } catch (err) {
            console.error(err)
            problem.value = err instanceof Error ? err.message : String(err)
        }
        busy = false
        if (queued) {
            const q = queued
            queued = null
            run(q)
        }
    }

    return {
        year: year,
        problem: problem,
        request(config: WorldConfig) {
            const code = encodeWorldCode(config)
            if (code === current) return
            current = code
            year.value = null
            if (busy) queued = code
            else run(code)
        },
        at(date: number): WeatherMap | null {
            const y = year.value
            return y ? y[Math.floor(((date % 1) + 1) % 1 * y.length) % y.length]! : null
        },
    }
}

export type WeatherPreviewHandle = ReturnType<typeof WeatherPreview>

/** The terrain in its own colours with the weather over it: cloud, rain (blue) or snow (white), and isobars every 4 hPa round the storms. */
export function paintWeather(fields: Fields, weather: WeatherMap): ImageData {
    const image = paint(fields, "terrain")
    const r = fields.resolution, w = weather.resolution
    const at = (f: Float32Array, x: number, y: number) => {
        const gx = (x + 0.5) / r * w - 0.5, gy = Math.max(0, Math.min(w - 1.001, (y + 0.5) / r * w - 0.5))
        const x0 = Math.floor(gx), y0 = Math.floor(gy), fx = gx - x0, fy = gy - y0
        const cell = (cx: number, cy: number) => f[cy * w + (((cx % w) + w) % w)]!
        return (cell(x0, y0) * (1 - fx) + cell(x0 + 1, y0) * fx) * (1 - fy) + (cell(x0, y0 + 1) * (1 - fx) + cell(x0 + 1, y0 + 1) * fx) * fy
    }
    const band = (x: number, y: number) => Math.floor(at(weather.pressure, x, y) / 4)
    for (let y = 0; y < r; y++)
        for (let x = 0; x < r; x++) {
            const o = (y * r + x) * 4
            let c: [number, number, number] = [image.data[o]!, image.data[o + 1]!, image.data[o + 2]!]
            // thin cloud lets the ground through; only thick cloud whitens it
            const cloud = Math.max(0, at(weather.cloud, x, y) - 0.35) / 0.65 * 0.75
            c = [c[0] + (235 - c[0]) * cloud, c[1] + (238 - c[1]) * cloud, c[2] + (242 - c[2]) * cloud]
            const rain = Math.min(1, Math.sqrt(at(weather.rain, x, y)) / 1.2)
            if (rain > 0.08) {
                const tint: [number, number, number] = at(weather.temperature, x, y) < 0 ? [250, 252, 255] : [60, 120, 230]
                c = [c[0] + (tint[0] - c[0]) * rain, c[1] + (tint[1] - c[1]) * rain, c[2] + (tint[2] - c[2]) * rain]
            }
            // isobars only where the pressure departs from its latitude: round storms, not along the bands
            const p = at(weather.pressure, x, y)
            if (Math.abs(p) >= 2 && (band(x, y) !== band(x + 1, y) || band(x, y) !== band(x, y + 1))) c = [c[0] * 0.55, c[1] * 0.55, c[2] * 0.6]
            image.data[o] = c[0]
            image.data[o + 1] = c[1]
            image.data[o + 2] = c[2]
        }
    return image
}
