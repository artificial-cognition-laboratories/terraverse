import type { Friend, Player, Social } from "../bridge/index.ts"

/**
 * The friends popover's list, in the order it's read: what's waiting on you, then who you can
 * join, who's around, and who's away. A group with no one in it isn't shown.
 */
export type Group =
    | { id: "requests"; label: string; players: Player[] }
    | { id: "playing" | "online" | "offline"; label: string; friends: Friend[] }

export function groups(view: NonNullable<Social["view"]>, query = ""): Group[] {
    const q = query.toLowerCase()
    const match = (p: Player) => !q || p.username.toLowerCase().includes(q) || !!p.name?.toLowerCase().includes(q)
    const byName = (a: Friend, b: Friend) => a.username.localeCompare(b.username)
    const playing = view.friends.filter(match).filter((f) => f.presence && f.presence.state !== "menu").sort(byName)
    const online = view.friends.filter(match).filter((f) => f.presence?.state === "menu").sort(byName)
    const offline = view.friends.filter(match).filter((f) => !f.presence).sort(byName)
    const all: Group[] = [
        { id: "requests", label: "Requests", players: view.incoming.filter(match) },
        { id: "playing", label: "In a world", friends: playing },
        { id: "online", label: "Online", friends: online },
        { id: "offline", label: "Offline", friends: offline },
    ]
    return all.filter((g) => ("players" in g ? g.players : g.friends).length > 0)
}

/** A friend's line under their name: where they are, in a few words. */
export function whereabouts(friend: Friend): string {
    const p = friend.presence
    if (!p) return "Offline"
    if (p.state === "menu") return "In the menu"
    if (p.state === "hosting") return p.open ? `Hosting ${p.world ?? "a world"}` : `In ${p.world ?? "their world"}`
    return `In ${p.world ?? "a world"}`
}

/** Their world can be joined from here: hosting and opened to friends. */
export function joinable(friend: Friend): boolean {
    return friend.presence?.state === "hosting" && friend.presence.open
}
