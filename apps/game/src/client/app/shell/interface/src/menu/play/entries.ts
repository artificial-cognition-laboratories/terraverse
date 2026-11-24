import type { Invite, LocalServer, Player, World } from "../../bridge/index.ts"
import { decodeWorldCode } from "../../worldgen/config.ts"
import { sizeLabel } from "../worldIcons.ts"

// What the Play list holds: your worlds; and online, servers on this machine, friends' open
// worlds and invitations, and an address typed into the search. Pure.

export type Entry =
    | { kind: "local"; id: string; label: string; search: string; world: World }
    | { kind: "friend"; id: string; label: string; note: string; tag: string; search: string; player: Player; world: string; invite?: Invite }
    | { kind: "server"; id: string; label: string; search: string; server: LocalServer }
    /** What's typed into the search, when it reads as an address: the engine judges it on Join. */
    | { kind: "address"; id: string; label: string; search: string; ref: string }
export type EntryGroup = { label: string; entries: Entry[] }

export const play = {
    /** How big its planet is, from its code; none for an older build's code. */
    planetSize(world: World): string | null {
        if (!world.code) return null
        try { return sizeLabel(decodeWorldCode(world.code).size) }
        catch { return null } // an older build's code: nothing to say about it
    },
    /** The search as an address to join, or null: the scheme, or a host with a port. */
    address(query: string): string | null {
        const text = query.trim()
        if (/^terra:\/\/\S+$/i.test(text)) return text
        if (/^[A-Za-z0-9.-]+:\d{1,5}$/.test(text) || /^\[[0-9a-fA-F:.]+\]:\d{1,5}$/.test(text)) return `terra://${text}`
        return null
    },
    date(seconds: number): string {
        return new Date(seconds * 1000).toLocaleDateString(undefined, { day: "numeric", month: "short", year: "numeric" })
    },
}
