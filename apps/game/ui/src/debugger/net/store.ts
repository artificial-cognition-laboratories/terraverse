import { ref, shallowRef } from "vue"
import type { NetCapture, NetEvent } from "../../bridge/index.ts"
import { TableFeed } from "../../table/feed.ts"
import { categoryOf } from "./categories.ts"

/** One message from a capture, as the Packets view lists it. */
export type Packet = {
    i: number
    time: number
    dir: 0 | 1 // 0 in, 1 out
    command: number
    name: string
    category: ReturnType<typeof categoryOf>
    channel: number
    reliable: boolean
    bytes: number
    head: string // hex, incoming only
}

/**
 * The Network tab's data beyond the frames (which carry the totals): message
 * types' names, the connection's events, the messages themselves as they
 * come (`live`, a feed the Packets view tails), and captures, fetched once and
 * looked through locally.
 */
export function NetStore(opts: { capture: (from: number, to: number) => Promise<NetCapture> }) {
    const names = shallowRef<{ in: string[]; out: string[] } | null>(null)
    const events = shallowRef<NetEvent[]>([])
    const packets = shallowRef<{ from: number; to: number; list: Packet[] } | null>(null)
    const capturing = ref(false)
    const problem = ref<string | null>(null)
    const live = TableFeed<Packet>({ key: (p) => `${p.time}:${p.dir}:${p.command}:${p.bytes}`, max: 5000 })
    let tailedTo = -1
    let tailing = false
    let made = 0 // every packet's number, live or captured: rows never share one

    /** A type's name: "TOCLIENT_BLOCKDATA", or its number while names haven't come. */
    function nameOf(dir: 0 | 1, command: number) {
        const list = names.value ? (dir === 0 ? names.value.in : names.value.out) : null
        return list?.[command] || `${dir === 0 ? "TOCLIENT" : "TOSERVER"}_0x${command.toString(16).padStart(2, "0")}`
    }

    function addEvents(incoming: readonly NetEvent[]) {
        events.value = events.value.concat(incoming).slice(-256)
    }

    function parse(got: NetCapture): Packet[] {
        if (got.messages.length % 6 !== 0) throw new Error("net: a capture's messages aren't in sixes")
        const list: Packet[] = []
        for (let i = 0, k = 0; i < got.messages.length; i += 6, k++) {
            const m = got.messages
            const dir = m[i + 1] === 0 ? 0 : 1
            const command = m[i + 2]!
            const name = nameOf(dir, command)
            list.push({
                i: made++, time: m[i]!, dir: dir, command: command, name: name, category: categoryOf(name),
                channel: m[i + 3]!, reliable: m[i + 4] === 1, bytes: m[i + 5]!, head: got.heads[k] ?? "",
            })
        }
        return list
    }

    /**
     * The messages since the last ask, up to `to` (the newest frame's end),
     * into the live feed. One ask at a time; the first only reaches back a second.
     */
    async function tail(to: number) {
        if (tailing || to <= tailedTo) return
        tailing = true
        const from = tailedTo < 0 ? Math.max(0, to - 1000) : tailedTo
        try {
            live.append(parse(await opts.capture(from, to)))
            tailedTo = to
        } catch (error) {
            problem.value = error instanceof Error ? error.message : String(error)
        } finally {
            tailing = false
        }
    }

    async function capture(from: number, to: number) {
        capturing.value = true
        problem.value = null
        try {
            packets.value = { from: from, to: to, list: parse(await opts.capture(from, to)) }
        } catch (error) {
            problem.value = error instanceof Error ? error.message : String(error)
        } finally {
            capturing.value = false
        }
    }

    /** Back to the live feed from a capture. */
    const release = () => (packets.value = null)

    return { names, events, packets, live, capturing, problem, nameOf, addEvents, tail, capture, release }
}

export type NetStoreHandle = ReturnType<typeof NetStore>
