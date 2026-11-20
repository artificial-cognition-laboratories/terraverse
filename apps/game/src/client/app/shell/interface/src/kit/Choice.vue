<script setup lang="ts" generic="T extends string">
/**
 * Pick one of a few big options, each with a line saying what it means.
 * `custom` marks that the current values match none of them.
 */
withDefaults(defineProps<{ options: readonly { id: T; label: string; note: string }[]; columns?: number; disabled?: boolean }>(), {
    columns: 2,
    disabled: false,
})
const model = defineModel<T | null>({ required: true })
</script>

<template>
    <div class="choice" :class="{ disabled: disabled }" :style="{ gridTemplateColumns: `repeat(${columns}, minmax(0, 1fr))` }" role="radiogroup">
        <button v-for="o in options" :key="o.id" role="radio" :aria-checked="model === o.id" :class="{ on: model === o.id }"
            :disabled="disabled" @click="model = o.id">
            <span class="label">{{ o.label }}</span>
            <span class="note">{{ o.note }}</span>
        </button>
    </div>
</template>

<style scoped>
.choice { display: grid; gap: 6px; }

button {
    display: flex;
    flex-direction: column;
    gap: 4px;
    padding: 10px 12px;
    border: var(--b) solid var(--edge);
    background: var(--stone-2);
    box-shadow: var(--raised);
    text-align: left;
}

button:hover:not(:disabled) { background: var(--stone-3); }
button.on { background: rgb(0 148 210 / 0.16); box-shadow: inset 0 0 0 var(--b) var(--accent), var(--raised); }

.label { font: 13px var(--font-display); text-transform: uppercase; color: var(--ink); }
.note { font-size: 11px; line-height: 1.35; color: var(--ink-dim); }
.on .label { color: var(--accent-hi); }

.disabled { opacity: 0.4; }
button:disabled { cursor: default; }
</style>
