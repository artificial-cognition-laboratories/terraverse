/**
 * A mass for a slot or a label, metric, as short as it reads: "350g",
 * "4.5kg", "45kg", "1.2t". `spaced` puts a space before the unit, for text.
 */
export function formatMass(kg: number, spaced = false): string {
    const gap = spaced ? " " : ""
    if (kg < 1) return `${Math.max(1, Math.round(kg * 1000))}${gap}g`
    if (kg < 10) return `${Math.round(kg * 10) / 10}${gap}kg`
    if (kg < 1000) return `${Math.round(kg)}${gap}kg`
    return `${Math.round(kg / 100) / 10}${gap}t`
}
