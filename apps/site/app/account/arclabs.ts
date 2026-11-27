// The ArcLabs backend, as the site uses it: who someone is, the name they
// claim, the device codes they approve. The seam: every answer is checked here
// into a strict shape, and a 4xx is the person's to fix (Refused), anything else ours.

import { Refused } from "./refused"

/** The profile ArcLabs keeps beside the sign-in. */
export interface Profile {
    username: string | null
    name: string | null
    /** The username was chosen by its person, not made up at sign-up: then it never changes */
    chosen: boolean
}

export function ArcLabs(opts: { api: string; token: () => string | null }) {
    async function call(method: "GET" | "POST" | "PATCH", path: string, body?: unknown): Promise<unknown> {
        const token = opts.token()
        if (!token) throw new Refused("Sign in first.")
        const response = await fetch(`${opts.api}${path}`, {
            method: method,
            headers: { Authorization: `Bearer ${token}`, ...(body === undefined ? {} : { "Content-Type": "application/json" }) },
            ...(body === undefined ? {} : { body: JSON.stringify(body) }),
        })
        if (response.status >= 400 && response.status < 500) throw new Refused(await reason(response))
        if (!response.ok) throw new Error(`ArcLabs ${method} ${path}: ${response.status} ${await reason(response)}`)
        return response.json()
    }

    return {
        async profile(): Promise<Profile> {
            return profile(await call("GET", "/api/user/profile"))
        },
        /** Chooses the name, once: refused if it's taken, the wrong shape, or already chosen. */
        async claim(name: string): Promise<{ username: string }> {
            const answer = await call("POST", "/api/user/profile/handle", { username: name })
            const username = Reflect.get(answer as object, "username")
            if (typeof username !== "string") throw new Error("ArcLabs claimed a name and didn't say which")
            return { username: username }
        },
        async approve(code: string): Promise<void> {
            await call("POST", "/api/user/device/approve", { user_code: code })
        },
    }
}
export type ArcLabs = ReturnType<typeof ArcLabs>

// h3 says why in statusMessage; a proxy's page says nothing worth showing
async function reason(response: Response): Promise<string> {
    const text = await response.text()
    try {
        const message = Reflect.get(JSON.parse(text) as object, "statusMessage")
        if (typeof message === "string" && message) return message
    } catch (e) {
        if (!(e instanceof SyntaxError)) throw e
    }
    return response.statusText || `status ${response.status}`
}

function profile(value: unknown): Profile {
    if (typeof value !== "object" || value === null) throw new Error("ArcLabs' profile isn't an object")
    const username = Reflect.get(value, "username")
    const name = Reflect.get(value, "name")
    const chosen = Reflect.get(value, "usernameChosen")
    if (typeof chosen !== "boolean") throw new Error("ArcLabs' profile doesn't say whether its name was chosen")
    const nullable = (v: unknown): v is string | null => v === null || v === undefined || typeof v === "string"
    if (!nullable(username) || !nullable(name)) throw new Error("ArcLabs' profile has the wrong shape")
    return { username: username ?? null, name: name ?? null, chosen: chosen }
}
