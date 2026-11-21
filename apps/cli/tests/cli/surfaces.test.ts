import { describe, expect, test } from "bun:test"
import { mkdtempSync, realpathSync } from "node:fs"
import { tmpdir } from "node:os"
import { join } from "node:path"
import { Renderer } from "@arcforge/arcline"
import { Cli } from "../../src/cli"
import { Presentation, Shown } from "../../src/cli/presentation"
import { parseArgs } from "../../src/cli/args"
import { err } from "../../src/errors"

const REGISTRY = { api: "https://registry.test", web: "https://registry.test/packages" }
const presentation = (argv: string[] = []) => {
    const capture = Renderer.capture({ columns: 100 })
    const p = Presentation({ capture: capture, registry: REGISTRY })
    p.mode.configure(parseArgs(argv))
    return { p: p, out: () => capture.output() }
}

describe("surfaces", () => {
    test("publish counts its steps, then says where it went", () => {
        const { p, out } = presentation()
        const s = p.surfaces.publish("@you/kilns")
        s.preparing()
        s.publishing("verifying")
        s.publishing("packing")
        s.publishing("uploading", "v0.1.0")
        s.finish({ version: "0.1.0", assets: [{ path: "docs/kiln.png", size: "12 KB" }], ms: 1200 })
        const text = out()
        for (const label of ["Preparing", "Verifying", "Packing", "Uploading", "docs/kiln.png", "https://registry.test", "published @you/kilns@0.1.0"])
            expect(text).toContain(label)
    })

    test("a failure is drawn under the steps that got there, once", () => {
        const { p, out } = presentation()
        const s = p.surfaces.publish("@you/kilns")
        s.preparing()
        s.publishing("verifying")
        const thrown = s.fail(err("PUBLISH_INVALID", { detail: "no version" }))
        expect(thrown).toBeInstanceOf(Shown)
        expect(out()).toContain("Verifying")
        expect(out()).toContain("no version")
    })

    test("a step never reported is skipped, not timed", () => {
        const { p, out } = presentation()
        const s = p.surfaces.prepare("/srv", "server")
        s.preparing("config")
        s.finish({ title: "Srv", modules: [], ms: 5 })
        expect(out()).toContain("Srv ready")
    })

    test("--json draws nothing, and a failure is left to the boundary", () => {
        const { p, out } = presentation(["publish", "--json"])
        const s = p.surfaces.publish("@you/kilns")
        s.preparing()
        const thrown = s.fail(err("PUBLISH_INVALID"))
        expect(thrown).not.toBeInstanceOf(Shown)
        expect(out()).toBe("")
    })
})

describe("terra mod init", () => {
    test("shows the folder it made, and what to run in it", async () => {
        const dir = mkdtempSync(join(realpathSync.native(tmpdir()), "terra-init-"))
        const was = process.cwd()
        process.chdir(dir)
        try {
            const capture = Renderer.capture({ columns: 100 })
            const code = await Cli({ exec: process.execPath, renderer: capture }).dispatch(["mod", "init", "kilns", "--scope", "@you"])
            const text = capture.output()
            expect(code).toBe(0)
            for (const shown of ["Creating mod", "Scaffolding", "Installing", "Generating", "./kilns", "mod.config.ts", "main.ts", "cd kilns && terra dev"])
                expect(text).toContain(shown)
            expect(text).not.toContain(".gitignore")
        } finally {
            process.chdir(was)
        }
    }, 30_000)
})
