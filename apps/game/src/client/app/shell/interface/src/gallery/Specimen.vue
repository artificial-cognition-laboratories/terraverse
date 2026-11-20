<script setup lang="ts">
/** One labelled exhibit on a gallery page. */
defineProps<{ name: string; note?: string; wide?: boolean }>()
</script>

<template>
    <figure class="specimen" :class="{ wide: wide }">
        <figcaption><span class="name">{{ name }}</span><span v-if="note" class="note">{{ note }}</span></figcaption>
        <div class="stage"><slot /></div>
    </figure>
</template>

<style scoped>
.specimen { margin: 0; display: flex; flex-direction: column; gap: 10px; }
.wide { grid-column: 1 / -1; }
figcaption { display: flex; gap: 12px; align-items: baseline; }
.name { font: 12px var(--font-display); text-transform: uppercase; color: var(--accent-hi); }
.note { font-size: 11px; color: var(--ink-faint); }
.stage {
    display: flex;
    flex-wrap: wrap;
    gap: 14px;
    align-items: flex-start;
    padding: 20px;
    border: var(--b) dashed var(--stone-3);
    background: var(--stone-1);
}
</style>
