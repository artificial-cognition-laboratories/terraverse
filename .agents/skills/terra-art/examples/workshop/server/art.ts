// The workshop's pictures: every one drawn from code.

// sawn oak: the lighter end of the game's oak_log_top, close in value as the game's art is
const OAK = ["#a8743f", "#b5824a", "#c0905a", "#c99b63"] as const

/** Planks across the face: grain side to side, a seam every four rows. */
export const planks = art.texture({
    palette: { s: "#7a4f2a" },
    layers: [
        { grain: OAK, along: "x", seed: 3 },
        { rows: Array.from({ length: 16 }, (_, y) => (y % 4 === 3 ? "s".repeat(16) : ".".repeat(16))) },
    ],
})

export const benchTop = art.texture({
    palette: { s: "#7a4f2a", m: "#7c7f86", h: "#a9adb4", d: "#5a3a20" },
    layers: [
        { grain: OAK, along: "x", seed: 5 },
        {
            rows: [
                "dddddddddddddddd",
                "d..............d",
                "d..............d",
                "d.mmmm.........d",
                "d.mhhm.........d",
                "d.mmmm.........d",
                "d..m...........d",
                "ssssssssssssssss",
                "d..............d",
                "d..............d",
                "d.........hhh..d",
                "d........h..m..d",
                "d.........mm...d",
                "d..............d",
                "d..............d",
                "dddddddddddddddd",
            ],
        },
    ],
})

export const benchSide = art.texture({
    palette: { d: "#5a3a20", t: "#c0905a", l: "#8a5d34", g: "#6b4527" },
    layers: [
        { fill: "#3a2515" },
        {
            rows: [
                "tttttttttttttttt",
                "tttttttttttttttt",
                "dddddddddddddddd",
                "ll............ll",
                "ll............ll",
                "ll............ll",
                "llgggggggggggggl",
                "ll............ll",
                "ll............ll",
                "ll............ll",
                "ll............ll",
                "llgggggggggggggl",
                "ll............ll",
                "ll............ll",
                "ll............ll",
                "dd............dd",
            ],
        },
        { bevel: 0.12 },
    ],
})

export const stoneBrick = art.texture({
    palette: { m: "#86876b" },
    layers: [
        { noise: ["#a2a189", "#a9a790"], scale: 8, seed: 9 },
        {
            rows: [
                "mmmmmmmmmmmmmmmm",
                ".......m........",
                ".......m........",
                ".......m........",
                "mmmmmmmmmmmmmmmm",
                "...m.......m....",
                "...m.......m....",
                "...m.......m....",
                "mmmmmmmmmmmmmmmm",
                ".......m........",
                ".......m........",
                ".......m........",
                "mmmmmmmmmmmmmmmm",
                "...m.......m....",
                "...m.......m....",
                "...m.......m....",
            ],
        },
    ],
})

// bark: the game's oak_log ramp, streaked top to bottom
export const logSide = art.texture({ layers: [{ grain: ["#532a1c", "#572d20", "#6b3a22", "#6a3e23"], along: "y", seed: 11 }] })

export const logEnd = art.texture({
    palette: { b: "#623321", r: "#d8ba74", d: "#c0a15b", c: "#a8843f" },
    rows: [
        "bbbbbbbbbbbbbbbb",
        "bbbbrrrrrrrrbbbb",
        "bbrrddddddddrrbb",
        "brrddrrrrrrddrrb",
        "brddrrddddrrddrb",
        "brdrrddrrddrrdrb",
        "brdrddrrrrddrdrb",
        "brdrdrrccrrdrdrb",
        "brdrdrrccrrdrdrb",
        "brdrddrrrrddrdrb",
        "brdrrddrrddrrdrb",
        "brddrrddddrrddrb",
        "brrddrrrrrrddrrb",
        "bbrrddddddddrrbb",
        "bbbbrrrrrrrrbbbb",
        "bbbbbbbbbbbbbbbb",
    ],
})

// ── tools: a silhouette, lit from the top-left, outlined so it reads in the hand ──

const TOOL = { w: "#8a5d34", W: "#5e3b1f", s: "#8f8b84", S: "#5d5a55", f: "#3b3a44", F: "#24232b", b: "#c9b27c" }

export const stoneAxe = art.texture({
    palette: TOOL,
    layers: [
        {
            rows: [
                "................",
                "........SSs.....",
                ".......Ssss.....",
                "......Sssss.....",
                "......Ssssss....",
                "......Sssbbs....",
                ".......SSbb.....",
                ".........bw.....",
                "........wW......",
                ".......wW.......",
                "......wW........",
                ".....wW.........",
                "....wW..........",
                "...wW...........",
                "..wW............",
                "................",
            ],
        },
        { bevel: 0.2 },
        { outline: "#14110e" },
    ],
})

export const flintChisel = art.texture({
    palette: TOOL,
    layers: [
        {
            rows: [
                "................",
                "................",
                "............Ff..",
                "...........Ffff.",
                "..........Fff...",
                ".........bbF....",
                "........bwb.....",
                ".......wW.......",
                "......wW........",
                ".....wW.........",
                "....wW..........",
                "...wW...........",
                "................",
                "................",
                "................",
                "................",
            ],
        },
        { bevel: 0.25 },
        { outline: "#14110e" },
    ],
})

export const stonePick = art.texture({
    palette: TOOL,
    layers: [
        {
            rows: [
                "................",
                "......SSSs......",
                "....SSssssss....",
                "..SSs..bb..sss..",
                ".Ss....bw....ss.",
                ".S....wW......s.",
                "S.....wW........",
                "......wW........",
                ".....wW.........",
                "....wW..........",
                "...wW...........",
                "..wW............",
                ".wW.............",
                "................",
                "................",
                "................",
            ],
        },
        { bevel: 0.2 },
        { outline: "#14110e" },
    ],
})
