import { describe, expect, test } from "bun:test"
import { existsSync, mkdtempSync, statSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { join } from "node:path"
import { Running } from "../../src/terra"

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
})
