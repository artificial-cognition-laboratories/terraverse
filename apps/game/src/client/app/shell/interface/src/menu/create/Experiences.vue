<script setup lang="ts">
import { Card } from "../../kit/index.ts"
import type { Mode } from "./rules.ts"
import { EXPERIENCES } from "./experiences.ts"

/** What the world is played as: a card each, the chosen one lit, and the way to more. */
const mode = defineModel<Mode>({ required: true })
defineEmits<{ browse: [] }>()
</script>

<template>
    <div class="experiences" role="radiogroup" aria-label="How you play">
        <Card v-for="e in EXPERIENCES" :key="e.id" :title="e.title" :seed="`@terraverse/${e.id}`" :by="e.by"
            :description="e.description" :badges="['Official']" :on="mode === e.id" dense role="radio"
            :aria-checked="mode === e.id" @open="mode = e.id" />
        <button class="more" type="button" @click="$emit('browse')">
            <span class="plus">+</span>
            <span class="label">More ways to play</span>
            <span class="note">Servers from the community: minigames, adventures, other rules</span>
        </button>
    </div>
</template>

<style scoped>
.experiences { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 10px; }
.more { grid-column: 1 / -1; display: grid; grid-template-columns: auto minmax(0, 1fr); align-items: center; gap: 2px 10px;
    padding: 10px 12px; border: 1px dashed var(--stone-4); border-radius: var(--radius); background: none; color: var(--ink-dim); text-align: left; }
.more:hover { border-color: var(--accent-lo); color: var(--ink); }
.plus { grid-row: span 2; font: 18px var(--font-display); color: var(--accent-hi); }
.label { font: 11px var(--font-display); text-transform: uppercase; }
.note { color: var(--ink-faint); font-size: 10px; }
</style>
