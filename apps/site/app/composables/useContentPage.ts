import type { Area } from "~/config/docs-nav"

/**
 * The markdown page at this route, from the docs or the wiki, with its head:
 * title, description and canonical from frontmatter. A route with no page is
 * a 404, never an empty article.
 */
export async function useContentPage(area: Area) {
    const route = useRoute()
    const site = useRuntimeConfig().public.site

    const query = useAsyncData(`${area}-${route.path}`, () => queryCollection(area).path(route.path).first())
    const page = query.data

    // Registered before the await: Nuxt's context doesn't survive one
    useHead(() => {
        const canonical = `${site}${route.path}`
        const description = page.value?.description
        return {
            title: page.value?.title,
            meta: [
                { name: "description", content: description },
                { property: "og:description", content: description },
                { property: "og:type", content: "article" },
                { property: "og:title", content: `${page.value?.title} / Terraverse` },
                { property: "og:url", content: canonical },
            ],
            link: [{ rel: "canonical", href: canonical }],
        }
    })

    // Awaited, so the next page resolves before the swap rather than flashing empty
    await query
    if (!page.value) throw createError({ statusCode: 404, statusMessage: `No such ${area} page` })
    return page
}
