import { err } from "../../../errors"
import type { ConfigsT } from "../configs"
import type { ServerConfigData } from "../types"

/**
 * ServerConfig — server.config.ts, read and checked: how this universe is set up. Read
 * without running any mod, so a setup screen or the registry can show it.
 */
export function ServerConfig(opts: { root: string; configs: ConfigsT }) {
    async function read(name: string): Promise<ServerConfigData> {
        return shape(await opts.configs.evaluate("server", opts.root), name)
    }

    return { read: read }
}

export type ServerConfigT = ReturnType<typeof ServerConfig>

const where = "server.config.ts"
const invalid = (detail: string) => err("CONFIG_INVALID", { detail: `${where}: ${detail}` })
const isRecord = (v: unknown): v is Record<string, unknown> => typeof v === "object" && v !== null && !Array.isArray(v)
const JOIN = ["anyone", "friends", "invited"] as const

function shape(config: object, name: string): ServerConfigData {
    const title: unknown = Reflect.get(config, "title") ?? name
    const system: unknown = Reflect.get(config, "system")
    const worlds: unknown = Reflect.get(config, "worlds") ?? {}
    const settings: unknown = Reflect.get(config, "settings") ?? {}
    const access: unknown = Reflect.get(config, "access") ?? {}
    if (typeof title !== "string") throw invalid("title must be a string")
    // the engine reads the code itself, and refuses one it can't; this only catches a slip
    if (typeof system !== "string" || !/^TF\d+\./.test(system)) throw invalid(`system is a world code ("TF9.…"), not ${JSON.stringify(system)}`)
    if (!isRecord(worlds) || !Object.values(worlds).every(b => typeof b === "string" && b.startsWith("@")))
        throw invalid("worlds is { name: \"@body\" }")
    if (!isRecord(settings) || !Object.values(settings).every(isRecord)) throw invalid("settings is { \"@mod/name\": { setting: value } }")
    if (!isRecord(access)) throw invalid("access is { join?, roles? }")
    const join: unknown = access.join ?? "friends"
    const roles: unknown = access.roles ?? {}
    if (!JOIN.some(j => j === join)) throw invalid(`access.join is ${JOIN.join(", ")}`)
    if (!isRecord(roles) || !Object.values(roles).every(r => Array.isArray(r) && r.every(a => typeof a === "string")))
        throw invalid("access.roles is { role: [\"@account\"] }")
    return {
        title: title,
        system: system,
        worlds: Object.fromEntries(Object.entries(worlds).map(([k, v]) => [k, String(v)])),
        settings: Object.fromEntries(Object.entries(settings).map(([k, v]) => [k, isRecord(v) ? { ...v } : {}])),
        access: {
            join: JOIN.find(j => j === join) ?? "friends",
            roles: Object.fromEntries(Object.entries(roles).map(([k, v]) => [k, Array.isArray(v) ? v.map(String) : []])),
        },
    }
}
