import { homedir } from "node:os"

/** Places — where terra keeps what's its own, outside any project: caches, scratch universes, your account. */
export function Places(opts: { env: Readonly<Record<string, string | undefined>>; platform: NodeJS.Platform }) {
    /** Terra's cache: kept between runs, safe to delete. */
    function cache(): string {
        if (opts.platform === "win32") return `${opts.env.LOCALAPPDATA ?? `${homedir()}/AppData/Local`}/terra/cache`
        if (opts.platform === "darwin") return `${homedir()}/Library/Caches/terra`
        return `${opts.env.XDG_CACHE_HOME ?? `${homedir()}/.cache`}/terra`
    }

    /** Terra's settings and your sign-in: kept, and not safe to delete casually. */
    function config(): string {
        if (opts.platform === "win32") return `${opts.env.APPDATA ?? `${homedir()}/AppData/Roaming`}/terra`
        if (opts.platform === "darwin") return `${homedir()}/Library/Application Support/terra`
        return `${opts.env.XDG_CONFIG_HOME ?? `${homedir()}/.config`}/terra`
    }

    return { cache: cache, config: config }
}

export type PlacesT = ReturnType<typeof Places>
