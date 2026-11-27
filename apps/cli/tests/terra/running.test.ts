import { describe, expect, test } from "bun:test"
import { existsSync, mkdtempSync, statSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { join } from "node:path"
import { Running, sources } from "../../src/terra"

const server = { title: "Hello", mods: ["@you/kilns"], port: 30123, server: "/srv/hello" }

describe("running/", () => {
    function fresh() {
        const dir = join(mkdtempSync(join(tmpdir(), "terra-running-")), "running")
        return Running({ dir: dir, owner: () => "cody" })
    }

    test("a started server is listed, the user's alone, with its ref, until it stops", () => {
        const running = fresh()
        const run = running.prepare()
        expect(run.secret).toMatch(/^[0-9a-f]{64}$/)
        expect(run.owner).toBe("cody")
        const stop = running.started(server, { pid: process.pid, secret: run.secret, owner: run.owner })
        const [record] = running.list()
        expect(record).toMatchObject({ ref: "terra://localhost:30123", title: "Hello", port: 30123, pid: process.pid, owner: "cody" })
        // Windows has no POSIX modes (stat reports 0o666): there the user's profile ACL guards the home
        if (process.platform !== "win32") {
            expect(statSync(join(running.dir, "30123.json")).mode & 0o777).toBe(0o600)
            expect(statSync(running.dir).mode & 0o777).toBe(0o700)
        }
        stop()
        expect(running.list()).toEqual([])
    })

    test("a record whose process is gone, or that isn't one, is cleared", () => {
        const running = fresh()
        running.started(server, { pid: 2 ** 22 + 1, secret: "s", owner: null })
        writeFileSync(join(running.dir, "1.json"), "{ half")
        expect(running.list()).toEqual([])
        expect(existsSync(join(running.dir, "30123.json"))).toBe(false)
        expect(existsSync(join(running.dir, "1.json"))).toBe(false)
    })

    test("a world the game opened is read as the game's; terra's own say terra, and an unknown runner isn't one", () => {
        const running = fresh()
        running.started(server, { pid: process.pid, secret: "s", owner: "cody" })
        expect(running.at(30123)?.by).toBe("terra")
        const game = { ref: "terra://localhost:41000", title: "Bramble Mesa", mods: [], port: 41000, pid: process.pid, secret: "g", owner: "cody", server: "/w/bramble", started: 1, by: "game" }
        writeFileSync(join(running.dir, "41000.json"), JSON.stringify(game))
        expect(running.at(41000)).toEqual({ ...game, by: "game" })
        writeFileSync(join(running.dir, "41001.json"), JSON.stringify({ ...game, port: 41001, by: "someone" }))
        expect(running.at(41001)).toBeNull()
        expect(running.at(41002)).toBeNull()
    })
})

describe("an attached world", () => {
    const record = { ref: "terra://localhost:41000", title: "Bramble Mesa", mods: [], port: 41000, pid: 7, secret: "g", owner: "cody", server: "/w", started: 1, by: "game" as const }

    test("is its record's port and secret; closing lets it go, never stopping it", async () => {
        const world = await sources.attached({ record: record, still: () => true }).open()
        expect(world).toMatchObject({ port: 41000, secret: "g" })
        if (typeof world === "number") throw new Error("never opened")
        world.close()
        expect(await world.ended).toBe(0)
    })

    test("ends when the world does", async () => {
        let up = true
        const world = await sources.attached({ record: record, still: () => up }).open()
        if (typeof world === "number") throw new Error("never opened")
        up = false
        expect(await world.ended).toBe(0)
    })
})
