import type { PerfFrame } from "../../bridge/index.ts"

/**
 * The Live tab's per-system charts, as the engine's graphs were: one each, its
 * time in µs on a scale of its own. A table, so a system is one row.
 *
 * `thread`: the kind of thread it runs on ("Main", "Server", a pool's name;
 * "*" any). `zones`: its zone's name, or names it's gone by, the first a
 * thread has counted (never summed: one may be inside another). Or
 * `counter`: a part timed by hand and reported in µs (the water's stages).
 */
export const SYSTEMS = [
    { title: "Physics (player)", thread: "Main", zones: ["Body move"] },
    { title: "Ground read", thread: "Main", zones: ["Physics: ground read"] },
    { title: "Water", thread: "Main", zones: ["Water all"] },
    { title: "Water: look", counter: "Water look" },
    { title: "Water: run", counter: "Water run" },
    { title: "Water: flat", counter: "Water flat" },
    { title: "Water: mesh", counter: "Water mesh" },
    { title: "Far view", thread: "Main", zones: ["Far view update"] },
    { title: "Far view: install", thread: "Main", zones: ["Far view: install"] },
    { title: "Far view: upload", thread: "Main", zones: ["Far view: upload"] },
    { title: "Far view: repaint", thread: "Main", zones: ["Far view: repaint"] },
    { title: "Far view: choose", thread: "Main", zones: ["Far view: choose"] },
    { title: "Far view: want", thread: "Main", zones: ["Far view: want"] },
    { title: "Far view: quarter", thread: "Main", zones: ["Far view: quarter"] },
    { title: "Far view: evict", thread: "Main", zones: ["Far view: evict"] },
    { title: "Draw list", thread: "Main", zones: ["CM::updateDrawList()"] },
    { title: "Draw list: block loop", counter: "Draw list: loop" },
    { title: "Draw list: placing blocks", counter: "Draw list: transform" },
    { title: "Render", thread: "Main", zones: ["Draw3D"] },
    { title: "Render: map", thread: "Main", zones: ["renderMap"] },
    { title: "Render: far view", thread: "Main", zones: ["Far view render"] },
    { title: "Render: space", thread: "Main", zones: ["Space render"] },
    { title: "Render: water", thread: "Main", zones: ["Water render"] },
    { title: "Render: far view mask", thread: "Main", zones: ["Far view: mask"] },
    { title: "Render: far view place", thread: "Main", zones: ["Far view: place"] },
    { title: "Render: far view under", thread: "Main", zones: ["Far view: under"] },
    { title: "Render: far view land", thread: "Main", zones: ["Far view: land"] },
    // the GPU's own time (render/gputime: read a few frames late)
    { title: "GPU: frame", counter: "GPU: frame" },
    { title: "GPU: world (Draw3D)", counter: "GPU: Draw3D" },
    { title: "GPU: map solid", counter: "GPU: Map: solid" },
    { title: "GPU: map transparent", counter: "GPU: Map: transparent" },
    { title: "GPU: far view", counter: "GPU: Far view" },
    { title: "GPU: far view under", counter: "GPU: Far view: under" },
    { title: "GPU: far view land", counter: "GPU: Far view: land" },
    { title: "GPU: space", counter: "GPU: Space" },
    { title: "GPU: water", counter: "GPU: Water" },
    { title: "GPU: post-processing", counter: "GPU: PostProcessingStep" },
    { title: "GPU: HUD", counter: "GPU: DrawHUD" },
    { title: "GPU: UI page", counter: "GPU: UI page" },
    { title: "GPU: debugger", counter: "GPU: Debugger" },
    { title: "UI", thread: "Main", zones: ["UI"] },
    { title: "UI: inbox", thread: "Main", zones: ["UI: inbox"] },
    { title: "UI: state", thread: "Main", zones: ["UI: state"] },
    { title: "UI: page paint", thread: "*", zones: ["Page paint"] },
    { title: "UI: page upload", thread: "Main", zones: ["Page upload"] },
    { title: "UI: page frame gap", counter: "UI: page frame gap" },
    { title: "UI: page longest task", counter: "UI: page longest task" },
    { title: "UI: input to paint", counter: "UI: input to paint" },
    { title: "UI: input to screen", counter: "UI: input to screen" },
    { title: "Present", thread: "Main", zones: ["Present"] },
    { title: "Server step", thread: "Server", zones: ["Server::AsyncRunStep()", "AsyncRunStep"] },
    { title: "Server: environment", thread: "Server", zones: ["Server: environment"] },
    { title: "Server: sending blocks", thread: "Server", zones: ["Server::SendBlocks(): Collect list"] },
    { title: "Server: matter", thread: "Server", zones: ["Server: matter"] },
    { title: "Server: mods", thread: "Server", zones: ["Server: mods (sidecar)"] },
    { title: "Server: weather", thread: "Server", zones: ["Server: weather"] },
    { title: "Server: players", thread: "Server", zones: ["Server: players"] },
    { title: "Ground shapes", thread: "Physics ground", zones: ["Physics: ground shape"] },
    { title: "Meshing", thread: "MeshUpdate", zones: ["MapBlockMesh"] },
    { title: "Far tiles", thread: "*", zones: ["Far view: tile built"] },
] as const satisfies readonly ({ title: string; thread: string; zones: readonly string[] } | { title: string; counter: string })[]

export type System = (typeof SYSTEMS)[number]

/** "Emerge-3" and "MeshUpdate 2" are their kind's. */
export const threadKind = (thread: string) => thread.replace(/[-\s]?\d+$/, "")

/**
 * A system's time in a frame (ms), every thread of its kind together. Every
 * system's found at once the first a frame's asked, and kept with it: a chart
 * asks of thousands of frames each draw, and finding it each time (the
 * threads' entries, their kinds by a regex) was garbage enough that the
 * page's collector stalled its frames, the charts juddering.
 */
export function systemTime(system: System, f: PerfFrame): number {
    let times = known.get(f)
    if (!times) known.set(f, (times = measure(f)))
    return times[INDEX.get(system)!]!
}

const INDEX = new Map<System, number>(SYSTEMS.map((s, i) => [s, i]))
const known = new WeakMap<PerfFrame, Float64Array>()
const kinds = new Map<string, string>() // a thread's name to its kind, once each

function measure(f: PerfFrame): Float64Array {
    const out = new Float64Array(SYSTEMS.length)
    for (let i = 0; i < SYSTEMS.length; i++) {
        const system = SYSTEMS[i]!
        if ("counter" in system) out[i] = (f.counters[system.counter] ?? 0) / 1000 // µs
    }
    for (const thread in f.threads) {
        let kind = kinds.get(thread)
        if (kind === undefined) kinds.set(thread, (kind = threadKind(thread)))
        const parts = f.threads[thread]!
        for (let i = 0; i < SYSTEMS.length; i++) {
            const system = SYSTEMS[i]!
            if ("counter" in system || (system.thread !== "*" && kind !== system.thread)) continue
            for (const zone of system.zones) {
                const v = parts[zone]
                if (v !== undefined) {
                    out[i] = out[i]! + v
                    break
                }
            }
        }
    }
    return out
}
