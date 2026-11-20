<script setup lang="ts">
import { Notice, Section, Segmented, Slider, Toggle } from "../../kit"
import type { WorldDraftHandle } from "./draft"

/** How the world plays: each survival system and how hard it bites. */
defineProps<{ draft: WorldDraftHandle }>()

const opts = <T extends string>(...values: T[]) => values.map((v) => ({ value: v, label: v }))
</script>

<template>
    <Notice v-if="draft.mode === 'creative'">
        <strong>Creative world:</strong> none of these apply. Switch to survival on Start to use them.
    </Notice>
    <Notice v-else tone="warn">
        <strong>Saved with the world.</strong> These are the rules the survival systems will read — most of the
        systems themselves are still to be built.
    </Notice>

    <div class="rules" :class="{ off: draft.mode === 'creative' }">
        <Section title="The body">
            <div class="row"><span>Hunger</span><Segmented v-model="draft.rules.hunger" :options="opts('off', 'slow', 'normal', 'fast')" /></div>
            <div class="row"><span>Healing</span><Segmented v-model="draft.rules.regen" :options="opts('off', 'slow', 'normal', 'fast')" /></div>
            <div class="row"><span>Illness</span><Segmented v-model="draft.rules.illness" :options="opts('off', 'rare', 'realistic')" /></div>
            <Toggle v-model="draft.rules.thirst" label="Thirst — drink, or weaken" />
            <Toggle v-model="draft.rules.temperature" label="Body temperature — dress for the weather, find shelter" />
            <Toggle v-model="draft.rules.stamina" label="Stamina — sprinting and swimming tire you" />
        </Section>

        <Section title="Danger">
            <div class="row"><span>Difficulty</span><Segmented v-model="draft.rules.difficulty" :options="opts('peaceful', 'easy', 'normal', 'hard')" /></div>
            <Toggle v-model="draft.rules.keepInventory" label="Keep inventory on death" />
        </Section>

        <Section title="Time & weather">
            <Slider v-model="draft.rules.dayLength" label="Day length" :min="10" :max="120" :step="5" :format="(v) => `${v} min`" />
            <Slider v-model="draft.config.year" label="Days in a year" :min="8" :max="128" :step="4"
                :format="(v) => `${v} days · ${Math.round(v / 4)} per season`" />
            <div class="row"><span>Weather</span><Segmented v-model="draft.rules.weather" :options="opts('off', 'mild', 'wild')" /></div>
            <Toggle v-model="draft.rules.seasons" label="Seasons — winters get cold, crops have a time" />
        </Section>
        <Section title="Knowledge">
            <Toggle v-model="draft.rules.wiki" label="Wiki (G) — see what everything is made of, and how it behaves" />
        </Section>
    </div>
</template>

<style scoped>
.rules { display: flex; flex-direction: column; gap: 22px; }
.off { opacity: 0.35; pointer-events: none; }
.row { display: grid; grid-template-columns: minmax(0, 1fr); gap: 5px; font-size: 12px; color: var(--ink-dim); }
.row :deep(.segmented button) { font-size: 10px; padding: 0 3px; }
</style>
