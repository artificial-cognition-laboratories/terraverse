<script setup lang="ts">
import type { Listing } from "~/registry"

/** Listings as a grid of cards, four across on a desktop. `fill` tops a short row up with open slots. */
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
.cards {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 1rem;
}

@include desktop-down {
    .cards {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
}

@include mobile {
    .cards {
        grid-template-columns: minmax(0, 1fr);
    }
}
</style>
