// The wiki's Flora page: places to try a plant in. Each is a site as terragen would
// hand it over (src/flora/flora.h: Site); the sliders start from one and go anywhere.

import type { FloraSite } from "../bridge"

export type FloraPlace = { id: string; title: string; site: FloraSite }

const site = (over: Partial<FloraSite>): FloraSite => ({
    heat: 0, wet: 0, season: 0, fertility: 0.5, sand: 0.4, clay: 0.2, stones: 0, depth: 4, wind: 0, slope: 0, upset: 0, light: 1, bank: false, ...over,
})

export const FLORA_PLACES: FloraPlace[] = [
    { id: "temperate", title: "Temperate lowland", site: site({ heat: 0.05, wet: 0.25, fertility: 0.6 }) },
    { id: "taiga", title: "Cold north", site: site({ heat: -0.35, wet: 0.2, fertility: 0.35 }) },
    { id: "jungle", title: "Wet tropics", site: site({ heat: 0.7, wet: 0.75, fertility: 0.6, sand: 0.3, depth: 6 }) },
    { id: "savanna", title: "Dry-season tropics", site: site({ heat: 0.6, wet: -0.15, season: 0.7, fertility: 0.4, sand: 0.5 }) },
    { id: "mediterranean", title: "Dry-summer coast", site: site({ heat: 0.3, wet: -0.2, season: -0.6, fertility: 0.4, sand: 0.5, depth: 2 }) },
    { id: "oasis", title: "Desert riverbank", site: site({ heat: 0.7, wet: -0.8, sand: 0.6, bank: true }) },
    { id: "headland", title: "Windy headland", site: site({ heat: 0.1, wet: 0.3, fertility: 0.4, sand: 0.6, depth: 3, wind: 0.9 }) },
    { id: "floor", title: "Rainforest floor", site: site({ heat: 0.5, wet: 0.7, fertility: 0.6, sand: 0.3, depth: 5, light: 0.15 }) },
    { id: "desert", title: "Open desert", site: site({ heat: 0.7, wet: -0.7, fertility: 0.3, sand: 0.7, depth: 3 }) },
    { id: "crag", title: "Bare rock", site: site({ heat: 0.1, wet: 0.2, fertility: 0.3, sand: 0.5, stones: 0.5, depth: 0 }) },
]

export const LIMITS: Record<string, string> = {
    none: "nothing much", cold: "cold", drought: "drought", hunger: "poor ground", rooting: "thin soil", shade: "too little light", wet: "too much water",
}

export const STRATEGIES: Record<string, string> = {
    competitor: "Competitor: plenty of everything, so the race is for light",
    tolerator: "Tolerator: short supply, so grow slow and last",
    pioneer: "Pioneer: knocked back often, so grow fast and small",
}
