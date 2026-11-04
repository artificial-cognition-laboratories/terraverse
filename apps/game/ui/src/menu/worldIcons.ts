import type { World } from "../bridge/index.ts"

// The Play page's pixel icons, and what a world's row says with them.
export type Section = "mine" | "friends"
type Icon = { rows: readonly string[]; palette: Record<string, string> }
export type Chip = Icon & { label: string; text: string }

const STEEL = "#c9d4dc", SKY = "#40b8ec", GRASS = "#5fae5a", SUN = "#f2c14e", EMBER = "#e0694a", SHADE = "#7c8a94", SNOW = "#e8f2f8"

export const RAIL: ({ id: Section; label: string; shortLabel: string } & Icon)[] = [
    { id: "mine", label: "My worlds", shortLabel: "Mine", palette: { S: SKY, G: GRASS, D: "#1f6f9c" },
        rows: ["...SSS...", ".SSGGSSS.", ".SGGGGSS.", "SSSGGSSDS", "SSSSSSGDS", "SGGSSGGDS", ".SGGSSDD.", ".SSSDDDD.", "...DDD..."] },
    { id: "friends", label: "Friends", shortLabel: "Friends", palette: { A: STEEL, B: SKY },
        rows: [".........", "..AA..BB.", ".AAAABBBB", ".AAAABBBB", "..AA..BB.", ".........", "AAAAABBBB", "AAAAABBBB", "AAAAABBBB"] },
]
export const GEAR: Icon = { palette: { G: STEEL },
    rows: ["...GGG...", ".G.GGG.G.", "..GGGGG..", "GGGG.GGGG", "GGG...GGG", "GGGG.GGGG", "..GGGGG..", ".G.GGG.G.", "...GGG..."] }
export const PIN: Icon = { palette: { P: SKY },
    rows: ["..PPPPP..", "...PPP...", "...PPP...", "...PPP...", ".PPPPPPP.", "....P....", "....P....", "....P....", "....P...."] }

const MODE: Record<"survival" | "creative", Chip> = {
    survival: { label: "Survival", text: "Survival", palette: { H: EMBER },
        rows: [".HH.HH.", "HHHHHHH", "HHHHHHH", "HHHHHHH", ".HHHHH.", "..HHH..", "...H..."] },
    creative: { label: "Creative", text: "Creative", palette: { S: SUN },
        rows: ["...S...", "...S...", "..SSS..", "SSSSSSS", "..SSS..", "...S...", "...S..."] },
}
// (a ladder's rungs: how hard, the more the harder)
const rungs = (n: number): readonly string[] => ["......."].concat(
    ...[3, 2, 1, 0].map((row) => [row < n ? "LLLLLLL" : ".DDDDD.", "......."])).slice(0, 7)
const DIFFICULTY: Record<string, number> = { peaceful: 1, easy: 2, normal: 3, hard: 4 }
const SEASONS: Chip = { label: "Seasons", text: "", palette: { S: SNOW },
    rows: ["...S...", "S..S..S", ".S.S.S.", "..SSS..", ".S.S.S.", "S..S..S", "...S..."] }
const WILD: Chip = { label: "Wild weather", text: "", palette: { C: STEEL, B: SUN },
    rows: ["..CCC..", ".CCCCC.", "CCCCCCC", "...BB..", "..BB...", ".BBB...", "..B...."] }
const MODS: Icon = { palette: { M: SKY },
    rows: ["MMM.MMM", "MMM.MMM", "MMM.MMM", ".......", "MMM.MMM", "MMM.MMM", "MMM.MMM"] }

/** A world's row: always what kind of game it is; the rest only when it's out of the ordinary. */
export function chipsOf(world: World, mods: number): Chip[] {
    const chips: Chip[] = []
    if (world.mode) chips.push(MODE[world.mode])
    const difficulty = world.rules.tf_difficulty
    if (world.mode !== "creative" && difficulty && DIFFICULTY[difficulty] !== undefined)
        chips.push({ label: `Difficulty: ${difficulty}`, text: difficulty, palette: { L: SKY, D: SHADE }, rows: rungs(DIFFICULTY[difficulty]) })
    if (world.rules.tf_seasons === "true") chips.push(SEASONS)
    if (world.rules.tf_weather === "wild") chips.push(WILD)
    if (mods > 0) chips.push({ ...MODS, label: `${mods} added mods`, text: `${mods}` })
    return chips
}

export function sizeLabel(size: number): string {
    return `${Math.round(size / (2 * Math.PI) / 1000)}k`
}

const GLOBE = ["...OOO...", ".OOLLLOO.", ".OLLOOLO.", "OOLOOOOOD", "OOOOLLODD", "OLLOOLLDD", ".OLOOODD.", ".OODDDDD.", "...DDD..."]
const TINTS = [{ O: "#2f7fb5", L: "#5fae5a", D: "#16405c" }, { O: "#b5653a", L: "#d9a066", D: "#5c2f16" },
    { O: "#3a9c8f", L: "#a8c76a", D: "#164c45" }, { O: "#7a6bb5", L: "#c2b5e8", D: "#352c5c" }, { O: "#a89c7a", L: "#e0d6b0", D: "#4c4632" }]
/** Stands in for a picture of the planet (not one of it yet): the same tint for the same world, to tell rows apart. */
export function planetIcon(key: string): Icon {
    let h = 0
    for (const c of key) h = (h * 31 + c.charCodeAt(0)) >>> 0
    return { rows: GLOBE, palette: TINTS[h % TINTS.length] }
}
