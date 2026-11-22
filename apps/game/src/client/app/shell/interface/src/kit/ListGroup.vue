<script setup lang="ts">
/** A titled run of ListItems in a workspace list. */
defineProps<{ title: string }>()
</script>

<template>
    <section class="list-group">
        <h3>{{ title }}</h3>
        <slot />
    </section>
</template>

<style scoped>
.list-group + .list-group { margin-top: 1.1rem; }
h3 { margin: 0 0 0.4rem; padding: 0 0.65rem; color: var(--ink-faint); font: 400 0.6rem var(--font-display); letter-spacing: 0.08em; text-transform: uppercase; }
</style>
