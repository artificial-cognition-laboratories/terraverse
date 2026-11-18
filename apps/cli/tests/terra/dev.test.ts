import { describe, expect, test } from "bun:test"
import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { Watch } from "../../src/terra/dev/watch"

function project() {
    const root = mkdtempSync(`${tmpdir()}/terra-watch-`)
    mkdirSync(`${root}/server`, { recursive: true })
    mkdirSync(`${root}/node_modules/x`, { recursive: true })
    return root
}

const within = <T>(ms: number, p: Promise<T>) =>
    Promise.race([p, Bun.sleep(ms).then(() => "nothing" as const)])

describe("Watch", () => {
    test("a source save is code, a config save is config", async () => {
        const root = project()
        const watch = Watch({ dirs: [root] })
        watch.start()
        writeFileSync(`${root}/server/main.ts`, "1")
        expect(await within(1000, watch.next())).toBe("code")
        writeFileSync(`${root}/mod.config.ts`, "1")
        expect(await within(1000, watch.next())).toBe("config")
        watch.stop()
    })

    test("node_modules, logs and lockfiles aren't saves", async () => {
        const root = project()
        const watch = Watch({ dirs: [root] })
        watch.start()
        writeFileSync(`${root}/node_modules/x/index.ts`, "1")
        writeFileSync(`${root}/dev.log`, "1")
        writeFileSync(`${root}/bun.lock`, "1")
        expect(await within(500, watch.next())).toBe("nothing")
        watch.stop()
    })

    test("a folder made after it started is watched too", async () => {
        const root = project()
        const watch = Watch({ dirs: [root] })
        watch.start()
        mkdirSync(`${root}/shared`)
        await Bun.sleep(100)
        writeFileSync(`${root}/shared/names.ts`, "1")
        expect(await within(1000, watch.next())).toBe("code")
        watch.stop()
    })

    test("a save while nobody's asking waits to be asked", async () => {
        const root = project()
        const watch = Watch({ dirs: [root] })
        watch.start()
        writeFileSync(`${root}/server/main.ts`, "1")
        await Bun.sleep(400)
        expect(await within(100, watch.next())).toBe("code")
        watch.stop()
    })
})
