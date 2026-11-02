<script setup lang="ts">
import { computed } from "vue"
import type { BodyCard } from "../bridge"
import type { LandMoment } from "../worldgen/fields"

const when = (gyr: number) => (gyr < 1 ? `${Math.round(gyr * 1000)} million years` : `${gyr.toFixed(1)} billion years`)

/**
 * A body's story, formation to today, as the body model stepped through it;
 * for the playable world, what its plates did lately too (`land`, million
 * years before today), in among it.
 */
const props = defineProps<{ card: BodyCard; age: number; land?: LandMoment[] }>()
const moments = computed(() => [
    ...props.card.history.map((m) => ({ age: m.age, when: when(m.age), text: m.text })),
    ...(props.land ?? []).map((m) => ({ age: props.age - m.ago / 1000, when: `${m.ago} million years ago`, text: m.text })),
].sort((a, b) => a.age - b.age))
</script>

<template>
    <ol class="history">
        <li class="start"><span class="age">0</span><span>it forms from the disc around its star</span></li>
        <li v-for="(moment, i) in moments" :key="i">
            <span class="age">{{ moment.when }}</span><span>{{ moment.text }}</span>
        </li>
        <li class="now"><span class="age">{{ when(age) }}</span><span>today: {{ card.kind }}</span></li>
    </ol>
</template>

<style scoped>
.history { list-style: none; margin: 0; padding: 0 0 0 14px; border-left: var(--b) solid var(--accent-lo); display: grid; gap: 8px; }
li { position: relative; display: grid; grid-template-columns: 112px 1fr; gap: 10px; font-size: 11px; color: var(--ink-dim); }
li::before { content: ""; position: absolute; left: -19px; top: 4px; width: 7px; height: 7px; background: var(--accent-lo); border: var(--b) solid var(--edge); }
.age { font: 10px var(--font-display); text-transform: uppercase; color: var(--ink-faint); }
.now { color: var(--ink); }
.now::before { background: var(--accent-hi); }
</style>
