<script setup lang="ts">
import PixelIcon from "../game/PixelIcon.vue"

/** A category to browse into: its pixel icon, its name and how many are in it. */
withDefaults(defineProps<{ label: string; rows: readonly string[]; palette: Record<string, string>; count?: number | null }>(), { count: null })
defineEmits<{ open: [] }>()
</script>

<template>
    <button type="button" class="category" @click="$emit('open')">
        <PixelIcon :rows="rows" :palette="palette" style="--u: 3px" />
        <span class="label">{{ label }}</span>
        <span v-if="count !== null" class="count">{{ count }}</span>
    </button>
</template>

<style scoped>
.category { display: grid; grid-template-columns: auto minmax(0, 1fr) auto; align-items: center; gap: 10px; min-width: 0; padding: 10px 12px; border: 1px solid var(--stone-3); background: none; color: var(--ink-dim); text-align: left; }
.category:hover { border-color: var(--accent-lo); color: var(--ink); }
.label { overflow: hidden; font: 10px var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
.count { color: var(--ink-faint); font-size: 10px; }
</style>
