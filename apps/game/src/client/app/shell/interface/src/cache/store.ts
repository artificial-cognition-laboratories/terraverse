import { shallowReactive } from "vue"

/** One kept answer: the value, when it was fetched, and whether this session has seen it live. */
export type Entry = { value: unknown; at: number; live: boolean }

const DB = "terraverse-cache"
const TABLE = "entries"
/** An entry nobody has fetched for this long is dropped at boot. */
const KEEP_MS = 30 * 24 * 60 * 60 * 1000

type Row = { shape: string; value: unknown; at: number }

/**
 * Every kept answer, in memory for this session and on disk across them. Memory is read
 * synchronously, so a page has its last answer on its first frame; the disk is read once, at
 * boot, before the app mounts. Reactive by key, shallowly (an answer is replaced whole, never
 * proxied): everything showing an entry follows its writes.
 */
export async function Store(opts: { shape: string }) {
    const entries = shallowReactive(new Map<string, Entry>())
    const db = await open()
    if (db) await load(db, opts.shape, entries)

    function get(key: string): Entry | null {
        return entries.get(key) ?? null
    }
    function put(key: string, value: unknown, keep: "disk" | "session") {
        const at = Date.now()
        // A live answer that says what was kept keeps the kept object: nothing showing it re-renders
        // (a new object, however equal, rebuilds every list and card under it).
        const kept = entries.get(key)
        const same = kept !== undefined && JSON.stringify(kept.value) === JSON.stringify(value)
        entries.set(key, { value: same ? kept.value : value, at: at, live: true })
        if (keep === "disk" && db) write(db, key, { shape: opts.shape, value: value, at: at })
    }

    return { get: get, put: put, persisted: db !== null }
}
export type Store = Awaited<ReturnType<typeof Store>>

// A cache that can't reach the disk is still correct, only cold after a restart: said once,
// loudly, and the page runs from memory.
function open(): Promise<IDBDatabase | null> {
    return new Promise((resolve) => {
        const req = indexedDB.open(DB, 1)
        req.onupgradeneeded = () => req.result.createObjectStore(TABLE)
        req.onsuccess = () => resolve(req.result)
        req.onerror = () => {
            console.error("cache: IndexedDB won't open, so nothing is kept across restarts", req.error)
            resolve(null)
        }
    })
}

function load(db: IDBDatabase, shape: string, into: Map<string, Entry>): Promise<void> {
    return new Promise((resolve, reject) => {
        const tx = db.transaction(TABLE, "readwrite")
        const table = tx.objectStore(TABLE)
        const cursor = table.openCursor()
        const oldest = Date.now() - KEEP_MS
        cursor.onsuccess = () => {
            const item = cursor.result
            if (!item) return
            const row = item.value as Row
            if (row.shape !== shape || row.at < oldest) item.delete()
            else into.set(String(item.key), { value: row.value, at: row.at, live: false })
            item.continue()
        }
        tx.oncomplete = () => resolve()
        tx.onerror = () => reject(tx.error ?? new Error("cache: reading IndexedDB failed"))
    })
}

function write(db: IDBDatabase, key: string, row: Row) {
    const tx = db.transaction(TABLE, "readwrite")
    tx.objectStore(TABLE).put(row, key)
    tx.onerror = () => console.error(`cache: couldn't keep ${key}`, tx.error)
}
