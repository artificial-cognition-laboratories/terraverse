import { inject, shallowRef, type InjectionKey } from "vue"
import type { BodyCard, BridgeHandle } from "../bridge"
import { causes, type SystemHandle } from "./model"

/**
 * Every body's card from the engine's body model (`system.preview`). A
 * request made while one is in flight is coalesced: only the newest goes
 * next. Cards are keyed by body id.
 */
export function SystemPreview(opts: { bridge: BridgeHandle }) {
    const cards = shallowRef<Map<string, BodyCard>>(new Map())
    const problem = shallowRef<string | null>(null)
    let busy = false
    let queued: ReturnType<typeof causes> | null = null

    async function run(request: ReturnType<typeof causes>) {
        busy = true
        try {
            const out = await opts.bridge.send("system.preview", request)
            cards.value = new Map(out.map((c) => [c.id, c]))
            problem.value = null
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
        cards: cards,
        problem: problem,
        request(system: SystemHandle, star: Parameters<typeof causes>[1]) {
            const request = causes(system, star)
            if (busy) queued = request
            else run(request)
        },
        card(id: string): BodyCard | null {
            return cards.value.get(id) ?? null
        },
    }
}

export type SystemPreviewHandle = ReturnType<typeof SystemPreview>


export const SYSTEM_PREVIEW: InjectionKey<SystemPreviewHandle> = Symbol("system preview")

/** The create screen's body cards (provided by CreateWorld). */
export function useSystemPreview() {
    const preview = inject(SYSTEM_PREVIEW)
    if (!preview) throw new Error("useSystemPreview() outside the create screen")
    return preview
}
