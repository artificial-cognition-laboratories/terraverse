<script setup lang="ts">
import PixelIcon from "../game/PixelIcon.vue"

/** A category to browse into: its pixel icon, its name and how many are in it. `disabled`: shown, not yet open. */
withDefaults(defineProps<{ label: string; rows: readonly string[]; palette: Record<string, string>; count?: number | null; disabled?: boolean }>(), { count: null, disabled: false })
defineEmits<{ open: [] }>()
</script>

<template>
    <button type="button" class="category" :disabled="disabled" @click="$emit('open')">
        <PixelIcon :rows="rows" :palette="palette" style="--u: 3px" />
        <span class="label">{{ label }}</span>
        <span v-if="count !== null" class="count">{{ count }}</span>
    </button>
</template>

<style scoped>
.category { display: flex; gap: 0.9rem; align-items: center; min-width: 0; padding: 0.85rem 1rem; border: 1px solid var(--glass-edge); background: var(--glass); backdrop-filter: blur(8px); color: var(--ink-dim); text-align: left; }
.category:hover:not(:disabled) { border-color: var(--edge-lit); }
.category:hover:not(:disabled) .label { color: var(--ink); }
.category:disabled { cursor: default; }
.category:disabled > * { opacity: 0.45; }
.label { flex: 1; min-width: 0; overflow: hidden; font: 0.68rem var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
.count { color: var(--ink-faint); font-size: 0.68rem; }
</style>
