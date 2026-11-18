import { describe, expect, test } from "bun:test"
import { mkdirSync, mkdtempSync, readFileSync, realpathSync, symlinkSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { join } from "node:path"
import { serverFixture } from "../fixtures"

describe("projects", () => {
    test("find: the nearest project, of whichever kind", () => {
        const { root, terra } = serverFixture()
        mkdirSync(`${root}/save/worlds`, { recursive: true })
        expect(terra.projects.find(`${root}/save/worlds`)).toMatchObject({ root: root, kind: "server" })
        expect(terra.projects.find(`${root}/mods/hello/server`)).toMatchObject({ root: join(root, "mods", "hello"), kind: "mod" })
        expect(() => terra.projects.find("/")).toThrow(expect.objectContaining({ code: "PROJECT_NOT_FOUND" }))
    })

    test("server: only a server", () => {
        const { root, terra } = serverFixture()
        expect(terra.projects.server(root).server.worlds).toBeDefined()
        expect(() => terra.projects.server(`${root}/mods/hello`)).toThrow(expect.objectContaining({ code: "PROJECT_WRONG_KIND" }))
    })

    test("the same root is the same project", () => {
        const { root, terra } = serverFixture()
        expect(terra.projects.open(root)).toBe(terra.projects.open(`${root}/.`))
    })

    test("a project reached by another name opens by its real one", () => {
        // a symlink here; on Windows the same holds for an 8.3 short name (RUNNER~1), which
        // bun install resolves file: dependencies against by the long name
        const { root, terra } = serverFixture()
        const alias = join(mkdtempSync(join(realpathSync.native(tmpdir()), "terra-alias-")), "server")
        symlinkSync(root, alias, "junction")
        expect(terra.projects.open(alias).root).toBe(root)
        expect(terra.projects.open(alias)).toBe(terra.projects.open(root))
        expect(terra.projects.find(`${alias}/mods/hello`).root).toBe(join(root, "mods", "hello"))
    })
})

describe("preparing a server", () => {
    test("installs, reads its config, orders and builds its mods; the manifest reads back", async () => {
        const { root, terra } = serverFixture()
        const project = terra.projects.server(root)
        const steps: string[] = []
        const prepared = await project.prepare({ progress: step => { steps.push(step) } })
        expect(steps).toEqual(["install", "config", "types", "mods", "build"])
        expect(prepared).toMatchObject({ kind: "server", name: "@check/server", installed: true })
        expect(prepared.mods.map(m => m.name)).toEqual(["@check/hello", "@check/greeter"])
        // a local mod is used where it is, not bun's copy
        expect(prepared.mods[0]?.root).toBe(join(root, "mods", "hello"))
        expect(await project.server.manifest.read()).toEqual(prepared.manifest)

        // greeter's import of hello is hello's loaded module, not a copy of its code
        const greeter = readFileSync(`${root}/.terra/build/greeter/server.js`, "utf8")
        expect(greeter).toContain("__terraMods")
        expect(greeter).not.toContain("hello:brick")
    })

    test("a second prepare doesn't install again", async () => {
        const { root, terra } = serverFixture()
        const project = terra.projects.server(root)
        await project.prepare()
        expect((await project.prepare()).installed).toBe(false)
    })

    test("importing a mod without depending on it is refused", async () => {
        const { root, terra } = serverFixture()
        writeFileSync(`${root}/mods/greeter/package.json`, JSON.stringify({ name: "@check/greeter", version: "0.1.0", type: "module" }))
        await expect(terra.projects.server(root).prepare()).rejects.toMatchObject({ code: "MOD_UNDECLARED" })
    })

    test("the engine is the one beside terra", async () => {
        const { root, terra } = serverFixture()
        const prepared = await terra.projects.server(root).prepare()
        await expect(terra.engine.run({ mode: "server", server: prepared, world: "/nowhere", port: 30000 })).rejects.toMatchObject({ code: "ENGINE_MISSING" })
    })
})

describe("preparing a mod", () => {
    test("installs and reads its config; no mods to build", async () => {
        const { root, terra } = serverFixture()
        const steps: string[] = []
        const prepared = await terra.projects.find(`${root}/mods/hello`).prepare({ progress: step => { steps.push(step) } })
        expect(steps).toEqual(["install", "config", "types"])
        expect(prepared).toMatchObject({ kind: "mod", name: "@check/hello", title: "Hello" })
    })

    test("a config that doesn't define the mod is refused", async () => {
        const { root, terra } = serverFixture()
        writeFileSync(`${root}/mods/hello/mod.config.ts`, "export const nothing = 1")
        await expect(terra.projects.find(`${root}/mods/hello`).prepare()).rejects.toMatchObject({ code: "CONFIG_INVALID" })
    })
})
