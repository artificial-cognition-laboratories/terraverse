import { GEAR } from "@game/menu/worldIcons.ts"

// The launcher's pixel icons (see PixelIcon.vue), in the menu's palette.
export type Icon = { rows: readonly string[]; palette: Record<string, string> }

const STEEL = "#c9d4dc", SKY = "#40b8ec", DEEP = "#1f6f9c", GRASS = "#5fae5a"

export const PLANET: Icon = { palette: { S: SKY, G: GRASS, D: DEEP },
    rows: ["...SSS...", ".SSGGSSS.", ".SGGGGSS.", "SSSGGSSDS", "SSSSSSGDS", "SGGSSGGDS", ".SGGSSDD.", ".SSSDDDD.", "...DDD..."] }

/** Builds stacked: one per version. */
export const STACK: Icon = { palette: { A: STEEL, B: SKY },
    rows: [".BBBBBBB.", "BBBBBBBBB", ".........", ".AAAAAAA.", "AAAAAAAAA", ".........", ".AAAAAAA.", "AAAAAAAAA", "........."] }

export { GEAR }
