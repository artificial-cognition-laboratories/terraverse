import { reactive } from "vue"
import { DEFAULTS, NEW_WORLD_SIZE, NEW_WORLD_STYLE, randomSeed, type SystemStyle, type WorldConfig } from "../../worldgen/config"
import { worldName } from "../../worldgen/names"
import { DEFAULT_RULES, type Mode, type Rules } from "./rules"
import type { SpinWire, SystemWire } from "../../bridge/types"
import { body as bodyOf, System } from "../../system/model"
import { Companion, type CompanionWire } from "../../system/stars"

/** How the seed lays out a system: roughly, for those who stay on Start. */
export const STYLE_CHOICES = [
    { id: "sunlike", label: "Sun-like", note: "A world like home at a comfortable distance, and neighbours to reach for." },
    { id: "random", label: "Random", note: "Whatever the seed makes round this star, moons and all." },
    { id: "binary", label: "Binary", note: "Two suns, the second chosen by the seed. Set your own on the Stars page." },
    { id: "double", label: "Double", note: "Your world and a second one, huge overhead." },
    { id: "harsh", label: "Harsh", note: "A hard star and thin worlds: survival on the wrong side of the odds." },
] as const satisfies readonly { id: SystemStyle; label: string; note: string }[]

/** How a body turns. Free is the default however close it is: locks are a challenge you choose. */
export const SPINS = [
    { id: "free", label: "Free", note: "Its own day and night, the world to live anywhere on." },
    { id: "partner", label: "Partner", note: "Always the same face to what it circles: that stays fixed in its sky, and its day is their orbit." },
    { id: "star", label: "Star", note: "One side always day, one always night. Life holds on along the line between." },
    { id: "resonance", label: "Resonant", note: "Turns a set number of times per orbit, so days can be very long." },
] as const
export type SpinId = (typeof SPINS)[number]["id"]
export const RESONANCES = ["3:2", "2:1", "5:2"] as const satisfies readonly SpinWire[]

/** A belt of small bodies: rock and metal inside the frost line, ice beyond it. */
export type Belt = { id: string; name: string; inner: number; outer: number; mass: number } // AU; log₁₀ Moons

/** What a body's surface is set to: the start body's in the code's own settings, the others' on their bodies. */
export type SurfaceSettings = Pick<WorldConfig, "continents" | "land" | "coast" | "mountains" | "roughness" | "temperature" | "humidity" | "rivers" | "caves" | "ores">

export function surfaceDefaults(): SurfaceSettings {
    const { continents, land, coast, mountains, roughness, temperature, humidity, rivers, caves, ores } = DEFAULTS
    return { continents, land, coast, mountains, roughness, temperature, humidity, rivers, caves, ores }
}

/** The world being set up: everything the create screen's tabs edit together; its system from the engine's (`system.home`). */
export function WorldDraft(opts: { home: SystemWire }) {
    const system = System(opts.home)
    return reactive({
        name: worldName([]),
        /** Names of worlds that already exist, once the screen has asked the engine. */
        taken: [] as string[],
        mode: "survival" as Mode,
        rules: { ...DEFAULT_RULES } as Rules,
        config: { ...DEFAULTS, companion: Companion(), size: NEW_WORLD_SIZE, style: NEW_WORLD_STYLE, seed: randomSeed() } as WorldConfig,
        /** The planetary system: causes only (src/system/model.ts). */
        system: system,
        /** The second star the seed chose, under style Binary: what the code will make. */
        rolled: null as CompanionWire | null,
        /** The system as its seed and style lay it out: what "edited" is against. */
        generated: "" ,
        belts: [] as Belt[],
        spawn: null as { x: number; z: number } | null,
    })
}

export type WorldDraftHandle = ReturnType<typeof WorldDraft>

/** Begin on this body. The code's land settings are the start body's, so they change places with its own. */
export function startHere(draft: WorldDraftHandle, id: string) {
    const was = draft.system.bodies.find((b) => b.playable)!, next = bodyOf(draft.system, id)
    const pick = (from: SurfaceSettings): SurfaceSettings => {
        const { continents, land, coast, mountains, roughness, temperature, humidity, rivers, caves, ores } = from
        return { continents, land, coast, mountains, roughness, temperature, humidity, rivers, caves, ores }
    }
    was.land = pick(draft.config)
    Object.assign(draft.config, next.land ?? surfaceDefaults())
    next.land = null
    was.playable = false
    next.playable = true
}

/** Which preset the current values match exactly, if any — so the menu can show "custom". */
export function matching<V extends object>(presets: readonly { id: string; values: Partial<V> }[], current: V): string | null {
    const found = presets.find((p) => Object.entries(p.values).every(([k, v]) => Math.abs(Number(current[k as keyof V]) - Number(v)) < 0.005 || current[k as keyof V] === v))
    return found?.id ?? null
}
