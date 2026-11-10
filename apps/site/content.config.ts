import { defineContentConfig, defineCollection, z } from "@nuxt/content"

const page = z.object({
    title: z.string(),
    description: z.string().min(1),
})

export default defineContentConfig({
    collections: {
        // The schema's `description` is required: it is the page's search result, and a
        // page without one would publish under the site-wide description.
        docs: defineCollection({ type: "page", source: "docs/**/*.md", schema: page }),
        wiki: defineCollection({ type: "page", source: "wiki/**/*.md", schema: page }),
    },
})
