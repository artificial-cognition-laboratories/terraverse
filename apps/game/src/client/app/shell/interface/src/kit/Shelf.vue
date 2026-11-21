<script setup lang="ts">
/**
 * A titled row of cards: one line only, as many as fit, the rest behind "see all".
 */
withDefaults(defineProps<{ title: string; more?: string | null; min?: number }>(), { more: null, min: 200 })
defineEmits<{ more: [] }>()
</script>

<template>
    <section class="shelf">
        <header>
            <h3>{{ title }}</h3>
            <button v-if="more" type="button" class="more" @click="$emit('more')">{{ more }} »</button>
        </header>
        <div class="row" :style="{ '--min': `${min}px` }"><slot /></div>
    </section>
</template>

<style scoped>
.shelf { display: grid; gap: 10px; min-width: 0; }
header { display: flex; align-items: baseline; justify-content: space-between; gap: 12px; padding-bottom: 6px; border-bottom: 1px solid var(--stone-3); }
h3 { margin: 0; font: 12px var(--font-display); text-transform: uppercase; color: var(--ink); }
.more { padding: 0; border: 0; background: none; color: var(--accent-hi); font: 10px var(--font-display); text-transform: uppercase; white-space: nowrap; }
.more:hover { color: var(--ink); }
/* one row: later rows collapse to nothing and are clipped */
.row { display: grid; grid-template-columns: repeat(auto-fill, minmax(var(--min), 1fr)); grid-template-rows: auto; grid-auto-rows: 0; row-gap: 0; column-gap: 12px; overflow: hidden; padding: 0 4px 6px 0; }
.row > :deep(*) { min-width: 0; }
</style>
