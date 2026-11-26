import { computed, ref, shallowRef } from "vue"
import { BridgeError } from "../bridge/index.ts"
import { keyOf, type Answer, type Ask, type CachedKind } from "./intents.ts"

/** An answer kept for a request, typed by its intent. */
export type Kept<K extends CachedKind> = { value: Answer<K>; at: number; live: boolean }

/** Where a Cached reads and fetches: the Cache, narrowed to what a page needs. */
export type Source = {
    peek<K extends CachedKind>(kind: K, request: Ask<K>): Kept<K> | null
    fetch<K extends CachedKind>(kind: K, request: Ask<K>): Promise<Answer<K>>
}

/** An answer fetched live this recently isn't asked for again by `run` (a tab flicked back to). */
const FRESH_MS = 10_000

/**
 * One request a page shows, never blocking on it: what was kept for it at once (`stale` until the
 * live answer lands), nothing (a skeleton) if nothing was, and the live answer over either when it
 * comes. The data follows the latest request's key, so a slow reply to an old search only ever
 * fills its own entry. A failure beside kept data is shown as a note over that data, not instead.
 */
export function Cached<K extends CachedKind>(opts: { source: Source; kind: K }) {
    const asked = shallowRef<Ask<K> | null>(null)
    const entry = computed(() => asked.value === null ? null : opts.source.peek(opts.kind, asked.value))
    const data = computed(() => entry.value?.value ?? null)
    const stale = computed(() => entry.value !== null && !entry.value.live)
    const at = computed(() => entry.value?.at ?? null)
    const problem = ref<string | null>(null)
    const busy = ref(false)
    let latest = 0

    async function ask(request: Ask<K>, fresh: "if-old" | "always") {
        asked.value = request
        const call = ++latest
        problem.value = null
        const kept = opts.source.peek(opts.kind, request)
        if (fresh === "if-old" && kept?.live && Date.now() - kept.at < FRESH_MS) {
            busy.value = false
            return
        }
        busy.value = true
        try {
            await opts.source.fetch(opts.kind, request)
        } catch (err) {
            if (call !== latest) return
            if (!(err instanceof BridgeError && err.refused)) console.error(err)
            problem.value = err instanceof Error ? err.message : String(err)
        } finally {
            if (call === latest) busy.value = false
        }
    }

    return {
        data: data,
        stale: stale,
        at: at,
        problem: problem,
        busy: busy,
        /** Show this request: kept at once, live unless it was fetched moments ago. */
        run: (request: Ask<K>) => ask(request, "if-old"),
        /** Show this request, always asking again (after a save, a change made elsewhere). */
        refresh: (request: Ask<K>) => ask(request, "always"),
    }
}
export type Cached<K extends CachedKind> = ReturnType<typeof Cached<K>>
