import { shallowRef } from "vue"
import type { ProcessSample, ServerSummary, TraceMark } from "../../bridge/index.ts"

/**
 * The Server tab's data: the server as last described (once a second; null
 * outside a world), the process, and the marks kept for its Startup waterfall.
 * The charts read the frames (the server's zones, the process counters).
 */
export function ServerStore() {
    const summary = shallowRef<ServerSummary | null>(null)
    const process = shallowRef<ProcessSample | null>(null)
    const marks = shallowRef<TraceMark[]>([])

    function described(server: ServerSummary | null, sample: ProcessSample) {
        summary.value = server
        process.value = sample
    }
    function marked(incoming: readonly TraceMark[]) {
        marks.value = marks.value.concat(incoming).slice(-1024)
    }
    return { summary, process, marks, described, marked }
}

export type ServerStoreHandle = ReturnType<typeof ServerStore>
