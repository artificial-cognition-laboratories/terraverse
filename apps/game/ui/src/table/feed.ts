import { reactive } from "vue"
import type { TableFeedHandle, TableFeedSink } from "./types.ts"

export interface TableFeedOpts<T> {
    /** Stable identity: a row seen twice is kept once. */
    key: (row: T) => string
    /** Rows kept; the oldest go past this. */
    max?: number
}

/**
 * A stream of rows for DataTable. It owns what a stream needs (a ring that
 * drops the oldest, dedupe, holding rows while paused, following the tail)
 * so the table only draws.
 */
export function TableFeed<T>(opts: TableFeedOpts<T>): TableFeedHandle<T> {
    const max = opts.max ?? 10_000
    const state: TableFeedHandle<T>["state"] = reactive({ total: 0, received: 0, buffered: 0, paused: false, following: true })
    const rows: T[] = []
    const seen = new Set<string>()
    const sinks = new Set<TableFeedSink<T>>()
    let held: T[] = []

    function push(incoming: readonly T[]): void {
        const fresh: T[] = []
        for (const row of incoming) {
            const key = opts.key(row)
            if (seen.has(key)) continue
            seen.add(key)
            fresh.push(row)
        }
        if (fresh.length === 0) return
        state.received += fresh.length
        // one at a time: spreading a resumed pause's rows can pass the argument limit
        for (const row of fresh) rows.push(row)
        const evicted = rows.length > max ? rows.splice(0, rows.length - max) : []
        for (const row of evicted) seen.delete(opts.key(row))
        state.total = rows.length
        for (const sink of sinks) {
            sink.add(fresh)
            if (evicted.length > 0) sink.remove(evicted)
        }
    }

    return {
        state,
        append(incoming) {
            if (!state.paused) return push(incoming)
            for (const row of incoming) held.push(row)
            if (held.length > max) held.splice(0, held.length - max)
            state.buffered = held.length
        },
        clear() {
            rows.length = 0
            held = []
            seen.clear()
            state.total = 0
            state.received = 0
            state.buffered = 0
            for (const sink of sinks) sink.clear()
        },
        setPaused(paused) {
            state.paused = paused
            if (paused || held.length === 0) return
            const incoming = held
            held = []
            state.buffered = 0
            push(incoming)
        },
        setFollowing(following) {
            state.following = following
        },
        snapshot() {
            return [...rows]
        },
        subscribe(sink) {
            sinks.add(sink)
            return () => sinks.delete(sink)
        },
    }
}
