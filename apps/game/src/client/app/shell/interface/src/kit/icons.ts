// The menus' pixel icons, 9×9, drawn in the same hand as the library's categories
// (menu/library/icons.ts) and the website's: steel, shade, and one colour that means it.

export type Icon = { rows: readonly string[]; palette: Record<string, string> }

const STEEL = "#c9d4dc", SHADE = "#6f7f8a", SKY = "#63a3e9", SUN = "#ffb040", LEAF = "#5eba81", EMBER = "#f85149", EARTH = "#7a5232"

export const ICONS = {
    play: { palette: { G: STEEL, L: SKY, E: EARTH, T: LEAF },
        rows: ["...LLL...", ".LLTTLL..", "LLTTTLLL.", "LTTETTLLL", "LLTTTTLLL", "LLLTTLLTL", ".LLLLLTT.", "..LLLLL..", "........."] },
    library: { palette: { G: STEEL, D: SHADE, L: SKY },
        rows: [".........", "GG.LL.DD.", "GG.LL.DD.", "GG.LL.DD.", "GG.LL..DD", "GG.LL..DD", "GG.LL...D", "GGGGGGGGG", "........."] },
    wiki: { palette: { G: STEEL, D: SHADE, L: SKY },
        rows: [".........", "GGGG.GGGG", "GDDG.GLLG", "GGGG.GGGG", "GDDG.GLLG", "GGGG.GGGG", "GDDG.GLLG", "GGGGGGGGG", "....G...."] },
    settings: { palette: { G: STEEL, D: SHADE },
        rows: ["...G.G...", "..GGGGG..", ".GGDDDGG.", "GGD...DGG", ".GD...DG.", "GGD...DGG", ".GGDDDGG.", "..GGGGG..", "...G.G..."] },
    profile: { palette: { G: STEEL, L: SKY },
        rows: ["...GGG...", "..G...G..", "..G...G..", "..G...G..", "...GGG...", ".........", ".GGGGGGG.", "G.......G", "GLLLLLLLG"] },
    servers: { palette: { G: STEEL, D: SHADE, L: SKY, T: LEAF },
        rows: ["GGGGGGGGG", "GDDDDDLDG", "GGGGGGGGG", "GDDDDDTDG", "GGGGGGGGG", "GDDDDDLDG", "GGGGGGGGG", "...G.G...", "..GG.GG.."] },
    star: { palette: { O: SUN },
        rows: ["....O....", "....O....", "...OOO...", "OOOOOOOOO", ".OOOOOOO.", "..OOOOO..", "..OO.OO..", ".OO...OO.", "........."] },
    download: { palette: { G: STEEL, L: SKY },
        rows: ["....L....", "....L....", "....L....", "..L.L.L..", "...LLL...", "....L....", ".........", "G.......G", "GGGGGGGGG"] },
    quit: { palette: { G: STEEL, R: EMBER },
        rows: ["GGGGG....", "G...G....", "G...G..R.", "G...GRRRR", "G...G..R.", "G...G....", "G...G....", "GGGGG....", "........."] },
    all: { palette: { G: STEEL, L: SKY },
        rows: [".........", ".GGG.LLL.", ".GGG.LLL.", ".GGG.LLL.", ".........", ".LLL.GGG.", ".LLL.GGG.", ".LLL.GGG.", "........."] },
    mods: { palette: { G: STEEL, D: SHADE, L: SKY },
        rows: ["....G....", "..GGLGG..", "GGLLLLLGG", "GDGLLLGDG", "GDDDGDDDG", "GDDDGDDDG", "GDDDGDDDG", "..GDGDG..", "....G...."] },
    textures: { palette: { G: STEEL, L: SKY, T: LEAF, E: EARTH },
        rows: ["GGGGGGGGG", "GLLLGTTTG", "GLLLGTTTG", "GLLLGTTTG", "GGGGGGGGG", "GEEEGLLLG", "GEEEGLLLG", "GEEEGLLLG", "GGGGGGGGG"] },
    players: { palette: { G: STEEL, L: SKY },
        rows: ["...LLL...", "..LLLLL..", "..LLLLL..", "...LLL...", ".........", ".LLLLLLL.", "LLLLLLLLL", "LLLLLLLLL", "........."] },
    region: { palette: { G: LEAF, L: SKY, E: EARTH },
        rows: ["...LLL...", ".LLGGLL..", "LLGGGLLL.", "LGGEGGLLL", "LLGGGGLLL", "LLLGGLLGL", ".LLLLLGG.", "..LLLLL..", "........."] },
    heart: { palette: { R: EMBER },
        rows: [".........", ".RR...RR.", "RRRR.RRRR", "RRRRRRRRR", "RRRRRRRRR", ".RRRRRRR.", "..RRRRR..", "...RRR...", "....R...."] },
    // a mod page's side nav (the website's config/icons.ts)
    readme: { palette: { G: STEEL, L: SKY },
        rows: [".........", "GGGG.GGGG", "G..LGL..G", "G.LLGLL.G", "G..LGL..G", "G.LLGLL.G", "G...G...G", "GGGG.GGGG", "........."] },
    versions: { palette: { G: STEEL, L: SKY },
        rows: ["GGGGG....", "G...GG...", "G.L..GG..", "G.....GG.", "G......GG", "GG.....G.", ".GG...G..", "..GG.G...", "...GG...."] },
    dependencies: { palette: { G: STEEL, L: SKY },
        rows: ["GGG......", "G.G......", "GGG......", ".L.......", ".LLLLLGGG", ".L....G.G", "GGG...GGG", "G.G......", "GGG......"] },
    source: { palette: { G: STEEL, L: SKY },
        rows: [".........", "GGG......", "G..GGGGG.", "G.......G", "G.LLLLL.G", "G.......G", "G.LLL...G", "GGGGGGGGG", "........."] },
    blocks: { palette: { G: STEEL, D: SHADE, L: SKY },
        rows: [".........", ".GGGGGGG.", ".GLLLLLG.", ".GLLLLLG.", ".GGGGGGG.", ".GDDDDDG.", ".GDDDDDG.", ".GGGGGGG.", "........."] },
    back: { palette: { G: STEEL },
        rows: [".........", "...G.....", "..GG.....", ".GGGGGGG.", "GGGGGGGG.", ".GGGGGGG.", "..GG.....", "...G.....", "........."] },
} satisfies Record<string, Icon>

export type IconId = keyof typeof ICONS
