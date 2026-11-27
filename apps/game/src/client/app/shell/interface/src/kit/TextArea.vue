<script setup lang="ts">
/** A labelled box for a few lines of text, sunk into the surface as Field is; counts down to `maxlength`. */
defineProps<{ label?: string; placeholder?: string; maxlength: number; rows?: number }>()
const model = defineModel<string>({ required: true })
</script>

<template>
    <label class="area">
        <span class="label">{{ label ?? "" }}<span class="left">{{ maxlength - model.length }}</span></span>
        <textarea v-model="model" :placeholder="placeholder" :maxlength="maxlength" :rows="rows ?? 3" spellcheck="false" />
    </label>
</template>

<style scoped>
.area { display: flex; flex-direction: column; gap: 0.4rem; }
.label { display: flex; justify-content: space-between; color: var(--ink-dim); font-size: 0.8rem; }
.left { color: var(--ink-faint); font: 0.7rem var(--font); }
textarea { min-width: 0; padding: 0.6rem 0.75rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: rgb(4 7 11 / 0.6); color: var(--ink); font: 0.85rem/1.45 var(--font-sans); resize: none; outline: none; }
textarea:focus { border-color: var(--edge-lit-hi); }
textarea::placeholder { color: var(--ink-faint); }
</style>
