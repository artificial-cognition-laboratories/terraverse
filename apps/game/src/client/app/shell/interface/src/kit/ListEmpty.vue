<script setup lang="ts">
/** What a workspace list says when it has nothing to show. */
defineProps<{ title?: string }>()
</script>

<template>
    <div class="list-empty">
        <strong v-if="title">{{ title }}</strong>
        <slot />
    </div>
</template>

<style scoped>
.list-empty { display: grid; gap: 6px; margin: 8px; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
.list-empty strong { color: var(--ink); font: 11px var(--font-display); text-transform: uppercase; }
.list-empty :deep(p) { margin: 0; }
</style>
