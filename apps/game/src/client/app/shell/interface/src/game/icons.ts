// Pixel maps for the HUD's built-in icons (see PixelIcon.vue).

export const HEART = [
    ".KK...KK.",
    "KRRK.KRRK",
    "KRWRKRRRK",
    "KRRRRRRRK",
    ".KRRRRRK.",
    "..KRRRK..",
    "...KRK...",
    "....K....",
] as const

export const BUBBLE = [
    "..KKKKK..",
    ".KBBBBBK.",
    "KBWWBBBBK",
    "KBWBBBBBK",
    "KBBBBBBBK",
    "KBBBBBBBK",
    ".KBBBBBK.",
    "..KKKKK..",
] as const

const OUTLINE = "#1a0b08"

export const PALETTES = {
    heart: {
        full: { K: OUTLINE, R: "#e3312a", W: "#ff9e8f" },
        empty: { K: OUTLINE, R: "#3d1612", W: "#3d1612" },
    },
    bubble: {
        full: { K: "#0d1a2e", B: "#4f8fe8", W: "#d6e8ff" },
        empty: { K: "#0d1a2e", B: "transparent", W: "transparent" },
    },
} as const

/** The left half of a map, for half-full icons. */
export function leftHalf(rows: readonly string[]) {
    return rows.map((row) => row.slice(0, Math.ceil(row.length / 2)))
}
