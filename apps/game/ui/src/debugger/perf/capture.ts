import type { PerfCapture, PerfFrame } from "../../bridge/index.ts"

/** One depth of one thread: its zones by start, in typed arrays so a wide view is cheap. */
export type Row = { names: Uint32Array; starts: Float64Array; ends: Float64Array }

export type Lane = { thread: string; since: number; rows: Row[] }

/** A capture as the flame graph reads it: every thread's zones, laid out once. */
export type Capture = {
    id: number
    from: number
    to: number
    names: string[]
    lanes: Lane[]
    frames: PerfFrame[]
}

/**
 * Rails in a fixed order, so they never shuffle between captures: Main, the
 * server, then the rest by name with numbers counted as numbers ("Emerge-2"
 * before "Emerge-10").
 */
const FIRST = ["Main", "Server"]
export function threadOrder(a: string, b: string) {
    const ia = FIRST.indexOf(a)
    const ib = FIRST.indexOf(b)
    if (ia >= 0 || ib >= 0) return (ia < 0 ? FIRST.length : ia) - (ib < 0 ? FIRST.length : ib)
    return a.localeCompare(b, undefined, { numeric: true })
}

export function buildCapture(id: number, from: number, to: number, data: PerfCapture, frames: PerfFrame[]): Capture {
    if (data.names.length === 0 && data.tracks.every((t) => t.spans.length === 0)) {
        // nothing at all is a real answer (a range older than every ring); keep the rails
    }
    const lanes = data.tracks.map((track): Lane => {
        if (track.spans.length % 4 !== 0) throw new Error(`perf: ${track.thread}'s spans aren't in fours`)
        const byDepth: number[][] = []
        for (let i = 0; i < track.spans.length; i += 4) {
            const depth = track.spans[i + 1]!
            ;(byDepth[depth] ??= []).push(i)
        }
        const rows = byDepth.map((at = []) => {
            at.sort((x, y) => track.spans[x + 2]! - track.spans[y + 2]!)
            const row: Row = { names: new Uint32Array(at.length), starts: new Float64Array(at.length), ends: new Float64Array(at.length) }
            at.forEach((i, k) => {
                const name = track.spans[i]!
                if (name >= data.names.length) throw new Error(`perf: zone name ${name} wasn't sent`)
                row.names[k] = name
                row.starts[k] = track.spans[i + 2]!
                row.ends[k] = track.spans[i + 3]!
            })
            return row
        })
        return { thread: track.thread, since: track.since, rows: rows }
    })
    lanes.sort((a, b) => threadOrder(a.thread, b.thread))
    return { id: id, from: from, to: to, names: data.names, lanes: lanes, frames: frames }
}

/** The first zone in a row ending at or after `ms` (a zone's start orders its end too, within one depth). */
export function firstEndingAfter(row: Row, ms: number) {
    let lo = 0
    let hi = row.ends.length
    while (lo < hi) {
        const mid = (lo + hi) >> 1
        if (row.ends[mid]! < ms) lo = mid + 1
        else hi = mid
    }
    return lo
}

/** The zone in a row under `ms`, give or take `slack`, or -1. */
export function zoneAt(row: Row, ms: number, slack: number) {
    const i = firstEndingAfter(row, ms - slack)
    return i < row.starts.length && row.starts[i]! <= ms + slack ? i : -1
}
