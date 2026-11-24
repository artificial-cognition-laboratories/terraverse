import { existsSync, readdirSync, readFileSync } from "node:fs"
import { join } from "node:path"
import { err } from "../../errors"

/** A world the player has, as the game's Play list shows it. */
export type SavedWorld = {
    /** Its folder's name: what it's asked for by. */
    name: string
    /** What the game calls it (world.mt's world_name). */
    title: string
    path: string
    /** Its own server folder (mods of its own), or null: the game's, as every world without one runs. */
    server: string | null
}

/**
 * Worlds — the worlds in the active profile, the ones the game's Play list shows: what `terra
 * worlds` lists and `terra serve --world` runs. A world is a folder with a world.mt.
 */
export function Worlds(opts: { dir: () => string }) {
    function read(path: string, name: string): SavedWorld {
        const mt = readFileSync(join(path, "world.mt"), "utf8")
        const title = /^world_name\s*=\s*(.+)$/m.exec(mt)?.[1]?.trim() ?? name
        // the game's rule (shell/bridge/world_server.cpp): a server/ terra has prepared
        const server = join(path, "server")
        return { name: name, title: title, path: path, server: existsSync(join(server, ".terra", "manifest.json")) ? server : null }
    }

    function list(): SavedWorld[] {
        const dir = opts.dir()
        if (!existsSync(dir)) return []
        return readdirSync(dir, { withFileTypes: true })
            .filter(entry => entry.isDirectory() && existsSync(join(dir, entry.name, "world.mt")))
            .map(entry => read(join(dir, entry.name), entry.name))
            .sort((a, b) => a.name.localeCompare(b.name))
    }

    return {
        list: list,
        /** One by its name in the Play list, or a folder's path. */
        find(said: string): SavedWorld {
            if (said.includes("/")) {
                if (!existsSync(join(said, "world.mt"))) throw err("USAGE", { detail: `${said} isn't a world (no world.mt)` })
                return read(said, said.split("/").filter(Boolean).at(-1)!)
            }
            const found = list().find(world => world.name === said)
            if (!found) throw err("USAGE", { detail: `there's no world ${said} in ${opts.dir()} — \`terra worlds\` lists them` })
            return found
        },
    }
}

export type WorldsT = ReturnType<typeof Worlds>
