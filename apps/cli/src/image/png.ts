import { deflateSync, inflateSync } from "node:zlib"
import { err } from "../errors"
import { raster, type Raster } from "./raster"

const SIGNATURE = Uint8Array.of(137, 80, 78, 71, 13, 10, 26, 10)

/** png — pictures as PNG files: written as RGBA, read from the forms textures come in. */
export const png = {
    encode(r: Raster): Uint8Array<ArrayBuffer> {
        const stride = r.width * 4
        const rows = new Uint8Array((stride + 1) * r.height)
        for (let y = 0; y < r.height; y++) rows.set(r.data.subarray(y * stride, (y + 1) * stride), y * (stride + 1) + 1)
        const header = new Uint8Array(13)
        const view = new DataView(header.buffer)
        view.setUint32(0, r.width)
        view.setUint32(4, r.height)
        header.set([8, 6, 0, 0, 0], 8) // 8 bits, RGBA, deflate, no filter method, not interlaced
        return concat([SIGNATURE, chunk("IHDR", header), chunk("IDAT", deflateSync(rows)), chunk("IEND", new Uint8Array(0))])
    },

    /** Grey, RGB, palette, grey+alpha or RGBA; 8 bits (palettes and grey at 1–8), not interlaced. */
    decode(bytes: Uint8Array, name: string): Raster {
        const bad = (why: string) => err("ASSET_MISSING", { detail: `${name} isn't a PNG terra reads: ${why}` })
        if (bytes.length < 8 || !SIGNATURE.every((b, i) => bytes[i] === b)) throw bad("no PNG signature")
        const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
        let at = 8
        let width = 0, height = 0, depth = 0, type = 0
        let palette: Uint8Array = new Uint8Array(0), alphas: Uint8Array = new Uint8Array(0)
        const data: Uint8Array[] = []
        while (at + 8 <= bytes.length) {
            const length = view.getUint32(at)
            const kind = String.fromCharCode(...bytes.subarray(at + 4, at + 8))
            const body = bytes.subarray(at + 8, at + 8 + length)
            if (kind === "IHDR") {
                width = view.getUint32(at + 8); height = view.getUint32(at + 12)
                depth = body[8]!; type = body[9]!
                if (body[12] !== 0) throw bad("interlaced")
            } else if (kind === "PLTE") palette = body
            else if (kind === "tRNS") alphas = body
            else if (kind === "IDAT") data.push(body)
            else if (kind === "IEND") break
            at += 12 + length
        }
        const channels = ({ 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 } as Record<number, number>)[type]
        if (!channels || (depth !== 8 && !(depth < 8 && (type === 0 || type === 3)))) throw bad(`colour type ${type} at ${depth} bits`)
        const bits = channels * depth
        const stride = Math.ceil((width * bits) / 8)
        const step = Math.max(1, bits / 8)
        const rows = unfilter(inflateSync(concat(data)), stride, height, step)
        const out = raster.blank(width, height)
        for (let y = 0; y < height; y++)
            for (let x = 0; x < width; x++) {
                const row = rows.subarray(y * stride, (y + 1) * stride)
                const sample = (k: number) => depth === 8 ? row[x * channels + k]! : (row[(x * depth) >> 3]! >> (8 - depth - ((x * depth) & 7))) & ((1 << depth) - 1)
                if (type === 3) {
                    const p = sample(0)
                    raster.set(out, x, y, [palette[p * 3]!, palette[p * 3 + 1]!, palette[p * 3 + 2]!, p < alphas.length ? alphas[p]! : 255])
                } else if (type === 0 || type === 4) {
                    const g = depth === 8 ? sample(0) : Math.round(sample(0) * 255 / ((1 << depth) - 1))
                    raster.set(out, x, y, [g, g, g, type === 4 ? sample(1) : 255])
                } else raster.set(out, x, y, [sample(0), sample(1), sample(2), type === 6 ? sample(3) : 255])
            }
        return out
    },
}

/** The scanlines with PNG's per-row filters undone. */
function unfilter(raw: Uint8Array, stride: number, height: number, step: number): Uint8Array {
    const out = new Uint8Array(stride * height)
    for (let y = 0; y < height; y++) {
        const filter = raw[y * (stride + 1)]!
        const src = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1))
        const row = out.subarray(y * stride, (y + 1) * stride)
        const up = y > 0 ? out.subarray((y - 1) * stride, y * stride) : new Uint8Array(stride)
        for (let i = 0; i < stride; i++) {
            const a = i >= step ? row[i - step]! : 0
            const b = up[i]!
            const c = i >= step ? up[i - step]! : 0
            const add = filter === 1 ? a : filter === 2 ? b : filter === 3 ? (a + b) >> 1 : filter === 4 ? paeth(a, b, c) : 0
            row[i] = (src[i]! + add) & 255
        }
    }
    return out
}

function paeth(a: number, b: number, c: number): number {
    const p = a + b - c
    const pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c)
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c
}

function chunk(kind: string, body: Uint8Array): Uint8Array {
    const out = new Uint8Array(12 + body.length)
    const view = new DataView(out.buffer)
    view.setUint32(0, body.length)
    out.set(new TextEncoder().encode(kind), 4)
    out.set(body, 8)
    view.setUint32(8 + body.length, Bun.hash.crc32(out.subarray(4, 8 + body.length)) >>> 0)
    return out
}

function concat(parts: readonly Uint8Array[]): Uint8Array<ArrayBuffer> {
    const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0))
    let at = 0
    for (const p of parts) { out.set(p, at); at += p.length }
    return out
}
