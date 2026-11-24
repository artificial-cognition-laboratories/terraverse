import type { MaybeRefOrGetter } from "vue"

/**
 * A page's 404, for a page whose thing arrives live (useLive). The prerender
 * has the answer before it draws, so it fails the page there; a browser learns
 * it when the read lands and shows the error page then.
 */
export function missing(gone: MaybeRefOrGetter<boolean>, message: MaybeRefOrGetter<string>): void {
    const error = () => createError({ statusCode: 404, statusMessage: toValue(message), fatal: true })
    if (import.meta.server) {
        if (toValue(gone)) throw error()
        return
    }
    watchEffect(() => {
        if (toValue(gone)) showError(error())
    })
}
