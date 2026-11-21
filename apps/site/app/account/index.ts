// The account: who is using the site, and what they've
// starred. One door for every page; today it answers from this browser
// (./mock.ts, ./store.ts), and pointed at ArcLabs no page changes.

import { MOCK_LATENCY_MS, MOCK_TAKEN, MOCK_USER } from "./mock"
import { store, type Saved } from "./store"
import { username } from "./username"
import type { Credentials, DeviceClient, User } from "./types"

export type * from "./types"
export { username } from "./username"

/** What makes an email and password worth sending: checked as the page asks and before any request. */
export const credential = {
    problem(c: Credentials): string | null {
        if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(c.email)) return "That email doesn't look right."
        if (c.password.length < 8) return "Passwords are at least 8 characters."
        return null
    },
}

/** An expected no, said to the person as it is. */
export class Refused extends Error {}

/** A device's code as ArcLabs mints it: three, a hyphen, three, from letters and digits that can't be misread. */
export const DEVICE_CODE = /^[A-HJ-NP-Z2-9]{3}-[A-HJ-NP-Z2-9]{3}$/

/**
 * Where to go once signed in, from the address's `next`: a path on this site,
 * or the registry. Never another site: a link can't send someone off after signing in.
 */
export function returnTo(next: unknown): string {
    return typeof next === "string" && next.startsWith("/") && !next.startsWith("//") ? next : "/registry"
}

export function Account() {
    // Shared by every page; null until this browser has been read (the prerendered page knows no one)
    const saved = useState<Saved | null>("account", () => null)
    const user = computed(() => saved.value?.user ?? null)

    function change(next: Partial<Saved>) {
        const current = saved.value ?? store.read()
        saved.value = { ...current, ...next }
        store.write(saved.value)
    }

    // TEMP(2026-11-21): every verb below pretends to be ArcLabs (./mock.ts), which
    // the site's accounts are underneath; the person never needs to know it

    /** Sign in with an email and password. */
    async function signIn(credentials: Credentials): Promise<User> {
        const problem = credential.problem(credentials)
        if (problem) throw new Refused(problem)
        await wait()
        return enter(credentials.email)
    }

    /** Make an account with an email and password; the name is chosen next. */
    async function register(credentials: Credentials): Promise<User> {
        const problem = credential.problem(credentials)
        if (problem) throw new Refused(problem)
        await wait()
        return enter(credentials.email)
    }

    /** Sign in, or make an account, with Google: the same thing to Google. */
    async function google(): Promise<User> {
        await wait()
        return enter(MOCK_USER.email)
    }

    // In, as this email: with the name this browser saw claimed, if any
    function enter(email: string): User {
        const claimed = (saved.value ?? store.read()).claimed
        const signedIn: User = { ...MOCK_USER, email: email, username: claimed }
        change({ user: signedIn })
        return signedIn
    }

    async function claim(name: string): Promise<User> {
        const current = user.value
        if (!current) throw new Refused("Sign in first.")
        const problem = username.problem(name)
        if (problem) throw new Refused(problem)
        await wait()
        if (MOCK_TAKEN.has(name)) throw new Refused(`@${name} is taken.`)
        const named: User = { ...current, username: name }
        change({ user: named, claimed: name })
        return named
    }

    function signOut() {
        change({ user: null })
    }

    async function approve(code: string, client: DeviceClient): Promise<void> {
        if (!user.value) throw new Refused("Sign in first.")
        if (!DEVICE_CODE.test(code)) throw new Refused(`${code} isn't a device code: it looks like ABC-234.`)
        if (client === "terra-cli" && !user.value.username) throw new Refused("Choose a username first: terra publishes under it.")
        await wait()
    }

    const stars = {
        has: (name: string) => saved.value?.stars.includes(name) ?? false,
        list: () => saved.value?.stars ?? [],
        /** Star or unstar; refuses when signed out (the page offers sign-in instead) */
        toggle(name: string) {
            if (!user.value) throw new Refused("Sign in to star.")
            const all = stars.list()
            change({ stars: all.includes(name) ? all.filter(s => s !== name) : [...all, name] })
        },
    }

    return {
        /** Whether this browser has been read yet: before it, nobody is known either way */
        ready: computed(() => saved.value !== null),
        user: user,
        /** Read this browser's account; the client plugin does this once */
        restore: () => (saved.value = store.read()),
        signIn: signIn,
        register: register,
        google: google,
        claim: claim,
        signOut: signOut,
        device: { approve: approve },
        stars: stars,
    }
}
export type Account = ReturnType<typeof Account>

export function useAccount(): Account {
    return Account()
}

function wait(): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, MOCK_LATENCY_MS))
}
