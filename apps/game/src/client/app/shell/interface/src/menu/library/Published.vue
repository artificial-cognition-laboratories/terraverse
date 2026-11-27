<script setup lang="ts">
import type { RegistryListing } from "../../bridge/index.ts"
import { CardSlot } from "../../kit/index.ts"
import ModCard from "./ModCard.vue"

/**
 * What a player has published, as the site's cards: as many across as fit. Yours, while empty,
 * is the site's open slot, saying how to fill it.
 */
defineProps<{ listings: readonly RegistryListing[]; reading: boolean; self: boolean }>()
const emit = defineEmits<{ open: [name: string] }>()
</script>

<template>
    <div class="cards">
        <template v-if="reading"><CardSlot v-for="n in 3" :key="n" /></template>
        <template v-else>
            <ModCard v-for="l in listings" :key="l.name" :listing="l" :tag="null" @open="emit('open', l.name)" />
            <div v-if="!listings.length" class="first">
                <span class="plus">+</span>
                <strong>{{ self ? "Be the first" : "Nothing yet" }}</strong>
                <code v-if="self"><span>$</span> terra publish</code>
            </div>
        </template>
    </div>
</template>

<style scoped>
.cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(min(100%, 14rem), 1fr)); gap: 1rem; }
/* the site's open slot: a dashed place for the first one */
.first { display: grid; place-content: center; justify-items: center; gap: 0.5rem; min-height: 13rem; border: 1px dashed var(--night-4); border-radius: var(--radius); }
.first .plus { color: var(--ink-faint); font: 1.2rem var(--font-display); }
.first strong { color: var(--ink-dim); font: 400 0.6rem var(--font-display); text-transform: uppercase; letter-spacing: 0.08em; }
.first code { color: var(--ink-faint); font: 0.72rem var(--font); }
.first code span { color: var(--accent-hi); }
</style>
