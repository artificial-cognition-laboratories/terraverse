/**
 * The docs' and the wiki's shape, as data: two areas on one layout. A section is a tab in the subnav; its groups are
 * the headings in the sidebar. Every path here must have a page under
 * content/docs, and every page should be reachable from here. The wiki is the same, under
 * content/wiki.
 */
export interface DocsNavItem {
    title: string
    path?: string
    icon?: string
    items?: DocsNavItem[]
}

export interface DocsGroup {
    title: string
    items: DocsNavItem[]
}

export interface DocsSection {
    id: string
    label: string
    /** Where the tab goes */
    path: string
    groups: DocsGroup[]
    /** Which end of the subnav the tab sits at: building on Terraverse groups right */
    side?: "left" | "right"
}

export const docsSections: DocsSection[] = [
    {
        id: "play",
        label: "Play",
        path: "/docs",
        groups: [
            {
                title: "Getting Started",
                items: [
                    { title: "What is Terraverse?", path: "/docs" },
                    { title: "Installation", path: "/docs/play/install" },
                    { title: "Controls", path: "/docs/play/controls" },
                ],
            },
        ],
    },
    {
        id: "worlds",
        label: "Worlds",
        path: "/docs/worlds",
        groups: [
            {
                title: "Worlds",
                items: [
                    { title: "Planets", path: "/docs/worlds" },
                    { title: "World codes", path: "/docs/worlds/codes" },
                ],
            },
        ],
    },
    {
        id: "matter",
        label: "Matter",
        path: "/docs/matter",
        groups: [{ title: "Matter", items: [{ title: "Substances and heat", path: "/docs/matter" }] }],
    },
    {
        id: "machines",
        label: "Machines",
        path: "/docs/machines",
        groups: [{ title: "Machines", items: [{ title: "Parts and joints", path: "/docs/machines" }] }],
    },
    {
        id: "modding",
        label: "Modding",
        path: "/docs/modding",
        side: "right",
        groups: [
            {
                title: "Modding",
                items: [
                    { title: "Overview", path: "/docs/modding" },
                    { title: "Your first mod", path: "/docs/modding/first-mod" },
                ],
            },
        ],
    },
    {
        id: "hosting",
        label: "Hosting",
        path: "/docs/hosting",
        side: "right",
        groups: [{ title: "Hosting", items: [{ title: "Hosting a world", path: "/docs/hosting" }] }],
    },
    {
        id: "engine",
        label: "Engine",
        path: "/docs/engine",
        side: "right",
        groups: [
            {
                title: "Engine",
                items: [
                    { title: "Architecture", path: "/docs/engine" },
                    { title: "Building from source", path: "/docs/engine/building" },
                ],
            },
        ],
    },
]

export const wikiSections: DocsSection[] = [
    {
        id: "wiki",
        label: "Wiki",
        path: "/wiki",
        groups: [{ title: "Wiki", items: [{ title: "About the wiki", path: "/wiki" }] }],
    },
    {
        id: "materials",
        label: "Materials",
        path: "/wiki/materials",
        groups: [
            {
                title: "Materials",
                items: [
                    { title: "Stone", path: "/wiki/materials" },
                    { title: "Water", path: "/wiki/materials/water" },
                ],
            },
        ],
    },
    {
        id: "blocks",
        label: "Blocks",
        path: "/wiki/blocks",
        groups: [{ title: "Blocks", items: [{ title: "Ground", path: "/wiki/blocks" }] }],
    },
]

export type Area = "docs" | "wiki"

export const areas: Record<Area, DocsSection[]> = { docs: docsSections, wiki: wikiSections }

/** The area a route is in: its first path segment. */
export function areaOf(path: string): Area {
    return path === "/wiki" || path.startsWith("/wiki/") ? "wiki" : "docs"
}

/** Every page of an area in reading order, for prev/next. */
export function pagesOf(area: Area): DocsNavItem[] {
    return areas[area].flatMap(s => s.groups.flatMap(g => flatten(g.items)))
}

function flatten(items: DocsNavItem[]): DocsNavItem[] {
    return items.flatMap(item => [...(item.path ? [item] : []), ...flatten(item.items ?? [])])
}
