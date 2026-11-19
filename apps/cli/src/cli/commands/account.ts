import { header, hyperlink, next, status, type RendererHandle } from "@arcforge/arcline"
import type { Identity, TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/** m:ss, the way a countdown is read. */
function countdown(ms: number): string {
    const s = Math.max(0, Math.floor(ms / 1000))
    return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`
}

/**
 * terra login · terra logout · terra whoami — your ArcLabs account, as terra holds it.
 * terra signs in as itself, with a key that can publish; the game keeps its own.
 */
export function AccountCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    const p = opts.presentation
    const account = opts.terra.account

    function who(identity: Identity): string {
        return identity.username ? `@${identity.username}` : (identity.name ?? identity.id)
    }

    /** Publishing needs a username: it's the scope. Said here, where it can be fixed first. */
    function scopeHint(identity: Identity): void {
        if (identity.username) return
        p.line(status(p.renderer, "warn", "No username yet", "choose one on your ArcLabs account: it's the @scope you publish under"))
    }

    async function login(args: Args): Promise<number> {
        flags.only(args, [])
        const pending = await account.authorize()
        const r = p.renderer
        p.line("")
        p.line(header(r, { title: "Sign in to Terraverse", subtitle: account.base }))
        p.line("")
        p.line(next(r, r.links ? hyperlink(pending.url, pending.url) : pending.url))
        p.line("")
        p.line(`   ${r.c.dim("Code")}   ${r.c.bold(r.c.primary(pending.code))}`)
        p.line("")
        const live = p.live((rr: RendererHandle, left: number, frame: string) => status(rr, "pending", "waiting for approval", `${countdown(left)} left`, frame), pending.expiresAt - Date.now())
        const tick = setInterval(() => live.set(pending.expiresAt - Date.now()), 1000)
        try {
            const identity = await account.complete(pending)
            live.abandon()
            p.line(status(r, "ok", `Signed in as ${who(identity)}`))
            scopeHint(identity)
            return 0
        } catch (error) {
            live.abandon()
            throw error
        } finally {
            clearInterval(tick)
        }
    }

    async function logout(args: Args): Promise<number> {
        flags.only(args, [])
        const had = account.logout()
        p.line(status(p.renderer, had ? "ok" : "info", had ? "Signed out" : "You weren't signed in"))
        return 0
    }

    async function whoami(args: Args): Promise<number> {
        flags.only(args, ["json"])
        const identity = await account.whoami()
        if (flags.on(args, "json")) {
            p.line(JSON.stringify(identity))
            return 0
        }
        p.line(status(p.renderer, "ok", who(identity), account.base))
        scopeHint(identity)
        return 0
    }

    return { login: login, logout: logout, whoami: whoami }
}
