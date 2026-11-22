import { describe, expect, test } from "bun:test"
import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { png, raster } from "../../src/image"
import { DryRun } from "../../src/terra/art/dry"
import { GameBlocks } from "../../src/terra/art/game"
import { sheet } from "../../src/terra/art/sheet"
import type { ManifestMod } from "../../src/terra"

function face(c: readonly [number, number, number, number]) {
    const r = raster.blank(16, 16)
    raster.rect(r, 0, 0, 16, 16, c)
    return r
}

describe("the game's blocks, to compare with", () => {
    function share() {
        const root = mkdtempSync(`${tmpdir()}/terra-share-`)
        mkdirSync(`${root}/world`, { recursive: true })
        mkdirSync(`${root}/textures/base/pack`, { recursive: true })
        writeFileSync(`${root}/world/blocks.json`, JSON.stringify({ blocks: { "core:stone": { tiles: ["stone.png^[crack"] }, "core:log": { tiles: ["top.png", "top.png", "bark.png"] } } }))
        for (const [file, c] of [["stone.png", [150, 150, 130, 255]], ["top.png", [200, 160, 90, 255]], ["bark.png", [90, 50, 30, 255]]] as const)
            writeFileSync(`${root}/textures/base/pack/${file}`, png.encode(face(c)))
        return GameBlocks({ share: root })
    }

    test("by name, with or without core:, their faces as Luanti reads tiles", () => {
        const [stone, log] = share().entries(["stone", "core:log"])
        expect(stone).toMatchObject({ type: "block", source: "game", name: "core:stone" })
        if (log?.type !== "block") throw new Error("a block")
        expect(raster.get(log.top, 0, 0)).toEqual([200, 160, 90, 255])
        expect(raster.get(log.side, 0, 0)).toEqual([90, 50, 30, 255])
    })

    test("an unknown name lists the ones there are", () => {
        expect(() => share().entries(["oak_log"])).toThrow(expect.objectContaining({ code: "BLOCK_UNKNOWN", message: expect.stringContaining("stone, log") }))
    })
})

describe("the sheet", () => {
    test("the game's, a reference's and the mod's side by side", () => {
        const block = { type: "block" as const, top: face([1, 2, 3, 255]), bottom: face([1, 2, 3, 255]), side: face([4, 5, 6, 255]) }
        const out = sheet.compose([
            { ...block, source: "game", name: "core:stone" },
            { type: "item", source: "reference", name: "w:axe", icon: face([7, 8, 9, 255]) },
            { ...block, source: "mod", name: "m:brick" },
        ])
        expect(out.width).toBe(3 * 380)
        expect(out.height).toBe(260)
    })
})

describe("a dry run", () => {
    test("loading again sees the new build, not the one it cached", async () => {
        const root = mkdtempSync(`${tmpdir()}/terra-dry-`)
        const server = `${root}/server.js`
        const mod: ManifestMod = { name: "m", prefix: "m", version: "1.0.0", root: root, server: server, settings: {} }
        const dry = DryRun({})
        const build = (description: string) => writeFileSync(server, `content.items.create("m:axe", { description: ${JSON.stringify(description)}, icon: art.texture({ layers: [{ fill: "#888" }] }) })`)
        build("First")
        expect((await dry.load({ version: 1, mods: [mod] })).table.items[0]?.description).toBe("First")
        build("Second")
        expect((await dry.load({ version: 1, mods: [mod] })).table.items[0]?.description).toBe("Second")
    })
})
