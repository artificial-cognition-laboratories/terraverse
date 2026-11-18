import { describe, expect, test } from "bun:test"
import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { Configs } from "../../src/terra/project/configs"
import { Mods } from "../../src/terra/project/server/mods"
import { Packages } from "../../src/terra/project/packages"

type Fake = { name: string; deps?: string[]; mod?: boolean }

/** A server folder with these packages already installed, as bun would leave them. */
function server(installed: Fake[], runs: string[]) {
    const root = mkdtempSync(`${tmpdir()}/terra-mods-`)
    const pkg = (deps: string[]) => Object.fromEntries(deps.map(d => [d, "1.0.0"]))
    writeFileSync(`${root}/package.json`, JSON.stringify({ name: "@t/server", version: "1.0.0", dependencies: pkg(runs) }))
    for (const p of installed) {
        const dir = `${root}/node_modules/${p.name}`
        mkdirSync(dir, { recursive: true })
        writeFileSync(`${dir}/package.json`, JSON.stringify({ name: p.name, version: "1.0.0", dependencies: pkg(p.deps ?? []) }))
        if (p.mod !== false) writeFileSync(`${dir}/mod.config.ts`, `export default defineMod({ title: ${JSON.stringify(p.name)} })`)
    }
    return Mods({ root: root, packages: Packages({}), configs: Configs({}) })
}

describe("Mods", () => {
    test("each after the mods it builds on; libraries aren't mods", async () => {
        const mods = server([
            { name: "@t/kilns", deps: ["@t/craft", "lodash"] },
            { name: "@t/craft" },
            { name: "lodash", mod: false },
        ], ["@t/kilns", "@t/craft", "lodash"])
        const read = await mods.read()
        expect(read.map(m => m.name)).toEqual(["@t/craft", "@t/kilns"])
        expect(read[1]?.needs).toEqual(["@t/craft"])
        expect(read[1]?.prefix).toBe("kilns")
        expect(read[1]?.title).toBe("@t/kilns")
    })

    test("hyphens become underscores in a prefix", async () => {
        const read = await server([{ name: "@t/hunger-games" }], ["@t/hunger-games"]).read()
        expect(read[0]?.prefix).toBe("hunger_games")
    })

    test("two mods with one prefix can't both run", async () => {
        const mods = server([{ name: "@a/kilns" }, { name: "@b/kilns" }], ["@a/kilns", "@b/kilns"])
        await expect(mods.read()).rejects.toMatchObject({ code: "PREFIX_TAKEN" })
    })

    test("a mod built on a mod the server doesn't run", async () => {
        const mods = server([{ name: "@t/kilns", deps: ["@t/craft"] }, { name: "@t/craft" }], ["@t/kilns"])
        await expect(mods.read()).rejects.toMatchObject({ code: "MOD_MISSING" })
    })

    test("a circle", async () => {
        const mods = server([{ name: "@t/a", deps: ["@t/b"] }, { name: "@t/b", deps: ["@t/a"] }], ["@t/a", "@t/b"])
        await expect(mods.read()).rejects.toMatchObject({ code: "MOD_CYCLE" })
    })

    test("listed but not installed", async () => {
        const mods = server([], ["@t/kilns"])
        await expect(mods.read()).rejects.toMatchObject({ code: "NOT_INSTALLED" })
    })
})
