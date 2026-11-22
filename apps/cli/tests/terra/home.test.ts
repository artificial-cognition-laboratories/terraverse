import { describe, expect, test } from "bun:test"
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs"
import { homedir, tmpdir } from "node:os"
import { join } from "node:path"
import { Home } from "../../src/terra/home"

/** A home in a fresh folder, as TERRAVERSE_HOME names one. */
function fresh(env: Record<string, string> = {}) {
    const root = mkdtempSync(join(tmpdir(), "terra-home-"))
    return { root: root, home: Home({ env: { TERRAVERSE_HOME: root, ...env }, platform: "linux", release: false }) }
}

describe("the home", () => {
    test("installed and source builds keep separate homes, unless one is named", () => {
        expect(Home({ env: {}, platform: "linux", release: true }).root).toBe(join(homedir(), ".terraverse"))
        expect(Home({ env: {}, platform: "linux", release: false }).root).toBe(join(homedir(), ".terraverse-dev"))
        expect(Home({ env: { APPDATA: "C:\\Users\\a\\AppData\\Roaming" }, platform: "win32", release: true }).root).toBe(join("C:\\Users\\a\\AppData\\Roaming", "Terraverse"))
        expect(fresh().home.root).toContain("terra-home-")
        expect(() => Home({ env: { TERRAVERSE_HOME: "relative" }, platform: "linux", release: false })).toThrow(expect.objectContaining({ code: "USAGE" }))
    })

    test("starts as local; mods and servers are the profile's", () => {
        const { root, home } = fresh()
        expect(home.active()).toBe("local")
        expect(home.mods()).toBe(join(root, "profiles", "local", "mods"))
        expect(JSON.parse(readFileSync(join(root, "profiles", "local", "profile.json"), "utf8"))).toMatchObject({ name: "local", handle: null })
    })

    test("the active profile is remembered, and TERRAVERSE_PROFILE wins for one run", () => {
        const { root, home } = fresh()
        home.use("@sam")
        expect(home.active()).toBe("@sam")
        expect(Home({ env: { TERRAVERSE_HOME: root, TERRAVERSE_PROFILE: "two" }, platform: "linux", release: false }).active()).toBe("two")
        expect(home.active()).toBe("@sam")
    })

    test("the first sign-in adopts local: what was made signed out is the account's", () => {
        const { root, home } = fresh()
        mkdirSync(home.mods(), { recursive: true })
        writeFileSync(join(home.mods(), "kept"), "x")
        const key = join(home.profile(), "terra.json")
        writeFileSync(key, "{}")
        const moved = home.signedIn("cody", key)
        expect(home.active()).toBe("@cody")
        expect(moved).toBe(join(root, "profiles", "@cody", "terra.json"))
        expect(existsSync(moved)).toBe(true)
        expect(existsSync(join(root, "profiles", "@cody", "mods", "kept"))).toBe(true)
        expect(JSON.parse(readFileSync(join(root, "profiles", "@cody", "profile.json"), "utf8"))).toMatchObject({ name: "@cody", handle: "cody" })
    })

    test("a later sign-in brings only the key; signing out goes back to local", () => {
        const { root, home } = fresh()
        home.use("@cody")
        home.signedOut()
        expect(home.active()).toBe("local")
        const key = join(home.profile(), "terra.json")
        writeFileSync(key, "{}")
        home.signedIn("cody", key)
        expect(existsSync(join(root, "profiles", "@cody", "terra.json"))).toBe(true)
        expect(existsSync(key)).toBe(false)
        expect(existsSync(join(root, "profiles", "local"))).toBe(true)
    })

    test("names are checked", () => {
        const { home } = fresh()
        for (const bad of ["../x", "a/b", "@", "@..", ""]) expect(() => home.use(bad)).toThrow(expect.objectContaining({ code: "USAGE" }))
    })
})
