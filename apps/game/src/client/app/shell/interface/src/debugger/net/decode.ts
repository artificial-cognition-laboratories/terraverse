import { short } from "./categories.ts"

/**
 * One-line summaries of the messages that matter most, read from the head of
 * their payload (the engine keeps the first 16 bytes of each incoming one).
 * A table: a type with no row here shows its size alone. The engine writes numbers
 * big-endian; a v3s16 is three s16s.
 */
type Reader = { u16: (at: number) => number; s16: (at: number) => number; size: number }
const SUMMARIES: Record<string, (r: Reader) => string | null> = {
    BLOCKDATA: (r) => (r.size >= 6 ? `block (${r.s16(0)}, ${r.s16(2)}, ${r.s16(4)})` : null),
    ADDNODE: (r) => (r.size >= 6 ? `node at (${r.s16(0)}, ${r.s16(2)}, ${r.s16(4)})` : null),
    REMOVENODE: (r) => (r.size >= 6 ? `removed at (${r.s16(0)}, ${r.s16(2)}, ${r.s16(4)})` : null),
    HP: (r) => (r.size >= 2 ? `hp ${r.u16(0)}` : null),
    TIME_OF_DAY: (r) => (r.size >= 2 ? `time ${r.u16(0)}` : null),
}

export function bytesOf(hex: string) {
    const out = new Uint8Array(hex.length >> 1)
    for (let i = 0; i < out.length; i++) out[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16)
    return out
}

export function summarise(name: string, head: string): string | null {
    const read = SUMMARIES[short(name)]
    if (!read || !head) return null
    const view = new DataView(bytesOf(head).buffer)
    return read({ u16: (at) => view.getUint16(at), s16: (at) => view.getInt16(at), size: view.byteLength })
}
