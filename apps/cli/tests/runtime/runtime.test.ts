import { describe, expect, test } from "bun:test"
import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import type { ManifestData } from "../../src/terra"
import { Runtime } from "../../src/runtime"
import { Call, Event, Level, MemoryWire, Op } from "../../src/runtime"

/** A mod whose server half is `code`, already bundled. */
function mod(name: string, code: string) {
    const root = mkdtempSync(`${tmpdir()}/terra-runtime-`)
    mkdirSync(`${root}/assets/textures`, { recursive: true })
    writeFileSync(`${root}/assets/textures/brick.png`, "")
    writeFileSync(`${root}/server.js`, code)
    return { name: name, prefix: name.split("/")[1]!, version: "1.0.0", root: root, server: `${root}/server.js`, settings: {} }
}

describe("Runtime", () => {
    test("loads, declares, then serves windows until stopped", async () => {
        const hello = mod("@t/hello", `
            content.blocks.create("hello:brick", { matter: "granite", texture: "brick.png" })
            game.players.on("join", p => p.tell("Hello, " + p.name))
            console.log("loaded")
        `)
        const manifest: ManifestData = { version: 1, mods: [hello] }
        const memory = MemoryWire({ answer: call => (call === Call["players.list"] ? [] : null) })
        memory.send(Op.step, { now: 10, dt: 0.1, events: [[Event.join, "@ana"]] })
        memory.send(Op.stop, null)

        await Runtime({ manifest: manifest, wire: memory.wire }).run()

        const ops = memory.written().map(f => f.op)
        expect(ops).toEqual([Op.log, Op.log, Op.content, Op.call, Op.call, Op.idle])
        expect(memory.written()[1]?.payload).toEqual([Level.info, "@t/hello", "loaded"])
        expect(memory.written()[2]?.payload).toMatchObject({ blocks: [{ name: "hello:brick" }] })
        expect(memory.written()[4]?.payload).toEqual([Call["player.tell"], "@ana", "Hello, @ana"])
        // and leaves the process as it found it
        expect("content" in globalThis).toBe(false)
    })

    test("a mod that throws while loading stops everything", async () => {
        const broken = mod("@t/broken", `throw new Error("no")`)
        const memory = MemoryWire()
        await expect(Runtime({ manifest: { version: 1, mods: [broken] }, wire: memory.wire }).run())
            .rejects.toMatchObject({ code: "MOD_LOAD_FAILED" })
        expect(memory.written().find(f => f.op === Op.content)).toBeUndefined()
    })
})
