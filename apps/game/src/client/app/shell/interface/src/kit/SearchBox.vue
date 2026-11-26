<script setup lang="ts">
import { onMounted, ref } from "vue"
import PixelIcon from "../game/PixelIcon.vue"

/** `autofocus`: takes the keyboard when shown, so a page that opens on a search can be typed into at once. */
const props = defineProps<{ label: string; placeholder?: string; autofocus?: boolean }>()
const model = defineModel<string>({ required: true })
const input = ref<HTMLInputElement | null>(null)
onMounted(() => { if (props.autofocus) input.value?.focus({ preventScroll: true }) })
defineExpose({ focus: () => input.value?.focus(), blur: () => input.value?.blur() })
// the site's magnifier (apps/site config/icons.ts: search), seven pixels square
const GLASS = { rows: [".###...", "#...#..", "#...#..", "#...#..", ".###...", "....##.", ".....##"], palette: { "#": "currentColor" } }
</script>

<template>
    <label class="search-box">
        <PixelIcon class="glyph" :rows="GLASS.rows" :palette="GLASS.palette" style="--u: 2px" />
        <input ref="input" v-model="model" type="search" :aria-label="label" :placeholder="placeholder ?? label" spellcheck="false">
        <button v-if="model" type="button" aria-label="Clear search" @click="model = ''">×</button>
    </label>
</template>

<style scoped>
/* the website's search (apps/site shared/search.vue): a pane of glass in the bar */
.search-box { display: flex; align-items: center; gap: 0.6rem; width: 100%; height: 2.2rem; padding: 0 0.8rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); backdrop-filter: blur(8px); }
.search-box:focus-within { border-color: var(--edge-lit-hi); }
.glyph { flex: none; color: var(--ink-faint); }
input { flex: 1; min-width: 0; height: 100%; border: 0; outline: 0; background: none; font: 0.75rem var(--font); }
input::placeholder { color: var(--ink-faint); }
input::-webkit-search-cancel-button { display: none; }
button { flex: none; padding: 0 3px; border: 0; background: none; color: var(--ink-dim); font-size: 1.2rem; line-height: 1; }
button:hover { color: var(--ink); }
</style>
