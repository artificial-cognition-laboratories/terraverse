// The online screen's pixel art: its rail.
import { ICONS } from "../kit/icons.ts"

const STEEL = "#c9d4dc", SHADE = "#6f7f8a", SKY = "#40b8ec", LEAF = "#4a9b64", EARTH = "#7a5232"

export type Page = "discover" | "browse" | "friends" | "you"
type Icon = { rows: readonly string[]; palette: Record<string, string> }

/** The online screen's pages: its rail's pictures, and the labels its bar shares. */
export const RAIL: readonly ({ id: Page; label: string } & Icon)[] = [
    // a planet, lit: worlds to go to
    { id: "discover", label: "Discover", palette: { G: STEEL, L: SKY, T: LEAF, E: EARTH },
        rows: ["...GGG...", ".GLLTTG..", "GLLTTTLG.", "GLTTELLLG", "GLLTLLTLG", "GLLLLTTLG", ".GLLTTLG.", "..GGGGG..", "........."] },
    { id: "browse", label: "Servers", ...ICONS.servers },
    { id: "friends", label: "Friends", palette: { G: STEEL, D: SHADE, L: SKY },
        rows: ["..GG.....", ".GGGG.DD.", ".GGGGDDDD", "..GG.DDDD", ".....D.DD", "GGGGG....", "GLLLGDDD.", "GLLLGDDDD", "GGGGGDDDD"] },
    // your own player page: the same page any player has, as the site's registry/@handle
    { id: "you", label: "You", ...ICONS.profile },
]
