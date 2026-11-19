import { err } from "../../errors"

/** What a mod passes the API, checked where it arrives: mods are someone else's code. */
export const checks = {
    function<F>(value: F, what: string): F {
        if (typeof value !== "function") throw err("ARGUMENT_INVALID", { detail: `${what} must be a function` })
        return value
    },

    /** A Duration, in seconds. */
    seconds(d: unknown): number {
        if (typeof d !== "object" || d === null) throw err("ARGUMENT_INVALID", { detail: "a duration is { seconds?, minutes?, hours? }" })
        const part = (key: string): number => {
            const v: unknown = Reflect.get(d, key) ?? 0
            if (typeof v !== "number") throw err("ARGUMENT_INVALID", { detail: `a duration's ${key} is a number` })
            return v
        }
        const s = part("seconds") + part("minutes") * 60 + part("hours") * 3600
        if (!Number.isFinite(s) || s < 0) throw err("ARGUMENT_INVALID", { detail: `a duration can't be ${JSON.stringify(d)}` })
        return s
    },

    /** A permission's name: "kilns:light", a mod's prefix and its own name; or core's. */
    permission(name: unknown): string {
        if (typeof name !== "string" || !/^[a-z0-9_]+:[a-z0-9_]+$/.test(name))
            throw err("ARGUMENT_INVALID", { detail: `a permission is named prefix:name, like "kilns:light", not ${JSON.stringify(name)}` })
        return name
    },

    unwired(what: string): never {
        throw err("NOT_WIRED", { detail: `${what} isn't wired yet` })
    },
}
