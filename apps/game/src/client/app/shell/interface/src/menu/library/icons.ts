// The library's pixel art: its rail, and one icon per registry category.

const STEEL = "#c9d4dc", SHADE = "#6f7f8a", SKY = "#40b8ec", SUN = "#ffb040", EMBER = "#e0603d", GRASS = "#3c6e46", LEAF = "#4a9b64", EARTH = "#7a5232"

export type Page = "discover" | "mods" | "servers"
type Icon = { rows: readonly string[]; palette: Record<string, string> }

/** The library's pages: its rail's pictures, and the labels its tabs share. */
export const RAIL: readonly ({ id: Page; label: string } & Icon)[] = [
    { id: "discover", label: "Discover", palette: { G: STEEL, L: SKY, O: SUN, S: SHADE },
        rows: ["...GGG...", ".GG...GG.", ".G...OO..", "G...OOS.G", "G..LLS..G", "G.LLL...G", ".GLL...G.", ".GG...GG.", "...GGG..."] },
    { id: "mods", label: "Browse", palette: { G: STEEL, D: SHADE, L: SKY },
        rows: ["....G....", "..GGLGG..", "GGLLLLLGG", "GDGLLLGDG", "GDDDGDDDG", "GDDDGDDDG", "GDDDGDDDG", "..GDGDG..", "....G...."] },
    { id: "servers", label: "Servers", palette: { G: STEEL, D: SHADE, L: SKY, T: GRASS },
        rows: ["GGGGGGGGG", "GDDDDDLDG", "GGGGGGGGG", "GDDDDDTDG", "GGGGGGGGG", "GDDDDDLDG", "GGGGGGGGG", "...G.G...", "..GG.GG.."] },
]

/**
 * The categories as the website lists them, shown on Discover while the registry keeps none of
 * its own (shell/bridge/registry.cpp: categories are always empty, and a search by one is refused).
 */
export const CATEGORIES: readonly { id: string; label: string }[] = [
    { id: "games", label: "Games" },
    { id: "survival", label: "Survival" },
    { id: "building", label: "Building" },
    { id: "machines", label: "Machines" },
    { id: "matter", label: "Matter" },
    { id: "world", label: "Worlds" },
    { id: "textures", label: "Shaders & textures" },
    { id: "library", label: "Libraries" },
]

/** What the registry's categories look like; a category with no entry here is drawn as `other`. */
export const CATEGORY_ICONS: Record<string, Icon> = {
    games: { palette: { G: STEEL, R: EMBER, O: SUN },
        rows: ["....R....", "...RRR...", "..R.O.R..", ".R.OOO.R.", "R.OOOOO.R", ".R.OOO.R.", "..R.O.R..", "...RRR...", "....R...."] },
    survival: { palette: { G: STEEL, O: SUN, R: EMBER, E: EARTH },
        rows: ["....O....", "...OOR...", "...ORR...", "..ORRRO..", "..RRORR..", ".RRROORR.", "EEEEEEEEE", ".E.E.E.E.", "........."] },
    building: { palette: { G: STEEL, D: SHADE },
        rows: ["GGGGDGGGG", "GGGGDGGGG", "DDDDDDDDD", "GGDGGGGDG", "GGDGGGGDG", "DDDDDDDDD", "GGGGDGGGG", "GGGGDGGGG", "DDDDDDDDD"] },
    machines: { palette: { G: STEEL, D: SHADE },
        rows: ["...G.G...", "..GGGGG..", ".GGDDDGG.", "GGD...DGG", ".GD...DG.", "GGD...DGG", ".GGDDDGG.", "..GGGGG..", "...G.G..."] },
    matter: { palette: { G: STEEL, L: SKY, O: SUN },
        rows: ["...GGG...", "...G.G...", "...G.G...", "..G...G..", ".G.LLL.G.", "G.LLOLL.G", "G.LLLLL.G", ".G.....G.", "..GGGGG.."] },
    world: { palette: { G: LEAF, L: SKY, E: EARTH },
        rows: ["...LLL...", ".LLGGLL..", "LLGGGLLL.", "LGGEGGLLL", "LLGGGGLLL", "LLLGGLLGL", ".LLLLLGG.", "..LLLLL..", "........."] },
    textures: { palette: { G: STEEL, L: SKY, T: LEAF, E: EARTH },
        rows: ["GGGGGGGGG", "GLLLGTTTG", "GLLLGTTTG", "GLLLGTTTG", "GGGGGGGGG", "GEEEGLLLG", "GEEEGLLLG", "GEEEGLLLG", "GGGGGGGGG"] },
    library: { palette: { G: STEEL, D: SHADE, L: SKY },
        rows: [".........", "GG.LL.DD.", "GG.LL.DD.", "GG.LL.DD.", "GG.LL..DD", "GG.LL..DD", "GG.LL...D", "GGGGGGGGG", "........."] },
    other: { palette: { G: STEEL, D: SHADE },
        rows: [".........", ".GGGGGGG.", ".G.....G.", ".G.DDD.G.", ".G.DDD.G.", ".G.DDD.G.", ".G.....G.", ".GGGGGGG.", "........."] },
}
