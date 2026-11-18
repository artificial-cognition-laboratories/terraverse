import { describe, expect, test } from "bun:test"
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { ConfigEditor } from "../../src/terra/project/editor"

const CONFIG = `// my universe
export default defineServer({
    title: "Arena", // what players see
    system: "TF24.abc",

    access: {
        join: "friends",
    },
})
`

function editor(text = CONFIG) {
    const path = `${mkdtempSync(`${tmpdir()}/terra-edit-`)}/server.config.ts`
    writeFileSync(path, text)
    return { edit: ConfigEditor({ path: path, define: "defineServer" }), text: () => readFileSync(path, "utf8") }
}

describe("ConfigEditor", () => {
    test("replaces a value, and nothing around it", () => {
        const { edit, text } = editor()
        expect(edit.set(["title"], "Arena II")).toEqual({ changed: true })
        expect(text()).toBe(CONFIG.replace('"Arena"', '"Arena II"'))
    })

    test("setting what's already there changes nothing", () => {
        const { edit } = editor()
        expect(edit.set(["title"], "Arena")).toEqual({ changed: false })
    })

    test("adds a key after the last, in the file's layout", () => {
        const { edit, text } = editor()
        edit.set(["access", "roles"], { admin: ["@cody"] })
        expect(text()).toContain(`    access: {
        join: "friends",
        roles: {
            admin: ["@cody"],
        },
    },`)
    })

    test("makes the objects on the way to a new key", () => {
        const { edit, text } = editor()
        edit.set(["settings", "@you/kilns", "kilnHeat"], 1500)
        expect(text()).toContain(`    settings: {
        "@you/kilns": {
            kilnHeat: 1500,
        },
    },
})`)
    })

    test("a one-line object stays on one line", () => {
        const { edit, text } = editor(`export default defineServer({ title: "A", system: "TF24.x" })\n`)
        edit.set(["access"], { join: "anyone" })
        expect(text()).toStartWith(`export default defineServer({ title: "A", system: "TF24.x", access: {`)
    })

    test("an empty object gets its first key on a line of its own", () => {
        const { edit, text } = editor(`export default defineServer({})\n`)
        edit.set(["system"], "TF24.x")
        expect(text()).toBe(`export default defineServer({\n    system: "TF24.x",\n})\n`)
    })

    test("removes a key with its line, keeping the comment above", () => {
        const { edit, text } = editor()
        expect(edit.remove(["system"])).toEqual({ changed: true })
        expect(text()).toBe(CONFIG.replace('    system: "TF24.abc",\n', ""))
        expect(edit.remove(["nothing"])).toEqual({ changed: false })
    })

    test("removes the last of several on a line", () => {
        const { edit, text } = editor(`export default defineServer({ title: "A", system: "TF24.x" })\n`)
        edit.remove(["system"])
        expect(text()).toBe(`export default defineServer({ title: "A" })\n`)
    })

    test("a file without the define call is refused", () => {
        const { edit } = editor(`export default { title: "A" }\n`)
        expect(() => edit.set(["title"], "B")).toThrow(expect.objectContaining({ code: "CONFIG_INVALID" }))
    })
})
