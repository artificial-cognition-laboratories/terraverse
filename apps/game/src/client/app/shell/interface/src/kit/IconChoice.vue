<script setup lang="ts" generic="T extends string">
import PixelIcon from "../game/PixelIcon.vue"

/** Compact illustrated choices. Explanations belong below the selected choice. */
withDefaults(defineProps<{
    options: readonly { id: T; label: string; rows: readonly string[]; palette: Record<string, string> }[]
    columns?: number
    layout?: "inline" | "stacked"
    label: string
}>(), { columns: 2, layout: "inline" })
const model = defineModel<T | null>({ required: true })
</script>

<template>
    <div class="icon-choice" :class="layout" :style="{ gridTemplateColumns: `repeat(${columns}, minmax(0, 1fr))` }" role="radiogroup" :aria-label="label">
        <button v-for="option in options" :key="option.id" type="button" role="radio" :aria-checked="model === option.id"
            :class="{ on: model === option.id }" @click="model = option.id">
            <PixelIcon :rows="option.rows" :palette="option.palette" style="--u: 3px" />
            <span>{{ option.label }}</span>
        </button>
    </div>
</template>

<style scoped>
.icon-choice { display: grid; gap: 6px; min-width: 0; }
button { display: flex; align-items: center; justify-content: flex-start; gap: 12px; min-width: 0; min-height: 50px; padding: 8px 12px; border: var(--b) solid var(--edge); background: var(--stone-2); box-shadow: var(--raised); text-align: left; }
button:hover { background: var(--stone-3); }
button:focus-visible { outline: var(--b) solid var(--accent-hi); outline-offset: 2px; }
button.on { background: rgb(0 148 210 / 0.16); box-shadow: inset 0 -4px 0 var(--accent), var(--raised); }
button span { min-width: 0; color: var(--ink); font: 11px var(--font-display); text-transform: uppercase; white-space: nowrap; }
button.on span { color: var(--accent-hi); }
button :deep(svg) { flex: none; }
.stacked button { min-height: 74px; flex-direction: column; justify-content: center; gap: 7px; padding: 8px 3px; text-align: center; }
.stacked button span { font-size: 9px; }
</style>
