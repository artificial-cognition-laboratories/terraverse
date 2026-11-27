<script setup lang="ts">
/**
 * A page's side column, as the website's (shared/side): one card of glass, its sections divided by
 * hairlines, nothing inside boxed again. A section gets the card's padding unless it's `.flush`.
 */
</script>

<template>
    <aside class="side"><slot /></aside>
</template>

<style scoped>
/* one column the card's width: a wide child (a row of stats) never widens it */
.side { display: grid; grid-template-columns: minmax(0, 1fr); align-content: start; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); backdrop-filter: blur(8px); }
.side > :slotted(*) { margin: 0; }
.side > :slotted(:not(.flush)) { padding: 1rem; }
.side > :slotted(:not(:first-child)) { border-top: 1px solid var(--glass-edge); }
</style>
