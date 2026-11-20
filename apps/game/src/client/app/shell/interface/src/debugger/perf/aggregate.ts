import { type Capture, firstEndingAfter } from "./capture.ts"

/** One zone name's cost over a range, as the summary shows it. */
export type ZoneRow = { name: string; calls: number; total: number; self: number; max: number; threads: string[] }

/**
 * Totals per zone name over [from, to]: time clipped to the range, and self
 * time (less what's nested directly inside, on the same thread).
 */
export function aggregate(capture: Capture, from: number, to: number): ZoneRow[] {
    const rows = new Map<string, ZoneRow>()
    const row = (name: string) => {
        let r = rows.get(name)
        if (!r) rows.set(name, (r = { name: name, calls: 0, total: 0, self: 0, max: 0, threads: [] }))
        return r
    }
    for (const lane of capture.lanes) {
        lane.rows.forEach((zones, depth) => {
            const parent = lane.rows[depth - 1]
            for (let i = firstEndingAfter(zones, from); i < zones.starts.length && zones.starts[i]! < to; i++) {
                const start = zones.starts[i]!
                const end = zones.ends[i]!
                const took = Math.min(end, to) - Math.max(start, from)
                if (took <= 0) continue // only touches the range
                const name = capture.names[zones.names[i]!]!
                const r = row(name)
                r.calls++
                r.total += took
                r.self += took
                r.max = Math.max(r.max, end - start)
                if (!r.threads.includes(lane.thread)) r.threads.push(lane.thread)
                // its parent is the zone one depth up that holds its start
                if (!parent) continue
                const p = firstEndingAfter(parent, start)
                if (p < parent.starts.length && parent.starts[p]! <= start) row(capture.names[parent.names[p]!]!).self -= took
            }
        })
    }
    return [...rows.values()].sort((a, b) => b.self - a.self)
}
