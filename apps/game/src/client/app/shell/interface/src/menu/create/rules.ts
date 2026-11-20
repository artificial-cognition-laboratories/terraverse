// How a world is played: survival systems and their strength, from arcade to
// realistic. Saved into the world (world.mt, tf_* keys) for the game's Lua to
// read; the systems themselves are built one at a time.

import type { WorldConfig } from "../../worldgen/config"
import { encodeWorldCode } from "../../worldgen/config"

export type Mode = "survival" | "creative"

export type Rules = {
    // the body
    hunger: "off" | "slow" | "normal" | "fast"
    thirst: boolean
    temperature: boolean // body heat from climate, weather, clothing and shelter
    illness: "off" | "rare" | "realistic"
    regen: "fast" | "normal" | "slow" | "off" // natural healing
    stamina: boolean
    // danger
    difficulty: "peaceful" | "easy" | "normal" | "hard"
    keepInventory: boolean
    // time and weather
    dayLength: number // real minutes per in-game day
    seasons: boolean
    weather: "off" | "mild" | "wild"
    // knowledge
    wiki: boolean // the wiki in game (key G): what everything is made of and how it behaves
}

export const RULE_PRESETS = [
    {
        id: "arcade", label: "Arcade", note: "explore and build; little can hurt you",
        rules: { hunger: "slow", thirst: false, temperature: false, illness: "off", regen: "fast", stamina: false, difficulty: "easy", keepInventory: true, dayLength: 30, seasons: false, weather: "mild", wiki: true },
    },
    {
        id: "classic", label: "Classic", note: "the survival you know",
        rules: { hunger: "normal", thirst: false, temperature: false, illness: "off", regen: "normal", stamina: false, difficulty: "normal", keepInventory: false, dayLength: 20, seasons: false, weather: "mild", wiki: true },
    },
    {
        id: "hardy", label: "Hardy", note: "cold nights, empty stomachs, real risk",
        rules: { hunger: "normal", thirst: true, temperature: true, illness: "rare", regen: "slow", stamina: true, difficulty: "hard", keepInventory: false, dayLength: 40, seasons: true, weather: "wild", wiki: true },
    },
    {
        id: "realistic", label: "Realistic", note: "every system on, slow and unforgiving",
        rules: { hunger: "fast", thirst: true, temperature: true, illness: "realistic", regen: "off", stamina: true, difficulty: "hard", keepInventory: false, dayLength: 60, seasons: true, weather: "wild", wiki: false },
    },
] as const satisfies readonly { id: string; label: string; note: string; rules: Rules }[]

export const DEFAULT_RULES: Rules = { ...RULE_PRESETS[1].rules }

/** The world.mt settings a new world is created with (see worlds.cpp). */
export function worldSettings(mode: Mode, rules: Rules, config: WorldConfig): Record<string, string | number | boolean> {
    return {
        creative_mode: mode === "creative",
        enable_damage: mode === "survival",
        tf_world_code: encodeWorldCode(config),
        tf_hunger: rules.hunger,
        tf_thirst: rules.thirst,
        tf_temperature: rules.temperature,
        tf_illness: rules.illness,
        tf_regen: rules.regen,
        tf_stamina: rules.stamina,
        tf_difficulty: rules.difficulty,
        tf_keep_inventory: rules.keepInventory,
        tf_day_length: rules.dayLength,
        tf_seasons: rules.seasons,
        tf_weather: rules.weather,
        tf_wiki: rules.wiki,
    }
}
