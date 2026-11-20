/**
 * Reference conditions to compare a substance against on the phase diagram.
 * Known numbers, not physics: where water is liquid on Earth, where methane
 * is liquid on Titan. Later these come from the system's own body cards.
 */
export const ENVIRONMENTS = [
    { label: "Earth", t: 288, p: 101325 },
    { label: "Mars", t: 210, p: 610 },
    { label: "Venus", t: 737, p: 9.2e6 },
    { label: "Titan", t: 94, p: 1.47e5 },
    { label: "Moon by day", t: 390, p: 1e-7 },
    { label: "Jupiter's clouds", t: 165, p: 1e5 },
    { label: "Campfire", t: 900, p: 101325 },
    { label: "Blast furnace", t: 1900, p: 101325 },
] as const satisfies readonly { label: string; t: number; p: number }[]
