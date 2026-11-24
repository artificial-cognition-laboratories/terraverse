// The account: who is using the site, as ArcLabs knows them, and what they've
// starred. One door for every page. Signing in is Supabase's (the accounts
// ArcLabs is built on, shared with the game and axon.arclabs.it); the name and
// device codes are the backend's (./arclabs.ts). The client plugin wires both.

import type { Session, SupabaseClient } from "@supabase/supabase-js"
import type { ArcLabs, Profile } from "./arclabs"
import { Refused } from "./refused"
import { store } from "./store"
import { username } from "./username"
import type { Credentials, DeviceClient, User } from "./types"

export type * from "./types"
export { username } from "./username"
export { Refused } from "./refused"

/** What makes an email and password worth sending: checked as the page asks and before any request. */
export const credential = {
    problem(c: Credentials): string | null {
        if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(c.email)) return "That email doesn't look right."
        if (c.password.length < 8) return "Passwords are at least 8 characters."
        return null
    },
}

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
    // Written by the client plugin: undefined until the browser's session is read, null when signed out
    const session = useState<Session | null | undefined>("session", () => undefined)
    // null until ArcLabs has said who the session is
    const profile = useState<Profile | null>("profile", () => null)
    const stars = useState<string[] | null>("stars", () => null)
    // Who was signed in here last (./store): only for drawing, until the session is read
    const last = useState<User | null>("last-user", () => null)

    const ready = computed(() => session.value === null || (session.value !== undefined && profile.value !== null))
    const user = computed((): User | null => {
        const s = session.value
        if (!s || !profile.value) return null
        const meta = s.user.user_metadata as Record<string, unknown>
        const named = typeof meta.full_name === "string" ? meta.full_name : null
        return { id: s.user.id, email: s.user.email ?? "", name: profile.value.name ?? named, username: profile.value.username }
    })

    /** Who to draw as signed in: the session's user once it's read, who was here last until then */
    const seen = computed((): User | null => (ready.value ? user.value : last.value))

    function clients(): { supabase: SupabaseClient; arclabs: ArcLabs } {
        const app = useNuxtApp()
        return { supabase: app.$supabase as SupabaseClient, arclabs: app.$arclabs as ArcLabs }
    }

    /** Sign in with an email and password. */
    async function signIn(c: Credentials): Promise<void> {
        const problem = credential.problem(c)
        if (problem) throw new Refused(problem)
        const { error } = await clients().supabase.auth.signInWithPassword(c)
        if (error) throw refusal(error)
    }

    /** Make an account with an email and password; the name is chosen next. */
    async function register(c: Credentials): Promise<void> {
        const problem = credential.problem(c)
        if (problem) throw new Refused(problem)
        const { data, error } = await clients().supabase.auth.signUp({ ...c, options: { emailRedirectTo: window.location.href } })
        if (error) throw refusal(error)
        // Where emails are confirmed (production), the account waits on the link
        if (!data.session) throw new Refused(`We've sent a link to ${c.email}: open it to finish.`)
    }

    /** Sign in, or make an account, with Google: away to Google and back to this page. */
    async function google(): Promise<void> {
        const { error } = await clients().supabase.auth.signInWithOAuth({ provider: "google", options: { redirectTo: window.location.href } })
        if (error) throw refusal(error)
    }

    async function claim(name: string): Promise<void> {
        if (!user.value) throw new Refused("Sign in first.")
        const problem = username.problem(name)
        if (problem) throw new Refused(problem)
        const claimed = await clients().arclabs.claim(name)
        profile.value = { ...profile.value!, username: claimed.username }
    }

    async function signOut(): Promise<void> {
        const { error } = await clients().supabase.auth.signOut()
        if (error) throw error
    }

    async function approve(code: string, client: DeviceClient): Promise<void> {
        if (!user.value) throw new Refused("Sign in first.")
        if (!DEVICE_CODE.test(code)) throw new Refused(`${code} isn't a device code: it looks like ABC-234.`)
        if (client === "terra-cli" && !user.value.username) throw new Refused("Choose a username first: terra publishes under it.")
        await clients().arclabs.approve(code)
    }

    // TEMP(2026-11-22): stars stay in this browser until the registry they star is ArcLabs' own
    const starred = {
        has: (name: string) => stars.value?.includes(name) ?? false,
        list: () => stars.value ?? [],
        /** Star or unstar; refuses when signed out (the page offers sign-in instead) */
        toggle(name: string) {
            if (!user.value) throw new Refused("Sign in to star.")
            const all = starred.list()
            stars.value = all.includes(name) ? all.filter(s => s !== name) : [...all, name]
            store.write({ stars: stars.value })
        },
    }

    return {
        /** Whether the session and its profile have been read: before it, nobody is known either way */
        ready: ready,
        user: user,
        seen: seen,
        /** This browser's stars and last user, read once the page is up (the prerendered page never knows them) */
        restore() {
            const saved = store.read()
            stars.value = saved.stars
            last.value = saved.user
        },
        /** Remembers who is signed in now, for the next visit's first frame */
        remember: (now: User | null) => store.write({ user: now }),
        signIn: signIn,
        register: register,
        google: google,
        claim: claim,
        signOut: signOut,
        device: { approve: approve },
        stars: starred,
    }
}
export type Account = ReturnType<typeof Account>

export function useAccount(): Account {
    return Account()
}

// Supabase's 4xx are the person's (wrong password, taken email); anything else is ours
function refusal(error: { message: string; status?: number }): Error {
    return error.status !== undefined && error.status >= 400 && error.status < 500 ? new Refused(error.message) : error instanceof Error ? error : new Error(error.message)
}
