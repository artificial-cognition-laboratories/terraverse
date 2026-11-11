<script setup lang="ts">
import { computed } from "vue"
import { Section, Slider } from "../../kit"
import { body as bodyOf, STAR } from "../../system/model"
import { useSystemPreview } from "../../system/preview"
import { surfaceDefaults, type SurfaceSettings, type WorldDraftHandle } from "./draft"

/** A body's surface. The start body's is the world code's own; the others' are on their bodies, once someone sets them. */
const props = defineProps<{ draft: WorldDraftHandle }>()
const preview = useSystemPreview()
const chosen = computed(() => props.draft.system.selected === STAR ? null : bodyOf(props.draft.system, props.draft.system.selected))
const card = computed(() => chosen.value ? preview.card(chosen.value.id) : null)
// another body's reads the usual until a slider moves: only looking at one isn't an edit
const settings = computed<SurfaceSettings | null>(() => {
    const body = chosen.value
    if (!body) return null
    if (body.playable) return props.draft.config
    return new Proxy<SurfaceSettings>(surfaceDefaults(), {
        get: (usual, key: keyof SurfaceSettings) => (body.land ?? usual)[key],
        set: (usual, key: keyof SurfaceSettings, value: number) => {
            body.land ??= { ...usual }
            body.land[key] = value
            return true
        },
    })
})
// what a body has decides what it offers: no coast without a sea
const wet = computed(() => card.value?.ocean ?? false)
const moving = computed(() => card.value?.plates ?? false)

const percent = (v: number) => `${Math.round(v * 100)}%`
const signed = (v: number) => (Math.round(v * 100) === 0 ? "0" : `${v > 0 ? "+" : ""}${Math.round(v * 100)}`)
</script>

<template>
    <div class="surface">
        <p v-if="!chosen" class="copy">The star has no surface. Choose a planet or moon above the view.</p>
        <p v-else-if="card?.giant" class="copy">{{ chosen.name }} is a giant, all cloud with nothing to stand on. You can visit, but there's no surface to shape.</p>
        <template v-else-if="settings">
            <strong class="name">{{ chosen.name }}</strong>
            <Section title="Land">
                <Slider v-if="moving" v-model="settings.continents" label="Continents" :min="1" :max="16" :step="1" />
                <Slider v-if="wet" v-model="settings.land" label="Land" :min="0.1" :max="0.9" :step="0.01" :format="percent" />
                <Slider v-if="wet" v-model="settings.coast" label="Coastline" :min="0" :max="1" :step="0.01"
                    :format="(v) => (v < 0.33 ? 'smooth' : v < 0.66 ? 'varied' : 'jagged')" />
                <Slider v-model="settings.mountains" label="Mountains" :min="0" :max="1" :step="0.01" :format="percent" />
                <Slider v-model="settings.roughness" label="Roughness" :min="0" :max="1" :step="0.01" :format="percent" />
            </Section>
            <Section v-if="wet" title="Water">
                <Slider v-model="settings.rivers" label="Rivers" :min="0" :max="1" :step="0.01"
                    :format="(v) => (v < 0.33 ? 'few, large' : v < 0.66 ? 'some' : 'many, branching')" />
                <Slider v-model="settings.humidity" label="Humidity" :min="-1" :max="1" :step="0.01" :format="signed" />
            </Section>
            <p v-else class="copy">No seas here, so there's no coast, rivers or rain to set.</p>
            <Section title="Climate">
                <Slider v-model="settings.temperature" label="Temperature" :min="-1" :max="1" :step="0.01" :format="signed" />
            </Section>
            <Section title="Underground">
                <Slider v-model="settings.caves" label="Caves" :min="0" :max="1" :step="0.01" :format="percent" />
                <Slider v-model="settings.ores" label="Ore richness" :min="0" :max="1" :step="0.01" :format="percent" />
            </Section>
        </template>
    </div>
</template>

<style scoped>
.surface { display: flex; flex-direction: column; gap: 18px; min-width: 0; }
.name { font: 12px var(--font-display); text-transform: uppercase; }
.copy { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
</style>
