<script setup lang="ts">
import PixelIcon from "../game/PixelIcon.vue"

/** The way back out of a screen: at the foot of its rail (IconRail's `foot`), as every full screen has it. */
withDefaults(defineProps<{ label?: string }>(), { label: "Back" })
defineEmits<{ back: [] }>()
const BACK = { rows: ["...G.....", "..GG.....", ".GGGGGGG.", "GGGGGGGG.", ".GGGGGGG.", "..GG.....", "...G....."], palette: { G: "currentColor" } }
</script>

<template>
    <button type="button" class="back" @click="$emit('back')">
        <PixelIcon :rows="BACK.rows" :palette="BACK.palette" style="--u: 3px" />
        <span>{{ label }}</span>
    </button>
</template>

<style scoped>
.back { display: flex; flex-direction: column; align-items: center; gap: 5px; width: 100%; padding: 10px 2px; border: 0; background: none; color: var(--ink-faint); font: 8px var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; }
.back:hover { color: var(--ink); }
</style>
