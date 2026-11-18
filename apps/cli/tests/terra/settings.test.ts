import { describe, expect, test } from "bun:test"
import { settings } from "../../src/terra/project/settings"
import type { Mod } from "../../src/terra/project/types"

const KILNS = settings.declarations({
    kilnHeat: { type: "number", default: 1300, unit: "K", range: [900, 1800], step: 50 },
    bricksOnly: { type: "boolean", default: false },
    fuel: { type: "choice", default: "charcoal", options: { charcoal: "Charcoal", coal: "Coal" } },
    motto: { type: "string", default: "Fire it", pattern: "^[A-Z]" },
}, "test")

const mod = (name: string, declared = KILNS): Mod =>
    ({ name: name, prefix: "kilns", version: "1.0.0", root: "/x", title: "Kilns", local: false, needs: [], settings: declared })

describe("settings", () => {
    test("defaults where the server chose nothing, its choices where it did", () => {
        const resolved = settings.resolve([mod("@you/kilns")], { "@you/kilns": { kilnHeat: 1500, fuel: "coal" } })
        expect(resolved.get("@you/kilns")).toEqual({ kilnHeat: 1500, bricksOnly: false, fuel: "coal", motto: "Fire it" })
    })

    test("each kind of setting refuses what it doesn't take", () => {
        const refuse = (values: Record<string, unknown>) =>
            expect(() => settings.resolve([mod("@you/kilns")], { "@you/kilns": values })).toThrow(expect.objectContaining({ code: "SETTING_INVALID" }))
        refuse({ kilnHeat: 2000 })          // out of range
        refuse({ kilnHeat: 1325 })          // off its step
        refuse({ kilnHeat: "hot" })
        refuse({ bricksOnly: "yes" })
        refuse({ fuel: "wood" })
        refuse({ motto: "lower case" })
    })

    test("a setting a mod doesn't declare, or a mod the server doesn't run", () => {
        expect(() => settings.resolve([mod("@you/kilns")], { "@you/kilns": { kilnHeet: 1500 } })).toThrow(expect.objectContaining({ code: "SETTING_UNKNOWN" }))
        expect(() => settings.resolve([mod("@you/kilns")], { "@you/bellows": {} })).toThrow(expect.objectContaining({ code: "SETTING_UNKNOWN" }))
    })

    test("a declaration that contradicts itself is refused where it's written", () => {
        expect(() => settings.declarations({ heat: { type: "number", default: 5000, range: [0, 100] } }, "kilns")).toThrow(expect.objectContaining({ code: "SETTING_INVALID" }))
        expect(() => settings.declarations({ heat: { type: "kelvin", default: 1 } }, "kilns")).toThrow(expect.objectContaining({ code: "CONFIG_INVALID" }))
        expect(() => settings.declarations({ "kiln-heat": { type: "boolean", default: true } }, "kilns")).toThrow(expect.objectContaining({ code: "CONFIG_INVALID" }))
    })

    test("a mod's types carry the settings of the mods it builds on, a clash as either", () => {
        const craft = mod("@you/craft", settings.declarations({ fuel: { type: "string", default: "x" }, benches: { type: "number", default: 1 } }, "craft"))
        const text = settings.types.mod(KILNS, [craft])
        expect(text).toContain(`readonly benches: number`)
        expect(text).toContain(`readonly fuel: "charcoal" | "coal" | string`)
    })

    test("as types: a mod's own, and a server's for the mods it runs", () => {
        expect(settings.types.mod(KILNS)).toContain(`readonly fuel: "charcoal" | "coal"`)
        expect(settings.types.mod(KILNS)).toContain(`readonly kilnHeat: number`)
        const server = settings.types.server([mod("@you/kilns"), mod("@you/plain", {})])
        expect(server).toContain(`"@you/kilns"?: {`)
        expect(server).toContain(`kilnHeat?: number`)
        expect(server).not.toContain("@you/plain")
    })
})
