import { err } from "../../errors"
import { Op } from "./ops"
import type { Frame, OpCode, WireT } from "./wire"

type MemoryWireOpts = {
    /** The engine's answer to a call that wants one: [call, ...args] → value. */
    answer?: (call: number, args: readonly unknown[]) => unknown
}

/**
 * MemoryWire — a wire with the engine played in memory: frames to read are queued, what
 * the sidecar writes is kept, and calls are answered by `answer`. For tests, and anything
 * that runs a Runtime without an engine.
 */
export function MemoryWire(opts: MemoryWireOpts = {}) {
    const incoming: Frame[] = []
    const written: Frame[] = []

    function read(): Frame {
        const frame = incoming.shift()
        if (!frame) throw err("ENGINE_GONE", { detail: "the memory wire has nothing more to read" })
        return frame
    }

    function write(op: OpCode, id: number, payload: unknown): void {
        written.push({ op: op, id: id, payload: payload })
        if (op !== Op.call || id === 0) return
        if (!opts.answer) throw err("PROTOCOL", { detail: "a call wants an answer, and this memory wire has none" })
        const [call, ...args] = Array.isArray(payload) ? payload : []
        // answered at once, ahead of anything queued: as the engine does inside a window
        incoming.unshift({ op: Op.reply, id: id, payload: [true, opts.answer(Number(call), args)] })
    }

    /** Queue a frame from the engine. */
    function send(op: OpCode, payload: unknown): void {
        incoming.push({ op: op, id: 0, payload: payload })
    }

    const wire: WireT = { read: read, write: write }

    return { wire: wire, send: send, written: (): readonly Frame[] => written }
}

export type MemoryWireT = ReturnType<typeof MemoryWire>
