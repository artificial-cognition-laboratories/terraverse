import type { PerfFrame } from "../../bridge/index.ts"

/**
 * The Live tab's per-system charts, as Luanti's graphs were: one each, its
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
    { title: "Draw list", thread: "Main", zones: ["CM::updateDrawList()"] },
    { title: "Render", thread: "Main", zones: ["Draw3D"] },
    { title: "Render: map", thread: "Main", zones: ["renderMap"] },
    { title: "Render: far view", thread: "Main", zones: ["Far view render"] },
    { title: "Render: space", thread: "Main", zones: ["Space render"] },
    { title: "Render: water", thread: "Main", zones: ["Water render"] },
    { title: "UI", thread: "Main", zones: ["UI"] },
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

/** A system's time in a frame (ms), every thread of its kind together. */
export function systemTime(system: System, f: PerfFrame): number {
    if ("counter" in system) return (f.counters[system.counter] ?? 0) / 1000 // µs
    let sum = 0
    for (const [thread, parts] of Object.entries(f.threads)) {
        if (system.thread !== "*" && threadKind(thread) !== system.thread) continue
        for (const zone of system.zones) {
            const v = parts[zone]
            if (v !== undefined) {
                sum += v
                break
            }
        }
    }
    return sum
}
