import { err } from "../../../errors"
import type { FrameT } from "../frame"
import type { SettingValue } from "../settings"
import type { ManifestData, ManifestMod } from "../types"

/** Manifest — .terra/manifest.json: what prepare made, and what the sidecar loads. */
export function Manifest(opts: { frame: FrameT }) {
    const path = opts.frame.path("manifest")

    async function write(data: ManifestData): Promise<void> {
        await Bun.write(path, JSON.stringify(data, null, 2) + "\n")
    }

    async function read(): Promise<ManifestData> {
        const file = Bun.file(path)
        if (!(await file.exists())) throw err("MANIFEST_MISSING", { detail: `${path} is missing` })
        const data: unknown = await file.json().catch((e: unknown) => {
            throw err("MANIFEST_INVALID", { detail: `${path} isn't JSON`, cause: e })
        })
        return shape(path, data)
    }

    return { path: path, write: write, read: read }
}

export type ManifestT = ReturnType<typeof Manifest>

const isString = (v: unknown): v is string => typeof v === "string"

function shape(path: string, data: unknown): ManifestData {
    if (typeof data !== "object" || data === null) throw err("MANIFEST_INVALID", { detail: `${path} isn't an object` })
    const version: unknown = Reflect.get(data, "version")
    if (version !== 1) throw err("MANIFEST_INVALID", { detail: `${path} is version ${version}; this terra reads 1` })
    const mods: unknown = Reflect.get(data, "mods")
    if (!Array.isArray(mods)) throw err("MANIFEST_INVALID", { detail: `${path}: mods isn't a list` })
    return {
        version: 1,
        mods: mods.map((m: unknown, i): ManifestMod => {
            if (typeof m !== "object" || m === null) throw err("MANIFEST_INVALID", { detail: `${path}: mods[${i}] isn't an object` })
            const name: unknown = Reflect.get(m, "name")
            const prefix: unknown = Reflect.get(m, "prefix")
            const version: unknown = Reflect.get(m, "version")
            const root: unknown = Reflect.get(m, "root")
            const server: unknown = Reflect.get(m, "server")
            const chosen: unknown = Reflect.get(m, "settings")
            if (!isString(name) || !isString(prefix) || !isString(version) || !isString(root) || !(server === null || isString(server)))
                throw err("MANIFEST_INVALID", { detail: `${path}: mods[${i}] isn't { name, prefix, version, root, server, settings }` })
            if (typeof chosen !== "object" || chosen === null || !Object.values(chosen).every(v => ["string", "number", "boolean"].includes(typeof v)))
                throw err("MANIFEST_INVALID", { detail: `${path}: mods[${i}].settings isn't { name: value }` })
            const values: Record<string, SettingValue> = {}
            for (const [k, v] of Object.entries(chosen)) if (typeof v === "string" || typeof v === "number" || typeof v === "boolean") values[k] = v
            return { name: name, prefix: prefix, version: version, root: root, server: server, settings: values }
        }),
    }
}
