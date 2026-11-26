import { BridgeError, type BridgeHandle } from "../bridge/index.ts"
import { Cached, type Kept } from "./cached.ts"
import { CACHED, keyOf, type Answer, type Ask, type CachedKind } from "./intents.ts"
import type { Store } from "./store.ts"

/**
 * The page's answers to the engine's reads, kept: what a page asks for is shown from here at
 * once, fetched live behind it, and kept again. One fetch per key at a time, however many pages
 * and prefetches ask.
 */
export function Cache(opts: { bridge: BridgeHandle; store: Store }) {
    const inflight = new Map<string, Promise<unknown>>()

    // The one place a kept value meets its type: entries are written only by fetch() under this
    // key, and the store drops every entry when the bridge's types change (intents.ts: SHAPE).
    function peek<K extends CachedKind>(kind: K, request: Ask<K>): Kept<K> | null {
        return opts.store.get(keyOf(kind, request)) as Kept<K> | null
    }

    function fetch<K extends CachedKind>(kind: K, request: Ask<K>): Promise<Answer<K>> {
        const key = keyOf(kind, request)
        const running = inflight.get(key)
        if (running) return running as Promise<Answer<K>>
        const call = opts.bridge.send(kind, request)
            .then((answer) => {
                opts.store.put(key, answer, CACHED[kind])
                return answer
            })
            .finally(() => inflight.delete(key))
        inflight.set(key, call)
        return call
    }

    /**
     * Fetch ahead of a click, unless this session already has it live. A refusal is left to the
     * page that asks for it: it asks again there and shows why.
     */
    function prefetch<K extends CachedKind>(kind: K, request: Ask<K>) {
        if (peek(kind, request)?.live) return
        fetch(kind, request).catch((err: unknown) => {
            if (!(err instanceof BridgeError && err.refused)) console.error(err)
        })
    }

    const source = { peek: peek, fetch: fetch }
    return {
        peek: peek,
        /** Ask live now, keeping the answer (a check that must be current: an update's latest version). */
        fetch: fetch,
        prefetch: prefetch,
        /** A page's handle on one intent: see Cached. */
        cached: <K extends CachedKind>(kind: K) => Cached({ source: source, kind: kind }),
    }
}
export type Cache = ReturnType<typeof Cache>
