<script setup lang="ts">
import type { Listing } from "~/registry"

/** Listings as a grid of cards, four across on a desktop. */
defineProps<{ listings: readonly Listing[] }>()
</script>

<template>
    <div class="cards">
        <registry-card v-for="l in listings" :key="l.name" :listing="l" />
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
