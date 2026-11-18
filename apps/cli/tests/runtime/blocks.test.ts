import { describe, expect, test } from "bun:test"
import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import type { ManifestMod } from "../../src/terra"
import { Channel } from "../../src/runtime/channel"
import { Running } from "../../src/runtime/running"
import { MemoryWire } from "../../src/runtime/wire"
import { Content } from "../../src/runtime/api/content"

function setup() {
    const root = mkdtempSync(`${tmpdir()}/terra-blocks-`)
    mkdirSync(`${root}/assets/textures/stone`, { recursive: true })
    writeFileSync(`${root}/assets/textures/brick.png`, "")
    writeFileSync(`${root}/assets/textures/stone/top.png`, "")
    const mod: ManifestMod = { name: "@t/kilns", prefix: "kilns", version: "1.0.0", root: root, server: null, settings: {} }
    const channel = Channel({ wire: MemoryWire().wire })
    const running = Running({ channel: channel })
    const content = Content({ running: running, channel: channel })
    const loading = <T>(fn: () => T) => running.load(mod, async () => fn())
    return { content: content, loading: loading }
}

describe("blocks", () => {
    test("a block, its faces, and its textures as the mod's media", async () => {
        const { content, loading } = setup()
        await loading(() => content.api.blocks.create("kilns:brick", { matter: "granite", texture: { top: "stone/top.png", sides: "brick.png" } }))
        const table = content.seal()
        expect(table.blocks).toEqual([{
            name: "kilns:brick",
            description: "Brick",
            tiles: ["kilns--stone--top.png", "kilns--brick.png", "kilns--brick.png", "kilns--brick.png", "kilns--brick.png", "kilns--brick.png"],
            matter: "granite",
            share: 1,
        }])
        expect(table.media.map(m => m.name).sort()).toEqual(["kilns--brick.png", "kilns--stone--top.png"])
    })

    test("only under the mod's own prefix", async () => {
        const { content, loading } = setup()
        await expect(loading(() => content.api.blocks.create("craft:bench", { matter: "oak", texture: "brick.png" })))
            .rejects.toMatchObject({ code: "NAME_NOT_YOURS" })
    })

    test("a name once", async () => {
        const { content, loading } = setup()
        await loading(() => content.api.blocks.create("kilns:brick", { matter: "granite", texture: "brick.png" }))
        await expect(loading(() => content.api.blocks.create("kilns:brick", { matter: "granite", texture: "brick.png" })))
            .rejects.toMatchObject({ code: "NAME_TAKEN" })
    })

    test("textures that are there", async () => {
        const { content, loading } = setup()
        await expect(loading(() => content.api.blocks.create("kilns:x", { matter: "granite", texture: "nope.png" })))
            .rejects.toMatchObject({ code: "ASSET_MISSING" })
        await expect(loading(() => content.api.blocks.create("kilns:y", { matter: "granite", texture: "../../../etc/passwd" })))
            .rejects.toMatchObject({ code: "ASSET_MISSING" })
    })

    test("only while mods load", async () => {
        const { content, loading } = setup()
        expect(() => content.api.blocks.create("kilns:brick", { matter: "granite", texture: "brick.png" }))
            .toThrow(expect.objectContaining({ code: "CONTENT_SEALED" }))
        content.seal()
        await expect(loading(() => content.api.blocks.create("kilns:late", { matter: "granite", texture: "brick.png" })))
            .rejects.toMatchObject({ code: "CONTENT_SEALED" })
    })
})
