// A sandbox the page drives: made in the engine from a spec, stepped live
// at a chosen speed, dropped when the view goes. The engine holds the
// physics (src/sandbox); the page only shows the frames it sends back.

import { shallowRef } from "vue"
import type { BridgeHandle } from "../bridge"
import type { SandboxFrame, SandboxSpec } from "../bridge"

const TICK = 0.1 // s of real time between steps

export const SPEEDS = [1, 10, 60, 600] as const

export function SandboxRun(opts: { bridge: BridgeHandle }) {
    const frame = shallowRef<SandboxFrame | null>(null)
    const problem = shallowRef<string | null>(null)
    const running = shallowRef(true)
    const speed = shallowRef<number>(SPEEDS[0])
    const shown = shallowRef<number | null>(null) // the sandbox made, for the scene to draw
    let id: number | null = null, spec: SandboxSpec | null = null, timer = 0, busy = false, generation = 0

    async function drop() {
        clearInterval(timer)
        timer = 0
        generation++
        const was = id
        id = null
        shown.value = null
        if (was !== null) await opts.bridge.send("sandbox.drop", { id: was })
    }

    async function tick(mine: number) {
        if (busy || id === null || !running.value) return
        busy = true
        try {
            const f = await opts.bridge.send("sandbox.step", { id: id, seconds: TICK * speed.value })
            if (mine === generation) frame.value = f
        } catch (err) {
            problem.value = err instanceof Error ? err.message : String(err)
            running.value = false
        }
        busy = false
    }

    async function start(next: SandboxSpec) {
        await drop()
        spec = next
        frame.value = null
        problem.value = null
        const mine = generation
        try {
            const made = await opts.bridge.send("sandbox.create", next)
            if (mine !== generation) { // replaced while it was being made
                await opts.bridge.send("sandbox.drop", { id: made.id })
                return
            }
            id = made.id
            shown.value = made.id
            frame.value = await opts.bridge.send("sandbox.step", { id: made.id, seconds: 0.001 })
            timer = window.setInterval(() => tick(mine), TICK * 1000)
        } catch (err) {
            problem.value = err instanceof Error ? err.message : String(err)
        }
    }

    return {
        frame: frame,
        shown: shown,
        problem: problem,
        running: running,
        speed: speed,
        start: start,
        restart() { if (spec) start(spec) },
        dispose: drop,
    }
}

export type SandboxRunHandle = ReturnType<typeof SandboxRun>
