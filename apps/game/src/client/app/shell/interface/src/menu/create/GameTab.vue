<script setup lang="ts">
import { computed } from "vue"
import PixelIcon from "../../game/PixelIcon.vue"
import { Button, Field, IconChoice, Section, Segmented, Slider } from "../../kit/index.ts"
import { DEFAULTS, EARTH_RADIUS, SIZES, WORLD_PRESETS, type WorldConfig } from "../../worldgen/config.ts"
import { worldName } from "../../worldgen/names.ts"
import { matching, STYLE_CHOICES, type WorldDraftHandle } from "./draft.ts"
import { body as bodyOf } from "../../system/model.ts"
import { RULE_PRESETS, type Mode } from "./rules.ts"
import SeedField from "./SeedField.vue"

const props = defineProps<{ draft: WorldDraftHandle }>()

const DICE = ["KKKKKKK", "K.....K", "K.K...K", "K..K..K", "K...K.K", "K.....K", "KKKKKKK"] as const
const MODES = [
    { id: "survival", label: "Survival", rows: ["...SS...", "..SSSS..", ".SSSSSS.", "...SS...", "...SS...", "...SS...", "..SSSS.."], palette: { S: "#c9d4dc" } },
    { id: "creative", label: "Creative", rows: ["...C...", ".C.C.C.", "..CCC..", "CCCCCCC", "..CCC..", ".C.C.C.", "...C..."], palette: { C: "#d9ae69" } },
] as const satisfies readonly { id: Mode; label: string; rows: readonly string[]; palette: Record<string, string> }[]
const LAND_ICONS = {
    continents: ["BBBBBBBBB", "BBLLBBBBB", "BLLLLBBLB", "BLLLLBLLB", "BBLLBBBLL", "BBBLBBBBB", "BBBBBBBBB"],
    archipelago: ["BBBBBBBBB", "BLBBBBLBB", "BLLBBBBBB", "BBBBLLBBB", "BBLBBBBLB", "BBBBBLBBB", "BBBBBBBBB"],
    pangaea: ["BBBBBBBBB", "BBBLLLBBB", "BBLLLLLBB", "BLLLLLLBB", "BBLLLLLBB", "BBBLLLBBB", "BBBBBBBBB"],
    highlands: ["BBBBBBBBB", "BBBMMBBBB", "BBMLMMBBB", "BMLMMLMBB", "BBLLMLBBB", "BBBLLBBBB", "BBBBBBBBB"],
    waterworld: ["BBBBBBBBB", "BBBBBBBBB", "BBBLBBBBB", "BBLLBBBBB", "BBBBBBLBB", "BBBBBBBBB", "BBBBBBBBB"],
} as const
const LAND_OPTIONS = WORLD_PRESETS.map((preset) => ({
    id: preset.id,
    label: preset.id === "archipelago" ? "Islands" : preset.id === "waterworld" ? "Ocean" : preset.label,
    rows: LAND_ICONS[preset.id],
    palette: { B: "#17416a", L: "#6caa73", M: "#bec8bd" },
}))
const LAND_KEYS = ["continents", "land", "coast", "mountains", "roughness", "temperature", "humidity", "rivers", "caves", "ores"] as const
const nameTaken = computed(() => props.draft.taken.some((n) => n.toLowerCase() === props.draft.name.trim().toLowerCase()))
const style = computed({
    get: () => matching(RULE_PRESETS.map((p) => ({ id: p.id, values: p.rules })), props.draft.rules),
    set: (id) => {
        const preset = RULE_PRESETS.find((p) => p.id === id)
        if (preset) Object.assign(props.draft.rules, preset.rules)
    },
})
const styleNote = computed(() => RULE_PRESETS.find((p) => p.id === style.value)?.note ?? "Your own mix of survival rules")
const land = computed({
    get: () => WORLD_PRESETS.find((preset) => LAND_KEYS.every((key) =>
        Math.abs(props.draft.config[key] - ((preset.values as Partial<WorldConfig>)[key] ?? DEFAULTS[key])) < 0.005))?.id ?? null,
    set: (id) => {
        const preset = WORLD_PRESETS.find((p) => p.id === id)
        if (!preset) return
        const reset = Object.fromEntries(LAND_KEYS.map((key) => [key, DEFAULTS[key]])) as Pick<WorldConfig, typeof LAND_KEYS[number]>
        Object.assign(props.draft.config, reset, preset.values)
        props.draft.spawn = null
    },
})
const landNote = computed(() => WORLD_PRESETS.find((p) => p.id === land.value)?.note ?? "A custom land shape")

const SYSTEM_OPTIONS = STYLE_CHOICES.map((style) => ({ value: style.id, label: style.label }))
const systemNote = computed(() => STYLE_CHOICES.find((style) => style.id === props.draft.config.style)?.note ?? "Our own system, as it comes.")
const startOn = computed(() => props.draft.system.bodies.find((b) => b.playable)!.name)

// the planet's size, a notch a size of the code's table
const sizeIndex = computed({
    get: () => {
        const i = SIZES.findIndex((s) => s === props.draft.config.size)
        if (i < 0) throw new Error(`world size ${props.draft.config.size} isn't one of this build's`)
        return i
    },
    set: (i: number) => {
        const size = SIZES[i]
        if (size === undefined) throw new Error(`no world size ${i}`)
        props.draft.config.size = size
        props.draft.spawn = null
    },
})
const radius = (i: number) => (SIZES[i] ?? 0) / (2 * Math.PI)
const blocks = (r: number) => (r >= 1e6 ? `${(r / 1e6).toFixed(r >= 1e7 ? 0 : 2)}M` : `${Math.round(r / 1000)}k`)
const sizeLabel = (i: number) => {
    const r = radius(i)
    const earth = r / EARTH_RADIUS
    return `${blocks(r)} radius · ${earth > 0.99 ? "Earth" : earth >= 0.1 ? `${earth.toFixed(1)}× Earth` : `1/${Math.round(1 / earth)} Earth`}`
}
const sizeNote = computed(() => {
    const r = radius(sizeIndex.value)
    const around = (2 * Math.PI * r) / 4 / 3600 // hours to walk round, at 4 blocks a second
    // heights in metres at √(r / Earth's) blocks a metre (terragen/world.h
    // blocksPerMetre): the highest ranges stand about 9 km
    const peaks = 9000 * Math.sqrt(Math.min(1, r / EARTH_RADIUS))
    return `${around < 48 ? `${Math.round(around)} hours` : `${Math.round(around / 24)} days`} to walk round` +
        ` · peaks up to ~${peaks >= 1000 ? `${(peaks / 1000).toFixed(1)}k` : Math.round(peaks / 10) * 10} blocks`
})
</script>

<template>
    <div class="start-page">
        <div class="start-intro">
            <h2>Make it yours</h2>
            <p>Everything here has good defaults: name it and play.</p>
        </div>

        <Section title="Your world">
            <Field v-model="draft.name" label="Name" :maxlength="64">
                <template #action>
                    <Button size="sm" title="Another name" @click="draft.name = worldName(draft.taken)">
                        <PixelIcon :rows="DICE" :palette="{ K: 'currentColor' }" style="--u: 2px" />
                    </Button>
                </template>
                <template #hint><span v-if="nameTaken" class="taken">already a world</span></template>
            </Field>
            <SeedField :draft="draft" />
        </Section>

        <Section title="How you play">
            <IconChoice v-model="draft.mode" label="Game mode" :options="MODES" />
            <p class="choice-note">{{ draft.mode === 'survival' ? 'Gather, build and stay alive.' : 'Build freely with every block and no danger.' }}</p>
            <div v-if="draft.mode === 'survival'" class="style-block">
                <span class="sub-label">Survival style</span>
                <Segmented v-model="style" :options="RULE_PRESETS.map((preset) => ({ value: preset.id, label: preset.label }))" />
                <p class="choice-note">{{ styleNote }}</p>
            </div>
        </Section>

        <Section title="Planet shape">
            <IconChoice v-model="land" label="Land preset" :options="LAND_OPTIONS" :columns="5" layout="stacked" />
            <p class="choice-note">{{ landNote }}</p>
        </Section>

        <Section title="Planet size">
            <Slider v-model="sizeIndex" label="Size" :min="0" :max="SIZES.length - 1" :step="1" :format="sizeLabel" />
            <p class="choice-note">{{ sizeNote }}</p>
        </Section>

        <Section title="Solar system">
            <Segmented v-model="draft.config.style" :options="SYSTEM_OPTIONS" />
            <p class="choice-note">{{ systemNote }}</p>
            <p v-if="draft.config.bodies" class="choice-note">You've changed this system, so it stays as it is when the seed or style changes. Orbit can put it back.</p>
            <p class="choice-note">You start on {{ startOn }}.</p>
        </Section>
    </div>
</template>

<style scoped>
.start-page { display: flex; flex-direction: column; gap: 18px; min-width: 0; }
.start-page :deep(.section .body) { gap: 10px; }
.start-intro { display: grid; gap: 5px; padding-bottom: 10px; border-bottom: 1px solid var(--stone-4); }
.start-intro p { margin: 0; color: var(--ink-dim); font-size: 11px; }
.start-intro h2 { margin: 0; font: 18px var(--font-display); text-transform: uppercase; }
.taken { color: var(--danger); font-size: 11px; }
.choice-note { margin: -5px 0 0; min-height: 16px; color: var(--ink-dim); font-size: 11px; }
.style-block { display: grid; gap: 10px; }
.sub-label { color: var(--ink-dim); font-size: 11px; }
</style>
