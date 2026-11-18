import type { PerfFrame, PerfZones } from "../../bridge/index.ts"

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
        { name: "UI", ms: 0.9, inside: [{ name: "Chromium", ms: 0.4 }, { name: "Page upload", ms: 0.3 }] },
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
    const parts: Record<string, number> = {}
    for (const [name, , a, b] of spans) parts[name] = (parts[name] ?? 0) + (b - a)
    return { id: id, start: start, end: end, parts: parts }
}

export function mockZones(frames: readonly PerfFrame[], from: number, to: number): PerfZones {
    const names: string[] = []
    const index = (name: string) => {
        const i = names.indexOf(name)
        return i >= 0 ? i : names.push(name) - 1
    }
    const since = (frames[Math.max(0, frames.length - 600)]?.start ?? 0) // the engine keeps ~10 s
    const main: PerfZones["tracks"][number] = { thread: "Main", since: since, spans: [] }
    const server: PerfZones["tracks"][number] = { thread: "Server", since: since, spans: [] }
    const mesh: PerfZones["tracks"][number] = { thread: "MeshUpdateWorker1", since: since, spans: [] }
    for (const f of frames) {
        if (f.end < from || f.start > to || f.start < since) continue
        for (const [name, depth, a, b] of layout(f.id, f.start).spans) main.spans.push([index(name), depth, a, b])
        server.spans.push([index("Server::AsyncRunStep"), 0, f.start + 1, f.start + 4])
        server.spans.push([index("ServerEnv: Run SAO::step"), 1, f.start + 1.5, f.start + 2.6])
        if (f.id % 3 === 0) mesh.spans.push([index("Client: Mesh making (sum)"), 0, f.start + 2, f.start + 9])
    }
    return { names: names, tracks: [main, server, mesh].filter((t) => t.spans.length) }
}
