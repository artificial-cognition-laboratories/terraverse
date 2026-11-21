import type { Mode } from "./rules.ts"

// What a new world is played as: the server package it starts from. Survival and
// Creative are the game's own; installed servers will list beside them once the
// registry carries the server package type. TEMP(2026-11-21): a table of two, until then.

export type Experience = {
    id: Mode; title: string; by: string; description: string
    /** The mods it brings: shown on the Mods page as part of the world, not removable. */
    mods: readonly string[]
}

export const EXPERIENCES: readonly Experience[] = [
    { id: "survival", title: "Survival", by: "terraverse", mods: ["@terraverse/materials", "@terraverse/commands"],
        description: "Gather, build and stay alive on a real planet. Tune how hard it is below." },
    { id: "creative", title: "Creative", by: "terraverse", mods: ["@terraverse/materials", "@terraverse/commands"],
        description: "Every block, flight, and nothing that can hurt you. Build freely." },
]

export function experience(id: Mode): Experience {
    const found = EXPERIENCES.find((e) => e.id === id)
    if (!found) throw new Error(`no experience ${id}`)
    return found
}
