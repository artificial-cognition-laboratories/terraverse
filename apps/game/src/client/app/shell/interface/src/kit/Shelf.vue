<script setup lang="ts">
/**
 * A titled row, as the website's registry draws one: its label over a hairline, "see all" on the
 * right, then the row's content (`cols` across when it's a grid of cards, or anything else).
 */
withDefaults(defineProps<{ title: string; more?: string | null; cols?: number | null }>(), { more: null, cols: 4 })
defineEmits<{ more: [] }>()
</script>

<template>
    <section class="shelf">
        <header>
            <h3>{{ title }}</h3>
            <button v-if="more" type="button" class="more" @click="$emit('more')">{{ more }} »</button>
        </header>
        <div :class="{ grid: cols }" :style="cols ? { '--cols': cols } : undefined"><slot /></div>
    </section>
</template>

<style scoped>
.shelf { min-width: 0; }
header { display: flex; align-items: baseline; justify-content: space-between; margin: 3rem 0 1rem; padding-bottom: 0.6rem; border-bottom: 1px solid var(--glass-edge); }
h3 { margin: 0; font: 400 0.7rem var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; color: var(--ink); }
.more { padding: 0; border: 0; background: none; color: var(--accent-hi); font: 0.62rem var(--font-display); text-transform: uppercase; white-space: nowrap; }
.more:hover { color: var(--ink); }
.grid { display: grid; grid-template-columns: repeat(var(--cols), minmax(0, 1fr)); gap: 1rem; }
.grid > :deep(*) { min-width: 0; }
@media (max-width: 1000px) { .grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
</style>
