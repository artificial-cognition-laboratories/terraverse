import type { Friend, Presence } from "../bridge"

/** How a friend's whereabouts read, and how they sort: joinable first, offline last. */
export function presenceLine(presence: Presence | null): string {
    if (!presence) return "Offline"
    switch (presence.state) {
        case "menu": return "In the menu"
        case "playing": return presence.world ? `In ${presence.world}` : "Playing"
        case "hosting": return presence.open ? `Hosting ${presence.world} · open to friends` : `In ${presence.world}`
    }
}

export type Dot = "open" | "online" | "offline"

export function presenceDot(presence: Presence | null): Dot {
    if (!presence) return "offline"
    return presence.state === "hosting" && presence.open ? "open" : "online"
}

const RANK: Record<Dot, number> = { open: 0, online: 1, offline: 2 }

export function byPresence(a: Friend, b: Friend): number {
    return RANK[presenceDot(a.presence)] - RANK[presenceDot(b.presence)] || a.username.localeCompare(b.username)
}
