import { shallowRef } from "vue"
import type { InspectorState } from "../../bridge/index.ts"

/**
 * The World tab's data: the game's inspected systems as last described, a few
 * times a second (none outside a world), and using one of their controls.
 */
export function WorldStore(opts: { act: (name: string, control: string, value: string | boolean | null) => Promise<unknown> }) {
    const inspectors = shallowRef<Record<string, InspectorState>>({})

    function described(incoming: Record<string, InspectorState>) {
        inspectors.value = incoming
    }
    return { inspectors: inspectors, described: described, act: opts.act }
}

export type WorldStoreHandle = ReturnType<typeof WorldStore>
