import { chmodSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs"
import { dirname } from "node:path"
import { err } from "../../errors"
import { Api, record, type ApiT } from "./api"

type AccountOpts = {
    /** The ArcLabs API, no trailing slash. */
    base: string
    /** Where the key is kept: one file per API, so staging and production never mix. */
    file: string
    /**
     * A key from the environment (TERRA_TOKEN), for machines that can't sign in by the
     * device flow: CI publishing the official mods. Wins over the file, which is left alone.
     */
    token?: string | null
}

/** A pending sign-in, as the person at the terminal needs it. */
export type Authorization = {
    /** The page to approve it on, with the code already in it. */
    url: string
    /** The code, for typing into another device. */
    code: string
    /** When it stops working, ms since the epoch. */
    expiresAt: number
}

/** Who is signed in. */
export type Identity = {
    id: string
    /** The scope they publish under, without "@": null until they choose a username. */
    username: string | null
    name: string | null
}

/**
 * Account — `terra login`'s key: the device flow that gets it, and the file that keeps it.
 *
 * terra signs in as its own client ("terra-cli"), not with the game's key: the game's can
 * only play, and a key that can publish shouldn't live where the game does. The file is
 * per API, the way the game keeps its own, so pointing ARCLABS_URL at staging never
 * sends a production key there.
 */
export function Account(opts: AccountOpts) {
    const token = opts.token || null
    let key: string | null = token ?? read()
    const api: ApiT = Api({ base: opts.base, key: () => key })

    function read(): string | null {
        if (!existsSync(opts.file)) return null
        let saved: unknown
        try {
            saved = JSON.parse(readFileSync(opts.file, "utf8"))
        } catch (e) {
            throw err("UNEXPECTED", { detail: `${opts.file} isn't the account terra saved: delete it and run terra login`, cause: e })
        }
        const value = typeof saved === "object" && saved !== null ? Reflect.get(saved, "key") : undefined
        if (typeof value !== "string" || !value) throw err("UNEXPECTED", { detail: `${opts.file} has no key: delete it and run terra login` })
        return value
    }

    function save(value: string): void {
        mkdirSync(dirname(opts.file), { recursive: true })
        writeFileSync(opts.file, JSON.stringify({ key: value }) + "\n", { mode: 0o600 })
        // it's a credential: owner-only even where the file already existed
        if (process.platform !== "win32") chmodSync(opts.file, 0o600)
        key = value
    }

    /** Start a sign-in: the code to approve. */
    async function authorize(): Promise<Authorization & { device: string; interval: number }> {
        if (token) throw err("USAGE", { detail: "signed in by TERRA_TOKEN: unset it to sign in as someone else" })
        const body = record(await api.post("/api/user/device/authorize", { client: "terra-cli" }, "none"), "the device authorization")
        const device = body.device_code, code = body.user_code, url = body.verification_uri_complete
        const expires = body.expires_in, interval = body.interval
        if (typeof device !== "string" || typeof code !== "string" || typeof url !== "string" || typeof expires !== "number" || typeof interval !== "number")
            throw err("REGISTRY_REFUSED", { detail: "the device authorization is missing a field" })
        return { device: device, code: code, url: url, expiresAt: Date.now() + expires * 1000, interval: interval }
    }

    /** Wait for the code to be approved, then keep the key. Refuses once it has expired. */
    async function complete(pending: { device: string; interval: number; expiresAt: number }, sleep = Bun.sleep): Promise<Identity> {
        for (;;) {
            if (Date.now() > pending.expiresAt) throw err("LOGIN_EXPIRED")
            await sleep(pending.interval * 1000)
            const body = record(await api.get(`/api/user/device/poll?device_code=${encodeURIComponent(pending.device)}`, "none"), "the device poll")
            if (body.status === "pending") continue
            if (body.status === "expired") throw err("LOGIN_EXPIRED")
            const token = body.access_token
            if (body.status !== "approved" || typeof token !== "string") throw err("REGISTRY_REFUSED", { detail: `the device poll answered ${JSON.stringify(body.status)}` })
            save(token)
            return whoami()
        }
    }

    /** Who the key belongs to. Refuses when signed out. */
    async function whoami(): Promise<Identity> {
        const body = record(await api.get("/api/terrafirma/me", "required"), "who you are")
        if (typeof body.id !== "string") throw err("REGISTRY_REFUSED", { detail: "who you are has no id" })
        return {
            id: body.id,
            username: typeof body.username === "string" ? body.username : null,
            name: typeof body.name === "string" ? body.name : null,
        }
    }

    /** Forget the key. Whether there was one. */
    function logout(): boolean {
        if (token) throw err("USAGE", { detail: "signed in by TERRA_TOKEN: unset it to sign out" })
        const had = key !== null
        rmSync(opts.file, { force: true })
        key = null
        return had
    }

    return {
        base: opts.base,
        api: api,
        signedIn: () => key !== null,
        authorize: authorize,
        complete: complete,
        whoami: whoami,
        logout: logout,
    }
}

export type AccountT = ReturnType<typeof Account>

/** The account file for an API: production's is account.json, any other's is named by its host. */
export function accountFile(dir: string, base: string, defaultBase: string): string {
    if (base === defaultBase) return `${dir}/account.json`
    const host = base.replace(/^[a-z]+:\/\//, "").replace(/[^a-zA-Z0-9.-]/g, "_")
    return `${dir}/account-${host}.json`
}
