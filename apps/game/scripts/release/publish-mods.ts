// The official mods onto the registry, at the versions this release pins.
//
//   bun apps/game/scripts/release/publish-mods.ts [--dry-run] [--warn-unbumped]
//
// Every new world pins each mod in registry/mods at its package.json version, and adding
// any registry mod to a world resolves those pins on the registry: a release whose official
// mods aren't published breaks "add a mod" for every world it makes. So each release, for
// each mod: a version already published must be byte for byte what's here (a version never
// changes meaning; a changed mod needs a bump), and one that isn't is published, by
// `terra publish` signed in with TERRA_TOKEN. Run before the game's packages go up: a
// refusal here stops the release.
//
// --dry-run publishes nothing (`terra publish --dry-run` instead). --warn-unbumped reports a
// changed, unbumped mod without failing: master pushes, where the release bot bumps it later.
// ARCLABS_URL names another registry, as terra's does.
import { appendFileSync, existsSync, mkdtempSync, readdirSync, readFileSync, rmSync } from "node:fs"
import { tmpdir } from "node:os"
import { join, relative } from "node:path"
import { Api, DEFAULT_ARCLABS, Pack, Registry, npmUrl } from "../../../cli/src/terra/registry"

const root = join(import.meta.dir, "../../../..")

type Outcome = "unchanged" | "published" | "would publish" | "unbumped" | "refused"

/** One official mod: its folder, and its identity from package.json. */
type Mod = { dir: string; name: string; version: string }

/**
 * Releaser — the official mods against the registry: which are there as they are here,
 * which to publish, and which changed under a version already published.
 */
function Releaser(opts: { base: string; dryRun: boolean; warnUnbumped: boolean; terra: string[] }) {
    const registry = Registry({ api: Api({ base: opts.base, key: () => null }) })

    async function release(mod: Mod): Promise<{ outcome: Outcome; detail: string }> {
        const page = await registry.page(mod.name)
        if (page?.versions.some(v => v.version === mod.version)) {
            const changed = await drift(mod)
            if (changed.length === 0) return { outcome: "unchanged", detail: "already published, the same files" }
            return { outcome: "unbumped", detail: `changed since ${mod.version} was published (${changed.join(", ")}): bump ${mod.name}'s version` }
        }
        const out = terra(["publish", mod.dir, "--json", ...(opts.dryRun ? ["--dry-run"] : [])])
        const said: { name: string; version: string; api: number; files?: unknown[] } = JSON.parse(out.trim().split("\n").at(-1)!)
        if (said.name !== mod.name || said.version !== mod.version)
            throw new Error(`terra publish answered ${said.name}@${said.version}, not ${mod.name}@${mod.version}`)
        return opts.dryRun ? { outcome: "would publish", detail: `${said.files?.length} files, mod API ${said.api}` } : { outcome: "published", detail: `mod API ${said.api}` }
    }

    /** The files that differ between the mod here and its published tarball, by path. */
    async function drift(mod: Mod): Promise<string[]> {
        const packument = await fetchJson(`${npmUrl(opts.base)}/${mod.name}`)
        const tarball = ["versions", mod.version, "dist", "tarball"].reduce<unknown>((at, key) => (typeof at === "object" && at !== null ? Reflect.get(at, key) : undefined), packument)
        if (typeof tarball !== "string") throw new Error(`${mod.name}@${mod.version} has no tarball on ${opts.base}`)
        const response = await fetch(tarball)
        if (!response.ok) throw new Error(`${tarball}: ${response.status} ${response.statusText}`)

        const unpacked = mkdtempSync(join(tmpdir(), "official-mod-"))
        try {
            const tar = Bun.spawnSync(["tar", "-xzf", "-", "-C", unpacked], { stdin: new Uint8Array(await response.arrayBuffer()), stderr: "pipe" })
            if (tar.exitCode !== 0) throw new Error(`${tarball} didn't unpack: ${tar.stderr.toString()}`)
            const published = join(unpacked, "package")
            const here = Pack({ root: mod.dir }).contents().files.map(f => f.path)
            const there = existsSync(published) ? files(published) : []
            const paths = [...new Set([...here, ...there])].sort()
            return paths.filter(path => {
                const a = join(mod.dir, path), b = join(published, path)
                return !existsSync(a) || !existsSync(b) || !readFileSync(a).equals(readFileSync(b))
            })
        } finally {
            rmSync(unpacked, { recursive: true, force: true })
        }
    }

    function terra(args: string[]): string {
        const run = Bun.spawnSync([...opts.terra, ...args], { cwd: root, env: { ...process.env, ARCLABS_URL: opts.base }, stdout: "pipe", stderr: "pipe" })
        if (run.exitCode !== 0) throw new Error(`terra ${args.join(" ")} failed (exit ${run.exitCode}):\n${run.stdout.toString()}${run.stderr.toString()}`)
        return run.stdout.toString()
    }

    return { release: release }
}

const args = process.argv.slice(2)
const unknown = args.filter(a => a !== "--dry-run" && a !== "--warn-unbumped")
if (unknown.length > 0) {
    console.error(`publish-mods: unknown ${unknown.join(" ")}; takes --dry-run, --warn-unbumped`)
    process.exit(2)
}
const dryRun = args.includes("--dry-run"), warnUnbumped = args.includes("--warn-unbumped")
const base = (process.env.ARCLABS_URL ?? DEFAULT_ARCLABS).replace(/\/+$/, "")
const releaser = Releaser({ base: base, dryRun: dryRun, warnUnbumped: warnUnbumped, terra: [process.execPath, join(root, "apps/cli/bin/terra.ts")] })

const rows: string[] = []
let failed = false
for (const mod of mods(join(root, "registry/mods"))) {
    // every mod is reported, and any refusal fails the run once all are
    const { outcome, detail } = await releaser.release(mod).catch((e: unknown) => ({ outcome: "refused" as const, detail: e instanceof Error ? e.message : String(e) }))
    const bad = outcome === "refused" || (outcome === "unbumped" && !warnUnbumped)
    failed ||= bad
    const line = `${mod.name}@${mod.version}: ${outcome}, ${detail}`
    console.log(`  ${bad ? "FAIL " : outcome === "unbumped" ? "WARN " : "ok   "} ${line}`)
    if (outcome === "unbumped" && warnUnbumped) console.log(`::warning title=${mod.name} needs a version bump::${detail}`)
    rows.push(`| \`${mod.name}\` | ${mod.version} | ${bad ? `**${outcome}**` : outcome} | ${oneLine(detail)} |`)
}
const summary = process.env.GITHUB_STEP_SUMMARY
if (summary) {
    const mode = dryRun ? "dry run: nothing published" : `published to ${base}`
    appendFileSync(summary, `### Official mods (${mode})\n\n| Mod | Version | Outcome | |\n|---|---|---|---|\n${rows.join("\n")}\n\n`)
}
if (failed) {
    console.error("publish-mods: the release can't go out pinning these. A changed mod needs its version bumped (a published one never changes);")
    console.error("a refused publish says why above. CI signs in with the TERRA_TOKEN secret, an account in the terraverse org.")
    process.exit(1)
}

/** Each folder under `dir` with a package.json, by name. */
function mods(dir: string): Mod[] {
    return readdirSync(dir, { withFileTypes: true }).filter(e => e.isDirectory() && existsSync(join(dir, e.name, "package.json"))).map(e => {
        const pkg = JSON.parse(readFileSync(join(dir, e.name, "package.json"), "utf8"))
        if (typeof pkg.name !== "string" || typeof pkg.version !== "string") throw new Error(`${dir}/${e.name}/package.json has no name or version`)
        return { dir: join(dir, e.name), name: pkg.name, version: pkg.version }
    }).sort((a, b) => a.name.localeCompare(b.name))
}

/** A detail for a table cell: terra's lines run together, the pipes escaped. */
function oneLine(text: string): string {
    return text.split("\n").map(l => l.trim()).filter(Boolean).join(" · ").replaceAll("|", "\\|").slice(0, 400)
}

/** Every file under `dir`, as paths relative to it. */
function files(dir: string): string[] {
    return readdirSync(dir, { recursive: true, withFileTypes: true }).filter(e => e.isFile()).map(e => relative(dir, join(e.parentPath, e.name)))
}

async function fetchJson(url: string): Promise<unknown> {
    const response = await fetch(url, { headers: { accept: "application/json" } })
    if (!response.ok) throw new Error(`${url}: ${response.status} ${response.statusText}`)
    return response.json()
}
