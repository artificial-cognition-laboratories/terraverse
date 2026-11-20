<script setup lang="ts" generic="T extends string">
/** A row (or column) of tabs; the selected one carries the accent. */
withDefaults(defineProps<{ tabs: readonly { id: T; label: string }[]; direction?: "row" | "column" }>(), { direction: "row" })
const model = defineModel<T>({ required: true })
</script>

<template>
    <nav class="tabs" :class="direction" role="tablist">
        <button v-for="t in tabs" :key="t.id" role="tab" :aria-selected="model === t.id" :class="{ on: model === t.id }"
            @click="model = t.id">
            {{ t.label }}
        </button>
    </nav>
</template>

<style scoped>
.tabs { display: flex; gap: 4px; }
.column { flex-direction: column; }

button {
    height: 36px;
    padding: 0 14px;
    border: var(--b) solid var(--edge);
    background: var(--stone-2);
    font: 12px var(--font-display);
    text-transform: uppercase;
    color: var(--ink-dim);
    white-space: nowrap;
    box-shadow: var(--raised);
}

.row button { flex: 1; }
.column button { text-align: left; border-color: transparent; background: none; box-shadow: none; }

button:hover { color: var(--ink); background: var(--stone-3); }

.row button.on { background: var(--stone-3); color: var(--ink); box-shadow: inset 0 calc(var(--b) * -2) 0 var(--accent), var(--raised); }
.column button.on { border-color: var(--edge); background: var(--stone-3); color: var(--ink); box-shadow: inset 4px 0 0 var(--accent), var(--raised); }
</style>
