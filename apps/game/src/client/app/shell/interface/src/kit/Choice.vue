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
/* the site's glass tiles (Button, CategoryTile): a hairline lit on hover, the ocean when chosen, at once */
.choice { display: grid; gap: 0.5rem; }
button { display: flex; flex-direction: column; gap: 0.3rem; padding: 0.8rem 0.9rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); text-align: left; }
button:hover:not(:disabled) { border-color: var(--edge-lit); }
button.on { border-color: var(--edge-lit-hi); background: var(--glass-lit); }
.label { font: 0.75rem var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; color: var(--ink-dim); }
button:hover:not(:disabled) .label, .on .label { color: var(--ink); }
.note { font: 0.75rem/1.45 var(--font-sans); color: var(--ink-faint); }
.on .note { color: var(--ink-dim); }
.disabled { opacity: 0.4; }
button:disabled { cursor: default; }
</style>
