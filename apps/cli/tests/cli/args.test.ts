import { describe, expect, test } from "bun:test"
import { flags, parseArgs } from "../../src/cli/args"

describe("parseArgs", () => {
    test("a command, its positionals and its flags", () => {
        expect(parseArgs(["serve", "./server", "--port", "30001"])).toEqual({
            command: "serve", positional: ["./server"], flags: { port: "30001" },
        })
    })

    test("nothing is no command", () => {
        expect(parseArgs([]).command).toBeUndefined()
    })

    test("a flag on its own is a switch", () => {
        expect(parseArgs(["dev", "--fresh", "--port", "30001"]).flags).toEqual({ fresh: true, port: "30001" })
        expect(parseArgs(["dev", "--headless"]).flags).toEqual({ headless: true })
    })

    test("a value where a switch belongs, or a switch where a value does, is a usage error", () => {
        expect(() => flags.value(parseArgs(["serve", "--port"]), "port")).toThrow(expect.objectContaining({ code: "USAGE" }))
        expect(() => flags.on(parseArgs(["dev", "--fresh", "yes"]), "fresh")).toThrow(expect.objectContaining({ code: "USAGE" }))
        expect(() => flags.port(parseArgs(["serve", "--port", "70000"]), 30000)).toThrow(expect.objectContaining({ code: "USAGE" }))
        expect(flags.port(parseArgs(["serve"]), 30000)).toBe(30000)
    })
})
