<script setup lang="ts">
import type { Listing } from "~/registry"

/** Listings as a grid of cards, as many across as fit while each stays taller than wide. `fill` tops a short row up with open slots. */
const props = defineProps<{ listings: readonly Listing[]; fill?: number }>()
const open = computed(() => Math.max(0, (props.fill ?? 0) - props.listings.length))
</script>

<template>
    <div class="cards">
        <registry-card v-for="l in listings" :key="l.name" :listing="l" />
        <registry-card-open v-for="i in open" :key="`open-${i}`" :first="listings.length === 0 && i === 1" />
    </div>
</template>

<style scoped lang="scss">
// As many as fit, never narrower than 14rem: a column only grows until another fits beside it
// (under 29rem), so a card stays taller than it is wide at every width
.cards {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(min(100%, 14rem), 1fr));
    gap: 1rem;
}
</style>
