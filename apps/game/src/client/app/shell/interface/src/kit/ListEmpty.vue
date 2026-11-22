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
.list-empty { display: grid; gap: 0.4rem; margin: 0.5rem 0.65rem; color: var(--ink-dim); font-size: 0.75rem; line-height: 1.5; }
.list-empty strong { color: var(--ink); font: 0.7rem var(--font-display); text-transform: uppercase; }
.list-empty :deep(p) { margin: 0; }
</style>
