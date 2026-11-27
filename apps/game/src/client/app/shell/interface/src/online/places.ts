import type { Friend, PublicServer, Social } from "../bridge/index.ts"

/** Where friends are, read from their presence: the worlds they're in, and which can be joined. */
export const places = {
    /** Friends in a world (theirs or another's), joinable ones first. */
    playing(social: Social | null): Friend[] {
        const friends = social?.view?.friends ?? []
        return friends.filter((f) => f.presence !== null && f.presence.state !== "menu")
            .sort((a, b) => Number(places.joinable(b)) - Number(places.joinable(a)) || a.username.localeCompare(b.username))
    },
    /** A friend hosting a world open to friends: the only kind `social.join` can reach. */
    joinable(friend: Friend): boolean {
        return friend.presence?.state === "hosting" && friend.presence.open
    },
    /** The busiest public servers first. */
    popular(servers: readonly PublicServer[], count: number): PublicServer[] {
        return [...servers].sort((a, b) => b.players - a.players).slice(0, count)
    },
}
