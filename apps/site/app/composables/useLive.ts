import type { MaybeRefOrGetter } from "vue"
import { cache } from "~/lib/cache"

/**
 * A live read that never makes a page wait. It's drawn from what's already
 * known, newest first: the page's own prerendered payload, then what this
 * browser last saw (lib/cache), and is fetched fresh behind it, the answer
 * swapped in when it lands. Nothing known draws nothing (the page's `v-if`),
 * never a made-up value. `every` polls while the tab is in view. A null
 * answer means "there's no such thing": the page turns it into its 404 (missing).
 */
export function useLive<T>(key: MaybeRefOrGetter<string>, fetch: () => Promise<T>, opts: { every?: number } = {}) {
    // Keys drawn from something already known, so still owed a fresh fetch
    const owed = new Set<string>()

    const read = useAsyncData(key, fetch, {
        lazy: true,
        // Asked on a first read and when the key moves; a refresh always fetches
        getCachedData(k, app, ctx) {
            if (ctx.cause === "refresh:manual" || ctx.cause === "refresh:hook") return undefined
            const known = (app.isHydrating ? app.payload.data[k] : undefined) ?? (import.meta.client ? cache.read<T>(k) : undefined)
            if (known !== undefined && import.meta.client) owed.add(k)
            return known
        },
    })

    if (import.meta.client) {
        const settle = (k: string) => {
            if (!owed.delete(k)) return
            void read.refresh()
        }
        // The first fetch waits for the page to be up, so hydration is never raced
        onMounted(() => settle(toValue(key)))
        watch(() => toValue(key), k => void nextTick(() => settle(k)))
        watch(read.data, value => {
            if (value !== undefined && value !== null) cache.write(toValue(key), value)
        })

        if (opts.every) {
            const every = opts.every
            let timer: ReturnType<typeof setInterval> | undefined
            onMounted(() => {
                timer = setInterval(() => {
                    if (document.visibilityState === "visible") void read.refresh()
                }, every)
            })
            onBeforeUnmount(() => clearInterval(timer))
        }
    }

    // Awaitable as useAsyncData is: the prerender waits for the answer, a browser never does
    return read
}
