// 9×9 pixel pictograms for the overlay tiles (drawn by PixelIcon).

import type { OverlayId } from "./fields"

type Icon = { rows: readonly string[]; palette: Record<string, string> }

export const OVERLAY_ICONS: Record<OverlayId, Icon> = {
    terrain: {
        rows: ["WWWWGGWWW", "WWWGGGGWW", "WWGGGLGWW", "WWGLLLGGW", "WGGLRLGGW", "WWGGLGGWW", "WWWGGGWWW", "WWWWGWWWW", "WWWWWWWWW"],
        palette: { W: "#2c5f96", G: "#5b9a45", L: "#86b85a", R: "#b8b0a4" },
    },
    biomes: {
        rows: ["AAABBBCCC", "AAABBBCCC", "AAABBBCCC", "DDDEEEFFF", "DDDEEEFFF", "DDDEEEFFF", "GGGHHHIII", "GGGHHHIII", "GGGHHHIII"],
        palette: { A: "#ecf0f4", B: "#3c6450", C: "#968c8c", D: "#34743a", E: "#68a048", F: "#b0aa54", G: "#1c783c", H: "#dec076", I: "#1e5490" },
    },
    elevation: {
        rows: [".........", "....W....", "...WWS...", "..WSSSS..", "..SSSSSS.", ".SSSDSSS.", "SSSDDDSSS", "SDDDDDDDS", "DDDDDDDDD"],
        palette: { W: "#f5f5f5", S: "#9a8f80", D: "#5d7a3e" },
    },
    temperature: {
        rows: ["...KKK...", "...KWK...", "...KWK...", "...KRK...", "...KRK...", "..KRRRK..", "..KRRRK..", "..KRRRK..", "...KKK..."],
        palette: { K: "#d8dde2", W: "#2a3036", R: "#e0533d" },
    },
    plates: {
        rows: ["AAAAR BBB", "AAAARBBBB", "AAARBBBBB", "AAARBBBBB", "CCCRRRRDD", "CCCCCYDDD", "CCCCCYDDD", "CCCCYDDDD", "CCCCYDDDD"].map((r) => r.replace(" ", "B")),
        palette: { A: "#7a6aa8", B: "#5a8a6a", C: "#8a7a5a", D: "#5a6a8a", R: "#ec3428", Y: "#ecd634" },
    },
    geology: {
        rows: ["SSSSSSSSS", "LLLLLLLLL", "SSSSSSSSS", "HHHHHHHHH", "HHHHHHHHH", "GGGGOGGGG", "GGGOOOGGG", "GGGGOGGGG", "GGGGGGGGG"],
        palette: { S: "#c9b88a", L: "#d6cfb4", H: "#7a6a5e", G: "#b07060", O: "#e0b040" },
    },
    weather: {
        rows: [".........", "...CCC...", "..CCCCCC.", ".CCCCCCCC", ".DDDDDDD.", "..B.B.B..", ".B.B.B...", "..B.B.B..", "........."],
        palette: { C: "#e6eaee", D: "#9aa4ae", B: "#4a86e0" },
    },
    moisture: {
        rows: ["....B....", "...BBB...", "...BBB...", "..BBBBB..", "..BWBBB..", ".BWBBBBB.", ".BBBBBBB.", "..BBBBB..", "...BBB..."],
        palette: { B: "#3a8ad6", W: "#bfe0ff" },
    },
    seasons: {
        rows: ["RRRRRRRRR", "RRRRRRRRR", "RRRRRRRRR", "GGGGGGGGG", "GGGGGGGGG", "GGGGGGGGG", "BBBBBBBBB", "BBBBBBBBB", "BBBBBBBBB"],
        palette: { R: "#e0603d", G: "#9a9a9a", B: "#4a76d0" },
    },
    currents: {
        rows: ["WWWWWWWWW", "WRRRRRWWW", "WRWWWRWWW", "WRWLWRWWW", "WRWLWBWWW", "WRWWWBWWW", "WRBBBBWWW", "WWWWWWWWW", "WWWWWWWWW"],
        palette: { W: "#1e4478", R: "#e0603d", B: "#6aa0e8", L: "#9a8f80" },
    },
    soil: {
        rows: ["GGGGGGGGG", "DDDDDDDDD", "DDODDDDOD", "SSSSSSSSS", "SSSSPSSSS", "CCCCCCCCC", "CCPCCCCCC", "RRRRRRPRR", "RRRRRRRRR"],
        palette: { G: "#5aa83c", D: "#4a3424", O: "#2e2218", S: "#a88c64", C: "#a86c52", P: "#8a8680", R: "#7a7470" },
    },
    fertility: {
        rows: [".........", "....L....", "...LLL...", "..LLSLL..", "....S....", "....S....", "DDDDSDDDD", "DDDDDDDDD", "BBBBBBBBB"],
        palette: { L: "#5aa83c", S: "#3c7a2c", D: "#6a4a30", B: "#4a3424" },
    },
}
