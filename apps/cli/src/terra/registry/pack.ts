import { cpSync, existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, statSync } from "node:fs"
import { tmpdir } from "node:os"
import { dirname, join, normalize, relative, sep } from "node:path"
import { err } from "../../errors"

/**
 * What a mod publishes, by its layout (docs: mods/structure): its config and package.json,
 * its README and licence, and the folders its code and assets live in. An allowlist, not a
 * denylist: a file only ships because a mod's layout says it belongs, so a stray .env or a
 * scratch folder never goes by accident.
 */
const ROOT_FILES = [/^package\.json$/, /^mod\.config\.ts$/, /^tsconfig\.json$/, /^README(\.md)?$/i, /^LICEN[CS]E(\.\w+)?$/i]
const FOLDERS = ["server", "client", "shared", "assets"]

/** Within those folders, what still never ships: tests, and anything hidden. */
const NEVER = [/\.test\.tsx?$/, /\.spec\.tsx?$/, /(^|\/)\./]

/** README media the registry stores, by extension: mirrors the backend's allowlist (no SVG). */
const MEDIA = /\.(png|jpe?g|webp|gif|mp4|webm|mov)$/i

/** Mirrors the registry's ceilings, so a publish too big is refused here, before the upload. */
const MAX_SOURCE = 50 * 1024 * 1024
const MAX_MEDIA_FILE = 10 * 1024 * 1024
const MAX_MEDIA = 25 * 1024 * 1024
const MAX_README = 512 * 1024

export type PackedFile = { path: string; bytes: number }

/** What a publish carries, before it's archived. */
export type Contents = {
    /** Paths from the project's root, sorted. */
    files: PackedFile[]
    readme: string | null
    /** README media: files the README links to, by the path it links them with. */
    media: PackedFile[]
}

/** A tarball, in memory. */
export type Packed = Contents & { source: Buffer; assets: Buffer | null }

/** Every file in `dir`, as paths from `root` with forward slashes. */
function walk(root: string, dir: string): string[] {
    const out: string[] = []
    for (const entry of readdirSync(join(root, dir), { withFileTypes: true })) {
        const path = dir ? `${dir}/${entry.name}` : entry.name
        if (entry.isDirectory()) out.push(...walk(root, path))
        else if (entry.isFile()) out.push(path)
    }
    return out
}

/**
 * The relative links a README makes to media: `![alt](docs/kiln.png)` and
 * `<img src="docs/kiln.png">`. Remote URLs are left alone: they aren't ours to store.
 */
export function mediaLinks(readme: string): string[] {
    const found = new Set<string>()
    const patterns = [/!\[[^\]]*\]\(\s*<?([^)\s>]+)>?(?:\s+"[^"]*")?\s*\)/g, /<(?:img|video|source)\b[^>]*\bsrc=["']([^"']+)["']/gi]
    for (const pattern of patterns) {
        for (const match of readme.matchAll(pattern)) {
            const link = match[1]!.split(/[?#]/)[0]!
            if (/^[a-z][a-z0-9+.-]*:/i.test(link) || link.startsWith("//") || link.startsWith("/")) continue
            if (MEDIA.test(link)) found.add(link.replace(/^\.\//, ""))
        }
    }
    return [...found].sort()
}

/** Pack — a mod's publishable contents, listed, then archived the way the registry reads them. */
export function Pack(opts: { root: string }) {
    /** What would be published: the files, the README and its media. Reads, writes nothing. */
    function contents(): Contents {
        const files: string[] = []
        for (const entry of readdirSync(opts.root, { withFileTypes: true })) {
            if (entry.isFile() && ROOT_FILES.some(p => p.test(entry.name))) files.push(entry.name)
        }
        for (const folder of FOLDERS) {
            if (existsSync(join(opts.root, folder))) files.push(...walk(opts.root, folder).filter(path => !NEVER.some(p => p.test(path))))
        }
        files.sort()

        const readmeName = files.find(path => /^README(\.md)?$/i.test(path))
        let readmeText: string | null = null
        if (readmeName) {
            const bytes = statSync(join(opts.root, readmeName)).size
            if (bytes > MAX_README) throw err("PUBLISH_INVALID", { detail: `${readmeName} is ${mb(bytes)}; a README is at most ${mb(MAX_README)}` })
            readmeText = readFileSync(join(opts.root, readmeName), "utf8")
        }

        const media: PackedFile[] = []
        for (const link of readmeText ? mediaLinks(readmeText) : []) {
            const path = normalize(link).split(sep).join("/")
            if (path.startsWith("../") || path === "..") throw err("PUBLISH_INVALID", { detail: `the README links to ${link}, outside the mod: media must be inside it` })
            const full = join(opts.root, path)
            if (!existsSync(full)) throw err("PUBLISH_INVALID", { detail: `the README links to ${link}, which isn't there` })
            const bytes = statSync(full).size
            if (bytes > MAX_MEDIA_FILE) throw err("PUBLISH_INVALID", { detail: `${link} is ${mb(bytes)}; README media is at most ${mb(MAX_MEDIA_FILE)} a file` })
            media.push({ path: path, bytes: bytes })
        }
        const mediaTotal = media.reduce((sum, f) => sum + f.bytes, 0)
        if (mediaTotal > MAX_MEDIA) throw err("PUBLISH_INVALID", { detail: `the README's media is ${mb(mediaTotal)}; at most ${mb(MAX_MEDIA)} in all` })

        return {
            files: files.map(path => ({ path: path, bytes: statSync(join(opts.root, path)).size })),
            readme: readmeText,
            media: media,
        }
    }

    /** The contents, archived: npm's layout (package/…) for the source, assets/… for media. */
    async function pack(): Promise<Packed> {
        const listed = contents()
        const work = mkdtempSync(join(tmpdir(), "terra-pack-"))
        try {
            const source = await archive(work, "package", listed.files.map(f => f.path))
            if (source.byteLength > MAX_SOURCE) throw err("PUBLISH_INVALID", { detail: `the mod packs to ${mb(source.byteLength)}; at most ${mb(MAX_SOURCE)}` })
            const assets = listed.media.length > 0 ? await archive(work, "assets", listed.media.map(f => f.path)) : null
            return { ...listed, source: source, assets: assets }
        } finally {
            rmSync(work, { recursive: true, force: true })
        }
    }

    /** `paths` copied under `work/<top>/` and tarred from there, so every member starts `<top>/`. */
    async function archive(work: string, top: string, paths: string[]): Promise<Buffer> {
        for (const path of paths) {
            const to = join(work, top, path)
            mkdirSync(dirname(to), { recursive: true })
            cpSync(join(opts.root, path), to)
        }
        const out = join(work, `${top}.tgz`)
        // tar is on every platform terra runs on (Windows ships bsdtar). Run from work with relative
        // names: GNU tar (Git's, first on a CI runner's PATH) reads "C:\…" as a remote host
        const proc = Bun.spawn(["tar", "-czf", `${top}.tgz`, top], { cwd: work, stdout: "pipe", stderr: "pipe" })
        const [code, error] = await Promise.all([proc.exited, new Response(proc.stderr).text()])
        if (code !== 0) throw err("UNEXPECTED", { detail: `tar couldn't pack ${relative(opts.root, work)}: ${error.trim()}` })
        return Buffer.from(await Bun.file(out).arrayBuffer())
    }

    return { contents: contents, pack: pack }
}

export type PackT = ReturnType<typeof Pack>

function mb(bytes: number): string {
    return bytes >= 1024 * 1024 ? `${(bytes / 1024 / 1024).toFixed(1)}MB` : `${Math.ceil(bytes / 1024)}KB`
}
