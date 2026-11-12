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
    /** Drawn as a shell command: the program in one colour, its flags in another */
    flavor?: "bash"
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

/** A mod as it sits on disk: each entry links to the page about that file. */
const modFolder: DocsNavItem = {
    title: "my_mod",
    icon: "vscode-icons:default-folder-opened",
    path: "/docs/mods/folder",
    items: [
        { title: "textures", icon: "vscode-icons:folder-type-images", path: "/docs/mods/textures" },
        { title: "init.lua", icon: "vscode-icons:file-type-lua", path: "/docs/mods/init" },
        { title: "lab.json", icon: "vscode-icons:file-type-json", path: "/docs/mods/lab" },
        { title: "lab.lua", icon: "vscode-icons:file-type-lua", path: "/docs/mods/lab-lua" },
        { title: "mod.conf", icon: "vscode-icons:file-type-config", path: "/docs/mods/conf" },
    ],
}

function cli(title: string, path: string): DocsNavItem {
    return { title, path, flavor: "bash" }
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
        id: "mods",
        label: "Mods",
        path: "/docs/mods",
        side: "right",
        groups: [
            {
                title: "Mods",
                items: [
                    { title: "Overview", path: "/docs/mods" },
                    { title: "Your first mod", path: "/docs/mods/first-mod" },
                ],
            },
            { title: "Mod structure", items: [modFolder] },
        ],
    },
    {
        id: "cli",
        label: "CLI",
        path: "/docs/cli",
        side: "right",
        groups: [
            {
                title: "CLI API",
                items: [
                    cli("terraverse", "/docs/cli"),
                    cli("terraverse <ref>", "/docs/cli/ref"),
                    cli("terraverse host", "/docs/cli/host"),
                    cli("terraverse version", "/docs/cli/version"),
                    cli("terraverse publish", "/docs/cli/publish"),
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
