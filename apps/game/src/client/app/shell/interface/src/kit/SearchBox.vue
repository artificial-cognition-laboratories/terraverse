<script setup lang="ts">
import { onMounted, ref } from "vue"

/** `autofocus`: takes the keyboard when shown, so a page that opens on a search can be typed into at once. */
const props = defineProps<{ label: string; placeholder?: string; autofocus?: boolean }>()
const model = defineModel<string>({ required: true })
const input = ref<HTMLInputElement | null>(null)
onMounted(() => { if (props.autofocus) input.value?.focus({ preventScroll: true }) })
defineExpose({ focus: () => input.value?.focus(), blur: () => input.value?.blur() })
</script>

<template>
    <label class="search-box">
        <span class="glyph">⌕</span>
        <input ref="input" v-model="model" type="search" :aria-label="label" :placeholder="placeholder ?? label" spellcheck="false">
        <button v-if="model" type="button" aria-label="Clear search" @click="model = ''">×</button>
    </label>
</template>

<style scoped>
.search-box { display: flex; align-items: center; gap: 6px; width: 100%; height: 32px; padding: 0 8px; border: var(--b) solid var(--edge); background: var(--stone-0); box-shadow: var(--sunk); }
.search-box:focus-within { border-color: var(--accent); }
.glyph { color: var(--accent-hi); font-size: 17px; line-height: 1; }
input { flex: 1; min-width: 0; height: 100%; border: 0; outline: 0; background: none; font-size: 11px; }
input::placeholder { color: var(--ink-faint); }
input::-webkit-search-cancel-button { display: none; }
button { flex: none; padding: 0 3px; border: 0; background: none; color: var(--ink-dim); font-size: 18px; line-height: 1; }
button:hover { color: var(--ink); }
</style>
