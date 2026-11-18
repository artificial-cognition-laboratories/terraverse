/**
 * The debugger's tabs, in row order: each placeholder says what it will show.
 * `icon` is SVG path data on a 16-unit grid, stroked.
 */
export const TABS = [
    { id: "console", label: "Console", icon: "M2.5 3h11v10h-11zM5 6.5l2 1.5-2 1.5M8.5 10H11",
        about: "The engine's and the mods' log, as it's written." },
    { id: "perf", label: "Perf", icon: "M1 9h3l2-5 3 9 2-6 1 2h3",
        about: "Frame time, and where each frame goes: drawing, the far view, the client step, the body." },
    { id: "player", label: "Player", icon: "M8 2.5a2.5 2.5 0 1 1 0 5a2.5 2.5 0 1 1 0-5M3 14c0-3 2.2-4.5 5-4.5s5 1.5 5 4.5",
        about: "Where you are, how you're moving, and the mover's state and stance." },
    { id: "world", label: "World", icon: "M8 1.5a6.5 6.5 0 1 1 0 13a6.5 6.5 0 1 1 0-13M1.5 8h13M8 1.5c-2.5 3-2.5 10 0 13M8 1.5c2.5 3 2.5 10 0 13",
        about: "The world code, and terragen's values under the cursor: block, biome, geology." },
    { id: "matter", label: "Matter", icon: "M6 1.5h4M6.5 1.5v4.5L2.5 13.5h11L9.5 6V1.5M4.2 10.5h7.6",
        about: "The block you're looking at: its substances, temperature and state." },
    { id: "net", label: "Network", icon: "M5 14V2M2 5l3-3 3 3M11 2v12M8 11l3 3 3-3",
        about: "Round trip, packets, and what the server is sending." },
] as const

export type TabId = (typeof TABS)[number]["id"]
