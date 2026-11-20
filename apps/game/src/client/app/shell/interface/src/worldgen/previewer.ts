import { shallowRef } from "vue"
import type { BridgeHandle } from "../bridge"
import { encodeWorldCode, type WorldConfig } from "./config"
import { decodePreview, type Fields } from "./fields"

export type { Fields } from "./fields"

/**
 * The preview of a world on a date, from the engine's terragen — the map you
 * see is the world you get. Requests made while one is in flight are
 * coalesced: only the newest is asked for next.
 */
export function Previewer(opts: { bridge: BridgeHandle; resolution: number }) {
    const fields = shallowRef<Fields | null>(null)
    const problem = shallowRef<string | null>(null)
    let busy = false
    let queued: { code: string; date: number } | null = null

    async function run(code: string, date: number) {
        busy = true
        try {
            const wire = await opts.bridge.send("worldgen.preview", { code: code, date: date, resolution: opts.resolution })
            fields.value = decodePreview(wire)
            problem.value = null
        } catch (err) {
            console.error(err)
            problem.value = err instanceof Error ? err.message : String(err)
        }
        busy = false
        if (queued) {
            const q = queued
            queued = null
            run(q.code, q.date)
        }
    }

    return {
        fields: fields,
        problem: problem,
        request(config: WorldConfig, date: number) {
            const code = encodeWorldCode(config)
            if (busy) queued = { code: code, date: date }
            else run(code, date)
        },
    }
}
