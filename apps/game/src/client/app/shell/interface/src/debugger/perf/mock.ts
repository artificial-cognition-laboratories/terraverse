import type { PerfCapture, PerfFrame } from "../../bridge/index.ts"

/**
 * A recorder's output for the gallery: frames and zones made up the same way
 * every time from the frame number, shaped like the engine's (Main's phases
 * with draw steps inside, a server step, a mesh worker), with a hitch now and then.
 */
type Phase = { name: string; ms: number; inside?: Phase[] }
const MAIN: Phase[] = [
    { name: "Present", ms: 3.6 },
    { name: "Wait", ms: 1.2 },
    { name: "Input", ms: 0.3 },
    { name: "Step", ms: 2.2, inside: [{ name: "Body move", ms: 1.1 }, { name: "Client: piles", ms: 0.3 }] },
    { name: "Interaction", ms: 0.4 },
    { name: "Update frame", ms: 6.5, inside: [
        { name: "Draw3D", ms: 3.2, inside: [{ name: "CM::updateDrawList", ms: 0.9 }] },
        { name: "Far view update", ms: 1.3 },
        { name: "DrawHUD", ms: 0.4 },
        { name: "UI", ms: 0.9, inside: [{ name: "UI: inbox", ms: 0.4 }, { name: "Page upload", ms: 0.3 }] },
    ] },
]

const wobble = (id: number, salt: number) => {
    const x = Math.sin(id * 12.9898 + salt * 78.233) * 43758.5453
    return x - Math.floor(x)
}
const hitch = (id: number, name: string) => (name === "Draw3D" && id % 97 === 0 ? 28 : name === "Far view update" && id % 41 === 0 ? 6 : 0)

/** The frame's phases laid out in time: [name, depth, start, end]. */
function layout(id: number, start: number) {
    const spans: [string, number, number, number][] = []
    const place = (phases: Phase[], depth: number, at: number, room: number | null) => {
        for (const p of phases) {
            const ms = p.ms * (0.8 + wobble(id, p.ms * 7 + depth) * 0.5) + hitch(id, p.name)
            const took = room === null ? ms : Math.min(ms, room)
            spans.push([p.name, depth, at, at + took])
            if (p.inside) place(p.inside, depth + 1, at + 0.05, took - 0.1)
            at += took + (room === null ? 0.08 : 0.02)
            if (room !== null) room -= took
        }
        return at
    }
    const end = place(MAIN, 0, start, null)
    return { spans: spans, end: end + 0.25 }
}

export function mockFrame(id: number, start: number): PerfFrame {
    const { spans, end } = layout(id, start)
    const main: Record<string, number> = {}
    for (const [name, , a, b] of spans) main[name] = (main[name] ?? 0) + (b - a)
    const threads: PerfFrame["threads"] = {
        Main: main,
        Server: { "Server::AsyncRunStep": 3, "ServerEnv: Run SAO::step": 1.1 * (0.8 + wobble(id, 3) * 0.4), Receive: 11 },
    }
    if (id % 3 === 0) threads.MeshUpdate = { "Client: Mesh making (sum)": 7 * (0.7 + wobble(id, 5) * 0.6) }
    if (id % 3 === 1) threads["MeshUpdate 2"] = { "Client: Mesh making (sum)": 6 * (0.7 + wobble(id, 6) * 0.6) }
    const drawn = 900 + Math.round(wobble(id, 9) * 120) + (id % 97 === 0 ? 700 : 0)
    const counters: PerfFrame["counters"] = {
        "MapBlocks in range": 4200, "MapBlocks examined": 2600, "MapBlocks drawn": drawn, "MapBlocks frustum culled": 1500,
        "Irr: drawcalls": drawn * 2 + 40, "Irr: primitives per drawcall": 310 + wobble(id, 11) * 30,
        "ActiveObjectMgr: CAO count": 42, "ParticleManager: particle buffer count": 3,
        "Game::updateFrame(): update frame": 6.4 * (0.8 + wobble(id, 12) * 0.4),
    }
    if (id % 5 === 0) Object.assign(counters, { "ServerEnv: ABMs run": 30 + Math.round(wobble(id, 13) * 20), "ServerEnv: active blocks": 380 })
    Object.assign(counters, { "Process: CPU": 34 + wobble(id, 16) * 8, "Process: memory": 1210 + id * 0.01, "ActiveObjectMgr: SAO count": 57 })
    Object.assign(counters, { "Net: RTT": 1.2 + wobble(id, 14) * 0.6, "Net: jitter": 0.3 + wobble(id, 15) * 0.2, "Net: lost": id % 211 < 3 ? 4 : 0 })
    return { id: id, start: start, end: end, threads: threads, counters: counters, net: mockNet(id) }
}

/** Message types the mock sends, by number each way (TOCLIENT_ in, TOSERVER_ out). */
export const MOCK_NAMES = (() => {
    const inn: string[] = Array.from({ length: 0x72 }, () => "")
    const out: string[] = Array.from({ length: 0x69 }, () => "")
    Object.assign(inn, { 0x02: "TOCLIENT_HELLO", 0x03: "TOCLIENT_AUTH_ACCEPT", 0x20: "TOCLIENT_BLOCKDATA", 0x21: "TOCLIENT_ADDNODE",
        0x31: "TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD", 0x32: "TOCLIENT_ACTIVE_OBJECT_MESSAGES", 0x27: "TOCLIENT_INVENTORY", 0x29: "TOCLIENT_TIME_OF_DAY",
        0x38: "TOCLIENT_MEDIA", 0x3a: "TOCLIENT_NODEDEF", 0x3c: "TOCLIENT_ANNOUNCE_MEDIA", 0x3d: "TOCLIENT_ITEMDEF", 0x49: "TOCLIENT_HUDCHANGE" })
    Object.assign(out, { 0x23: "TOSERVER_PLAYERPOS", 0x24: "TOSERVER_GOTBLOCKS", 0x39: "TOSERVER_INTERACT", 0x31: "TOSERVER_INVENTORY_ACTION" })
    return { in: inn, out: out }
})()

/** A frame's traffic: five numbers a type (type, 0 in / 1 out, count, bytes, largest). */
function mockNet(id: number) {
    const net: number[] = []
    const put = (command: number, dir: number, count: number, bytes: number) => net.push(command, dir, count, Math.round(bytes), Math.round(bytes / count))
    if (id % 4 === 0) put(0x20, 0, 1 + (id % 3), 3800 * (0.6 + wobble(id, 20)) * (id % 300 < 40 ? 4 : 1))
    put(0x32, 0, 3 + (id % 4), 220 * (0.7 + wobble(id, 21) * 0.6))
    if (id % 7 === 0) put(0x31, 0, 1, 410)
    if (id % 30 === 0) put(0x49, 0, 2, 96)
    if (id % 60 === 0) put(0x29, 0, 1, 6)
    put(0x23, 1, 1, 42)
    if (id % 4 === 0) put(0x24, 1, 1, 14)
    return net
}

/** A connection's events, as the engine would keep them, ending at `at`. */
export function mockEvents(at: number) {
    const t = at - 9000
    return [
        { time: t, what: "Connecting", command: -1 },
        { time: t + 40, what: "", command: 0x02 }, { time: t + 95, what: "", command: 0x03 },
        { time: t + 180, what: "", command: 0x3c }, { time: t + 260, what: "", command: 0x38 },
        { time: t + 2100, what: "", command: 0x3a }, { time: t + 2160, what: "", command: 0x3d },
        { time: t + 2400, what: "Content ready", command: -1 }, { time: t + 2600, what: "", command: 0x20 },
        { time: t + 2650, what: "", command: 0x32 }, { time: t + 3900, what: "In the world", command: -1 },
    ]
}

/** Every message over [from, to], laid out from the frames' totals, with heads where the summaries read them. */
export function mockMessages(frames: readonly PerfFrame[], from: number, to: number) {
    const messages: number[] = []
    const heads: string[] = []
    const hex = (n: number) => ((n & 0xffff) >>> 0).toString(16).padStart(4, "0")
    for (const f of frames) {
        if (f.end < from || f.start > to) continue
        for (let k = 0; k + 5 <= f.net.length; k += 5) {
            const [command, dir, count, bytes] = [f.net[k]!, f.net[k + 1]!, f.net[k + 2]!, f.net[k + 3]!]
            for (let c = 0; c < count; c++) {
                const at = f.start + ((c + 1) / (count + 1)) * (f.end - f.start)
                if (at < from || at > to) continue
                messages.push(at, dir, command, command === 0x23 ? 0 : 1, command === 0x32 || command === 0x23 ? 0 : 1, Math.round(bytes / count))
                heads.push(dir === 1 ? "" : command === 0x20 ? hex(f.id % 40 - 20) + hex(-2) + hex(f.id % 13 - 6) + "0000aabbccdd0102" : "00".repeat(16))
            }
        }
    }
    return { messages: messages, heads: heads }
}

/** The units the engine would name for the mock's counters. */
export const MOCK_UNITS: Record<string, string> = {
    "MapBlocks in range": "#", "MapBlocks examined": "#", "MapBlocks drawn": "#", "MapBlocks frustum culled": "#",
    "Irr: drawcalls": "", "Irr: primitives per drawcall": "", "ActiveObjectMgr: CAO count": "#",
    "ParticleManager: particle buffer count": "#", "Game::updateFrame(): update frame": "ms", "Net: RTT": "ms", "Net: jitter": "ms", "Net: lost": "KiB/s", "ServerEnv: ABMs run": "", "ServerEnv: active blocks": "",
}

export function mockCapture(frames: readonly PerfFrame[], from: number, to: number): PerfCapture {
    const names: string[] = []
    const index = (name: string) => {
        const i = names.indexOf(name)
        return i >= 0 ? i : names.push(name) - 1
    }
    const since = frames[Math.max(0, frames.length - 600)]?.start ?? 0 // the engine keeps ~10 s
    const track = (thread: string) => ({ thread: thread, since: since, spans: [] as number[] })
    const main = track("Main")
    const server = track("Server")
    const mesh = track("MeshUpdate")
    const mesh2 = track("MeshUpdate 2")
    const emerge = track("Emerge-0") // idle here: its rail stays
    for (const f of frames) {
        if (f.end < from || f.start > to || f.start < since) continue
        for (const [name, depth, a, b] of layout(f.id, f.start).spans) main.spans.push(index(name), depth, a, b)
        server.spans.push(index("Server::AsyncRunStep"), 0, f.start + 1, f.start + 4)
        server.spans.push(index("ServerEnv: Run SAO::step"), 1, f.start + 1.5, f.start + 2.6)
        server.spans.push(index("Receive"), 0, f.start + 4.2, f.start + 15)
        if (f.id % 3 === 0) mesh.spans.push(index("Client: Mesh making (sum)"), 0, f.start + 2, f.start + 9)
        if (f.id % 3 === 1) mesh2.spans.push(index("Client: Mesh making (sum)"), 0, f.start + 3, f.start + 9)
    }
    return { names: names, tracks: [main, server, mesh, mesh2, emerge] }
}
