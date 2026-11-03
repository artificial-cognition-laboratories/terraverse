import { reactive } from "vue"

/**
 * Stars by spectral class, coolest first as the world code stores them (must
 * match src/terragen/code.cpp STARS).
 */
export const STAR_CLASSES = [
    { id: "M", label: "M · red dwarf", mass: { least: 0.08, usual: 0.3, most: 0.45 }, kelvin: "under 3,700 K", color: [255, 128, 82] },
    { id: "K", label: "K · orange", mass: { least: 0.45, usual: 0.75, most: 0.8 }, kelvin: "3,700–5,200 K", color: [255, 190, 120] },
    { id: "G", label: "G · yellow, like our sun", mass: { least: 0.8, usual: 1, most: 1.04 }, kelvin: "5,200–6,000 K", color: [255, 244, 220] },
    { id: "F", label: "F · yellow-white", mass: { least: 1.04, usual: 1.2, most: 1.4 }, kelvin: "6,000–7,500 K", color: [248, 247, 255] },
    { id: "A", label: "A · white", mass: { least: 1.4, usual: 1.7, most: 2.1 }, kelvin: "7,500–10,000 K", color: [202, 215, 255] },
    { id: "B", label: "B · blue-white", mass: { least: 2.1, usual: 5, most: 16 }, kelvin: "10,000–30,000 K", color: [170, 191, 255] },
    { id: "O", label: "O · blue giant", mass: { least: 16, usual: 30, most: 60 }, kelvin: "over 30,000 K", color: [155, 176, 255] },
] as const satisfies readonly { id: string; label: string; mass: Span; kelvin: string; color: readonly [number, number, number] }[]

export type StarClass = (typeof STAR_CLASSES)[number]["id"]

export function starClass(id: StarClass) {
    const found = STAR_CLASSES.find((c) => c.id === id)
    if (!found) throw new Error(`unknown star class ${id}`)
    return found
}

/** Billions of years it shines, as src/terragen/code.cpp lifetime(): heavy stars burn out fast. */
export const lifetime = (mass: number) => Math.max(0.003, 10 * mass ** -2.5)

/** A range over orders of magnitude, stored as a byte whose middle is exactly `usual`. */
export type Span = { least: number; usual: number; most: number }

export function spanned(span: Span, byte: number) {
    if (byte === 128) return span.usual
    const below = byte < 128
    const t = below ? (128 - byte) / 128 : (byte - 128) / 127
    return span.usual * ((below ? span.least : span.most) / span.usual) ** t
}

export function spannedByte(span: Span, value: number) {
    const v = Math.min(span.most, Math.max(span.least, value))
    if (v === span.usual) return 128
    return v < span.usual
        ? 128 - Math.round(128 * Math.log(v / span.usual) / Math.log(span.least / span.usual))
        : 128 + Math.round(127 * Math.log(v / span.usual) / Math.log(span.most / span.usual))
}

/** The bounds on where planets stay on stable orbits, in AU. */
export const SEPARATIONS = {
    close: { min: 0.02, max: 1, note: (s: number) => `Planets circle both suns, from about ${(s * 3).toFixed(2)} AU out.` },
    wide: { min: 30, max: 2000, note: (s: number) => `Planets circle the main sun, within about ${Math.round(s / 3)} AU. The other is a brilliant star in the sky.` },
} as const
export type CompanionKind = "none" | keyof typeof SEPARATIONS
/** A code's bytes for it: kind (none is no bytes), class, mass, separation; as src/terragen/code.cpp decode. */
export const COMPANION_KINDS = ["none", "close", "wide"] as const
export const separationByte = (kind: keyof typeof SEPARATIONS, s: number) => {
    const { min, max } = SEPARATIONS[kind]
    return Math.round(255 * Math.log(s / min) / Math.log(max / min))
}
export const separationOf = (kind: keyof typeof SEPARATIONS, byte: number) => {
    const { min, max } = SEPARATIONS[kind]
    return min * (max / min) ** (byte / 255)
}

export type CompanionWire = { kind: CompanionKind; class: StarClass; mass: number; separation: number }
/** The second star (src/terragen/code.h Companion). */
export function Companion(): CompanionWire {
    return reactive({ kind: "none", class: "K", mass: 0.75, separation: 0.2 })
}
