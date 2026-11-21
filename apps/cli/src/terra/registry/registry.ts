import { existsSync, readFileSync, writeFileSync } from "node:fs"
import { join } from "node:path"
import { err } from "../../errors"
import { Configs, MOD_API } from "../project"
import { record, type ApiT } from "./api"
import { Pack, type Contents, type PackedFile } from "./pack"

/** What package.json says and mod.config.ts must not: one file is a mod's identity. */
const IDENTITY_FIELDS = ["name", "version", "description", "dependencies"] as const

const SCOPED = /^@([a-z0-9][a-z0-9._-]*)\/([a-z0-9][a-z0-9._-]*)$/
const EXACT = /^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/

/** Where bun installs Terraverse mods from: the npm surface, with the "-" filler the backend expects. */
export function npmUrl(base: string): string {
    return `${base}/api/terrafirma/registry/npm/-`
}

/** One version of a mod, as the registry describes it. */
/** What a publish does after preparing, in order. */
export type PublishStep = "verifying" | "packing" | "uploading"

export type ModVersion = {
    version: string
    /** The mod API it was built against. */
    api: number
    title: string
    /** Why it's withheld, or null. */
    withheld: string | null
}

/** A mod's page on the registry. */
export type ModPage = {
    name: string
    description: string | null
    owner: string | null
    latestVersion: string | null
    deprecated: string | null
    /** Newest first. */
    versions: ModVersion[]
}

/** `@you/kilns` or `@you/kilns@0.3.0`. */
export function parseSpec(spec: string): { name: string; version: string | null } {
    const at = spec.lastIndexOf("@")
    const name = at > 0 ? spec.slice(0, at) : spec
    const version = at > 0 ? spec.slice(at + 1) : null
    if (!SCOPED.test(name)) throw err("USAGE", { detail: `${spec}: a mod is @scope/name, optionally @version` })
    if (version !== null && !EXACT.test(version)) throw err("USAGE", { detail: `${spec}: a version is exact, like 0.3.0` })
    return { name: name, version: version }
}

/**
 * The [install.scopes] line bun needs to find `scope`'s mods on the registry, set in a
 * bunfig.toml's text. Only that line is touched: the rest of the file is the author's.
 */
export function withScope(bunfig: string, scope: string, url: string): string {
    const line = `"${scope}" = { url = "${url}" }`
    const lines = bunfig.split("\n")
    const header = lines.findIndex(l => l.trim() === "[install.scopes]")
    if (header === -1) {
        const body = bunfig.replace(/\n*$/, "")
        return `${body}${body ? "\n\n" : ""}[install.scopes]\n${line}\n`
    }
    let end = lines.length
    for (let i = header + 1; i < lines.length; i++) {
        if (/^\s*\[/.test(lines[i]!)) { end = i; break }
    }
    const key = new RegExp(`^\\s*"?${scope.replace(/[.]/g, "\\.")}"?\\s*=`)
    const existing = lines.slice(header + 1, end).findIndex(l => key.test(l))
    if (existing !== -1) lines[header + 1 + existing] = line
    else lines.splice(header + 1, 0, line)
    return lines.join("\n")
}

/**
 * Registry — terra's side of Terraverse's registry: publishing the mod you stand in,
 * reading a mod's page, deprecating, and adding a published mod to a project.
 */
export function Registry(opts: { api: ApiT }) {
    const configs = Configs({})

    /** The mod at `root`, checked and listed for publishing. Nothing leaves the machine. */
    async function check(root: string): Promise<Contents & { name: string; version: string; api: number; title: string; settings: Record<string, unknown> }> {
        const pkg: unknown = JSON.parse(readFileSync(join(root, "package.json"), "utf8"))
        if (typeof pkg !== "object" || pkg === null) throw err("PACKAGE_INVALID", { detail: `${root}/package.json isn't an object` })
        const name = Reflect.get(pkg, "name"), version = Reflect.get(pkg, "version")
        if (typeof name !== "string" || !SCOPED.test(name)) throw err("PUBLISH_INVALID", { detail: `package.json name ${JSON.stringify(name)} isn't @scope/name: the scope is your account` })
        if (typeof version !== "string" || !EXACT.test(version)) throw err("PUBLISH_INVALID", { detail: `package.json version ${JSON.stringify(version)} isn't an exact version like 0.1.0` })
        if (Reflect.get(pkg, "private") === true) throw err("PUBLISH_INVALID", { detail: `package.json says "private": true. Remove it to publish ${name}` })
        const deps = Reflect.get(pkg, "dependencies") ?? {}
        if (typeof deps !== "object" || deps === null) throw err("PACKAGE_INVALID", { detail: "dependencies isn't an object" })
        const loose = Object.entries(deps).filter(([, spec]) => typeof spec !== "string" || !EXACT.test(spec)).map(([dep, spec]) => `${dep}: ${String(spec)}`)
        if (loose.length > 0) throw err("PUBLISH_INVALID", { detail: `every dependency is pinned to an exact version, so a world never changes under its players. Not: ${loose.join(", ")}` })

        const config = await configs.evaluate("mod", root)
        const restated = IDENTITY_FIELDS.filter(field => Reflect.has(config, field))
        if (restated.length > 0) throw err("PUBLISH_INVALID", { detail: `mod.config.ts sets ${restated.join(", ")}: that's package.json's to say, and only package.json's. Remove it from mod.config.ts` })
        const read = await configs.read("mod", root, name)

        const apiFile = join(root, ".terra", "types", "api.json")
        if (!existsSync(apiFile)) throw err("PUBLISH_INVALID", { detail: `${apiFile} is missing: run terra prepare` })
        const api: unknown = Reflect.get(Object(JSON.parse(readFileSync(apiFile, "utf8"))), "api")
        if (typeof api !== "number") throw err("PUBLISH_INVALID", { detail: `${apiFile} has no api version: run terra prepare` })

        return { ...Pack({ root: root }).contents(), name: name, version: version, api: api, title: read.title, settings: read.settings }
    }

    /** Publish the mod at `root`: checked, packed, uploaded, each step reported as it starts. */
    async function publish(root: string, e: { progress?: (step: PublishStep) => void } = {}): Promise<{ name: string; version: string; api: number; media: PackedFile[] }> {
        e.progress?.("verifying")
        const checked = await check(root)
        e.progress?.("packing")
        const packed = await Pack({ root: root }).pack()
        const form = new FormData()
        form.append("source", new Blob([new Uint8Array(packed.source)]), "source.tar.gz")
        form.append("api", String(checked.api))
        form.append("mod", JSON.stringify({ title: checked.title, settings: checked.settings }))
        if (packed.readme !== null) form.append("readme", packed.readme)
        if (packed.assets) form.append("assets", new Blob([new Uint8Array(packed.assets)]), "assets.tar.gz")

        e.progress?.("uploading")
        const body = record(await opts.api.upload("/api/terrafirma/registry/publish", form), "the publish")
        if (body.name !== checked.name || body.version !== checked.version)
            throw err("REGISTRY_REFUSED", { detail: `published as ${String(body.name)}@${String(body.version)}, not ${checked.name}@${checked.version}` })
        return { name: checked.name, version: checked.version, api: checked.api, media: packed.media }
    }

    /** A mod's page, or null when nothing's published under the name. */
    async function page(name: string): Promise<ModPage | null> {
        const found = await opts.api.find(`/api/terrafirma/registry/packages/${name}`)
        if (found === null) return null
        const body = record(found, `${name}'s page`)
        const versions = Array.isArray(body.versions) ? body.versions : []
        const text = (v: unknown) => (typeof v === "string" ? v : null)
        return {
            name: name,
            description: text(body.description),
            owner: text(body.owner),
            latestVersion: text(body.latestVersion),
            deprecated: text(body.deprecated),
            versions: versions.map(v => {
                const row = record(v, `a version of ${name}`)
                const mod = typeof row.mod === "object" && row.mod !== null ? row.mod : {}
                return {
                    version: String(row.version),
                    api: Number(row.api),
                    title: String(Reflect.get(mod, "title") ?? name),
                    withheld: text(row.withheld),
                }
            }),
        }
    }

    /**
     * Which version of `name` to install: the one asked for, or the newest this terra can
     * load that isn't withheld. Refuses what can't be installed, saying why.
     */
    async function resolve(name: string, wanted: string | null): Promise<ModVersion> {
        const found = await page(name)
        if (!found) throw err("MOD_NOT_PUBLISHED", { detail: name })
        if (wanted) {
            const exact = found.versions.find(v => v.version === wanted)
            if (!exact) throw err("MOD_NOT_PUBLISHED", { detail: `${name}@${wanted}: published versions are ${found.versions.map(v => v.version).join(", ") || "none"}` })
            if (exact.withheld) throw err("REGISTRY_REFUSED", { detail: `${name}@${wanted} is withheld: ${exact.withheld}` })
            if (exact.api > MOD_API) throw err("MOD_TOO_NEW", { detail: `${name}@${wanted} needs mod API ${exact.api}; this terra has ${MOD_API}` })
            return exact
        }
        const usable = found.versions.find(v => !v.withheld && v.api <= MOD_API)
        if (!usable) {
            const newer = found.versions.find(v => !v.withheld)
            if (newer) throw err("MOD_TOO_NEW", { detail: `every version of ${name} needs a newer mod API than this terra's ${MOD_API}` })
            throw err("MOD_NOT_PUBLISHED", { detail: `${name} has no version that can be installed` })
        }
        return usable
    }

    /**
     * Add `name@version` to the project at `root`: pinned in package.json, and its scope
     * pointed at the registry in bunfig.toml. Installing is the caller's next step (prepare).
     */
    function add(root: string, name: string, version: string): { previous: string | null } {
        const path = join(root, "package.json")
        const text = readFileSync(path, "utf8")
        const pkg: unknown = JSON.parse(text)
        if (typeof pkg !== "object" || pkg === null) throw err("PACKAGE_INVALID", { detail: `${path} isn't an object` })
        const deps: unknown = Reflect.get(pkg, "dependencies") ?? {}
        if (typeof deps !== "object" || deps === null) throw err("PACKAGE_INVALID", { detail: `${path}: dependencies isn't an object` })
        const previous = Reflect.get(deps, name)
        Reflect.set(pkg, "dependencies", { ...deps, [name]: version })
        const indent = text.match(/^\{\n([ \t]+)"/)?.[1] ?? "    "
        writeFileSync(path, JSON.stringify(pkg, null, indent) + "\n")

        const scope = SCOPED.exec(name)![1]!
        const bunfig = join(root, "bunfig.toml")
        const before = existsSync(bunfig) ? readFileSync(bunfig, "utf8") : ""
        const after = withScope(before, scope, npmUrl(opts.api.base))
        if (after !== before) writeFileSync(bunfig, after)
        return { previous: typeof previous === "string" ? previous : null }
    }

    /** Deprecate a mod of yours with a reason, or lift it with null. */
    async function deprecate(name: string, message: string | null): Promise<void> {
        await opts.api.post("/api/terrafirma/registry/deprecate", { name: name, message: message })
    }

    /**
     * Where the registry is: the API publishes go to, and where a mod's page is read. There's
     * no web page for a mod yet, so its page is the API's own (a mod's JSON).
     */
    const where = { api: opts.api.base, web: `${opts.api.base}/api/terrafirma/registry/packages` }

    return { where: where, check: check, publish: publish, page: page, resolve: resolve, add: add, deprecate: deprecate }
}

export type RegistryT = ReturnType<typeof Registry>
