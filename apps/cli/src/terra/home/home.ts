import { existsSync, mkdirSync, readdirSync, readFileSync, renameSync, writeFileSync } from "node:fs"
import { homedir } from "node:os"
import { basename, dirname, join } from "node:path"
import { err } from "../../errors"

type HomeOpts = {
    env: Readonly<Record<string, string | undefined>>
    platform: NodeJS.Platform
    /** Whether this is the terra players install: its home is ~/.terraverse, a source run's ~/.terraverse-dev. */
    release: boolean
}

/** A profile's name: local, @handle, or a plain test player's ("two"). */
const NAME = /^(local|@[A-Za-z0-9_.-]{1,64}|[A-Za-z0-9_-]{1,64})$/

/**
 * Home — where Terraverse keeps things on this machine (.agents/planning/home.md), as the
 * engine and the launcher keep them: one root, a profile per player, a cache that's safe to
 * delete. terra's sign-in, the mods and servers it makes, and its scratch all live here.
 */
export function Home(opts: HomeOpts) {
    const root = locate()

    function locate(): string {
        const given = opts.env.TERRAVERSE_HOME
        if (given) {
            if (!given.startsWith("/") && !/^[A-Za-z]:[\\/]/.test(given)) throw err("USAGE", { detail: "TERRAVERSE_HOME must be an absolute path" })
            return given
        }
        // installed and source builds never share: work on one never repoints the other
        if (opts.platform === "win32") return join(opts.env.APPDATA ?? join(homedir(), "AppData", "Roaming"), opts.release ? "Terraverse" : "Terraverse-dev")
        return join(homedir(), opts.release ? ".terraverse" : ".terraverse-dev")
    }

    /** Rebuildable: delete it and nothing is lost. */
    function cache(): string {
        return join(root, "cache")
    }

    /** The profile running now: TERRAVERSE_PROFILE, then the remembered one, then local. */
    function active(): string {
        const env = opts.env.TERRAVERSE_PROFILE
        if (env) return checked(env)
        const file = activeFile()
        if (!existsSync(file)) return "local"
        const saved: unknown = JSON.parse(readFileSync(file, "utf8"))
        const name = typeof saved === "object" && saved !== null ? Reflect.get(saved, "profile") : undefined
        if (typeof name !== "string" || !NAME.test(name)) throw err("USAGE", { detail: `${file} isn't { "profile": <name> }: delete it to start as local` })
        return name
    }

    /** A profile's folder, made if it's new: the active one's by default. */
    function profile(name: string = active()): string {
        const dir = join(root, "profiles", checked(name))
        if (!existsSync(dir)) {
            mkdirSync(dir, { recursive: true })
            writeJson(join(dir, "profile.json"), { name: name, handle: name.startsWith("@") ? name.slice(1) : null, created: Math.floor(Date.now() / 1000) })
        }
        return dir
    }

    /** The account the active profile is, or null for one that's no one's (local). */
    function handle(): string | null {
        const file = join(profile(), "profile.json")
        if (!existsSync(file)) return null
        const saved: unknown = JSON.parse(readFileSync(file, "utf8"))
        const value = typeof saved === "object" && saved !== null ? Reflect.get(saved, "handle") : undefined
        return typeof value === "string" && value ? value : null
    }

    /** Where the servers running on this machine are recorded (running/): the game reads it too. */
    function running(): string {
        return join(root, "running")
    }

    /** Run as `name` from now on, here and in the game: it's remembered as the active one. */
    function use(name: string): void {
        profile(name)
        mkdirSync(cache(), { recursive: true })
        writeJson(activeFile(), { profile: checked(name) })
    }

    /**
     * An account signed in: its profile becomes the active one, adopting `local` when the
     * account has none yet (so nothing played signed out is stranded), and the key file in
     * `keyFile` moves into it. Its new path.
     */
    function signedIn(handle: string, keyFile: string): string {
        const name = checked(`@${handle}`)
        const from = active()
        if (from === name) return keyFile
        const dir = join(root, "profiles", name)
        if (from === "local" && !existsSync(dir)) {
            renameSync(profile("local"), dir)
            writeJson(join(dir, "profile.json"), { name: name, handle: handle, created: Math.floor(Date.now() / 1000) })
            use(name)
            return join(dir, basename(keyFile))
        }
        use(name)
        const to = join(profile(name), basename(keyFile))
        if (existsSync(keyFile)) renameSync(keyFile, to)
        return to
    }

    /** Signed out: back to local; the account's profile is kept for next time. */
    function signedOut(): void {
        use("local")
    }

    /**
     * What older terras kept elsewhere, moved in: the sign-in from its config folder into
     * the active profile, the scratch servers into the cache. Safe to repeat: what's moved is
     * gone. A note is left where things were taken from.
     */
    function moveIn(old: { config: string; cache: string }): void {
        const moved: string[] = []
        const move = (from: string, to: string) => {
            if (!existsSync(from) || existsSync(to)) return
            mkdirSync(dirname(to), { recursive: true })
            renameSync(from, to)
            moved.push(`- ${from}  →  ${to}`)
        }
        if (existsSync(old.config))
            for (const file of readdirSync(old.config))
                if (/^account(-.+)?\.json$/.test(file)) move(join(old.config, file), join(profile(), file.replace(/^account/, "terra")))
        move(join(old.cache, "dev"), join(cache(), "dev"))
        for (const dir of [old.config, old.cache])
            if (moved.length > 0 && existsSync(dir))
                writeFileSync(join(dir, "MOVED.md"), `# Moved\n\nterra keeps everything in ${root} now (.agents/planning/home.md in the Terraverse repo):\n\n${moved.join("\n")}\n`)
    }

    function activeFile(): string {
        return join(cache(), "active-profile.json")
    }

    return {
        root: root,
        cache: cache,
        active: active,
        profile: profile,
        handle: handle,
        running: running,
        use: use,
        signedIn: signedIn,
        signedOut: signedOut,
        moveIn: moveIn,
        /** Mods the active profile is developing: found by default, shown in the game. */
        mods: () => join(profile(), "mods"),
        /** Server folders the active profile hosts. */
        servers: () => join(profile(), "servers"),
    }
}

export type HomeT = ReturnType<typeof Home>

function checked(name: string): string {
    if (!NAME.test(name) || name === "@." || name === "@..") throw err("USAGE", { detail: `not a profile's name: ${JSON.stringify(name)} (local, @handle, or letters, digits, _ and -)` })
    return name
}

function writeJson(path: string, value: unknown): void {
    writeFileSync(path, JSON.stringify(value, null, 2) + "\n")
}

/** Where terra kept its sign-in and its scratch before the home (the old Places): for moving in. */
export function oldPlaces(env: Readonly<Record<string, string | undefined>>, platform: NodeJS.Platform): { config: string; cache: string } {
    if (platform === "win32") return { config: join(env.APPDATA ?? join(homedir(), "AppData", "Roaming"), "terra"), cache: join(env.LOCALAPPDATA ?? join(homedir(), "AppData", "Local"), "terra", "cache") }
    if (platform === "darwin") return { config: join(homedir(), "Library", "Application Support", "terra"), cache: join(homedir(), "Library", "Caches", "terra") }
    return { config: join(env.XDG_CONFIG_HOME ?? join(homedir(), ".config"), "terra"), cache: join(env.XDG_CACHE_HOME ?? join(homedir(), ".cache"), "terra") }
}
