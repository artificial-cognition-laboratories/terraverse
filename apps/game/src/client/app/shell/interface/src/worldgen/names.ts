// World names: a descriptive word and a place word, never one already taken.

const FIRST = [
    "Mossy", "Saltwind", "Amber", "Hollow", "Silver", "Ashen", "Cinder", "Frost", "Ember", "Willow",
    "Granite", "Misty", "Copper", "Thorn", "Raven", "Sunken", "Pale", "Wild", "Iron", "Golden",
    "Drift", "Bramble", "Cedar", "Storm", "Quiet", "Red", "Elder", "Glimmer", "Hazel", "Lonely",
] as const

const SECOND = [
    "Hollow", "Reach", "Vale", "Ridge", "Fen", "Shore", "Highlands", "Isles", "Basin", "Hearth",
    "Crossing", "Expanse", "Glen", "Moor", "Peaks", "Wilds", "Coast", "Steppe", "Downs", "Falls",
    "Haven", "Barrens", "Grove", "Sound", "Mesa", "Tundra", "Marsh", "Plateau", "Cove", "Wold",
] as const

const pick = <T>(list: readonly T[]) => list[Math.floor(Math.random() * list.length)]!

/** A fresh name not in `taken` (compared ignoring case). */
export function worldName(taken: Iterable<string>): string {
    const used = new Set([...taken].map((n) => n.toLowerCase()))
    for (let i = 0; i < 50; i++) {
        const first = pick(FIRST)
        const second = pick(SECOND.filter((s) => s !== first))
        const name = `${first} ${second}`
        if (!used.has(name.toLowerCase())) return name
    }
    // every try collided: number one instead
    const base = `${pick(FIRST)} ${pick(SECOND)}`
    let n = 2
    while (used.has(`${base} ${n}`.toLowerCase())) n++
    return `${base} ${n}`
}
