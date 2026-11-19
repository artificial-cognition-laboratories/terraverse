import { describe, expect, test } from "bun:test"
import { Renderer } from "@arcforge/arcline"
import { Cli } from "../../src/cli"
import { serverFixture } from "../fixtures"

/** argv through a Cli writing to a capture. `exec` is this bun, so installs can run; no engine is beside it. */
const run = async (argv: string[]) => {
    const renderer = Renderer.capture({ columns: 100 })
    const code = await Cli({ exec: process.execPath, renderer: renderer }).dispatch(argv)
    return { code: code, out: renderer.output() }
}

describe("Cli", () => {
    test("help lists the commands", async () => {
        const { code, out } = await run(["help"])
        expect(code).toBe(0)
        expect(out).toContain("terra prepare [folder]")
        expect(out).toContain("terra host [folder] [--port n]")
    })

    test("no command is help", async () => {
        expect((await run([])).out).toContain("terra prepare")
    })

    test("an unknown command is reported, with its code", async () => {
        const { code, out } = await run(["frob"])
        expect(code).toBe(1)
        expect(out).toContain("COMMAND_UNKNOWN")
        expect(out).toContain("terra frob")
    })

    test("a published server waits for the registry", async () => {
        const { code, out } = await run(["@you/hunger-games"])
        expect(code).toBe(1)
        expect(out).toContain("needs the registry")
    })

    test("a designed command that isn't built says so", async () => {
        const { code, out } = await run(["snapshot"])
        expect(code).toBe(1)
        expect(out).toContain("NOT_WIRED")
    })

    test("a bad port is a usage error, before anything runs", async () => {
        const { code, out } = await run(["host", "--port", "70000"])
        expect(code).toBe(1)
        expect(out).toContain("USAGE")
    })

    test("a flag the command doesn't take is refused", async () => {
        const { code, out } = await run(["dev", "--fersh"])
        expect(code).toBe(1)
        expect(out).toContain("doesn't take --fersh")
    })

    test("outside any project", async () => {
        const { code, out } = await run(["prepare", "/"])
        expect(code).toBe(1)
        expect(out).toContain("PROJECT_NOT_FOUND")
    })

    test("host only runs a server", async () => {
        const { root } = serverFixture()
        const { code, out } = await run(["host", `${root}/mods/hello`])
        expect(code).toBe(1)
        expect(out).toContain("PROJECT_WRONG_KIND")
    })

    test("prepare shows what a server runs", async () => {
        const { root } = serverFixture()
        const { code, out } = await run(["prepare", root])
        expect(code).toBe(0)
        expect(out).toContain("Mods check ready")
        expect(out).toContain("@check/greeter")
    })
})
