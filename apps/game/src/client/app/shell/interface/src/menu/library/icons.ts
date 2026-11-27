// The library's pixel art: its rail, and one icon per registry category.

const STEEL = "#c9d4dc", SHADE = "#6f7f8a", SKY = "#40b8ec", SUN = "#ffb040", EMBER = "#e0603d", GRASS = "#3c6e46", LEAF = "#4a9b64", EARTH = "#7a5232"

export type Page = "discover" | "public" | "packs" | "mods" | "worlds" | "shaders" | "account"
/** Where the library opens when asked from outside it: a player's page, or one of its own. */
export type LibraryOpen = { player: string } | { page: Page }
type Icon = { rows: readonly string[]; palette: Record<string, string> }

/** The library's pages: its rail's pictures, and the labels its tabs share. */
export const RAIL: readonly ({ id: Page; label: string; end?: boolean } & Icon)[] = [
    { id: "discover", label: "Discover", palette: { G: STEEL, L: SKY, O: SUN, S: SHADE },
        rows: ["...GGG...", ".GG...GG.", ".G...OO..", "G...OOS.G", "G..LLS..G", "G.LLL...G", ".GLL...G.", ".GG...GG.", "...GGG..."] },
    // Drawn as the wiki's rail is (wiki/WikiScreen.vue): the thing itself in its own colours, lit
    // from the top left (light, mid, dark), with no steel outline.
    // places to go: a rack of servers, its lights on
    { id: "public", label: "Public", palette: { D: "#2b3346", M: "#4a5674", G: "#5eba81", B: "#40b8ec", O: "#ffb040" },
        rows: ["DDDDDDDDD", "DMMMMMGMD", "DDDDDDDDD", "DMMMMMBMD", "DDDDDDDDD", "DMMMMMOMD", "DDDDDDDDD", ".D.....D.", "DD.....DD"] },
    // a game in a box: a crate, its latch gold
    { id: "packs", label: "Packs", palette: { T: "#c9a46a", L: "#8a5d34", D: "#5e3b1f", O: "#ffd060" },
        rows: [".........", ".TTTTTTT.", "TTTTTTTTT", "LLLLOLLLD", "DDDDODDDD", "LLLLLLLLD", "LLLLLLLLD", "LLLLLLLLD", "DDDDDDDDD"] },
    // a block that does something: the wiki's block, in the accent's blue
    { id: "mods", label: "Mods", palette: { T: "#9fd4ff", L: "#3a8ee0", D: "#1f5aa8" },
        rows: ["....T....", "..TTTTT..", "TTTTTTTTT", "LTTTTTTTD", "LLLTTTDDD", "LLLLLDDDD", "LLLLLDDDD", "..LLLDD..", "....L...."] },
    // a planet: oceans, land and a cap of ice, its night side darker
    { id: "worlds", label: "Worlds", palette: { W: "#e6eef5", O: "#3f76e4", o: "#24479a", G: "#5eba81", g: "#3c8a55" },
        rows: ["...WWW...", ".OGGOOOo.", "OGGGGOOoo", "OOGGgOOoo", "OOOGOOGgo", "OOOOOGGgo", ".OOOOOgo.", "..ooooo..", "........."] },
    // light on the world: a sunset over hills
    { id: "shaders", label: "Shaders", palette: { P: "#6b4fb8", R: "#e0603d", O: "#ffb040", Y: "#ffe08a", G: "#3c6e46", H: "#24452e" },
        rows: ["PPPPPPPPP", "PPPPPPPPP", "PPPOOOPPP", "PPOOYOOPP", "ROOYYYOOR", "RRRRRRRRR", "GGRRRRGGG", "GGGGGGGGG", "HHHHHHHHH"] },
    // your account, at the rail's foot by the way back: you, as the figure is drawn
    { id: "account", label: "Account", end: true, palette: { H: "#5e3b1f", S: "#d8a07a", E: "#2b3346", B: "#0094d2", D: "#006a99" },
        rows: ["...HHH...", "..HHHHH..", "..SSSSS..", "..SESES..", "..SSSSS..", "...SSS...", ".BBBBBBB.", "BBBBBBBDD", "BBBBBBDDD"] },
]

/** What the bar's search looks through, page by page. */
export const SEARCHES: Record<Page, string> = {
    discover: "Search everything…",
    public: "Search servers, hosts…",
    packs: "Search packs…",
    mods: "Search mods, authors…",
    worlds: "Search worlds and seeds…",
    shaders: "Search shaders and textures…",
    account: "Search everything…",
}

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
