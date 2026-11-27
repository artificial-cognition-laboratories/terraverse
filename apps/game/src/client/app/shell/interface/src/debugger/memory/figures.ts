import type { Sampled } from "./store.ts"

/**
 * The Memory tab's figures, read from the samples. A band is one part of a
 * whole (a stacked chart's layer); a figure is one number through time (the
 * table, a system's own chart). Systems, process roles and owners come and
 * go, so they're found in the samples, not listed.
 */
export type Band = { name: string; color: string; value: (s: Sampled) => number }
export type Figure = { name: string; kind: "machine" | "process" | "heap" | "gpu" | "system"; value: (s: Sampled) => number; count?: (s: Sampled) => number }

/** An owner's system: its name up to the colon ("Far view: tiles #3" → "Far view"). */
export const systemOf = (owner: string) => owner.split(":")[0]!.trim()

const sum = <T>(list: readonly T[], of: (t: T) => number) => list.reduce((n, t) => n + of(t), 0)
const isChromium = (role: string) => role.startsWith("chromium")
/** What the owners in RAM claim: the rest of the heap is unaccounted. */
const claimed = (s: Sampled) => sum(s.owners, (o) => (o.gpu ? 0 : o.bytes))
const systemBytes = (system: string, gpu: boolean | null) => (s: Sampled) =>
    sum(s.owners, (o) => (systemOf(o.name) === system && (gpu === null || o.gpu === gpu) ? o.bytes : 0))

/** Fixed colours for the wholes' standing parts; systems take the zone palette's. */
export const COLORS = { engine: "#4f9dff", chromium: "#e5a33b", sidecar: "#c678dd", unaccounted: "#5a5963", others: "#3a4656", free: "#22303d", swap: "#e0524f" } as const

/** RAM: the game's processes, stacked, out of the machine's whole. */
export function ramBands(): Band[] {
    return [
        { name: "engine", color: COLORS.engine, value: (s) => sum(s.processes, (p) => (p.role === "engine" ? p.private : 0)) },
        { name: "chromium", color: COLORS.chromium, value: (s) => sum(s.processes, (p) => (isChromium(p.role) ? p.private : 0)) },
        { name: "terra & others", color: COLORS.sidecar, value: (s) => sum(s.processes, (p) => (p.role !== "engine" && !isChromium(p.role) ? p.private : 0)) },
    ]
}
/** Everything on the machine that's in use, the game included: the line over the game's bands. */
export const machineUsed = (s: Sampled) => s.total - s.available
/** The game's own swapped memory: what the chart hangs below its floor. */
export const gameSwap = (s: Sampled) => sum(s.processes, (p) => p.swap)

/** The engine's heap: each system's owners, stacked, the unclaimed rest on top. */
export function heapBands(systems: readonly string[], color: (name: string) => string): Band[] {
    return [
        ...systems.map((system): Band => ({ name: system, color: color(system), value: systemBytes(system, false) })),
        { name: "unaccounted", color: COLORS.unaccounted, value: (s) => Math.max(0, s.heap - claimed(s)) },
    ]
}

/** The graphics card: the game's processes on it, everyone else's, and what's free. */
export function gpuBands(): Band[] {
    return [
        { name: "engine", color: COLORS.engine, value: (s) => sum(s.processes, (p) => (p.role === "engine" ? p.gpu : 0)) },
        { name: "chromium", color: COLORS.chromium, value: (s) => sum(s.processes, (p) => (isChromium(p.role) ? p.gpu : 0)) },
        { name: "other apps", color: COLORS.others, value: (s) => s.gpuOthers },
        { name: "free", color: COLORS.free, value: (s) => s.gpuFree },
    ]
}

/** Every system that's said what it holds, biggest now first. */
export function systemsOf(samples: readonly Sampled[]): string[] {
    const last = samples[samples.length - 1]
    const seen = new Set<string>()
    for (const s of samples) for (const o of s.owners) seen.add(systemOf(o.name))
    const now = (system: string) => (last ? systemBytes(system, null)(last) : 0)
    return [...seen].sort((a, b) => now(b) - now(a))
}

/** A system's whole (RAM and card), and how many things its owners hold. */
export function systemFigure(system: string): Figure {
    return {
        name: system, kind: "system", value: systemBytes(system, null),
        count: (s) => sum(s.owners, (o) => (systemOf(o.name) === system ? o.count : 0)),
    }
}

/** Every figure the table lists: the wholes, each process role, each system. */
export function figuresOf(samples: readonly Sampled[]): Figure[] {
    const roles = new Set<string>()
    for (const s of samples) for (const p of s.processes) roles.add(p.role)
    return [
        { name: "the game (private)", kind: "process", value: (s) => sum(s.processes, (p) => p.private) },
        ...[...roles].sort().map((r): Figure => ({ name: r, kind: "process", value: (s) => sum(s.processes, (p) => (p.role === r ? p.private : 0)) })),
        { name: "heap in use", kind: "heap", value: (s) => s.heap },
        { name: "unaccounted", kind: "heap", value: (s) => s.heap - claimed(s) },
        ...systemsOf(samples).map(systemFigure),
        { name: "the game on the card", kind: "gpu", value: (s) => sum(s.processes, (p) => p.gpu) },
        { name: "machine in use", kind: "machine", value: machineUsed },
        { name: "machine swap used", kind: "machine", value: (s) => s.swapUsed },
        { name: "the game's swap", kind: "machine", value: gameSwap },
    ]
}

/** A figure through the samples: now, its peak, its start, and how fast it's moving (bytes a minute, least squares). */
export type Reading = { now: number; peak: number; start: number; perMinute: number; count: number }

export function readingOf(f: Figure, samples: readonly Sampled[], sinceMs: number): Reading {
    let now = Number.NaN
    let start = Number.NaN
    let peak = Number.NaN
    let n = 0, st = 0, sv = 0, stt = 0, stv = 0
    for (const s of samples) {
        const v = f.value(s)
        if (Number.isNaN(v)) continue
        if (Number.isNaN(start)) start = v
        now = v
        peak = Number.isNaN(peak) ? v : Math.max(peak, v)
        if (s.t < sinceMs) continue
        const m = s.t / 60_000
        n++
        st += m
        sv += v
        stt += m * m
        stv += m * v
    }
    const d = n * stt - st * st
    const last = samples[samples.length - 1]
    return { now: now, peak: peak, start: start, perMinute: n < 2 || d === 0 ? 0 : (n * stv - st * sv) / d,
        count: f.count && last ? f.count(last) : Number.NaN }
}
