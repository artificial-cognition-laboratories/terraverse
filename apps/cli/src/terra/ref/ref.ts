/**
 * terra:// refs — every place you can go, one address. Parsed once, here, into a strict
 * union; past this nothing re-checks. The engine parses the same grammar (shared/platform/online/ref),
 * and both pass one golden table (apps/game/assets/misc/refs.json).
 *
 * A ref never carries a secret: what admits you to a server on this machine is in its running/
 * record, found by the address.
 */
export type Ref =
    /** A server at an address. */
    | { kind: "address"; host: string; port: number }
    /** A clean universe from a world code, opened alone. */
    | { kind: "code"; code: string }
    /** A published server, by its registry name. */
    | { kind: "published"; scope: string; name: string }
    /** A friend's open world. */
    | { kind: "friend"; player: string }

export const SCHEME = "terra://"
/** Where a server listens when its ref doesn't say. */
export const DEFAULT_PORT = 30000

const CODE = /^TF\d+\.[A-Za-z0-9._-]+$/
const PUBLISHED = /^@([a-z0-9-]+)\/([a-z0-9-]+)$/
const FRIEND = /^friend\/([A-Za-z0-9_.-]{1,32})$/
const HOST = /^(?:\[([0-9a-fA-F:.]+)\]|([A-Za-z0-9.-]+))(?::(\d{1,5}))?$/

export const refs = {
    /** What `text` refers to, or null when it isn't a ref. */
    parse(text: string): Ref | null {
        let rest = text.trim()
        if (rest.toLowerCase().startsWith(SCHEME)) rest = rest.slice(SCHEME.length)
        else if (rest.includes("://")) return null
        if (rest.endsWith("/")) rest = rest.slice(0, -1)
        if (!rest) return null
        if (CODE.test(rest)) return { kind: "code", code: rest }
        if (rest.startsWith("@")) {
            const m = PUBLISHED.exec(rest)
            return m ? { kind: "published", scope: m[1]!, name: m[2]! } : null
        }
        // "friend" is a kind, never a host
        if (rest === "friend" || rest.startsWith("friend/")) {
            const m = FRIEND.exec(rest)
            return m ? { kind: "friend", player: m[1]! } : null
        }
        const m = HOST.exec(rest)
        if (!m) return null
        const port = m[3] === undefined ? DEFAULT_PORT : Number(m[3])
        if (port < 1 || port > 65535) return null
        const host = m[1] ?? m[2]!.toLowerCase()
        if (m[2] !== undefined && (m[2].startsWith(".") || m[2].endsWith(".") || m[2].startsWith("-"))) return null
        return { kind: "address", host: host, port: port }
    },

    /** Its one spelling. */
    print(ref: Ref): string {
        switch (ref.kind) {
            case "address": return `${SCHEME}${ref.host.includes(":") ? `[${ref.host}]` : ref.host}:${ref.port}`
            case "code": return SCHEME + ref.code
            case "published": return `${SCHEME}@${ref.scope}/${ref.name}`
            case "friend": return `${SCHEME}friend/${ref.player}`
        }
    },
}
