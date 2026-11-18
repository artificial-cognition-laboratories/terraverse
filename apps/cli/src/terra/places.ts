import { homedir } from "node:os"

/** Places — where terra keeps what's its own, outside any project: caches, scratch universes. */
export function Places(opts: { env: Readonly<Record<string, string | undefined>>; platform: NodeJS.Platform }) {
    /** Terra's cache: kept between runs, safe to delete. */
    function cache(): string {
        if (opts.platform === "win32") return `${opts.env.LOCALAPPDATA ?? `${homedir()}/AppData/Local`}/terra/cache`
        if (opts.platform === "darwin") return `${homedir()}/Library/Caches/terra`
        return `${opts.env.XDG_CACHE_HOME ?? `${homedir()}/.cache`}/terra`
    }

    return { cache: cache }
}

export type PlacesT = ReturnType<typeof Places>
