<script setup lang="ts" generic="T extends string | number">
/** Pick one of a few: joined blocks, the chosen one sunk and lit. `line`: borders only, for filters on a bare page. */
withDefaults(defineProps<{ options: readonly { value: T; label: string }[]; look?: "block" | "line"; size?: "md" | "sm" }>(), { look: "block", size: "md" })
const model = defineModel<T | null>({ required: true })
</script>

<template>
    <div class="segmented" :class="[look, size]" role="radiogroup">
        <button v-for="o in options" :key="String(o.value)" role="radio" :aria-checked="model === o.value"
            :class="{ on: model === o.value }" @click="model = o.value">
            {{ o.label }}
        </button>
    </div>
</template>

<style scoped>
/* glass, as the site's layout toggle (registry browse: .views): the chosen one tinted with the ocean */
.segmented { display: flex; min-width: 0; }
button { flex: 1 0 auto; height: 2.1rem; margin-left: -1px; padding: 0 0.5rem; border: 1px solid var(--glass-edge); background: var(--glass); backdrop-filter: blur(8px);
    color: var(--ink-faint); font: 0.68rem var(--font-display); text-transform: uppercase; white-space: nowrap; }
button:first-child { margin-left: 0; }
button:hover { color: var(--ink); }
button.on { position: relative; z-index: 1; border-color: var(--edge-lit-hi); background: var(--glass-lit); color: var(--ink); }
.sm button { height: 1.75rem; font-size: 0.6rem; }
.line button { background: none; backdrop-filter: none; }
.line button.on { background: none; color: var(--accent-hi); }
</style>
