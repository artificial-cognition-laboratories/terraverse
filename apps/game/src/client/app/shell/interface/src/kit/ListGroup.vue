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
.list-group + .list-group { margin-top: 14px; }
h3 { margin: 0 0 6px; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }
</style>
