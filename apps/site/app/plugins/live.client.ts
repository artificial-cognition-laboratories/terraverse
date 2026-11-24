import { cache } from "~/lib/cache"

/** Clears what earlier versions of the site cached, so no old shape is ever drawn. */
export default defineNuxtPlugin(() => {
    cache.sweep()
})
