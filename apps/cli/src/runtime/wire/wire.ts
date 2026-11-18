import { readSync, writeSync } from "node:fs"
import { Packr } from "msgpackr"
import { err } from "../../errors"
import { Op } from "./ops"

export type OpCode = (typeof Op)[keyof typeof Op]
export type Frame = { readonly op: OpCode; readonly id: number; readonly payload: unknown }

/** What the channel needs of a wire. A test passes its own. */
export type WireT = {
    /** The next frame. Blocks until it comes. */
    read(): Frame
    write(op: OpCode, id: number, payload: unknown): void
}

/**
 * Wire — frames on the socket the engine handed us: a u32 (little endian) length, then
 * MessagePack [op, id, payload].
 *
 * Reads block. Between windows the sidecar has nothing to do but wait for the next one,
 * and inside a window a call waits for its answer: that's what keeps a mod's world reads
 * synchronous.
 */
export function Wire(opts: { fd: number }): WireT {
    // plain MessagePack: the engine's reader knows no extensions
    const packr = new Packr({ useRecords: false, mapsAsObjects: true, variableMapSize: true })

    function exactly(n: number): Buffer {
        const buf = Buffer.allocUnsafe(n)
        let got = 0
        while (got < n) {
            const r = readSync(opts.fd, buf, got, n - got, null)
            if (r === 0) throw err("ENGINE_GONE")
            got += r
        }
        return buf
    }

    function read(): Frame {
        const length = exactly(4).readUInt32LE(0)
        const decoded: unknown = packr.unpack(exactly(length))
        if (!Array.isArray(decoded) || decoded.length !== 3)
            throw err("PROTOCOL", { detail: "a frame that isn't [op, id, payload]" })
        const [op, id, payload] = decoded
        if (!isOp(op) || typeof id !== "number")
            throw err("PROTOCOL", { detail: `a frame with op ${op} and id ${id}` })
        return { op: op, id: id, payload: payload }
    }

    function write(op: OpCode, id: number, payload: unknown): void {
        const body = packr.pack([op, id, payload])
        const frame = Buffer.allocUnsafe(4 + body.length)
        frame.writeUInt32LE(body.length, 0)
        body.copy(frame, 4)
        let sent = 0
        while (sent < frame.length) sent += writeSync(opts.fd, frame, sent, frame.length - sent)
    }

    return { read: read, write: write }
}

const opCodes: ReadonlySet<number> = new Set(Object.values(Op))
const isOp = (n: unknown): n is OpCode => typeof n === "number" && opCodes.has(n)
