<script setup lang="ts">
/**
 * Something under way for a known time: the bar fills over `seconds` from when
 * it's shown, as one CSS transition (no frame-by-frame repaint). Keyed by the
 * caller, so a new job starts a new bar.
 */
import { onMounted, ref } from "vue"

const props = defineProps<{ seconds: number; label?: string }>()
const full = ref(false)
onMounted(() => requestAnimationFrame(() => { full.value = true }))
</script>

<template>
    <div class="progress" role="progressbar" :aria-label="label">
        <div class="fill" :class="{ full }" :style="{ transitionDuration: `${props.seconds}s` }" />
        <span v-if="label" class="label">{{ label }}</span>
    </div>
</template>

<style scoped>
.progress { position: relative; height: 22px; border: var(--b) solid var(--edge); background: var(--stone-0); box-shadow: var(--sunk); }
.fill { position: absolute; inset: 0 auto 0 0; width: 0; background: var(--accent-lo); box-shadow: var(--raised); transition-property: width; transition-timing-function: linear; }
.fill.full { width: 100%; }
.label { position: relative; display: block; padding: 0 8px; font: 11px/18px var(--font-display); text-transform: uppercase; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
</style>
