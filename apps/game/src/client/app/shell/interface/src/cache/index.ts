import { inject, type InjectionKey } from "vue"
import type { BridgeHandle } from "../bridge/index.ts"
import { Cache } from "./cache.ts"
import { SHAPE } from "./intents.ts"
import { Store } from "./store.ts"

/**
 * The page's cache of the engine's reads: every page shows at once, from what was kept or a
 * skeleton, and the live answer replaces it when it comes. Booted before the app mounts, so the
 * disk is already in memory on the first frame.
 */
export async function bootCache(opts: { bridge: BridgeHandle }): Promise<Cache> {
    const store = await Store({ shape: SHAPE })
    return Cache({ bridge: opts.bridge, store: store })
}

// Symbol.for: one key across hot reloads, so a page reloaded under `bun dev` still finds what main.ts provided
export const CACHE: InjectionKey<Cache> = Symbol.for("terraverse.cache")

export function useCache(): Cache {
    const cache = inject(CACHE)
    if (!cache) throw new Error("useCache() outside the app: the cache was not provided")
    return cache
}

export type { Cache } from "./cache.ts"
export type { Cached } from "./cached.ts"
export type { CachedKind } from "./intents.ts"
