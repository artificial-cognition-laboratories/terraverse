// The solar model: where the sun is for a given latitude and date, and how
// much warmth arrives. Small on purpose — terragen and the engine's sky will
// carry the same formulas (see AGENTS.md: The sun).
//
// date: 0..1 through the year, 0 = northern spring equinox, 0.25 = northern
// midsummer. latitude: radians, positive north.

/** The sun's declination: the latitude it stands overhead at noon. */
export function declination(tiltDegrees: number, date: number) {
    return (tiltDegrees * Math.PI) / 180 * Math.sin(2 * Math.PI * date)
}

/** Hours of daylight as a share of the day (0 = polar night, 1 = midnight sun). */
export function daylight(latitude: number, decl: number) {
    const x = -Math.tan(latitude) * Math.tan(decl)
    if (x <= -1) return 1
    if (x >= 1) return 0
    return Math.acos(x) / Math.PI
}

/** Mean sunlight over a day, relative to the equator at equinox (≈1). */
export function insolation(latitude: number, decl: number) {
    const x = -Math.tan(latitude) * Math.tan(decl)
    const h = x <= -1 ? Math.PI : x >= 1 ? 0 : Math.acos(x) // half the day's arc
    return h * Math.sin(latitude) * Math.sin(decl) + Math.cos(latitude) * Math.cos(decl) * Math.sin(h)
}

export const SEASONS = ["spring", "summer", "autumn", "winter"] as const

/** The northern season name for a date. */
export function season(date: number) {
    return SEASONS[Math.floor((((date % 1) + 1) % 1) * 4)]!
}
