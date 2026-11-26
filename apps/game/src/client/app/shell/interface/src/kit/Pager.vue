<script setup lang="ts">
/** Page n of m, with a step either way. */
defineProps<{ pages: number }>()
const page = defineModel<number>({ required: true })
</script>

<template>
    <nav class="pager" aria-label="Pages">
        <button type="button" aria-label="Previous page" :disabled="page <= 1" @click="page--">‹</button>
        <span><b>{{ page }}</b> of {{ pages }}</span>
        <button type="button" aria-label="Next page" :disabled="page >= pages" @click="page++">›</button>
    </nav>
</template>

<style scoped>
.pager { display: flex; align-items: center; gap: 0.6rem; color: var(--ink-faint); font-size: 0.72rem; white-space: nowrap; }
b { color: var(--ink); font-weight: normal; }
button { display: grid; place-items: center; width: 2.1rem; height: 2.1rem; padding: 0; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); color: var(--ink); font-size: 1rem; }
button:hover:not(:disabled) { border-color: var(--edge-lit); color: var(--accent-hi); }
button:disabled { color: var(--ink-faint); opacity: 0.5; }
</style>
