import { readdirSync } from "node:fs"
import { join } from "node:path"

const SITE = "https://terraverse.arclabs.it"
const DESCRIPTION =
    "An open-source voxel universe. Real planets, real physics and real matter, and whatever game you want to play on them."

// The sidebar is data, so the crawler can't find every page by links alone:
// list each content tree so every page is prerendered.
function contentRoutes(area: string, directory = `./content/${area}`, relative = ""): string[] {
    return readdirSync(directory, { withFileTypes: true }).flatMap(entry => {
        const path = relative ? `${relative}/${entry.name}` : entry.name
        if (entry.isDirectory()) return contentRoutes(area, join(directory, entry.name), path)
        if (!entry.isFile() || !entry.name.endsWith(".md")) return []
        const slug = path.slice(0, -".md".length).replace(/(^|\/)index$/, "")
        return [slug ? `/${area}/${slug}` : `/${area}`]
    })
}

export default defineNuxtConfig({
    compatibilityDate: "2025-07-15",
    devtools: { enabled: false },
    modules: ["@nuxt/content", "@nuxt/icon"],
    css: ["~/assets/styles/main.scss"],

    app: {
        head: {
            htmlAttrs: { lang: "en" },
            meta: [
                { name: "viewport", content: "width=device-width, initial-scale=1" },
                { name: "theme-color", content: "#0c0e10" },
                { name: "description", content: DESCRIPTION },
                { property: "og:type", content: "website" },
                { property: "og:url", content: SITE },
                { property: "og:title", content: "Terraverse" },
                { property: "og:description", content: DESCRIPTION },
                { property: "og:site_name", content: "Terraverse" },
            ],
            link: [
                {
                    rel: "icon",
                    href: "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 8 8' shape-rendering='crispEdges'%3E%3Cpath fill='%2363a3e9' d='M0 1h2v1h1v1h1v2H3v1H2v1H0V6h1V5h1V3H1V2H0zM4 1h2v1h1v1h1v2H7v1H6v1H4V6h1V5h1V3H5V2H4z'/%3E%3C/svg%3E",
                },
            ],
        },
    },

    runtimeConfig: {
        // The accounts are ArcLabs': production's by default, the local stack's under `arc u` (.env.stack, never
        // .env.local: bun loads that into every `bun run`, the production build included;
        // NUXT_PUBLIC_*). The Supabase key is the publishable one, made to be in a browser.
        public: {
            site: SITE,
            api: "https://axon-api-t53zrgvpga-ew.a.run.app",
            supabaseUrl: "https://rygdiujydppljkwgajth.supabase.co",
            supabaseKey: "sb_publishable_ZGJCskKDg3IMVRv06yXWow_t1FuUnJi",
        },
    },

    // Icons inlined as SVG at build time, so nothing pops in after hydration.
    icon: { serverBundle: "local", mode: "svg" },

    content: {
        // node:sqlite (Node 22.5+), so no native better-sqlite3 build to install
        experimental: { sqliteConnector: "native" },
        build: {
            markdown: {
                highlight: {
                    theme: "dark-plus",
                    langs: ["bash", "shell", "json", "cpp", "typescript", "toml", "ini", "vue"],
                },
            },
        },
    },

    nitro: {
        preset: "static",
        // TEMP(2026-11-21): the wiki is one "coming soon" page; its content routes return with it
        // The account's pages are reached from the profile menu, which only the browser draws: name them
        prerender: {
            routes: ["/", ...contentRoutes("docs"), "/wiki", "/signin", "/register", "/device", "/registry/starred"],
            failOnError: true,
        },
    },

    vite: {
        build: { cssCodeSplit: false },
        css: {
            preprocessorOptions: {
                scss: {
                    additionalData:
                        '@use "~/assets/styles/variables.scss" as *; @use "~/assets/styles/mixins.scss" as *;',
                },
            },
        },
    },
})
