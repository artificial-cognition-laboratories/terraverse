/**
 * The debugger's tabs, in row order: each placeholder says what it will show.
 * `icon` is SVG path data on a 16-unit grid, stroked.
 */
export const TABS = [
    { id: "console", label: "Console", icon: "M2.5 3h11v10h-11zM5 6.5l2 1.5-2 1.5M8.5 10H11",
        about: "The engine's and the mods' log, as it's written." },
    { id: "perf", label: "Performance", icon: "M1 9h3l2-5 3 9 2-6 1 2h3",
        about: "Frame time, and where each frame goes: drawing, the far view, the client step, the body." },
    { id: "memory", label: "Memory", icon: "M3 4.5h10v7H3zM5.5 4.5v7M8 4.5v7M10.5 4.5v7M5 2.5v2M8 2.5v2M11 2.5v2M5 11.5v2M8 11.5v2M11 11.5v2",
        about: "Where the memory goes: each process of the game, the engine's heap, what each system holds, and what's growing." },
    { id: "net", label: "Network", icon: "M5 14V2M2 5l3-3 3 3M11 2v12M8 11l3 3 3-3",
        about: "Round trip, packets, and what the server is sending." },
    { id: "world", label: "World", icon: "M8 1.5a6.5 6.5 0 1 0 0 13 6.5 6.5 0 0 0 0-13zM1.5 8h13M8 1.5c-2 2-2 11 0 13M8 1.5c2 2 2 11 0 13",
        about: "The game's systems as they run: what each is doing now, and switches to see inside it (water's first)." },
    { id: "server", label: "Server", icon: "M2.5 2.5h11v4h-11zM2.5 9.5h11v4h-11zM5 4.5h.01M5 11.5h.01",
        about: "The server this game runs: its step, its players, what it's simulating." },
] as const

export type TabId = (typeof TABS)[number]["id"]
