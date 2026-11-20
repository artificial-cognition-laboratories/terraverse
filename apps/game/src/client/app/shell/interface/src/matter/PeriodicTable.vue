<script setup lang="ts">
import type { MatterElement } from "../bridge"

/**
 * The periodic table: every element in its place, the f-block below. The
 * ones with a modelled substance are lit; choosing one emits it.
 */
defineProps<{ elements: MatterElement[]; modelled: Set<string>; selected: string | null }>()
const emit = defineEmits<{ pick: [symbol: string] }>()

const CATEGORY: Record<MatterElement["category"], string> = {
    nonmetal: "#5f8a5a", noble: "#6c5f99", alkali: "#9a5a4c", alkaline: "#9a7a4c", metalloid: "#5f8a8a",
    halogen: "#7a9a4c", post: "#5f6f8a", transition: "#6a6f78", lanthanide: "#8a5f7a", actinide: "#8a5f5f",
}
</script>

<template>
    <div class="table">
        <button v-for="e in elements" :key="e.number" class="cell"
            :class="{ lit: modelled.has(e.symbol), on: selected === e.symbol, gap: e.row >= 9 }"
            :style="{ gridRow: e.row + (e.row >= 9 ? 1 : 0), gridColumn: e.col, '--tone': CATEGORY[e.category] }"
            :title="`${e.number} ${e.name}`" @click="emit('pick', e.symbol)">
            <span class="n">{{ e.number }}</span>
            <span class="s">{{ e.symbol }}</span>
        </button>
    </div>
</template>

<style scoped>
.table { display: grid; grid-template-columns: repeat(18, minmax(0, 1fr)); grid-template-rows: repeat(7, 1fr) 8px repeat(2, 1fr); gap: 2px; width: 100%; }
.cell {
    position: relative;
    aspect-ratio: 1;
    padding: 0;
    border: var(--b) solid var(--edge);
    background: color-mix(in srgb, var(--tone) 35%, #101418);
    color: var(--ink-faint);
    opacity: 0.55;
}
.cell.lit { opacity: 1; color: var(--ink); background: color-mix(in srgb, var(--tone) 70%, #101418); }
.cell.on { outline: 2px solid var(--accent-hi); z-index: 1; }
.cell:hover { opacity: 1; }
.n { position: absolute; top: 1px; left: 2px; font-size: 7px; }
.s { font: 11px var(--font-display); }
</style>
