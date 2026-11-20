<script setup lang="ts">
/**
 * An amount against a scale: the bar fills to `value` of `max`, with ticks at
 * `marks` (where it changes meaning), in `tone` once it's past them.
 */
import { computed } from "vue"

const props = defineProps<{ value: number; max: number; marks?: number[]; label: string; tone?: "ok" | "warn" | "danger" }>()
const share = (v: number) => `${Math.min(100, Math.max(0, (v / props.max) * 100))}%`
const fill = computed(() => share(props.value))
</script>

<template>
    <div class="meter" role="meter" :aria-label="label" :aria-valuenow="value" aria-valuemin="0" :aria-valuemax="max">
        <div class="fill" :class="tone ?? 'ok'" :style="{ width: fill }" />
        <span v-for="m in marks ?? []" :key="m" class="mark" :style="{ left: share(m) }" />
        <span class="label">{{ label }}</span>
    </div>
</template>

<style scoped>
.meter { position: relative; height: 18px; border: var(--b) solid var(--edge); background: var(--stone-0); box-shadow: var(--sunk); }
.fill { position: absolute; inset: 0 auto 0 0; box-shadow: var(--raised); }
.fill.ok { background: var(--accent-lo); }
.fill.warn { background: var(--warn-lo); }
.fill.danger { background: var(--danger-lo); }
.mark { position: absolute; top: 0; bottom: 0; width: var(--b); background: var(--edge); }
.label { position: relative; display: block; padding: 0 6px; font: 11px/14px var(--font-display); text-transform: uppercase; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
</style>
