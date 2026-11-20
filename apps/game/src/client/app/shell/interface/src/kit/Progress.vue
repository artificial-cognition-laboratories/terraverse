<script setup lang="ts">
/**
 * Something under way: for a known time, the bar fills over `seconds` from when
 * it's shown, as one CSS transition (no frame-by-frame repaint), keyed by the
 * caller so a new job starts a new bar; or as far as it's been measured, `value` (0..1).
 */
import { onMounted, ref } from "vue"

// one of `seconds` and `value`
const props = defineProps<{ seconds?: number; value?: number; label?: string }>()
if ((props.seconds === undefined) === (props.value === undefined))
    throw new Error("Progress takes one of seconds and value")
const full = ref(false)
onMounted(() => requestAnimationFrame(() => { full.value = true }))
</script>

<template>
    <div class="progress" role="progressbar" :aria-label="label">
        <div v-if="props.seconds !== undefined" class="fill" :class="{ full }" :style="{ transitionDuration: `${props.seconds}s` }" />
        <div v-else class="fill measured" :style="{ width: `${Math.round((props.value ?? 0) * 100)}%` }" />
        <span v-if="label" class="label">{{ label }}</span>
    </div>
</template>

<style scoped>
.progress { position: relative; height: 22px; border: var(--b) solid var(--edge); background: var(--stone-0); box-shadow: var(--sunk); }
.fill { position: absolute; inset: 0 auto 0 0; width: 0; background: var(--accent-lo); box-shadow: var(--raised); transition-property: width; transition-timing-function: linear; }
.fill.full { width: 100%; }
.fill.measured { transition: width 0.25s linear; }
.label { position: relative; display: block; padding: 0 8px; font: 11px/18px var(--font-display); text-transform: uppercase; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
</style>
