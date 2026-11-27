import { shallowRef } from "vue"
import type { MemorySample } from "../../bridge/index.ts"

/** Five minutes at ten a second: as many as the engine keeps for the page. */
const KEEP = 3000

/** A sample as a chart reads it: a moment, the second before it. */
export type Sampled = MemorySample & { start: number; end: number }

/**
 * The Memory tab's data, kept by the page so it fills while another tab is
 * open: every sample since the session started (the engine sends what it kept
 * to a fresh page), and writing a report of them.
 */
export function MemoryStore(opts: { report: () => Promise<string> }) {
    const samples = shallowRef<Sampled[]>([])

    function append(incoming: readonly MemorySample[]) {
        const made = incoming.map((s): Sampled => ({ ...s, start: s.t - 100, end: s.t }))
        const joined = samples.value.concat(made)
        samples.value = joined.length > KEEP ? joined.slice(joined.length - KEEP) : joined
    }

    return { samples: samples, append: append, report: opts.report }
}

export type MemoryStoreHandle = ReturnType<typeof MemoryStore>
