import { err } from "../../errors"
import type { PackageData } from "./types"

/** Packages — reads package.json files, the server's and every mod's, checked where they're read. */
export function Packages(_opts: Record<string, never>) {
    async function read(path: string): Promise<PackageData> {
        const file = Bun.file(path)
        if (!(await file.exists())) throw err("PACKAGE_MISSING", { detail: `${path} is missing` })
        const data: unknown = await file.json().catch((e: unknown) => {
            throw err("PACKAGE_INVALID", { detail: `${path} isn't JSON`, cause: e })
        })
        return shape(path, data)
    }

    return { read: read }
}

export type PackagesT = ReturnType<typeof Packages>

function shape(path: string, data: unknown): PackageData {
    if (typeof data !== "object" || data === null) throw err("PACKAGE_INVALID", { detail: `${path} isn't an object` })
    const name: unknown = Reflect.get(data, "name")
    const version: unknown = Reflect.get(data, "version") ?? "0.0.0"
    const deps: unknown = Reflect.get(data, "dependencies") ?? {}
    if (typeof name !== "string") throw err("PACKAGE_INVALID", { detail: `${path} has no name` })
    if (typeof version !== "string") throw err("PACKAGE_INVALID", { detail: `${path}: version isn't a string` })
    if (typeof deps !== "object" || deps === null) throw err("PACKAGE_INVALID", { detail: `${path}: dependencies isn't an object` })
    const dependencies: Record<string, string> = {}
    for (const [dep, spec] of Object.entries(deps)) {
        if (typeof spec !== "string") throw err("PACKAGE_INVALID", { detail: `${path}: dependency ${dep} isn't a version` })
        dependencies[dep] = spec
    }
    return { name: name, version: version, dependencies: dependencies }
}
