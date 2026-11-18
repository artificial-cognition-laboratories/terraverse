/**
 * Worlds — where a server's universe is saved: save/worlds/, one engine world holding every
 * body of the system. The engine makes it from the config's system on first boot.
 */
export function Worlds(opts: { root: string }) {
    // TEMP(2026-11-18): one engine world for every body, and its players inside it, until
    // worlds and players are kept apart as the docs lay out (save/worlds, save/players)
    const folder = `${opts.root}/save/worlds`

    return { folder: folder }
}

export type WorldsT = ReturnType<typeof Worlds>
