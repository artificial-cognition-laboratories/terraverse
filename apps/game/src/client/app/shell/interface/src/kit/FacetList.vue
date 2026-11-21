<script setup lang="ts">
/** A filter's options, any number picked, each with how many it would show. */
defineProps<{ title: string; options: readonly { id: string; label: string; count?: number | null }[] }>()
const model = defineModel<string[]>({ required: true })
function flip(id: string) {
    model.value = model.value.includes(id) ? model.value.filter((v) => v !== id) : [...model.value, id]
}
</script>

<template>
    <fieldset class="facets">
        <legend>{{ title }}</legend>
        <button v-for="o in options" :key="o.id" type="button" role="checkbox" :aria-checked="model.includes(o.id)"
            :class="{ on: model.includes(o.id) }" @click="flip(o.id)">
            <span class="box" />
            <span class="label">{{ o.label }}</span>
            <span v-if="o.count != null" class="count">{{ o.count }}</span>
        </button>
    </fieldset>
</template>

<style scoped>
.facets { display: grid; gap: 1px; min-width: 0; margin: 0; padding: 0; border: 0; }
legend { margin-bottom: 6px; padding: 0; color: var(--ink-faint); font: 9px var(--font-display); text-transform: uppercase; }
button { display: grid; grid-template-columns: 12px minmax(0, 1fr) auto; gap: 8px; align-items: center; padding: 5px 6px; border: 0; background: none; color: var(--ink-dim); font-size: 11px; text-align: left; }
button:hover { background: var(--stone-2); color: var(--ink); }
.box { width: 12px; height: 12px; border: var(--b) solid var(--edge); background: var(--stone-0); box-shadow: var(--sunk); }
.on { color: var(--ink); }
.on .box { background: var(--accent); box-shadow: inset 0 0 0 2px var(--stone-0); }
.label { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.count { color: var(--ink-faint); font-size: 10px; }
</style>
