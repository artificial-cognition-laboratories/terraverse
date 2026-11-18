import { readdirSync, statSync, watch, type FSWatcher } from "node:fs"
import { basename, join } from "node:path"

/** What changed: code (prepare again, and the engine reloads) or a config (restart it). */
export type Change = "code" | "config"

/** Folders that are never source, and churn: never watched, never walked into. */
const SKIPPED = new Set(["node_modules", ".terra", "save", ".git"])
const CONFIGS = new Set(["server.config.ts", "mod.config.ts", "package.json"])
/** What a project is made of: code, and its assets. Anything else (logs, lockfiles, an editor's swap files) isn't a save. */
const SOURCE = /\.(ts|tsx|js|mjs|vue|json)$/
const ASSETS = /(^|[/\\])assets([/\\]|$)/
const SETTLE_MS = 150

/**
 * Watch — saves in a project's folders, settled into one change: an editor writes a file
 * in several steps, and a save can touch several files. Never misses one: a change waits
 * here until it's asked for.
 *
 * Each folder is watched on its own, skipping node_modules, .terra and save, and new ones
 * are added as they appear. Not one recursive watch: Bun's crashes while `bun install`
 * fills a node_modules beneath it (1.4.0), and those folders churn on every prepare.
 */
export function Watch(opts: { dirs: readonly string[] }) {
    let pending: Change | null = null
    let timer: ReturnType<typeof setTimeout> | null = null
    let waiting: ((change: Change) => void) | null = null
    const watchers = new Map<string, FSWatcher>()

    /** Watch `dir` and every folder under it that isn't skipped. */
    function add(dir: string): void {
        if (watchers.has(dir) || SKIPPED.has(basename(dir))) return
        watchers.set(dir, watch(dir, (_event, file) => saw(dir, file)))
        for (const entry of readdirSync(dir, { withFileTypes: true }))
            if (entry.isDirectory()) add(join(dir, entry.name))
    }

    function saw(dir: string, file: string | null): void {
        if (file === null || SKIPPED.has(file)) return
        const path = join(dir, file)
        // a folder made (or moved in) since: watch it too
        // gone again by now (a deleted file, an editor's swap file) is no folder to add
        if (statSync(path, { throwIfNoEntry: false })?.isDirectory()) {
            add(path)
            return
        }
        if (!SOURCE.test(file) && !ASSETS.test(path)) return
        const change: Change = CONFIGS.has(file) ? "config" : "code"
        pending = pending === "config" ? "config" : change
        if (timer) clearTimeout(timer)
        timer = setTimeout(settle, SETTLE_MS)
    }

    function settle(): void {
        timer = null
        if (!pending || !waiting) return
        const change = pending
        const wake = waiting
        pending = null
        waiting = null
        wake(change)
    }

    function start(): void {
        for (const dir of opts.dirs) add(dir)
    }

    function stop(): void {
        for (const w of watchers.values()) w.close()
        watchers.clear()
        if (timer) clearTimeout(timer)
    }

    /** The next change, settled: at once if one already happened. */
    function next(): Promise<Change> {
        return new Promise(resolve => {
            waiting = resolve
            if (pending && !timer) settle()
        })
    }

    return { start: start, stop: stop, next: next }
}

export type WatchT = ReturnType<typeof Watch>
