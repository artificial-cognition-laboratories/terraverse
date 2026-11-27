<script setup lang="ts">
/** A text input, sunk into the surface; labelled, unless a FormRow says what it is. */
defineProps<{ label?: string; placeholder?: string; maxlength?: number; mono?: boolean }>()
const model = defineModel<string>({ required: true })
</script>

<template>
    <label class="field">
        <span v-if="label || $slots.hint" class="label">{{ label }}<slot name="hint" /></span>
        <span class="box">
            <input v-model="model" :placeholder="placeholder" :maxlength="maxlength" :class="{ mono: mono }" spellcheck="false">
            <slot name="action" />
        </span>
    </label>
</template>

<style scoped>
.field { display: flex; flex-direction: column; gap: 0.4rem; }
.label { display: flex; justify-content: space-between; color: var(--ink-dim); font-size: 0.8rem; }
.box { display: flex; }
input { flex: 1; min-width: 0; height: 2.4rem; padding: 0 0.75rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: rgb(4 7 11 / 0.6); outline: none; }
input:focus { border-color: var(--edge-lit-hi); }
input::placeholder { color: var(--ink-faint); }
.mono { font-size: 0.78rem; letter-spacing: 0.02em; }
</style>
