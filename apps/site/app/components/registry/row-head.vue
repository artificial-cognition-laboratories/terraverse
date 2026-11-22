<script setup lang="ts">
import type { RouteLocationRaw } from "vue-router"

/** A row's label over a hairline, with an optional "see all" on the right. */
defineProps<{ label: string; link?: { label: string; to: RouteLocationRaw } }>()
</script>

<template>
    <div class="row-head">
        <h3>{{ label }}</h3>
        <NuxtLink v-if="link" :to="link.to" class="more">{{ link.label }} »</NuxtLink>
    </div>
</template>

<style scoped lang="scss">
.row-head {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    margin: 3rem 0 1rem;
    padding-bottom: 0.6rem;
    border-bottom: 1px solid $glass-edge;
}

h3 {
    margin: 0;
    font-family: $font-display;
    font-weight: 400;
    font-size: 0.7rem;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: $ink;
}

.more {
    font-family: $font-display;
    font-size: 0.62rem;
    text-transform: uppercase;
    color: $primary-ink;
    text-decoration: none;

    &:hover {
        color: $ink;
    }
}
</style>
