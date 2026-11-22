import type { Invite, Player, World } from "../../bridge/index.ts"
import { decodeWorldCode } from "../../worldgen/config.ts"
import { sizeLabel } from "../worldIcons.ts"

// What the Play list holds: your worlds, and friends' open worlds and invitations. Pure.

export type Entry =
    | { kind: "local"; id: string; label: string; search: string; world: World }
    | { kind: "friend"; id: string; label: string; note: string; tag: string; search: string; player: Player; world: string; invite?: Invite }
export type EntryGroup = { label: string; entries: Entry[] }

export const play = {
    /** How big its planet is, from its code; none for an older build's code. */
    planetSize(world: World): string | null {
        if (!world.code) return null
        try { return sizeLabel(decodeWorldCode(world.code).size) }
        catch { return null } // an older build's code: nothing to say about it
    },
    date(seconds: number): string {
        return new Date(seconds * 1000).toLocaleDateString(undefined, { day: "numeric", month: "short", year: "numeric" })
    },
}
