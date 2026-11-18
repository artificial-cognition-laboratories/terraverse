import { err } from "../../errors"
import type { Mod } from "./types"

/** A value a server can choose for a mod. */
export type SettingValue = string | number | boolean

/** What a mod declares a setting to be (mod.config.ts `settings`). */
export type SettingDeclaration =
    | { readonly type: "number"; readonly default: number; readonly range?: readonly [number, number]; readonly step?: number; readonly unit?: string; readonly description?: string }
    | { readonly type: "boolean"; readonly default: boolean; readonly description?: string }
    | { readonly type: "choice"; readonly default: string; readonly options: Readonly<Record<string, string>>; readonly description?: string }
    | { readonly type: "string"; readonly default: string; readonly pattern?: string; readonly description?: string }

export type SettingDeclarations = Readonly<Record<string, SettingDeclaration>>

const NAME = /^[A-Za-z_$][\w$]*$/

/**
 * Settings — what a mod lets a server choose, and what a server chose: declared in
 * mod.config.ts, chosen in server.config.ts, checked before any mod runs. A value is
 * never missing and never out of range by the time a mod reads it.
 */
export const settings = {
    /** A mod's `settings`, as declared, checked. */
    declarations(raw: unknown, where: string): SettingDeclarations {
        if (raw === undefined) return {}
        if (typeof raw !== "object" || raw === null) throw invalid(where, "settings is { name: { type, default, … } }")
        const out: Record<string, SettingDeclaration> = {}
        for (const [name, decl] of Object.entries(raw)) {
            if (!NAME.test(name)) throw invalid(where, `${name} isn't a setting's name: letters, digits, _`)
            out[name] = settings.declaration(decl, `${where}: settings.${name}`)
        }
        return out
    },

    declaration(decl: unknown, where: string): SettingDeclaration {
        if (typeof decl !== "object" || decl === null) throw invalid(where, "a setting is { type, default, … }")
        const type: unknown = Reflect.get(decl, "type")
        const fallback: unknown = Reflect.get(decl, "default")
        const description: unknown = Reflect.get(decl, "description")
        if (description !== undefined && typeof description !== "string") throw invalid(where, "description is a string")
        const about = description === undefined ? {} : { description: description }
        if (type === "number") {
            const range: unknown = Reflect.get(decl, "range")
            const step: unknown = Reflect.get(decl, "step")
            const unit: unknown = Reflect.get(decl, "unit")
            if (typeof fallback !== "number") throw invalid(where, "a number's default is a number")
            const [least, most]: unknown[] = Array.isArray(range) ? range : []
            if (range !== undefined && !(Array.isArray(range) && range.length === 2 && typeof least === "number" && typeof most === "number" && least <= most))
                throw invalid(where, "range is [least, most]")
            if (step !== undefined && !(typeof step === "number" && step > 0)) throw invalid(where, "step is a number more than 0")
            if (unit !== undefined && typeof unit !== "string") throw invalid(where, "unit is a string")
            const bounds: readonly [number, number] | null = typeof least === "number" && typeof most === "number" ? [least, most] : null
            const declared: SettingDeclaration = {
                type: "number",
                default: fallback,
                ...(bounds ? { range: bounds } : {}),
                ...(typeof step === "number" ? { step: step } : {}),
                ...(typeof unit === "string" ? { unit: unit } : {}),
                ...about,
            }
            settings.check(declared, fallback, `${where}: its default`)
            return declared
        }
        if (type === "boolean") {
            if (typeof fallback !== "boolean") throw invalid(where, "a boolean's default is true or false")
            return { type: "boolean", default: fallback, ...about }
        }
        if (type === "choice") {
            const options: unknown = Reflect.get(decl, "options")
            if (typeof options !== "object" || options === null || Object.keys(options).length === 0 || !Object.values(options).every(l => typeof l === "string"))
                throw invalid(where, "options is { value: \"label\" }, at least one")
            const declared: SettingDeclaration = { type: "choice", default: String(fallback), options: Object.fromEntries(Object.entries(options).map(([k, v]) => [k, String(v)])), ...about }
            settings.check(declared, fallback, `${where}: its default`)
            return declared
        }
        if (type === "string") {
            const pattern: unknown = Reflect.get(decl, "pattern")
            if (typeof fallback !== "string") throw invalid(where, "a string's default is a string")
            if (pattern !== undefined && typeof pattern !== "string") throw invalid(where, "pattern is a regular expression, as a string")
            const declared: SettingDeclaration = { type: "string", default: fallback, ...(typeof pattern === "string" ? { pattern: pattern } : {}), ...about }
            settings.check(declared, fallback, `${where}: its default`)
            return declared
        }
        throw invalid(where, `type is "number", "boolean", "choice" or "string", not ${JSON.stringify(type)}`)
    },

    /** `value` as `decl` takes it, or SETTING_INVALID naming where. */
    check(decl: SettingDeclaration, value: unknown, where: string): SettingValue {
        const wrong = (detail: string) => err("SETTING_INVALID", { detail: `${where}: ${detail}` })
        switch (decl.type) {
            case "number": {
                if (typeof value !== "number" || !Number.isFinite(value)) throw wrong(`a number, not ${JSON.stringify(value)}`)
                if (decl.range && (value < decl.range[0] || value > decl.range[1]))
                    throw wrong(`${value} is outside ${decl.range[0]}–${decl.range[1]}${decl.unit ? ` ${decl.unit}` : ""}`)
                if (decl.step && Math.abs(Math.round((value - (decl.range?.[0] ?? 0)) / decl.step) * decl.step - (value - (decl.range?.[0] ?? 0))) > 1e-9)
                    throw wrong(`${value} isn't on a step of ${decl.step}`)
                return value
            }
            case "boolean":
                if (typeof value !== "boolean") throw wrong(`true or false, not ${JSON.stringify(value)}`)
                return value
            case "choice":
                if (typeof value !== "string" || !(value in decl.options)) throw wrong(`one of ${Object.keys(decl.options).join(", ")}, not ${JSON.stringify(value)}`)
                return value
            case "string":
                if (typeof value !== "string") throw wrong(`a string, not ${JSON.stringify(value)}`)
                if (decl.pattern && !new RegExp(decl.pattern).test(value)) throw wrong(`${JSON.stringify(value)} doesn't match ${decl.pattern}`)
                return value
        }
    },

    /** What each mod runs with: the server's choices, checked, and every default it didn't choose. */
    resolve(mods: readonly Mod[], chosen: Readonly<Record<string, Readonly<Record<string, unknown>>>>): Map<string, Record<string, SettingValue>> {
        const byName = new Map(mods.map(m => [m.name, m]))
        for (const name of Object.keys(chosen))
            if (!byName.has(name)) throw err("SETTING_UNKNOWN", { detail: `server.config.ts: settings for ${name}, which this server doesn't run` })
        const out = new Map<string, Record<string, SettingValue>>()
        for (const mod of mods) {
            const values = chosen[mod.name] ?? {}
            for (const key of Object.keys(values))
                if (!(key in mod.settings)) throw err("SETTING_UNKNOWN", { detail: `server.config.ts: ${mod.name} has no setting ${key}`, context: { mod: mod.name } })
            const resolved: Record<string, SettingValue> = {}
            for (const [key, decl] of Object.entries(mod.settings))
                resolved[key] = key in values ? settings.check(decl, values[key], `server.config.ts: settings["${mod.name}"].${key}`) : decl.default
            out.set(mod.name, resolved)
        }
        return out
    },

    /** The declarations, as types: `mod.settings` in a mod, and `settings` in a server's config. */
    types: {
        /**
         * A mod's `mod.settings`. With the settings of the mods it builds on, too: their source
         * is type-checked in this mod's frame when it imports them, and reads its own
         * `mod.settings` there. Loose for this mod (keys it doesn't have), never a false error;
         * a name two declare differently is either. Exact needs a frame per mod: references.
         */
        mod(declared: SettingDeclarations, builtOn: readonly Mod[] = []): string {
            const types = new Map<string, Set<string>>()
            for (const decls of [declared, ...builtOn.map(m => m.settings)])
                for (const [k, d] of Object.entries(decls)) types.set(k, (types.get(k) ?? new Set()).add(typeOf(d)))
            const fields = [...types].map(([k, t]) => `        readonly ${k}: ${[...t].join(" | ")}`)
            return `// ${GENERATED}\nexport {}\n\ndeclare global {\n    interface ModSettings {\n${fields.join("\n")}\n    }\n}\n`
        },

        server(mods: readonly Mod[]): string {
            const entries = mods.filter(m => Object.keys(m.settings).length > 0).map(m => {
                const fields = Object.entries(m.settings).map(([k, d]) => `            ${k}?: ${typeOf(d)}`)
                return `        ${JSON.stringify(m.name)}?: {\n${fields.join("\n")}\n        }`
            })
            return `// ${GENERATED}\nexport {}\n\ndeclare global {\n    interface ServerSettings {\n${entries.join("\n")}\n    }\n}\n`
        },
    },
}

const GENERATED = "Generated by terra prepare from the mods' declared settings. Don't edit."

function typeOf(decl: SettingDeclaration): string {
    if (decl.type === "choice") return Object.keys(decl.options).map(o => JSON.stringify(o)).join(" | ")
    return decl.type
}

function invalid(where: string, detail: string) {
    return err("CONFIG_INVALID", { detail: `${where}: ${detail}` })
}
