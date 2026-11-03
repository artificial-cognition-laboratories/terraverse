<script setup lang="ts">
import { computed } from "vue"
import { Button, Field, Section, Segmented, Slider, Toggle } from "../../kit/index.ts"
import BodyFacts from "../../system/BodyFacts.vue"
import History from "../../system/History.vue"
import { body as bodyOf, LIMITS, partnerOf, STAR } from "../../system/model.ts"
import { useSystemPreview } from "../../system/preview.ts"
import type { SpinWire } from "../../bridge/types"
import { RESONANCES, SPINS, startHere, type SpinId, type WorldDraftHandle } from "./draft.ts"
import { useLooks } from "./looks.ts"

const props = defineProps<{ draft: WorldDraftHandle }>()
const preview = useSystemPreview()
const chosen = computed(() => props.draft.system.selected === STAR ? null : bodyOf(props.draft.system, props.draft.system.selected))
const card = computed(() => chosen.value ? preview.card(chosen.value.id) : null)
const looks = useLooks()
// the playable world's plates, from its terrain preview
const land = computed(() => chosen.value?.playable ? looks.terrain.fields.value?.history ?? [] : [])
const moon = computed(() => chosen.value?.parent != null)
const partner = computed(() => chosen.value ? partnerOf(props.draft.system, chosen.value.id) : null)
// a moon can face its planet, not its star; a planet can face its partner, if it has one
const spinOptions = computed(() => SPINS
    .filter((spin) => spin.id === "partner" ? moon.value || partner.value : spin.id !== "star" || !moon.value)
    .map((spin) => ({ value: spin.id, label: spin.label })))
const isResonance = (s: SpinWire): s is (typeof RESONANCES)[number] => RESONANCES.some((r) => r === s)
const lock = computed({
    get: (): SpinId => { const s = chosen.value?.spin ?? "free"; return isResonance(s) ? "resonance" : s },
    set: (id: SpinId) => { if (chosen.value) chosen.value.spin = id === "resonance" ? "3:2" : id },
})
const resonance = computed({
    get: () => { const s = chosen.value?.spin; return s && isResonance(s) ? s : "3:2" },
    set: (r: (typeof RESONANCES)[number]) => { if (chosen.value) chosen.value.spin = r },
})
// (the tides that hold it stand its axis square to the orbit)
const heldSquare = computed(() => lock.value === "partner" || lock.value === "star")
const spinNote = computed(() => SPINS.find((item) => item.id === lock.value)!.note)
const percent = (v: number) => `${Math.round(v * 100)}%`
const mass = (v: number) => {
    const m = 10 ** v
    return `${m < 0.1 ? m.toPrecision(2) : m < 10 ? m.toFixed(2) : Math.round(m)} Earth masses`
}
const formed = (v: number) => v < 0.25 ? "close to its star: rock and metal" : v < 0.5 ? "inside the frost line" : v < 0.7 ? "near the frost line" : "beyond the frost line: icy"
const day = (h: number) => h < 48 ? `${Math.round(h)} hours` : `${(h / 24).toFixed(1)} days`
</script>

<template>
    <div class="inspector">
        <div v-if="!chosen" class="empty">
            <h2>Select a planet or moon</h2>
            <p>The star is selected. Choose a body above the view to change it.</p>
        </div>
        <template v-else>
            <Field v-model="chosen.name" label="Name" :maxlength="32" />
            <div class="start-here">
                <span>{{ chosen.playable ? "You start here" : card?.giant ? "A giant: visit only" : "A place you could start" }}</span>
                <Button v-if="!chosen.playable" size="sm" :disabled="!card || card.giant" @click="startHere(draft, chosen!.id)">Start here</Button>
            </div>
            <p v-if="partner" class="copy">A double planet with {{ partner.name }}: they circle a point between them.</p>
            <p v-if="chosen.playable" class="copy">The world's land and climate follow what this body is, so moving it changes where you live.</p>
            <BodyFacts v-if="card" :card="card" />
            <p v-else class="copy">{{ preview.problem.value ?? 'Working out this body…' }}</p>

            <Section :title="moon ? 'Around its planet' : 'Around the star'">
                <Slider v-model="chosen.orbit" :label="moon ? 'Distance' : 'Distance from the star'"
                    :min="moon ? LIMITS.orbit.planet.min : LIMITS.orbit.star.min" :max="moon ? 100 : 40" :step="moon ? 0.5 : 0.01"
                    :format="(v) => moon ? `${v} planet radii` : `${v.toFixed(2)} AU`" />
                <Slider v-model="chosen.eccentricity" label="Eccentricity" :min="0" :max="0.6" :step="0.005" :format="(v) => v.toFixed(3)" />
                <Slider v-model="chosen.inclination" label="Inclination" :min="-30" :max="30" :step="1" :format="(v) => `${v}°`" />
            </Section>
            <Section title="What it formed from">
                <Slider v-model="chosen.mass" label="Mass" :min="LIMITS.mass.min" :max="LIMITS.mass.max" :step="0.01" :format="mass" />
                <Slider v-model="chosen.formed" label="Where it formed" :min="0" :max="1" :step="0.01" :format="formed" />
                <Slider v-model="chosen.volatiles" label="Water and gases" :min="0" :max="1" :step="0.01" :format="percent" />
            </Section>
            <Section title="Rotation and life">
                <span class="sub">Spin: free, or locked to</span>
                <Segmented v-model="lock" :options="spinOptions" />
                <p class="copy">{{ spinNote }}</p>
                <Segmented v-if="lock === 'resonance'" v-model="resonance" :options="RESONANCES.map((r) => ({ value: r, label: r }))" />
                <Slider v-if="lock === 'free'" v-model="chosen.day" label="Day" :min="4" :max="2000" :step="1" :format="day" />
                <p v-else-if="card" class="copy">It turns once in {{ day(card.day) }}.</p>
                <Slider v-if="!heldSquare" v-model="chosen.tilt" label="Axial tilt" :min="0" :max="90" :step="1"
                    :format="(v) => `${v}° · ${v < 8 ? 'no real seasons' : v < 18 ? 'mild seasons' : v < 30 ? 'earthlike' : 'harsh seasons'}`" />
                <Toggle v-model="chosen.life" label="Life arises where it can" />
            </Section>
            <History v-if="card" :card="card" :age="draft.system.age" :land="land" />
        </template>
    </div>
</template>

<style scoped>
.inspector { display: flex; flex-direction: column; gap: 18px; min-width: 0; }
.start-here { display: flex; align-items: center; justify-content: space-between; gap: 8px; min-height: 30px; color: var(--accent-hi); font: 9px var(--font-display); text-transform: uppercase; }
.copy, .empty p { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
.sub { color: var(--ink-dim); font-size: 12px; }
.empty { display: grid; gap: 10px; }
.empty h2 { margin: 0; font: 16px var(--font-display); text-transform: uppercase; }
</style>
