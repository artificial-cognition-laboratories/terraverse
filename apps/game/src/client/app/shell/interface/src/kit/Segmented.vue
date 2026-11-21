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
.segmented { display: flex; min-width: 0; }

button {
    flex: 1 0 auto;
    height: 32px;
    padding: 0 6px;
    white-space: nowrap;
    border: var(--b) solid var(--edge);
    margin-left: calc(var(--b) * -1);
    background: var(--stone-2);
    font: 12px var(--font-display);
    text-transform: uppercase;
    color: var(--ink-dim);
    box-shadow: var(--raised);
}

button:first-child { margin-left: 0; }
button:hover { color: var(--ink); }
button.on { position: relative; background: var(--accent-lo); color: var(--ink); box-shadow: inset 0 calc(var(--b) * -2) 0 var(--accent-hi), var(--sunk); }
.sm button { height: 26px; font-size: 10px; }
.line button { border-color: var(--stone-3); background: none; box-shadow: none; color: var(--ink-faint); margin-left: -1px; border-width: 1px; }
.line button:hover { color: var(--ink); border-color: var(--stone-4); z-index: 1; }
.line button.on { z-index: 2; border-color: var(--accent); background: none; box-shadow: none; color: var(--accent-hi); }
</style>
