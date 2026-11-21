// The registry's category icons, as the game draws them
// (apps/game/.../interface/src/menu/community/icons.ts). TEMP(2026-11-21): a copy
// until both apps import one package; change both together until then.

const STEEL = "#c9d4dc", SHADE = "#6f7f8a", SKY = "#40b8ec", SUN = "#ffb040", EMBER = "#e0603d", GRASS = "#3c6e46", LEAF = "#4a9b64", EARTH = "#7a5232"

export type PixelIcon = { rows: readonly string[]; palette: Record<string, string> }

export const CATEGORY_ICONS = {
    games: { palette: { R: EMBER, O: SUN },
        rows: ["....R....", "...RRR...", "..R.O.R..", ".R.OOO.R.", "R.OOOOO.R", ".R.OOO.R.", "..R.O.R..", "...RRR...", "....R...."] },
    survival: { palette: { O: SUN, R: EMBER, E: EARTH },
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
    servers: { palette: { G: STEEL, D: SHADE, L: SKY, T: GRASS },
        rows: ["GGGGGGGGG", "GDDDDDLDG", "GGGGGGGGG", "GDDDDDTDG", "GGGGGGGGG", "GDDDDDLDG", "GGGGGGGGG", "...G.G...", "..GG.GG.."] },
} satisfies Record<string, PixelIcon>

export type CategoryId = keyof typeof CATEGORY_ICONS

/** The landing's own icons, drawn in the same hand. */
const LANDING_ICONS = {
    sky: { palette: { G: STEEL, L: SKY, O: SUN },
        rows: [".......O.", "...LLL...", "..LLLLL.G", ".LLLLLLG.", "GGGGGGG..", ".LLLLL...", "..LLL....", "G........", "........."] },
    open: { palette: { G: STEEL, D: SHADE, L: SKY },
        rows: ["...GGG...", "..G...G..", "..G......", "..G......", ".LLLLLLL.", ".LLLDLLL.", ".LLLDLLL.", ".LLLLLLL.", "........."] },
} satisfies Record<string, PixelIcon>

/** Every icon the landing can draw, by name. */
export const ICONS = { ...CATEGORY_ICONS, ...LANDING_ICONS }
export type IconId = keyof typeof ICONS
