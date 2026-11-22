import { describe, expect, test } from "bun:test"
import { mkdtempSync, readFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { png, raster } from "../../src/image"
import type { ManifestMod } from "../../src/terra"
import { Channel } from "../../src/runtime/channel"
import { Running } from "../../src/runtime/running"
import { MemoryWire } from "../../src/runtime/wire"
import { Content } from "../../src/runtime/api/content"

function setup() {
    const root = mkdtempSync(`${tmpdir()}/terra-art-`)
    const mod: ManifestMod = { name: "@t/bench", prefix: "bench", version: "1.0.0", root: root, server: null, settings: {} }
    const channel = Channel({ wire: MemoryWire().wire })
    const running = Running({ channel: channel })
    const content = Content({ running: running, channel: channel })
    const loading = <T>(fn: () => T) => running.load(mod, async () => fn())
    return { content: content, loading: loading }
}

const ROWS = Array.from({ length: 16 }, (_, y) => y === 0 ? "ab" + ".".repeat(14) : ".".repeat(16))

describe("art", () => {
    test("a drawn texture is a block's media, named by what's in it", async () => {
        const { content, loading } = setup()
        await loading(() => {
            const t = content.art.texture({ palette: { a: "#f00", b: "#0000ff80" }, layers: [{ fill: "#202020" }], rows: ROWS })
            content.api.blocks.create("bench:top", { matter: "wood", texture: t, description: "Bench top" })
        })
        const table = content.seal()
        expect(table.blocks[0]!.description).toBe("Bench top")
        expect(new Set(table.blocks[0]!.tiles).size).toBe(1)
        const file = table.media[0]!
        expect(file.name).toMatch(/^bench--art-[0-9a-f]{16}\.png$/)
        const r = png.decode(new Uint8Array(readFileSync(file.path)), file.name)
        expect(raster.get(r, 0, 0)).toEqual([255, 0, 0, 255])
        expect(raster.get(r, 1, 0)[2]).toBeGreaterThan(raster.get(r, 1, 0)[0]) // blue over grey
        expect(raster.get(r, 5, 5)).toEqual([32, 32, 32, 255])
    })

    test("the same picture twice is one file; noise is the same every run", async () => {
        const { content, loading } = setup()
        const def = { layers: [{ noise: ["#333", "#666", "#999"], seed: 4 }] } as const
        await loading(() => {
            content.api.blocks.create("bench:a", { matter: "granite", texture: content.art.texture(def) })
            content.api.blocks.create("bench:b", { matter: "granite", texture: content.art.texture(def) })
        })
        expect(content.seal().media.length).toBe(1)
    })

    test("an item, its icon drawn", async () => {
        const { content, loading } = setup()
        await loading(() => content.api.items.create("bench:chisel", { icon: content.art.texture({ palette: { a: "#888" }, rows: ROWS.map(r => r.replace("b", "a")), layers: [{ outline: "#000" }] }) }))
        const table = content.seal()
        expect(table.items).toEqual([{ name: "bench:chisel", description: "Chisel", icon: table.media[0]!.name }])
    })

    test("a bad grid says where", async () => {
        const { content, loading } = setup()
        await expect(loading(() => content.art.texture({ palette: { a: "#888", b: "#999" }, rows: ROWS.map((r, y) => y === 3 ? "x" + r.slice(1) : r) })))
            .rejects.toMatchObject({ code: "ART_INVALID", message: expect.stringContaining("rows[3][0]") })
        await expect(loading(() => content.art.texture({ rows: ["...."] })))
            .rejects.toMatchObject({ code: "ART_INVALID", message: expect.stringContaining("16 strings") })
        await expect(loading(() => content.art.texture({ layers: [{ fill: "red" }] })))
            .rejects.toMatchObject({ code: "ART_INVALID" })
    })

    test("only what art drew passes as a texture", async () => {
        const { content, loading } = setup()
        await expect(loading(() => content.api.blocks.create("bench:x", { matter: "wood", texture: Object.freeze({ size: 16 }) })))
            .rejects.toMatchObject({ code: "ASSET_MISSING" })
    })
})
