import { err } from "../../errors"

/** The ArcLabs API the game also signs in to, unless ARCLABS_URL names another (local staging is http://localhost:3099). */
export const DEFAULT_ARCLABS = "https://axon-api-t53zrgvpga-ew.a.run.app"

type ApiOpts = {
    /** The ArcLabs API, no trailing slash. */
    base: string
    /** The signed-in key, read when a request needs it: null when signed out. */
    key: () => string | null
}

/** What a request sends: JSON, a form, or nothing. */
type Body = { json: unknown } | { form: FormData } | null

/**
 * Api — terra's requests to the ArcLabs API. A refusal comes back as REGISTRY_REFUSED with
 * the server's own reason, a dead key as ACCOUNT_REFUSED, no answer at all as
 * REGISTRY_UNREACHABLE: what the person at the terminal can do differs for each.
 */
export function Api(opts: ApiOpts) {
    async function request(method: "GET" | "POST", path: string, body: Body, auth: "required" | "optional" | "none", allowed: readonly number[] = []): Promise<Response> {
        const headers: Record<string, string> = { accept: "application/json" }
        const key = auth === "none" ? null : opts.key()
        if (auth === "required" && !key) throw err("SIGNED_OUT", { detail: `${method} ${path} needs you signed in` })
        if (key) headers.authorization = `Bearer ${key}`
        let payload: BodyInit | undefined
        if (body && "json" in body) {
            headers["content-type"] = "application/json"
            payload = JSON.stringify(body.json)
        } else if (body) {
            payload = body.form
        }

        const url = `${opts.base}${path}`
        const response = await fetch(url, { method: method, headers: headers, ...(payload !== undefined ? { body: payload } : {}) }).catch((e: unknown) => {
            throw err("REGISTRY_UNREACHABLE", { detail: `${method} ${url}`, cause: e })
        })
        if (response.ok || allowed.includes(response.status)) return response

        const reason = await refusal(response)
        if (response.status === 401 && key) throw err("ACCOUNT_REFUSED", { detail: reason })
        throw err("REGISTRY_REFUSED", { detail: reason, context: { status: response.status, path: path } })
    }

    /** A JSON answer, as unknown: each caller checks the shape it relies on. */
    async function json(method: "GET" | "POST", path: string, body: Body, auth: "required" | "optional" | "none"): Promise<unknown> {
        const response = await request(method, path, body, auth)
        return response.json().catch((e: unknown) => {
            throw err("REGISTRY_REFUSED", { detail: `${method} ${path} answered something that isn't JSON`, cause: e })
        })
    }

    return {
        base: opts.base,
        get: (path: string, auth: "required" | "optional" | "none" = "optional") => json("GET", path, null, auth),
        post: (path: string, body: unknown, auth: "required" | "optional" | "none" = "required") => json("POST", path, { json: body }, auth),
        upload: (path: string, form: FormData) => json("POST", path, { form: form }, "required"),
        /** A GET whose 404 means "nothing there" rather than a failure: null. */
        async find(path: string): Promise<unknown | null> {
            const response = await request("GET", path, null, "optional", [404])
            if (response.status === 404) return null
            return response.json()
        },
    }
}

export type ApiT = ReturnType<typeof Api>

/** The server's reason for a refusal: h3's message, or the status line. */
async function refusal(response: Response): Promise<string> {
    const text = await response.text().catch(() => "")
    try {
        const body: unknown = JSON.parse(text)
        const message = typeof body === "object" && body !== null ? (Reflect.get(body, "message") ?? Reflect.get(body, "statusMessage")) : undefined
        if (typeof message === "string" && message) return message
    } catch {
        // not JSON: the status line is the reason
    }
    return `${response.status} ${response.statusText}`.trim()
}

/** A record, or throws: what every caller of Api does to an answer before reading it. */
export function record(value: unknown, what: string): Record<string, unknown> {
    if (typeof value !== "object" || value === null || Array.isArray(value)) throw err("REGISTRY_REFUSED", { detail: `${what} isn't an object` })
    return Object.fromEntries(Object.entries(value))
}
