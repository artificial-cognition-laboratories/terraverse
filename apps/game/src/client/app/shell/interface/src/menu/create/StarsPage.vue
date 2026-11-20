<script setup lang="ts">
import { computed } from "vue"
import { IconTile, Section, Segmented, Slider } from "../../kit/index.ts"
import { LIMITS } from "../../system/model.ts"
import { lifetime, SEPARATIONS, STAR_CLASSES, starClass, type CompanionKind, type StarClass } from "../../system/stars.ts"
import BodyIcon from "../../worldgen/BodyIcon.vue"
import type { WorldDraftHandle } from "./draft.ts"

/** The star, what it's made of and how old, and whether it has a companion. */
const props = defineProps<{ draft: WorldDraftHandle }>()
const config = computed(() => props.draft.config)
const chosen = computed(() => starClass(config.value.star))
const companion = computed(() => props.draft.config.companion)
const other = computed(() => starClass(companion.value.class))
const HOTTEST_FIRST = [...STAR_CLASSES].reverse()

function pick(id: StarClass) {
    config.value.star = id
    config.value.starMass = starClass(id).mass.usual
}
function pickOther(id: StarClass) {
    companion.value.class = id
    companion.value.mass = starClass(id).mass.usual
}
const solar = (v: number) => `${v < 1 ? v.toFixed(2) : v.toFixed(1)} Suns`
const years = (v: number) => v >= 1 ? `${v.toFixed(1)} billion years` : v >= 0.01 ? `${Math.round(v * 1000)} million years` : `${(v * 1000).toFixed(1)} million years`

// masses and ages span orders of magnitude: their sliders are in the logarithm
const lives = computed(() => Math.min(LIMITS.age.max, lifetime(config.value.starMass) * 0.98))
const age = computed({
    get: () => Math.log10(config.value.age),
    set: (v: number) => { config.value.age = 10 ** v },
})
const mass = computed({
    get: () => Math.log10(config.value.starMass),
    set: (v: number) => { config.value.starMass = 10 ** v },
})

const KINDS: { value: CompanionKind; label: string }[] = [
    { value: "none", label: "Single" }, { value: "close", label: "Close pair" }, { value: "wide", label: "Wide pair" },
]
const kind = computed({
    get: () => companion.value.kind,
    set: (next: CompanionKind) => {
        companion.value.kind = next
        if (next !== "none") companion.value.separation = Math.sqrt(SEPARATIONS[next].min * SEPARATIONS[next].max)
    },
})
const reach = computed(() => companion.value.kind === "none" ? null : SEPARATIONS[companion.value.kind])
const separation = computed({
    get: () => Math.log10(companion.value.separation),
    set: (v: number) => { companion.value.separation = 10 ** v },
})
</script>

<template>
    <div class="stars-page">
        <Section title="Star class">
            <div class="classes">
                <IconTile v-for="c in HOTTEST_FIRST" :key="c.id" :label="c.id" :on="config.star === c.id" :size="40" @click="pick(c.id)">
                    <BodyIcon kind="star" :color="c.color" />
                </IconTile>
            </div>
            <p class="copy"><strong :style="{ color: `rgb(${chosen.color.join(' ')})` }">{{ chosen.label }}</strong> · {{ chosen.kelvin }}</p>
            <Slider :key="config.star" v-model="mass" label="Mass" :min="Math.log10(chosen.mass.least)" :max="Math.log10(chosen.mass.most)" :step="0.002" :format="(v) => solar(10 ** v)" />
        </Section>

        <Section title="History">
            <Slider v-model="age" label="Age" :min="Math.log10(LIMITS.age.min)" :max="Math.log10(lives)" :step="0.005" :format="(v) => years(10 ** v)" />
            <p class="copy">A star this heavy lives about {{ years(lifetime(config.starMass)) }}.</p>
            <Slider v-model="config.wind" label="Stellar wind" :min="0" :max="1" :step="0.01" :format="(v) => `${Math.round(v * 100)}%`" />
            <Slider v-model="config.metallicity" label="Metallicity" :min="-1" :max="0.5" :step="0.05"
                :format="(v) => `${(10 ** v).toFixed(1)}× the Sun's`" />
            <p class="copy">More metal means more rock and iron to build worlds from.</p>
        </Section>

        <Section title="Companion">
            <Segmented v-model="kind" :options="KINDS" />
            <p v-if="draft.rolled" class="copy">This seed's Binary layout gives a {{ draft.rolled.class }} star {{ draft.rolled.separation < 1 ? draft.rolled.separation.toFixed(2) : Math.round(draft.rolled.separation) }} AU away. Pick a pair above to choose your own.</p>
            <template v-if="reach">
                <div class="classes">
                    <IconTile v-for="c in STAR_CLASSES" :key="c.id" :label="c.id" :on="companion.class === c.id" :size="40" @click="pickOther(c.id)">
                        <BodyIcon kind="star" :color="c.color" />
                    </IconTile>
                </div>
                <p class="copy"><strong :style="{ color: `rgb(${other.color.join(' ')})` }">{{ other.label }}</strong></p>
                <Slider :key="companion.class" v-model="companion.mass" label="Mass" :min="other.mass.least" :max="other.mass.most" :step="(other.mass.most - other.mass.least) / 100" :format="solar" />
                <Slider :key="companion.kind" v-model="separation" label="Separation" :min="Math.log10(reach.min)" :max="Math.log10(reach.max)" :step="0.01"
                    :format="() => `${companion.separation < 1 ? companion.separation.toFixed(2) : Math.round(companion.separation)} AU`" />
                <p class="copy">{{ reach.note(companion.separation) }}</p>
            </template>
        </Section>
    </div>
</template>

<style scoped>
.stars-page { display: flex; flex-direction: column; gap: 18px; min-width: 0; }
.classes { display: flex; flex-wrap: wrap; gap: 6px; }
.copy { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
.copy strong { font: 400 12px var(--font-display); text-transform: uppercase; }
.tag { margin: -8px 0 0; color: var(--ink-faint); font-size: 11px; }
</style>
