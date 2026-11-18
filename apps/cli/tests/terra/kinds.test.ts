import { describe, expect, test } from "bun:test"
import { mkdtempSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { KINDS, detectKind } from "../../src/terra/project/kinds"

const folder = (files: string[]) => {
    const root = mkdtempSync(`${tmpdir()}/terra-kind-`)
    for (const f of files) writeFileSync(`${root}/${f}`, "")
    return root
}

describe("kinds", () => {
    test("a project is package.json and its kind's config", () => {
        expect(detectKind(folder(["package.json", "server.config.ts"]))).toBe("server")
        expect(detectKind(folder(["package.json", "mod.config.ts"]))).toBe("mod")
        expect(detectKind(folder(["mod.config.ts"]))).toBeNull()
        expect(detectKind(folder(["package.json"]))).toBeNull()
    })

    test("only a server runs mods", () => {
        expect(Object.entries(KINDS).filter(([, spec]) => spec.runs).map(([kind]) => kind)).toEqual(["server"])
    })
})
