import { describe, expect, test } from "bun:test"
import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { join } from "node:path"
import { accountFile, mediaLinks, npmUrl, Pack, parseSpec, withScope } from "../../src/terra/registry"

describe("a mod spec", () => {
    test("is a scoped name, with an optional exact version", () => {
        expect(parseSpec("@you/kilns")).toEqual({ name: "@you/kilns", version: null })
        expect(parseSpec("@you/kilns@0.3.0")).toEqual({ name: "@you/kilns", version: "0.3.0" })
    })

    test("refuses an unscoped name and a range", () => {
        expect(() => parseSpec("kilns")).toThrow("@scope/name")
        expect(() => parseSpec("@you/kilns@^0.3.0")).toThrow("exact")
    })
})

describe("bunfig.toml's scope line", () => {
    const url = npmUrl("http://localhost:3099")

    test("is added to a file that has none", () => {
        expect(withScope("", "you", url)).toBe(`[install.scopes]\n"you" = { url = "${url}" }\n`)
        expect(withScope("[install]\nexact = true\n", "you", url)).toBe(`[install]\nexact = true\n\n[install.scopes]\n"you" = { url = "${url}" }\n`)
    })

    test("joins an existing [install.scopes], leaving the author's lines", () => {
        const before = `[install.scopes]\n"sam" = { url = "https://elsewhere" }\n\n[test]\nroot = "."\n`
        const after = withScope(before, "you", url)
        expect(after).toContain(`"sam" = { url = "https://elsewhere" }`)
        expect(after).toContain(`"you" = { url = "${url}" }`)
        expect(after).toContain(`[test]\nroot = "."`)
    })

    test("replaces the scope's own line rather than adding a second", () => {
        const once = withScope("", "you", "https://old/-")
        const twice = withScope(once, "you", url)
        expect(twice.match(/"you" =/g)).toHaveLength(1)
        expect(twice).toContain(url)
        expect(withScope(twice, "you", url)).toBe(twice)
    })
})

describe("a README's media", () => {
    test("is its relative links to images and video", () => {
        const readme = [
            "![kiln](docs/kiln.png)",
            "![](./shots/fire.webm \"burning\")",
            "<img src=\"docs/logo.jpg\" width=80>",
            "![remote](https://example.com/a.png)",
            "![rooted](/abs.png)",
            "[a link, not an image](docs/notes.png)",
            "![vector](docs/diagram.svg)",
        ].join("\n")
        expect(mediaLinks(readme)).toEqual(["docs/kiln.png", "docs/logo.jpg", "shots/fire.webm"])
    })
})

describe("the account file", () => {
    test("is account.json for production, named by host for anything else", () => {
        expect(accountFile("/c", "https://prod", "https://prod")).toBe("/c/account.json")
        expect(accountFile("/c", "http://localhost:3099", "https://prod")).toBe("/c/account-localhost_3099.json")
    })
})

describe("what a mod packs", () => {
    function mod(files: Record<string, string>): string {
        const root = mkdtempSync(join(tmpdir(), "terra-pack-test-"))
        for (const [path, text] of Object.entries(files)) {
            mkdirSync(join(root, path, ".."), { recursive: true })
            writeFileSync(join(root, path), text)
        }
        return root
    }

    test("is its layout's files only: no secrets, tests, generated or stray folders", () => {
        const root = mod({
            "package.json": "{}",
            "mod.config.ts": "",
            "tsconfig.json": "",
            "README.md": "# Kilns\n\n![](docs/kiln.png)\n",
            "LICENSE": "",
            "server/main.ts": "",
            "server/main.test.ts": "",
            "client/main.ts": "",
            "assets/textures/brick.png": "png",
            ".env": "SECRET=1",
            "server/.cache/x": "",
            ".terra/types/api.json": "{}",
            "node_modules/x/index.js": "",
            "scratch/notes.txt": "",
            "bun.lock": "",
            "docs/kiln.png": "png",
        })
        const listed = Pack({ root: root }).contents()
        expect(listed.files.map(f => f.path)).toEqual([
            "LICENSE",
            "README.md",
            "assets/textures/brick.png",
            "client/main.ts",
            "mod.config.ts",
            "package.json",
            "server/main.ts",
            "tsconfig.json",
        ])
        expect(listed.media.map(f => f.path)).toEqual(["docs/kiln.png"])
        expect(listed.readme).toContain("# Kilns")
    })

    test("refuses a README linking to media that isn't there, or outside the mod", () => {
        expect(() => Pack({ root: mod({ "package.json": "{}", "README.md": "![](gone.png)" }) }).contents()).toThrow("isn't there")
        expect(() => Pack({ root: mod({ "package.json": "{}", "README.md": "![](../up.png)" }) }).contents()).toThrow("outside the mod")
    })

    test("archives the source under package/ and media under assets/", async () => {
        const root = mod({ "package.json": "{}", "server/main.ts": "", "README.md": "![](docs/a.png)", "docs/a.png": "png" })
        const packed = await Pack({ root: root }).pack()
        const list = async (archive: Buffer) => {
            const proc = Bun.spawn(["tar", "-tzf", "-"], { stdin: archive, stdout: "pipe" })
            return (await new Response(proc.stdout).text()).split("\n").filter(l => l && !l.endsWith("/")).sort()
        }
        expect(await list(packed.source)).toEqual(["package/README.md", "package/package.json", "package/server/main.ts"])
        expect(await list(packed.assets!)).toEqual(["assets/docs/a.png"])
    })
})
