<script setup lang="ts">
import { computed } from "vue"
import { CATALOGUE } from "./catalogue.ts"

/**
 * A form of a material, drawn: the form's outline in the material's colour, 16 of the HUD's pixels.
 * TEMP(2026-11-23): drawn, not baked: parts get their faces from the art pipeline once they're real.
 */
const props = defineProps<{ form: string; material: string }>()

// each shape, in the 9×9 the kit's glyphs use
const OUTLINE: Record<string, string> = {
    long: "M7 0h2v2h-1v1h-1v1h-1v1h-1v1h-1v1h-1v1h-1v1h-2v-2h1v-1h1v-1h1v-1h1v-1h1v-1h1v-1h1z",
    peg: "M5 2h2v2h-1v1h-1v1h-1v1h-2v-2h1v-1h1v-1h1z",
    wedge: "M1 1h5v1h1v1h1v3h-1v1h-1v1h-5v-2h2v-3h-2z",
    lump: "M3 1h3v1h1v1h1v3h-1v1h-1v1h-3v-1h-1v-1h-1v-3h1v-1h1z",
    chips: "M1 1h2v2h-2zM5 2h2v1h-2zM2 5h2v2h-2zM6 5h2v2h-1v1h-1z",
    strand: "M1 0h1v3h1v3h-1v3h-1v-3h1v-3h-1zM5 0h1v3h1v3h-1v3h-1v-3h1v-3h-1z",
    round: "M2 1h5v1h1v5h-1v1h-5v-1h-1v-5h1zM3 3v3h3v-3z",
    axe: "M2 0h4v1h1v3h-1v1h-1v-1h-1v5h-1v-5h-1v1h-1v-4h1z",
    knife: "M7 0h2v1h-1v1h-1v1h-1v1h-1v1h-1v1h-1v1h-1v1h-1v1h-1v-2h1v-1h1v-1h1v-1h1v-1h1v-1h1v-1h1z",
}
const path = computed(() => OUTLINE[CATALOGUE.forms[props.form]?.shape ?? "lump"] ?? OUTLINE.lump)
const colour = computed(() => CATALOGUE.materials[props.material]?.colour ?? "#888")
</script>

<template>
    <svg class="form-icon" viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true"><path :fill="colour" :d="path" /></svg>
</template>

<style scoped>
.form-icon { width: calc(var(--u) * 16); height: calc(var(--u) * 16); filter: drop-shadow(1px 1px 0 rgb(0 0 0 / 0.6)); }
</style>
