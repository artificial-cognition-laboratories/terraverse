import { ref, shallowRef } from "vue"
import type { PerfCapture, PerfFrame } from "../../bridge/index.ts"
import { buildCapture, type Capture } from "./capture.ts"

/** Frames the page keeps: a minute at 60 fps. The engine keeps fewer zones (about ten seconds of a busy thread). */
const KEEP = 3600
/** Captures kept to go back to; the oldest goes past this. */
const CAPTURES = 8

/**
 * The Perf tab's data, kept by the page so it fills while another tab is
 * open: the frames as they stream (the Live charts), and captures: every
 * thread's zones over a range, fetched once and then looked through locally.
 */
export function PerfStore(opts: { capture: (from: number, to: number) => Promise<PerfCapture> }) {
    const frames = shallowRef<PerfFrame[]>([])
    const captures = shallowRef<Capture[]>([])
    const current = shallowRef<Capture | null>(null)
    const capturing = ref(false)
    const problem = ref<string | null>(null)
    let next = 1

    function append(incoming: readonly PerfFrame[]) {
        const joined = frames.value.concat(incoming)
        frames.value = joined.length > KEEP ? joined.slice(joined.length - KEEP) : joined
    }

    /** Every thread's zones over [from, to], whole; it becomes the current capture. */
    async function capture(from: number, to: number) {
        capturing.value = true
        problem.value = null
        try {
            const data = await opts.capture(from, to)
            const inside = frames.value.filter((f) => f.end > from && f.start < to)
            const made = buildCapture(next++, from, to, data, inside)
            captures.value = [...captures.value, made].slice(-CAPTURES)
            current.value = made
        } catch (error) {
            problem.value = error instanceof Error ? error.message : String(error)
        } finally {
            capturing.value = false
        }
    }

    function clear() {
        frames.value = []
        captures.value = []
        current.value = null
    }

    return { frames, captures, current, capturing, problem, append, capture, clear }
}

export type PerfStoreHandle = ReturnType<typeof PerfStore>
