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
    /** Drawn dimmed: there, but nothing you write (node_modules) */
    muted?: boolean
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

/**
 * A server as it sits on disk. Source at the top level, the save in one visible folder (never
 * edited, never deleted), and everything regenerable in .terra/.
 *
 * A server is one universe under one set of rules: the mods
 * every world in it runs, its players, and the worlds themselves (one planet each). A world's
 * own folder is opaque, an implementation detail, so it has no tree of its own in the docs.
 * Singleplayer is a server too, made for you.
 */
const serverFolder: DocsNavItem = {
    title: "my_server",
    icon: "vscode-icons:default-folder-opened",
    path: "/docs/server/structure",
    items: [
        { title: ".terra", icon: "vscode-icons:folder-type-typings", path: "/docs/server/structure/dot-terra" },
        { title: "mods", icon: "vscode-icons:folder-type-plugin", path: "/docs/server/structure/mods" },
        {
            title: "save",
            icon: "vscode-icons:folder-type-db",
            path: "/docs/server/structure/save",
            items: [
                { title: "worlds", icon: "vscode-icons:default-folder", path: "/docs/server/structure/worlds" },
                { title: "players", icon: "vscode-icons:default-folder", path: "/docs/server/structure/players" },
                { title: "store", icon: "vscode-icons:default-folder", path: "/docs/server/structure/store" },
            ],
        },
        { title: "start", icon: "vscode-icons:folder-type-images", path: "/docs/server/structure/start" },
        { title: "package.json", icon: "vscode-icons:file-type-json", path: "/docs/server/structure/package" },
        { title: "server.config.ts", icon: "vscode-icons:file-type-typescript", path: "/docs/server/structure/config" },
    ],
}

/**
 * A mod as it sits on disk: each entry links to the page about that file.
 *
 * A mod IS a TypeScript project: package.json, tsconfig, a bun install and a generated
 * type frame. That is what gives server/main.ts and client/main.ts autocomplete on the engine's globals with no
 * imports, and why it gets a folder tree like any other project.
 */
const modFolder: DocsNavItem = {
    title: "my_mod",
    icon: "vscode-icons:default-folder-opened",
    path: "/docs/mods/structure",
    items: [
        { title: ".terra", icon: "vscode-icons:folder-type-typings", path: "/docs/mods/structure/dot-terra" },
        { title: "assets", icon: "vscode-icons:folder-type-images", path: "/docs/mods/structure/assets" },
        { title: "server", icon: "vscode-icons:folder-type-server", path: "/docs/mods/structure/server" },
        { title: "client", icon: "vscode-icons:folder-type-client", path: "/docs/mods/structure/client" },
        { title: "mod.config.ts", icon: "vscode-icons:file-type-typescript", path: "/docs/mods/structure/config" },
        { title: "package.json", icon: "vscode-icons:file-type-json", path: "/docs/mods/structure/package" },
    ],
}

/**
 * The server half's globals: what things are (declared at load), what's being played, the
 * other half, and the mod's own. Few and nested: each names a real thing. `server` and `client`
 * only ever mean the other half; `game` is what's being played, as each side sees it.
 */
const serverApi: DocsNavItem[] = [
    { title: "content", path: "/docs/mods/api/content" },
    {
        title: "game",
        path: "/docs/mods/api/game",
        items: [
            { title: "worlds", path: "/docs/mods/api/game/worlds" },
            { title: "players", path: "/docs/mods/api/game/players" },
            { title: "commands", path: "/docs/mods/api/game/commands" },
        ],
    },
    { title: "client", path: "/docs/mods/api/client" },
    { title: "mod", path: "/docs/mods/api/mod" },
    { title: "types", path: "/docs/mods/api/types" },
]

/** The client half's globals: one player's view. It presents and asks, never decides. Panels need none: they are files. */
const clientApi: DocsNavItem[] = [
    { title: "game", path: "/docs/mods/api/client/game" },
    { title: "player", path: "/docs/mods/api/client/player" },
    { title: "server", path: "/docs/mods/api/client/server" },
]

function cli(title: string, path: string): DocsNavItem {
    return { title, path, flavor: "bash" }
}

export const docsSections: DocsSection[] = [
    {
        id: "introduction",
        label: "Introduction",
        path: "/docs",
        groups: [
            {
                title: "Introduction",
                items: [
                    { title: "What is Terraverse?", path: "/docs" },
                    { title: "Installation", path: "/docs/install" },
                ],
            },
        ],
    },
    {
        id: "server",
        label: "Server",
        path: "/docs/server",
        groups: [
            {
                title: "Server",
                items: [
                    { title: "Overview", path: "/docs/server" },
                    { title: "Lifecycle", path: "/docs/server/lifecycle" },
                    { title: "Setup", path: "/docs/server/setup" },
                    { title: "Mods", path: "/docs/server/mods" },
                    { title: "Publish", path: "/docs/server/publish" },
                ],
            },
            { title: "Server structure", items: [serverFolder] },
        ],
    },
    {
        id: "mods",
        label: "Mods",
        path: "/docs/mods",
        groups: [
            {
                title: "Mods",
                items: [
                    { title: "Overview", path: "/docs/mods" },
                    { title: "Lifecycle", path: "/docs/mods/lifecycle" },
                    { title: "Server", path: "/docs/mods/server" },
                    { title: "Client", path: "/docs/mods/client" },
                    { title: "Publish", path: "/docs/mods/publish" },
                ],
            },
            { title: "Mod structure", items: [modFolder] },
            // Placeholders: the namespaces are placed, their contents not yet designed
            { title: "Server API", items: serverApi },
            { title: "Client API", items: clientApi },
        ],
    },
    {
        id: "deploy",
        label: "Deploy",
        path: "/docs/deploy",
        side: "right",
        // Managed hosting: the same server folder, run by us. Not built yet; the pages say so
        groups: [
            {
                title: "Deploy",
                items: [
                    { title: "Overview", path: "/docs/deploy" },
                    { title: "Lifecycle", path: "/docs/deploy/lifecycle" },
                    { title: "Updates", path: "/docs/deploy/updates" },
                    { title: "Saves", path: "/docs/deploy/saves" },
                ],
            },
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
                    cli("terra", "/docs/cli"),
                    cli("terra <ref>", "/docs/cli/ref"),
                    cli("terra init", "/docs/cli/init"),
                    cli("terra dev", "/docs/cli/dev"),
                    cli("terra host", "/docs/cli/host"),
                    cli("terra prepare", "/docs/cli/prepare"),
                    cli("terra install", "/docs/cli/install"),
                    cli("terra uninstall", "/docs/cli/uninstall"),
                    cli("terra snapshot", "/docs/cli/snapshot"),
                    cli("terra publish", "/docs/cli/publish"),
                    cli("terra deploy", "/docs/cli/deploy"),
                    cli("terra version", "/docs/cli/version"),
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
