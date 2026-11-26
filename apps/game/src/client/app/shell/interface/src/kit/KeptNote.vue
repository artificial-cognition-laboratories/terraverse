<script setup lang="ts">
import { computed } from "vue"

/**
 * Over a page shown from what was kept, when asking again failed: the kept answer stays, and
 * this says how old it is and why it isn't fresh. Never in place of the data, never hiding why.
 */
const props = defineProps<{ at: number; problem: string }>()
const age = computed(() => {
    const s = Math.max(0, Math.round((Date.now() - props.at) / 1000))
    return s < 60 ? "moments ago" : s < 3600 ? `${Math.round(s / 60)} min ago` : s < 86_400 ? `${Math.round(s / 3600)} h ago` : `${Math.round(s / 86_400)} d ago`
})
</script>

<template>
    <p class="kept"><span class="tag">Offline</span> Showing what was here {{ age }}: {{ problem }}</p>
</template>

<style scoped>
.kept { display: flex; gap: 0.6rem; align-items: baseline; margin: 0; padding: 0.45rem 0.7rem; border: 1px solid var(--glass-edge); background: var(--glass); color: var(--ink-dim); font-size: 0.72rem; }
.tag { color: #d9a441; font: 0.55rem var(--font-display); text-transform: uppercase; letter-spacing: 0.08em; }
</style>
