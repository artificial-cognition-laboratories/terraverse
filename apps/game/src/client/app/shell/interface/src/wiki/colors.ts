// How the wiki draws substances: a colour for the common ones (what they
// look like), the first element's colour for the rest.

import type { MatterElement, MatterSubstance } from "../bridge"

const LOOKS: Record<string, string> = {
    water: "#3d7fd6", wood: "#8a5a32", carbon: "#2a2a2c", iron: "#8c8f94", copper: "#c8743c", tin: "#c4c8cc",
    gold: "#e6c048", lead: "#6d7178", aluminium: "#c9ccd2", zinc: "#a9b0b8", bronze: "#b4823c", cast_iron: "#5f6266",
    steel: "#9aa0a8", pewter: "#a6a9ad", glass: "#bfe3ea", silica: "#e6dcc4", salt: "#f2f2f2", sulfur: "#e8d03a",
    lime: "#f0eee6", slaked_lime: "#ecebe2", calcite: "#e8e2d0", soda_ash: "#f4f4f0", hematite: "#8a3a2a",
    magnetite: "#3a3a3e", malachite: "#2e8a5a", cassiterite: "#5a4a3e", galena: "#6a6e78", glucose: "#f6f2e6",
    kaolinite: "#d8cdb8", metakaolin: "#c9a98a", mercury: "#b8bcc4", sulfuric_acid: "#d8d4a0",
}

export function colorOf(s: MatterSubstance, elements: Map<string, MatterElement>): string {
    const look = LOOKS[s.id]
    if (look) return look
    const first = Object.keys(s.composition)[0]
    const e = first ? elements.get(first) : undefined
    if (!e) throw new Error(`no colour for ${s.id}: its first element isn't in the table`)
    return e.color
}

/** The glow of something this hot (K), as an RGB triple 0..1 and a strength 0..1: none below ~750 K. */
export function glow(t: number): { color: [number, number, number]; strength: number } {
    const strength = Math.max(0, Math.min(1, (t - 750) / 1250))
    const g = Math.max(0, Math.min(1, (t - 900) / 1200)), b = Math.max(0, Math.min(1, (t - 1600) / 1500))
    return { color: [1, 0.25 + 0.75 * g, 0.05 + 0.9 * b], strength: strength }
}

/** A temperature as a colour for the heat overlay: blue cold, white at room temperature, red hot. */
export function heatColor(t: number): [number, number, number] {
    const x = Math.max(-1, Math.min(1, (t - 293) / (t > 293 ? 900 : 120)))
    return x >= 0 ? [1, 1 - 0.8 * x, 1 - 0.95 * x] : [1 + 0.7 * x, 1 + 0.4 * x, 1]
}
