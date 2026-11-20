<script setup lang="ts">
/** A labelled text input, sunk into the surface. */
defineProps<{ label: string; placeholder?: string; maxlength?: number; mono?: boolean }>()
const model = defineModel<string>({ required: true })
</script>

<template>
    <label class="field">
        <span class="label">{{ label }}<slot name="hint" /></span>
        <span class="box">
            <input v-model="model" :placeholder="placeholder" :maxlength="maxlength" :class="{ mono: mono }" spellcheck="false">
            <slot name="action" />
        </span>
    </label>
</template>

<style scoped>
.field { display: flex; flex-direction: column; gap: 6px; }
.label { display: flex; justify-content: space-between; font-size: 13px; color: var(--ink-dim); }
.box { display: flex; }

input {
    flex: 1;
    min-width: 0;
    height: 38px;
    padding: 0 12px;
    border: var(--b) solid var(--edge);
    background: var(--stone-0);
    box-shadow: var(--sunk);
    outline: none;
}

input:focus { border-color: var(--accent); }
input::placeholder { color: var(--ink-faint); }
.mono { font-size: 12px; letter-spacing: 0.02em; }
</style>
