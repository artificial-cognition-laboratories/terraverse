import { ref, shallowRef } from "vue"
import { BridgeError } from "../../bridge/index.ts"

/**
 * One request the screen shows: what came back, or why it didn't. Only the latest call
 * lands, so a slow reply to an old search never overwrites a newer one.
 */
export function Request<A, T>(fetch: (args: A) => Promise<T>) {
    const data = shallowRef<T | null>(null)
    const problem = ref<string | null>(null)
    const busy = ref(false)
    let latest = 0

    async function run(args: A) {
        const call = ++latest
        busy.value = true
        problem.value = null
        try {
            const result = await fetch(args)
            if (call === latest) data.value = result
        } catch (err) {
            if (call !== latest) return
            if (!(err instanceof BridgeError && err.refused)) console.error(err)
            problem.value = err instanceof Error ? err.message : String(err)
        } finally {
            if (call === latest) busy.value = false
        }
    }

    return { data: data, problem: problem, busy: busy, run: run }
}
export type Request<A, T> = ReturnType<typeof Request<A, T>>
