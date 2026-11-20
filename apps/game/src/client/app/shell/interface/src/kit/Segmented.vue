<script setup lang="ts" generic="T extends string | number">
/** Pick one of a few: joined blocks, the chosen one sunk and lit. */
defineProps<{ options: readonly { value: T; label: string }[] }>()
const model = defineModel<T | null>({ required: true })
</script>

<template>
    <div class="segmented" role="radiogroup">
        <button v-for="o in options" :key="String(o.value)" role="radio" :aria-checked="model === o.value"
            :class="{ on: model === o.value }" @click="model = o.value">
            {{ o.label }}
        </button>
    </div>
</template>

<style scoped>
.segmented { display: flex; min-width: 0; }

button {
    flex: 1;
    min-width: 0;
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
</style>
