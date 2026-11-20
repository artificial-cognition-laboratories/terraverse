import type { PerfFrame } from "../../bridge/index.ts"
import { categoryOf, type CategoryId } from "./categories.ts"

/** Each frame's traffic as rates over the quarter second up to it, by category and way. */
export type Rate = {
    down: Partial<Record<CategoryId, number>> // bytes/s in
    up: Partial<Record<CategoryId, number>> // bytes/s out
    downMsgs: Partial<Record<CategoryId, number>> // messages/s in
    upMsgs: Partial<Record<CategoryId, number>>
    downAll: number
    upAll: number
    byType: Map<string, number> // "0:32" (way:type) → bytes/s
}

/** A quarter second: long enough to smooth the frame-to-frame lumps, short enough that a burst shows as one. */
const WINDOW_MS = 250

/**
 * Rates for the frames given, each over the window ending at it. Frames only
 * ever arrive in order, so one pass with a window that slides along them.
 */
export function rates(frames: readonly PerfFrame[], nameOf: (dir: 0 | 1, command: number) => string): Map<number, Rate> {
    const out = new Map<number, Rate>()
    type Sum = { bytes: Map<string, number>; msgs: Map<string, number> }
    const window: { end: number; entries: [string, number, number][] }[] = []
    const sum: Sum = { bytes: new Map(), msgs: new Map() }
    const add = (entries: [string, number, number][], sign: 1 | -1) => {
        for (const [key, count, bytes] of entries) {
            sum.bytes.set(key, (sum.bytes.get(key) ?? 0) + sign * bytes)
            sum.msgs.set(key, (sum.msgs.get(key) ?? 0) + sign * count)
        }
    }
    for (const f of frames) {
        const entries: [string, number, number][] = []
        for (let i = 0; i + 5 <= f.net.length; i += 5) entries.push([`${f.net[i + 1]}:${f.net[i]}`, f.net[i + 2]!, f.net[i + 3]!])
        window.push({ end: f.end, entries: entries })
        add(entries, 1)
        while (window.length && window[0]!.end <= f.end - WINDOW_MS) add(window.shift()!.entries, -1)

        const seconds = WINDOW_MS / 1000
        const rate: Rate = { down: {}, up: {}, downMsgs: {}, upMsgs: {}, downAll: 0, upAll: 0, byType: new Map() }
        for (const [key, bytes] of sum.bytes) {
            if (bytes <= 0) continue
            const [way, command] = key.split(":").map(Number) as [0 | 1, number]
            const id = categoryOf(nameOf(way, command)).id
            const perSecond = bytes / seconds
            const msgs = (sum.msgs.get(key) ?? 0) / seconds
            rate.byType.set(key, perSecond)
            if (way === 0) {
                rate.down[id] = (rate.down[id] ?? 0) + perSecond
                rate.downMsgs[id] = (rate.downMsgs[id] ?? 0) + msgs
                rate.downAll += perSecond
            } else {
                rate.up[id] = (rate.up[id] ?? 0) + perSecond
                rate.upMsgs[id] = (rate.upMsgs[id] ?? 0) + msgs
                rate.upAll += perSecond
            }
        }
        out.set(f.id, rate)
    }
    return out
}
