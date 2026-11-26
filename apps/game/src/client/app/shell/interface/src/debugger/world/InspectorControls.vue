<script setup lang="ts">
/**
 * An inspected system's controls, as it describes them: a choice as a row of
 * buttons (the chosen one lit), a toggle as a checkbox, a button as itself.
 * Each value is the engine's: nothing changes here until it says so.
 */
import type { InspectorControl } from "../../bridge/index.ts"

defineProps<{ controls: readonly InspectorControl[] }>()
const emit = defineEmits<{ act: [control: string, value: string | boolean | null] }>()
</script>

<template>
    <div class="controls">
        <div v-for="c in controls" :key="c.id" class="control">
            <span class="label">{{ c.label }}</span>
            <div v-if="c.type === 'choice'" class="choice" role="radiogroup" :aria-label="c.label">
                <button v-for="o in c.options" :key="o.id" role="radio" :aria-checked="o.id === c.value" :class="{ on: o.id === c.value }"
                    :title="o.about" @click="emit('act', c.id, o.id)">{{ o.label }}</button>
            </div>
            <label v-else-if="c.type === 'toggle'" class="toggle" :title="c.about">
                <input type="checkbox" :checked="c.value" @change="emit('act', c.id, !c.value)" />
                <span>{{ c.about }}</span>
            </label>
            <button v-else class="press" :title="c.about" @click="emit('act', c.id, null)">{{ c.about }}</button>
        </div>
    </div>
</template>

<style scoped>
.controls { display: grid; gap: 10px; padding: 12px 14px 0; max-width: 780px; }
.control { display: grid; grid-template-columns: minmax(120px, 220px) minmax(0, 1fr); gap: 16px; align-items: center; }
.label { font-size: 11px; color: var(--dt-ink); white-space: nowrap; }
.choice { display: flex; flex-wrap: wrap; gap: 4px; min-width: 0; }
button { border: 1px solid var(--dt-chip-line); background: var(--dt-chip); color: var(--dt-dim); font: inherit; font-size: 11px;
    padding: 3px 8px; white-space: nowrap; cursor: default; }
button:hover { color: var(--dt-ink); background: var(--dt-hover); }
button.on { color: var(--dt-ink); border-color: var(--dt-accent); background: var(--dt-held); }
.press { justify-self: start; }
.toggle { display: flex; gap: 6px; align-items: center; font-size: 11px; color: var(--dt-dim); min-width: 0; }
.toggle input { margin: 0; accent-color: var(--dt-accent); }
</style>
