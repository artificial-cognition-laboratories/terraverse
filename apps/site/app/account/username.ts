// What makes a username: pure, so the page can say what's wrong as it's typed
// and the account refuses the same things when it's claimed.

/** Names no one can take: ours, and the site's own addresses. */
const RESERVED = new Set(["terraverse", "arclabs", "admin", "registry", "docs", "wiki", "browse", "starred", "official"])

/** Each rule, in the order a person meets them as they type. */
const RULES: { test: (name: string) => boolean; reason: string }[] = [
    { test: n => n.length >= 3, reason: "At least 3 characters." },
    { test: n => n.length <= 24, reason: "At most 24 characters." },
    { test: n => /^[a-z0-9-]+$/.test(n), reason: "Lowercase letters, digits and hyphens only." },
    { test: n => /^[a-z]/.test(n), reason: "Starts with a letter." },
    { test: n => !n.endsWith("-") && !n.includes("--"), reason: "No hyphen at the end, or two together." },
    { test: n => !RESERVED.has(n), reason: "That one's reserved." },
]

export const username = {
    /** Why a name can't be had, or null when it can (taken names aside: only ArcLabs knows those). */
    problem(name: string): string | null {
        return RULES.find(rule => !rule.test(name))?.reason ?? null
    },
}
