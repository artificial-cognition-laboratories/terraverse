<script setup lang="ts">
/** A titled group of controls. */
defineProps<{ title: string; hint?: string }>()
</script>

<template>
    <section class="section">
        <header><span class="title">{{ title }}</span><span v-if="hint" class="hint">{{ hint }}</span></header>
        <div class="body"><slot /></div>
    </section>
</template>

<style scoped>
.section { display: flex; flex-direction: column; gap: 12px; }
header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    padding-bottom: 4px;
    border-bottom: var(--b) solid var(--stone-3);
}
.title { font: 12px var(--font-display); text-transform: uppercase; color: var(--accent-hi); }
.hint { font-size: 11px; color: var(--ink-faint); }
.body { display: flex; flex-direction: column; gap: 14px; }
</style>
