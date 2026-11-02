<script setup lang="ts">
import { computed } from "vue"
import { Field, Section, Slider, Toggle } from "../../kit/index.ts"
import BodyFacts from "../../system/BodyFacts.vue"
import History from "../../system/History.vue"
import { body as bodyOf, LIMITS, STAR } from "../../system/model.ts"
import { useSystemPreview } from "../../system/preview.ts"
import type { WorldDraftHandle } from "./draft.ts"
import { useLooks } from "./looks.ts"
import WorldTab from "./WorldTab.vue"

type BodyPage = "overview" | "orbit" | "makeup" | "spin" | "land" | "climate" | "underground" | "history"
const props = defineProps<{ draft: WorldDraftHandle; page: BodyPage }>()
const preview = useSystemPreview()
const chosen = computed(() => props.draft.system.selected === STAR ? null : bodyOf(props.draft.system, props.draft.system.selected))
const card = computed(() => chosen.value ? preview.card(chosen.value.id) : null)
const looks = useLooks()
// the playable world's plates, from its terrain preview
const land = computed(() => chosen.value?.playable ? looks.terrain.fields.value?.history ?? [] : [])
const moon = computed(() => chosen.value?.parent != null)
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
            <p>The star is selected. Choose a body above the viewer, or edit the star under System.</p>
        </div>
        <template v-else>
            <div class="identity">
                <Field v-if="page === 'overview'" v-model="chosen.name" label="Body name · preview only" :maxlength="32" />
                <strong v-else>{{ chosen.name }}</strong>
                <span class="status">{{ chosen.playable ? "Playable surface" : "Preview only" }}</span>
            </div>

            <template v-if="page === 'overview'">
                <p class="copy">Set the causes on the pages below; the model works out what this body becomes.</p>
                <BodyFacts v-if="card" :card="card" />
                <p v-else class="copy">{{ preview.problem.value ?? 'Working out this body…' }}</p>
            </template>
            <template v-else-if="page === 'orbit'">
                <Section :title="moon ? 'Around its planet' : 'Around the star'" hint="system preview only">
                    <Slider v-model="chosen.orbit" :label="moon ? 'Distance' : 'Distance from the star'"
                        :min="moon ? LIMITS.orbit.planet.min : LIMITS.orbit.star.min" :max="moon ? 100 : 40" :step="moon ? 0.5 : 0.01"
                        :format="(v) => moon ? `${v} planet radii` : `${v.toFixed(2)} AU`" />
                    <Slider v-model="chosen.eccentricity" label="Eccentricity" :min="0" :max="0.6" :step="0.005" :format="(v) => v.toFixed(3)" />
                    <Slider v-model="chosen.inclination" label="Inclination" :min="-30" :max="30" :step="1" :format="(v) => `${v}°`" />
                </Section>
            </template>
            <template v-else-if="page === 'makeup'">
                <Section title="What it formed from" hint="system preview only">
                    <Slider v-model="chosen.mass" label="Mass" :min="LIMITS.mass.min" :max="LIMITS.mass.max" :step="0.01" :format="mass" />
                    <Slider v-model="chosen.formed" label="Where it formed" :min="0" :max="1" :step="0.01" :format="formed" />
                    <Slider v-model="chosen.volatiles" label="Water and gases" :min="0" :max="1" :step="0.01" :format="percent" />
                </Section>
            </template>
            <template v-else-if="page === 'spin'">
                <Section title="Rotation and life" hint="system preview only">
                    <Slider v-model="chosen.day" label="Day" :min="4" :max="2000" :step="1" :format="day" />
                    <Slider v-if="!chosen.playable" v-model="chosen.tilt" label="Axial tilt" :min="0" :max="90" :step="1" :format="(v) => `${v}°`" />
                    <p v-else class="copy">Earth's tilt and year are saved in its world code. Change them under Climate.</p>
                    <Toggle v-model="chosen.life" label="Life arises where it can" />
                </Section>
            </template>
            <template v-else-if="page === 'history'">
                <History v-if="card" :card="card" :age="draft.system.age" :land="land" />
                <p v-else class="copy">{{ preview.problem.value ?? 'Working out this body…' }}</p>
            </template>
            <template v-else-if="chosen.playable">
                <WorldTab :draft="draft" :page="page" />
            </template>
            <div v-else class="empty planned">
                <h2>Surface editor planned</h2>
                <p>This body's generated appearance is a preview. Fine control over its land, climate, and underground will appear here when those surfaces become playable.</p>
                <BodyFacts v-if="card" :card="card" />
            </div>
        </template>
    </div>
</template>

<style scoped>
.inspector { display: flex; flex-direction: column; gap: 18px; min-width: 0; }
.identity { display: flex; align-items: end; gap: 8px; }
.identity :deep(.field) { flex: 1; min-width: 0; }
.identity strong { flex: 1; font: 12px var(--font-display); text-transform: uppercase; }
.status { flex: none; padding-bottom: 9px; color: var(--accent-hi); font: 9px var(--font-display); text-transform: uppercase; }
.copy, .empty p { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
.empty { display: grid; gap: 10px; }
.empty h2 { margin: 0; font: 16px var(--font-display); text-transform: uppercase; }
.planned { padding: 12px; border-left: 3px solid var(--ink-faint); background: var(--stone-2); }
</style>
