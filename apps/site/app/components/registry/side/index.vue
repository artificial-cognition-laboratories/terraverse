<script setup lang="ts">
/**
 * A registry page's side column: one card, its sections divided by hairlines,
 * nothing inside boxed again. Each section gets the card's padding unless it
 * says `flush` and sets its own (a nav runs closer to the edge).
 */
</script>

<template>
    <aside class="side"><slot /></aside>
</template>

<style scoped lang="scss">
.side {
    @include panel;
    display: grid;
    align-content: start;

    > :slotted(*) {
        margin: 0;
    }

    > :slotted(:not(.flush)) {
        padding: 1rem;
    }

    > :slotted(:not(:first-child)) {
        border-top: 1px solid $glass-edge;
    }
}
</style>
